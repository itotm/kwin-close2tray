#include "closeinterceptor.h"

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationButtonGroup>
#include <QChildEvent>
#include <QDebug>
#include <QLoggingCategory>
#include <QTimer>
#include <QVariant>

using KDecoration3::Decoration;
using KDecoration3::DecorationButton;
using KDecoration3::DecorationButtonGroup;
using KDecoration3::DecorationButtonType;

// QT_LOGGING_RULES="close2tray.debug=true" to trace the interception
Q_LOGGING_CATEGORY(C2T, "close2tray", QtWarningMsg)

CloseInterceptor::CloseInterceptor(QObject *parent) : QObject(parent) {}

CloseInterceptor::~CloseInterceptor() {
    // the script is going away, give every window its normal close button back
    const auto windows = m_windows.keys();
    for (QObject *window : windows) {
        unhookAll(window);
    }
}

void CloseInterceptor::setIntercepted(QObject *window, bool intercepted) {
    if (!window) {
        return;
    }
    if (!intercepted) {
        unhookAll(window);
        m_windows.remove(window);
        return;
    }
    if (!m_windows.contains(window)) {
        m_windows.insert(window, {});
        // the decoration is created (and recreated) as a child of the window
        window->installEventFilter(this);
        connect(window, &QObject::destroyed, this, [this, window]() { m_windows.remove(window); });
    }
    scan(window);
}

bool CloseInterceptor::isIntercepted(QObject *window) const { return m_windows.contains(window); }

bool CloseInterceptor::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ChildAdded) {
        // watched is the window, its decoration or one of its button groups
        QObject *window = watched;
        while (window && !m_windows.contains(window)) {
            window = window->parent();
        }
        if (window) {
            scheduleScan(window);
        }
    }
    return false;
}

void CloseInterceptor::scheduleScan(QObject *window) {
    // children are announced from their QObject constructor, look at them once they are complete
    QPointer<QObject> guard(window);
    QTimer::singleShot(0, this, [this, guard]() {
        if (guard && m_windows.contains(guard)) {
            scan(guard);
        }
    });
}

void CloseInterceptor::scan(QObject *window) {
    const auto decorations = window->findChildren<Decoration *>(Qt::FindDirectChildrenOnly);
    qCDebug(C2T) << "scanning" << window << "decorations:" << decorations.size();
    for (Decoration *decoration : decorations) {
        hookDecoration(window, decoration);
    }
}

void CloseInterceptor::hookDecoration(QObject *window, Decoration *decoration) {
    // removeEventFilter + installEventFilter keeps a single filter per object
    decoration->removeEventFilter(this);
    decoration->installEventFilter(this);
    const auto groups = decoration->findChildren<DecorationButtonGroup *>();
    for (DecorationButtonGroup *group : groups) {
        group->removeEventFilter(this);
        group->installEventFilter(this);
    }
    const auto buttons = decoration->findChildren<DecorationButton *>();
    for (DecorationButton *button : buttons) {
        if (button->type() == DecorationButtonType::Close) {
            hookButton(window, decoration, button);
        }
    }
}

void CloseInterceptor::hookButton(QObject *window, Decoration *decoration, DecorationButton *button) {
    auto &hooks = m_windows[window];
    hooks.removeIf([](const Hook &hook) { return !hook.button; });
    for (const Hook &hook : std::as_const(hooks)) {
        if (hook.button == button) {
            return;
        }
    }

    if (!QObject::disconnect(button, &DecorationButton::clicked, decoration, &Decoration::requestClose)) {
        qCWarning(C2T) << "close button of" << window << "is not wired to requestClose, leaving it alone";
        return;
    }

    QPointer<QObject> guard(window);
    const auto connection = connect(
        button, &DecorationButton::clicked, this,
        [this, guard]() {
            qCDebug(C2T) << "close button clicked on" << guard;
            if (guard) {
                emit closeRequested(guard->property("internalId").toUuid());
            }
        },
        Qt::QueuedConnection);
    hooks.append({button, decoration, connection});
    qCDebug(C2T) << "hooked close button" << button << "of" << window;
}

void CloseInterceptor::unhookAll(QObject *window) {
    window->removeEventFilter(this);
    const auto hooks = m_windows.value(window);
    for (const Hook &hook : hooks) {
        QObject::disconnect(hook.connection);
        if (hook.button && hook.decoration) {
            connect(hook.button, &DecorationButton::clicked, hook.decoration, &Decoration::requestClose,
                    Qt::QueuedConnection);
        }
    }
    const auto decorations = window->findChildren<Decoration *>(Qt::FindDirectChildrenOnly);
    for (Decoration *decoration : decorations) {
        decoration->removeEventFilter(this);
        const auto groups = decoration->findChildren<DecorationButtonGroup *>();
        for (DecorationButtonGroup *group : groups) {
            group->removeEventFilter(this);
        }
    }
    m_windows[window].clear();
}
