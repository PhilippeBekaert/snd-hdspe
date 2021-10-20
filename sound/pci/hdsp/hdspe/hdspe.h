// SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note
/**
 * @file hdspe.h
 * @brief RME HDSPe driver user space API.
 *
 * 20210728 ... 0813 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work by Winfried Ritsch (IEM, 2003) and
 * Thomas Charbonnel (thomas@undata.org),
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

#ifndef __SOUND_HDSPE_H
#define __SOUND_HDSPE_H

#include <linux/types.h>
#include <linux/ioctl.h>

/* User space API version.
 * Structs returned by the HDSPe driver ioctls contain the API version with which the
 * kernel driver has been compiled. API users should check that version against
 * HDSPE_VERSION and take appropriate action in case versions differ. */
#define HDSPE_VERSION            2

/* Maximum hardware input, software playback and hardware output
 * channels is 64 even on 56Mode you have 64playbacks to matrix. */
#define HDSPE_MAX_CHANNELS      64

/* Card model */
enum hdspe_io_type {
	HDSPE_MADI            = 0,
	HDSPE_MADIFACE        = 1,
	HDSPE_AIO             = 2,
	HDSPE_AES             = 3,
	HDSPE_RAYDAT          = 4,
	HDSPE_AIO_PRO         = 5,
	HDSPE_IO_TYPE_COUNT   = 6,
	HDSPE_IO_TYPE_INVALID = 7,
	HDSPE_IO_TYPE_FORCE_32BIT = 0xffffffff
};

#define HDSPE_IO_TYPE_NAME(i)			\
	(i == HDSPE_MADI         ? "MADI" :	\
	 i == HDSPE_MADIFACE     ? "MADIface" :	\
	 i == HDSPE_AIO          ? "AIO" :	\
	 i == HDSPE_AES          ? "AES" :	\
	 i == HDSPE_RAYDAT       ? "RayDAT" :	\
	 i == HDSPE_AIO_PRO      ? "AIO Pro" :	\
	 "???")

/* Clock mode */
enum hdspe_clock_mode {
        HDSPE_CLOCK_MODE_AUTOSYNC      = 0,
	HDSPE_CLOCK_MODE_MASTER        = 1,
	HDSPE_CLOCK_MODE_COUNT         = 2,
	HDSPE_CLOCK_MODE_INVALID       = 3,
	HDSPE_CLOCK_MODE_FORCE_32BIT   = 0xffffffff	
};

#define HDSPE_CLOCK_MODE_NAME(i)			 \
	(i == HDSPE_CLOCK_MODE_AUTOSYNC ? "AutoSync" :   \
	 i == HDSPE_CLOCK_MODE_MASTER   ? "Master" :     \
	 "???")

/* Speed mode */
enum hdspe_speed {
	HDSPE_SPEED_SINGLE  = 0,
	HDSPE_SPEED_DOUBLE  = 1,
	HDSPE_SPEED_QUAD    = 2,
	HDSPE_SPEED_COUNT   = 3,
	HDSPE_SPEED_INVALID = 4,
	HDSPE_SPEED_FORCE_32BIT  = 0xffffffff
};

#define HDSPE_SPEED_NAME(i)			\
	(i == HDSPE_SPEED_SINGLE ? "Single Speed" :	\
	 i == HDSPE_SPEED_DOUBLE ? "Double Speed" :	\
	 i == HDSPE_SPEED_QUAD   ? "Quad Speed"   : 	\
	"???")

/* frequency class */
enum hdspe_freq {
	HDSPE_FREQ_NO_LOCK   = 0,
	HDSPE_FREQ_32KHZ     = 1,
	HDSPE_FREQ_44_1KHZ   = 2,
	HDSPE_FREQ_48KHZ     = 3,
	HDSPE_FREQ_64KHZ     = 4,
	HDSPE_FREQ_88_2KHZ   = 5,
	HDSPE_FREQ_96KHZ     = 6,
	HDSPE_FREQ_128KHZ    = 7,
	HDSPE_FREQ_176_4KHZ  = 8,
	HDSPE_FREQ_192KHZ    = 9,
	HDSPE_FREQ_COUNT     = 10,
	HDSPE_FREQ_INVALID   = 11,
	HDSPE_FREQ_FORCE_32BIT= 0xffffffff	
};

#define HDSPE_FREQ_NAME(i)			   \
	(i == HDSPE_FREQ_NO_LOCK   ? ""          : \
	 i == HDSPE_FREQ_32KHZ     ? "32 KHz"    : \
	 i == HDSPE_FREQ_44_1KHZ   ? "44.1 KHz"  : \
	 i == HDSPE_FREQ_48KHZ     ? "48 KHz"    : \
	 i == HDSPE_FREQ_64KHZ     ? "64 KHz"    : \
	 i == HDSPE_FREQ_88_2KHZ   ? "88.2 KHz"  : \
	 i == HDSPE_FREQ_96KHZ     ? "96 KHz"    : \
	 i == HDSPE_FREQ_128KHZ    ? "128 KHz"   : \
	 i == HDSPE_FREQ_176_4KHZ  ? "176.4 KHz" : \
	 i == HDSPE_FREQ_192KHZ    ? "192 KHz"   : \
	 "???")

