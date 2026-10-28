#!/bin/sh
# Build a side-by-side libslirp 4.9.5 RPM, no root and no system installation.
set -eu
usage() { echo "Usage: $0 /path/to/libslirp-v4.9.5 [sailQemu-base]" >&2; exit 2; }
[ "$#" -ge 1 ] && [ "$#" -le 2 ] || usage
[ -f "$1/meson.build" ] || usage
SRC=$(cd "$1" && pwd -P)
BASE=${2:-"$HOME/Downloads/edp17Apps/sailQemu"}
mkdir -p "$BASE/packages"
BASE=$(cd "$BASE" && pwd -P)
PREFIX=/opt/sailqemu/libslirp-4.9.5
BUILD="$BASE/libslirp-4.9.5-m10-build"
for cmd in meson ninja pkg-config rpmbuild rpm tar strip mktemp; do
  command -v "$cmd" >/dev/null || { echo "Missing $cmd (Meson is available as a Sailfish package)." >&2; exit 1; }
done
pkg-config --exists glib-2.0 || { echo 'glib2-devel/pkgconfig(glib-2.0) missing' >&2; exit 1; }
grep -Eq "version *: *'4[.]9[.]5'" "$SRC/meson.build" || { echo 'Expected libslirp 4.9.5 sources' >&2; exit 1; }
[ ! -e "$BUILD/meson-info" ] || { echo "Build already configured at $BUILD. Reuse it with ninja -C $BUILD." >&2; exit 1; }
meson setup "$BUILD" "$SRC" --prefix="$PREFIX" --libdir=lib64 --buildtype=release --wrap-mode=nodownload
ninja -C "$BUILD" -j 2
WORK=$(mktemp -d "$BASE/.m10-libslirp.XXXXXXXX")
trap 'rm -rf -- "$WORK"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
STAGE="$WORK/stage"
TOP="$WORK/rpmbuild"
mkdir -p "$STAGE" "$TOP/SOURCES" "$TOP/SPECS" "$TOP/BUILD" "$TOP/RPMS" "$TOP/BUILDROOT" "$TOP/SRPMS"
DESTDIR="$STAGE" meson install -C "$BUILD"
LIB="$STAGE$PREFIX/lib64/libslirp.so.0"
[ -e "$LIB" ] || { echo "Shared library missing: $LIB" >&2; exit 1; }
# Install matching licensing information without modifying upstream files.
mkdir -p "$STAGE$PREFIX/share/doc"
[ ! -f "$SRC/COPYRIGHT" ] || cp "$SRC/COPYRIGHT" "$STAGE$PREFIX/share/doc/"
[ ! -f "$SRC/LICENSE" ] || cp "$SRC/LICENSE" "$STAGE$PREFIX/share/doc/"
( cd "$STAGE" && tar -czf "$TOP/SOURCES/libslirp-staged.tar.gz" opt/sailqemu/libslirp-4.9.5 )
cat > "$TOP/SPECS/sailqemu-libslirp.spec" <<'SPEC'
Name: sailqemu-libslirp
Version: 4.9.5
Release: 1
Summary: Versioned SLIRP NAT backend for SailQEMU QEMU
License: BSD-3-Clause AND MIT
Group: System/Libraries
Source0: libslirp-staged.tar.gz
%description
Side-by-side libslirp runtime and development files under /opt/sailqemu.
%prep
%build
%install
mkdir -p %{buildroot}
tar -xzf %{_sourcedir}/libslirp-staged.tar.gz -C %{buildroot}
%files
/opt/sailqemu/libslirp-4.9.5
SPEC
rpmbuild -bb --define "_topdir $TOP" "$TOP/SPECS/sailqemu-libslirp.spec"
RPM="$TOP/RPMS/$(rpm --eval '%{_target_cpu}')/sailqemu-libslirp-4.9.5-1.$(rpm --eval '%{_target_cpu}').rpm"
[ -f "$RPM" ] || { echo 'RPM not created' >&2; exit 1; }
DEST="$BASE/packages/$(basename "$RPM")"
[ ! -e "$DEST" ] || cmp -s "$RPM" "$DEST" || { echo "Refusing to overwrite $DEST" >&2; exit 1; }
cp -p "$RPM" "$DEST"
echo "RPM saved: $DEST"
sha256sum "$DEST"
echo "Install this dependency before rebuilding QEMU: sudo zypper in '$DEST'"
