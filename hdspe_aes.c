// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file hdspe_aes.c
 * @brief RME HDSPe AES driver methods.
 *
 * 20210728 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORs,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

#include "hdspe.h"
#include "hdspe_core.h"

/* Map AES WR_CONTROL / RD_STATUS0 sync ref 4-bit code to hdspe_clock_source 
 * enum: identity mapping except for the unused codes. */
static enum hdspe_clock_source aes_autosync_ref[16] = {
	HDSPE_CLOCK_SOURCE_WORD,
	HDSPE_CLOCK_SOURCE_AES1,
	HDSPE_CLOCK_SOURCE_AES2,	
	HDSPE_CLOCK_SOURCE_AES3,
	HDSPE_CLOCK_SOURCE_AES4,	
	HDSPE_CLOCK_SOURCE_AES5,
	HDSPE_CLOCK_SOURCE_AES6,	
	HDSPE_CLOCK_SOURCE_AES7,
	HDSPE_CLOCK_SOURCE_AES8,
	HDSPE_CLOCK_SOURCE_TCO,
	HDSPE_CLOCK_SOURCE_SYNC_IN,
	HDSPE_CLOCK_SOURCE_INTERN,    // unused codes
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN     // master clock mode
};

const char* const hdspe_aes_clock_source_names[HDSPE_CLOCK_SOURCE_COUNT] = {
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  0),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  1),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  2),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  3),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  4),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  5),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  6),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  7),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  8),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES,  9),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES, 10),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES, 11),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES, 12),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES, 13),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES, 14),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES, 15),
};

/* Number of channels for different Speed Modes */
#define AES_CHANNELS		16

/* port names */
static const char * const texts_ports_aes[] = {
	"AES.1", "AES.2", "AES.3", "AES.4", "AES.5", "AES.6", "AES.7",
	"AES.8", "AES.9.", "AES.10", "AES.11", "AES.12", "AES.13", "AES.14",
	"AES.15", "AES.16"
};

/* These tables map the ALSA channels 1..N to the channels that we
   need to use in order to find the relevant channel buffer. RME
   refers to this kind of mapping as between "the ADAT channel and
   the DMA channel." We index it using the logical audio channel,
   and the value is the DMA channel (i.e. channel buffer number)
   where the data for that channel can be read/written from/to.
*/
static const char channel_map_aes[HDSPE_MAX_CHANNELS] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 11, 12, 13, 14, 15,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};


#ifdef CONFIG_SND_DEBUG
/* WR_CONTROL register bit names for AES card */
static const char* const aes_control_bitNames[32] = {
	"START",
	"LAT_0",
	"LAT_1",
	"LAT_2",
	"Master",
	"IE_AUDIO",
	"freq0",
	"freq1",
	"freq2",
	"PRO",
	"EMP",
	"Dolby",
	"?12",
	"SyncRef2",
	"?14",
	"?15",
	"SyncRef0",
	"SyncRef1",
	"SMUX",
	"CLR_TMS",
	"WCK48",
	"IEN2",
	"IEN0",
	"IEN1",
	"LineOut",
	"SyncRef3",
	"DS_DoubleWire",
	"QS_DoubleWire",
	"QS_QuadWire",
	"?29",
	"AES_float_format",
	"freq3"
};
#endif /*CONFIG_SND_DEBUG*/

static void hdspe_aes_read_status(struct hdspe* hdspe,
				  struct hdspe_status* status)
{
	int i;
	struct hdspe_control_reg_aes control = hdspe->reg.control.aes;
	struct hdspe_status0_reg_aes status0 = hdspe_read_status0(hdspe).aes;
	struct hdspe_status2_reg_aes status2 = hdspe_read_status2(hdspe).aes;
	u32 fbits = hdspe_read_fbits(hdspe);

	status->version = HDSPE_VERSION;

	hdspe_read_sample_rate_status(hdspe, status);