#define HDSPE_FREQ_SAMPLE_RATE(i) \
	(i == HDSPE_FREQ_NO_LOCK   ?      0 : \
	 i == HDSPE_FREQ_32KHZ     ?  32000 : \
	 i == HDSPE_FREQ_44_1KHZ   ?  44100 : \
	 i == HDSPE_FREQ_48KHZ     ?  48000 : \
	 i == HDSPE_FREQ_64KHZ     ?  64000 : \
	 i == HDSPE_FREQ_88_2KHZ   ?  88200 : \
	 i == HDSPE_FREQ_96KHZ     ?  96000 : \
	 i == HDSPE_FREQ_128KHZ    ? 128000 : \
	 i == HDSPE_FREQ_176_4KHZ  ? 176400 : \
	 i == HDSPE_FREQ_192KHZ    ? 192000 : \
	 0)

/* Clock source aka AutoSync references */
enum hdspe_clock_source {
	HDSPE_CLOCK_SOURCE_WORD    =  0,   // Word clock
	HDSPE_CLOCK_SOURCE_1       =  1,   // Digital audio input 1
	HDSPE_CLOCK_SOURCE_2       =  2,   // Digital audio input 2
	HDSPE_CLOCK_SOURCE_3       =  3,   // Digital audio input 3
	HDSPE_CLOCK_SOURCE_4       =  4,   // Digital audio input 4
	HDSPE_CLOCK_SOURCE_5       =  5,   // Digital audio input 5
	HDSPE_CLOCK_SOURCE_6       =  6,   // Digital audio input 6
	HDSPE_CLOCK_SOURCE_7       =  7,   // Digital audio input 7
	HDSPE_CLOCK_SOURCE_8       =  8,   // Digital audio input 8
	HDSPE_CLOCK_SOURCE_TCO     =  9,   // Time Code Option
	HDSPE_CLOCK_SOURCE_SYNC_IN = 10,   // Internal Sync input
	HDSPE_CLOCK_SOURCE_11      = 11,   // Unused
	HDSPE_CLOCK_SOURCE_12      = 12,   // Unused
	HDSPE_CLOCK_SOURCE_13      = 13,   // Unused
	HDSPE_CLOCK_SOURCE_14      = 14,   // Unused
	HDSPE_CLOCK_SOURCE_INTERN  = 15,   // Internal clock ("master mode")
	HDSPE_CLOCK_SOURCE_COUNT   = 16,
	HDSPE_CLOCK_SOURCE_INVALID = 17,
	HDSPE_CLOCK_SOURCE_FORCE_32BIT = 0xffffffff
};

/* Synonyms for HDSP_CLOCK_SOURCE_1..8, for each card model: */
#define HDSPE_CLOCK_SOURCE_MADI      HDSPE_CLOCK_SOURCE_1    /* MADI */

#define HDSPE_CLOCK_SOURCE_AES1      HDSPE_CLOCK_SOURCE_1    /* AES */
#define HDSPE_CLOCK_SOURCE_AES2      HDSPE_CLOCK_SOURCE_2
#define HDSPE_CLOCK_SOURCE_AES3      HDSPE_CLOCK_SOURCE_3
#define HDSPE_CLOCK_SOURCE_AES4      HDSPE_CLOCK_SOURCE_4
#define HDSPE_CLOCK_SOURCE_AES5      HDSPE_CLOCK_SOURCE_5
#define HDSPE_CLOCK_SOURCE_AES6      HDSPE_CLOCK_SOURCE_6
#define HDSPE_CLOCK_SOURCE_AES7      HDSPE_CLOCK_SOURCE_7
#define HDSPE_CLOCK_SOURCE_AES8      HDSPE_CLOCK_SOURCE_8

#define HDSPE_CLOCK_SOURCE_AES       HDSPE_CLOCK_SOURCE_1    /* RayDAT/AIO/AIO Pro */
#define HDSPE_CLOCK_SOURCE_SPDIF     HDSPE_CLOCK_SOURCE_2
#define HDSPE_CLOCK_SOURCE_ADAT      HDSPE_CLOCK_SOURCE_3    /* AIO/AIO Pro */
#define HDSPE_CLOCK_SOURCE_ADAT1     HDSPE_CLOCK_SOURCE_3    /* RayDAT */
#define HDSPE_CLOCK_SOURCE_ADAT2     HDSPE_CLOCK_SOURCE_4
#define HDSPE_CLOCK_SOURCE_ADAT3     HDSPE_CLOCK_SOURCE_5
#define HDSPE_CLOCK_SOURCE_ADAT4     HDSPE_CLOCK_SOURCE_6

#define HDSPE_MADI_CLOCK_SOURCE_NAME(i)	      \
	(i == HDSPE_CLOCK_SOURCE_WORD    ? "WordClk": \
	 i == HDSPE_CLOCK_SOURCE_MADI    ? "MADI"   : \
	 i == HDSPE_CLOCK_SOURCE_TCO     ? "TCO"    : \
	 i == HDSPE_CLOCK_SOURCE_SYNC_IN ? "SyncIn" : \
	 i == HDSPE_CLOCK_SOURCE_INTERN  ? "Intern" : \
	 "???")

