#!/bin/sh
# Separate QEMU 10.2.4 NAT build and package; never replaces original QEMU.
set -eu
usage() { echo "Usage: $0 /path/to/qemu-10.2.4 [sailQemu-base]" >&2; exit 2; }
[ "$#" -ge 1 ] && [ "$#" -le 2 ] || usage
SRC=$(cd "$1" && pwd -P)
[ "$(tr -d '\r\n' < "$SRC/VERSION")" = '10.2.4' ] || usage
BASE=${2:-"$HOME/Downloads/edp17Apps/sailQemu"}
mkdir -p "$BASE/packages"
BASE=$(cd "$BASE" && pwd -P)
BUILD="$BASE/qemu-10.2.4-sailqemu-m10-net-build"
PREFIX=/opt/sailqemu/qemu-10.2.4-net
SLIRP=/opt/sailqemu/libslirp-4.9.5
[ -x "$SRC/configure" ] || usage
[ -f "$SLIRP/lib64/pkgconfig/slirp.pc" ] || { echo "Install sailqemu-libslirp 4.9.5 RPM first" >&2; exit 1; }
for cmd in make pkg-config rpmbuild rpm tar strip readelf mktemp; do
  command -v "$cmd" >/dev/null || { echo "Missing tool: $cmd" >&2; exit 1; }
done
export PKG_CONFIG_PATH="$SLIRP/lib64/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --atleast-version=4.9.5 slirp || { echo 'libslirp pkg-config dependency cannot be found' >&2; exit 1; }
FREE=$(df -Pm "$BASE" | awk 'NR==2 {print $4}')
[ "$FREE" -ge 3900 ] || { echo "Need >= 3900 MiB free to start network-enabled QEMU build ($FREE available)" >&2; exit 1; }
[ ! -d "$BUILD/meson-info" ] || { echo "Build directory exists; investigate/reuse before reconfiguring: $BUILD" >&2; exit 1; }
mkdir -p "$BUILD"
# The packaged libslirp lives under /opt and cannot be found by ld.so alone.
# Embed its stable path in the QEMU binary at link time (RUNPATH).
export LDFLAGS="${LDFLAGS:+$LDFLAGS }-Wl,-rpath,$SLIRP/lib64"
( cd "$BUILD" && "$SRC/configure" \
  --prefix="$PREFIX" --libdir=lib64 --target-list=x86_64-softmmu \
  --enable-system --disable-user --enable-tcg --disable-kvm \
  --enable-sdl --enable-dbus-display --enable-gio --enable-pixman \
  --disable-gtk --disable-opengl --disable-spice --disable-vnc \
  --disable-rust --disable-modules --disable-guest-agent \
  --disable-docs --disable-werror --disable-download --enable-slirp )
make -C "$BUILD" -j "${JOBS:-2}"
BIN="$BUILD/qemu-system-x86_64"
[ -x "$BIN" ] || { echo 'Built binary missing' >&2; exit 1; }
readelf -d "$BIN" | grep -E '(RUNPATH|RPATH)' | grep -F "$SLIRP/lib64" >/dev/null || {
  echo "QEMU build does not embed SLIRP runtime path. Do not package." >&2; exit 1; }
"$BIN" -netdev help 2>&1 | grep -Eq '(^|[[:space:]])user([[:space:]]|$)' || { echo 'Built QEMU lacks user/NAT netdev' >&2; exit 1; }
WORK=$(mktemp -d "$BASE/.m10-qemu-net.XXXXXXXX")
trap 'rm -rf -- "$WORK"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
STAGE="$WORK/stage"
TOP="$WORK/rpmbuild"
mkdir -p "$STAGE" "$TOP/SOURCES" "$TOP/SPECS" "$TOP/BUILD" "$TOP/RPMS" "$TOP/BUILDROOT" "$TOP/SRPMS"
DESTDIR="$STAGE" make -C "$BUILD" install
INST="$STAGE$PREFIX"
[ -x "$INST/bin/qemu-system-x86_64" ] && [ -f "$INST/share/qemu/bios.bin" ] || {
  echo 'Missing staged QEMU executable or SeaBIOS firmware' >&2; exit 1; }
for name in qemu-system-x86_64 qemu-img qemu-io qemu-nbd qemu-storage-daemon qemu-pr-helper; do
  F="$INST/bin/$name"
  [ ! -f "$F" ] || strip --strip-unneeded "$F"
done
readelf -d "$INST/bin/qemu-system-x86_64" | grep -E '(RUNPATH|RPATH)' | grep -F "$SLIRP/lib64" >/dev/null || {
  echo 'Staged QEMU has lost its SLIRP runtime path' >&2; exit 1; }
( cd "$STAGE" && tar -czf "$TOP/SOURCES/qemu-net-staged.tar.gz" opt/sailqemu/qemu-10.2.4-net )
cat > "$TOP/SPECS/sailqemu-qemu-net.spec" <<'SPEC'
Name: sailqemu-qemu-net
Version: 10.2.4
Release: 1
Summary: QEMU x86_64 SDL and D-Bus emulator with user NAT backend
License: GPL-2.0-or-later
Group: Applications/System
Source0: qemu-net-staged.tar.gz
Requires: sailqemu-libslirp = 4.9.5-1
%description
Versioned alternative QEMU binary with SLIRP enabled for SailQEMU,
installed alongside the existing QEMU 10.2.4 package.
%prep
%build
%install
mkdir -p %{buildroot}
tar -xzf %{_sourcedir}/qemu-net-staged.tar.gz -C %{buildroot}
%files
/opt/sailqemu/qemu-10.2.4-net
SPEC
rpmbuild -bb --define "_topdir $TOP" "$TOP/SPECS/sailqemu-qemu-net.spec"
RPM="$TOP/RPMS/$(rpm --eval '%{_target_cpu}')/sailqemu-qemu-net-10.2.4-1.$(rpm --eval '%{_target_cpu}').rpm"
[ -f "$RPM" ] || { echo 'RPM not created' >&2; exit 1; }
DEST="$BASE/packages/$(basename "$RPM")"
[ ! -e "$DEST" ] || cmp -s "$RPM" "$DEST" || { echo "Refusing to overwrite existing RPM: $DEST" >&2; exit 1; }
cp -p "$RPM" "$DEST"
echo "QEMU NAT RPM saved: $DEST"
sha256sum "$DEST"
echo "Next: sudo zypper in '$DEST'"
echo 'The old /opt/sailqemu/qemu-10.2.4 directory has not been changed.'
