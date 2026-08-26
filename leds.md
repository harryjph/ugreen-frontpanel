# Front-panel LEDs — iDX6011 Pro (and every other UGREEN model)

## 1. Architecture

The LEDs are **not** wired to the chipset. They hang off a dedicated MCU on the
front panel PCB (HT32F52231-class, Holtek). The host talks to it over **I²C**
(scan address list in the driver: `0x3a`, terminated by `I2C_CLIENT_END`).
UGREEN ships one kernel driver per MCU variant; each driver implements the
standard Linux LED class so everything below lives in `/sys/class/leds/`.

There is no userspace protocol to reverse: the entire interface is sysfs.

```
iDX6011 Pro front panel                    host
┌─────────────────────────────┐        ┌──────────────────────┐
│ HT32F52231 MCU              │◄──I²C──│ leds-mcu.ko          │
│  ├─ power LED               │  0x3a  │  + triggers:         │
│  ├─ network_stat (LAN1)     │        │    normal / breath / │
│  ├─ network_stat2 (LAN2)    │        │    timer2 / netdev2  │
│  └─ disk1 … disk6           │        └──────────────────────┘
└─────────────────────────────┘
```

## 2. LED inventory

Extracted from the per-model name tables baked into `drivers/ugreen/leds-mcu.ko`
(symbol `iDX6011_pro_led_name` etc., stride = 24-byte C strings):

| Model | Table | LEDs |
|---|---|---|
| **iDX6011 Pro** *(this device)* | `@0x180` | `power`, `network_stat`, `network_stat2`, `disk1`…`disk6` |
| iDX6011 | `@0x260` | `power`, `network_stat`, `disk1`…`disk6` |
| iDX6012 | `@0x380 area` | `power`, `network_stat`, `network_stat2`, `disk1`… (12-bay set) |
| DXP2800/S | `@0x320` | `power`, `network_stat`, `disk1`, `disk2` |
| DXP6800/DXP8800 family | `@0x0c0` | `power`, `network_stat`, `disk1`…`disk6` (+8-disk variants) |

*(Models with N76E003/28a48 MCUs use `leds-mcu-n76e003*` / `leds-mcu-28a48.ko` /
`leds-mcu-68a88.ko` instead of `leds-mcu.ko`; same sysfs interface.)*

So on your unit you will see exactly:

```
/sys/class/leds/power/
/sys/class/leds/network_stat/
/sys/class/leds/network_stat2/
/sys/class/leds/disk1/ … /sys/class/leds/disk6/
```

## 3. Loading the drivers

Canonical model map (verbatim from `<img:sq-fw>/usr/sbin/ug-load-drive.sh`,
`elif [[ "$model" =~ "DX601" ]]` branch):

```sh
modprobe ug_gpio_btn                      # power button input device
modprobe leds-mcu                         # LED MCU driver
modprobe ledtrig-breath-ht32f52231        # trigger "breath"
modprobe ledtrig-normal-ht32f52231        # trigger "normal"
modprobe ledtrig-netdev2                  # trigger "netdev2"
modprobe ledtrig-timer2-ht32f52231        # trigger "timer2"
modprobe ug_idx6011pro-sio                # SIO/EC: backlight, fans, wdt, wake
modprobe ug_sataio_beep                   # beeper
/usr/sbin/ugpwproctl -s performance -f    # perf profile hook (optional)
```

For reference, what *other* models load (so you can spot issues if you ever run a
module from a different unit): DX4600/DX4700 → `leds-mcu-28a48`; DXP4800/-S,
EM_DXP2800 → `leds-mcu-28a48` else `leds-mcu`; DXP480T Plus → `leds-mcu-n76e003`;
DH2600 → `leds-mcu-n76e003-dh2600`; DXP8800*/FORT6 → `leds-mcu-68a88`.
None of the modules take parameters except the watchdog (`timeout=`).

Probe messages worth grepping for (`dmesg`):

* `LEDMCU-28A48: detect success!` / `get chipid:0x..` (28a48 flavour)
* `ugreen_nas_leds` → `leds_ugreen_probe product name is :%s` (`leds-mcu`)
* on failure: `i2c-%d adapter not found`, `Failed to register LED %d`

## 4. The sysfs interface (this is the whole API)

Each LED is a standard LED class device with these attributes:

### `brightness` — on/off + intensity
```sh
echo 255 > /sys/class/leds/disk3/brightness   # full on
echo 0   > /sys/class/leds/disk3/brightness   # off
```
Internally the MCU only gets an on/off + level byte; 255 is what UGREEN uses everywhere.
The driver treats setting brightness as "state = solid", unless a trigger owns the LED.

### `color` — palette index (not RGB!)
Values accepted by all three MCU drivers; table recovered from `.rodata:led_color_table`:

| value | hex colour | note |
|---|---|---|
| `0` | `#FFFFFF` | white — UGREEN's default for idle disks/power |
| `1` | `#DC2800` | UGREEN brand orange/amber |
| `2` | `#FF0000` | red — fault |
| `3` | `#00FF00` | green — healthy/activity |
| `4` | `#0000FF` | blue — locate |

