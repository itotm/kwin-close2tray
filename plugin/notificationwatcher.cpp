#include "notificationwatcher.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <dbus/dbus.h>
#include <functional>
#include <thread>

// QT_LOGGING_RULES="close2tray.debug=true" to trace the notifications seen
Q_LOGGING_CATEGORY(C2TNotifications, "close2tray", QtWarningMsg)

namespace {

using Deliver = std::function<void(const QString &desktopEntry, const QString &appName)>;

QString readString(DBusMessageIter *iter) {
    if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRING) {
        return {};
    }
    const char *value = nullptr;
    dbus_message_iter_get_basic(iter, &value);
    return QString::fromUtf8(value);
}

// string value of an a{sv} entry
QString readHint(DBusMessageIter *iter, const char *key) {
    if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY) {
        return {};
    }
    DBusMessageIter entries;
    dbus_message_iter_recurse(iter, &entries);
    while (dbus_message_iter_get_arg_type(&entries) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry;
        dbus_message_iter_recurse(&entries, &entry);
        if (readString(&entry) == QLatin1String(key) && dbus_message_iter_next(&entry) &&
            dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT) {
            DBusMessageIter variant;
            dbus_message_iter_recurse(&entry, &variant);
            return readString(&variant);
        }
        dbus_message_iter_next(&entries);
    }
    return {};
}

DBusHandlerResult filter(DBusConnection *, DBusMessage *message, void *data) {
    const auto &deliver = *static_cast<Deliver *>(data);
    const char *interface = dbus_message_get_interface(message);
    const char *member = dbus_message_get_member(message);
    DBusMessageIter iter;
    if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_METHOD_CALL && interface && member &&
        dbus_message_iter_init(message, &iter)) {
        QString desktopEntry;
        QString appName;
        if (qstrcmp(interface, "org.freedesktop.Notifications") == 0 && qstrcmp(member, "Notify") == 0) {
            // app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout
            appName = readString(&iter);
            for (int i = 0; i < 6; ++i) {
                dbus_message_iter_next(&iter);
            }
            desktopEntry = readHint(&iter, "desktop-entry");
        } else if (qstrcmp(interface, "org.freedesktop.impl.portal.Notification") == 0 &&
                   qstrcmp(member, "AddNotification") == 0) {
            // app_id, id, notification
            desktopEntry = readString(&iter);
        }
        if (!desktopEntry.isEmpty() || !appName.isEmpty()) {
            qCDebug(C2TNotifications) << "notification from" << desktopEntry << appName;
            deliver(desktopEntry, appName);
        }
    }
    // a monitor must never reply: claiming every message keeps libdbus from sending errors
    return DBUS_HANDLER_RESULT_HANDLED;
}

void monitor(const std::shared_ptr<std::atomic<bool>> &stop, Deliver deliver) {
    dbus_threads_init_default();
    DBusError error;
    dbus_error_init(&error);
    DBusConnection *connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (!connection) {
        qCWarning(C2TNotifications) << "cannot connect to the session bus:" << error.message;
        dbus_error_free(&error);
        return;
    }
    dbus_connection_set_exit_on_disconnect(connection, false);

    DBusMessage *call = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                     "org.freedesktop.DBus.Monitoring", "BecomeMonitor");
    const char *rules[] = {
        "type='method_call',interface='org.freedesktop.Notifications',member='Notify'",
        "type='method_call',interface='org.freedesktop.impl.portal.Notification',member='AddNotification'",
    };
    const char **rulesArg = rules;
    dbus_uint32_t flags = 0;
    dbus_message_append_args(call, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &rulesArg, int(std::size(rules)),
                             DBUS_TYPE_UINT32, &flags, DBUS_TYPE_INVALID);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, call, 5000, &error);
    dbus_message_unref(call);
    if (!reply) {
        qCWarning(C2TNotifications) << "cannot monitor notifications:" << error.message;
        dbus_error_free(&error);
        dbus_connection_close(connection);
        dbus_connection_unref(connection);
        return;
    }
    dbus_message_unref(reply);

    dbus_connection_add_filter(connection, filter, &deliver, nullptr);
    while (!stop->load() && dbus_connection_read_write_dispatch(connection, 250)) {
    }
    dbus_connection_remove_filter(connection, filter, &deliver);
    dbus_connection_close(connection);
    dbus_connection_unref(connection);
}

} // namespace

NotificationWatcher::NotificationWatcher(QObject *parent) : QObject(parent) {}

NotificationWatcher::~NotificationWatcher() { stop(); }

void NotificationWatcher::setEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    if (enabled) {
        start();
    } else {
        stop();
    }
    emit enabledChanged();
}

void NotificationWatcher::start() {
    auto stopFlag = std::make_shared<std::atomic<bool>>(false);
    m_stop = stopFlag;
    // runs in the monitoring thread: hand the result to the main thread, where this object lives.
    // stop() sets the flag before this object can go away, so a late delivery is dropped
    Deliver deliver = [this, stopFlag](const QString &desktopEntry, const QString &appName) {
        if (QCoreApplication *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(
                app,
                [this, stopFlag, desktopEntry, appName]() {
                    if (!stopFlag->load()) {
                        emit notified(desktopEntry, appName);
                    }
                },
                Qt::QueuedConnection);
        }
    };
    std::thread(monitor, stopFlag, std::move(deliver)).detach();
}

void NotificationWatcher::stop() {
    if (m_stop) {
        m_stop->store(true);
        m_stop.reset();
    }
}
