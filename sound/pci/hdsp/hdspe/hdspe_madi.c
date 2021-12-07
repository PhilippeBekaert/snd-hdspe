// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe_madi.c
 * @brief RME HDSPe MADI driver methods.
 *
 * 20210728,1207 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORS,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

#include "hdspe.h"
#include "hdspe_core.h"

/* Map MADI WR_CONTROL / RD_STATUS2 sync ref 3-bit code to hdspe_autosync_ref 
 * enum */
static enum hdspe_clock_source madi_autosync_ref[8] = {
	HDSPE_CLOCK_SOURCE_WORD,
	HDSPE_CLOCK_SOURCE_MADI,
	HDSPE_CLOCK_SOURCE_TCO,
	HDSPE_CLOCK_SOURCE_SYNC_IN,
	HDSPE_CLOCK_SOURCE_INTERN,    // unused codes
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN     // master clock mode
};

const char* const hdspe_madi_clock_source_names[HDSPE_CLOCK_SOURCE_COUNT] = {
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  0),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  1),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  2),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  3),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  4),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  5),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  6),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  7),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  8),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI,  9),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI, 10),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI, 11),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI, 12),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI, 13),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI, 14),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI, 15),
};

/* Number of channels for different Speed Modes */
#define MADI_SS_CHANNELS       64
#define MADI_DS_CHANNELS       32
#define MADI_QS_CHANNELS       16

/* port names */
static const char * const texts_ports_madi[] = {
	"MADI.1", "MADI.2", "MADI.3", "MADI.4", "MADI.5", "MADI.6",
	"MADI.7", "MADI.8", "MADI.9", "MADI.10", "MADI.11", "MADI.12",
	"MADI.13", "MADI.14", "MADI.15", "MADI.16", "MADI.17", "MADI.18",
	"MADI.19", "MADI.20", "MADI.21", "MADI.22", "MADI.23", "MADI.24",
	"MADI.25", "MADI.26", "MADI.27", "MADI.28", "MADI.29", "MADI.30",
	"MADI.31", "MADI.32", "MADI.33", "MADI.34", "MADI.35", "MADI.36",
	"MADI.37", "MADI.38", "MADI.39", "MADI.40", "MADI.41", "MADI.42",
	"MADI.43", "MADI.44", "MADI.45", "MADI.46", "MADI.47", "MADI.48",
	"MADI.49", "MADI.50", "MADI.51", "MADI.52", "MADI.53", "MADI.54",
	"MADI.55", "MADI.56", "MADI.57", "MADI.58", "MADI.59", "MADI.60",
	"MADI.61", "MADI.62", "MADI.63", "MADI.64",
};

/* These tables map the ALSA channels 1..N to the channels that we
   need to use in order to find the relevant channel buffer. RME
   refers to this kind of mapping as between "the ADAT channel and
   the DMA channel." We index it using the logical audio channel,
   and the value is the DMA channel (i.e. channel buffer number)
   where the data for that channel can be read/written from/to.
*/

static const char channel_map_unity_ss[HDSPE_MAX_CHANNELS] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63
};

#ifdef CONFIG_SND_DEBUG
/* WR_CONTROL register bit names for MADI card */
static const char* const madi_control_bitNames[32] = {
	"START",
	"LAT_0",
	"LAT_1",
	"LAT_2",
	"Master",
	"IE_AUDIO",
	"freq0",
	"freq1",
	"freq2",
	"?09",
	"tx_64ch",
	"AutoInp",
	"opt_out",
	"SyncRef2",
	"inp_0",
	"inp_1",
	"SyncRef0",
	"SyncRef1",
	"SMUX",
	"CLR_TMS",
	"WCK48",
	"IEN2",
	"IEN0",
	"IEN1",
	"LineOut",
	"HDSPe_float_format",
	"IEN3",
	"?27",
	"?28",
	"?29",
	"?30",
	"freq3"
};

