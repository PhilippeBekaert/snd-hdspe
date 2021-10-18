#ALSA control elements#

#Elements common to all supported cards#

| Interface | Name | Access | Value Type | Description |
| :- | :- | :- | :- | :- |
| CARD | Card Revision | R | Int |             | 
| CARD | Firmware Build | R | Int |             | 
| CARD | Serial | R | Int |             | 
| CARD | TCO Present | R | Bool |             | 
| CARD | Capture PID | RV | Int |             | 
| CARD | Playback PID | RV | Int |             | 
| CARD | Running | RV | Bool |             | 
| CARD | Buffer Size | RV | Int |             | 
| CARD | Status Polling | RW | Int |             | 
| HWDEP | DDS | RW | Int |             | 
| HWDEP | Raw Sample Rate | RV | Int64 |             | 
| CARD | Internal Frequency | RW | Enum |             | 
| CARD | Current AutoSync Reference | RV | Enum |             | 
| CARD | External Frequency | RV | Enum |             | 
| CARD | Clock Mode | RW | Enum |             | 
| CARD | Preferred AutoSync Reference | RW | Enum |             | 
| CARD | AutoSync Status | RV | Enum |             | 
| CARD | AutoSync Frequency | RV | Enum |             | 

#AIO Pro elements#

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

#TCO elements#

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
