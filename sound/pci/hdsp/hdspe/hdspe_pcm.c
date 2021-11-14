// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe_pcm.c
 * @brief RME HDSPe PCM interface.
 *
 * 20210728 - Philippe.Bekaert@uhasselt.be
 *
 * Refactored work of the other MODULE_AUTHORs.
 */

#include "hdspe.h"
#include "hdspe_core.h"

#include <linux/pci.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>


/* the size of a substream (1 mono data stream) */
#define HDSPE_CHANNEL_BUFFER_SAMPLES  (16*1024)
#define HDSPE_CHANNEL_BUFFER_BYTES    (4*HDSPE_CHANNEL_BUFFER_SAMPLES)

/* the size of the area we need to allocate for DMA transfers. the
   size is the same regardless of the number of channels, and
   also the latency to use.
   for one direction !!!
*/
#define HDSPE_DMA_AREA_BYTES (HDSPE_MAX_CHANNELS * HDSPE_CHANNEL_BUFFER_BYTES)
#define HDSPE_DMA_AREA_KILOBYTES (HDSPE_DMA_AREA_BYTES/1024)


/*------------------------------------------------------------
   memory interface
 ------------------------------------------------------------*/
static int snd_hdspe_preallocate_memory(struct hdspe *hdspe)
{
	struct snd_pcm *pcm;
	size_t wanted;

	pcm = hdspe->pcm;

	wanted = HDSPE_DMA_AREA_BYTES;
	/* TODO: set 32-bit DMA mask */

	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV_SG,
					      &hdspe->pci->dev,
					      wanted, wanted);
	dev_dbg(hdspe->card->dev, " Preallocated %zd Bytes\n", wanted);
	return 0;
}

/* Inform the card what DMA addresses to use for the indicated channel. */
/* Each channel got 16 4K pages allocated for DMA transfers. */
static void hdspe_set_channel_dma_addr(struct hdspe *hdspe,
				       struct snd_pcm_substream *substream,
				       unsigned int reg,
				       int dma_channel, int logical_channel)
{
	int i;
#ifdef OLDSTUFF
	for (i = dma_channel * 16; i < dma_channel * 16 + 16; i++) {
		hdspe_write(hdspe, reg + 4 * i,
			    snd_pcm_sgbuf_get_addr(substream, 4096 * i));
	}
#endif /*OLDSTUFF*/
	for (i = 0; i < 16; i++) {
		int c = dma_channel * 16 + i;
		int n = logical_channel * 16 + i;
		hdspe_write(hdspe, reg + 4 * c,
			    snd_pcm_sgbuf_get_addr(substream, 4096 * n));
	}
}

/* enable DMA for specific channels, now available for DSP-MADI */
static inline void snd_hdspe_enable_in(struct hdspe * hdspe, int i, int v)
{
	hdspe_write(hdspe, HDSPE_inputEnableBase + (4 * i), v);
}

static inline void snd_hdspe_enable_out(struct hdspe * hdspe, int i, int v)
{
	hdspe_write(hdspe, HDSPE_outputEnableBase + (4 * i), v);
}

/* ------------------------------------------------------- */

/* Return hardware buffer pointer in samples (always 4 bytes) */
snd_pcm_uframes_t hdspe_hw_pointer(struct hdspe *hdspe)
{
	// (BUF_PTR << 6) bytes / 4 bytes per sample
	return le16_to_cpu(hdspe->reg.status0.common.BUF_PTR) << 4;

#ifdef OLDSTUFF	
	int position;

	position = hdspe_read(hdspe, HDSPE_RD_STATUS0);

	switch (hdspe->io_type) {
	case HDSPE_RAYDAT:
	case HDSPE_AIO:
	case HDSPE_AIO_PRO:		
		position &= HDSPE_BufferPositionMask;
		position /= 4; /* Bytes per sample */
		break;
	default:
		position = (position & HDSPE_BufferID) ?
		  (hdspe->period_bytes / 4) : 0; // hdspe_period_size(hdspe)
	}

	return position;
#endif /*OLDSTUFF*/	
}

