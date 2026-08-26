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
