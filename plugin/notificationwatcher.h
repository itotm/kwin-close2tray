#ifndef NOTIFICATIONWATCHER_H
#define NOTIFICATIONWATCHER_H

#pragma once

#include <QObject>
#include <atomic>
#include <memory>

// Reports which application sent a desktop notification.
//
// A private session bus connection becomes a monitor (like dbus-monitor) for the two
// ways notifications reach Plasma: org.freedesktop.Notifications.Notify from regular
// applications, and the portal backend AddNotification for sandboxed ones (Flatpak).
// Only the application id and name and the website origin are read, never the content of the notification.
class NotificationWatcher : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

  public:
    explicit NotificationWatcher(QObject *parent = nullptr);
    ~NotificationWatcher() override;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

  signals:
    void enabledChanged();
    // desktopEntry: desktop file name or application id; origin: website of a browser notification.
    // Any of them may be empty
    void notified(const QString &desktopEntry, const QString &appName, const QString &origin);

  private:
    void start();
    void stop();

    bool m_enabled = false;
    // shared with the monitoring thread, which outlives this object for a moment when stopping
    std::shared_ptr<std::atomic<bool>> m_stop;
};

#endif