static u32 hdspe_hw_buffer_size(struct hdspe* hdspe)
{
	return (1<<16) / 4;   /* 16-bit pointer (only 10 msb in status reg),
			       * 4 bytes per sample */
}

/**
 * Returns true if the card is a RayDAT / AIO / AIO Pro 
 */
static inline bool hdspe_is_raydat_or_aio(struct hdspe *hdspe)
{
	return ((HDSPE_AIO == hdspe->io_type) ||
		(HDSPE_RAYDAT == hdspe->io_type) ||
		(HDSPE_AIO_PRO == hdspe->io_type));
}

/* return period size in samples per period */
u32 hdspe_period_size(struct hdspe *hdspe)
{
	int n = hdspe->reg.control.common.LAT;

	/* Special case for new RME cards with 32 samples period size.
	 * The three latency bits in the control register
	 * (HDSP_LatencyMask) encode latency values of 64 samples as
	 * 0, 128 samples as 1 ... 4096 samples as 6. For old cards, 7
	 * denotes 8192 samples, but on new cards like RayDAT or AIO,
	 * it corresponds to 32 samples.
	 */
	if ((7 == n) && hdspe_is_raydat_or_aio(hdspe))
		n = -1;

	return 1 << (n + 6);
}

static int hdspe_set_interrupt_interval(struct hdspe *hdspe, unsigned int frames)
{
	int n;

	spin_lock_irq(&hdspe->lock);

	if (32 == frames) {
		/* Special case for new RME cards like RayDAT/AIO which
		 * support period sizes of 32 samples. Since latency is
		 * encoded in the three bits of HDSP_LatencyMask, we can only
		 * have values from 0 .. 7. While 0 still means 64 samples and
		 * 6 represents 4096 samples on all cards, 7 represents 8192
		 * on older cards and 32 samples on new cards.
		 *
		 * In other words, period size in samples is calculated by
		 * 2^(n+6) with n ranging from 0 .. 7.
		 */
		n = 7;
	} else {
		frames >>= 7;
		n = 0;
		while (frames) {
			n++;
			frames >>= 1;
		}
	}

	hdspe->reg.control.common.LAT = n;
	hdspe_write_control(hdspe);

	spin_unlock_irq(&hdspe->lock);

	snd_ctl_notify(hdspe->card, SNDRV_CTL_EVENT_MASK_VALUE,
		       hdspe->cid.buffer_size);
	
	return 0;
}

/* Called right from the interrupt handler in order to update the frame
 * counter. In absence of xruns, the frame counter increments by
 * hdspe_period_size() frames each period. This routine will correctly
 * determine the frame counter even in the presence of xruns or late
 * interrupt handling, as long as the hardware pointer did not wrap more 
 * than once since the previous invocation. The hardware pointer wraps every 
 * 16K frames, so about 3 times a second at 48 KHz sampling rate. */
void hdspe_update_frame_count(struct hdspe* hdspe)
{
	u32 hw_pointer;

	spin_lock(&hdspe->lock);
	hw_pointer = hdspe_hw_pointer(hdspe);
	if (hw_pointer < hdspe->last_hw_pointer)
		hdspe->hw_pointer_wrap_count ++;
	hdspe->last_hw_pointer = hw_pointer;

	hdspe->frame_count =
		(u64)hdspe->hw_pointer_wrap_count * hdspe_hw_buffer_size(hdspe)
		+ (hw_pointer & ~(hdspe_period_size(hdspe) - 1));
	spin_unlock(&hdspe->lock);
	
#ifdef NEVER
	{
		static u64 last_frame_count =0;
		dev_dbg(hdspe->card->dev, "%s: frame_count=%llu, delta=%llu\n",
			__func__, hdspe->frame_count,
			hdspe->frame_count - last_frame_count);
		last_frame_count = hdspe->frame_count;
	}
#endif /*NEVER*/
}