#define HDSPE_AES_CLOCK_SOURCE_NAME(i)	      \
	(i == HDSPE_CLOCK_SOURCE_WORD    ? "WordClk": \
	 i == HDSPE_CLOCK_SOURCE_AES1    ? "AES1"   : \
	 i == HDSPE_CLOCK_SOURCE_AES2    ? "AES2"   : \
	 i == HDSPE_CLOCK_SOURCE_AES3    ? "AES3"   : \
	 i == HDSPE_CLOCK_SOURCE_AES4    ? "AES4"   : \
	 i == HDSPE_CLOCK_SOURCE_AES5    ? "AES5"   : \
	 i == HDSPE_CLOCK_SOURCE_AES6    ? "AES6"   : \
	 i == HDSPE_CLOCK_SOURCE_AES7    ? "AES7"   : \
	 i == HDSPE_CLOCK_SOURCE_AES8    ? "AES8"   : \
	 i == HDSPE_CLOCK_SOURCE_TCO     ? "TCO"    : \
	 i == HDSPE_CLOCK_SOURCE_SYNC_IN ? "SyncIn" : \
	 i == HDSPE_CLOCK_SOURCE_INTERN  ? "Intern" : \
	 "???")

#define HDSPE_RAYDAT_CLOCK_SOURCE_NAME(i)	      \
	(i == HDSPE_CLOCK_SOURCE_WORD    ? "WordClk": \
	 i == HDSPE_CLOCK_SOURCE_AES     ? "AES"    : \
	 i == HDSPE_CLOCK_SOURCE_SPDIF   ? "S/PDIF" : \
	 i == HDSPE_CLOCK_SOURCE_ADAT1   ? "ADAT1"  : \
	 i == HDSPE_CLOCK_SOURCE_ADAT2   ? "ADAT2"  : \
	 i == HDSPE_CLOCK_SOURCE_ADAT3   ? "ADAT3"  : \
	 i == HDSPE_CLOCK_SOURCE_ADAT4   ? "ADAT4"  : \
	 i == HDSPE_CLOCK_SOURCE_TCO     ? "TCO"    : \
	 i == HDSPE_CLOCK_SOURCE_SYNC_IN ? "SyncIn" : \
	 i == HDSPE_CLOCK_SOURCE_INTERN  ? "Intern" : \
	 "???")

/* AIO & AIO Pro */
#define HDSPE_AIO_CLOCK_SOURCE_NAME(i)	      \
	(i == HDSPE_CLOCK_SOURCE_WORD    ? "WordClk": \
	 i == HDSPE_CLOCK_SOURCE_AES     ? "AES"    : \
	 i == HDSPE_CLOCK_SOURCE_SPDIF   ? "S/PDIF" : \
	 i == HDSPE_CLOCK_SOURCE_ADAT    ? "ADAT"   : \
	 i == HDSPE_CLOCK_SOURCE_TCO     ? "TCO"    : \
	 i == HDSPE_CLOCK_SOURCE_SYNC_IN ? "SyncIn" : \
	 i == HDSPE_CLOCK_SOURCE_INTERN  ? "Intern" : \
	 "???")

/* (Use this for initializations, or with compile time constants only) */
#define HDSPE_CLOCK_SOURCE_NAME(io_type, i) \
	(io_type == HDSPE_MADI     ? HDSPE_MADI_CLOCK_SOURCE_NAME(i)     : \
	 io_type == HDSPE_MADIFACE ? HDSPE_MADI_CLOCK_SOURCE_NAME(i)     : \
  	 io_type == HDSPE_AES      ? HDSPE_AES_CLOCK_SOURCE_NAME(i)      : \
  	 io_type == HDSPE_RAYDAT   ? HDSPE_RAYDAT_CLOCK_SOURCE_NAME(i)   : \
  	 io_type == HDSPE_AIO      ? HDSPE_AIO_CLOCK_SOURCE_NAME(i)      : \
  	 io_type == HDSPE_AIO_PRO  ? HDSPE_AIO_CLOCK_SOURCE_NAME(i)      : \
	 "???")

/* SyncCheck Status: with each AutoSync reference, a lock and sync bit
 * are associated. They are converted to a single sync status value: */
enum hdspe_sync_status {	
	HDSPE_SYNC_STATUS_NO_LOCK       = 0,
	HDSPE_SYNC_STATUS_LOCK          = 1,
	HDSPE_SYNC_STATUS_SYNC	        = 2,
	HDSPE_SYNC_STATUS_NOT_AVAILABLE = 3,
	HDSPE_SYNC_STATUS_COUNT         = 4,
	HDSPE_SYNC_STATUS_INVALID       = 5,
	HDSPE_SYNC_STATUS_FORCE_32BIT   = 0xffffffff
};

#define HDSPE_SYNC_STATUS_NAME(i)				\
	(i == HDSPE_SYNC_STATUS_NO_LOCK       ? "No Lock" :	\
	 i == HDSPE_SYNC_STATUS_LOCK          ? "Lock" :	\
	 i == HDSPE_SYNC_STATUS_SYNC          ? "Sync" :	\
	 i == HDSPE_SYNC_STATUS_NOT_AVAILABLE ? "N/A" :		\
	 "???" )