/* RD_STATUS2 register bit names for MADI */
static const char* const madi_status2_bitNames[32] = {
	"?00",
	"?01",
	"?02",
	"wc_lock",	
	"wc_sync",
	"inp_freq0",
	"inp_freq1",
	"inp_freq2",
	"SelSyncRef0",
	"SelSyncRef1",
	"SelSyncRef2",
	"inp_freq3",	
	"?12",
	"?13",
	"?14",
	"?15",	
	"?16",
	"?17",
	"?18",
	"?19",	
	"?20",
	"?21",
	"?22",
	"?23",	
	"?24",
	"?25",
	"?26",
	"?27",	
	"?28",
	"?29",
	"?30",
	"?31"
};
#endif /*CONFIG_SND_DEBUG*/

static void hdspe_madi_read_status(struct hdspe* hdspe,
	struct hdspe_status* status)
{
	int i, inp_freq;
	struct hdspe_control_reg_madi control = hdspe->reg.control.madi;
	struct hdspe_status0_reg_madi status0 = hdspe_read_status0(hdspe).madi;
	struct hdspe_status2_reg_madi status2 = hdspe_read_status2(hdspe).madi;

	status->version = HDSPE_VERSION;

	hdspe_read_sample_rate_status(hdspe, status);

	status->clock_mode = control.Master;
	status->internal_freq = hdspe_internal_freq(hdspe);
	status->speed_mode = hdspe_speed_mode(hdspe);
	status->preferred_ref =	madi_autosync_ref[control.SyncRef];
	status->autosync_ref = 	madi_autosync_ref[status2.sync_ref];

	for (i = 0; i < HDSPE_CLOCK_SOURCE_COUNT; i++) {
		hdspe_set_sync_source(status, i,
				      0, false, false, false);
	}

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_WORD,
		0,  /* MADI doesn't report word clock frequency class */
		status2.wc_lock,
		status2.wc_sync, true);

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_MADI,
		status0.madi_freq,
		status0.madi_lock,
		status0.madi_sync, true);

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_TCO,
		0,   /* MADI doesn't report TCO frequency class */
		status0.tco_lock,
		status0.tco_sync,
		status0.tco_detect);

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_SYNC_IN,
		0,   /* MADI doesn't report SyncIn frequency class */
		status0.sync_in_lock,
		status0.sync_in_sync, true);

	/* MADI reports only the frequency class of the active AutoSync ref. */
	inp_freq = (status2.inp_freq3 << 3) |
		(status2.inp_freq2 << 2) |
		(status2.inp_freq1 << 1) |
		(status2.inp_freq0 << 0);
	status->external_freq = hdspe_speed_adapt(inp_freq, status->speed_mode);

	status->wck48 = control.WCK48;
	status->clr_tms = control.CLR_TMS;

	// MADI specific settings
	status->madi.input_select = control.inp_0;
	status->madi.auto_select = control.AutoInp;
	status->madi.tx_64ch = control.tx_64ch;
	status->madi.smux = control.SMUX;

	// MADI specific status
	status->madi.input_source = status0.AB_int;
	status->madi.rx_64ch = status0.rx_64ch;
}

/* set 32-bit float sample format if val is true, s32le format if false */
static void hdspe_madi_set_float_format(struct hdspe* hdspe, bool val)
{
	hdspe->reg.control.madi.FloatFmt = val;
	hdspe_write_control(hdspe);
}

static bool hdspe_madi_get_float_format(struct hdspe* hdspe)
{
	return hdspe->reg.control.madi.FloatFmt;
}

static enum hdspe_clock_mode hdspe_madi_get_clock_mode(struct hdspe *hdspe)
{
	return hdspe->reg.control.madi.Master;
}

static void hdspe_madi_set_clock_mode(struct hdspe* hdspe,
					  enum hdspe_clock_mode master)
{
	hdspe->reg.control.madi.Master = master;
	hdspe_write_control(hdspe);
}

static enum hdspe_clock_source hdspe_madi_get_preferred_sync_ref(
	struct hdspe* hdspe)
{
	return madi_autosync_ref[hdspe->reg.control.madi.SyncRef];
}

static void hdspe_madi_set_preferred_sync_ref(struct hdspe* hdspe,
					      enum hdspe_clock_source ref)
{
	int madi_syncref_value[HDSPE_CLOCK_SOURCE_COUNT] = {
		0,  // WCLK
		1,  // MADI
		0, 0, 0, 0, 0, 0, 0,   // invalid, map to WCLK (default)
		2,  // TCO
		3,  // SyncIn
		0, 0, 0, 0, 0          // invalid, map to WCLK (default)
	};
	hdspe->reg.control.madi.SyncRef = madi_syncref_value[ref];
	hdspe_write_control(hdspe);
}