static inline void hdspe_start_audio(struct hdspe * s)
{
	if (s->tco) return;   // always running
	s->reg.control.common.START = s->reg.control.common.IE_AUDIO = true;
	hdspe_write_control(s);
}

static inline void hdspe_stop_audio(struct hdspe * s)
{
	if (s->tco) return;   // always running	
	s->reg.control.common.START = s->reg.control.common.IE_AUDIO = false;
	hdspe_write_control(s);
}

/* should I silence all or only opened ones ? doit all for first even is 4MB*/
static void hdspe_silence_playback(struct hdspe *hdspe)
{
	int i;
	int n = hdspe_period_size(hdspe) * 4;
	void *buf = hdspe->playback_buffer;

	if (!buf)
		return;

	for (i = 0; i < HDSPE_MAX_CHANNELS; i++) {
		memset(buf, 0, n);
		buf += HDSPE_CHANNEL_BUFFER_BYTES;
	}
}

static snd_pcm_uframes_t snd_hdspe_hw_pointer(struct snd_pcm_substream
					      *substream)
{
	struct hdspe *hdspe = snd_pcm_substream_chip(substream);
	return hdspe_hw_pointer(hdspe);
}

static int snd_hdspe_reset(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hdspe *hdspe = snd_pcm_substream_chip(substream);
	struct snd_pcm_substream *other;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		other = hdspe->capture_substream;
	else
		other = hdspe->playback_substream;

	if (hdspe->running)
		runtime->status->hw_ptr = hdspe_hw_pointer(hdspe);
	else
		runtime->status->hw_ptr = 0;

	if (other) {
		struct snd_pcm_substream *s;
		struct snd_pcm_runtime *oruntime = other->runtime;
		snd_pcm_group_for_each_entry(s, substream) {
			if (s == other) {
				oruntime->status->hw_ptr =
					runtime->status->hw_ptr;
				break;
			}
		}
	}
	return 0;
}

static void snd_hdspe_set_float_format(struct hdspe* hdspe, bool val)
{
	if (hdspe->m.get_float_format(hdspe) == val)
		return;

	dev_info(hdspe->card->dev,
		 "Switching to native 32-bit %s format.\n",
		 val ? "LE float" : "LE integer");
	hdspe->m.set_float_format(hdspe, val);
}

