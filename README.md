# snd-hdspe
New linux kernel driver for RME HDSPe MADI / AES / RayDAT / AIO and AIO Pro sound cards and extension modules.
In addition to the functionality offered by the stock snd-hdspm linux driver, this driver provides support for the HDSPe AIO Pro card and TCO module LTC output. The control interface has been completely updated, enabling access to the advanced features of these professional audio cards though the generic ALSA control mechanisms instead of ad-hoc ioctl's. 
It comes with a friendly user space configuration tool: [hdspeconf](https://github.com/PhilippeBekaert/hdspeconf).

**Instructions**

The driver is provided here in source code, to be compiled out of kernel tree.

- Install the kernel headers and software development toolchain for your linux distribution.
On ubuntu:

       sudo apt-get install linux-headers-$(uname -r)
     
- Clone this repository. cd to your clone copy folder and type 

       make 
     
This builds the hdspe.ko kernel driver

- Installing the hdspe.ko driver: If installed, remove the snd-hdspm.ko driver (the old driver for RME HDSPe cards) first:

      sudo -s
      rmmod snd-hdspm
      insmod hdspe.ko
    
You need to stop all (audio) applications using the snd-hdspm driver before, in particular PulseAudio and the jack audio server.

You may need to disable secure boot in your systems BIOS before you will be able to load this non-signed kernel module.
    
- In case you would love or need to see the debug messages spit out by the hdspe.ko module, enable debug log output:

      sudo echo 8 > /proc/sys/kernel/printk
    
You may need to add the debug linux kernel boot flag and restart your computer.   
 
- removing the hdspe.ko driver and re-installing the default snd-hdspm driver:

      sudo -s 
      rmmod hdspe.ko
      modprobe snd-hdspm

- viewing ALSA controls:

      alsactl -f asound.state store
      less asound.state
    
- cleaning up your repository clone folder:

      make clean
    

**Note**

- The RME HDSPe MADI-FX is a different beast and is not (yet) supported by this driver. See 
[Adrian Knoths MADI-FX driver work in progress](https://github.com/adiknoth/madifx).

- Older RME HDSP cards are not supported by this driver either. Use the stock snd-hdsp driver for the RPM, Digiface, Multiface, etc...

**Status**

At this time (October, 15 2021), the driver is early work in progress.
- AIO Pro and TCO support is pretty complete and lab-tested.
- MADI/AES/RayDAT card support is to be completed and tested.
- It is developed on ubuntu studio 20.04 and has only been tested on that distribution so far.

**Acknowledgements**

- This work builds on previous work by the hdspm linux kernel driver authors over the last nearly 20 years:
Winfried Ritsch, Paul Davis, Marcus Andersson, Thomas Charbonnel, Remy Bruno, Florian Faber and Andrian Knoth.
Blame [me](mailto:linux@panokkel.be) for mistakes in the new driver.
- Thanks to [RME](http://www.rme-audio.com) for providing the necessary information and code for writing this driver.
- Thanks to [Amptec Belgium](http://www.amptec.be) for hardware support.

**License and (No) Warranty**

See [LICENSE](https://github.com/PhilippeBekaert/snd-hdspe/blob/main/LICENSE).

**Author**

[Philippe Bekaert](mailto:linux@panokkel.be), October 2021.
