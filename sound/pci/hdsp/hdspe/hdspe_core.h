// SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note
/**
 * @file hdspe_core.h
 * @brief RME HDSPe driver internal header.
 *
 * 20210728 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORs,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

/* TODO: remove in production version */
#define DEBUG
#define CONFIG_SND_DEBUG

#ifndef __SOUND_HDSPE_CORE_H
#define __SOUND_HDSPE_CORE_H

#include <linux/io.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/info.h>

// #define HDSPE_HDSP_REV  60  //  HDSPe PCIe/ExpressCard
#define HDSPE_MADI_REV		210  // TODO: use
#define HDSPE_RAYDAT_REV	211
#define HDSPE_AIO_REV		212
#define HDSPE_MADIFACE_REV	213
#define HDSPE_AES_REV		240  // TODO: use

/* --- Write registers. ---
  These are defined as byte-offsets from the iobase value.  */

#define HDSPE_WR_SETTINGS              0  /* RayDAT, AIO, AIO Pro settings */
#define HDSPE_outputBufferAddress     32
#define HDSPE_inputBufferAddress      36
#define HDSPE_WR_FLASH             (12*4) /* get serial nr on older firmware */
#define HDSPE_WR_CONTROL	      64  /* main control register */
#define HDSPE_interruptConfirmation   96
#define HDSPE_WR_TCO                 128  /* Time Code Option control */
#define HDSPE_WR_PLL_FREQ            256  /* for setting arbitrary clock values (DDS feature) */

#define HDSPE_midiDataOut0           352
#define HDSPE_midiDataOut1           356
#define HDSPE_midiDataOut2           368

#define HDSPE_eeprom_wr		     384  /* for AES, not used in mac driver */
#define HDSPE_eeprom_rd              (97*4)   /* Unused */

/* DMA enable for 64 channels, only Bit 0 is relevant */
#define HDSPE_outputEnableBase       512  /* 512-767  input  DMA */
#define HDSPE_inputEnableBase        768  /* 768-1023 output DMA */

// TODO: LOOPBACK
#define MADI_RECORD_OUTPUT	(384*4)   /* (384+i)*4 = channel i loopback */

/* 16 page addresses for each of the 64 channels DMA buffer in and out
   (each 64k=16*4k) Buffer must be 4k aligned (which is default i386 ????) */
#define HDSPE_pageAddressBufferOut       8192
#define HDSPE_pageAddressBufferIn        (HDSPE_pageAddressBufferOut+64*16*4)

/* Hardware mixer: for each hardware output, there is a hardware input
 * fader and a software playback fader. Regardless of the actual number of
 * physical inputs and outputs of a card, the mixer always accomodates 
 * 64 hardware outputs, 64 hardware inputs and 64 software playbacks.
 * So its size is 64*64*2 values. Each value is an uint16_t stored
 * into a regular uint32_t i/o mapped register. 0 means mute. 32768 is
 * unit gain.
 * Hardware input index i fader for output index o
 * is at byte address 4*(mixerBase + 128*o + i).
 * Software playback index p fader for output index o
 * is at byte address 4*(mixerBase + 128*o + 64 + p). */
#define HDSPE_MADI_mixerBase    32768	/* 32768-65535 for 2x64x64 Fader */

#define HDSPE_MATRIX_MIXER_SIZE  8192	/* = 2*64*64 * 4 Byte => 32kB */


/* --- Read registers. ---
   These are defined as byte-offsets from the iobase value */

#define HDSPE_RD_STATUS0         0     /* all cards, different interpretation */
#define HDSPE_RD_STATUS1        64     /* RayDAT, AIO, AIO Pro */
#define HDSPE_RD_STATUS2       192     /* all cards, different interpretation */
#define HDSPE_RD_FBITS         128     /* all cards except MADI */
#define HDSPE_RD_TCO           256     /* Time Code Option status */
#define HDSPE_RD_BARCODE0      (104*4) /* serial number part 1, rev2 cards */
#define HDSPE_RD_BARCODE1      (105*4) /* serial number part 2, rev2 cards */
#define HDSPE_RD_FLASH	       (112*4) /* contains firmware build */
#define HDSPE_RD_PLL_FREQ      512     /* PLL frequency (DDS feature) */

#define HDSPE_midiDataIn0     360
#define HDSPE_midiDataIn1     364
#define HDSPE_midiDataIn2     372
#define HDSPE_midiDataIn3     376

/* status is data bytes in MIDI-FIFO (0-128) */
#define HDSPE_midiStatusOut0  384
#define HDSPE_midiStatusOut1  388
#define HDSPE_midiStatusOut2  400

#define HDSPE_midiStatusIn0   392
#define HDSPE_midiStatusIn1   396
#define HDSPE_midiStatusIn2   404
#define HDSPE_midiStatusIn3   408

/* Peak and RMS level meters are regular i/o-mapped registers, but offset
   considerably from the rest. the peak registers are reset
   when read; the least-significant 4 bits are full-scale counters;
   the actual peak value is in the most-significant 24 bits.
*/
#define HDSPE_MADI_INPUT_PEAK		4096
#define HDSPE_MADI_PLAYBACK_PEAK	4352
#define HDSPE_MADI_OUTPUT_PEAK		4608

#define HDSPE_MADI_INPUT_RMS_L		6144
#define HDSPE_MADI_PLAYBACK_RMS_L	6400
#define HDSPE_MADI_OUTPUT_RMS_L		6656

#define HDSPE_MADI_INPUT_RMS_H		7168
#define HDSPE_MADI_PLAYBACK_RMS_H	7424
#define HDSPE_MADI_OUTPUT_RMS_H		7680


/* Register definitions.
 * All registers are 32-bit little endian. */
#pragma scalar_storage_order little-endian

/**
 * WR_CONTROL register (byte offset 64)
 *

 * Pos    Mask  MADI               AES               RayDAT/AIO/AIO Pro
 *
 * 00        1  START              START             START
 * 01        2  LAT_0              LAT_0             LAT_0
 * 02        4  LAT_1              LAT_1             LAT_1
 * 03        8  LAT_2              LAT_2             LAT_2
 * 04       10  Master             Master
 * 05       20  IE_AUDIO           IE_AUDIO          IE_AUDIO
 * 06       40  freq0              freq0             freq0
 * 07       80  freq1              freq1             freq1
 * 08      100  freq2              freq2             freq2
 * 09      200                     PRO
 * 10      400  tx_64ch            EMP
 * 11      800  AutoInp            Dolby
 * 12     1000  
 * 13     2000                     SyncRef2
 * 14     4000  inp_0
 * 15     8000  inp_1
 * 16    10000  SyncRef0           SyncRef0
 * 17    20000  SyncRef1           SyncRef1
 * 18    40000  SMUX               SMUX
 * 19    80000  CLR_TMS            CLR_TMS
 * 20   100000  WCK48              WCK48
 * 21   200000  IEN2               IEN2             IEN2 (RayDAT TCO)
 * 22   400000  IEN0               IEN0             IEN0
 * 23   800000  IEN1               IEN1             IEN1 (AIO/Pro: TCO)
 * 24  1000000  LineOut            LineOut          LineOut
 * 25  2000000  HDSPe_float_format SyncRef3         HDSPe_float_format
 * 26  4000000  IEN3 (TCO)         DS_DoubleWire
 * 27  8000000                     QS_DoubleWire
 * 28 10000000                     QS_QuadWire
 * 29 20000000
 * 30 40000000                     AES_float_format
 * 31 80000000  freq3              freq3            freq3
 *
 * LAT_{0,1,2}: 3-bit value defining buffer size (and latency): 0=64 samples, 
 * 1=128, ..., 6=4096, 7=8192 on MADI/AES, 32 on RayDAT/AIO/AIO Pro.
 * 
 */