static int snd_hdspe_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct hdspe *hdspe = snd_pcm_substream_chip(substream);
	int err;
	int i;
	pid_t this_pid;
	pid_t other_pid;

	spin_lock_irq(&hdspe->lock);

	if (substream->pstr->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		this_pid = hdspe->playback_pid;
		other_pid = hdspe->capture_pid;
	} else {
		this_pid = hdspe->capture_pid;
		other_pid = hdspe->playback_pid;
	}

	if (other_pid > 0 && this_pid != other_pid) {

		/* The other stream is open, and not by the same
		   task as this one. Make sure that the parameters
		   that matter are the same.
		   */

		u32 sysrate = hdspe_read_system_sample_rate(hdspe);		
		if (params_rate(params) != sysrate) {
			spin_unlock_irq(&hdspe->lock);
			dev_warn(hdspe->card->dev,
 "Requested sample rate %d does not match actual rate %d used by process %d.\n",
				 params_rate(params), sysrate, other_pid);
			_snd_pcm_hw_param_setempty(params,
					SNDRV_PCM_HW_PARAM_RATE);
			return -EBUSY;
		}

		if (params_period_size(params) != hdspe_period_size(hdspe)) {
			spin_unlock_irq(&hdspe->lock);
			dev_warn(hdspe->card->dev,
 "Requested period size %d does not match actual latency used by process %d.\n",
				 params_period_size(params),
				 hdspe_period_size(hdspe));
			_snd_pcm_hw_param_setempty(params,
					SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
			return -EBUSY;
		}

	}
	/* We're fine. */
	spin_unlock_irq(&hdspe->lock);

	/* how to make sure that the rate matches an externally-set one ?   */

	spin_lock_irq(&hdspe->lock);
	err = hdspe_set_sample_rate(hdspe, params_rate(params));
	if (err < 0) {
		dev_info(hdspe->card->dev, "err on hdspe_set_rate: %d\n", err);
		spin_unlock_irq(&hdspe->lock);
		_snd_pcm_hw_param_setempty(params,
				SNDRV_PCM_HW_PARAM_RATE);
		return err;
	}
	spin_unlock_irq(&hdspe->lock);

	err = hdspe_set_interrupt_interval(hdspe,
			params_period_size(params));
	if (err < 0) {
		dev_info(hdspe->card->dev,
			 "err on hdspe_set_interrupt_interval: %d\n", err);
		_snd_pcm_hw_param_setempty(params,
				SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
		return err;
	}

	/* Memory allocation, takashi's method, dont know if we should
	 * spinlock
	 */
	/* malloc all buffer even if not enabled to get sure */
	/* Update for MADI rev 204: we need to allocate for all channels,
	 * otherwise it doesn't work at 96kHz */

	err =
		snd_pcm_lib_malloc_pages(substream, HDSPE_DMA_AREA_BYTES);
	if (err < 0) {
		dev_info(hdspe->card->dev,
			 "err on snd_pcm_lib_malloc_pages: %d\n", err);
		return err;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		for (i = 0; i < params_channels(params); ++i) {
			int c = hdspe->channel_map_out[i];

			if (c < 0)
				continue;      /* just make sure */
			hdspe_set_channel_dma_addr(hdspe, substream,
						   HDSPE_pageAddressBufferOut,
						   c, i);
			snd_hdspe_enable_out(hdspe, c, 1);
		}

		hdspe->playback_buffer =
			(unsigned char *) substream->runtime->dma_area;
		dev_dbg(hdspe->card->dev,
			"Allocated sample buffer for playback at %p\n",
				hdspe->playback_buffer);
	} else {
		for (i = 0; i < params_channels(params); ++i) {
			int c = hdspe->channel_map_in[i];

			if (c < 0)
				continue;
			hdspe_set_channel_dma_addr(hdspe, substream,
						   HDSPE_pageAddressBufferIn,
						   c, i);
			snd_hdspe_enable_in(hdspe, c, 1);
		}

		hdspe->capture_buffer =
			(unsigned char *) substream->runtime->dma_area;
		dev_dbg(hdspe->card->dev,
			"Allocated sample buffer for capture at %p\n",
				hdspe->capture_buffer);
	}

	/*
	   dev_dbg(hdspe->card->dev,
	   "Allocated sample buffer for %s at 0x%08X\n",
	   substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
	   "playback" : "capture",
	   snd_pcm_sgbuf_get_addr(substream, 0));
	   */
	/*
	   dev_dbg(hdspe->card->dev,
	   "set_hwparams: %s %d Hz, %d channels, bs = %d\n",
	   substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
	   "playback" : "capture",
	   params_rate(params), params_channels(params),
	   params_buffer_size(params));
	   */

	/* Switch to native float format if requested, s32le otherwise. */
	snd_hdspe_set_float_format(
		hdspe, params_format(params) == SNDRV_PCM_FORMAT_FLOAT_LE);

	return 0;
}

static int snd_hdspe_hw_free(struct snd_pcm_substream *substream)
{
	int i;
	struct hdspe *hdspe = snd_pcm_substream_chip(substream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* Just disable all channels. The saving when disabling a */
		/* smaller set is not worth the trouble. */
		for (i = 0; i < HDSPE_MAX_CHANNELS; ++i)
			snd_hdspe_enable_out(hdspe, i, 0);

		hdspe->playback_buffer = NULL;
	} else {
		for (i = 0; i < HDSPE_MAX_CHANNELS; ++i)
			snd_hdspe_enable_in(hdspe, i, 0);

		hdspe->capture_buffer = NULL;
	}

	snd_pcm_lib_free_pages(substream);

	return 0;
}


static int snd_hdspe_channel_info(struct snd_pcm_substream *substream,
		struct snd_pcm_channel_info *info)
{
	struct hdspe *hdspe = snd_pcm_substream_chip(substream);
	unsigned int channel = info->channel;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (snd_BUG_ON(channel >= hdspe->max_channels_out)) {
			dev_info(hdspe->card->dev,
				 "snd_hdspe_channel_info: output channel out of range (%d)\n",
				 channel);
			return -EINVAL;
		}

		channel = array_index_nospec(channel, hdspe->max_channels_out);
		if (hdspe->channel_map_out[channel] < 0) {
			dev_info(hdspe->card->dev,
				 "snd_hdspe_channel_info: output channel %d mapped out\n",
				 channel);
			return -EINVAL;
		}
		info->offset = channel * HDSPE_CHANNEL_BUFFER_BYTES;
	} else {
		if (snd_BUG_ON(channel >= hdspe->max_channels_in)) {
			dev_info(hdspe->card->dev,
				 "snd_hdspe_channel_info: input channel out of range (%d)\n",
				 channel);
			return -EINVAL;
		}

		channel = array_index_nospec(channel, hdspe->max_channels_in);
		if (hdspe->channel_map_in[channel] < 0) {
			dev_info(hdspe->card->dev,
				 "snd_hdspe_channel_info: input channel %d mapped out\n",
				 channel);
			return -EINVAL;
		}
		info->offset = channel * HDSPE_CHANNEL_BUFFER_BYTES;
	}

	info->first = 0;
	info->step = 32;
	return 0;
}


