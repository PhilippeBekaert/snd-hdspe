// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe_common.c
 * @brief RME HDSPe cards common driver methods.
 *
 * 20210727,28,29 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORS,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

#include "hdspe.h"
#include "hdspe_core.h"

#include <linux/math64.h>

#ifdef FROM_WIN_DRIVER
//TODO
// Loopback
void hwRecordOutput(PDEVICE_EXTENSION deviceExtension, int channel, int value)
{
	TRACE(TL_TRACE, ("hwRecordOutput: channel=%d, value=%d, numChannels=%d\n", channel, value, deviceExtension->NumChannels));

	if (channel < deviceExtension->NumChannels) {

		deviceExtension->RecordOutput[channel] = value;

		if (deviceExtension->DeviceType < kHDSP_AES) {
			WriteRegister(deviceExtension, RECORD_OUTPUT+channel, value);
		} else {
			TRACE(TL_TRACE, ("...hwRecordOutput MADI\n"));
			WriteRegister(deviceExtension, MADI_RECORD_OUTPUT+channel, value);
		}
	}
}
#endif 

u32 hdspe_freq_sample_rate(enum hdspe_freq f)
{
	static u32 rates[HDSPE_FREQ_COUNT] = {
		HDSPE_FREQ_SAMPLE_RATE(0),
		HDSPE_FREQ_SAMPLE_RATE(1),
		HDSPE_FREQ_SAMPLE_RATE(2),		
		HDSPE_FREQ_SAMPLE_RATE(3),
		HDSPE_FREQ_SAMPLE_RATE(4),
		HDSPE_FREQ_SAMPLE_RATE(5),		
		HDSPE_FREQ_SAMPLE_RATE(6),
		HDSPE_FREQ_SAMPLE_RATE(7),
		HDSPE_FREQ_SAMPLE_RATE(8),		
	};
	return (f >= 0 && f < HDSPE_FREQ_COUNT) ? rates[f] : 0;
}

/* Get the speed mode reflecting the sample rate f */
static enum hdspe_speed hdspe_sample_rate_speed_mode(u32 rate)
{
	if (rate < 56000)
		return HDSPE_SPEED_SINGLE;
	else if (rate < 112000)
		return HDSPE_SPEED_DOUBLE;
	else
		return HDSPE_SPEED_QUAD;
}

/* Get the speed mode encoded in the frequency class */
static enum hdspe_speed hdspe_freq_speed(enum hdspe_freq f)
{
	switch (f) {
	case 1: case 2: case 3:
		return HDSPE_SPEED_SINGLE;
	case 4: case 5: case 6:
		return HDSPE_SPEED_DOUBLE;
	case 7: case 8: case 9:
		return HDSPE_SPEED_QUAD;
	default:
		snd_BUG();
		return HDSPE_SPEED_INVALID;
	};
}

/* Get the frequency class best representing the given rate */
enum hdspe_freq hdspe_sample_rate_freq(u32 rate)
{
	enum hdspe_freq f;
	int speed_coef = 0;
	
	if (rate >= 112000) {
		rate /= 4;
		speed_coef = 6;
	} else if (rate >= 56000) {
		rate /= 2;
		speed_coef = 3;
	}

	if (rate < 38050)
		f = HDSPE_FREQ_32KHZ;
	else if (rate < 46050)
		f = HDSPE_FREQ_44_1KHZ;
	else
		f = HDSPE_FREQ_48KHZ;

	return f + speed_coef;
}

/* Converts frequency class <f> to speed mode <speed_mode> */
enum hdspe_freq hdspe_speed_adapt(enum hdspe_freq f, enum hdspe_speed speed_mode)
{
	switch (f) {
	case 0: break; /* This happens when autosync source looses sync e.g. */
	case 1: case 2: case 3:
		switch (speed_mode) {
		case HDSPE_SPEED_DOUBLE: f += 3; break;
		case HDSPE_SPEED_QUAD  : f += 6; break;
		default: {}
		}
		break;
	case 4: case 5: case 6:
		switch (speed_mode) {
		case HDSPE_SPEED_SINGLE: f -= 3; break;
		case HDSPE_SPEED_QUAD  : f += 3; break;
		default: {}
		}
		break;
	case 7: case 8: case 9:
		switch (speed_mode) {
		case HDSPE_SPEED_SINGLE: f -= 6; break;
		case HDSPE_SPEED_DOUBLE: f -= 3; break;
		default: {}
		}
		break;
	default:
		snd_BUG();
		return HDSPE_FREQ_INVALID;
	}
	return f;
}

