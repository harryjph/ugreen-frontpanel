# Front LCD ("mini screen") — iDX6011 Pro

## 1. What the hardware actually is

The display is **not** an I²C gadget and needs no special driver for pixels:

* Panel: MIPI/eDP LCD connected to the **Intel integrated GPU** internal eDP output.
  Native panel is 960×258 (ultra-wide strip); UGREEN configures it in portrait as
  `eDP-1` mode `258x960` (Xorg rotates the content).
* Host pipeline: `i915` → DRM → `/dev/fb0` (or X11).
* Backlight: controlled by UGREEN's Super-IO/EC driver `ug_idx6011pro-sio.ko`
  (ITE IT55xx EC, also drives fans/watchdog), which registers a standard
  backlight class device: `/sys/class/backlight/mipi_backlight/`.

So on a stock distro: picture = plug&play; brightness = needs that vendor module
(or you reverse its EC port writes yourself).

The panel is also a **touch screen** — see §6 below for the touch layer
(AiXieSheng AXS15205 combo controller over I²C, driver `axs_touch`).

## 2. Backlight

```sh
modprobe ug_idx6011pro-sio        # registers mipi_backlight (max_brightness=100)
cat /sys/class/backlight/mipi_backlight/max_brightness    # -> 100
echo 80   > /sys/class/backlight/mipi_backlight/brightness
```

Firmware details worth knowing (`ug-load-drive.sh`, logs
`MIPI backlight: Set brightness to %d` / `MIPI backlight: Device registered`):

* default after cold boot: **80**
* userspace value cache lives at `/etc/.backlight`; on every boot the script reads
  it and pokes sysfs:
  ```sh
  echo "$(cat /etc/.backlight)" > /sys/class/backlight/mipi_backlight/brightness
  ```
  → replicate this if you want your setting to survive reboots.
* debug via `cat /sys/module/ug_idx6011pro_sio/parameters/*` not applicable;
  module prints EC command timeouts if the EC misbehaves.

Without this module you can still drive the panel; only brightness control is lost.

## 3. How UGOS drives it — service chain

From `/usr/lib/systemd/system/` + `/etc/systemd/system/multi-user.target.wants/`:

```
startx.service          ExecStart=/usr/bin/startx            (DISPLAY=:0)
   └─ miniscreen_serv.service
        ExecStartPre: wait up to 2.5s for `xdpyinfo` to succeed
        ExecStartPre: xrandr --output eDP-1 --mode 258x960 --pos 0x0 --primary
        ExecStartPre: unbind_VTconsoles.sh      # unbinds vtcon1 from fbcon so
                                                # kernel console text doesn't leak through
        ExecStartPre: dd if=/dev/zero of=/dev/fb0 bs=1M count=1   # clear framebuffers
        ExecStartPre: kill plymouthd
        ExecStart:    DISPLAY=:0 SDL_VIDEODRIVER=x11 /usr/bin/mini_screen
        ExecStopPost: dd zero again over /dev/fb0
        Restart=on-failure, Nice=-20, OOMScoreAdjust=-500
```

Other important facts:

* The whole chain is model-gated twice:
  * `startx.service` has `ExecCondition=[ product_name == "iDX6011 Pro" ]`
  * boot script disables `mini_screen` unless DMI says `iDX6011 Pro`.
* No custom xorg.conf beyond disabling DPMS blank timers (`BlankTime 0`,
  `StandbyTime 0`, `SuspendTime 0`, `OffTime 0`) so the strip never blanks.
* A helper with embedded source path `fix_hdmi_screen_resolution.c` exists inside
  `mini_screen` — HDMI hotplug used to be able to steal eDP modes; not relevant
  unless you attach a monitor.

## 4. Inside `mini_screen` (the vendor UI app)

Binary: `/usr/bin/mini_screen`, ~94 MB, **not stripped**, full DWARF present.
Deps: `libSDL2-2.0.so.0`, `libgrpc.so.7`, `libgrpc-c.so`, `libprotobuf-c`,
`libcjson`, `libzlog`. Built from LVGL (`lv_drivers/display/fbdev.c`) + SDL2.

It is an LVGL "smart screen" UI showing pages such as (from symbol names):
homepage, disk info cards, network info, message/notification page,
AOD (always-on-display) settings, brightness page, wallpaper picker,
shutdown confirm page, registration page.