static int snd_hdspe_ioctl(struct snd_pcm_substream *substream,
		unsigned int cmd, void *arg)
{
	switch (cmd) {
	case SNDRV_PCM_IOCTL1_RESET:
		return snd_hdspe_reset(substream);

	case SNDRV_PCM_IOCTL1_CHANNEL_INFO:
		{
			struct snd_pcm_channel_info *info = arg;
			return snd_hdspe_channel_info(substream, info);
		}
	default:
		break;
	}

	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int snd_hdspe_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct hdspe *hdspe = snd_pcm_substream_chip(substream);
	struct snd_pcm_substream *other;
	int running;

	spin_lock(&hdspe->lock);
	running = hdspe->running;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		running |= 1 << substream->stream;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		running &= ~(1 << substream->stream);
		break;
	default:
		snd_BUG();
		spin_unlock(&hdspe->lock);
		return -EINVAL;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		other = hdspe->capture_substream;
	else
		other = hdspe->playback_substream;

	if (other) {
		struct snd_pcm_substream *s;
		snd_pcm_group_for_each_entry(s, substream) {
			if (s == other) {
				snd_pcm_trigger_done(s, substream);
				if (cmd == SNDRV_PCM_TRIGGER_START)
					running |= 1 << s->stream;
				else
					running &= ~(1 << s->stream);
				goto _ok;
			}
		}
		if (cmd == SNDRV_PCM_TRIGGER_START) {
			if (!(running & (1 << SNDRV_PCM_STREAM_PLAYBACK))
					&& substream->stream ==
					SNDRV_PCM_STREAM_CAPTURE)
				hdspe_silence_playback(hdspe);
		} else {
			if (running &&
				substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
				hdspe_silence_playback(hdspe);
		}
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			hdspe_silence_playback(hdspe);
	}
_ok:
	snd_pcm_trigger_done(substream, substream);
	if (!hdspe->running && running)
		hdspe_start_audio(hdspe);
	else if (hdspe->running && !running)
		hdspe_stop_audio(hdspe);
	hdspe->running = running;
	spin_unlock(&hdspe->lock);

	snd_ctl_notify(hdspe->card, SNDRV_CTL_EVENT_MASK_VALUE,
		       hdspe->cid.running);
	
	return 0;
}

static int snd_hdspe_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static const struct snd_pcm_hardware snd_hdspe_playback_subinfo = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_NONINTERLEAVED |
		 SNDRV_PCM_INFO_SYNC_START | SNDRV_PCM_INFO_DOUBLE),
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
//	.formats = SNDRV_PCM_FMTBIT_FLOAT_LE,	
	.rates = (SNDRV_PCM_RATE_32000 |
		  SNDRV_PCM_RATE_44100 |
		  SNDRV_PCM_RATE_48000 |
		  SNDRV_PCM_RATE_64000 |
		  SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 |
		  SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000 ),
	.rate_min = 32000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = HDSPE_MAX_CHANNELS,
	.buffer_bytes_max =
	    HDSPE_CHANNEL_BUFFER_BYTES * HDSPE_MAX_CHANNELS,
	.period_bytes_min = (32 * 4),
	.period_bytes_max = (8192 * 4) * HDSPE_MAX_CHANNELS,
	.periods_min = 2,
	.periods_max = 512,
	.fifo_size = 0
};

