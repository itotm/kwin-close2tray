#include "closeinterceptor.h"
#include "trayicon.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class TrayIconPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

  public:
    void registerTypes(const char *uri) override {
        qmlRegisterType<TrayIcon>(uri, 1, 0, "TrayIcon");
        qmlRegisterType<CloseInterceptor>(uri, 1, 0, "CloseInterceptor");
    }
};

#include "trayiconplugin.moc"
