# Official UGREEN module sources on GitHub

**Both** front-panel driver families have published sources:

* https://github.com/ugreen-opensource/kernel-6.12  — full UGREEN kernel tree incl. `drivers/ugreen/`
* https://github.com/ugreen-opensource/linux-headers-6.12.30 — matching headers

This supersedes most of [userspace.md](userspace.md): instead of reversing the
wire protocol, you can now build these modules for **any kernel you like**
(via DKMS) — including recent mainline ones, subject to the porting notes below.

## What you need for the iDX6011 Pro

Mapping from `drivers/ugreen/Makefile` (verbatim):

| Module (.ko) | Source files |
|---|---|
| `leds-mcu` | `drivers/ugreen/leds-mcu.c` (+ `leds.h`) — LED MCU driver used by **iDX6011/Pro**, DXP6800/-S, DXP8800, DXP2800-JW |
| `ledtrig-normal-ht32f52231` | `ledtrig-normal-ht32f52231.c` |
| `ledtrig-timer2-ht32f52231` | `ledtrig-timer2-ht32f52231.c` |
| `ledtrig-breath-ht32f52231` | `ledtrig-breath-ht32f52231.c` |
| `ug_idx6011pro-sio` | `ug_it55pro_functions.c` + `it55_helper.{c,h}` — backlight (`mipi_backlight`), fans, watchdog, wake settings |
| `ug_gpio_btn` | `ug_gpio_btn.c` |
| `ug_sataio_beep` | `ug_sataio_beeper.c` + `it55_helper.c` |

Touch screen:

```
drivers/input/touchscreen/axs_touch/
├── axs_core.c/h     probe, IRQ, MT-B input registration, suspend/resume
├── axs_i2c.c        transport
├── axs_debug.c      sysfs debug attrs (rawdata/diff/rw_reg/version…)
├── axs_upgrade.c    firmware upgrade engine (AXS15205 flash)
├── axs_download.c   RAM app download
├── axs_gesture.c / axs_esd.c
├── axs_config.h / axs_platform.h
└── firmware/trans_bin_to_i.py    fw-packaging helper
```

Other models' drivers are also present (`leds-mcu-28a48.c`, `dh2600/`,
`dx4700/`, `dxp480t/`, `dxp68a88/`, `ug_it86x-sio.c`, `leds-sio-201x.c`,
`ryzen-smu-amkillam/`, plus `tools/` and `tests/` under `drivers/ugreen/`).

Note: `ledtrig-netdev2.c` exists in-tree but is **not listed in the Makefile**
(built out upstream?). The netdev2 trigger it provides is only needed if you
want NIC-linked LEDs via the trigger rather than `hwmonitor` logic — add
`obj-m += ledtrig-netdev2.o` if required, or substitute the standard upstream
`netdev` trigger at the cost of losing whatever tweaks netdev2 added.

## Building against latest kernels

Nothing here uses exotic kernel APIs (LED class, i2c smbus, procfs/sysfs,
input core, fb notifier). Sketch:

```sh
git clone https://github.com/ugreen-opensource/kernel-6.12
cd kernel-6.12/drivers/ugreen

cat > Kbuild <<'EOF'
obj-m := leds-mcu.o ledtrig-normal-ht32f52231.o ledtrig-timer2-ht32f52231.o \
         ledtrig-breath-ht32f52231.o ug_gpio_btn.o ug_idx6011pro-sio.o \
         ug_sataio_beep.o axs_touch.o
ccflags-y := -I$(src)
axs_touch-objs   := ../input/touchscreen/axs_touch/axs_core.o ../input/touchscreen/axs_touch/axs_i2c.o
# extend objs list per feature (debug/upgrade/gesture/esd/download)
ug_idx6011pro-sio-objs := ug_it55pro_functions.o it55_helper.o
ug_sataio_beep-objs := ug_sataio_beeper.o it55_helper.o
EOF

make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo make -C /lib/modules/$(uname -r)/build M=$PWD modules_install && depmod -a
```

(Or wrap as a proper DKMS package so every kernel update rebuilds automatically.)

Expected porting friction points when jumping far ahead of 6.12:

1. **LED class API** — stable; low risk.
2. **fbcon/fb notifier** — if anything churned it's this path (used by
   `axs_touch` blank handling); mechanical fix.
3. **proc_create/perms** — cosmetic API drift.
4. `iDX601x_MCU_DEBUG` etc. guards reference internal headers — define
   stubs if unset already.

Load order after install (same as firmware):

```sh
modprobe leds-mcu ledtrig-normal-ht32f52231 ledtrig-breath-ht32f52231 \
        ledtrig-timer2-ht32f52231 ug_gpio_btn ug_idx6011pro-sio ug_sataio_beep
# touch autoloads via ACPI modalias acpi:CUST0000 once axs_touch is installed+depmod'd
echo 80 > /sys/class/backlight/mipi_backlight/brightness
```

## Cross-check against my disassembly findings (userspace.md)

The shipped-firmware `.ko`s match the GitHub protocol: chip-id read
reg `0x5A` == `0xC5B2`, ACK register `0x80` returning 1, additive BE16
checksum over payload, 3× retry with 5 ms sleep. One divergence worth knowing:
the `.ko` inside your factory image writes LED-set frames with magic byte
`0x1A` where current GitHub `leds-mcu.c` uses template `{0xA0,0x01,...}` with
the op-status byte at offset 3 — i.e., the image ships a **older/different
build** than the public tree (frame offsets shifted). Practical implication:
if you ever do drive the MCU from pure userspace, first verify your MCU's
generation with the chip-id word-read, and validate a single brightness write
against whichever framing applies — don't assume the two agree.

Also updated consequence: `userspace.md` remains useful as a wire-level
reference and fallback, but building the official sources via DKMS is now the
recommended primary route.
