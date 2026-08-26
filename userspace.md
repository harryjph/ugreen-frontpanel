# Kernel-independent control: reverse-engineering notes

Goal: run current/mainline kernels, where the vendor `.ko`s (vermagic
`6.12.30+`) cannot load. Strategy per subsystem:

| Function | Path forward |
|---|---|
| LCD pixels | nothing to do — plain `i915` eDP output |
| Backlight | small amount of EC port I/O (reverse from `ug_idx6011pro-sio.ko`; it even logs its own actions). Alternatively a ~100-line out-of-tree backlight driver via DKMS |
| Touch | reimplement minimal probe+interrupt handling as own DKMS module (the hard part is only the reset/init sequence + MT report parsing — all visible in `axs_touch.ko`, which is fully symbolised); firmware-upgrade/debug features optional |
| HDD LEDs | **no kernel code needed** if the MCU wire protocol is reversed — the transport is ordinary SMBus over `/dev/i2c-*`. Findings below |

All of this is reconstruction work on GPL binaries (`license="GPL v2"` in every
module), so requesting the sources from UGREEN support is also a legitimate
shortcut — the `.ko`s retain full symbol tables which makes binary analysis
fast regardless.

## LEDs (`leds-mcu.ko`) — protocol status: CONFIRMED FRAME LAYOUT, payloads partially mapped

Everything below was extracted by disassembling `leds-mcu.ko`
(symbols intact; see functions named). Device on iDX6011 Pro answers at
i²c address **0x3a** (probe list `normal_i2c[] = {0x3a}`).

### Framing (verified in code)

* Every transfer is an 11-byte blob whose last two bytes are a checksum:
  `chksum_be16 = bswap16( sum(bytes[0..8]) )`.
  * Writes: `sum(payload[0..8]) << ... rol $0x8,%dx` before storing — see
    `i2c_iDX601x_write_led+0x64..0x76`.
  * Reads: identical validation loop summing reply[0..8] against the
    big-endian word at reply[9..10] — `i2c_iDX601x_read_led+0xb1..0xcd`.
* Transport = SMBus:
  * write: `i2c_smbus_write_block_data(client, CMD, 11, buf)`
  * read:  `i2c_smbus_read_i2c_block_data(client, ADDR_CMD, 11, buf)`
* After every successful operation the driver pokes a confirmation/ack:
  `i2c_smbus_read_byte(client, 0x80)` expecting `== 1`
  (retried up to 3×, 5 ms apart — same retry wrapper around main transfers).
* Per-transfer retry policy: 3 attempts, sleep 5 ms between.

### Known commands

| Op | Transaction | Source |
|---|---|---|
| Chip-id / detect | `read_word_data(CMD=0x5A)` must return `0xB2C5` | `leds_ugreen_detect` |
| Clear-all (shutdown notifier) | `write_block_data(CMD=0x0A, {0x0A,0x55})` | `ug_notifier_call` |
| LED status readback for LED #k | `read_i2c_block_data(CMD=(k-0x7F)&0xFF, 11)` | `i2c_iDX601x_read_led` |
| LED set | `write_block_data(CMD=IDX, 11-byte frame)` where frame starts `{0x1A,0x00,0x00,0x01,...}` | `i2c_iDX601x_write_led` |

### LED set-frame layout (byte offsets within the 11-byte block)

```
[0] 0x1A        constant magic for LED-set ops
[1] 0x00        constant
[2] 0x00        constant
[3] 0x01        constant (param-count / version?)
[4] param A     op-dependent       <- call-site args edx..
[5] param B     op-dependent       <- ecx
[6] param C     op-dependent       <- r8b
[7] param D     op-dependent       <- r9b
[8] param E     op-dependent       <- stack slot
[9..10]         BE16 checksum of [0..8]
```

Call sites observed (via wrappers):

* solid-on path   : A=0x03, B=0xFF, C=D=E=0x00  (`leds_blink_set` fall-through)
* blink-off/dark  : A=0x04, B=C=D=E=0x00
* brightness work : A=0x01, B=on/off byte   (`brightness_work`: B∈{0,1}, or 0xFF latched)
* read-reply parse (mirrors what readback should return):
  `[1]`→on/off state, `[4..6]`→24-bit RGB colour, `[7..8]`→BE16 delay_on,
  `[9..10 of frame]`→BE16 delay_off — i.e. colours are transported as real
  RGB triples, matching `led_color_table` (`FFFFFF DC2800 FF0000 00FF00 0000FF`)
  being applied before transmit.

### Status of payload mapping

Map is *structural*, not yet bit-exact for every op: colour-write, breath-on
and delay-packing paths pass their values through cached fields
(`led_data+0xC4 = (delay_off<<16)|delay_on`, `+0xC1 = op-state`,
`+0xCC/0xCA = rgb`) and then re-extract bytes for transmit — careful single-stepping
of `leds_blink_set_custom` / `color_set` / `leds_brightness_set` will pin the exact
slots. Remaining unknowns are concentrated in:

1. which param slot carries R/G/B vs delay hi/lo per op-id,
2. the full op-id list (at least 1, 3, 4, 5 seen),
3. whether CMD byte is always the LED index (brightness_work stores index at
   `led_data+0xC0` and passes it as first arg — yes for that path).

### How to finish it empirically (one boot of the stock system)

On the vendor kernel once, capture ground truth and compare with the static map:

```sh
# 1. watch live traffic while toggling LEDs through UGOS UI
modprobe i2c-stub      # not needed on hw; use ftrace instead:
trace-cmd stream -e 'smbus:*' -e i2c:i2c_write -e i2c:i2c_result &
echo 255 > /sys/class/leds/disk1/brightness ; echo 3 > /sys/class/leds/disk1/color
echo timer2 > /sys/class/leds/disk1/trigger ; echo 200 > ...delay_on
```

or simpler, replay-and-compare with `i2cdetect`/`i2cset` interactively now that
checksum + magic constants are known — any guessed byte that breaks checksums
tells you instantly via NAK/timeout behaviour.

### Userspace controller sketch (kernel-version independent)

```python
#!/usr/bin/env python3
import struct, smbus2                      # pip install smbus2 (or use i2c-tools)
bus = smbus2.SMBus(0)                      # adapter number TBD: scan 0..N for 0x3a
ADDR = 0x3a

def led_frame(a, b, c, d, e):
    p = bytes([0x1A, 0, 0, 1, a, b, c, d, e])
    return p + struct.pack('>H', sum(p))

def set_led(idx, *params):
    bus.write_i2c_block_data(ADDR, idx, led_frame(*params))
    assert bus.read_byte(ADDR) == 1        # ACK convention
```

(Adapter discovery: iterate `/dev/i2c-*`, send the detect word-read `0x5A`; the
responder that answers `B2 C5` is the panel MCU.)

## Recommended build order

1. Finish the frame map (capture approach above, ~an evening).
2. LEDs-only userspace daemon + small udev rule — zero kernel risk, works on any kernel.
3. Backlight: dump EC port sequence from `ug_idx6011pro-sio.ko` (symbols +
   log strings `MIPI backlight: Set brightness to %d` make this tractable);
   wrap in tiny script or submit as proper platform driver later.
4. Touch: port `axs_touch.ko` forward as your own DKMS module; probe/reset/MT
   parsing logic all recoverable from the (symbolised) binary. Optional longer
   term: ask UGREEN for GPL source drop covering `drivers/ugreen/*` + axs_touch.