/* MIDI interrupt enable bitmasks */
#define HDSPE_Midi0InterruptEnable 0x00400000 /* MIDI 0 interrupt enable */
#define HDSPE_Midi1InterruptEnable 0x00800000 /* MIDI 1 interrupt enable */
#define HDSPE_Midi2InterruptEnable 0x00200000 /* MIDI 2 interrupt enable */
#define HDSPE_Midi3InterruptEnable 0x04000000 /* MIDI 3 interrupt enable */

/* Common to all cards */
struct hdspe_control_reg_common { __le32
	START   : 1,   // start engine 
	LAT     : 3,   // buffer size is 2^n where n is defined by LAT
	_04     : 1,
	IE_AUDIO: 1,   // what do you think?
	freq    : 2,   // internal clock: 1=32KHz, 2=44.1KKhz, 3=48KHz
	
	ds      : 1,   // double speed mode	
	_09     : 1,
	_10     : 1,
	_11     : 1,
	_12     : 1,
	_13     : 1,
	_14     : 1,
	_15     : 1,

	_16     : 1,
	_17     : 1,
	_18     : 1,
	_19     : 1,
	_20     : 1,
	IEN2    : 1,   // MIDI 2 interrupt enable
	IEN0    : 1,   // MIDI 0 interrupt enable
	IEN1    : 1,   // MIDI 1 interrupt enable

	LineOut : 1,   // Analog Out on channel 63/64 on=1, mute=0 
	_25     : 1,
	_26     : 1,
	_27     : 1,
	_28     : 1,
	_29     : 1,
	_30     : 1,
	qs      : 1;    // quad speed mode
};

/* MADI */
struct hdspe_control_reg_madi { __le32
	START   : 1,
	LAT     : 3,
	Master  : 1,   // Clock mode: 0=AutoSync, 1=Master
	IE_AUDIO: 1,
	freq    : 2,

	ds      : 1,
	_09     : 1,
	tx_64ch : 1,   // Output 64channel MODE=1, 56channelMODE=0
	AutoInp : 1,   // Auto Input (takeover) == Safe Mode, 0=off, 1=on 
	opt_out : 1,   // unused
	_13     : 1,
	inp_0   : 1,   // Input select 0=optical, 1=coaxial
	inp_1   : 1,   // Unused. Must be 0.

	SyncRef : 2,   // Preferred AutoSync reference:
	               // 0=WCK, 1=MADI, 2=TCO, 3=SyncIn
	SMUX    : 1,   // Frame???
	CLR_TMS : 1,   // Clear AES/SPDIF channel status and track marker bits
	WCK48   : 1,   // Single speed word clock output
	IEN2    : 1,
	IEN0    : 1,
	IEN1    : 1,

	LineOut : 1,
	FloatFmt: 1,   // 32-bit LE floating point sample format
	IEN3    : 1,   // MIDI 3 interrupt enable (TCO)
	_27     : 1,
	_28     : 1,
	_29     : 1,
	_30     : 1,
	qs      : 1;
};

/* AES */
struct hdspe_control_reg_aes { __le32
	START   : 1,
	LAT     : 3,
	Master  : 1,   // Clock mode: 0=AutoSync, 1=Master
	IE_AUDIO: 1,
	freq    : 2,

	ds      : 1,
	PRO     : 1,   // Professional
	EMP     : 1,   // Emphasis
	Dolby   : 1,   // Dolby = "NonAudio"
	_12     : 1,
	SyncRef2: 1,   // Preferred AutoSync ref bit 2. Values:
	_14     : 1,   // 0=WCLK, 1=AES1, 2=AES2, 3=AES3, 4=AES4,
	_15     : 1,   // 5=AES5, 6=AES6, 7=AES7, 8=AES8, 9=TCO, 10=SyncIn

	SyncRef0: 1,   // Preferred AutoSync ref bit 0
	SyncRef1: 1,   // Preferred AutoSync ref bit 1
	SMUX    : 1,
	CLR_TMS : 1,   // Clear AES/SPDIF channel status and track marker bits
	WCK48   : 1,   // Single speed word clock output
	IEN2    : 1,
	IEN0    : 1,
	IEN1    : 1,

	LineOut : 1,
	SyncRef3: 1,   // Preferred AutoSync ref bit 3
	ds_mode : 1,   // DoubleSpeed mode: 0=single wire, 1=double wire
	qs_mode : 2,   // QuadSpeed mode: 0=single, 1=double, 2=quad wire
	_29     : 1,
	FloatFmt: 1,   // 32-bit LE floating point sample format
	qs      : 1;
};

/* RayDAT / AIO / AIO Pro */
struct hdspe_control_reg_raio { __le32
	START   : 1,
        LAT     : 3,
	_4      : 1,
	IE_AUDIO: 1,
	freq    : 2,

	ds      : 1,
	_09     : 1,
	_10     : 1,
	_11     : 1,
	_12     : 1,
	_13     : 1,
	_14     : 1,
	_15     : 1,

	_16     : 1,
	_17     : 1,
	_18     : 1,
	_19     : 1,
	_20     : 1,
	IEN2    : 1,
	IEN0    : 1,
	IEN1    : 1,

	LineOut : 1,
	FloatFmt: 1,   // 32-bit LE floating point sample format
	_26     : 1,
	_27     : 1,
	_28     : 1,
	_29     : 1,
	_30     : 1,
	qs      : 1;
};

union hdspe_control_reg {
	__le32                          raw;    // register value
	struct hdspe_control_reg_common common; // common fields
	struct hdspe_control_reg_madi   madi;   // MADI fields
	struct hdspe_control_reg_aes    aes;    // AES fields
	struct hdspe_control_reg_raio   raio;   // RayDAT/AIO/AIO Pro fields
};


/**
 * WR_SETTINGS register bits (byte offset 0) - RayDAT / AIO / AIO Pro only.
 */

struct hdspe_settings_reg_raio { __le32
	Master    : 1,  // Clock Master (1) or AutoSync (0)
	SyncRef   : 4,  // Preferred AutoSync reference:
	                // 0=WCLK, 1=AES, 2=SPDIF, 3=ADAT/ADAT1
	                // 4=ADAT2, 5=ADAT3, 6=ADAT4, 9=TCO, 10=SyncIn
	Wck48     : 1,  // Single speed word clock output = Single Speed in win
	DS_DoubleWire : 1,        // win , mac unused
	QS_DoubleWire : 1,        // win , mac unused

