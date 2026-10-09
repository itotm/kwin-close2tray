#!/bin/sh
# Runs inside the Debian container started by build-deb.sh
set -eu

apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
    cmake extra-cmake-modules g++ dpkg-dev \
    qt6-base-dev qt6-declarative-dev \
    libkf6statusnotifieritem-dev libkf6service-dev libkf6kcmutils-dev \
    libkf6config-dev libkf6package-dev libkdecorations3-dev >/dev/null

mkdir -p /build/src
tar -xzf /sources/source.tar.gz -C /build/src
cmake -S /build/src -B /build/b -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build /build/b -j"$(nproc)"

pkg=/build/pkg
DESTDIR=$pkg cmake --install /build/b
find $pkg -name '*.so' -exec strip --strip-unneeded {} +
install -Dm644 /build/src/LICENSE $pkg/usr/share/doc/kwin-close2tray/copyright

# dpkg-shlibdeps wants a debian/control next to it
mkdir -p /build/shlibs/debian
printf 'Source: kwin-close2tray\n\nPackage: kwin-close2tray\nArchitecture: any\n' >/build/shlibs/debian/control
cd /build/shlibs
libdeps=$(dpkg-shlibdeps -O $(find $pkg -name '*.so') 2>/dev/null | sed -n 's/^shlibs:Depends=//p')

mkdir -p $pkg/DEBIAN
cat >$pkg/DEBIAN/control <<CONTROL
Package: kwin-close2tray
Version: $VERSION
Architecture: $(dpkg --print-architecture)
Maintainer: itotm <1804363+itotm@users.noreply.github.com>
Installed-Size: $(du -sk --exclude=DEBIAN $pkg | cut -f1)
Depends: $libdeps, kwin-common (>= 4:6.3)
Section: kde
Priority: optional
Homepage: https://github.com/itotm/kwin-close2tray
Description: KWin script that sends windows to the system tray
 Hide windows to the system tray instead of the taskbar. Windows can be sent
 to the tray with a shortcut, or automatically when they are minimized, when
 the close button of the title bar is clicked, or as soon as they open.
 Fork of kwin-minimize2tray by Luis Bocanegra.
CONTROL

dpkg-deb --root-owner-group --build $pkg "/out/kwin-close2tray_${VERSION}_$(dpkg --print-architecture).deb"
