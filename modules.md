# Vendor kernel modules reference

Location inside firmware: `kernel.squashfs` →
`usr/lib/modules/6.12.30+/kernel/drivers/ugreen/`
(mount recipe in [README.md](README.md)).

All descriptions below were derived from module metadata, strings, symbol tables
and rodata of the exact `.ko` files shipped. Common vermagic:
`6.12.30+ SMP preempt mod_unload modversions`.

## LED drivers (pick ONE per model)

| Module | MCU / bus | Models | Notes |
|---|---|---|---|
| `leds-mcu.ko` | HT32F52231-class @ i²c 0x3a | **iDX6011**, iDX6012, DXP6800/-S, DXP8800, DXP2800 (JW rev), FORT… | DMI-keyed name tables (`iDX6011_led_name`, …); registers reboot notifier to clear LEDs |
| `leds-mcu-28a48.ko` | same class | DX4600/DX4700, DXP4800(S/Plus/Pro), DXP2800S, EM_DXP2800 | verbose probe logs `LEDMCU-28A48:`; chipid check; CRC-checked I²C block protocol |
| `leds-mcu-68a88.ko` | same class | DXP8800 Pro/Ultra, FORT 6 | alias of the above family ("ugreen_ht32f52231 driver") |
| `leds-mcu-n76e003.ko` | Nuvoton N76E003 | DXP480T Plus | + breath/normal triggers variant |
| `leds-mcu-n76e003-dh2600.ko` | N76E003 | DH2600 | |

All produce the same LED-class interface documented in [leds.md](leds.md).

## LED triggers

| Module | Trigger string it registers | Purpose |
|---|---|---|
| `ledtrig-normal-ht32f52231.ko` | `normal` | steady state, MCU-native "solid" op-mode |
| `ledtrig-breath-ht32f52231.ko` | `breath` | hardware breathing fade |
| `ledtrig-timer2-ht32f52231.ko` | `timer2` | blink with `delay_on`/`delay_off` ms attrs (like upstream timer but via MCU) |
| `ledtrig-netdev2.ko` | `netdev2` | like upstream netdev trigger: `device_name`, `interval`, `link`, `tx`, `rx` attrs; polls stats |

(Description strings: "breathing LED trigger", "normaling LED trigger",
"Timer2 LED trigger", "Netdev LED trigger".)

## Front-panel / platform companions

| Module | Provides | Key details |
|---|---|---|
| `ug_gpio_btn.ko` | power button as Linux input device | GPIO remapped, long-press handled by ACPI/firmware too |
| `ug_idx6011pro-sio.ko` (**this model**) | `/sys/class/backlight/mipi_backlight`, `/proc/nas/{fan,pwr,g3wakeup,lanwakeup}`, watchdog | ITE IT55xx EC over ports; watchdog param `timeout=` (default 600 s); fans via EC commands (`set_cpu_fan`, `set_sys_fan`) |
| `ug_idx6011-sio.ko` | non-Pro iDX6011 platform bits | no backlight |
| `ug_sataio_beep.ko` | beeper `/proc/nas/beeper` | ops: `booton`, `bootoff`, `one`, `on`, `off`, `rep <n> <on> <off>` |
| `ug_it86x-sio.ko`, `ug_it86x-sio-dh2600.ko` | ITE SIO: `/proc/it86/{fan,startup}` hwmon-ish control | used by smaller desktop models |
| `ug_it86x-cpufan.ko` | CPU-fan split control for Plus/Pro models | |
| `ug_sata_beep-dx4700.ko`, `ug_sataio_beep` variants | disk fault beep wiring | |
| `ug-beeper-dxp480t.ko` | beeper for DXP480T | input `num` handling logs |

## Userspace pieces that use all this

* `/usr/sbin/hwmonitor{-6011pro,…}` — LED policy daemon (see leds.md §5)
* `/usr/sbin/FlowingLeds` — boot animation script (pure sysfs)
* `/usr/bin/mini_screen` — LCD UI (see lcd.md)
* `storage_serv` (`com.ugreen.storagemgr`) — only talks gRPC/dbus to hwmonitor/ctl_serv,
  never touches sysfs directly; safe to replace wholesale.

## Rebuilding against a distro kernel

Nothing here is exotic: i2c-core LED class, standard procfs/sysfs APIs, plus port
I/O for the IT55 EC. A DKMS-style repackage is realistic. The hard part is
finding source — UGREEN doesn't publish it (GPL headers present: modules are
`license=GPL v2`; you can request sources or reconstruct from disassembly — the
binaries retain full symbols and even some original file paths, e.g.
`drivers/ugreen/it55_helper.c`).