	QS_QuadWire   : 1,        // win , mac unused
	Madi_Smux     : 1,        // win , mac unused
	Madi_64_Channels  : 1,    // win , mac unused
	Madi_AutoInput : 1,       // win , mac unused
	Input     : 2,  // SPDIF in: 0=Optical, 1=Coaxial, 2=Internal
	Spdif_Opt : 1,  // SPDIF Optical out (ADAT4 optical out on RayDAT)
	Pro       : 1,  // SPDIF Professional format out

	clr_tms   : 1,  // Clear AES/SPDIF channel status and track marker bits
	AEB1      : 1,  // ADAT1 internal (use with TEB or AEB extension boards)
	AEB2      : 1,  // RayDAT ADAT2 internal, AIO: S/PDIF emphasis??
	LineOut   : 1,  // AIO Pro only (AD/DA/PH power on/off?)
	AD_GAIN   : 2,  // Input Level 0-2 (AIO) / 0-3 (AIO Pro)
	DA_GAIN   : 2,  // Output Level 0-2 (AIO) / 0-3 (AIO Pro)

	PH_GAIN   : 2,  // Phones Level 0-2 (AIO) / 0-1 (AIO Pro)
	Sym6db    : 1,  // Analog output: 0=RCA, 1=XLR
	_27       : 1,
	_28       : 1,
	_29       : 1,
	_30       : 1,
	_21       : 1;
};

union hdspe_settings_reg {
	__le32                         raw;  // register value
	struct hdspe_settings_reg_raio raio; // RayDAT / AIO / AIO Pro bits
};


/**
 * Clock logic:
 *
 * If the card is running in master clock mode or no valid AutoSync
 * reference is present, the internal clock is used ("internal frequency"). 
 * If in AutoSync mode, and a valid AutoSync reference is present, the card 
 * synchronises to that AutoSync reference ("external frequency").
 *
 * In all cases, the RD_PLL_FREQ register indicates at what exact frequency
 * the card is running, relative to a fixed oscillator frequency that depends
 * on the type of card - see hdspe_read_system_sample_rate() - and up to 
 * double/quad speed rate multiplication as set in the WR_CONTROL register.
 *
 * The frequency class of the internal clock is in the WR_CONTROL register
 * (see above). When reduced to single speed mode, it shall always correspond 
 * to the PLL frequency rounded to the nearest of 32KHz, 44.1KHz, 48KHz.
 * If in Master clock mode, the internal clock frequency is fine tuned through 
 * the PLL frequency control register (WR_PLL_FREQ), see 
 * hdspe_write_system_sample_rate().
 *
 * MADI cards provide frequency and lock and sync status of the MADI input
 * and the external frequency (up to single/double/quad speed rate setting). 
 * The MADI input lock/sync/frequency status is in the RD_STATUS0 register. The
 * external frequency and selected AutoSync reference is in RD_STATUS2.
 * lock/sync status of tco and sync in are in the RD_STATUS register,
 * lock/sync status of the word clock is in the RD_STATUS2 register.
 *
 * Other cards provide frequency and lock/sync status of the word clock, tco, 
 * sync in, and all digital audio inputs individually, in different
 * registers depending on the type of card.
 *
 * The frequency class values reported in the status registers are always
 * 4-bit values, with the following interpretation:
 *   1=32KHz, 2=44.1KHz, 3=48KHz, 
 *   4=64KHz, 5=88.2KHz, 6=96KHz,
 *   7=128KHz, 8=176.4KHz, 9=192KHz
 *   other values indicate no lock.
 *
 * All cards report whether there is a valid external clock reference or not, 
 * and if so, which reference is used for synchronisation.
 * The selected AutoSync reference is in different places and has different
 * semantics for MADI, AES or RayDAT/AIO/AIO Pro. For MADI, it is a 3-bit 
 * value in the RD_STATUS2 register. For other cards it is a 4-bit value.
 * For AES, it is sync_ref{3,2,1,0} in the RD_STATUS0 register.
 * For RayDAT/AIO/AIO Pro it is in the RD_STATUS1 register.
 *
 * So, MADI cards readily report their external frequency, if in sync.
 * For other cards, we need to read the selected AutoSync reference. The
 * external frequency is the frequency of thet AutoSync reference.
 *
 * The external frequency thus found needs conversion of the single/double/
 * quad speed mode as set in the WR_CONTROL register.
 */

/**
 * RD_STATUS0 register (byte offset 0) bits:
 *
 * Pos    Mask  MADI               AES                RayDAT/AIO/AIO Pro
 *
 * 00        1  IRQ                IRQ                IRQ
 * 01        2  rx_64ch            tco_freq0
 * 02        4  AB_int             tco_freq1
 * 03        8  madi_lock          tco_freq2
 * 04       10                     tco_freq3
 * 05       20  madi_tco_lock      mIRQ2_AES
 *        FFC0  hw buffer pointer  hw buffer pointer  hw buffer pointer  
 * 16    10000  madi_sync_in_lock  sync_ref0
 * 17    20000  madi_sync_in_sync  sync_ref1
 * 18    40000  madi_sync          sync_ref2
 * 19    80000                     sync_ref3
 * 20   100000                     wclk_sync
 * 21   200000  mIRQ3              wclk_lock
 * 22   400000  F_0                wclk_freq0
 * 23   800000  F_1                wclk_freq1
 * 24  1000000  F_2                wclk_freq2
 * 25  2000000  F_3                wclk_freq3
 * 26  4000000  BUF_ID             BUF_ID             BUF_ID
 * 27  8000000  tco_detect         tco_detect
 * 28 10000000  tco_sync           tco_sync
 * 29 20000000  mIRQ2              aes_tco_lock       mIRQ2 (RayDAT TCO)
 * 30 40000000  mIRQ0              mIRQ0              mIRQ0
 * 31 80000000  mIRQ1              mIRQ1              mIRQ1 (AIO/Pro: TCO)
 */

/* MIDI interrupt pending */
#define HDSPE_midi0IRQPending    0x40000000
#define HDSPE_midi1IRQPending    0x80000000
#define HDSPE_midi2IRQPending    0x20000000
#define HDSPE_midi2IRQPendingAES 0x00000020   /* AES with TCO */
#define HDSPE_midi3IRQPending    0x00200000   /* MADI with TCO */

/* common */
struct hdspe_status0_reg_common { __le32
	IRQ          : 1,  // Audio interrupt pending
	_01          : 1,
	_02          : 1,
	_03          : 1,
	_04          : 1,
	_05          : 1,
	BUF_PTR      : 10, // Most significant bits of the hardware buffer
	                   // pointer. Since 64-byte accurate, the least
	                   // significant 6 bits are 0. Little endian!

	_16          : 1,
	_17          : 1,
	_18          : 1,	
	_19          : 1,
	_20          : 1,
	_21          : 1,
	_22          : 1,
	_23          : 1,

