# Driving the front-panel LEDs

The `ugreen-leds-mcu` DKMS package provides standard Linux LED class devices
driven by the panel MCU over I²C:

```
/sys/class/leds/power/
/sys/class/leds/network_stat/
/sys/class/leds/network_stat2/
/sys/class/leds/disk1/ … /sys/class/leds/disk6/
```

(For reference, other UGREEN models expose fewer/similar sets — DXP2800 has
only `disk1`-`disk2`, the 8-bay DXP8800 family up to `disk8`.)

## Attributes

### `brightness` — on/off + intensity

```sh
echo 255 > /sys/class/leds/disk3/brightness   # on
echo 0   > /sys/class/leds/disk3/brightness   # off
```

### `color` — palette index (not RGB!)

| value | colour | typical use |
|---|---|---|
| `0` | white  | idle / healthy |
| `1` | orange | brand/activity accent |
| `2` | red    | fault |
| `3` | green  | ok/access |
| `4` | blue   | locate |

### `trigger` — behaviour engine

| trigger | behaviour | extra attributes |
|---|---|---|
| `none` + brightness | solid | — |
| `normal`            | steady (MCU-native) | — |
| `breath`            | hardware breathing fade | — |
| `timer2`            | blink at custom rates | `delay_on`, `delay_off` (ms) |
| `netdev2`           | NIC activity mirror | `device_name`, `interval`, `link`, `tx`, `rx` |

Upstream kernel triggers (`heartbeat`, `timer`, …) also work, but only toggle
brightness: native colours and breath effects require the vendor triggers.

## Recipes

```sh
# All six disks solid white (idle look)
for d in disk{1..6}; do
    echo normal > /sys/class/leds/$d/trigger
    echo 0      > /sys/class/leds/$d/color
    echo 255    > /sys/class/leds/$d/brightness
done

# LAN LEDs mirror the two NICs
echo netdev2 > /sys/class/leds/network_stat/trigger;  echo eth0 > .../device_name
echo netdev2 > /sys/class/leds/network_stat2/trigger; echo eth1 > .../device_name

# Locate disk 4 (blue flash)
echo timer2 > /sys/class/leds/disk4/trigger
echo 4 > /sys/class/leds/disk4/color
echo 300 > /sys/class/leds/disk4/delay_on
echo 1200 > /sys/class/leds/disk4/delay_off

# Colour test sweep
for d in power network_stat network_stat2 disk{1..6}; do
    for c in 0 1 2 3 4; do
        echo $c > /sys/class/leds/$d/color
        echo normal > /sys/class/leds/$d/trigger
        echo 255 > /sys/class/leds/$d/brightness
        sleep 0.4
    done
done
```

Read-only diagnostics per LED: `mode` (active MCU mode), `state`
(last programmed state), `version` (MCU firmware version).

Notes:
* The driver caches/coalesces state ("already set, needn't write"); don't
  hammer the sysfs files in tight loops.
* The kernel clears all LEDs cleanly via a reboot notifier.

## Automation

[`examples/hdd-leds.sh`](examples/hdd-leds.sh) is a drop-in daemon that maps
block-device presence to LED states and blinks on I/O (the white-sol /
blink-on-access look of the factory firmware). Wire it to systemd or your init
of choice; hook `mdadm --monitor`/smartd for fault → red if you run arrays.
