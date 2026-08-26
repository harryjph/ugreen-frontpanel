# Driver sources, packages and COPR

## Where the sources come from

UGREEN publishes the kernel modules at:

* https://github.com/ugreen-opensource/kernel-6.12 — full kernel tree, incl.
  `drivers/ugreen/` and `drivers/input/touchscreen/axs_touch/`

The DKMS trees in `packages/` were taken from upstream commit `c75c5abd6`
(2026-04-03). All modules are GPL-2.0-only.

## Package → module mapping (from upstream `drivers/ugreen/Makefile`)

| `packages/` dir | DKMS name | Built .ko files | Source files |
|---|---|---|---|
| `ugreen-leds-mcu/` | `ugreen-leds-mcu` | `leds-mcu`, `ledtrig-normal-ht32f52231`, `ledtrig-timer2-ht32f52231`, `ledtrig-breath-ht32f52231` | `leds-mcu.c`, `ledtrig-*.c` |
| `ugreen-sio/` | `ugreen-sio` | `ug_idx6011pro-sio`, `ug_gpio_btn`, `ug_sataio_beep` | `ug_it55pro_functions.c` + `it55_helper.c`, `ug_gpio_btn.c`, `ug_sataio_beeper.c` + `it55_helper.c` |
| `axs-touch/` | `axs-touch` | `axs_touch` | `axs_core/i2c/spi/debug/upgrade/download/gesture/esd.c` |

Upstream also contains drivers for *other* UGREEN models (`leds-mcu-28a48.c`,
`dh2600/`, `dx4700/`, `dxp480t/`, `dxp68a88/`, `ug_it86x-sio.c`,
`leds-sio-201x.c`, …) which we deliberately don't package. The useful
`ledtrig-netdev2.c` trigger is not built by upstream's Makefile; add
`obj-m += ledtrig-netdev2.o` if you prefer it over the standard `netdev`
trigger for the LAN LEDs.

## Local / manual build

Each package directory is self-contained:

```sh
cd packages/<name>
make all KDIR=/lib/modules/$(uname -r)/build      # test compile
sudo make -C ... M=$PWD modules_install           # or use dkms:
sudo dkms add -m <name> -v 1.0.git20260403
sudo dkms build -m <name> -v 1.0.git20260403 -k $(uname -r)
sudo dkms install -m <name> -v 1.0.git20260403 -k $(uname -r)
```

Porting notes when jumping across kernel versions: LED class, i2c smbus,
input core and procfs APIs are stable; expect friction only around the
fbcon/fb-notifier path used by axs_touch blank handling.

⚠ Do **not** pass `KBUILD_CFLAGS+=...` overrides on these Makefiles' command
lines — with newer gcc that changes asm-constraint handling and breaks
compilation ("impossible constraint in asm"). Plain kernel defaults are fine.

## RPM packaging

```
rpm/
├── SPECS/ugreen-frontpanel-dkms.spec   # one noarch RPM shipping all three DKMS trees
├── build-rpm.sh                        # tarballs -> local RPM
└── build-srpm.sh                       # tarballs -> SRPM (for COPR upload)
```

The RPM is deliberately **noarch and compiles nothing at build time**;
compilation happens on the target machine via DKMS in `%post` (and again on
every kernel update). `%post` runs `dkms add/build/install` for each of the
three modules against the running kernel.

## Building on COPR

The repo is COPR-native via the `make_srpm` method:

1. Push this tree to GitHub (it must contain `.copr/Makefile`,
   `rpm/SPECS/*.spec`, `packages/*`).
2. Create a project and submit a *Git/SCM* build with SRPM build method
   **make_srpm**, or via CLI:
   ```sh
   copr-cli buildscm --srpm-buildmethod make_srpm \
       --clone-url https://github.com/<you>/ugreen-frontpanel <project>
   ```
3. `.copr/Makefile` generates the source tarballs on the builder and emits the
   SRPM into Copr's results dir (`$(outdir)`).

Because nothing compiles in COPR's chroot, one build serves every distro the
project declares chroots for; users install the resulting noarch RPM and their
machine builds the kernel modules through DKMS at install time.
