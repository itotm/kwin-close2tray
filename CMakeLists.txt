cmake_minimum_required(VERSION 3.16)
project("kwin-close2tray")
option(BUILD_PLUGIN "Build the plugin" ON)
option(INSTALL_SCRIPT "Install KWin Script" ON)
option(PACKAGE_SCRIPT "Package KWin Script" OFF)

set(PROJECT_DEP_VERSION "6.0.0")
set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.0.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Get id and version from metadata
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/package/metadata.json METADATA)
string(JSON PLUGIN_ID GET ${METADATA} KPlugin Id)
string(JSON CUR_VERSION GET ${METADATA} KPlugin Version)
message("Plugin Id: ${PLUGIN_ID}")
message("Version: ${CUR_VERSION}")

include(KDEInstallDirs)

if(INSTALL_SCRIPT)
    find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
        Package
    )
    set(SCRIPT_PACKAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/package)
    if(BUILD_PLUGIN AND NOT KDE_INSTALL_USE_QT_SYS_PATHS)
        # KWin looks the settings module up relative to the Qt plugin paths, which
        # don't include a user prefix like ~/.local: point it to the installed file
        set(SCRIPT_PACKAGE_DIR ${CMAKE_CURRENT_BINARY_DIR}/package)
        file(REMOVE_RECURSE ${SCRIPT_PACKAGE_DIR})
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/package DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
        string(JSON METADATA SET ${METADATA} "X-KDE-ConfigModule"
            "\"${KDE_INSTALL_FULL_PLUGINDIR}/kwin/scripts/configs/kcm_close2tray\"")
        file(WRITE ${SCRIPT_PACKAGE_DIR}/metadata.json ${METADATA})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/package/metadata.json)
    endif()
    kpackage_install_package(${SCRIPT_PACKAGE_DIR} ${PLUGIN_ID} scripts kwin)
endif()

if(PACKAGE_PLASMOID)
    set(PLASMOID_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}-v${CUR_VERSION}.plasmoid")
    add_custom_target(plasmoid ALL
        COMMAND rm -rf "${PLASMOID_FILE}"
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/package zip -r ${PLASMOID_FILE} ./
        COMMENT "Generating ${PLASMOID_FILE}"
    )
endif()

if(BUILD_PLUGIN)
    include(KDECMakeSettings)
    find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS CoreAddons)
    add_subdirectory(plugin)
    add_subdirectory(kcm)
endif()