/* Boolean setting / status */
enum hdspe_bool {
	HDSPE_BOOL_OFF     = 0,
	HDSPE_BOOL_ON      = 1,
	HDSPE_BOOL_COUNT   = 2,
	HDSPE_BOOL_INVALID = 3,
	HDSPE_BOOL_FORCE_32BIT    = 0xffffffff	
};

#define HDSPE_BOOL_NAME(i)	       \
	(i == HDSPE_BOOL_OFF ? "Off" : \
	 i == HDSPE_BOOL_ON  ? "On"  : \
	 "???")

/* MADI input source */
enum hdspe_madi_input {
	HDSPE_MADI_INPUT_OPTICAL   = 0,
	HDSPE_MADI_INPUT_COAXIAL   = 1,
	HDSPE_MADI_INPUT_COUNT     = 2,
	HDSPE_MADI_INPUT_INVALID   = 3,
	HDSPE_MADI_INPUT_FORCE_32_BIT = 0xffffffff
};

#define HDSPE_MADI_INPUT_NAME(i) \
	(i == HDSPE_MADI_INPUT_OPTICAL ? "Optical" : \
	 i == HDSPE_MADI_INPUT_COAXIAL ? "Coaxial" : \
	 "???")

/* Double speed mode. Double wire means that two consecutive
 * channels in single speed mode are used for transmitting
 * a double speed audio signal (same as S/MUX for MADI). */
enum hdspe_ds_mode {
	HDSPE_DS_MODE_SINGLE_WIRE   = 0,
	HDSPE_DS_MODE_DOUBLE_WIRE   = 1,
	HDSPE_DS_MODE_COUNT         = 2,
	HDSPE_DS_MODE_INVALID       = 3,
	HDSPE_DS_MODE_FORCE_32_BIT  = 0xffffffff
};

#define HDSPE_DS_MODE_NAME(i) \
	(i == HDSPE_DS_MODE_SINGLE_WIRE  ? "Single Wire" :	\
	 i == HDSPE_DS_MODE_DOUBLE_WIRE  ? "Double Wire" : \
	 "???")

/* Quad speed mode. Double wire means that two consecutive channels
 * at double speed mode are used to transfor a quad speed audio
 * signal. Quad wire means that four channels in single speed mode 
 * are used to transfer a quad speed audio signal (same as SMUX/4 for MADI). */
enum hdspe_qs_mode {
	HDSPE_QS_MODE_SINGLE_WIRE   = 0,
	HDSPE_QS_MODE_DOUBLE_WIRE   = 1,
	HDSPE_QS_MODE_QUAD_WIRE     = 2,
	HDSPE_QS_MODE_COUNT         = 3,
	HDSPE_QS_MODE_INVALID       = 4,
	HDSPE_QS_MODE_FORCE_32_BIT  = 0xffffffff
};

#define HDSPE_QS_MODE_NAME(i) \
	(i == HDSPE_QS_MODE_SINGLE_WIRE  ? "Single Wire" : \
	 i == HDSPE_QS_MODE_DOUBLE_WIRE  ? "Double Wire" : \
	 i == HDSPE_QS_MODE_QUAD_WIRE    ? "Quad Wire"   : \
	 "???")

/* RayDAT / AIO / AIO Pro S/PDIF input source */
enum hdspe_raio_spdif_input {
        HDSPE_RAIO_SPDIF_INPUT_OPTICAL  = 0,
	HDSPE_RAIO_SPDIF_INPUT_COAXIAL  = 1,
	HDSPE_RAIO_SPDIF_INPUT_INTERNAL = 2,
	HDSPE_RAIO_SPDIF_INPUT_COUNT    = 3,
	HDSPE_RAIO_SPDIF_INPUT_INVALID  = 4,
	HDSPE_RAIO_SPDIF_INPUT_FORCE_32BIT    = 0xffffffff
};

#define HDSPE_RAIO_SPDIF_INPUT_NAME(i)			    \
	(i == HDSPE_RAIO_SPDIF_INPUT_OPTICAL  ? "Optical" : \
	 i == HDSPE_RAIO_SPDIF_INPUT_COAXIAL  ? "Coaxial" : \
	 i == HDSPE_RAIO_SPDIF_INPUT_INTERNAL ? "Internal" : \
	 "???")

/* AIO Input / Output / Phones levels */
enum hdspe_aio_level {
	HDSPE_AIO_LEVEL_HI_GAIN        = 0,
	HDSPE_AIO_LEVEL_PLUS_4_DBU     = 1,
	HDSPE_AIO_LEVEL_MINUS_10_DBV   = 2,
	HDSPE_AIO_LEVEL_COUNT          = 3,
	HDSPE_AIO_LEVEL_INVALID        = 4,
	HDSPE_AIO_LEVEL_FORCE_32BIT    = 0xffffffff
};

#define HDSPE_AIO_LEVEL_NAME(i) \
	(i == HDSPE_AIO_LEVEL_HI_GAIN      ? "Hi Gain" : \
	 i == HDSPE_AIO_LEVEL_PLUS_4_DBU   ? "+4 dBu"  : \
	 i == HDSPE_AIO_LEVEL_MINUS_10_DBV ? "-10 dBV" : \
	 "???")