	status->clock_mode = control.Master;
	status->internal_freq = hdspe_internal_freq(hdspe);
	status->speed_mode = hdspe_speed_mode(hdspe);
	status->preferred_ref =
		(control.SyncRef3 << 3) |
		(control.SyncRef2 << 2) |		
		(control.SyncRef1 << 1) |
		(control.SyncRef0 << 0);
	status->autosync_ref = status0.sync_ref;

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_WORD,
		status0.wc_freq,
		status0.wc_lock,
		status0.wc_sync, true);

	for (i = 0; i < 8; i ++) {
		hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_AES1+i,
			HDSPE_FBITS_FREQ(fbits, i),
			status2.lock & (0x80 >> i),
			status2.sync & (0x80 >> i), true);
	}

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_TCO,
		status0.tco_freq,
		status0.tco_lock,
		status0.tco_sync,
		status0.tco_detect);

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_SYNC_IN,
		status2.sync_in_freq,
		status2.sync_in_lock,
		status2.sync_in_sync, true);

	for (i = HDSPE_CLOCK_SOURCE_SYNC_IN+1; i < HDSPE_CLOCK_SOURCE_COUNT; i++) {
		hdspe_set_sync_source(status, i,
				      0, false, false, false);
	}

	status->external_freq = hdspe_speed_adapt(
		status->freq[status->autosync_ref],
		status->speed_mode);

	status->wck48 = control.WCK48;
	status->clr_tms = control.CLR_TMS;

	// AES specific settings
	status->aes.pro = control.PRO;
	status->aes.emp = control.EMP;
	status->aes.dolby = control.Dolby;
	status->aes.smux = control.SMUX;
	status->aes.ds_mode = control.ds_mode;
	status->aes.qs_mode = control.qs_mode;

	// AES specific status
	status->aes.aes_mode = status2.aes_mode;
}

static bool hdspe_aes_has_status_changed(struct hdspe* hdspe)
{
	dev_dbg(hdspe->card->dev, "%s TODO\n", __func__);
	return false;
}

static void hdspe_aes_set_float_format(struct hdspe* hdspe, bool val)
{
	hdspe->reg.control.aes.FloatFmt = val;
	hdspe_write_control(hdspe);
}

static bool hdspe_aes_get_float_format(struct hdspe* hdspe)
{
	return hdspe->reg.control.aes.FloatFmt;
}

static enum hdspe_clock_mode hdspe_aes_get_clock_mode(struct hdspe *hdspe)
{
	return hdspe->reg.control.aes.Master;
}

static void hdspe_aes_set_clock_mode(struct hdspe* hdspe,
					  enum hdspe_clock_mode master)
{
	hdspe->reg.control.aes.Master = master;
	hdspe_write_control(hdspe);
}

static enum hdspe_clock_source hdspe_aes_get_preferred_sync_ref(
	struct hdspe* hdspe)
{
	struct hdspe_control_reg_aes control = hdspe->reg.control.aes;	
	return aes_autosync_ref[(control.SyncRef3 << 3) |
				(control.SyncRef2 << 2) |		
				(control.SyncRef1 << 1) |
				(control.SyncRef0 << 0)];
}

static void hdspe_aes_set_preferred_sync_ref(struct hdspe* hdspe,
					     enum hdspe_clock_source ref)
{
	struct hdspe_control_reg_aes control = hdspe->reg.control.aes;
	if (aes_autosync_ref[ref] == HDSPE_CLOCK_SOURCE_INTERN) ref = 0;
	control.SyncRef3 = (ref>>3)&1;
	control.SyncRef2 = (ref>>2)&1;
	control.SyncRef1 = (ref>>1)&1;
	control.SyncRef0 = (ref>>0)&1;
	hdspe_write_control(hdspe);
}

static enum hdspe_clock_source hdspe_aes_get_autosync_ref(struct hdspe* hdspe)
{
	return aes_autosync_ref[hdspe_read_status0(hdspe).aes.sync_ref];
}

