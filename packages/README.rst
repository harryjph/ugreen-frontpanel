UGREEN iDX6011 Pro front-panel drivers (DKMS)
=============================================

Three DKMS modules, one directory each under /usr/src after install:

* ``ugreen-leds-mcu`` : LED class devices
  ``/sys/class/leds/{power,network_stat,network_stat2,disk1..6}``;
  triggers ``normal | breath | timer2`` (+ any upstream triggers)
* ``ugreen-sio``      : backlight device ``/sys/class/backlight/mipi_backlight``
  (0-100), fans/watchdog/wake via ``/proc/nas/*``, GPIO power button,
  SATA fault beeper (``/proc/nas/beeper``)
* ``axs-touch``       : touch screen evdev device named ``axs_ts``,
  stable symlink ``/dev/input/eventTS``

Module load order comes from modules-load.d/ugreen-frontpanel.conf; the
touch driver autoloads through its ACPI modalias (acpi:CUST0000).

Set the LCD backlight default on boot:
    echo 80 > /sys/class/backlight/mipi_backlight/brightness

Upstream drivers (GPL-2.0-only):
https://github.com/ugreen-opensource/kernel-6.12