static enum hdspe_clock_source hdspe_madi_get_autosync_ref(struct hdspe* hdspe)
{
	return madi_autosync_ref[hdspe_read_status2(hdspe).madi.sync_ref];
}

enum hdspe_freq hdspe_madi_get_external_freq(struct hdspe* hdspe)
{
	struct hdspe_status2_reg_madi status2 = hdspe_read_status2(hdspe).madi;
	enum hdspe_freq inp_freq = (status2.inp_freq3 << 3) |
		(status2.inp_freq2 << 2) |
		(status2.inp_freq1 << 1) |
		(status2.inp_freq0 << 0);
	return hdspe_speed_adapt(inp_freq, hdspe_speed_mode(hdspe));
}

static bool hdspe_madi_check_status_change(struct hdspe* hdspe,
					   struct hdspe_status* o,
					   struct hdspe_status* n)
{
	bool changed = false;

	if (n->external_freq != o->external_freq && hdspe->cid.external_freq) {
		dev_dbg(hdspe->card->dev, "external freq changed %d -> %d.\n",
			o->external_freq, n->external_freq);		
		HDSPE_CTL_NOTIFY(external_freq);
		changed = true;
	}

	if (n->madi.input_source != o->madi.input_source) {
		dev_dbg(hdspe->card->dev, "input source changed %d -> %d\n",
			o->madi.input_source, n->madi.input_source);
		HDSPE_CTL_NOTIFY(madi_input_source);
		changed = true;
	}

	if (n->madi.rx_64ch != o->madi.rx_64ch) {
		dev_dbg(hdspe->card->dev, "rx_64ch changed %d -> %d\n",
			o->madi.rx_64ch, n->madi.rx_64ch);
		HDSPE_CTL_NOTIFY(madi_rx_64ch);
		changed = true;
	}
	
	return changed;
}

static void hdspe_madi_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct hdspe *hdspe = entry->private_data;
	struct hdspe_status s;

	hdspe_proc_read_common(buffer, hdspe, &s);

	snd_iprintf(buffer, "Preferred input\t\t: %d %s\n", s.madi.input_select,
		    HDSPE_MADI_INPUT_NAME(s.madi.input_select));
	snd_iprintf(buffer, "Auto input\t\t: %d %s\n", s.madi.auto_select,
		    HDSPE_BOOL_NAME(s.madi.auto_select));
	snd_iprintf(buffer, "Current input\t\t: %d %s\n", s.madi.input_source,
		    HDSPE_MADI_INPUT_NAME(s.madi.input_source));
	snd_iprintf(buffer, "Tx 64Ch\t\t\t: %d %s\n", s.madi.tx_64ch,
		    HDSPE_BOOL_NAME(s.madi.tx_64ch));
	snd_iprintf(buffer, "Rx 64Ch\t\t\t: %d %s\n", s.madi.rx_64ch,
		    HDSPE_BOOL_NAME(s.madi.rx_64ch));
	snd_iprintf(buffer, "S/Mux\t\t\t: %d %s\n", s.madi.smux,
		    HDSPE_BOOL_NAME(s.madi.smux));

	snd_iprintf(buffer, "\n");
	IPRINTREG(buffer, "CONTROL", hdspe->reg.control.raw,
		  madi_control_bitNames);
	{
		union hdspe_status2_reg status2 = hdspe_read_status2(hdspe);
		u32 fbits = hdspe_read_fbits(hdspe);
		IPRINTREG(buffer, "STATUS2", status2.raw,
			  madi_status2_bitNames);
		hdspe_iprint_fbits(buffer, "FBITS", fbits);
	}

	hdspe_proc_read_common2(buffer, hdspe, &s);	