static const struct snd_pcm_hardware snd_hdspe_capture_subinfo = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_NONINTERLEAVED |
		 SNDRV_PCM_INFO_SYNC_START),
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
//	.formats = SNDRV_PCM_FMTBIT_FLOAT_LE,
	.rates = (SNDRV_PCM_RATE_32000 |
		  SNDRV_PCM_RATE_44100 |
		  SNDRV_PCM_RATE_48000 |
		  SNDRV_PCM_RATE_64000 |
		  SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 |
		  SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000),
	.rate_min = 32000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = HDSPE_MAX_CHANNELS,
	.buffer_bytes_max =
	    HDSPE_CHANNEL_BUFFER_BYTES * HDSPE_MAX_CHANNELS,
	.period_bytes_min = (32 * 4),
	.period_bytes_max = (8192 * 4) * HDSPE_MAX_CHANNELS,
	.periods_min = 2,
	.periods_max = 512,
	.fifo_size = 0
};

static int snd_hdspe_hw_rule_in_channels_rate(struct snd_pcm_hw_params *params,
					   struct snd_pcm_hw_rule *rule)
{
	struct hdspe *hdspe = rule->private;
	struct snd_interval *c =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_interval *r =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	if (r->min > 96000 && r->max <= 192000) {
		struct snd_interval t = {
			.min = hdspe->t.qs_in_channels,
			.max = hdspe->t.qs_in_channels,
			.integer = 1,
		};
		return snd_interval_refine(c, &t);
	} else if (r->min > 48000 && r->max <= 96000) {
		struct snd_interval t = {
			.min = hdspe->t.ds_in_channels,
			.max = hdspe->t.ds_in_channels,
			.integer = 1,
		};
		return snd_interval_refine(c, &t);
	} else if (r->max < 64000) {
		struct snd_interval t = {
			.min = hdspe->t.ss_in_channels,
			.max = hdspe->t.ss_in_channels,
			.integer = 1,
		};
		return snd_interval_refine(c, &t);
	}

	return 0;
}

static int snd_hdspe_hw_rule_out_channels_rate(struct snd_pcm_hw_params *params,
					   struct snd_pcm_hw_rule * rule)
{
	struct hdspe *hdspe = rule->private;
	struct snd_interval *c =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_interval *r =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	if (r->min > 96000 && r->max <= 192000) {
		struct snd_interval t = {
			.min = hdspe->t.qs_out_channels,
			.max = hdspe->t.qs_out_channels,
			.integer = 1,
		};
		return snd_interval_refine(c, &t);
	} else if (r->min > 48000 && r->max <= 96000) {
		struct snd_interval t = {
			.min = hdspe->t.ds_out_channels,
			.max = hdspe->t.ds_out_channels,
			.integer = 1,
		};
		return snd_interval_refine(c, &t);
	} else if (r->max < 64000) {
		struct snd_interval t = {
			.min = hdspe->t.ss_out_channels,
			.max = hdspe->t.ss_out_channels,
			.integer = 1,
		};
		return snd_interval_refine(c, &t);
	} else {
	}
	return 0;
}

