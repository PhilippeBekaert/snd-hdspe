// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe-mixer.c
 * @brief RME HDSPe hardware mixer status and control interface.
 *
 * 20210728,1206,07 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORS,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

#include "hdspe.h"
#include "hdspe_core.h"
#include "hdspe_control.h"

#include <linux/slab.h>


/* for each output channel (chan) I have an Input (in) and Playback (pb) Fader
   mixer is write only on hardware so we have to cache him for read
   each fader is a u32, but uses only the first 16 bit */

static inline u16 hdspe_read_in_gain(struct hdspe * hdspe, unsigned int chan,
				     unsigned int in)
{
	if (chan >= HDSPE_MIXER_CHANNELS || in >= HDSPE_MIXER_CHANNELS)
		return 0;

	return hdspe->mixer->ch[chan].in[in];
}

static inline u16 hdspe_read_pb_gain(struct hdspe * hdspe, unsigned int chan,
				     unsigned int pb)
{
	if (chan >= HDSPE_MIXER_CHANNELS || pb >= HDSPE_MIXER_CHANNELS)
		return 0;
	return hdspe->mixer->ch[chan].pb[pb];
}

static int hdspe_write_in_gain(struct hdspe *hdspe, unsigned int chan,
				      unsigned int in, u16 data)
{
	if (chan >= HDSPE_MIXER_CHANNELS || in >= HDSPE_MIXER_CHANNELS)
		return -1;

	hdspe_write(hdspe,
		    HDSPE_MADI_mixerBase +
		    ((in + 128 * chan) * sizeof(u32)),
		    cpu_to_le32(hdspe->mixer->ch[chan].in[in] = data & 0xFFFF));
	return 0;
}

static int hdspe_write_pb_gain(struct hdspe *hdspe, unsigned int chan,
				      unsigned int pb, u16 data)
{
	if (chan >= HDSPE_MIXER_CHANNELS || pb >= HDSPE_MIXER_CHANNELS)
		return -1;

	hdspe_write(hdspe,
		    HDSPE_MADI_mixerBase +
		    ((64 + pb + 128 * chan) * sizeof(u32)),
		    (hdspe->mixer->ch[chan].pb[pb] = data & 0xFFFF));
	return 0;
}

void hdspe_mixer_read_proc(struct snd_info_entry *entry,
			       struct snd_info_buffer *buffer)
{
	struct hdspe *hdspe = entry->private_data;
	int i, j;

	snd_iprintf(buffer, "Capture Volume:\n");
	snd_iprintf(buffer, "    ");
	for (j = 0; j < HDSPE_MIXER_CHANNELS; j++)
		snd_iprintf(buffer, "   %02u ", j);
	snd_iprintf(buffer, "\n");
	
	for (i = 0; i < HDSPE_MIXER_CHANNELS; i++) {
		snd_iprintf(buffer, "%02d: ", i);
		for (j = 0; j < HDSPE_MIXER_CHANNELS; j++)
			snd_iprintf(buffer, "%5u ",
				   hdspe_read_in_gain(hdspe, i, j));
		snd_iprintf(buffer, "\n");		
	}

	snd_iprintf(buffer, "\nPlayback Volume:\n");
	snd_iprintf(buffer, "    ");
	for (j = 0; j < HDSPE_MIXER_CHANNELS; j++)
		snd_iprintf(buffer, "   %02u ", j);
	snd_iprintf(buffer, "\n");
	
	for (i = 0; i < HDSPE_MIXER_CHANNELS; i++) {
		snd_iprintf(buffer, "%02d: ", i);
		for (j = 0; j < HDSPE_MIXER_CHANNELS; j++)
			snd_iprintf(buffer, "%5u ",
				   hdspe_read_pb_gain(hdspe, i, j));
		snd_iprintf(buffer, "\n");		
	}
}

void hdspe_mixer_update_channel_map(struct hdspe* hdspe)
{
	int i, j;
	bool used[HDSPE_MAX_CHANNELS];

	dev_dbg(hdspe->card->dev, "%s:\n", __func__);
	
	/* mute all unused playback channels */
	for (i = 0; i < HDSPE_MIXER_CHANNELS; i ++) {
		used[i] = false;
	}
	for (i = 0; i < HDSPE_MIXER_CHANNELS; i ++) {
		int c = hdspe->channel_map_out[i];
		used[c] = true;
	}
	for (i = 0; i < HDSPE_MIXER_CHANNELS; i ++) {
		if (!used[i]) {
			for (j = 0; j < HDSPE_MIXER_CHANNELS; j ++) {
				hdspe_write_in_gain(hdspe, i, j, 0);
				hdspe_write_pb_gain(hdspe, i, j, 0);
			}
		} else {
#ifdef DAW_MODE
			hdspe_write_pb_gain(hdspe, i, i, HDSPE_UNITY_GAIN);
#endif /*DAW_MODE*/
		}
	}
	
	/* mute all unused capture channels */
	for (i = 0; i < HDSPE_MIXER_CHANNELS; i ++) {
		used[i] = false;
	}
	for (i = 0; i < HDSPE_MIXER_CHANNELS; i ++) {
		int c = hdspe->channel_map_in[i];
		used[c] = true;
	}
	for (i = 0; i < HDSPE_MIXER_CHANNELS; i ++) {	
		if (!used[i]) {
			for (j = 0; j < HDSPE_MIXER_CHANNELS; j ++) {
				hdspe_write_in_gain(hdspe, j, i, 0);
			}
		} else {
#ifdef PASSTHROUGH_MODE
			hdspe_write_in_gain(hdspe, i, i, HDSPE_UNITY_GAIN);
#endif /*PASSTHROUGH_MODE*/
		}		
	}
}