	_24          : 1,
	_25          : 1,
	BUF_ID       : 1,  // (Double) buffer ID, toggles with interrupt
	_27          : 1,	
	_28          : 1,
	_29          : 1,
	mIRQ0        : 1,  // MIDI 0 interrupt pending
	mIRQ1        : 1;  // MIDI 1 interrupt pending
};

/* MADI */
struct hdspe_status0_reg_madi { __le32
	IRQ          : 1,
	rx_64ch      : 1,  // Input 64chan. MODE=1, 56chn MODE=0
	AB_int       : 1,  // Input channel: 0=optical, 1=coaxial
	madi_lock    : 1,  // MADI input lock status
	_04          : 1,
	tco_lock     : 1,  // TCO lock status
	BUF_PTR      : 10,

	sync_in_lock : 1,  // Sync In lock status
	sync_in_sync : 1,  // Sync In sync status
	madi_sync    : 1,  // MADI input sync status
	_19          : 1,
	_20          : 1,
	mIRQ3        : 1,  // MIDI 3 interrupt pending
	madi_freq    : 4,  // MADI input frequency class
	BUF_ID       : 1,
	tco_detect   : 1,  // TCO present
	tco_sync     : 1,  // TCO is in sync
	mIRQ2        : 1,  // MIDI 2 interrupt pending
	mIRQ0        : 1,
	mIRQ1        : 1;
};

/* AES */
struct hdspe_status0_reg_aes { __le32
	IRQ          : 1,
	tco_freq     : 4,  // TCO frequency class
	mIRQ2        : 1,  // MIDI 2 interrupt pending
	BUF_PTR      : 10,

	sync_ref     : 4,  // active AutoSync reference: 0=WCLK, 1=AES1,
                           // ... 8=AES8, 9=TCO, 10=SyncIn, other=None
	wc_sync      : 1,  // word clock sync status
	wc_lock      : 1,  // word clock lock status
	wc_freq      : 4,  // word clock frequency class
	BUF_ID       : 1,
	tco_detect   : 1,  // TCO present
	tco_sync     : 1,  // TCO sync status
	tco_lock     : 1,  // TCO lock status
	mIRQ0        : 1,
	mIRQ1        : 1;
};

/* RayDAT / AIO / AIO Pro */
struct hdspe_status0_reg_raio { __le32
	IRQ          : 1,
	_01          : 1,
	_02          : 1,
	_03          : 1,
	_04          : 1,
	_05          : 1,
	BUF_PTR      : 10,

	_16          : 1,
	_17          : 1,
	_18          : 1,
	_19          : 1,
	_20          : 1,
	_21          : 1,
	_22          : 1,
	_23          : 1,

	_24          : 1,
	_25          : 1,
	BUF_ID       : 1,
	_27          : 1,
	_28          : 1,
	mIRQ2        : 1,  // MIDI 2 interrupt pending (RayDAT TCO MIDI)
	mIRQ0        : 1,
	mIRQ1        : 1;
};

union hdspe_status0_reg {
	__le32                          raw;    // register value 
	struct hdspe_status0_reg_common common; // common bits
	struct hdspe_status0_reg_madi   madi;   // MADI bits
	struct hdspe_status0_reg_aes    aes;    // AES bits
	struct hdspe_status0_reg_raio   raio;   // RayDAT/AIO/AIO Pro bits
};


/**
 * RD_STATUS2 register (byte offset 192)
 *
 * Pos Mask     MADI               AES               RayDAT/AIO/AIO Pro
 *
 * 00        1                     spdif_lock7
 * 01        2                     spdif_lock6
 * 02        4                     spdif_lock5
 * 03        8  wc_lock            spdif_lock4
 * 04       10  wc_sync            spdif_lock3
 * 05       20  inp_freq0          spdif_lock2
 * 06       40  inp_freq1          spdif_lock1       s2_tco_detect
 * 07       80  inp_freq2          spdif_lock0       s2_AEBO_D
 * 08      100  SelSyncRef0        spdif_sync7       s2_AEBI_D
 * 09      200  SelSyncRef1        spdif_sync6
 * 10      400  SelSyncRef2        spdif_sync5       s2_sync_in_lock
 * 11      800  inp_freq3          spdif_sync4       s2_sync_in_sync
 * 12     1000                     spdif_sync3       s2_sync_in_freq0
 * 13     2000                     spdif_sync2       s2_sync_in_freq1
 * 14     4000                     spdif_sync1       s2_sync_in_freq2
 * 15     8000                     spdif_sync0       s2_sync_in_freq3
 * 16    10000                     aes_mode0
 * 17    20000                     aes_mode1
 * 18    40000                     aes_mode2
 * 19    80000                     aes_mode3
 * 20   100000                     sync_in_lock
 * 21   200000                     sync_in_sync
 * 22   400000                     sync_in_freq0
 * 23   800000                     sync_in_freq1
 * 24  1000000                     sync_in_freq2
 * 25  2000000                     sync_in_freq3
 */

/* MADI */
struct hdspe_status2_reg_madi { __le32
	_00          : 1,
	_01          : 1,
	_02          : 1,
	wc_lock      : 1,  // Word clock lock status
	wc_sync      : 1,  // Word clock sync status
	inp_freq0    : 1,  // AutoSync frequency class (4 bits, scattered)
	inp_freq1    : 1,  // "
	inp_freq2    : 1,  // "

	sync_ref     : 3,  // Active AutoSync ref: 0=WCLK, 1=MADI, 2=TCO, 
	inp_freq3    : 1,  //                      3=Sync In, other=None
	_12          : 1,
	_13          : 1,
	_14          : 1,
	_15          : 1,

	__2          : 8,
	__3          : 8;
};

/* AES */
struct hdspe_status2_reg_aes { __le32
	lock         : 8,  // bit 0=AES8, 1=AES7 ... 7=AES1 lock status
	sync         : 8,  // bit 0=AES8, 1=AES7 ... 7=AES1 sync status
	aes_mode     : 4,
	sync_in_lock : 1,  // Sync In lock status
	sync_in_sync : 1,  // Sync In sync status
	sync_in_freq : 4,  // Sync In frequency class
	_26          : 1,
	_27          : 1,
	_28          : 1,
	_29          : 1,	
	_30          : 1,
	_31          : 1;
};

/* RayDAT / AIO / AIO Pro */
struct hdspe_status2_reg_raio { __le32
	_00          : 1,
	_01          : 1,
	_02          : 1,
	_03          : 1,
	_04          : 1,
	_05          : 1,
	tco_detect   : 1,  // TCO detected
	AEBO_D       : 1,  // Output Audio Extension Board NOT present

	AEBI_D       : 1,  // Input Audio Extension Board NOT present
	_09          : 1,
	sync_in_lock : 1,  // SyncIn lock status
	sync_in_sync : 1,  // SyncIn sync status
	sync_in_freq : 4,  // SyncIn frequency class

	__2          : 8,
	__3          : 8;
};