static int snd_hdspe_hw_rule_rate_in_channels(struct snd_pcm_hw_params *params,
					   struct snd_pcm_hw_rule * rule)
{
	struct hdspe *hdspe = rule->private;
	struct snd_interval *c =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_interval *r =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	if (c->min >= hdspe->t.ss_in_channels) {
		struct snd_interval t = {
			.min = 32000,
			.max = 48000,
			.integer = 1,
		};
		return snd_interval_refine(r, &t);
	} else if (c->max <= hdspe->t.qs_in_channels) {
		struct snd_interval t = {
			.min = 128000,
			.max = 192000,
			.integer = 1,
		};
		return snd_interval_refine(r, &t);
	} else if (c->max <= hdspe->t.ds_in_channels) {
		struct snd_interval t = {
			.min = 64000,
			.max = 96000,
			.integer = 1,
		};
		return snd_interval_refine(r, &t);
	}

	return 0;
}
static int snd_hdspe_hw_rule_rate_out_channels(struct snd_pcm_hw_params *params,
					   struct snd_pcm_hw_rule *rule)
{
	struct hdspe *hdspe = rule->private;
	struct snd_interval *c =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_interval *r =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	if (c->min >= hdspe->t.ss_out_channels) {
		struct snd_interval t = {
			.min = 32000,
			.max = 48000,
			.integer = 1,
		};
		return snd_interval_refine(r, &t);
	} else if (c->max <= hdspe->t.qs_out_channels) {
		struct snd_interval t = {
			.min = 128000,
			.max = 192000,
			.integer = 1,
		};
		return snd_interval_refine(r, &t);
	} else if (c->max <= hdspe->t.ds_out_channels) {
		struct snd_interval t = {
			.min = 64000,
			.max = 96000,
			.integer = 1,
		};
		return snd_interval_refine(r, &t);
	}

	return 0;
}

static int snd_hdspe_hw_rule_in_channels(struct snd_pcm_hw_params *params,
				      struct snd_pcm_hw_rule *rule)
{
	unsigned int list[3];
	struct hdspe *hdspe = rule->private;
	struct snd_interval *c = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	list[0] = hdspe->t.qs_in_channels;
	list[1] = hdspe->t.ds_in_channels;
	list[2] = hdspe->t.ss_in_channels;
	return snd_interval_list(c, 3, list, 0);
}

static int snd_hdspe_hw_rule_out_channels(struct snd_pcm_hw_params *params,
				      struct snd_pcm_hw_rule *rule)
{
	unsigned int list[3];
	struct hdspe *hdspe = rule->private;
	struct snd_interval *c = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	list[0] = hdspe->t.qs_out_channels;
	list[1] = hdspe->t.ds_out_channels;
	list[2] = hdspe->t.ss_out_channels;
	return snd_interval_list(c, 3, list, 0);
}


static const unsigned int hdspe_aes_sample_rates[] = {
	32000, 44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000
};

static const struct snd_pcm_hw_constraint_list
hdspe_hw_constraints_aes_sample_rates = {
	.count = ARRAY_SIZE(hdspe_aes_sample_rates),
	.list = hdspe_aes_sample_rates,
	.mask = 0
};