/* Adapt the sample rate <rate> to the given speed mode */
static u32 hdspe_sample_rate_adapt(u32 rate, enum hdspe_speed speed_mode)
{
	switch (hdspe_sample_rate_speed_mode(rate)) {
	case HDSPE_SPEED_SINGLE:
		switch (speed_mode) {
		case HDSPE_SPEED_DOUBLE: rate *= 2; break;
		case HDSPE_SPEED_QUAD  : rate *= 4; break;
		default: {}
		}
		break;

	case HDSPE_SPEED_DOUBLE:
		switch (speed_mode) {
		case HDSPE_SPEED_SINGLE: rate /= 2; break;
		case HDSPE_SPEED_QUAD  : rate *= 2; break;
		default: {}
		}
		break;

	case HDSPE_SPEED_QUAD:
		switch (speed_mode) {
		case HDSPE_SPEED_SINGLE: rate /= 4; break;
		case HDSPE_SPEED_DOUBLE: rate /= 2; break;
		default: {}
		}
		break;

	default:
		snd_BUG();
	}

	return rate;
}


/* Get the current speed mode */
enum hdspe_speed hdspe_speed_mode(struct hdspe* hdspe)
{
	struct hdspe_control_reg_common control = hdspe->reg.control.common;
	return control.qs ? HDSPE_SPEED_QUAD
		: control.ds ? HDSPE_SPEED_DOUBLE
		: HDSPE_SPEED_SINGLE;
}

int hdspe_speed_factor(struct hdspe* hdspe)
{
	struct hdspe_control_reg_common control = hdspe->reg.control.common;
	return control.qs ? 4 : control.ds ? 2 : 1;
}

/* Get the current internal frequency class */
enum hdspe_freq hdspe_internal_freq(struct hdspe* hdspe)
{
	struct hdspe_control_reg_common control = hdspe->reg.control.common;
	return control.freq + (control.qs ? 6 : control.ds ? 3 : 0);
}

/* Write the internal frequency (single speed frequency and speed
 * mode control register bits). */
int hdspe_write_internal_freq(struct hdspe* hdspe, enum hdspe_freq f)
{
	enum hdspe_freq single_speed_freq =
		hdspe_speed_adapt(f, HDSPE_SPEED_SINGLE);
	enum hdspe_speed speed_mode =
		hdspe_freq_speed(f);

	dev_dbg(hdspe->card->dev, "%s(%d)\n", __func__, f);

	if (f == hdspe_internal_freq(hdspe))
		return false;
	
	hdspe->reg.control.common.freq = single_speed_freq;
	hdspe->reg.control.common.ds = (speed_mode == HDSPE_SPEED_DOUBLE);
	hdspe->reg.control.common.qs = (speed_mode == HDSPE_SPEED_QUAD);
	hdspe_write_control(hdspe);

	/* Update TCO module "app" sample rate */
	if (hdspe->tco)
		hdspe_tco_set_app_sample_rate(hdspe);
	
	return true;
}

/* PLL reference frequency constants */
static u64 freq_const[HDSPE_IO_TYPE_COUNT] = {
	110069313433624ULL,   // MADI
	131072000000000ULL,   // MADIface
	110069313433624ULL,   // AES
	104857600000000ULL,   // RayDAT
	104857600000000ULL,   // AIO
	104857600000000ULL    // AIO Pro
};

/* Convert DDS register period to Parts Pro Milion pitch value, 
 * relative to the control registers single speed internal frequency. */
static int hdspe_dds2ppm(struct hdspe* hdspe, u32 dds)
{
	// ppm = 1000000 * refdds / dds - 1000000
	// refdds = fconst / refrate
	struct hdspe_control_reg_common control = hdspe->reg.control.common;
	u32 refrate = hdspe_freq_sample_rate(control.freq);
	u64 fconst = freq_const[hdspe->io_type];
	u64 refdds = 1000000ULL * div_u64(fconst, refrate);
	snd_BUG_ON(dds == 0);
	return dds != 0 ? (int)div_u64(refdds, dds) : 1000000; // - 1000000;
}

