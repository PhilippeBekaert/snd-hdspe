ALSA control elements
=====================

As usual

     alsactl -f asound.state store

stores the actual ALSA control elements with all metadata and exact enumeration values 
in the file name asound.state.

See [hdspeconf](https://github.com/PhilippeBekaert/hdspeconf) for an example application showing how to use these elements.

Elements common to all supported cards
--------------------------------------

| Interface | Name | Access | Value Type | Description |
| :- | :- | :- | :- | :- |
| CARD | Card Revision | R | Int | PCI class revision. Uniquely identifies card model.  | 
| CARD | Firmware Build | R | Int | Firmware version.           | 
| CARD | Serial | R | Int | Card serial number.            | 
| CARD | TCO Present | R | Bool | Whether or not TCO module is present.            | 
| CARD | Capture PID | RV | Int | Current capture process ID, or -1.            | 
| CARD | Playback PID | RV | Int | Current playback process ID, or -1.            | 
| CARD | Running | RV | Bool | Whether or not some process is capturing or playing back.            | 
| CARD | Buffer Size | RV | Int | Sample buffer size, in frames.            | 
| CARD | Status Polling | RW | Int | See below **Status Polling**            | 
| HWDEP | DDS | RW | Int | See below **DDS**            | 
| HWDEP | Raw Sample Rate | RV | Int64 | See below **DDS**            | 
| CARD | Internal Frequency | RW | Enum | Internal sampling rate class: 32 KHz, 44.1 KHz, 48 KHz etc....           | 
| CARD | Current AutoSync Reference | RV | Enum | Current clock source.            | 
| CARD | External Frequency | RV | Enum | Current external sampling rate.            | 
| CARD | Clock Mode | RW | Enum | Master or AutoSync.            | 
| CARD | Preferred AutoSync Reference | RW | Enum | Preferred clock source, if in AutoSync mode.            | 
| CARD | AutoSync Status | RV | Enum | AutoSync clock status: N/A, No Lock, Lock or Sync, for all sources.            | 
| CARD | AutoSync Frequency | RV | Enum | Current clock source sample rate class, for all sources.            | 

**Status Polling**

Use this control element to enable or disable kernel space status polling. The value of this element is
the frequency at which to perform status polling in the driver, or 0 to disable the feature. 
If non-zero, the driver will poll for card changes in the value of volatile control elements
at approximately the indicated frequency.
A notification event is generated on any status ALSA control elements that has changed. If any
have changed, the value of the Status Polling control element is reset to 0, notifying client 
applications, and effectively disabling
status polling until a client application enables it again by setting a non-zero value. Status
polling is also automatically disabled after a few seconds. When automatically disabled, a notification
is sent as well, so that client applications can re-enable it. Whenever an application receives 
a "Status Polling" event notification, it shall set the value of this control element to the maximum
of its reported value and the applications desired value to avoid ping-ponging changes with other
applications.

**DDS**

The HDSPe cards report effective sampling frequency as a ratio of a fixed frequency constant 
typical for the card, and the content of a register. This ratio is returned
in the "Raw Sample Rate" control element. The numerator is the first value, and is a true 64-bit value. The denominator is a 32-bit value, and provided as the second value.

The "DDS" control element enables setting the DDS register, determining internal sample rate to sub-Hz accuracy. The DDS register value is the denominator of the desired sample rate, given as a ratio
with same numerator as the "Raw Sample Rate" control element, i.o.w. the numerator is the first value
of the "Raw Sample Rate" control element.
This can be used to synchronise the cards internal clock to e.g. a system clock.

AIO Pro elements
----------------

| Interface | Name | Access | Value Type | Description |
| :- | :- | :- | :- | :- |
| CARD | Input Level | RW | Enum |             | 
| CARD | Output Level | RW | Enum |             | 
| CARD | Phones Level | RW | Enum |             | 
| CARD | S/PDIF In | RW | Enum |             | 
| CARD | S/PDIF Out Optical | RW | Bool |             | 
| CARD | S/PDIF Out Professional | RW | Bool |             | 
| CARD | ADAT Internal | RW | Bool |             | 
| CARD | Single Speed WordClk Out | RW | Bool |             | 
| CARD | Clear TMS | RW | Bool |             | 

TCO elements
------------

| Interface | Name | Access | Value Type | Description |
| :- | :- | :- | :- | :- |
| CARD | LTC In | RV | Int64 | Incoming LTC code - see below **LTC controls** | 
| CARD | LTC In Drop Frame | RV | Bool | Whether incoming LTC is drop frame format or not | 
| CARD | LTC In Frame Rate | RV | Enum | Incoming LTC frame rate: 24, 25 or 30 fps | 
| CARD | LTC In Pull Factor | RV | Int | Incoming LTC actual frame rate deviation from standard | 
| CARD | LTC In Valid | RV | Bool | Whether or not valid LTC input is detected | 
| CARD | LTC Out | W | Int64 | LTC output control - see below **LTC controls** |
| CARD | LTC Time | RV | Int64 | Current periods end LTC time - see below **LTC controls** | 
| CARD | LTC Run | RW | Bool | Pauze / restart LTC output | 
| CARD | LTC Frame Rate | RW | Enum | TCO LTC engine frame rate: 24, 25, 29.97, 29.97 DF or 30 fps | 
| CARD | LTC Sample Rate | RW | Enum | TCO LTC engine audio sample rate: 44.1 KHz, 48 KHz, **From App** | 
| CARD | TCO Lock | RV | Bool | Whether or not the TCO is locked to LTC, Video or Word Clock | 
| CARD | TCO Pull | RW | Enum | Pull Up / Pull Down factor |
| CARD | TCO Sync Source | RW | Enum | TCO preferred synchronisation source: LTC, Video or Word Clock | 
| CARD | TCO Video Format | RV | Enum | Video format reference signal detected | 
| CARD | TCO WCK Conversion | RW | Enum | World clock conversion 1:1, 44.1 -> 48 KHz, 48 -> 44.1 KHz | 
| CARD | TCO Word Term | RW | Bool | Whether or not to terminate the word clock/video input BNC | 
| CARD | TCO WordClk Valid | RV | Bool | Whether or not a valid word clock signal is detected | 
| CARD | TCO WordClk Speed | RV | Enum | Detected input word clock speed | 

**LTC Control**

If a valid LTC signal is presented to the TCO module, the 'LTC In' control
will report two 64-bit values: the current LTC 64-bit code and the LTC time
at which that current code started.

LTC output is started by writing the 'LTC Out' control. 
The 'LTC Out' control contains two 64-bit values similar to the 'LTC In' control
element: the LTC 64-bit code
to start the LTC output with, and the LTC time at which that code shall be
started. If the indicated time is in the past or further in the future
than a few LTC frame durations, the start time and time code are adapted
accordingly to create output that would result if the indicated time code
were started at the indicated time (if in the past) or that will result in
the indicated time code being generated at the indicated time (if in the
future).

The SMPTE 12-1 standard defines LTC as a 80-bit code, with 64 data bits and
16 synchronisation bits. The 64-bit LTC code mentioned above corresponds to
the 64 data bits of an SMPTE 12-1 80-bit code. The data bits are described
in the [SMPTE 12-1 standard](https://ieeexplore.ieee.org/document/7291029/definitions?anchor=definitions) and on [wikipedia](https://en.wikipedia.org/wiki/Linear_timecode).

The LTC time is the number of audio frames processed since the snd-hdspe driver
was started. The 'LTC Time' control allows to read the LTC time corresponding to
the end of the current period. In a jack audio connection kit application
process callback, an application would query the current period jack audio frame
counter using the jack_get_cycle_times() call and add the period size, on
the one hand side. The application would read the LTC time from the 'LTC Time'
control on the other hand size. The thus obtained counters represent the
same moment in time, and allow to convert the time reported in a 'LTC In'
control to a jack audio frame count, or to calculate the
LTC time at which LTC output shall be started from a jack audio frame count.

*Note* ALSA probably provides mechanisms to query clocks, such as the here
described LTC time, more efficiently than through a control element. In
future, the 'LTC Time' control element may be replaced by such more efficient
mechanism.

Examples:
- positional time code is generated by starting code 00:00:00:00 at time 0.
The output time code will reflect the time since the start of the driver.
- jam sync: set 'LTC Out' time code and time to the current 'LTC In' time code
and time.

Outputting wall clock LTC: set 'LTC Out' time code to the special value -1,
and the time to the number of seconds east of GMT corresponding to the
time zone, and corrected for daylight saving time if in effect.
The driver will replace a hexadecimal time code of 3f:7f:7f:3f by the exact
value of the real-time clock at the time the request gets processed, and add
the number of seconds indicated in the time field of the control. Such
hexadecimal time code results from setting the first value of the 'LTC Out'
control to -1.

**From App**

The 'From App' LTC sample rate setting will set the TCO LTC engine sample rate
to match the audio card sample rate class: 44.1 KHz if the sound card is
running at 44.1 KHz, and 48 KHz otherwise (the TCO does not support 32 KHz
sample rate).
