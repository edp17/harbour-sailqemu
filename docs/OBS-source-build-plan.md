# SailQEMU OBS source-build plan (pre-beta planning)

## Goal and boundaries

Build all distributed executable code from reviewable, pinned source revisions
on an Open Build Service (OBS), for Sailfish OS **5.1** `aarch64` and `armv7hl`.
No staged on-device binaries in the build inputs. Maintain versioned install
prefixes and a reproducible, tested guest configuration.

This plan is **not** evidence that the OBS packages currently build. The QEMU
and libslirp RPM spec files, build-dependency closures and target repositories
must be created and tested. We will retain the independently installed,
functional device-built packages until replacement packages pass validation.

## Three RPM packages

1. `sailqemu-libslirp` 4.9.5 — system/user networking library. Build the
   upstream v4.9.5 tag with Meson from source, with a versioned prefix under
   `/opt/sailqemu/libslirp-4.9.5` if preserving current runtime layout. Ensure
   it provides a usable `libslirp.pc`, shared-library dependencies and dynamic
   loader resolution for QEMU. Do not publish a staged device-built tarball.
2. `sailqemu-qemu` 10.2.4 — build upstream QEMU 10.2.4 with only the necessary
   x86_64 system target, TCG, SDL, D-Bus/GIO, Pixman and system libslirp
   support. Install under an agreed versioned `/opt/sailqemu/` prefix and
   package `qemu-system-x86_64`, `qemu-img` and QEMU runtime data.
   Existing app profiles currently point to
   `/opt/sailqemu/qemu-10.2.4-net/bin/qemu-system-x86_64`; preserve this path,
   or implement a deliberate migration before releasing OBS-built packages.
3. `harbour-sailqemu` — this Qt 5.6/Sailfish Silica frontend; keep it in its own
   package, with separate runtime requirements for the supported QEMU package
   only **after** confirming repository availability and the exact package names.

## Proven local build flags to translate to the OBS spec

Local project build scripts used `--target-list=x86_64-softmmu`,
`--enable-tcg`, `--disable-kvm`, `--enable-sdl`, `--enable-dbus-display`,
`--enable-gio`, `--enable-pixman`, `--disable-download`, `--disable-rust`,
`--disable-opengl`, `--disable-vnc`, `--disable-docs` and an enabled libslirp
backend. Use `--enable-slirp=system` where supported, with the dependency
installed for the target architecture. Package build commands must run in
OBS, never fetch/build binaries during RPM `%install` from the network.

**This is a conversion of local working flags, not a tested OBS recipe.**
QEMU Meson/Ninja tooling, host-side Python modules, GLib development headers,
Pixman, SDL2, GIO, libslirp pkg-config discovery, transitive dependencies,
versioned lib directories, linker rpath and build-job memory must be checked
against the real Sailfish OS 5.1 OBS build root.

## Version pinning and OBS wiring

Use a permanent Git tag or commit for each upstream package, and keep source
checksums. Use OBS source services (`obs_scm`, `tar`, optionally `recompress`)
or the Sailfish Chum documented Git source service; pin an immutable `revision`
for beta builds. Store the corresponding `.spec` in the package Git repo's
`rpm/` directory. Maintain an SBOM/source and license inventory where
appropriate. Avoid mutable latest-branch checkouts for release RPMs.

Build in dependency order `libslirp → QEMU → SailQEMU`, declaring
`BuildRequires` for required development packages and `Requires` for
runtime shared libraries. Source-build packages for **both** architectures;
`armv7hl` is a package architecture, but OBS repository XML may use `armv8el`
as its worker architecture (see Chum examples). Do not confuse that mapping
with an AArch64 package target.

Use a personal OBS home project or a separate testing subproject initially;
add Sailfish 5.1 architecture targets and any package repository providing
required build dependencies. On successful dependency and smoke tests,
consider publication in the SailfishOS:Chum testing channel.

## Validation gates

* Both architectures: source/RPM license and dependency audit; clean OBS
  build; install/uninstall check; `ldd`/loader paths; program versions and
  QEMU `-display help` / backend capability check.
* Boot a FreeDOS image using the frontend with display and keyboard input.
* Verify Windows 95 QCOW1 attachment and PS/2 relative pointer behaviour.
* Verify Windows XP and ReactOS 0.4.16 guest boot, guest mouse and safe
  shutdown; verify NAT through the separately packaged libslirp build.
* Verify `qemu-img` inspect/create/convert and pre-existing profiles.
* Ensure the frontend RPM **does not** bundle QEMU or libslirp binaries.
* Publish beta only after both CPU architectures pass the relevant smoke
  tests. Runtime quality on ARM32 is a separate question from build success.

## Upstream references

* https://docs.sailfishos.org/Services/Development/Open_Build_Service/
* https://docs.sailfishos.org/Tools/Sailfish_SDK/Building_packages/
* https://github.com/sailfishos-chum/main/blob/main/GettingStarted.md
* https://www.open-build-service.org/help/manuals/obs-user-guide/cha-obs-best-practices-scm-integration.html
* https://qemu.googlesource.com/libslirp/ (tag v4.9.5)
