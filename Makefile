obj-m += hdspe.o hdspm.o
hdspe-objs := hdspe_core.o hdspe_pcm.o hdspe_midi.o hdspe_hwdep.o \
	hdspe_proc.o hdspe_control.o hdspe_mixer.o hdspe_tco.o \
	hdspe_common.o hdspe_madi.o hdspe_aes.o hdspe_raio.o \
	hdspe_ltc_math.o

KDIR    ?= /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)
EXTRA_CFLAGS += -DDEBUG -DCONFIG_SND_DEBUG

default: depend
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	-rm *~
	-touch deps

insert: default
	-rmmod snd-hdspm
	insmod hdspe.ko

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
	gcc -MM hdspe*.c > deps