#ifdef OLDSTUFF
	unsigned int status, status2;

	int prefsync;
	char *autosync_ref;
	char *system_clock_mode;
	int x, x2;

	status = hdspe_read(hdspe, HDSPE_RD_STATUS0);
	status2 = hdspe_read(hdspe, HDSPE_RD_STATUS2);

	snd_iprintf(buffer, "%s (Card #%d) Rev.%x Status2first3bits: %x\n",
			hdspe->card_name, hdspe->card->number + 1,
			hdspe->firmware_rev,
			(status2 & HDSPE_version0) |
			(status2 & HDSPE_version1) | (status2 &
				HDSPE_version2));

	snd_iprintf(buffer, "HW Serial: 0x%06x%06x\n",
			(hdspe_read(hdspe, HDSPE_midiStatusIn1)>>8) & 0xFFFFFF,
			hdspe->serial);

	snd_iprintf(buffer, "IRQ: %d Registers bus: 0x%lx VM: 0x%lx\n",
			hdspe->irq, hdspe->port, (unsigned long)hdspe->iobase);

	snd_iprintf(buffer, "--- System ---\n");

	snd_iprintf(buffer,
		"IRQ Pending: Audio=%d, MIDI0=%d, MIDI1=%d, IRQcount=%d\n",
		status & HDSPE_audioIRQPending,
		(status & HDSPE_midi0IRQPending) ? 1 : 0,
		(status & HDSPE_midi1IRQPending) ? 1 : 0,
		hdspe->irq_count);
	snd_iprintf(buffer,
		"HW pointer: id = %d, rawptr = %d (%d->%d) "
		"estimated= %ld (bytes)\n",
		((status & HDSPE_BufferID) ? 1 : 0),
		(status & HDSPE_BufferPositionMask),
		(status & HDSPE_BufferPositionMask) %
		(2 * (int)hdspe->period_bytes),
		((status & HDSPE_BufferPositionMask) - 64) %
		(2 * (int)hdspe->period_bytes),
		(long) hdspe_hw_pointer(hdspe) * 4);

	snd_iprintf(buffer,
		"MIDI FIFO: Out1=0x%x, Out2=0x%x, In1=0x%x, In2=0x%x \n",
		hdspe_read(hdspe, HDSPE_midiStatusOut0) & 0xFF,
		hdspe_read(hdspe, HDSPE_midiStatusOut1) & 0xFF,
		hdspe_read(hdspe, HDSPE_midiStatusIn0) & 0xFF,
		hdspe_read(hdspe, HDSPE_midiStatusIn1) & 0xFF);
	snd_iprintf(buffer,
		"MIDIoverMADI FIFO: In=0x%x, Out=0x%x \n",
		hdspe_read(hdspe, HDSPE_midiStatusIn2) & 0xFF,
		hdspe_read(hdspe, HDSPE_midiStatusOut2) & 0xFF);
	snd_iprintf(buffer,
		"Register: ctrl1=0x%x, status0=0x%x, "
		"status2=0x%x\n",
		hdspe->reg.control.raw,
		status, status2);


	snd_iprintf(buffer, "--- Settings ---\n");

	x = hdspe_period_size(hdspe);

	snd_iprintf(buffer,
		"Size (Latency): %d samples (2 periods of %lu bytes)\n",
		x, (unsigned long) hdspe->period_bytes);

	snd_iprintf(buffer, "Line out: %s\n",
		(hdspe->reg.control.raw & HDSPE_LineOut) ? "on " : "off");

	snd_iprintf(buffer,
		"ClearTrackMarker = %s, Transmit in %s Channel Mode, "
		"Auto Input %s\n",
		(hdspe->reg.control.raw & HDSPE_clr_tms) ? "on" : "off",
		(hdspe->reg.control.raw & HDSPE_TX_64ch) ? "64" : "56",
		(hdspe->reg.control.raw & HDSPE_AutoInp) ? "on" : "off");


	if (!(hdspe->reg.control.raw & HDSPE_ClockModeMaster))
		system_clock_mode = "AutoSync";
	else
		system_clock_mode = "Master";
	snd_iprintf(buffer, "AutoSync Reference: %s\n", system_clock_mode);

	prefsync = hdspe_madi_get_preferred_sync_ref(hdspe);
	snd_iprintf(buffer, "Preferred Sync Reference: %s\n",
		    HDSPE_CLOCK_SOURCE_NAME(HDSPE_MADI, prefsync));

	snd_iprintf(buffer, "System Clock Frequency: %d\n",
			hdspe->system_sample_rate);


	snd_iprintf(buffer, "--- Status:\n");

	x = status & HDSPE_madiSync;
	x2 = status2 & HDSPE_wcSync;

	snd_iprintf(buffer, "Inputs MADI=%s, WordClock=%s\n",
			(status & HDSPE_madiLock) ? (x ? "Sync" : "Lock") :
			"NoLock",
			(status2 & HDSPE_wcLock) ? (x2 ? "Sync" : "Lock") :
			"NoLock");

	switch (hdspe_madi_get_autosync_ref(hdspe)) {
	case HDSPE_CLOCK_SOURCE_SYNC_IN:
		autosync_ref = "Sync In";
		break;
	case HDSPE_CLOCK_SOURCE_TCO:
		autosync_ref = "TCO";
		break;
	case HDSPE_CLOCK_SOURCE_WORD:
		autosync_ref = "Word Clock";
		break;
	case HDSPE_CLOCK_SOURCE_MADI:
		autosync_ref = "MADI Sync";
		break;
	case HDSPE_CLOCK_SOURCE_INTERN:
		autosync_ref = "Input not valid";
		break;
	default:
		autosync_ref = "---";
		break;
	}
	snd_iprintf(buffer,
		"AutoSync: Reference= %s, Freq=%d (MADI = %d, Word = %d)\n",
		autosync_ref, hdspe_freq_sample_rate(hdspe_autosync_freq(hdspe)),
		HDSPE_MADI_freq(status),
		HDSPE_MADI_inp_freq(status2));

	snd_iprintf(buffer, "Input: %s, Mode=%s\n",
		(status & HDSPE_AB_int) ? "Coax" : "Optical",
		(status & HDSPE_RX_64ch) ? "64 channels" :
		"56 channels");

	snd_iprintf(buffer, "\n");