/* AIO Pro Input levels. Define sensitivity of the input circuitery:
 * the voltage that causes 0 dBFS digital input. */
enum hdspe_aio_pro_input_level {
	HDSPE_AIO_PRO_INPUT_LEVEL_PLUS_4_DBU  = 0,
	HDSPE_AIO_PRO_INPUT_LEVEL_PLUS_13_DBU = 1,
	HDSPE_AIO_PRO_INPUT_LEVEL_PLUS_19_DBU = 2,
	HDSPE_AIO_PRO_INPUT_LEVEL_PLUS_24_DBU = 3,
	HDSPE_AIO_PRO_INPUT_LEVEL_COUNT       = 4,
	HDSPE_AIO_PRO_INPUT_LEVEL_INVALID     = 5,
	HDSPE_AIO_PRO_INPUT_LEVEL_FORCE_32BIT = 0xffffffff	
};

#define HDSPE_AIO_PRO_INPUT_LEVEL_NAME(i) \
	(i == HDSPE_AIO_PRO_INPUT_LEVEL_PLUS_4_DBU  ? "+4 dBu"  : \
	 i == HDSPE_AIO_PRO_INPUT_LEVEL_PLUS_13_DBU ? "+13 dBu" : \
	 i == HDSPE_AIO_PRO_INPUT_LEVEL_PLUS_19_DBU ? "+19 dBu" : \
	 i == HDSPE_AIO_PRO_INPUT_LEVEL_PLUS_24_DBU ? "+24 dBu" : \
	 "???")

/* AIO Pro Output levels. Defines the output voltage caused by
 * 0 dBFS digital output. */
enum hdspe_aio_pro_output_level {
	HDSPE_AIO_PRO_OUTPUT_LEVEL_MINUS_2_DBU_RCA  = 0,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_4_DBU_RCA   = 1,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_13_DBU_RCA  = 2,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_19_DBU_RCA  = 3,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_4_DBU_XLR   = 4,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_13_DBU_XLR  = 5,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_19_DBU_XLR  = 6,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_24_DBU_XLR  = 7,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_COUNT            = 8,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_INVALID          = 9,
	HDSPE_AIO_PRO_OUTPUT_LEVEL_FORCE_32BIT      = 0xffffffff
};

#define HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(i)				\
	(i == HDSPE_AIO_PRO_OUTPUT_LEVEL_MINUS_2_DBU_RCA ? "-2 dBu RCA"  : \
	 i == HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_4_DBU_RCA  ? "+4 dBu RCA"  : \
	 i == HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_13_DBU_RCA ? "+13 dBu RCA" : \
	 i == HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_19_DBU_RCA ? "+19 dBu RCA" : \
	 i == HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_4_DBU_XLR  ? "+4 dBu XLR"  : \
	 i == HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_13_DBU_XLR ? "+13 dBu XLR" : \
	 i == HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_19_DBU_XLR ? "+19 dBu XLR" : \
	 i == HDSPE_AIO_PRO_OUTPUT_LEVEL_PLUS_24_DBU_XLR ? "+24 dBu XLR" : \
	 "???")

/* AIO Pro Phones levels */
enum hdspe_aio_pro_phones_level {
	HDSPE_AIO_PRO_PHONES_LEVEL_LO_POWER = 0,
	HDSPE_AIO_PRO_PHONES_LEVEL_HI_POWER = 1,
	HDSPE_AIO_PRO_PHONES_LEVEL_COUNT    = 2,
	HDSPE_AIO_PRO_PHONES_LEVEL_INVALID  = 3,
	HDSPE_AIO_PRO_PHONES_LEVEL_FORCE_32BIT = 0xffffffff
};

#define HDSPE_AIO_PRO_PHONES_LEVEL_NAME(i) \
	(i == HDSPE_AIO_PRO_PHONES_LEVEL_LO_POWER  ? "Lo Power"  : \
	 i == HDSPE_AIO_PRO_PHONES_LEVEL_HI_POWER  ? "Hi Power"  : \
	 "???")

/* -------------------- IOCTL Peak/RMS Meters -------------------- */

struct hdspe_peak_rms {
	uint32_t input_peaks[64];
	uint32_t playback_peaks[64];
	uint32_t output_peaks[64];

	uint64_t input_rms[64];
	uint64_t playback_rms[64];
	uint64_t output_rms[64];

	uint8_t speed; /* enum {ss, ds, qs} */
	int32_t status2;
};

#define SNDRV_HDSPE_IOCTL_GET_PEAK_RMS \
	_IOR('H', 0x42, struct hdspe_peak_rms)

/* ------------ CONFIG block IOCTL ---------------------- */

struct hdspe_config {
	unsigned char pref_sync_ref;
	unsigned char wordclock_sync_check;
	unsigned char madi_sync_check;
	unsigned int system_sample_rate;
	unsigned int autosync_sample_rate;
	unsigned char system_clock_mode;
	unsigned char clock_source;
	unsigned char autosync_ref;
	unsigned char line_out;
	unsigned int passthru;
	unsigned int analog_out;
};

#define SNDRV_HDSPE_IOCTL_GET_CONFIG \
	_IOR('H', 0x41, struct hdspe_config)

