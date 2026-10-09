find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS DBus Widgets)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS KCMUtils Config Service)

kcoreaddons_add_plugin(kcm_close2tray SOURCES close2traykcm.cpp INSTALL_NAMESPACE "kwin/scripts/configs")

target_link_libraries(kcm_close2tray
    Qt6::DBus
    Qt6::Widgets
    KF6::KCMUtils
    KF6::ConfigCore
    KF6::Service
)