#endif /*OLDSTUFF*/	
}

static const struct hdspe_methods hdspe_madi_methods = {
	.get_card_info = hdspe_get_card_info,
	.read_status = hdspe_madi_read_status,
	.get_float_format = hdspe_madi_get_float_format,
	.set_float_format = hdspe_madi_set_float_format,
	.read_proc = hdspe_madi_proc_read,
	.get_autosync_ref = hdspe_madi_get_autosync_ref,
	.get_clock_mode = hdspe_madi_get_clock_mode,
	.set_clock_mode = hdspe_madi_set_clock_mode,
	.get_pref_sync_ref = hdspe_madi_get_preferred_sync_ref,
	.set_pref_sync_ref = hdspe_madi_set_preferred_sync_ref,
	.check_status_change = hdspe_madi_check_status_change
};

static const struct hdspe_tables hdspe_madi_tables = {
	.ss_in_channels = MADI_SS_CHANNELS,
	.ss_out_channels = MADI_SS_CHANNELS,
	.ds_in_channels = MADI_DS_CHANNELS,
	.ds_out_channels = MADI_DS_CHANNELS,
	.qs_in_channels = MADI_QS_CHANNELS,
	.qs_out_channels = MADI_QS_CHANNELS,
			
	.channel_map_in_ss = channel_map_unity_ss,
	.channel_map_out_ss = channel_map_unity_ss,
	.channel_map_in_ds = channel_map_unity_ss,
	.channel_map_out_ds = channel_map_unity_ss,
	.channel_map_in_qs = channel_map_unity_ss,
	.channel_map_out_qs = channel_map_unity_ss,
		
	.port_names_in_ss = texts_ports_madi,
	.port_names_out_ss = texts_ports_madi,
	.port_names_in_ds = texts_ports_madi,
	.port_names_out_ds = texts_ports_madi,		
	.port_names_in_qs = texts_ports_madi,
	.port_names_out_qs = texts_ports_madi,

	.clock_source_names = hdspe_madi_clock_source_names
};