static enum hdspe_sync_status hdspe_aes_get_sync_status(
	struct hdspe* hdspe, enum hdspe_clock_source src)
{
	dev_warn(hdspe->card->dev, "%s TODO.\n", __func__);
	return HDSPE_SYNC_STATUS_NOT_AVAILABLE;
}

static enum hdspe_freq hdspe_aes_get_freq(
	struct hdspe* hdspe, enum hdspe_clock_source src)
{
	dev_warn(hdspe->card->dev, "hdspe_aes_get_freq todo");
	return HDSPE_FREQ_NO_LOCK;
}

static enum hdspe_freq hdspe_aes_get_external_freq(struct hdspe* hdspe)
{
	enum hdspe_clock_source src = hdspe_aes_get_autosync_ref(hdspe);
	return hdspe_speed_adapt(hdspe_aes_get_freq(hdspe, src),
				 hdspe_speed_mode(hdspe));
}



static void hdspe_aes_proc_read(struct snd_info_entry * entry,
				struct snd_info_buffer *buffer)
{
	struct hdspe *hdspe = entry->private_data;
	dev_warn(hdspe->card->dev, "%s TODO.\n", __func__);

#ifdef OLDSTUFF
	struct hdspe *hdspe = entry->private_data;
	unsigned int status;
	unsigned int status2;
	unsigned int timecode;
	unsigned int wcLock, wcSync;
	int pref_syncref;
	char *autosync_ref;
	int x;

	status = hdspe_read(hdspe, HDSPE_RD_STATUS0);
	status2 = hdspe_read(hdspe, HDSPE_RD_STATUS2);
	timecode = hdspe_read(hdspe, HDSPE_RD_FBITS);

	snd_iprintf(buffer, "%s (Card #%d) Rev.%x\n",
		    hdspe->card_name, hdspe->card->number + 1,
		    hdspe->firmware_rev);

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
		    (hdspe->
		     reg.control.raw & HDSPE_LineOut) ? "on " : "off");

	snd_iprintf(buffer,
		    "ClearTrackMarker %s, Emphasis %s, Dolby %s\n",
		    (hdspe->
		     reg.control.raw & HDSPE_clr_tms) ? "on" : "off",
		    (hdspe->
		     reg.control.raw & HDSPE_Emphasis) ? "on" : "off",
		    (hdspe->
		     reg.control.raw & HDSPE_Dolby) ? "on" : "off");


	pref_syncref = hdspe_aes_get_preferred_sync_ref(hdspe);
	snd_iprintf(buffer, "Preferred Sync Reference: %s\n",
		    HDSPE_CLOCK_SOURCE_NAME(HDSPE_AES, pref_syncref));

	snd_iprintf(buffer, "System Clock Frequency: %d\n",
		    hdspe->system_sample_rate);

	snd_iprintf(buffer, "Double speed: %s\n",
			hdspe->reg.control.raw & HDSPE_DS_DoubleWire?
			"Double wire" : "Single wire");
	snd_iprintf(buffer, "Quad speed: %s\n",
			hdspe->reg.control.raw & HDSPE_QS_DoubleWire?
			"Double wire" :
			hdspe->reg.control.raw & HDSPE_QS_QuadWire?
			"Quad wire" : "Single wire");

	snd_iprintf(buffer, "--- Status:\n");

	wcLock = status & HDSPE_AES_wcLock;
	wcSync = wcLock && (status & HDSPE_AES_wcSync);

	snd_iprintf(buffer, "Word: %s  Frequency: %d\n",
		    (wcLock) ? (wcSync ? "Sync   " : "Lock   ") : "No Lock",
		    hdspe_freq_sample_rate(HDSPE_AES_wclk_freq(status)));

	for (x = 0; x < 8; x++) {
		snd_iprintf(buffer, "AES%d: %s  Frequency: %d\n",
			    x+1,
			    (status2 & (HDSPE_LockAES >> x)) ?
			    "Sync   " : "No Lock",
			    hdspe_freq_sample_rate((timecode >> (4*x)) & 0xF));
	}

	switch (hdspe_aes_get_autosync_ref(hdspe)) {
	case HDSPE_CLOCK_SOURCE_INTERN:
		autosync_ref = "None"; break;
	case HDSPE_CLOCK_SOURCE_WORD:
		autosync_ref = "Word Clock"; break;
	case HDSPE_CLOCK_SOURCE_AES1:
		autosync_ref = "AES1"; break;
	case HDSPE_CLOCK_SOURCE_AES2:
		autosync_ref = "AES2"; break;
	case HDSPE_CLOCK_SOURCE_AES3:
		autosync_ref = "AES3"; break;
	case HDSPE_CLOCK_SOURCE_AES4:
		autosync_ref = "AES4"; break;
	case HDSPE_CLOCK_SOURCE_AES5:
		autosync_ref = "AES5"; break;
	case HDSPE_CLOCK_SOURCE_AES6:
		autosync_ref = "AES6"; break;
	case HDSPE_CLOCK_SOURCE_AES7:
		autosync_ref = "AES7"; break;
	case HDSPE_CLOCK_SOURCE_AES8:
		autosync_ref = "AES8"; break;
	case HDSPE_CLOCK_SOURCE_TCO:
		autosync_ref = "TCO"; break;
	case HDSPE_CLOCK_SOURCE_SYNC_IN:
		autosync_ref = "Sync In"; break;
	default:
		autosync_ref = "---"; break;
	}
	snd_iprintf(buffer, "AutoSync ref = %s\n", autosync_ref);

	snd_iprintf(buffer, "\n");