/* ------------ Time Code Option (TCO) IOCTL --------------- */

enum hdspe_ltc_frame_rate {
	HDSPE_LTC_FRAME_RATE_24       =0,
	HDSPE_LTC_FRAME_RATE_25       =1,
	HDSPE_LTC_FRAME_RATE_29_97    =2,
	HDSPE_LTC_FRAME_RATE_30       =3,
	HDSPE_LTC_FRAME_RATE_COUNT    =4,
	HDSPE_LTC_FRAME_RATE_FORCE_32BIT =0xffffffff
};

#define HDSPE_LTC_FRAME_RATE_NAME(i)			\
	(i == HDSPE_LTC_FRAME_RATE_24    ? "24" :	\
	 i == HDSPE_LTC_FRAME_RATE_25    ? "25" :	\
	 i == HDSPE_LTC_FRAME_RATE_29_97 ? "29.97" :	\
	 i == HDSPE_LTC_FRAME_RATE_30    ? "30" :	\
	 "???")

enum hdspe_video_format {
	HDSPE_VIDEO_FORMAT_NO_VIDEO   =0,
	HDSPE_VIDEO_FORMAT_NTSC       =1,
	HDSPE_VIDEO_FORMAT_PAL        =2,
	HDSPE_VIDEO_FORMAT_COUNT      =3,	
	HDSPE_VIDEO_FORMAT_FORCE_32BIT =0xffffffff
};

#define HDSPE_VIDEO_FORMAT_NAME(i)				\
	(i == HDSPE_VIDEO_FORMAT_NO_VIDEO    ? "No Video" :	\
	 i == HDSPE_VIDEO_FORMAT_NTSC        ? "NTSC" :		\
	 i == HDSPE_VIDEO_FORMAT_PAL         ? "PAL" :		\
	 "???")

enum hdspe_tco_source {
	HDSPE_TCO_SOURCE_WCK          =0,
	HDSPE_TCO_SOURCE_VIDEO        =1,
	HDSPE_TCO_SOURCE_LTC          =2,
	HDSPE_TCO_SOURCE_COUNT        =3,
	HDSPE_TCO_SOURCE_FORCE_32BIT  =0xffffffff
};

#define HDSPE_TCO_SOURCE_NAME(i)				\
	(i == HDSPE_TCO_SOURCE_WCK        ? "Word Clk" :	\
	 i == HDSPE_TCO_SOURCE_VIDEO      ? "Video" :		\
	 i == HDSPE_TCO_SOURCE_LTC        ? "LTC" :		\
	 "???")

enum hdspe_pull {
	HDSPE_PULL_NONE              =0,
	HDSPE_PULL_UP_0_1            =1,
	HDSPE_PULL_DOWN_0_1          =2,
	HDSPE_PULL_UP_4              =3,
	HDSPE_PULL_DOWN_4            =4,
	HDSPE_PULL_COUNT             =5,
	HDSPE_PULL_FORCE_32BIT       =0xffffffff
};

#define HDSPE_PULL_NAME(i)				\
	(i == HDSPE_PULL_NONE        ? "0" :		\
	 i == HDSPE_PULL_UP_0_1      ? "+0.1 %" :	\
	 i == HDSPE_PULL_DOWN_0_1    ? "-0.1 %" :	\
	 i == HDSPE_PULL_UP_4        ? "+4 %" :		\
	 i == HDSPE_PULL_DOWN_4      ? "-4 %" :		\
	 "???")

enum hdspe_tco_sample_rate {
	HDSPE_TCO_SAMPLE_RATE_44_1     =0,
	HDSPE_TCO_SAMPLE_RATE_48       =1,
	HDSPE_TCO_SAMPLE_RATE_FROM_APP =2,
	HDSPE_TCO_SAMPLE_RATE_COUNT    =3,
	HDSPE_TCO_SAMPLE_RATE_FORCE_32BIT = 0xffffffff
};

#define HDSPE_TCO_SAMPLE_RATE_NAME(i)				\
	(i == HDSPE_TCO_SAMPLE_RATE_44_1     ? "44.1 KHz" :	\
	 i == HDSPE_TCO_SAMPLE_RATE_48       ? "48 KHz" :	\
	 i == HDSPE_TCO_SAMPLE_RATE_FROM_APP ? "From App" :	\
	 "???")

enum hdspe_wck_conversion {
	HDSPE_WCK_CONVERSION_1_1       =0,
	HDSPE_WCK_CONVERSION_44_1_48   =1,
	HDSPE_WCK_CONVERSION_48_44_1   =2,
	HDSPE_WCK_CONVERSION_COUNT     =3,
	HDSPE_WCK_CONVERSION_FORCE_32BIT =0xffffffff
};

#define HDSPE_WCK_CONVERSION_NAME(i)					\
	(i == HDSPE_WCK_CONVERSION_1_1     ? "1:1" :			\
	 i == HDSPE_WCK_CONVERSION_44_1_48 ? "44.1 KHz -> 48 KHz" :	\
	 i == HDSPE_WCK_CONVERSION_48_44_1 ? "48 KHz -> 44.1 KHz" :	\
	 "???")

#ifdef NEVER
#pragma scalar_storage_order little-endian

