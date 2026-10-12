# KWin Close2Tray

> [!NOTE]
> This is a fork of [**kwin-minimize2tray**](https://github.com/luisbocanegra/kwin-minimize2tray) by Luis Bocanegra.
> If you find it useful, please [**support the original author**](https://github.com/sponsors/luisbocanegra).

A KWin script for KDE Plasma 6 that sends windows to the system tray, hiding them from the taskbar, the task switcher (Alt+Tab) and the pager. Clicking the tray icon shows the window again.

## Features

- Per-application rules, each one can be disabled without removing it:
  - **Minimize to tray**: minimizing the window hides it in the tray
  - **Close to tray**: the close button of the title bar hides the window in the tray instead of closing it
  - **Start in tray**: new windows go to the tray as soon as they open
- **Detect Window…** in the settings picks a window with a click, like KDE's Window Rules
- Rule changes apply right away, no need to re-enable the script
- A shortcut sends any window to the tray
- Tray icons show the unread count, the progress and the attention state of the application
- Applications that don't report an unread count, like Thunderbird, get a dot on their tray icon when they send a notification, until the window is shown

## Installation

### Fedora package

Build an RPM in a clean Fedora container (needs `podman`):

```sh
./packaging/build-rpm.sh      # builds on Fedora 44, the package also installs on Fedora 45
```

The package lands in `dist/`. Install it with `sudo dnf install ./dist/kwin-close2tray-*.x86_64.rpm`, or with `rpm-ostree install` / your image's Containerfile on atomic desktops.

### Debian package

For Debian 13 (trixie), build a `.deb` in a clean Debian container (needs `podman`):

```sh
./packaging/build-deb.sh
```

Install it with `sudo apt install ./dist/kwin-close2tray_*+deb13_amd64.deb`.

Both packages are also attached to the [releases](https://github.com/itotm/kwin-close2tray/releases).

### From source

Build dependencies on Fedora:

```sh
sudo dnf install cmake extra-cmake-modules gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel \
    kf6-kstatusnotifieritem-devel kf6-kservice-devel kf6-kcmutils-devel kf6-kconfig-devel \
    kf6-kpackage-devel kdecoration-devel dbus-devel
```

Then:

```sh
git clone https://github.com/itotm/kwin-close2tray.git
cd kwin-close2tray
./install.sh
```

### Immutable distributions

1. Run `./install-immutable.sh` to install
2. Add `QML_IMPORT_PATH` environment variable for the C++ plugin to work:

    Create the file `~/.config/plasma-workspace/env/path.sh` (and folders if they don't exist) with the following:

    ```sh
    export QML_IMPORT_PATH="$HOME/.local/lib64/qml:$HOME/.local/lib/qml:$QML_IMPORT_PATH"
    ```

    The settings page needs nothing extra: for installs outside the system Qt paths the script points KWin to it with an absolute path.

3. Log-out or reboot to apply the change

## Configuration/Usage

Enable/disable

1. *Close to Tray* from *System Settings* > *Window Management* > *KWin Scripts*

Rules are configured from the settings button of the script: use *Detect Window…* and click on a window, or *Add Manually* and type its class. A rule matches the window class, the resource name or the desktop file name, ignoring case.

If you used the original *Minimize to Tray* script, its *Start minimized* classes are imported the first time the settings are opened. Disable the original script, both use the same shortcut.

By default the shortcut is set to `Meta+Alt+PgDown`, to change it:

1. *System Settings* > *Keyboard* > *Shortcuts*
2. From the list on the left select *KWin*
3. Search for "Minimize window to tray" and change it to whatever you prefer

*Close to tray* handles the close button of the title bar. KWin gives scripts no way to intercept Alt+F4, so to make it behave the same:

1. *System Settings* > *Keyboard* > *Shortcuts* > *KWin*
2. Remove Alt+F4 from *Close Window*
3. Assign it to *Close window or send it to tray*: it sends windows with *Close to tray* to the tray and closes all the others

Closing from the task manager or from the window menu still closes the window. To really close a window that goes to the tray, use *Quit* in the menu of its tray icon.

> [!IMPORTANT]
> Close to tray only works with the title bar drawn by KWin. Many applications draw their own (client-side decorations): their close button is part of the application and KWin never sees the click. Switch them to the system title bar, for example:
>
> - **Thunderbird**: set `mail.tabs.drawInTitlebar` to `false` in *Settings › General › Config Editor*
> - **Electron apps** (e.g. Standard Notes): look for an option like "use system title bar" (in Standard Notes it is stored as `useSystemMenuBar`)

> [!NOTE]
> Close to tray works by rewiring the close button of the window decoration from inside KWin. It is tested with Breeze; decorations that don't use the standard KDecoration close button keep closing windows normally.

The unread count and progress support is a simplified version of [KDE's task manager smart launchers](https://github.com/KDE/plasma-desktop/tree/e3ba92b113d8a4e2d47a589835e9d867059dc2b9/applets/taskmanager/plugin/smartlaunchers).
