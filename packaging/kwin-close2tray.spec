Name:           kwin-close2tray
Version:        0.1.0
Release:        1%{?dist}
Summary:        KWin script that sends windows to the system tray when minimized or closed

License:        GPL-3.0-only
URL:            https://github.com/itotm/kwin-close2tray
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake(KF6CoreAddons)
BuildRequires:  cmake(KF6Config)
BuildRequires:  cmake(KF6KCMUtils)
BuildRequires:  cmake(KF6Package)
BuildRequires:  cmake(KF6Service)
BuildRequires:  cmake(KF6StatusNotifierItem)
BuildRequires:  cmake(KDecoration3)

# the script and its QML plugin run inside KWin
Requires:       kwin-common%{?_isa} >= 6.0

%description
Hide windows to the system tray instead of the taskbar. Windows can be sent
to the tray with a shortcut, or automatically when they are minimized, when
the close button of the title bar is clicked, or as soon as they open.
Fork of kwin-minimize2tray by Luis Bocanegra.

%prep
%autosetup

%build
%cmake_kf6
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_datadir}/kwin/scripts/com.github.itotm.close2tray/
%{_qt6_qmldir}/com/github/itotm/
%{_qt6_plugindir}/kwin/scripts/configs/kcm_close2tray.so

%changelog
* Fri Oct 09 2026 itotm <1804363+itotm@users.noreply.github.com> - 0.1.0-1
- First release of the fork: close to tray, per-application rules, window detection in the settings