/* Raw 32-bit HDSPe LTC code - does not include user bits (TCO doesn't 
 * report user bits) */
struct hdspe_raw_ltc { uint32_t 
        frame_units : 4,
	frame_tens  : 2,
	drop_frame  : 1,
	color_frame : 1,
		
	sec_units   : 4,
	sec_tens    : 3,
	flag1       : 1,

	min_units   : 4,
        min_tens    : 3,
	flag2       : 1,

	hour_units  : 4,
	hour_tens   : 2,
	clock_flag  : 1,
	flag3       : 1;
};

/* Generic 64-bit LTC code including user bits (all 0 for HDSPe TCO), 
 * as returned by the "TCO LTC In" ALSA control. */
struct hdspe_ltc { uint64_t
        frame_units : 4,
	user1       : 4,
		
	frame_tens  : 2,
	drop_frame  : 1,
	color_frame : 1,
	user2       : 4,
		
	sec_units   : 4,
	user3       : 4,

	sec_tens    : 3,
	flag1       : 1,
	user4       : 4,

	min_units   : 4,
	user5       : 4,

        min_tens    : 3,
	flag2       : 1,
	user6       : 4,

	hour_units  : 4,
	user7       : 4,

	hour_tens   : 2,
	clock_flag  : 1,
	flag3       : 1,
	user8       : 4;
};

#pragma scalar_storage_order default
#endif /*NEVER*/

struct hdspe_tco_status {
	uint32_t version;
	
	uint32_t ltc_in;
	uint32_t ltc_in_offset;
	
	enum hdspe_bool tco_lock;
	enum hdspe_bool ltc_valid;
	enum hdspe_ltc_frame_rate ltc_in_fps;
	enum hdspe_bool ltc_in_drop;
	enum hdspe_video_format video;
	enum hdspe_bool wck_valid;
	enum hdspe_speed wck_speed;

	enum hdspe_tco_source input;
	enum hdspe_ltc_frame_rate ltc_fps;
	enum hdspe_bool ltc_drop;
	enum hdspe_tco_sample_rate sample_rate;
	enum hdspe_pull pull;
	enum hdspe_wck_conversion wck_conversion;
	enum hdspe_bool term;

	// LTC output control
	enum hdspe_bool ltc_run;
	enum hdspe_bool ltc_flywheel;
};

#define SNDRV_HDSPE_IOCTL_GET_LTC _IOR('H', 0x46, struct hdspe_tco_status)

/* ------------ STATUS block IOCTL ---------------------- */

/*
 * The status data reflects the device's current state
 * as determined by the card's configuration and
 * connection status.
 */

/* Device status */
struct hdspe_status {
	// API version (this structs size and layout may change with version)
	uint32_t                   version;

	// Exact system sample rate as a fraction of a 64-bit unsigned
	// integer over a 32-bit unsigned integer. The numerator is
	// the cards frequency constant adapted to the current speed
	// mode. The denominator is the content of the DDS hardware
	// status register.
	uint32_t                   sample_rate_denominator;
	uint64_t                   sample_rate_numerator;

	// Exact internal sample rate is sample_rate_numerator /
	// internal_sample_rate_denominator. internal_sample_rate_denominator
	// is the content of the DDS hardware control register.
	uint32_t                   internal_sample_rate_denominator;
  
	// Period buffer size in number of samples.
	uint32_t                   buffer_size;

	// Whether the card is running or not, and what processes
	// are capturing or playing back on the card (-1 if none).
	// Frequency shall not be changed by means of the snd_ctls
	// if running.
	enum hdspe_bool            running;
	pid_t                      capture_pid;
	pid_t                      playback_pid;

	// Device clock mode: AutoSync (0) or Master (1).
	enum hdspe_clock_mode      clock_mode;

	// Frequency class of the internal clock. The internal clock
	// is used when in Master clock mode, or no valid AutoSync
	// reference is detected.
	enum hdspe_freq            internal_freq;

	// Preferred clock source for AutoSync clock mode.
	enum hdspe_clock_source    preferred_ref;

	// Active clock source ("AutoSync reference").
	enum hdspe_clock_source    autosync_ref;

	// AutoSync clock source sync status.
	enum hdspe_sync_status     sync[HDSPE_CLOCK_SOURCE_COUNT];

	// AutoSync clock source frequency class.
	enum hdspe_freq            freq[HDSPE_CLOCK_SOURCE_COUNT];

	// Frequency class of the active AutoSync reference, if any. Used
	// if in AutoSync clock mode and a valid AutoSync reference is
	// available (autosync_ref != HDSPE_SYNC_INTERN).
	enum hdspe_freq            external_freq;
	
	// Single-speed word clock output. If false, word clock output
	// is at the systems sample rate. If true, word clock output
	// frequency is the systems sample rate reduced to single speed.
	enum hdspe_bool            wck48;

	// Current speed mode.
	enum hdspe_speed           speed_mode;

	// Clear track markers. Affects channel status and track marker
	// bits in AES, S/PDIF, ADAT and MADI input.
	enum hdspe_bool            clr_tms;