static void hdspe_clear_mixer(struct hdspe * hdspe, u16 sgain)
{
	int i, j;
	unsigned int gain;

	if (sgain > HDSPE_UNITY_GAIN)
		gain = HDSPE_UNITY_GAIN;
	else if (sgain < 0)
		gain = 0;
	else
		gain = sgain;

	for (i = 0; i < HDSPE_MIXER_CHANNELS; i++)
		for (j = 0; j < HDSPE_MIXER_CHANNELS; j++) {
			hdspe_write_in_gain(hdspe, i, j, gain);
			hdspe_write_pb_gain(hdspe, i, j, gain);
		}	
}

#define HDSPE_MIXER(xname, xindex) \
{	.iface = SNDRV_CTL_ELEM_IFACE_HWDEP, \
	.name = xname, \
	.index = xindex, \
	.device = 0, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE | \
		SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
	.info = snd_hdspe_info_mixer, \
	.get = snd_hdspe_get_mixer, \
	.put = snd_hdspe_put_mixer \
}

static int snd_hdspe_info_mixer(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 3;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65535;
	uinfo->value.integer.step = 1;
	return 0;
}

static int snd_hdspe_get_mixer(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	int source;
	int destination;

	source = ucontrol->value.integer.value[0];
	if (source < 0)
		source = 0;
	else if (source >= 2 * HDSPE_MAX_CHANNELS)
		source = 2 * HDSPE_MAX_CHANNELS - 1;

	destination = ucontrol->value.integer.value[1];
	if (destination < 0)
		destination = 0;
	else if (destination >= HDSPE_MAX_CHANNELS)
		destination = HDSPE_MAX_CHANNELS - 1;

	spin_lock_irq(&hdspe->lock);
	if (source >= HDSPE_MAX_CHANNELS)
		ucontrol->value.integer.value[2] =
		    hdspe_read_pb_gain(hdspe, destination,
				       source - HDSPE_MAX_CHANNELS);
	else
		ucontrol->value.integer.value[2] =
		    hdspe_read_in_gain(hdspe, destination, source);

	spin_unlock_irq(&hdspe->lock);

	return 0;
}

static int snd_hdspe_put_mixer(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	int change;
	int source;
	int destination;
	int gain;

	if (!snd_hdspe_use_is_exclusive(hdspe))
		return -EBUSY;

	source = ucontrol->value.integer.value[0];
	destination = ucontrol->value.integer.value[1];

	if (source < 0 || source >= 2 * HDSPE_MAX_CHANNELS)
		return -1;
	if (destination < 0 || destination >= HDSPE_MAX_CHANNELS)
		return -1;

	gain = ucontrol->value.integer.value[2];

	spin_lock_irq(&hdspe->lock);

	if (source >= HDSPE_MAX_CHANNELS)
		change = gain != hdspe_read_pb_gain(hdspe, destination,
						    source -
						    HDSPE_MAX_CHANNELS);
	else
		change = gain != hdspe_read_in_gain(hdspe, destination,
						    source);

	if (change) {
		if (source >= HDSPE_MAX_CHANNELS)
			hdspe_write_pb_gain(hdspe, destination,
					    source - HDSPE_MAX_CHANNELS,
					    gain);
		else
			hdspe_write_in_gain(hdspe, destination, source,
					    gain);
	}
	spin_unlock_irq(&hdspe->lock);

	return change;
}

/* The simple mixer control(s) provide gain control for the
   basic 1:1 mappings of playback streams to output
   streams.
*/

#define HDSPE_PLAYBACK_MIXER \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_WRITE | \
		SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
	.info = snd_hdspe_info_playback_mixer, \
	.get = snd_hdspe_get_playback_mixer, \
	.put = snd_hdspe_put_playback_mixer \
}

static int snd_hdspe_info_playback_mixer(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 64;
	uinfo->value.integer.step = 1;
	return 0;
}

