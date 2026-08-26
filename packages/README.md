# DKMS driver packages (UGREEN iDX6011 Pro front panel)

Three self-contained DKMS source trees, built from the official upstream at
https://github.com/ugreen-opensource/kernel-6.12 (commit c75c5abd6):

| dir | dkms name | produces |
|---|---|---|
| `ugreen-leds-mcu/` | `ugreen-leds-mcu` | `leds-mcu.ko`, `ledtrig-normal-ht32f52231.ko`, `ledtrig-timer2-ht32f52231.ko`, `ledtrig-breath-ht32f52231.ko` |
| `ugreen-sio/` | `ugreen-sio` | `ug_idx6011pro-sio.ko` (mipi_backlight/fans/watchdog/wake), `ug_gpio_btn.ko`, `ug_sataio_beep.ko` |
| `axs-touch/` | `axs-touch` | `axs_touch.ko` (AXS15205 touch; autoloads via ACPI CUST0000) |

Each directory contains its own `dkms.conf`; install the RPM (built from
../rpm) or register manually:

    sudo dkms add -m <name> -v 1.0.git20260403
    sudo dkms build -m <name> -v 1.0.git20260403 -k $(uname -r)
    sudo dkms install -m <name> -v 1.0.git20260403 -k $(uname -r)

Local test-build without dkms:

    make all KDIR=/lib/modules/$(uname -r)/build   # inside a package dir

Verified compiling against AlmaLinux 10.2 kernel 6.12.0-211.
Note for future maintainers: do not pass KBUILD_CFLAGS+= overrides on these
Makefiles' command lines — with newer gcc that changes constraint handling and
breaks the build ("impossible constraint in asm"); plain kernel defaults work.