/* Convert Parts Pro Milion pitch value relative to the control registers 
 * single speed internal frequency to DDS register period. */
static u32 hdspe_ppm2dds(struct hdspe* hdspe, int ppm)
{
	// dds = 1000000 * refdds / (1000000 + ppm)
	// refdds = fconst / refrate	
	struct hdspe_control_reg_common control = hdspe->reg.control.common;
	u32 refrate = hdspe_freq_sample_rate(control.freq);
	u64 fconst = freq_const[hdspe->io_type];
	u64 refdds = div_u64(fconst, refrate);
	u64 refddsM = 1000000ULL * refdds;
	/*	snd_BUG_ON(ppm == 0); */
	return ppm != 0 ? (u32)div_u64(refddsM, ppm) : refdds;
}

/* Convert DDS value to sample rate, taking into account the current speed
 * mode. */
static u32 hdspe_dds_sample_rate(struct hdspe* hdspe, u32 dds)
{
	struct hdspe_control_reg_common control = hdspe->reg.control.common;
	u64 fconst = freq_const[hdspe->io_type] *
		(control.qs ? 4 : control.ds ? 2 : 1);
	/*	snd_BUG_ON(dds == 0); not a bug here, but need to catch it */
	return dds != 0 ? (u32)div_u64(fconst, dds)
	  : hdspe_freq_sample_rate(control.freq);
}

void hdspe_dds_range(struct hdspe* hdspe, u32* min, u32* max)
{
	u64 fconst = freq_const[hdspe->io_type];
	*min = (u32)div_u64(fconst, 51750); /* 207KHz / 4 */
	*max = (u32)div_u64(fconst, 27000);
}

u32 hdspe_get_dds(struct hdspe* hdspe)
{
	return le32_to_cpu(hdspe->reg.pll_freq);
}

int hdspe_write_dds(struct hdspe* hdspe, u32 dds)
{
	__le32 dds_le = cpu_to_le32(dds);
	u32 ddsmin, ddsmax;
	int rc = 0;

	hdspe_dds_range(hdspe, &ddsmin, &ddsmax);
	if (dds < ddsmin || dds > ddsmax) {
		rc = -EINVAL;
		goto done;
	}

	if (dds_le == hdspe->reg.pll_freq) {
		rc = 0;
		goto done;
	}
	
	hdspe->reg.pll_freq = dds_le;
	hdspe_write_pll_freq(hdspe);
	rc = 1;

done:
	dev_dbg(hdspe->card->dev, "%s() dds = %u sample_rate = %u rc = %d.\n",
		__func__, dds, hdspe_dds_sample_rate(hdspe, dds), rc);
	return rc;
}

u32 hdspe_internal_pitch(struct hdspe* hdspe)
{
	return hdspe_dds2ppm(hdspe, hdspe_get_dds(hdspe));
}

int hdspe_write_internal_pitch(struct hdspe* hdspe, int ppm)
{
	return hdspe_write_dds(hdspe, hdspe_ppm2dds(hdspe, ppm));
}

u32 hdspe_read_system_pitch(struct hdspe* hdspe)
{
	return hdspe_dds2ppm(hdspe, hdspe_read_pll_freq(hdspe));
}

u32 hdspe_read_system_sample_rate(struct hdspe* hdspe)
{
	return hdspe_dds_sample_rate(hdspe, hdspe_read_pll_freq(hdspe));
}

void hdspe_read_sample_rate_status(struct hdspe* hdspe,
				   struct hdspe_status* status)
{
	struct hdspe_control_reg_common control = hdspe->reg.control.common;
	status->sample_rate_numerator = freq_const[hdspe->io_type] *
		(control.qs ? 4 : control.ds ? 2 : 1);
	status->sample_rate_denominator = hdspe_read_pll_freq(hdspe);
	status->internal_sample_rate_denominator =
		le32_to_cpu(hdspe->reg.pll_freq);
	status->buffer_size = hdspe_period_size(hdspe);
	status->running = hdspe->running;
	status->capture_pid = hdspe->capture_pid;
	status->playback_pid = hdspe->playback_pid;
}

