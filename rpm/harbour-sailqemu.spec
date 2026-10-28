Name:       harbour-sailqemu
Summary:    Native Sailfish frontend controller for QEMU virtual machines
Version:    0.5.0
Release:    4
Group:      Applications/System
License:    MIT
URL:        https://github.com/edp17/harbour-sailqemu
Source0:    %{name}-%{version}.tar.gz

Requires:   sailfishsilica-qt5
Requires:   SDL2
BuildRequires: cmake
BuildRequires: pkgconfig(sailfishapp)
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Gui)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(pixman-1)

%description
SailQEMU manages QEMU x86_64 guests using Sailfish Silica and QMP.
M6 onward provides a separate D-Bus framebuffer listener for QEMU 10.2.4 that
receives guest scanouts and incremental updates without QMP screenshots.
The zoomable console has native mouse input, guest cursor support, and
a screenshot fallback; legacy SDL mode remains available.
The independently packaged QEMU emulator is not bundled here.

%prep
%setup -q

%build
%cmake .
%make_build

%install
%make_install

%files
%license LICENSE
%{_bindir}/harbour-sailqemu
%{_bindir}/harbour-sailqemu-display-listener
%{_datadir}/harbour-sailqemu/
%{_datadir}/applications/harbour-sailqemu.desktop
%{_datadir}/icons/hicolor/86x86/apps/harbour-sailqemu.png

%config /etc/sailjail/permissions/SailQemuDbus.permission
