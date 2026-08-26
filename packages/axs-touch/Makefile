# AiXieSheng AXS15205 touch controller driver for the UGREEN iDX6011 Pro LCD
# autoloads via ACPI modalias acpi:CUST0000
ccflags-y += -Wall
KBUILD_CFLAGS += -Wno-error

obj-m += axs_touch.o
axs_touch-objs := src/axs_core.o src/axs_i2c.o src/axs_spi.o src/axs_debug.o \
                  src/axs_upgrade.o src/axs_download.o src/axs_gesture.o src/axs_esd.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
