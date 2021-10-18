ALSA control elements
=====================

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
| CARD | Internal Frequency | RW | Enum | Internal sampling rate.           | 
| CARD | Current AutoSync Reference | RV | Enum | Current clock source.            | 
| CARD | External Frequency | RV | Enum | Current external sampling rate.            | 
| CARD | Clock Mode | RW | Enum | Master or Autosync.            | 
| CARD | Preferred AutoSync Reference | RW | Enum | Preferred clock source, if in AutoSync mode.            | 
| CARD | AutoSync Status | RV | Enum | AutoSync clock status: N/A, No Lock, Lock or Sync, for all sources.            | 
| CARD | AutoSync Frequency | RV | Enum | Current clock source sample rates, for all sources.            | 

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
| CARD | LTC In | RV | Int64 |             | 
| CARD | LTC In Drop Frame | RV | Bool |             | 
| CARD | LTC In Frame Rate | RV | Enum |             | 
| CARD | LTC In Pull Factor | RV | Int |             | 
| CARD | LTC In Valid | RV | Bool |             | 
| CARD | LTC Out | W | Int64 |             | 
| CARD | LTC Run | RW | Bool |             | 
| CARD | TCO Frame Rate | RW | Enum |             | 
| CARD | TCO Lock | RV | Bool |             | 
| CARD | TCO Pull | RW | Enum |             | 
| CARD | TCO Sample Rate | RW | Enum |             | 
| CARD | TCO Sync Source | RW | Enum |             | 
| CARD | TCO Video Format | RV | Enum |             | 
| CARD | TCO WCK Conversion | RW | Enum |             | 
| CARD | TCO Word Term | RW | Bool |             | 
| CARD | TCO WordClk Speed | RV | Enum |             | 
| CARD | TCO WordClk Valid | RV | Bool |             | 
