import QtQuick
import org.kde.kwin
import com.github.itotm.close2tray.trayicon 1.0
import "../code/utils.js" as Utils

Item {
    id: root
    property bool debugEnabled: false
    property var rules: []
    property bool countUseDot: false
    property bool hideOnMinimize: false
    property var trayIcons: new Object()
    // KWin is shutting down, the workspace may already be gone
    property bool quitting: false
    readonly property Component trayIconComponent: TrayIcon {}

    function loadConfig() {
        debugEnabled = KWin.readConfig("debugEnabled", false);
        rules = Utils.parseRules(KWin.readConfig("rules", "[]"));
        countUseDot = KWin.readConfig("countUseDot", false);
        hideOnMinimize = KWin.readConfig("hideOnMinimize", false);
        Utils.debug(`loaded ${rules.length} enabled rules`);
    }

    CloseInterceptor {
        id: closeInterceptor
        onCloseRequested: windowId => Utils.onCloseRequested(windowId)
    }

    Connections {
        target: Options
        function onConfigChanged() {
            root.loadConfig();
            Workspace.windows.forEach(window => Utils.applyRule(window));
        }
    }

    ShortcutHandler {
        name: "Close to tray"
        text: "Minimize window to tray (sets skip Taskbar, Switcher & Pager)"
        sequence: "Meta+Alt+PgDown"
        onActivated: {
            let window = Workspace.activeWindow;
            while (window.transient) {
                window = window.transientFor;
            }
            if (!Utils.isValidWindow(window))
                return;
            Utils.toggleShowHide(window.internalId);
            Utils.addTrayIcon(window);
        }
    }

    ShortcutHandler {
        name: "Close to tray: close or send to tray"
        text: "Close window or send it to tray"
        sequence: ""
        onActivated: Utils.closeOrSendToTray(Workspace.activeWindow)
    }

    Connections {
        target: Workspace
        function onWindowAdded(window) {
            Utils.setup(window);
            Utils.setupAutoHide(window);
        }
        function onWindowRemoved(window) {
            Utils.teardown(window);
        }
    }

    Component.onCompleted: {
        loadConfig();
        Workspace.windows.forEach(window => Utils.setup(window));
    }

    Connections {
        target: Qt.application
        function onAboutToQuit() {
            root.quitting = true;
        }
    }

    Component.onDestruction: {
        if (!quitting) {
            Utils.teardownAll();
        }
    }
}