static int hdspe_write_system_sample_rate(struct hdspe* hdspe, u32 rate)
{
	int changed = false;
	u32 single_speed_rate =
		hdspe_sample_rate_adapt(rate, HDSPE_SPEED_SINGLE);
	enum hdspe_freq freq =
		hdspe_sample_rate_freq(rate);
	u64 dds = div_u64(freq_const[hdspe->io_type], single_speed_rate);

	dev_dbg(hdspe->card->dev, "%s(%d) ...\n", __func__, rate);	

	changed = hdspe_write_internal_freq(hdspe, freq);

	/* dds should be less than 2^32 for being written to FREQ register */
	snd_BUG_ON(dds >> 32);
	if (hdspe_write_dds(hdspe, dds))
		changed = true;

	return changed;
}

void hdspe_set_channel_map(struct hdspe* hdspe, enum hdspe_speed speed)
{
	dev_dbg(hdspe->card->dev, "%s()\n", __func__);
	
	switch (speed) {
	case HDSPE_SPEED_SINGLE:
		hdspe->channel_map_in = hdspe->t.channel_map_in_ss;
		hdspe->channel_map_out = hdspe->t.channel_map_out_ss;
		hdspe->max_channels_in = hdspe->t.ss_in_channels;
		hdspe->max_channels_out = hdspe->t.ss_out_channels;
		hdspe->port_names_in = hdspe->t.port_names_in_ss;
		hdspe->port_names_out = hdspe->t.port_names_out_ss;
		break;

	case HDSPE_SPEED_DOUBLE:
		hdspe->channel_map_in = hdspe->t.channel_map_in_ds;
		hdspe->channel_map_out = hdspe->t.channel_map_out_ds;
		hdspe->max_channels_in = hdspe->t.ds_in_channels;
		hdspe->max_channels_out = hdspe->t.ds_out_channels;
		hdspe->port_names_in = hdspe->t.port_names_in_ds;
		hdspe->port_names_out = hdspe->t.port_names_out_ds;
		break;

	case HDSPE_SPEED_QUAD:
		hdspe->channel_map_in = hdspe->t.channel_map_in_qs;
		hdspe->channel_map_out = hdspe->t.channel_map_out_qs;
		hdspe->max_channels_in = hdspe->t.qs_in_channels;
		hdspe->max_channels_out = hdspe->t.qs_out_channels;
		hdspe->port_names_in = hdspe->t.port_names_in_qs;
		hdspe->port_names_out = hdspe->t.port_names_out_qs;
		break;

	default: {}
	};
}

int hdspe_set_sample_rate(struct hdspe * hdspe, u32 desired_rate)
{
	/* TODO:
	   Changing between Single, Double and Quad speed is not
	   allowed if any substreams are open. This is because such a change
	   causes a shift in the location of the DMA buffers and a reduction
	   in the number of available buffers.

	   Note that a similar but essentially insoluble problem exists for
	   externally-driven rate changes. All we can do is to flag rate
	   changes in the read/write routines.
	 */
	int changed;
	enum hdspe_speed desired_speed_mode =
		hdspe_sample_rate_speed_mode(desired_rate);

	dev_dbg(hdspe->card->dev, "%s(%d)\n", __func__, desired_rate);

#ifdef NEVER
	/* locks up ubuntu desktop if we do that here. */
	if (!snd_hdspe_use_is_exclusive(hdspe)) {
		dev_warn(hdspe->card->dev, "Cannot change sample rate: no exclusive use.\n");
		return -EBUSY;
	}
#endif /*NEVER*/

#ifdef NEVER
	/* This prevents sample rate changes also in cases where they
	 * are perfectly fine. */
	if (desired_speed_mode != current_speed_mode) {
		if (hdspe->capture_pid >= 0 || hdspe->playback_pid >= 0) {
			dev_err(hdspe->card->dev,
				"Cannot change from %s speed to %s speed mode"
				"(capture PID = %d, playback PID = %d)\n",
				HDSPE_SPEED_NAME(current_speed_mode),
				HDSPE_SPEED_NAME(desired_speed_mode),
				hdspe->capture_pid, hdspe->playback_pid);
			return -EBUSY;
		}
	}
#endif /*NEVER*/

	changed = hdspe_write_system_sample_rate(hdspe, desired_rate);

	/*	if (changed) */
		hdspe_set_channel_map(hdspe, desired_speed_mode);

	if (changed) {
		/* notify Internal Frequency and DDS control elements */
		HDSPE_CTL_NOTIFY(internal_freq);
		HDSPE_CTL_NOTIFY(dds);
	}
	
	return changed;
}

