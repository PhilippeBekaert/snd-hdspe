# snd-hdspe
New linux kernel ALSA driver for [RME](http://www.rme-audio.com) HDSPe MADI / AES / RayDAT / AIO and AIO Pro sound cards and extension modules.
In addition to the functionality offered by the stock snd-hdspm linux driver, this driver provides support for the HDSPe AIO Pro card and TCO module LTC output and fixes issues such as double/quad speed support of AIO cards. Programmers will appreciate the completely updated control interface, enabling access to the advanced features of these professional audio cards though generic ALSA control mechanisms instead of ad-hoc ioctl's. Users will appreciate [hdspeconf](https://github.com/PhilippeBekaert/hdspeconf), the friendly user space HDSPe sound card configuration tool that builds upon this driver. The driver is compatible with the stock linux ALSA hdspmixer application for controlling the hardware mixer.


**Supported hardware**

- The snd-hdspe driver focusses on the current (2021) range of RME HDSPe PCIe cards, except MADI-FX: MADI, AES, RayDAT, AIO Pro. AIO is supported as well.

- The RME HDSPe MADI-FX is a different beast and is not supported by this driver. See 
[Adrian Knoths MADI-FX driver work in progress](https://github.com/adiknoth/madifx).


**Building the driver**

The driver is provided here in source code, to be compiled out of kernel tree.

- Install the kernel headers and software development toolchain for your linux distribution.
On ubuntu:

       sudo apt-get install linux-headers-$(uname -r)
     
- Clone this repository. cd to your clone copy folder and type 

       make 
     
This builds the snd-hdspe.ko kernel driver in the sound/pci/hdsp/hdspe subdirectory.

- You may need to blacklist the stock snd-hdspm driver as this driver targets the same devices. Create a file /usr/lib/modprobe.d/hdspe.conf with the following content:

        blacklist snd-hdspm


**Trying out the driver**

- Manually installing the snd-hdspe.ko driver: If installed, remove the snd-hdspm.ko driver (the old driver for RME HDSPe cards) first:

      sudo -s
      rmmod snd-hdspm
      insmod sound/pci/hdsp/hdspe/snd-hdspe.ko

or

      sudo -s
      make insert

You need to stop all (audio) applications using the snd-hdspm driver before, in particular PulseAudio and the jack audio server.

When manually inserting a non-signed kernel module like this, you may need to disable secure boot in your systems BIOS.

See below for how to install the kernel module using DKMS. Installing with DKMS
is preferred if you plan to use this module on a regular basis instead of just
testing it once.
    
- In case you would love or need to see the debug messages spit out by the snd-hdspe.ko module, enable debug log output:

      sudo echo 8 > /proc/sys/kernel/printk

or

      sudo -s
      make enable-debug-log
    
- Removing the snd-hdspe.ko driver and re-installing the default snd-hdspm driver:

      sudo -s 
      rmmod snd-hdspe.ko
      modprobe snd-hdspm

or

      sude -s
      make remove

- Viewing ALSA controls:

      alsactl -f asound.state store
      less asound.state

or

      make show-controls
    
- Cleaning up your repository clone folder:

      make clean


**Installing the driver with DKMS build**

[Dynamic Kernel Module System (DKMS)](https://github.com/dell/dkms) makes it easy to maintain
out-of-tree kernel modules. It's preferable to use DKMS instead of manually installation since it
assists module signing for secure boot.

- Install DKMS package. On Ubuntu:

        sudo apt install dkms

- For preparation, execute cd to your clone copy folder, then type

        sudo ln -s $(pwd) /usr/src/alsa-hdspe-0.0

- For installing, type

        sudo dkms install alsa-hdspe/0.0

- For uninstalling, type

        sudo dkms remove alsa-hdspe/0.0
      

**Documentation**

- [ALSA control elements provided by this driver](doc/controls.md)


**Status**

At this time (November, 25 2021), the driver is work in progress.
- AES, AIO, AIO Pro, RayDAT and TCO control and PCM capture and playback
support done.
- MADI card support is to be completed and tested.
- It is developed on ubuntu studio 20.04 and has only been tested on that distribution so far.
- Other TODOs: more MIDI testing, mixer interface, ...

As the code is still work in progress, the control interface may change at
any time and features may be added or updated. Inform the author if you
are relying on the control interface or features as they are now.


**Acknowledgements**

- This work builds on previous work by the hdspm linux kernel driver authors over the last nearly 20 years:
Winfried Ritsch, Paul Davis, Marcus Andersson, Thomas Charbonnel, Remy Bruno, Florian Faber and Andrian Knoth.
(Blame [the author](mailto:linux@panokkel.be) for bugs in the new driver.)
- Thanks to [RME](http://www.rme-audio.com) for providing the necessary information and code for writing this driver.
- Thanks to [Amptec Belgium](http://www.amptec.be) for hardware support.


**License and (No) Warranty**

See [LICENSE](https://github.com/PhilippeBekaert/snd-hdspe/blob/main/LICENSE).


**Author**

[Philippe Bekaert](mailto:linux@panokkel.be), November 2021.