static int snd_hdspe_open(struct snd_pcm_substream *substream)
{
	struct hdspe *hdspe = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	bool playback = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);

	spin_lock_irq(&hdspe->lock);
	snd_pcm_set_sync(substream);
	runtime->hw = (playback) ? snd_hdspe_playback_subinfo :
		snd_hdspe_capture_subinfo;

	if (playback) {
		if (!hdspe->capture_substream)
			hdspe_stop_audio(hdspe);

		hdspe->playback_pid = current->pid;
		hdspe->playback_substream = substream;
	} else {
		if (!hdspe->playback_substream)
			hdspe_stop_audio(hdspe);

		hdspe->capture_pid = current->pid;
		hdspe->capture_substream = substream;
	}

	spin_unlock_irq(&hdspe->lock);

	snd_pcm_hw_constraint_msbits(runtime, 0, 32, 24);
	snd_pcm_hw_constraint_pow2(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);

	switch (hdspe->io_type) {
	case HDSPE_AIO:		
	case HDSPE_RAYDAT:
	case HDSPE_AIO_PRO:		
		snd_pcm_hw_constraint_minmax(runtime,
					     SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
					     32, 4096);
		/* RayDAT & AIO have a fixed buffer of 16384 samples per channel */
		snd_pcm_hw_constraint_single(runtime,
					     SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
					     16384);
		break;

	default:
		snd_pcm_hw_constraint_minmax(runtime,
					     SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
					     64, 8192);
		snd_pcm_hw_constraint_single(runtime,
					     SNDRV_PCM_HW_PARAM_PERIODS, 2);
		break;
	}

	if (HDSPE_AES == hdspe->io_type) {
		runtime->hw.rates |= SNDRV_PCM_RATE_KNOT;
		snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				&hdspe_hw_constraints_aes_sample_rates);
	} else {
		snd_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				(playback ?
				 snd_hdspe_hw_rule_rate_out_channels :
				 snd_hdspe_hw_rule_rate_in_channels), hdspe,
				SNDRV_PCM_HW_PARAM_CHANNELS, -1);
	}

	snd_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
			(playback ? snd_hdspe_hw_rule_out_channels :
			 snd_hdspe_hw_rule_in_channels), hdspe,
			SNDRV_PCM_HW_PARAM_CHANNELS, -1);

	snd_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
			(playback ? snd_hdspe_hw_rule_out_channels_rate :
			 snd_hdspe_hw_rule_in_channels_rate), hdspe,
			SNDRV_PCM_HW_PARAM_RATE, -1);

	return 0;
}

static int snd_hdspe_release(struct snd_pcm_substream *substream)
{
	struct hdspe *hdspe = snd_pcm_substream_chip(substream);
	bool playback = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);

	spin_lock_irq(&hdspe->lock);

	if (playback) {
		hdspe->playback_pid = -1;
		hdspe->playback_substream = NULL;
	} else {
		hdspe->capture_pid = -1;
		hdspe->capture_substream = NULL;
	}

	spin_unlock_irq(&hdspe->lock);

	return 0;
}

static const struct snd_pcm_ops snd_hdspe_ops = {
	.open = snd_hdspe_open,
	.close = snd_hdspe_release,
	.ioctl = snd_hdspe_ioctl,
	.hw_params = snd_hdspe_hw_params,
	.hw_free = snd_hdspe_hw_free,
	.prepare = snd_hdspe_prepare,
	.trigger = snd_hdspe_trigger,
	.pointer = snd_hdspe_hw_pointer,
	/* TODO: .get_time_info = snd_hdspe_get_time_info */
};

int snd_hdspe_create_pcm(struct snd_card *card,
			 struct hdspe *hdspe)
{
	struct snd_pcm *pcm;
	int err;

	hdspe->playback_pid = -1;
	hdspe->capture_pid = -1;
	hdspe->capture_substream = NULL;
	hdspe->playback_substream = NULL;

	err = snd_pcm_new(card, hdspe->card_name, 0, 1, 1, &pcm);
	if (err < 0)
		return err;

	hdspe->pcm = pcm;
	pcm->private_data = hdspe;
	strcpy(pcm->name, hdspe->card_name);

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_hdspe_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,
			&snd_hdspe_ops);

	pcm->info_flags = SNDRV_PCM_INFO_JOINT_DUPLEX;

	err = snd_hdspe_preallocate_memory(hdspe);
	if (err < 0)
		return err;

	return 0;
}

