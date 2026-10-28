# SailQEMU M10 — optional NAT enablement, side by side

These scripts are **not** needed to build the SailQEMU 0.9.0 frontend. They are
for the second half of M10: enabling `-netdev user` in a NEW QEMU binary.

The existing, tested emulator and its RPM remain intact:

`/opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64`

The build creates and installs two additional packages, only after you choose
to install them:

* `sailqemu-libslirp-4.9.5-1.aarch64.rpm` → `/opt/sailqemu/libslirp-4.9.5`
* `sailqemu-qemu-net-10.2.4-1.aarch64.rpm` → `/opt/sailqemu/qemu-10.2.4-net`

No firmware, executable, or package under the original QEMU prefix is replaced.

## On-device build instructions (Sailfish 5.1 aarch64)

Prerequisites: GCC, `meson`, `ninja`, pkg-config, GLib devel, standard RPM
build tools, and at least **4 GB free** before rebuilding QEMU. The previous
successful build needed ~2 GB for temporary source and objects; additional
packaging staging requires free space. Build with `JOBS=2`.

1. Make sure Meson is on PATH (`meson --version`). If missing, check the
   Sailfish repository for `meson` (e.g. `zypper se -s meson`). Your earlier
   QEMU bootstrap used its own temporary Python environment; that environment
   may no longer exist after M4 cleanup.

2. Obtain the **official tagged source** for libslirp 4.9.5. One option:

   ```sh
   cd "$HOME/Downloads/edp17Apps/sailQemu"
   git clone --depth 1 --branch v4.9.5 \
       https://qemu.googlesource.com/libslirp libslirp-v4.9.5
   git -C libslirp-v4.9.5 rev-parse HEAD
   ```

   The expected upstream release commit is
   `390848f47391f412451b2d259d8527fabb51e5e3`. If it differs,
   investigate before compiling. The official tagged tar archive is another
   option: <https://gitlab.freedesktop.org/slirp/libslirp/-/archive/v4.9.5/libslirp-v4.9.5.tar.bz2>.

3. Build an RPM (this command does **not** install anything):

   ```sh
   BASE="$HOME/Downloads/edp17Apps/sailQemu"
   sh ./01-package-libslirp.sh "$BASE/libslirp-v4.9.5" "$BASE"
   ```

4. Install the versioned libslirp RPM (the user authorises this step):

   ```sh
   sudo zypper in "$BASE/packages/sailqemu-libslirp-4.9.5-1.aarch64.rpm"
   PKG_CONFIG_PATH=/opt/sailqemu/libslirp-4.9.5/lib64/pkgconfig pkg-config --modversion slirp
   ```

5. Extract the QEMU 10.2.4 source you kept after M4 cleanup:

   ```sh
   cd "$BASE"
   tar -xf qemu-10.2.4.tar.xz
   ```

6. Build a **second QEMU RPM with SLIRP explicitly enabled**:

   ```sh
   JOBS=2 sh ./02-package-qemu-net.sh "$BASE/qemu-10.2.4" "$BASE"
   ```

   The script requires working `pkg-config` libslirp and verifies the
   compiled binary advertises the `user` network backend. The QEMU binary
   contains RUNPATH pointing to the separate libslirp runtime. It refuses
   to package when RUNPATH or the backend is missing.

7. Install the alternative QEMU package:

   ```sh
   sudo zypper in "$BASE/packages/sailqemu-qemu-net-10.2.4-1.aarch64.rpm"
   python3 ./03-smoke-network.py \
       /opt/sailqemu/qemu-10.2.4-net/bin/qemu-system-x86_64
   ```

   The smoke test checks that the NAT backend loads and a VM with rtl8139
   can start and respond to QMP. It does **not** prove guest Internet access.

8. In SailQEMU, set the **QEMU executable** path for a test profile to:

   `/opt/sailqemu/qemu-10.2.4-net/bin/qemu-system-x86_64`

   Then enable **Enable guest networking** in that profile and boot Windows XP.
   Confirm the guest receives a DHCP address and can reach the network. Keep
   a separate profile using the known-good original QEMU while testing.

## Recovery / limitations

* The existing non-network QEMU RPM is never removed or replaced.
* The scripts leave extracted source/build directories in place. Clean them
  only **after** checking network guest boots and the installed packages.
* The libslirp shared library is dynamically linked from a private `/opt`
  directory. Both RPMs are needed for the NAT-enabled QEMU. Don't move the
  libslirp prefix without rebuilding QEMU.
* This is a device-native RPM build, **not** a signed OBS/source-RPM release.
  Please keep the downloaded upstream sources and scripts for provenance.
* Old Windows guest operating systems are no longer secure to expose to the
  Internet. Prefer a test-only NAT configuration; do not forward inbound ports.
