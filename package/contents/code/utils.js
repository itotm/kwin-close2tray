function parseRules(string) {
    try {
        const rules = JSON.parse(string || "[]");
        return Array.isArray(rules) ? rules.filter(rule => rule.enabled && rule.windowClass) : [];
    } catch (e) {
        console.warn("close2tray: invalid rules " + e);
        return [];
    }
}

function debug(message) {
    if (debugEnabled) {
        // warning level: distributions like Fedora hide debug output
        console.warn("close2tray: " + message);
    }
}

function isValidWindow(window) {
    return window.minimizable && window.normalWindow &&
        !window.dialog && !window.modal && !window.specialWindow &&
        !window.appletPopup && !window.splash && !window.utility && !window.transient;
}

function dumpProps(obj) {
    for (var k of Object.keys(obj)) {
        const val = obj[k];
        if (k === 'metaData')
            continue;
        print(k + "=" + val + "\n");
    }
}

function matchRule(window) {
    const names = [window.resourceClass, window.resourceName, window.desktopFileName]
        .filter(name => name)
        .map(name => name.toLowerCase());
    return rules.find(rule => names.includes(rule.windowClass.toLowerCase()));
}

// Applications like Thunderbird or Electron ones show their window again right after
// opening it: for a while after start, windows sent to the tray are kept there
const startInTrayGraceMs = 10000;
const startingInTray = {};

function isStartingInTray(window) {
    const until = startingInTray[window.internalId];
    if (until === undefined) return false;
    if (Date.now() < until) return true;
    delete startingInTray[window.internalId];
    return false;
}

// Rules apply to the main window of an application, the one opened while no other was: windows it opens
// later (a message being written in Thunderbird, a web app popup) stay normal windows
const mainWindows = {};

function appKey(window) {
    return (window.desktopFileName || window.resourceClass || "").toLowerCase();
}

function markMainWindow(window) {
    const key = appKey(window);
    const hasMain = Workspace.windows.some(w => w.internalId !== window.internalId && mainWindows[w.internalId] && appKey(w) === key);
    mainWindows[window.internalId] = !hasMain;
    if (hasMain) debug(`${window.resourceClass}: secondary window, rules not applied`);
}

function isMainWindow(window) {
    return !!mainWindows[window.internalId];
}

// Windows this script has hidden in the tray, until something shows them again
const hiddenInTray = {};

// Signal connections made from JS outlive the script, they are tracked to drop them on unload
const windowConnections = {};

function connectWindow(window, signal, handler) {
    signal.connect(handler);
    const id = window.internalId;
    windowConnections[id] = windowConnections[id] || [];
    windowConnections[id].push([signal, handler]);
}

function disconnectWindow(window) {
    const id = window.internalId;
    for (const [signal, handler] of windowConnections[id] || []) {
        try {
            signal.disconnect(handler);
        } catch (e) {
            // the window is already gone
        }
    }
    delete windowConnections[id];
}

function setup(window) {
    if (!isValidWindow(window)) return;
    markMainWindow(window);
    connectWindow(window, window.captionChanged, () => {
        updateToolTip(window);
    });
    connectWindow(window, window.desktopsChanged, () => {
        updateToolTip(window);
    });
    connectWindow(window, window.outputChanged, () => {
        updateToolTip(window);
    });
    connectWindow(window, window.demandsAttentionChanged, () => {
        updateDemandsAttention(window);
    });
    connectWindow(window, window.minimizedChanged, () => {
        onMinimizeChanged(window);
    });
    connectWindow(window, window.iconChanged, () => {
        updateWindowIcon(window);
    });
    connectWindow(window, window.skipTaskbarChanged, () => {
        keepInTray(window);
    });
    connectWindow(window, window.activeChanged, () => {
        if (window.active) clearNotified(window);
    });
    applyRule(window);
}

// For X11 windows KWin resets skip taskbar to the value requested by the application whenever it updates
// the window visibility (e.g. when the monitor goes to sleep): a window hidden in the tray would show up
// in the taskbar again. Skip pager and switcher have no such reset.
function keepInTray(window) {
    if (!hiddenInTray[window.internalId] || !window.minimized || window.skipTaskbar) return;
    debug(`${window.resourceClass} came back to the taskbar while in the tray, hiding it again`);
    window.skipTaskbar = true;
    window.skipSwitcher = true;
}

function teardown(window) {
    delete startingInTray[window.internalId];
    delete mainWindows[window.internalId];
    delete hiddenInTray[window.internalId];
    disconnectWindow(window);
    removeTrayIcon(window);
    closeInterceptor.setIntercepted(window, false);
}