### Where it gets data: gRPC over unix sockets

| Socket | Service | Methods used |
|---|---|---|
| `/tmp/.cache/ugreen.ctlserv.internal.grpc.sock` | `ctlserv.Internal` | `get_memory_info`, `get_net_info`, `get_speed_info`, `get_system_status`, `get_hardware_fault_reason`, `get_deivce_behavior_status`, `get_display_setting`, `set_display_setting`, `stop_buzzer`, `get_ups_config`, `get_wallpaper_list` |
| same host, separate svcs | `ctlserv.led_screen`, `ctlserv.ups`, `ctlserv.resource`, `ctlserv.net` | screen-specific hardware & UPS RPCs |
| `/tmp/.cache/ugreen.desktop.internal.grpc.sock` | `desktop.DesktopService` | `read_notify`, `get_notify_list`, `watch_notify_change`, `delete_notify`, **`reboot`, `shutdown`** (the strip's power button) |

(Proto types are simple: `common.single_int / single_bool / single_string` etc.)

Periodic refresh callbacks: `async_homepage_update_cb`,
`async_disk_info_update_cb`, `async_network_update_cb`,
`async_message_page_update_cb`, `async_toast_update_cb`,
`async_update_control_pane_cb`.

### So, three options under plain Linux

1. **Use the panel as a real second monitor** — trivial. It shows your console/X/Wayland.
   Remember DPMS-off disabled or add your own; portrait orientation → either accept
   960×258 landscape or set rotation in your compositor/kernel cmdline
   (`video=eDP-1:panel_orientation=right_side_up` typically gives you upright 258×960).
2. **Run the vendor `mini_screen` binary** — only sensible while running the
   UGREEN services (`ctl_serv`, `desktop_serv`) that own those sockets. Doable but
   heavy; you inherit the whole UGOS stack.
3. **Write your own mini-screen UI** against the raw framebuffer:
   * direct: open `/dev/fb0` (LVGL's fbdev backend or `pygame`/Qt/cava-style loops)
   * compositored: SDL2 window on X11 exactly like UGREEN does.
   Any language works; LVGL + Python (lvgl_micropython/pylvgl) is a quick path.

### Gotchas

* VT console bleed-through: UGREEN explicitly unbinds vtconsole1 before drawing
  (`unbind_VTconsoles.sh`: `echo 0 > /sys/class/vtconsole/vtcon1/bind`). Do the
  same if text appears behind/between frames when you draw straight to fbdev.
* Clear both KMS and fb layers: their startup zeroes 1 MiB at `/dev/fb0`.
* The panel must keep a mode assigned even when nothing draws; there is no GPU-side
  off state (screensaver instead dims backlight from the Brightness page → writes
  to `mipi_backlight`).

## 5. Example: minimal "useful strip" setup without UGOS

Skeleton of what works today (nothing exotic required):

```sh
# one-time
video=eDP-1:panel_orientation=right_side_up  # kernel cmdline, or use xrandr rotate
echo 80 > /sys/class/backlight/mipi_backlight/brightness

# simple weather/net-top page with SDL2 on X11
#   or pygame onto /dev/fb0 — 960×258 RGB
```

If you want pixel-exact parity with the factory look, extract page assets from the
binary (`mini_screen` embeds them; strings show wallpaper/theme tables loaded from
RPC `get_wallpaper_list` served by `ctl_serv`'s resource service, files live under
the UGREEN-SERVICE partition `/avatar`, `/wallpaper`, `/guide_icons`).

## 6. Touch layer (the panel is a capacitive touch screen)

### 6.1 Hardware

* Controller: **AiXieSheng AXS15205** — a display+touch *combo* chip (it also
  holds the panel reset/init lines, which is why its driver owns `LCD init/off`).
* Bus: **I²C** on the same internal bus family as the front-panel MCU.
  Firmware identifies as i2c device id **`axs_ts`**, ACPI hardware ID
  **`CUST0000`** (module alias `acpi*:CUST0000:*`, seen in `modules.alias`).
* Reset: GPIO line via the ACPI device (`axs_hw_reset` / `gpiod_*` calls,
  log string `hw reset`).
* Interrupt: dedicated GPIO IRQ, obtained with
  `acpi_dev_gpio_irq_wake_get_by(...)` and served by a threaded handler +
  workqueue (`axs_wq`), wake-capable for resume-from-suspend touches.

Proof points in the image:

```
kernel drivers/input/touchscreen/axs_touch/axs_touch.ko   (UGREEN kernel)
    description = "AXS TouchScreen Driver"
    author      = "AiXieSheng Technology."
    version     = V2.1.7        ; chip routines: axs_Y15205_download/_upgrade
/etc/udev/rules.d/99-touchscreen.rules:
    SUBSYSTEM=="input", ATTRS{name}=="axs_ts", SYMLINK+="input/eventTS"
```

### 6.2 Kernel driver

Driver: `drivers/input/touchscreen/axs_touch/axs_touch.ko` (out-of-tree vendor
code, GPL, vermagic `6.12.30+` like everything else). There is **no AXS driver
in mainline Linux**, so porting/copying this module is mandatory if you want
touch to work under your distro.

On UGOS it is not loaded explicitly — udev coldplug auto-loads it because the
ACPI node advertises modalias `acpi:CUST0000:` and the module carries that alias.
Same will happen on your distro once the `.ko` is installed + `depmod` run
(and you're running a compatible kernel).

Input interface:

* registers an evdev multi-touch device named **`axs_ts`**
  (`/dev/input/event*`; symlink `/dev/input/eventTS` from the udev rule above),
* standard MT protocol B: `input_mt_init_slots` / `input_mt_report_slot_state`,
* absolute axes configured with `input_set_abs_params` at probe time → X/Y
  match the native touch coordinates of the 960×258 panel strip,
* finger release is synthesised centrally (`axs_release_all_finger`),
* suspend/resume handled (`axs_ts_suspend/_resume`, `irq_disable/_enable`),
* FB notifier: on screen blank/suspend (`FB_BLANK`) it powers the combo chip's
  LCD part down (`axs_lcd_off`) and re-inits on unblank (`axs_lcd_init`,
  `axs_reset_and_lcd_init`) — display and touch live/die together.

Debug/tuning sysfs attached to the I²C client (`axs_debug_create_sysfs`):

| attribute | use |
|---|---|
| `axs_rw_reg` | raw register read/write against the controller (`hex_to_int` parsing; protocol strings like `read_cmd`, `read_sfr_cmd`) |
| `axs_driver_rawdata` | dump raw touch data |
| `axs_driver_diff` | diff-mode sensor deltas |
| `axs_driver_version` | show driver vs firmware version (`driver version = %s firmware version = 0x%x`) |
| `axs_hw_reset` | echo to force hardware reset |
| `axs_upgrade_bin` | firmware upgrade: echo trigger → `request_firmware("firmware_flash.bin")`, flash erase/read-check/write cycle |
| `axs_dlapp_bin` | app firmware download path (`request_firmware("firmware_app.bin")`, RAM download) |

Plus a procfs file created by `axs_create_proc_file` (mode 0777; content-driven
debug read/write).

### 6.3 Userspace

Stock UGOS does nothing special beyond the udev symlink:

* Xorg runs the modesetting driver with `libinput_drv.so`
  (`/usr/lib/xorg/modules/input/`) → the touch device is used as-is.
* No calibration tool is shipped (`xinput_calibrator` absent) — the driver's
  ABS ranges are expected to line up with the framebuffer 1:1.
* Orientation: no coordinate transform matrix is applied anywhere; UI content is
  authored directly in the 258×960 portrait layout that matches the touch grid.

To reproduce on your distro:

```sh
# after installing axs_touch.ko for your kernel tree & depmod:
udevadm settle
ls -l /dev/input/eventTS                  # symlink present?
libinput list-devices | grep -A3 axs_ts   # or: evtest /dev/input/eventTS

# quick test without any display stack:
evtest /dev/input/eventTS                 # touch the strip, watch MT events
```

If your compositor treats the strip as rotated relative to what you render
(e.g. you chose kernel cmdline orientation fix from §1 instead of portrait fb),
apply the matching libinput calibration/quaternion or an X11
`TransformationMatrix` property — UGREEN never needs one.
