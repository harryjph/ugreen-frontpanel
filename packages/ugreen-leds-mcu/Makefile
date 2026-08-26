# UGREEN front-panel LED MCU driver + triggers (iDX6011/iDX6012 family)
# Sources: https://github.com/ugreen-opensource/kernel-6.12 (GPL-2.0-only)
obj-m += leds-mcu.o \
         ledtrig-normal-ht32f52231.o \
         ledtrig-timer2-ht32f52231.o \
         ledtrig-breath-ht32f52231.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