static int snd_hdspe_get_playback_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	int channel;

	channel = ucontrol->id.index - 1;

	if (snd_BUG_ON(channel < 0 || channel >= HDSPE_MAX_CHANNELS))
		return -EINVAL;

	spin_lock_irq(&hdspe->lock);
	ucontrol->value.integer.value[0] =
	  (hdspe_read_pb_gain(hdspe, channel, channel)*64)/HDSPE_UNITY_GAIN;
	spin_unlock_irq(&hdspe->lock);

	return 0;
}

static int snd_hdspe_put_playback_mixer(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	int change;
	int channel;
	int gain;

	if (!snd_hdspe_use_is_exclusive(hdspe))
		return -EBUSY;

	channel = ucontrol->id.index - 1;

	if (snd_BUG_ON(channel < 0 || channel >= HDSPE_MAX_CHANNELS))
		return -EINVAL;

	gain = ucontrol->value.integer.value[0]*HDSPE_UNITY_GAIN/64;

	spin_lock_irq(&hdspe->lock);
	change =
	    gain != hdspe_read_pb_gain(hdspe, channel,
				       channel);
	if (change)
		hdspe_write_pb_gain(hdspe, channel, channel,
				    gain);
	spin_unlock_irq(&hdspe->lock);
	return change;
}

static struct snd_kcontrol_new snd_hdspe_playback_mixer = HDSPE_PLAYBACK_MIXER;

static int hdspe_update_simple_mixer_controls(struct hdspe * hdspe)
{
	int i;

	dev_dbg(hdspe->card->dev, "Update mixer controls...\n");
	// TODO: what about quad speed nr of channels??
	for (i = hdspe->t.ds_out_channels; i < hdspe->t.ss_out_channels; ++i) {
		if (hdspe_speed_mode(hdspe) > HDSPE_SPEED_SINGLE) {
			hdspe->playback_mixer_ctls[i]->vd[0].access =
				SNDRV_CTL_ELEM_ACCESS_INACTIVE |
				SNDRV_CTL_ELEM_ACCESS_READ |
				SNDRV_CTL_ELEM_ACCESS_VOLATILE;
		} else {
			hdspe->playback_mixer_ctls[i]->vd[0].access =
				SNDRV_CTL_ELEM_ACCESS_READWRITE |
				SNDRV_CTL_ELEM_ACCESS_VOLATILE;
		}
		snd_ctl_notify(hdspe->card, SNDRV_CTL_EVENT_MASK_VALUE |
				SNDRV_CTL_EVENT_MASK_INFO,
				&hdspe->playback_mixer_ctls[i]->id);
	}

	return 0;
}

static int hdspe_nr_out_channels(struct hdspe* hdspe)
{
	switch (hdspe_speed_mode(hdspe)) {
	case HDSPE_SPEED_SINGLE: return hdspe->t.ss_out_channels;
	case HDSPE_SPEED_DOUBLE: return hdspe->t.ds_out_channels;
	case HDSPE_SPEED_QUAD  : return hdspe->t.qs_out_channels;
	default:
		snd_BUG();
	}
	return 0;
}

static const struct snd_kcontrol_new snd_hdspe_controls_mixer[] = {
	HDSPE_MIXER("Mixer", 0)
};

int hdspe_create_mixer_controls(struct hdspe* hdspe)
{
	unsigned int idx, limit;
	int err;
	struct snd_kcontrol *kctl;

	err = hdspe_add_controls(hdspe, ARRAY_SIZE(snd_hdspe_controls_mixer),
				 snd_hdspe_controls_mixer);
	if (err < 0)
		return err;

 	/* create simple 1:1 playback mixer controls */
	snd_hdspe_playback_mixer.name = "Chn";
	limit = hdspe_nr_out_channels(hdspe);
	for (idx = 0; idx < limit; ++idx) {
		snd_hdspe_playback_mixer.index = idx + 1;
		kctl = snd_ctl_new1(&snd_hdspe_playback_mixer, hdspe);
		err = snd_ctl_add(hdspe->card, kctl);
		if (err < 0)
			return err;
		hdspe->playback_mixer_ctls[idx] = kctl;
	}

	hdspe_update_simple_mixer_controls(hdspe);
	
	return 0;
}

int hdspe_init_mixer(struct hdspe* hdspe)
{
	dev_dbg(hdspe->card->dev, "kmalloc Mixer memory of %zd Bytes\n",
		sizeof(*hdspe->mixer));
	hdspe->mixer = kzalloc(sizeof(*hdspe->mixer), GFP_KERNEL);
	if (!hdspe->mixer)
		return -ENOMEM;
	
	hdspe_clear_mixer(hdspe, 0 * HDSPE_UNITY_GAIN);
	
	return 0;
}

void hdspe_terminate_mixer(struct hdspe* hdspe)
{
        kfree(hdspe->mixer);	
}