static struct hdspe_midi hdspe_madi_midi_ports[4] = {
	{.portname = "MIDIoverMADI 1",
	 .dataIn = HDSPE_midiDataIn0,
	 .statusIn = HDSPE_midiStatusIn0,
	 .dataOut = HDSPE_midiDataOut0,
	 .statusOut = HDSPE_midiStatusOut0,
	 .ie = HDSPE_Midi0InterruptEnable,
	 .irq = HDSPE_midi0IRQPending,
	},
	{.portname = "MIDIoverMADI 2",
	 .dataIn = HDSPE_midiDataIn1,
	 .statusIn = HDSPE_midiStatusIn1,
	 .dataOut = HDSPE_midiDataOut1,
	 .statusOut = HDSPE_midiStatusOut1,
	 .ie = HDSPE_Midi1InterruptEnable,
	 .irq = HDSPE_midi1IRQPending,
	},
	{.portname = "MIDIoverMADI 3",
	 .dataIn = HDSPE_midiDataIn2,
	 .statusIn = HDSPE_midiStatusIn2,
	 .dataOut = HDSPE_midiDataOut2,
	 .statusOut = HDSPE_midiStatusOut2,
	 .ie = HDSPE_Midi2InterruptEnable,
	 .irq = HDSPE_midi2IRQPending,
	},
	{.portname = "MTC",
	 .dataIn = HDSPE_midiDataIn3,
	 .statusIn = HDSPE_midiStatusIn3,
	 .dataOut = -1,
	 .statusOut = -1,
	 .ie = HDSPE_Midi3InterruptEnable,
	 .irq = HDSPE_midi3IRQPending,
	}
};

static struct hdspe_midi hdspe_madiface_midi_ports[1] = {
	{.portname = "MIDIoverMADI",
	 .dataIn = HDSPE_midiDataIn2,
	 .statusIn = HDSPE_midiStatusIn2,
	 .dataOut = HDSPE_midiDataOut2,
	 .statusOut = HDSPE_midiStatusOut2,
	 .ie = HDSPE_Midi2InterruptEnable,
	 .irq = HDSPE_midi2IRQPending,
	}	
};

int hdspe_init_madi(struct hdspe* hdspe)
{
	int midi_count = 0;
	struct hdspe_midi *midi_ports = NULL;
	
	hdspe->reg.control.madi.Master = true;
	hdspe->reg.control.madi.tx_64ch = true;
	hdspe->reg.control.madi.inp_0 = HDSPE_MADI_INPUT_COAXIAL;
	// TODO: shouldn't we prefer Auto Input?

#ifdef FROM_WIN_DRIVER
			deviceExtension->Register |= LineOut;

			if (deviceExtension->Channels) deviceExtension->Register |= tx_64ch;
			else deviceExtension->Register &= ~tx_64ch;

			if (deviceExtension->AutoInput) deviceExtension->Register |= AutoInp;
			else deviceExtension->Register &= ~AutoInp;

			if (deviceExtension->Frame) deviceExtension->Register &= ~SMUX;
			else deviceExtension->Register |= SMUX;

			if (deviceExtension->InputSource) // || deviceExtension->Revision == MADI_EXPRESSCARD_REV)
				deviceExtension->Register |= inp_0;
			else
				deviceExtension->Register &= ~inp_0;

			if (deviceExtension->SingleSpeed) deviceExtension->Register |= MADI_WCK48;
			else deviceExtension->Register &= ~MADI_WCK48;

			deviceExtension->Register &= ~disable_MM;	
#endif
	
	hdspe_write_control(hdspe);

	hdspe->m = hdspe_madi_methods;

	switch (hdspe->io_type) {
	case HDSPE_MADI:
		hdspe->card_name = "RME MADI";
		midi_count = 3 + (hdspe->tco ? 1 : 0);
		midi_ports = hdspe_madi_midi_ports;
		break;

	case HDSPE_MADIFACE:
		hdspe->card_name = "RME MADIface";
		midi_count = 1;
		midi_ports = hdspe_madiface_midi_ports;
		break;

	default:
		snd_BUG();
		return -ENODEV;
	}
	hdspe_init_midi(hdspe, midi_count, midi_ports);
	
	hdspe->t = hdspe_madi_tables;
	hdspe_init_autosync_tables(
		hdspe, ARRAY_SIZE(madi_autosync_ref), madi_autosync_ref);

	return 0;
}

void hdspe_terminate_madi(struct hdspe* hdspe)
{
	/* nothing to do */
}