union hdspe_status2_reg {
	__le32                         raw;  // register value
	struct hdspe_status2_reg_madi  madi; // MADI fields
	struct hdspe_status2_reg_aes   aes;  // AES fields
	struct hdspe_status2_reg_raio  raio; // RayDAT / AIO / AIO Pro fields
};


/**
 * RD_STATUS1 register (byte offset 64) - RayDAT/AIO/AIO Pro only. 
 */

struct hdspe_status1_reg_raio { __le32
        lock      : 8,     // bit 0=AES, 1=SPDIF, 2..5=ADAT1..4 lock status
	
	sync      : 8,     // bit 0=AES, 1=SPDIF, 2..5=ADAT1..4 sync status

	wc_freq   : 4,     // word clock frequency class
	tco_freq  : 4,     // TCO frequency class

	wc_lock   : 1,     // word clock lock status
	wc_sync   : 1,     // word clock sync status
	tco_lock  : 1,     // TCO lock status
	tco_sync  : 1,     // TCO sync status
	sync_ref  : 4;     // Active AutoSync ref: 0=WCLK, 1=AES, 2=SPDIF
                           // 3=ADAT1, 4=ADAT2, 5=ADAT3, 6=ADAT4,
	                   // 9=TCO, 10=SyncIn, 15=Internal
};

union hdspe_status1_reg {
	__le32                         raw;  // register value
	struct hdspe_status1_reg_raio  raio; // RayDAT / AIO / AIO Pro fields
};


/**
 * RD_FBITS register (byte offset 192) contains audio input clock rate values
 * for AES and RayDAT/AIO/AIO Pro digital audio inputs.
 *
 * Bit mask  AES               RayDAT/AIO/AIO Pro
 *
 * 0000000f  AES1              AES
 * 000000f0  AES2              SPDIF
 * 00000f00  AES3              ADAT (AIO/AIO Pro), ADAT1 (RayDAT)
 * 0000f000  AES4              ADAT2 (RayDAT)
 * 000f0000  AES5              ADAT3 (RayDAT)
 * 00f00000  AES6              ADAT4 (RayDAT)
 * 0f000000  AES7
 * f0000000  AES8
 *
 * Frequency values: 1=32KHz, 2=44.1KHz, ... 9=192KHz, other=NoLock */

/* end of register definitions */
#pragma scalar_storage_order default


/**
 * Compose HSDPE_SYNC_STATUS from lock, sync and present bitfields in
 * the various status registers.
 */
#define HDSPE_MAKE_SYNC_STATUS(lock, sync, present) \
	(!(present) ? HDSPE_SYNC_STATUS_NOT_AVAILABLE	\
	 : !(lock) ? HDSPE_SYNC_STATUS_NO_LOCK		\
	 : !(sync) ? HDSPE_SYNC_STATUS_LOCK		\
	 : HDSPE_SYNC_STATUS_SYNC)

/**
 * Get frequency class from RD_FBITS register 
 */
#define HDSPE_FBITS_FREQ(reg, i) \
	(((reg) >> ((i)*4)) & 0xF)


/* --------------------------------------------------------- */

/* max. 4 MIDI ports per card */
#define HDSPE_MAX_MIDI 4

struct hdspe_midi {
	struct timer_list timer;
	spinlock_t lock;
	
	struct hdspe *hdspe;
	const char* portname;
	int id;
	int dataIn;
	int statusIn;
	int dataOut;
	int statusOut;
	int ie;
	int irq;

	struct snd_rawmidi *rmidi;
	struct snd_rawmidi_substream *input;
	struct snd_rawmidi_substream *output;

	int pending;            /* interrupt is pending */
	int istimer;		/* timer in use */	
};

//#define DEBUG_LTC
//#define DEBUG_MTC
struct hdspe_tco {
	spinlock_t lock;

	/* cached control registers */
	u32 reg[4];

	enum hdspe_tco_source input;
	enum hdspe_ltc_frame_rate ltc_fps;
	enum hdspe_bool ltc_drop;
	enum hdspe_tco_sample_rate sample_rate;
	enum hdspe_pull pull;
	enum hdspe_wck_conversion wck_conversion;
	enum hdspe_bool term;

	/* LTC out control */
	u32 ltc_out;             /* requested start LTC for output            */
	u64 ltc_out_frame_count; /* start LTC output at this frame count      */
	bool ltc_set;            /* time code set - need reset at next period */
	bool ltc_run;            /* time code output is running               */
	bool ltc_flywheel;       /* loop back time code output to input       */

	/* Current LTC in */
	bool ltc_changed;        /* set when new LTC has been received        */
	u32 ltc_in;              /* current LTC: last parsed LTC + 1 frame    */
	u64 ltc_time;            /* frame_count at start of current period    */
	u64 ltc_in_frame_count;  /* frame count at start of current LTC       */

	/* for status polling */
	struct hdspe_tco_status last_status;

	/* for measuring the actual LTC In fps and pull factor */
#define LTC_CACHE_SIZE 60
	u64 prev_ltc_time;        /* nanosecond timestamp of previous LTC irq */
	u64 ltc_duration_sum;          /* sum of observed LTC frame durations */
	u32 ltc_duration[LTC_CACHE_SIZE];     /* observed LTC frame durations */
	u32 ltc_count;                       /* number of received LTC frames */
	
	u32 ltc_in_pullfac;      /* actual LTC in pull factor - estimated     */
	u32 last_ltc_in_pullfac;             /* for change notification       */

#ifdef DEBUG_MTC
	u32 mtc;                                    /* current MIDI time code */
#endif /*DEBUG_MTC*/
};

/**
 * Card-dependent methods. Initialized by hdspe_init_[madi|aes|raio].
 */
struct hdspe_methods {
	void (*get_card_info)(struct hdspe* hdspe, struct hdspe_card_info* inf);
	void (*read_status)(struct hdspe* hdspe, struct hdspe_status* status);
	void (*set_float_format)(struct hdspe* hdspe, bool val);
	bool (*get_float_format)(struct hdspe* hdspe);
	void (*read_proc)(struct snd_info_entry*, struct snd_info_buffer*);
	enum hdspe_freq (*get_freq)(struct hdspe*, enum hdspe_clock_source);
	enum hdspe_freq (*get_external_freq)(struct hdspe*);
	enum hdspe_clock_source (*get_autosync_ref)(struct hdspe*);
	enum hdspe_clock_mode (*get_clock_mode)(struct hdspe*);
	void (*set_clock_mode)(struct hdspe*, enum hdspe_clock_mode);
	enum hdspe_clock_source (*get_pref_sync_ref)(struct hdspe*);
	void (*set_pref_sync_ref)(struct hdspe*, enum hdspe_clock_source);
	enum hdspe_sync_status (*get_sync_status)(struct hdspe*,
						  enum hdspe_clock_source);
	bool (*has_status_changed)(struct hdspe* hdspe);
};

/**
 * Card dependant tables. Initialized by hdspe_init_[madi|aes|raio].
 */
