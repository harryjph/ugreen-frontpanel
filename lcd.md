# The front LCD — display, backlight and touch

## What the hardware is

The display is a **MIPI/eDP LCD strip (960×258, used portrait) on the Intel
integrated GPU** with an integrated **capacitive touch controller**
(AiXieSheng AXS15205) plus a backlight driven by the EC (`ugreen-sio` package).

That means: pixels are handled entirely by the normal graphics stack
(`i915` → DRM → `/dev/fb0` or X11/Wayland). Only backlight and touch need our
DKMS modules.

## Backlight

Provided by `ug_idx6011pro-sio` (in the `ugreen-sio` package):

```sh
cat /sys/class/backlight/mipi_backlight/brightness     # firmware default: 80
echo 50 > /sys/class/backlight/mipi_backlight/brightness   # 0-100
```

There is no persistence in the driver — save/restore your value at boot if you
care. Without the module loaded the panel still works; only brightness control
is unavailable.

## Display

Any modern kernel drives the panel as output `eDP-1`:

```sh
modetest -c | grep eDP            # libdrm diagnostics (optional)
xrandr --output eDP-1 --mode 258x960 --pos 0x0 --primary
```

* Native panel is 960×258 shown portrait; if your compositor presents it
  sideways, add `video=eDP-1:panel_orientation=right_side_up` to the kernel
  cmdline (or use xrandr rotation).
* Use DPMS "off" + backlight=0 as your screensaver; there is no separate
  standby mode worth wiring up.
* Because it's just an ordinary display you have full freedom: console, a
  tiny X session, a Wayland compositor window, or drawing straight to
  `/dev/fb0` for a custom status strip UI.

## Touch layer

The touch controller is bound by the `axs_touch` module (in the `axs-touch`
package). It registers itself automatically through its ACPI modalias — no
manual configuration needed:

```sh
ls /dev/input/eventTS                 # stable symlink from the packaged udev rule
evtest /dev/input/eventTS             # multi-touch events while touching the strip
```

Properties:

* standard **multi-touch protocol B** evdev device named `axs_ts`
* wake-capable IRQ; suspends cleanly with the system
* coordinates match the native touch grid 1:1 — no calibration needed when
  the display runs in its native orientation; add a libinput calibration or
  X11 `TransformationMatrix` only if you rotate the content differently
* debug interfaces exist under the i2c client sysfs dir
  (`axs_driver_rawdata`, `axs_rw_reg`, …); avoid them unless debugging
* firmware upgrades (`firmware_flash.bin` / `firmware_app.bin`) are supported
  by the driver but only relevant if UGREEN publishes new controller firmware

## Typical full setup check-list

1. DKMS packages installed (see [README.md](README.md)).
2. Boot: modules load via `modules-load.d`, touch autoloads via ACPI.
3. `echo N > /sys/class/backlight/mipi_backlight/brightness`.
4. Point whatever UI you want at the framebuffer/X11 as usual.
