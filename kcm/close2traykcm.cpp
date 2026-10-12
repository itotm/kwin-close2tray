#include "close2traykcm.h"

#include <KPluginFactory>
#include <KService>
#include <KSharedConfig>
#include <QCheckBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

K_PLUGIN_CLASS_WITH_JSON(Close2TrayKcm, "kcm_close2tray.json")

namespace {
const QString scriptId = QStringLiteral("com.github.itotm.close2tray");
const QString upstreamScriptId = QStringLiteral("com.github.luisbocanegra.minimize2tray");

QJsonObject newRule(const QString &windowClass) {
    return {
        {QStringLiteral("enabled"), true},
        {QStringLiteral("windowClass"), windowClass},
        {QStringLiteral("desktopFile"), QString()},
        {QStringLiteral("name"), QString()},
        {QStringLiteral("minimizeToTray"), true},
        {QStringLiteral("closeToTray"), true},
        {QStringLiteral("startInTray"), false},
    };
}
} // namespace

Close2TrayKcm::Close2TrayKcm(QObject *parent, const KPluginMetaData &data) : KCModule(parent, data) {
    auto *layout = new QVBoxLayout(widget());

    auto *intro = new QLabel(QStringLiteral(
        "Windows matching an entry are sent to the system tray. Uncheck an entry to disable it without removing it."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_rules = new QTreeWidget;
    m_rules->setRootIsDecorated(false);
    m_rules->setAllColumnsShowFocus(true);
    m_rules->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rules->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_rules->setHeaderLabels({QStringLiteral("Application"), QStringLiteral("Window class"),
                              QStringLiteral("Minimize to tray"), QStringLiteral("Close to tray"),
                              QStringLiteral("Start in tray")});
    m_rules->headerItem()->setToolTip(ClassColumn, QStringLiteral("Matched against the window class, the resource "
                                                                  "name and the desktop file name of each window"));
    m_rules->headerItem()->setToolTip(CloseColumn,
                                      QStringLiteral("The close button of the title bar sends the window to the tray"));
    m_rules->headerItem()->setToolTip(StartColumn, QStringLiteral("New windows are sent to the tray as soon as they open"));
    m_rules->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_rules->header()->setSectionResizeMode(ClassColumn, QHeaderView::Stretch);
    for (int column : {MinimizeColumn, CloseColumn, StartColumn}) {
        m_rules->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    m_rules->setMinimumSize(720, 220);
    layout->addWidget(m_rules, 1);

    auto *buttons = new QHBoxLayout;
    m_detectButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-find")), QStringLiteral("Detect Window…"));
    m_detectButton->setToolTip(QStringLiteral("Click on a window to add it to the list"));
    m_addButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), QStringLiteral("Add Manually"));
    m_removeButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")), QStringLiteral("Remove"));
    buttons->addWidget(m_detectButton);
    buttons->addWidget(m_addButton);
    buttons->addStretch();
    buttons->addWidget(m_removeButton);
    layout->addLayout(buttons);

    m_message = new QLabel;
    m_message->setWordWrap(true);
    m_message->hide();
    layout->addWidget(m_message);

    m_hideOnMinimize = new QCheckBox(QStringLiteral("Windows sent to the tray with the shortcut go back to the tray "
                                                    "when minimized"));
    m_countUseDot = new QCheckBox(QStringLiteral("Show unread count as a dot"));
    m_notificationDot = new QCheckBox(QStringLiteral("Mark the tray icon when the application sends a notification"));
    m_notificationDot->setToolTip(QStringLiteral("For applications that don't report an unread count, like "
                                                 "Thunderbird: the mark goes away when the window is shown"));
    m_debugEnabled = new QCheckBox(QStringLiteral("Debug logging"));
    layout->addWidget(m_hideOnMinimize);
    layout->addWidget(m_countUseDot);
    layout->addWidget(m_notificationDot);
    layout->addWidget(m_debugEnabled);

    auto *shortcutNote = new QLabel(QStringLiteral(
        "<i>Close to tray</i> handles the close button of the title bar. To make Alt+F4 behave the same, remove it "
        "from <i>KWin › Close Window</i> in the Shortcuts settings and assign it to <i>KWin › Close window or send it "
        "to tray</i>."));
    shortcutNote->setWordWrap(true);
    layout->addWidget(shortcutNote);

    connect(m_detectButton, &QPushButton::clicked, this, &Close2TrayKcm::detectWindow);
    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        QTreeWidgetItem *item = addRule(newRule(QString()));
        m_rules->setCurrentItem(item);
        m_rules->editItem(item, ClassColumn);
        markChanged();
    });
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        delete m_rules->currentItem();
        updateButtons();
        markChanged();
    });
    connect(m_rules, &QTreeWidget::currentItemChanged, this, &Close2TrayKcm::updateButtons);
    connect(m_rules, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (m_loading) {
            return;
        }
        if (column == ClassColumn) {
            // a class typed by hand no longer belongs to the detected application
            QJsonObject rule = ruleFromItem(item);
            rule[QStringLiteral("desktopFile")] = QString();
            rule[QStringLiteral("name")] = QString();
            updateItemDecoration(item, rule);
        }
        markChanged();
    });
    for (QCheckBox *box : {m_hideOnMinimize, m_countUseDot, m_notificationDot, m_debugEnabled}) {
        connect(box, &QCheckBox::toggled, this, &Close2TrayKcm::markChanged);
    }

    updateButtons();
}