struct hdspe_tables {
	/* See hdspe_init_autosync_tables() */
	int autosync_count;
	const char* autosync_texts[HDSPE_CLOCK_SOURCE_COUNT];
	enum hdspe_clock_source autosync_idx2ref[HDSPE_CLOCK_SOURCE_COUNT];
	int autosync_ref2idx[HDSPE_CLOCK_SOURCE_COUNT];

	/* Initialized by hdspe_<model>_init() and assigned to
	 * hdspe::port_names_in, hdspe::port_names_out, hdspe::channel_map_in,
	 * hdspe_channel_map_out, hdspe::max_channels_in,
	 * hdspe::max_channels_out by hdspe_set_channel_map(). */
	const char * const *port_names_in_ss;
	const char * const *port_names_in_ds;
	const char * const *port_names_in_qs;
	const char * const *port_names_out_ss;
	const char * const *port_names_out_ds;
	const char * const *port_names_out_qs;

	const signed char *channel_map_in_ss, *channel_map_in_ds, *channel_map_in_qs;
	const signed char *channel_map_out_ss, *channel_map_out_ds, *channel_map_out_qs;
	
	unsigned char ss_in_channels;
	unsigned char ds_in_channels;
	unsigned char qs_in_channels;
	unsigned char ss_out_channels;
	unsigned char ds_out_channels;
	unsigned char qs_out_channels;

	const char * const *clock_source_names;

	/* For status change detection */
	__le32 status1, status1_mask;
	__le32 status2, status2_mask;
	u32 fbits;
};

/* status element ids for status change notification */
struct hdspe_ctl_ids {
	// TODO: there's probably a better way to query whether
	// we're running or changing buffer size, without control elements.
	struct snd_ctl_elem_id* running;
	struct snd_ctl_elem_id* buffer_size;
	
	struct snd_ctl_elem_id* status_polling;
	struct snd_ctl_elem_id* internal_freq;
	struct snd_ctl_elem_id* raw_sample_rate;
	struct snd_ctl_elem_id* dds;
	struct snd_ctl_elem_id* autosync_ref;
	struct snd_ctl_elem_id* external_freq;
	struct snd_ctl_elem_id* autosync_status;
	struct snd_ctl_elem_id* autosync_freq;

	/* TCO */
	struct snd_ctl_elem_id* ltc_in;
	struct snd_ctl_elem_id* ltc_valid;
	struct snd_ctl_elem_id* ltc_in_fps;
	struct snd_ctl_elem_id* ltc_in_drop;
	struct snd_ctl_elem_id* ltc_in_pullfac;
	struct snd_ctl_elem_id* video;
	struct snd_ctl_elem_id* wck_valid;
	struct snd_ctl_elem_id* wck_speed;
	struct snd_ctl_elem_id* tco_lock;
	struct snd_ctl_elem_id* ltc_run;
	struct snd_ctl_elem_id* ltc_jam_sync;
};

struct hdspe {
	struct pci_dev *pci;	     /* pci info */
	int vendor_id;               /* PCI vendor ID: Xilinx or RME */
	int dev;		     /* hardware vars... */
	int irq;
	unsigned long port;
	void __iomem *iobase;

	u16 firmware_rev;            /* determines io_type (card model) */
	u16 reserved;
	u32 fw_build;                /* firmware build */
	u32 serial;                  /* serial nr */

	enum hdspe_io_type io_type;  /* MADI, AES, RAYDAT, AIO or AIO_PRO */
	char *card_name;	     /* for procinfo */
	struct hdspe_methods m;      /* methods for the card model */
	struct hdspe_tables t;       /* tables for the card model */

	/* ALSA devices */
	struct snd_card *card;	     /* one card */
	struct snd_pcm *pcm;	     /* has one pcm */
	struct snd_hwdep *hwdep;     /* and a hwdep for additional ioctl */
  
	/* Only one playback and/or capture stream */
        struct snd_pcm_substream *capture_substream;
        struct snd_pcm_substream *playback_substream;

	/* MIDI */
	struct hdspe_midi midi[HDSPE_MAX_MIDI];
	struct work_struct midi_work;
	__le32 midiInterruptEnableMask;
	__le32 midiIRQPendingMask;
	int midiPorts;               /* number of MIDI ports */

	/* Check for status changes, status_polling times per second, if >0. 
	 * Status polling is disabled if 0.
	 * Initially, it is 0 and needs to be enabled by the client.
	 * hdspe_status_work() resets it to 0 when detecting a change,
	 * notifying the client with a "Status Polling"
	 * control notification event and notifications for all
	 * changed status control elements. 
	 * Status polling is also automatically disabled by the driver,
	 * with "Status Polling" notification, after 2 seconds without
	 * changes. The client must re-enable by setting "Status Polling"
	 * to a non-zero value to re-enable it. */
	int status_polling;
	struct work_struct status_work;
	unsigned long last_status_jiffies;
	unsigned long last_status_change_jiffies;
	struct hdspe_status last_status;
	struct hdspe_ctl_ids cid;   /* control ids to be notified */
	
	/* Mixer vars */
	/* full mixer accessible over mixer ioctl or hwdep-device */
	struct hdspe_mixer *mixer;
	struct hdspe_peak_rms peak_rms;
	/* fast alsa mixer */
	struct snd_kcontrol *playback_mixer_ctls[HDSPE_MAX_CHANNELS];
	/* but input to much, so not used */
	struct snd_kcontrol *input_mixer_ctls[HDSPE_MAX_CHANNELS];

	/* Optional Time Code Option module handle (NULL if absent) */
	struct hdspe_tco *tco;
#ifdef DEBUG_LTC
	struct timer_list tco_timer;
#endif /*DEBUG_LTC*/

	/* Channel map and port names - set by hdspe_set_channel_map() */
	unsigned char max_channels_in;
	unsigned char max_channels_out;
	const signed char *channel_map_in;
	const signed char *channel_map_out;
	const char * const *port_names_in;
	const char * const *port_names_out;

	unsigned char *playback_buffer;	/* suitably aligned address */
	unsigned char *capture_buffer;	/* suitably aligned address */

	pid_t capture_pid;	     /* process id which uses capture */
	pid_t playback_pid;	     /* process id which uses capture */
	int running;		     /* running status */

        spinlock_t lock;
	int irq_count;		     /* for debug */
/* #define TIME_INTERRUPT_INTERVAL */
#ifdef TIME_INTERRUPT_INTERVAL
	u64 last_interrupt_time;
#endif /*TIME_INTERRUPT_INTERVAL*/

	/* Register cache */
	struct reg {
		union hdspe_control_reg  control;
		union hdspe_settings_reg settings;
		__le32                   pll_freq;
		union hdspe_status0_reg  status0; /* read at every interrupt */
	} reg;

	u64 frame_count;            /* current period frame counter */
	u32 hw_pointer_wrap_count;  /* hw pointer wrapped this many times */
	u32 last_hw_pointer;        /* previous period hw pointer */
};