// Script disabled: windows hidden in the tray would be unreachable without their icon
function teardownAll() {
    notificationWatcher.enabled = false;
    for (const window of Workspace.windows) {
        const wasInTray = window.internalId in trayIcons;
        // disconnect first, keepInTray would hide the window again
        teardown(window);
        if (wasInTray) {
            setSkip(false, window);
        }
    }
}

// Close button interception follows the rules, also when they change at runtime
function applyRule(window) {
    if (!isValidWindow(window)) return;
    const rule = matchRule(window);
    const intercept = !!rule && !!rule.closeToTray && isMainWindow(window);
    if (intercept !== closeInterceptor.isIntercepted(window)) {
        debug(`${intercept ? "intercepting" : "releasing"} close button of ${window.resourceClass}`);
        closeInterceptor.setIntercepted(window, intercept);
    }
}

function formattedWindowInfo(w) {
    let desktops;
    if (w.onAllDesktops) {
        desktops = "All Desktops";
    } else {
        desktops = w.desktops.map(desktop => desktop.name).join(", ");
    }
    return `${w.caption || w.resourceName || w.resourceClass}\n${desktops} @ ${w.output.name}`;
}

function addTrayIcon(window) {
    const windowId = window.internalId;
    if (windowId in trayIcons) {
        return;
    }
    let launcherUrl = window.desktopFileName;
    launcherUrl = "application://" + launcherUrl;
    launcherUrl = launcherUrl + ".desktop";

    const trayItem = trayIconComponent.createObject(root, {
        "icon": window.icon,
        "windowId": windowId,
        "toolTipText": formattedWindowInfo(window),
        "launcherUrl": launcherUrl,
        "xdgName": window.desktopFileName,
        "countUseDot": countUseDot
    });
    trayItem.requestShowHide.connect(toggleShowHide);
    trayItem.requestClose.connect(closeWindow);
    trayItem.requestUnpin.connect(unpinIcon);
    trayIcons[windowId] = trayItem;
}

function removeTrayIcon(window) {
    const windowId = window.internalId;
    if (!(windowId in trayIcons)) {
        return;
    }
    trayIcons[windowId].destroy();
    delete trayIcons[windowId];
}

function setMinimize(minimize, window) {
    window.minimized = minimize;
}

function setSkip(skip, window) {
    window.skipPager = skip;
    window.skipTaskbar = skip;
    window.skipSwitcher = skip;
}

function toggleMinimize(windowId) {
    const window = getWindow(windowId);
    if (!window) return;
    window.minimized = !window.minimized;
}

function toggleShowHide(windowId) {
    const window = getWindow(windowId);
    if (!window) return;
    // the user asked for the window, stop holding it in the tray
    delete startingInTray[windowId];

    window.minimized = !window.minimized;
    if (!window.minimized) Workspace.activeWindow = window;

    if (window.minimized) {
        hiddenInTray[windowId] = true;
        setSkip(true, window);
    } else {
        delete hiddenInTray[windowId];
        setSkip(false, window);
    }
}

function sendToTray(window) {
    if (!isValidWindow(window)) return;
    debug(`sending ${window.resourceClass} to tray`);
    hiddenInTray[window.internalId] = true;
    addTrayIcon(window);
    setSkip(true, window);
    setMinimize(true, window);
}

function onCloseRequested(windowId) {
    const window = getWindow(windowId);
    if (!window) return;
    sendToTray(window);
}

function onMinimizeChanged(window) {
    if (!window.minimized && isStartingInTray(window)) {
        // the application showed itself while starting, put it back once its request is done
        debug(`${window.resourceClass} showed itself while starting, back to tray`);
        const windowId = window.internalId;
        Qt.callLater(() => {
            const w = getWindow(windowId);
            if (w) sendToTray(w);
        });
        return;
    }
    if (!window.minimized) {
        clearNotified(window);
        // restored by the application itself or by KWin, show it in the taskbar again.
        // Not only when skipTaskbar is set: for X11 windows KWin has already reset it
        delete hiddenInTray[window.internalId];
        if (window.internalId in trayIcons) {
            setSkip(false, window);
        }
        return;
    }
    const rule = matchRule(window);
    if (rule && rule.minimizeToTray && isMainWindow(window)) {
        sendToTray(window);
    } else if (window.internalId in trayIcons && hideOnMinimize) {
        hiddenInTray[window.internalId] = true;
        setSkip(true, window);
    }
}

function getWindow(windowId) {
    return Workspace.windows.find(w => w.internalId === windowId);
}

function closeWindow(windowId) {
    const window = getWindow(windowId);
    if (!window) return;
    window.closeWindow();
}