  //	union hdspe_card_specific_status {
		// MADI specific status
		struct hdspe_status_madi {
			enum hdspe_madi_input       input_select;
			enum hdspe_bool             auto_select;
			enum hdspe_bool             tx_64ch;
			enum hdspe_bool             smux;
			enum hdspe_madi_input       input_source;
			enum hdspe_bool             rx_64ch;
		} madi;

		// AES specific status
		struct hdspe_status_aes {
			enum hdspe_bool             pro;
			enum hdspe_bool             emp;
			enum hdspe_bool             dolby;
			enum hdspe_bool             smux;
			enum hdspe_ds_mode          ds_mode;
			enum hdspe_qs_mode          qs_mode;
			uint32_t                    aes_mode;
		} aes;

		// RayDAT / AIO / AIO Pro specific status
		struct hdspe_status_raio {
			// Output Audio Extension Board presence.
			enum hdspe_bool    aebo;

			// Input Audio Extension Board presence.
			enum hdspe_bool    aebi;

			// S/PDIF input source.
			enum hdspe_raio_spdif_input    spdif_in;

			// S/PDIF optical output.
			enum hdspe_bool    spdif_opt;

			// S/PDIF professional format.
			enum hdspe_bool    spdif_pro;

			union {
				// AIO analog I/O levels
				struct {
					enum hdspe_aio_level  input_level;
					enum hdspe_aio_level  output_level;
					enum hdspe_aio_level  phones_level;
					enum hdspe_bool       xlr;
				} aio;

				// AIO Pro analog I/O levels
				struct {
					enum hdspe_aio_pro_input_level
					                      input_level;
					enum hdspe_aio_pro_output_level
					                      output_level;
					enum hdspe_aio_pro_phones_level
					                      phones_level;
					uint32_t              reserved;
				} aio_pro;
			};
		} raio;
  //	};
};

#define SNDRV_HDSPE_IOCTL_GET_STATUS \
	_IOR('H', 0x49, struct hdspe_status)

/* 47 is hdspm status - incompatible */

/* ------------- Card information  --------------- */

/*
 * DEPRECATED (hdspm compatible)
 */

#define HDSPE_ADDON_TCO 1

struct hdspe_version {
	__u8 card_type; /* enum hdspe_io_type */
	char cardname[20];
	unsigned int serial;
	unsigned short firmware_rev;
	int addons;
};

#define SNDRV_HDSPE_IOCTL_GET_VERSION _IOR('H', 0x48, struct hdspe_version)

/*
 * Use the following in new developments.
 */

#define HDSPE_EXPANSION_TCO     0x01
#define HDSPE_EXPANSION_AI4S    0x02
#define HDSPE_EXPANSION_AO4S    0x04

#ifndef PCI_VENDOR_ID_XILINX
#define PCI_VENDOR_ID_XILINX    0x10ee
#endif
#ifndef PCI_VENDOR_ID_RME
#define PCI_VENDOR_ID_RME       0x1d18
#endif

struct hdspe_card_info {
	// API version (this structs size and layout may change with version)
	uint32_t                   version;

	enum hdspe_io_type         card_type;
	uint32_t                   serial;          /* serial nr */
	uint32_t                   fw_rev;    // firmware revision 
	uint32_t                   fw_build;  // firmware build

	uint32_t                   irq;
	uint64_t                   port;
	uint32_t                   vendor_id; // PCI vendor ID: Xilinx or RME

	uint32_t                   expansion;
};

#define SNDRV_HDSPE_IOCTL_GET_CARD_INFO \
	_IOR('H', 0x45, struct hdspe_status)


/* ------------- Matrix Mixer IOCTL --------------- */

/* MADI mixer: 64inputs+64playback in 64outputs = 8192 => *4Byte =
 * 32768 Bytes
 */

/* Organisation is 64 channelfader in a continuous memory block,
 * equivalent to hardware definition, maybe for future feature of mmap of
 * them.
 * Each of 64 outputs has 64 input faders and 64 software playback faders.
 * Input in to output out fader is mixer.ch[out].in[in].
 * Software playback pb to output out fader is mixer.ch[out].pb[pb] */

/* Mixer Values */
#define HDSPE_UNITY_GAIN          32768	/* = 65536/2 */
#define HDSPE_MINUS_INFINITY_GAIN 0

#define HDSPE_MIXER_CHANNELS HDSPE_MAX_CHANNELS

struct hdspe_channelfader {
	uint32_t in[HDSPE_MIXER_CHANNELS];
	uint32_t pb[HDSPE_MIXER_CHANNELS];
};

struct hdspe_mixer {
	struct hdspe_channelfader ch[HDSPE_MIXER_CHANNELS];
};

struct hdspe_mixer_ioctl {
	struct hdspe_mixer *mixer;
};

/* use indirect access due to the limit of ioctl bit size */
#define SNDRV_HDSPE_IOCTL_GET_MIXER _IOR('H', 0x44, struct hdspe_mixer_ioctl)

/* typedefs for compatibility to user-space */
typedef struct hdspe_peak_rms hdspe_peak_rms_t;
typedef struct hdspe_config_info hdspe_config_info_t;
typedef struct hdspe_version hdspe_version_t;
typedef struct hdspe_channelfader snd_hdspe_channelfader_t;
typedef struct hdspe_mixer hdspe_mixer_t;


#endif /* __SOUND_HDSPE_H */