#endif /*OLDSTUFF*/
}

static const struct hdspe_methods hdspe_aes_methods = {
	.get_card_info = hdspe_get_card_info,
	.read_status = hdspe_aes_read_status,
	.get_float_format = hdspe_aes_get_float_format,
	.set_float_format = hdspe_aes_set_float_format,
	.read_proc = hdspe_aes_proc_read,
	.get_freq = hdspe_aes_get_freq,
	.get_autosync_ref = hdspe_aes_get_autosync_ref,
	.get_external_freq = hdspe_aes_get_external_freq,
	.get_clock_mode = hdspe_aes_get_clock_mode,
	.set_clock_mode = hdspe_aes_set_clock_mode,
	.get_pref_sync_ref = hdspe_aes_get_preferred_sync_ref,
	.set_pref_sync_ref = hdspe_aes_set_preferred_sync_ref,
	.get_sync_status = hdspe_aes_get_sync_status,
	.has_status_changed = hdspe_aes_has_status_changed
};

static const struct hdspe_tables hdspe_aes_tables = {
	.ss_in_channels = AES_CHANNELS,
	.ss_out_channels = AES_CHANNELS,
	.ds_in_channels = AES_CHANNELS,
	.ds_out_channels = AES_CHANNELS,
	.qs_in_channels = AES_CHANNELS,
	.qs_out_channels = AES_CHANNELS,
	
	.channel_map_in_ss = channel_map_aes,
	.channel_map_out_ss = channel_map_aes,
	.channel_map_in_ds = channel_map_aes,
	.channel_map_out_ds = channel_map_aes,
	.channel_map_in_qs = channel_map_aes,
	.channel_map_out_qs = channel_map_aes,
	
	.port_names_in_ss = texts_ports_aes,
	.port_names_out_ss = texts_ports_aes,
	.port_names_in_ds = texts_ports_aes,
	.port_names_out_ds = texts_ports_aes,
	.port_names_in_qs = texts_ports_aes,
	.port_names_out_qs = texts_ports_aes,

	.clock_source_names = hdspe_aes_clock_source_names
};

