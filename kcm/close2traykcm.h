#ifndef CLOSE2TRAYKCM_H
#define CLOSE2TRAYKCM_H

#pragma once

#include <KCModule>
#include <KConfigGroup>
#include <QJsonArray>
#include <QJsonObject>

class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// Configuration module of the KWin script, replaces the generic .ui based one
class Close2TrayKcm : public KCModule {
    Q_OBJECT

  public:
    Close2TrayKcm(QObject *parent, const KPluginMetaData &data);

    void load() override;
    void save() override;
    void defaults() override;

  private:
    enum Column { NameColumn, ClassColumn, MinimizeColumn, CloseColumn, StartColumn };

    KConfigGroup configGroup() const;
    QJsonArray readRules() const;
    QJsonArray importUpstreamRules() const;
    QTreeWidgetItem *addRule(const QJsonObject &rule);
    QJsonObject ruleFromItem(const QTreeWidgetItem *item) const;
    void updateItemDecoration(QTreeWidgetItem *item, const QJsonObject &rule);
    void detectWindow();
    void addDetectedWindow(const QVariantMap &info);
    void updateButtons();
    void markChanged();

    QTreeWidget *m_rules;
    QPushButton *m_detectButton;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QLabel *m_message;
    QCheckBox *m_hideOnMinimize;
    QCheckBox *m_countUseDot;
    QCheckBox *m_notificationDot;
    QCheckBox *m_debugEnabled;
    bool m_loading = false;
};

#endif
