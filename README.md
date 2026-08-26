# UGREEN iDX6011 Pro — front-panel LEDs & LCD on a stock Linux distro

Everything in this directory was reverse-engineered from the factory firmware image
`/storage/backups/UGREEN NAS Factory firmware.img` (UGREEN UGOS Pro, kernel `6.12.30+`).
No guessing: every claim links back to a file or binary inside the image.

## Documents

| File | Contents |
|---|---|
| [leds.md](leds.md) | HDD status / power / network LEDs: kernel drivers, sysfs interface, colors, blink triggers, the stock `hwmonitor` daemon's behaviour, ready-to-use scripts |
| [lcd.md](lcd.md) | The front LCD ("mini screen"): hardware pipeline (iGPU eDP panel), backlight driver, the `mini_screen` app internals and its gRPC data sources, how to drive it from plain Linux. Includes §6: the capacitive **touch layer** (AiXieSheng AXS15205, `axs_ts` I²C/ACPI CUST0000, vendor kernel module) |

## TL;DR quick start

```sh
# 1. Get the vendor kernel modules into your system (see "Extracting modules")
cp -r <extracted>/kernel/drivers/ugreen /lib/modules/$(uname -r)/kernel/drivers/
depmod -a

# 2. Load everything for this exact model
modprobe leds-mcu                 # front-panel MCU LED driver
modprobe ledtrig-normal-ht32f52231   # adds trigger name "normal"
modprobe ledtrig-breath-ht32f52231   # adds trigger name "breath"
modprobe ledtrig-timer2-ht32f52231   # adds trigger name "timer2"
modprobe ledtrig-netdev2             # adds trigger name "netdev2"
modprobe ug_idx6011pro-sio        # backlight + fans + watchdog + wake settings
modprobe ug_gpio_btn              # power button as input device
modprobe ug_sataio_beep           # beeper

# 3. Drive an LED
echo normal >  /sys/class/leds/disk1/trigger
echo 3      >  /sys/class/leds/disk1/color       # 3 = green (see leds.md)
echo 255    >  /sys/class/leds/disk1/brightness

# 4. Backlight for the LCD
cat /sys/class/backlight/mipi_backlight/brightness     # 0-100, firmware default 80
```

The LCD itself needs **no special driver**: it is a MIPI/eDP panel on the Intel
integrated GPU and appears as a regular DRM/fbdev output (`eDP-1`, 960×258 rotated
to 258×960) with any modern kernel. Only the *backlight* is vendor-specific. The
**touch screen** is an I²C combo controller needing the vendor `axs_touch` module —
both covered in [lcd.md](lcd.md).

## Extracting modules from the firmware image

```sh
losetup -f --show -P -r "/storage/backups/UGREEN NAS Factory firmware.img"   # -> /dev/loopN
mount -o ro /dev/loopNp2 /mnt/fw-root            # p2 = "UGREEN-ROOTFS" ext4
mount -o ro,loop /mnt/fw-root/kernel.squashfs /mnt/fw-kernel
# modules are at:
ls /mnt/fw-kernel/usr/lib/modules/6.12.30+/kernel/drivers/ugreen/
```

All files referenced below (in `<img:sq-fw>` style) were read from these mounts:

* `/tmp/opencode/mnt/sq-fw/usr/sbin/ug-load-drive.sh` — canonical per-model module list
* `/tmp/opencode/mnt/sq-kernel/usr/lib/modules/6.12.30+/kernel/drivers/ugreen/*.ko`
* `/tmp/opencode/mnt/sq-root/usr/sbin/hwmonitor{-6011pro,-480t,-idx}` — LED automation daemon
* `/tmp/opencode/mnt/sq-root/usr/bin/mini_screen`, `usr/lib/systemd/system/miniscreen_serv.service`
* `/tmp/opencode/mnt/sq-root/etc/default/dx*.conf`, `etc/led.conf`, `etc/fan.conf`, `etc/power.conf`, `etc/.backlight`

## Kernel compatibility warning

Every UGREEN module reports vermagic `6.12.30+ SMP preempt mod_unload modversions`.
They will only load against that exact kernel build (the one shipped in the image,
part 1 of the disk). On a different distro kernel you must either:

* boot the UGREEN kernel (`vmlinuz` + `initrd.img` from partition p1), or
* rebuild the drivers from source — see [modules.md](modules.md) for what each one does.

## Related front-panel hardware (bonus)

Documented briefly because they share the same drivers:

* **Power button** — `ug_gpio_btn.ko`: registers a standard Linux input power button.
* **Beeper** — `ug_sataio_beep.ko`: `/proc/nas/beeper`, commands `booton`, `bootoff`,
  `one`, `on`, `off`, `rep <count> <delay_on> <delay_off>`. Alarm logic in `hwmonitor`
  (disk fault / fan fault / over-temp, config `/etc/power.conf`-style disable flags in DB).
* **Fans** — `ug_idx6011pro-sio.ko`: exposes `/proc/nas/fan`; the stock daemon sends
  e.g. `echo set <0-255> > /proc/nas/fan`, `cpu <pwm>`, `coff`, `off`/`on` (temp-based
  cruise thresholds live in `/etc/default/dx*.conf` → copied to `/etc/fan.conf` usage).
  Watch out: fan fault also triggers the beeper unless disabled.
* **Watchdog** — part of `ug_idx6011pro-sio.ko` (ITE IT55xx EC watchdog), module
  param `timeout=<sec>` default 600; registers a standard Linux watchdog device.
* **Wake settings** — `ug_idx6011pro-sio.ko`: `/proc/nas/pwr` (power state read/write),
  `g3wakeup`, `lanwakeup` proc files.
* **Power-on behaviour** — `ugpwproctl -s performance -f` is run by the boot script
  (perf profile switch; modes `performance`, `balanced`).

Module loading is gated per model by DMI product name (`dmidecode -s system-product-name`
== `"iDX6011 Pro"`); on other models UGREEN loads a different LED MCU driver — table in [leds.md](leds.md).