KConfigGroup Close2TrayKcm::configGroup() const {
    return KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Script-") + scriptId);
}

QJsonArray Close2TrayKcm::readRules() const {
    const KConfigGroup group = configGroup();
    if (!group.hasKey("rules")) {
        return importUpstreamRules();
    }
    return QJsonDocument::fromJson(group.readEntry("rules", QString()).toUtf8()).array();
}

// First run: offer the windows configured for the original minimize2tray script
QJsonArray Close2TrayKcm::importUpstreamRules() const {
    const KConfigGroup upstream =
        KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Script-") + upstreamScriptId);
    const bool hideOnMinimize = upstream.readEntry("hideOnMinimize", false);
    QJsonArray rules;
    const QStringList classes = upstream.readEntry("hideByDefaultClass", QString()).split(QLatin1Char(','));
    for (const QString &entry : classes) {
        const QString windowClass = entry.trimmed();
        if (windowClass.isEmpty()) {
            continue;
        }
        QJsonObject rule = newRule(windowClass);
        rule[QStringLiteral("minimizeToTray")] = hideOnMinimize;
        rule[QStringLiteral("closeToTray")] = false;
        rule[QStringLiteral("startInTray")] = true;
        rules.append(rule);
    }
    return rules;
}

void Close2TrayKcm::load() {
    m_loading = true;
    m_rules->clear();
    const QJsonArray rules = readRules();
    for (const QJsonValue &rule : rules) {
        addRule(rule.toObject());
    }

    const KConfigGroup group = configGroup();
    const bool upstreamHideOnMinimize =
        KSharedConfig::openConfig(QStringLiteral("kwinrc"))
            ->group(QStringLiteral("Script-") + upstreamScriptId)
            .readEntry("hideOnMinimize", false);
    m_hideOnMinimize->setChecked(group.readEntry("hideOnMinimize", upstreamHideOnMinimize));
    m_countUseDot->setChecked(group.readEntry("countUseDot", false));
    m_notificationDot->setChecked(group.readEntry("notificationDot", true));
    m_debugEnabled->setChecked(group.readEntry("debugEnabled", false));
    m_message->hide();
    m_loading = false;

    updateButtons();
    // imported rules are not saved yet
    setNeedsSave(!group.hasKey("rules") && !rules.isEmpty());
}

void Close2TrayKcm::save() {
    QJsonArray rules;
    for (int i = 0; i < m_rules->topLevelItemCount(); ++i) {
        const QJsonObject rule = ruleFromItem(m_rules->topLevelItem(i));
        if (!rule.value(QStringLiteral("windowClass")).toString().isEmpty()) {
            rules.append(rule);
        }
    }

    KConfigGroup group = configGroup();
    group.writeEntry("rules", QString::fromUtf8(QJsonDocument(rules).toJson(QJsonDocument::Compact)));
    group.writeEntry("hideOnMinimize", m_hideOnMinimize->isChecked());
    group.writeEntry("countUseDot", m_countUseDot->isChecked());
    group.writeEntry("notificationDot", m_notificationDot->isChecked());
    group.writeEntry("debugEnabled", m_debugEnabled->isChecked());
    group.sync();

    // the script reloads its rules when KWin reconfigures
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"),
        QStringLiteral("reconfigure")));

    KCModule::save();
}

void Close2TrayKcm::defaults() {
    m_rules->clear();
    m_hideOnMinimize->setChecked(false);
    m_countUseDot->setChecked(false);
    m_notificationDot->setChecked(true);
    m_debugEnabled->setChecked(false);
    updateButtons();
    markChanged();
}

QTreeWidgetItem *Close2TrayKcm::addRule(const QJsonObject &rule) {
    const bool wasLoading = m_loading;
    m_loading = true;
    auto *item = new QTreeWidgetItem(m_rules);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
    item->setCheckState(NameColumn, rule.value(QStringLiteral("enabled")).toBool(true) ? Qt::Checked : Qt::Unchecked);
    item->setText(ClassColumn, rule.value(QStringLiteral("windowClass")).toString());
    item->setCheckState(MinimizeColumn,
                        rule.value(QStringLiteral("minimizeToTray")).toBool() ? Qt::Checked : Qt::Unchecked);
    item->setCheckState(CloseColumn, rule.value(QStringLiteral("closeToTray")).toBool() ? Qt::Checked : Qt::Unchecked);
    item->setCheckState(StartColumn, rule.value(QStringLiteral("startInTray")).toBool() ? Qt::Checked : Qt::Unchecked);
    updateItemDecoration(item, rule);
    m_loading = wasLoading;
    return item;
}