(Raw table bytes read from `.rodata`: `ff ff ff`, `dc 28 00`, `ff 00 00`,
`00 ff 00`, `00 00 ff`.)

```sh
echo 0 > /sys/class/leds/power/color
```

### `trigger` — behaviour engine

Available once the four `ledtrig-*-ht32f52231`/`netdev2` modules are loaded:

| trigger | description | extra attrs |
|---|---|---|
| `none`+`brightness` | static | — |
| `normal` | steady/solid (MCU-native mode) | — |
| `breath` | fade in/out breathing loop | — |
| `timer2` | square-wave blink at custom rates | `delay_on`, `delay_off` (ms) |
| `netdev2` | activity of one NIC: link up/down/tx/rx | `device_name`, `interval`, `link`, `tx`, `rx` |

Examples:

```sh
# blink disk1 red slowly (like UGREEN's "locate")
echo timer2 > /sys/class/leds/disk1/trigger
echo 2 > /sys/class/leds/disk1/color
echo 300 > /sys/class/leds/disk1/delay_on
echo 1200 > /sys/class/leds/disk1/delay_off

# network LED mirrors eth0 link+activity
echo netdev2 > /sys/class/leds/network_stat/trigger
echo eth0 > /sys/class/leds/network_stat/device_name

# breathing amber while scrubbing…
echo breath > /sys/class/leds/disk2/trigger && echo 1 > /sys/class/leds/disk2/color
```

### Read-only diagnostics on each LED
* `mode` — currently active mode string as known by the MCU (`normal`, `breath`,
  `netdev`, `blink`, …)
* `state` — last programmed op-state (on/off/blink phase)
* `version` — reports MCU firmware version (chipid probe result)
* `debug` — module-level debug switch (`cat` shows status)

The driver also registers a **reboot notifier**, so LEDs are forced off cleanly on
reboot/shutdown by the kernel itself.

## 5. What stock UGOS does with them (to replicate the factory look)

Two pieces:

### a) Boot animation — `/usr/sbin/FlowingLeds` (shell script, plain sysfs)
Sets `power` and all `diskN`+`network_stat` LEDs to white, `timer2`-blinks them
sequentially ("flowing") using delays like 200/1800 ms or 300/1200 ms per model.

### b) Steady state — `/usr/sbin/hwmonitor` daemon
systemd unit `/etc/systemd/system/hwmonitor.service` →
`ExecStartPre=/etc/startpre.d/hwmonitor.sh` copies the right binary over
`/usr/sbin/hwmonitor` (**for this model: `/usr/sbin/hwmonitor-6011pro`**) then starts it.

Observed behaviour from strings/logic of `hwmonitor-6011pro`:

* polls `/proc/diskstats` & udev; **disk present** → white solid; **I/O active** → blink;
  **standby** (it sends `ATA_OP_STANDBYNOW` after idle timeout) → LED off-ish dim;
  **fault/secure-erase/etc.** → red (`breath` during rebuild-type ops);
  **locate** request → `locate_disk %d locate_color:%d locate_duration:%d` blink
  (that's the blue flash you get when clicking "locate" in UGOS UI).
* LAN LEDs follow carriers of `/sys/class/net/eth%d/carrier` (`network_stat`,
  `network_stat2` = the two 10G ports).
* Fan/temp supervision with beeper alarms, thresholds from
  `/etc/default/<model>.conf` (key `ledsEnable=1` gates everything, `hwmonitor`
  reads it at startup).

Equivalent minimal drop-in script if you just want present-blink-green, present-white:
see [examples/hdd-leds.sh](examples/hdd-leds.sh) in this directory.

## 6. Common recipes

```sh
# All six disks solid white (idle look)
for d in disk{1..6}; do echo normal > /sys/class/leds/$d/trigger; echo 0 > /sys/class/leds/$d/color; echo 255 > /sys/class/leds/$d/brightness; done

# Locate disk 4 (blue flash) for 60s
( echo 4 > /sys/class/leds/disk4/color; echo timer2 > ... ; sleep 60; restore )

# LED test sweep: cycles each LED through the whole colour palette
for d in power network_stat network_stat2 disk{1..6}; do for c in 0 1 2 3 4; do
  echo $c > /sys/class/leds/$d/color; echo normal > /sys/class/leds/$d/trigger; echo 255 > /sys/class/leds/$d/brightness; sleep 0.4
done; done
```

Persist across boots with a small systemd unit that runs the modprobes above
(`After=multi-user.target`) followed by your desired policy — or install
`ledmon`-style scripts hooked to `udev`/`mdadm --monitor`.

## 7. Gotchas

1. **Trigger names only exist after loading the vendor trigger modules** — plain
   upstream `heartbeat/timer` triggers also work but drive brightness only, they
   cannot change MCU colours or use native breath modes.
2. The MCU supports per-command retry with CRC checks on the I²C bus; rapid writes
   are coalesced by the driver (log line: `is already ,need't to set`). Don't
   spam-bang the file in a loop at kHz rates — batch changes instead.
3. Without `ledtrig-timer2-*` loaded the kernel's built-in `timer` trigger shows
   different attr names (`delay_on` still works via generic core though).