/**
 * Write/read to/from HDSPE with Adresses in Bytes
 * not words but only 32Bit writes are allowed.
 */

static inline __attribute__((always_inline))
void hdspe_write(struct hdspe * hdspe, u32 reg, __le32 val)
{
	writel(val, hdspe->iobase + reg);
}

static inline __attribute__((always_inline))
__le32 hdspe_read(struct hdspe * hdspe, u32 reg)
{
	return readl(hdspe->iobase + reg);
#ifdef FROM_WIN_DRIVER
	if (!deviceExtension->bShutdown) {
		value = READ_REGISTER_ULONG(deviceExtension->MemBase+offset);
		if (value == 0xFFFFFFFF && !(offset >= 256 && offset < 320)) { // surprise removal
			value = 0;
		}
#endif
}

static inline __attribute__((always_inline))
void hdspe_write_control(struct hdspe* hdspe)
{
	hdspe_write(hdspe, HDSPE_WR_CONTROL, hdspe->reg.control.raw);
}

static inline __attribute__((always_inline))
void hdspe_write_settings(struct hdspe* hdspe)
{
	hdspe_write(hdspe, HDSPE_WR_SETTINGS, hdspe->reg.settings.raw);
}

static inline __attribute__((always_inline))
void hdspe_write_pll_freq(struct hdspe* hdspe)
{
	hdspe_write(hdspe, HDSPE_WR_PLL_FREQ, hdspe->reg.pll_freq);
}

// status0 register is read at every interrupt if audio is started and
// audio interrupt enabled. We make hdspe_read_status() return the
// value of the status register at the time of the interrupt in that
// case, thus reducing the number of times the hardware is read to a
// minimum as well as reducing the need for locking.
static inline __attribute__((always_inline))
union hdspe_status0_reg hdspe_read_status0_nocache(struct hdspe* hdspe)
{
	union hdspe_status0_reg reg;
	reg.raw = hdspe_read(hdspe, HDSPE_RD_STATUS0);
	return reg;
}

static inline __attribute__((always_inline))
bool hdspe_is_running(struct hdspe* hdspe)
{
	return hdspe->reg.control.common.START &&
		hdspe->reg.control.common.IE_AUDIO;
}

static inline __attribute__((always_inline))
union hdspe_status0_reg hdspe_read_status0(struct hdspe* hdspe)
{
	return hdspe_is_running(hdspe) ? hdspe->reg.status0 :
		hdspe_read_status0_nocache(hdspe);
}

static inline __attribute__((always_inline))
union hdspe_status1_reg hdspe_read_status1(struct hdspe* hdspe)
{
	union hdspe_status1_reg reg;
	reg.raw = hdspe_read(hdspe, HDSPE_RD_STATUS1);
	return reg;
}

static inline __attribute__((always_inline))
union hdspe_status2_reg hdspe_read_status2(struct hdspe* hdspe)
{
	union hdspe_status2_reg reg;
	reg.raw = hdspe_read(hdspe, HDSPE_RD_STATUS2);
	return reg;
}

static inline __attribute__((always_inline))
u32 hdspe_read_fbits(struct hdspe* hdspe)
{
	return le32_to_cpu(hdspe_read(hdspe, HDSPE_RD_FBITS));
}

static inline __attribute__((always_inline))
u32 hdspe_read_pll_freq(struct hdspe* hdspe)
{
	return le32_to_cpu(hdspe_read(hdspe, HDSPE_RD_PLL_FREQ));
}


/**
 * hdspe_pcm.c
 */
extern int snd_hdspe_create_pcm(struct snd_card *card,
				struct hdspe *hdspe);

/* Current period size in samples. */
extern u32 hdspe_period_size(struct hdspe *hdspe);

/* Get current hardware frame counter. Wraps every 16K frames. */
extern snd_pcm_uframes_t hdspe_hw_pointer(struct hdspe *hdspe);

/* Called right from the interrupt handler in order to update 
 * hdspe->frame_count.
 * In absence of xruns, the frame counter increments by
 * hdspe_period_size() frames each period. This routine will correctly
 * determine the frame counter even in the presence of xruns or late
 * interrupt handling, as long as the hardware pointer did not wrap more 
 * than once since the previous invocation. */
extern void hdspe_update_frame_count(struct hdspe* hdspe);

/**
 * hdspe_midi.c
 */
extern void hdspe_init_midi(struct hdspe* hdspe,
			    int count, struct hdspe_midi *list);

extern void hdspe_terminate_midi(struct hdspe* hdspe);

extern int snd_hdspe_create_midi(struct snd_card *card,
				 struct hdspe *hdspe, int id);

#ifdef OLDSTUFF
extern void snd_hdspe_flush_midi_input(struct hdspe *hdspe, int id);
#endif /*OLDSTUFF*/

extern void hdspe_midi_work(struct work_struct *work);

/**
 * hdspe_hwdep.c
 */
extern int snd_hdspe_create_hwdep(struct snd_card *card,
				  struct hdspe *hdspe);

extern void hdspe_get_card_info(struct hdspe* hdspe, struct hdspe_card_info *s);

/**
 * hdspe_proc.c
 */
extern void snd_hdspe_proc_init(struct hdspe *hdspe);

/* Read hdspe_status from hardware and prints properties common to all 
 * HDSPe cards. */
extern void hdspe_proc_read_common(struct snd_info_buffer *buffer,
				   struct hdspe* hdspe,
				   struct hdspe_status* s);

/* Prints the fields of the FBITS register  */
extern void hdspe_iprint_fbits(struct snd_info_buffer *buffer,
			       const char* name, u32 fbits);

/* Prints the bits that are on in a register. */
extern void hdspe_iprintf_reg(struct snd_info_buffer *buffer,
			      const char* name, __le32 reg,
			      const char* const *bitNames);

#ifdef CONFIG_SND_DEBUG
#define IPRINTREG(buffer, name, reg, bitNames) \
	hdspe_iprintf_reg(buffer, name, reg, bitNames);
#else /* not CONFIG_SND_DEBUG */
#define IPRINTREG(buffer, name, reg, bitNames) \
	hdspe_iprintf_reg(buffer, name, reg, 0);
#endif /* CONFIG_SND_DEBUG */

/**
 * hdspe_control.c
 */
extern void hdspe_init_autosync_tables(struct hdspe* hdspe,
				       int nr_autosync_opts,
				       enum hdspe_clock_source* autosync_opts);

extern int snd_hdspe_create_controls(struct snd_card *card,
				     struct hdspe *hdspe);

extern int hdspe_add_controls(struct hdspe *hdspe,
			      int count, const struct snd_kcontrol_new *list);

extern struct snd_kcontrol* hdspe_add_control(struct hdspe* hdspe,
				const struct snd_kcontrol_new* newctl);

/* Same, filling in control element snd_ctl_elem_id* in ctl_id */
extern int hdspe_add_control_id(struct hdspe* hdspe,
				const struct snd_kcontrol_new* nctl,
				struct snd_ctl_elem_id** ctl_id);

extern void hdspe_status_work(struct work_struct* work);

#define HDSPE_CTL_NOTIFY(prop)					\
	snd_ctl_notify(hdspe->card, SNDRV_CTL_EVENT_MASK_VALUE, \
		       hdspe->cid.prop);

/**
 * hdspe_mixer.c
 */
extern int hdspe_init_mixer(struct hdspe* hdspe);

extern void hdspe_terminate_mixer(struct hdspe* hdspe);

extern int hdspe_create_mixer_controls(struct hdspe* hdspe);

#ifdef OLDSTUFF
extern int hdspe_update_simple_mixer_controls(struct hdspe * hdspe);
#endif /*OLDSTUFF*/

/**
 * hdspe_tco.c
 */
extern int hdspe_init_tco(struct hdspe* hdspe);

extern void hdspe_terminate_tco(struct hdspe* hdspe);

extern int hdspe_create_tco_controls(struct hdspe* hdspe);

extern void hdspe_tco_read_status(struct hdspe* hdspe,
				  struct hdspe_tco_status* status);

extern void snd_hdspe_proc_read_tco(struct snd_info_entry *entry,
				    struct snd_info_buffer *buffer);

/* Called from the MIDI input handler, whenever an MTC message comes in */
extern void hdspe_tco_mtc(struct hdspe* hdspe,
			  const u8* data, int count);

/* Scheduled from the audio interrupt handler */
extern void hdspe_tco_period_elapsed(struct hdspe* hdspe);
	
/* TCO module status polling */
extern bool hdspe_tco_notify_status_change(struct hdspe* hdspe);

/* Set "app" sample rate on TCO module, when sound card sample rate changes. */
extern void hdspe_tco_set_app_sample_rate(struct hdspe* hdspe);

/**
 * hdspe_common.c
 */

/* Get name of the clock source */
extern const char* const hdspe_clock_source_name(struct hdspe* hdspe, int i);
/* Get name of the frequency class */
extern const char* const hdspe_freq_name(enum hdspe_freq f);
/* Get frequency class frame rate */
extern u32 hdspe_freq_sample_rate(enum hdspe_freq f);
/* Get the frequency class best representing the given rate */
enum hdspe_freq hdspe_sample_rate_freq(u32 rate);

/* Sets the channel map and port names according the indicated speed mode. */
extern void hdspe_set_channel_map(struct hdspe* hdspe, enum hdspe_speed speed);

/* Get the current internal frequency class (single speed
 * frequency and speed mode control register bits encoded into
 * a single frequency class value. */
extern enum hdspe_freq hdspe_internal_freq(struct hdspe* hdspe);

/* Write the internal frequency (single speed frequency and speed
 * mode control register bits). No checks, no channel map update. 
 * Returns 1 if changed and 0 if not. */
extern int hdspe_write_internal_freq(struct hdspe* hdspe, enum hdspe_freq f);

/* Get the current speed mode */
extern enum hdspe_speed hdspe_speed_mode(struct hdspe* hdspe);
/* Get the current speed factor: 1, 2 or 4 */
extern int hdspe_speed_factor(struct hdspe* hdspe);

/* Convert frequency class <f> to speed mode <speed_mode> */
extern enum hdspe_freq hdspe_speed_adapt(enum hdspe_freq f,
					 enum hdspe_speed speed_mode);

/* Get WR_PLL_FREQ (DDS) register value valid range (controls internal sampe 
 * rate in the range 27000 ... 207000/4 Hz) */
extern void hdspe_dds_range(struct hdspe* hdspe, u32* ddsmin, u32* ddsmax);

/* Get DDS register value */
extern u32 hdspe_get_dds(struct hdspe*);

/* Check and write DDS register value. Return -EINVAL if dds is out
 * of range, 0 if no change, 1 if change. */ 
extern int hdspe_write_dds(struct hdspe*, u32 dds);

/*Writes the desired internal clock pitch to the WR_PLL_FREQ.
 * Pitch is in parts per milion relative to the current control registers
 * single speed frequency setting. If ppm is 1000000, the cards internal
 * clock will run at exactly the internal frequency set in the control 
 * register. If pitch < 1000000, the card will run slower. If > 1000000 it 
 * will run faster. Returns 1 if changed and 0 if not. */
extern int hdspe_write_internal_pitch(struct hdspe* hdspe, int ppm);

/* Return the cached value of the internal pitch. */
extern u32 hdspe_internal_pitch(struct hdspe* hdspe);

/* Reads effective system pitch from the RD_PLL_FREQ register, converting
 * to parts per milion relative to the controls registers single speed
 * frequency setting. */
extern u32 hdspe_read_system_pitch(struct hdspe* hdspe);

/* Read effective system sample rate from hardware. This may differ 
 * from 32KHz, 44.1KHz, etc... because of DDS (non-neutral system pitch). */
extern u32 hdspe_read_system_sample_rate(struct hdspe* hdspe);

/* Read current system sample rate from hardware - read_status() helper. */
extern void hdspe_read_sample_rate_status(struct hdspe* hdspe,
					  struct hdspe_status* status);

/* Set arbitray sample rate in the cards range, writing the control
 * register single speed frequency closest to the desired rate,
 * the speed mode corresponding with the desired rate, and the pll_freq
 * register. Forbids speed mode change if there are processes capturing or 
 * playing back. Sets the channel map according to the desired speed mode 
 * if allowed and rate differs from current.
 * Returns:
 * -EBUSY : forbidden speed mode change.
 * 0 : desired rate is same as current.
 * 1 : new rate set. */
extern int hdspe_set_sample_rate(struct hdspe * hdspe, u32 desired_rate);

/* Check if same process is writing and reading. */
static inline __attribute__((always_inline))
int snd_hdspe_use_is_exclusive(struct hdspe *hdspe)
{
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&hdspe->lock, flags);
	if ((hdspe->playback_pid != hdspe->capture_pid) &&
	    (hdspe->playback_pid >= 0) && (hdspe->capture_pid >= 0)) {
		ret = 0;
	}
	spin_unlock_irqrestore(&hdspe->lock, flags);
	return ret;
}

/* read_status() helper. */
static inline __attribute__((always_inline))
void hdspe_set_sync_source(struct hdspe_status* status,
			   int i,
			   enum hdspe_freq freq,
			   bool lock, bool sync, bool present)
{
	status->freq[i] = freq;
	status->sync[i] = HDSPE_MAKE_SYNC_STATUS(lock, sync, present);
}

/**
 * hdspe_madi.c
 */
extern int hdspe_init_madi(struct hdspe* hdspe);

extern void hdspe_terminate_madi(struct hdspe* hdspe);

/**
 * hdspe_aes.c
 */
extern int hdspe_init_aes(struct hdspe* hdspe);

extern void hdspe_terminate_aes(struct hdspe* hdspe);

/**
 * hdspe_raio.c
 */
extern int hdspe_init_raio(struct hdspe* hdspe);

extern void hdspe_terminate_raio(struct hdspe* hdspe);

#endif /* __SOUND_HDSPE_CORE_H */