static struct hdspe_midi hdspe_aes_midi_ports[3] = {
	{.portname = "MIDI 1",
	 .dataIn = HDSPE_midiDataIn0,
	 .statusIn = HDSPE_midiStatusIn0,
	 .dataOut = HDSPE_midiDataOut0,
	 .statusOut = HDSPE_midiStatusOut0,
	 .ie = HDSPE_Midi0InterruptEnable,
	 .irq = HDSPE_midi0IRQPending,
	},
	{.portname = "MIDI 2",
	 .dataIn = HDSPE_midiDataIn1,
	 .statusIn = HDSPE_midiStatusIn1,
	 .dataOut = HDSPE_midiDataOut1,
	 .statusOut = HDSPE_midiStatusOut1,
	 .ie = HDSPE_Midi1InterruptEnable,
	 .irq = HDSPE_midi1IRQPending,
	},
	{.portname = "MTC",
	 .dataIn = HDSPE_midiDataIn2,
	 .statusIn = HDSPE_midiStatusIn2,
	 .dataOut = -1,
	 .statusOut = -1,
	 .ie = HDSPE_Midi2InterruptEnable,
	 .irq = HDSPE_midi2IRQPendingAES,
	}
};

int hdspe_init_aes(struct hdspe* hdspe)
{
	hdspe->reg.control.aes.Master = true;
	hdspe->reg.control.aes.SyncRef0 = 1; // preferred sync is AES1
	hdspe->reg.control.aes.PRO = true;   // Professional mode

#ifdef FROM_WIN_DRIVER
	// TODO

			if (deviceExtension->Emphasis) deviceExtension->Register |= EMP;
			else deviceExtension->Register &= ~EMP;

			if (deviceExtension->Professional) deviceExtension->Register |= PRO;
			else deviceExtension->Register &= ~PRO;

			if (deviceExtension->NonAudio) deviceExtension->Register |= Dolby;
			else deviceExtension->Register &= ~Dolby;

			if (deviceExtension->DoubleSpeed) deviceExtension->Register |= DS_DoubleWire;
			else deviceExtension->Register &= ~DS_DoubleWire;

			deviceExtension->Register &= ~(QS_DoubleWire+QS_QuadWire);
			switch (deviceExtension->QuadSpeed) {
			case 1: deviceExtension->Register |= QS_DoubleWire; break;
			case 2: deviceExtension->Register |= QS_QuadWire; break;
			}

			if (deviceExtension->Frame) deviceExtension->Register |= MADI_WCK48;
			else deviceExtension->Register &= ~MADI_WCK48;

			if (deviceExtension->InputSource) deviceExtension->Register |= WCK_TERM;
			else deviceExtension->Register &= ~WCK_TERM;
	
	if (deviceExtension->FormatAC3 && deviceExtension->DeviceType != kRPM && deviceExtension->DeviceType != kHDSP_MADI) {
		deviceExtension->Register |= Dolby;
		deviceExtension->Register &= ~LineOut;
	}
#endif
	
	hdspe_write_control(hdspe);	
	
	hdspe->m = hdspe_aes_methods;

	hdspe->card_name = "RME AES";
	hdspe_init_midi(hdspe, 2 + (hdspe->tco ? 1 : 0), hdspe_aes_midi_ports);

	hdspe->t = hdspe_aes_tables;
	hdspe_init_autosync_tables(
		hdspe, ARRAY_SIZE(aes_autosync_ref), aes_autosync_ref);

	return 0;
}

void hdspe_terminate_aes(struct hdspe* hdspe)
{
#ifdef FROM_WIN_DRIVER
	// TODO
	if (!deviceExtension->NonAudio && deviceExtension->DeviceType != kRPM && deviceExtension->DeviceType != kHDSP_MADI)
		deviceExtension->Register &= ~Dolby;

	if (deviceExtension->FormatAC3)
		deviceExtension->Register |= LineOut;
#endif 
}
