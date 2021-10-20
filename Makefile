obj-m += sound/pci/hdsp/

KDIR    ?= /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)
EXTRA_CFLAGS += -DDEBUG -DCONFIG_SND_DEBUG

# Force to build the module as loadable kernel module.
# Keep in mind that this configuration sound be in 'sound/pci/Kconfig' when upstreaming.
export CONFIG_SND_HDSPE=m

default: depend
	$(MAKE) W=1 -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) W=1 -C $(KDIR) M=$(PWD) clean
	-rm *~
	-touch deps

insert: default
	-rmmod snd-hdspm
	insmod snd-hdspe.ko

remove:
	rmmod hdspe

list-controls:
	-rm asound.state
	alsactl -f asound.state store

show-controls: list-controls
	less asound.state

enable-debug-log:
	echo 8 > /proc/sys/kernel/printk

depend:
	gcc -MM sound/pci/hdsp/hdspe/hdspe*.c > deps
