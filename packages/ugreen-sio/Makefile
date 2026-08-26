# UGREEN iDX6011 Pro platform drivers: IT55xx EC Super-IO
# (backlight mipi_backlight / fans / watchdog / wake config), power button, beeper
obj-m += ug_idx6011pro-sio.o ug_gpio_btn.o ug_sataio_beep.o
ug_idx6011pro-sio-objs := ug_it55pro_functions.o it55_helper.o
ug_sataio_beep-objs    := ug_sataio_beeper.o it55_helper.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