// Alt+F4 replacement: tray for windows with close to tray, real close otherwise
function closeOrSendToTray(window) {
    if (!window) return;
    // dialogs are not valid windows, so they keep closing normally
    const rule = matchRule(window);
    if (rule && rule.closeToTray && isValidWindow(window) && isMainWindow(window)) {
        sendToTray(window);
    } else {
        window.closeWindow();
    }
}

function unpinIcon(windowId) {
    const window = getWindow(windowId);
    if (!window) return;
    delete startingInTray[windowId];
    delete hiddenInTray[windowId];
    setMinimize(false, window);
    setSkip(false, window);
    removeTrayIcon(window);
    Workspace.activeWindow = window;
}

function updateToolTip(window) {
    if (window.internalId in trayIcons) {
        trayIcons[window.internalId].toolTipText = formattedWindowInfo(window);
    }
}

function updateWindowIcon(window) {
    if (window.internalId in trayIcons) {
        trayIcons[window.internalId].icon = window.icon;
    }
}

function updateDemandsAttention(window) {
    if (window.internalId in trayIcons) {
        trayIcons[window.internalId].demandsAttention = window.demandsAttention;
    }
}

// How closely a notification names the application of a window: 3 desktop file, 2 instance, 1 class, 0 not at all
function notificationMatch(window, names) {
    const fields = [window.desktopFileName, window.resourceName, window.resourceClass];
    for (let i = 0; i < fields.length; i++) {
        if (fields[i] && names.includes(fields[i].toLowerCase())) return fields.length - i;
    }
    return 0;
}

// Windows of web apps installed from a Chromium based browser: chrome-<app id>-<profile>, brave-…, msedge-…
function isWebAppWindow(window) {
    return /^[a-z]+-[a-p]{32}-/.test(window.desktopFileName || "");
}

// Parts of a website name too common to identify a web app (mail.google.com must not mark Outlook's "Mail")
const genericOriginLabels = ["www", "m", "mail", "web", "app", "apps", "cloud", "online", "login", "my", "home", "office"];

// Browsers send every notification under their own name, web apps included, and tell the website in the
// origin (e.g. outlook.office.com): the web app is the one whose title names the site
function webAppsForOrigin(origin) {
    const host = origin.toLowerCase().split(/[/:]/)[0];
    const labels = host.split(".").slice(0, -1).filter(label => label.length > 1 && !genericOriginLabels.includes(label));
    const webApps = Workspace.windows.filter(window => isValidWindow(window) && isWebAppWindow(window));
    for (const label of labels) {
        const word = new RegExp(`\\b${label.replace(/[^a-z0-9]/g, "\\$&")}\\b`);
        const matches = webApps.filter(window => word.test(window.caption.toLowerCase()));
        if (matches.length > 0) return matches;
    }
    return [];
}

// A notification from the application of a window in the tray marks its icon until the window is seen
function onNotification(desktopEntry, appName, origin) {
    const matches = (origin && webAppsForOrigin(origin)) || [];
    if (matches.length === 0) {
        matches.push(...windowsForNotificationName(desktopEntry, appName));
    }
    for (const window of matches) {
        if (!(window.internalId in trayIcons) || (window.active && !window.minimized)) continue;
        debug(`notification from ${window.resourceClass}`);
        trayIcons[window.internalId].notified = true;
    }
}

// Only the windows matched most closely count: Chrome web apps all have the class google-chrome on X11, and
// a notification naming a single web app, or Chrome itself, must not mark the others
function windowsForNotificationName(desktopEntry, appName) {
    const names = [desktopEntry, appName].filter(name => name).map(name => name.toLowerCase());
    const matches = [];
    let best = 0;
    for (const window of Workspace.windows) {
        if (!isValidWindow(window)) continue;
        const match = notificationMatch(window, names);
        if (match === 0 || match < best) continue;
        if (match > best) {
            best = match;
            matches.length = 0;
        }
        matches.push(window);
    }
    // matched only by a class shared by different applications: no way to tell which one sent it
    const apps = new Set(matches.map(window => window.desktopFileName || window.resourceName));
    if (best === 1 && apps.size > 1) {
        debug(`notification from ${[...apps].join(", ")}: ambiguous, ignored`);
        return [];
    }
    return matches;
}

function clearNotified(window) {
    const trayIcon = trayIcons[window.internalId];
    if (trayIcon && trayIcon.notified) {
        debug(`notification of ${window.resourceClass} seen`);
        trayIcon.notified = false;
    }
}

function setupAutoHide(window) {
    if (!isValidWindow(window)) return;
    const rule = matchRule(window);
    if (rule && rule.startInTray && isMainWindow(window)) {
        startingInTray[window.internalId] = Date.now() + startInTrayGraceMs;
        sendToTray(window);
    }
}