QJsonObject Close2TrayKcm::ruleFromItem(const QTreeWidgetItem *item) const {
    QJsonObject rule = item->data(NameColumn, Qt::UserRole).toJsonObject();
    rule[QStringLiteral("enabled")] = item->checkState(NameColumn) == Qt::Checked;
    rule[QStringLiteral("windowClass")] = item->text(ClassColumn).trimmed();
    rule[QStringLiteral("minimizeToTray")] = item->checkState(MinimizeColumn) == Qt::Checked;
    rule[QStringLiteral("closeToTray")] = item->checkState(CloseColumn) == Qt::Checked;
    rule[QStringLiteral("startInTray")] = item->checkState(StartColumn) == Qt::Checked;
    return rule;
}

// Name and icon come from the desktop file when the window was detected
void Close2TrayKcm::updateItemDecoration(QTreeWidgetItem *item, const QJsonObject &rule) {
    const bool wasLoading = m_loading;
    m_loading = true;
    const QString desktopFile = rule.value(QStringLiteral("desktopFile")).toString();
    const QString windowClass = rule.value(QStringLiteral("windowClass")).toString();
    QString name = rule.value(QStringLiteral("name")).toString();
    QIcon icon;
    const KService::Ptr service =
        KService::serviceByDesktopName(desktopFile.isEmpty() ? windowClass.toLower() : desktopFile);
    if (service) {
        if (name.isEmpty()) {
            name = service->name();
        }
        icon = QIcon::fromTheme(service->icon());
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(desktopFile.isEmpty() ? windowClass.toLower() : desktopFile,
                                QIcon::fromTheme(QStringLiteral("application-x-executable")));
    }
    item->setText(NameColumn, name.isEmpty() ? windowClass : name);
    item->setIcon(NameColumn, icon);
    item->setToolTip(NameColumn, desktopFile.isEmpty() ? windowClass : desktopFile + QStringLiteral(".desktop"));
    item->setData(NameColumn, Qt::UserRole,
                  QJsonObject{{QStringLiteral("desktopFile"), desktopFile}, {QStringLiteral("name"), name}});
    m_loading = wasLoading;
}

void Close2TrayKcm::detectWindow() {
    m_message->hide();
    m_detectButton->setEnabled(false);
    const QDBusMessage message =
        QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
                                       QStringLiteral("org.kde.KWin"), QStringLiteral("queryWindowInfo"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        m_detectButton->setEnabled(true);
        const QDBusPendingReply<QVariantMap> reply = *self;
        if (reply.isValid()) {
            addDetectedWindow(reply.value());
            return;
        }
        if (reply.error().name() == QLatin1String("org.kde.KWin.Error.UserCancel")) {
            return;
        }
        m_message->setText(reply.error().name() == QLatin1String("org.kde.KWin.Error.InvalidWindow")
                               ? QStringLiteral("That window is not managed by KWin.")
                               : QStringLiteral("Could not detect the window: %1").arg(reply.error().message()));
        m_message->show();
    });
}

void Close2TrayKcm::addDetectedWindow(const QVariantMap &info) {
    QString windowClass = info.value(QStringLiteral("resourceClass")).toString();
    if (windowClass.isEmpty()) {
        windowClass = info.value(QStringLiteral("resourceName")).toString();
    }
    if (windowClass.isEmpty()) {
        m_message->setText(QStringLiteral("That window has no class to match against."));
        m_message->show();
        return;
    }

    for (int i = 0; i < m_rules->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_rules->topLevelItem(i);
        if (item->text(ClassColumn).compare(windowClass, Qt::CaseInsensitive) == 0) {
            m_rules->setCurrentItem(item);
            m_message->setText(QStringLiteral("%1 is already in the list.").arg(item->text(NameColumn)));
            m_message->show();
            return;
        }
    }

    QJsonObject rule = newRule(windowClass);
    const QString desktopFile = info.value(QStringLiteral("desktopFile")).toString();
    rule[QStringLiteral("desktopFile")] = desktopFile;
    const KService::Ptr service = KService::serviceByDesktopName(desktopFile);
    rule[QStringLiteral("name")] = service ? service->name() : info.value(QStringLiteral("caption")).toString();
    m_rules->setCurrentItem(addRule(rule));
    markChanged();
}

void Close2TrayKcm::updateButtons() { m_removeButton->setEnabled(m_rules->currentItem() != nullptr); }

void Close2TrayKcm::markChanged() {
    if (!m_loading) {
        setNeedsSave(true);
    }
}

#include "close2traykcm.moc"
