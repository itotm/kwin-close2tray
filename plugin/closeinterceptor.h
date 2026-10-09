#ifndef CLOSEINTERCEPTOR_H
#define CLOSEINTERCEPTOR_H

#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUuid>

namespace KDecoration3 {
class Decoration;
class DecorationButton;
} // namespace KDecoration3

// Redirects the close button of a window's server-side decoration.
//
// KWin creates the decoration as a child of the KWin::Window object, and the
// decoration wires each Close button with
//   connect(button, &DecorationButton::clicked, decoration, &Decoration::requestClose)
// That connection is swapped for one that emits closeRequested() instead, so the
// script decides what closing means. Disabling the interception restores it.
class CloseInterceptor : public QObject {
    Q_OBJECT

  public:
    explicit CloseInterceptor(QObject *parent = nullptr);
    ~CloseInterceptor() override;

    // window is a KWin::Window coming from the scripting API
    Q_INVOKABLE void setIntercepted(QObject *window, bool intercepted);
    Q_INVOKABLE bool isIntercepted(QObject *window) const;

  signals:
    void closeRequested(const QUuid windowId);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void scheduleScan(QObject *window);
    void scan(QObject *window);
    void hookDecoration(QObject *window, KDecoration3::Decoration *decoration);
    void hookButton(QObject *window, KDecoration3::Decoration *decoration, KDecoration3::DecorationButton *button);
    void unhookAll(QObject *window);

    struct Hook {
        QPointer<KDecoration3::DecorationButton> button;
        QPointer<KDecoration3::Decoration> decoration;
        QMetaObject::Connection connection;
    };

    // KWin::Window -> hooked close buttons
    QHash<QObject *, QList<Hook>> m_windows;
};

#endif
