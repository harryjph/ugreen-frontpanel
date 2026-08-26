# UGREEN iDX6011 Pro — front-panel drivers for stock Linux

The NAS's front panel has an LCD strip (with touch), six HDD LEDs, two LAN
LEDs, a power LED, backlight control, a power button and a beeper. All of it
works on ordinary distribution kernels via three DKMS driver packages built
from UGREEN's published sources:

* `ugreen-leds-mcu` — LED class devices in `/sys/class/leds/`
  (`power`, `network_stat`, `network_stat2`, `disk1`…`disk6`)
* `ugreen-sio` — EC/Super-IO platform driver: the LCD backlight device,
  fans, watchdog, wake settings, plus the GPIO power button and SATA beeper
* `axs-touch` — the capacitive touch controller of the LCD

## Install

COPR (recommended):

```sh
dnf copr enable harryjph/ugreen-frontpanel
dnf install ugreen-frontpanel-dkms
```

From this repo:

```sh
make -f .copr/Makefile srpm OUTDIR=.      # or rpm/build-rpm.sh for local RPMs
sudo dnf install ./ugreen-frontpanel-dkms-*.noarch.rpm
```

Installation is automatic: `%post` registers each tree with DKMS, builds
against the running kernel and installs the modules. **Every future kernel
update rebuilds them automatically** — nothing pins you to a specific kernel.

The package also ships:
* `modules-load.d/ugreen-frontpanel.conf` — correct load order at boot
* `udev rule` — stable `/dev/input/eventTS` symlink for touch input

Reboot (or `systemctl restart systemd-modules-load`) and then verify:

```sh
ls /sys/class/leds/                       # power network_stat network_stat2 disk1..6
cat /sys/class/backlight/mipi_backlight/brightness     # default 80 on boot
evtest /dev/input/eventTS                 # touch events when you drag a finger
```

Set your preferred LCD brightness once per boot (e.g. in your setup):

```sh
echo 80 > /sys/class/backlight/mipi_backlight/brightness
```

## Documentation index

| File | Contents |
|---|---|
| [leds.md](leds.md) | Driving the LEDs: colours, triggers, blink rates, automation scripts |
| [lcd.md](lcd.md) | The display itself: eDP pipeline, backlight, touch layer usage |
| [sources.md](sources.md) | Where the driver sources come from, how the DKMS packages & COPR builds work |

For reference documentation of what the hardware *was doing under the factory
firmware*, see the git history of this repository (pre-v2 docs described the
vendor binaries and wire protocol reverse-engineering that made these packages
possible; they were retired once official sources became the path forward).

The LCD picture needs no vendor code at all — it is a plain MIPI/eDP panel on
the Intel iGPU (`eDP-1`, portrait strip). It will show whatever your system
renders there: console, X11/Wayland, or a custom app (see [lcd.md](lcd.md)).
