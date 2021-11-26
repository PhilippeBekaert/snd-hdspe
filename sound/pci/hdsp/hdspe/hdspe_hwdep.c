// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe_hwdep.c
 * @brief RME HDSPe driver HWDEP interface.
 *
 * 20210728 - Philippe.Bekaert@uhasselt.be
 * 20210810,12 - PhB : new card info ioctl.
 * 20211125 - PhB : IOCTL_GET_CONFIG reimplemented in terms of hdspe_status.
 *
 * Refactored work of the other MODULE_AUTHORs.
 */

#include "hdspe.h"
#include "hdspe_core.h"

#include <sound/hwdep.h>

#ifdef OLDSTUFF
/* AutoSync external sync source frequency class. Returns 0 if
 * no valid external reference. */
static enum hdspe_freq hdspe_autosync_freq(struct hdspe *hdspe)
{
	struct hdspe_status status;
	hdspe->m.read_status(hdspe, &status);
	return status.external_freq;
}
#endif /*OLDSTUFF*/

static int snd_hdspe_hwdep_dummy_op(struct snd_hwdep *hw, struct file *file)
{
	/* we have nothing to initialize but the call is required */
	return 0;
}

static inline int copy_u32_le(void __user *dest, void __iomem *src)
{
	u32 val = readl(src);
	return copy_to_user(dest, &val, 4);
}

void hdspe_get_card_info(struct hdspe* hdspe, struct hdspe_card_info *s)
{
	s->version = HDSPE_VERSION;
	s->card_type = hdspe->io_type;
	s->serial = hdspe->serial;
	s->fw_rev = hdspe->firmware_rev;
	s->fw_build = hdspe->fw_build;
	s->irq = hdspe->irq;
	s->port = hdspe->port;
	s->vendor_id = hdspe->vendor_id;
	s->expansion = 0;
	if (hdspe->tco)
		s->expansion |= HDSPE_EXPANSION_TCO;
}

static int snd_hdspe_hwdep_ioctl(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct hdspe *hdspe = hw->private_data;
	struct hdspe_mixer_ioctl mixer;
	struct hdspe_config info;
	struct hdspe_version hdspe_version;
	struct hdspe_peak_rms *levels;
#ifdef OLDSTUFF	
	struct hdspe_ltc ltc;
#endif /*OLDSTUFF*/
	struct hdspe_status status;
	struct hdspe_card_info card_info;
	struct hdspe_tco_status tco_status;
	long unsigned int s;
	int i = 0;

	switch (cmd) {

	case SNDRV_HDSPE_IOCTL_GET_CARD_INFO:
		hdspe->m.get_card_info(hdspe, &card_info);
		if (copy_to_user(argp, &card_info,
				 sizeof(struct hdspe_card_info)))
			return -EFAULT;
		break;

	case SNDRV_HDSPE_IOCTL_GET_STATUS:
		hdspe->m.read_status(hdspe, &status);
		if (copy_to_user(argp, &status, sizeof(struct hdspe_status)))
			return -EFAULT;
		break;

	case SNDRV_HDSPE_IOCTL_GET_LTC:
		if (!hdspe->tco) {
			dev_dbg(hdspe->card->dev, "%s: %d: EINVAL\n", __func__, __LINE__);
			return -EINVAL;
		} else {
			hdspe_tco_read_status(hdspe, &tco_status);
		}
		if (copy_to_user(argp, &tco_status,
				 sizeof(struct hdspe_tco_status)))
			return -EFAULT;
		break;
		
	case SNDRV_HDSPE_IOCTL_GET_PEAK_RMS:
		levels = &hdspe->peak_rms;
		for (i = 0; i < HDSPE_MAX_CHANNELS; i++) {
			levels->input_peaks[i] =
				readl(hdspe->iobase +
						HDSPE_MADI_INPUT_PEAK + i*4);
			levels->playback_peaks[i] =
				readl(hdspe->iobase +
						HDSPE_MADI_PLAYBACK_PEAK + i*4);
			levels->output_peaks[i] =
				readl(hdspe->iobase +
						HDSPE_MADI_OUTPUT_PEAK + i*4);

			levels->input_rms[i] =
				((uint64_t) readl(hdspe->iobase +
					HDSPE_MADI_INPUT_RMS_H + i*4) << 32) |
				(uint64_t) readl(hdspe->iobase +
						HDSPE_MADI_INPUT_RMS_L + i*4);
			levels->playback_rms[i] =
				((uint64_t)readl(hdspe->iobase +
					HDSPE_MADI_PLAYBACK_RMS_H+i*4) << 32) |
				(uint64_t)readl(hdspe->iobase +
					HDSPE_MADI_PLAYBACK_RMS_L + i*4);
			levels->output_rms[i] =
				((uint64_t)readl(hdspe->iobase +
					HDSPE_MADI_OUTPUT_RMS_H + i*4) << 32) |
				(uint64_t)readl(hdspe->iobase +
						HDSPE_MADI_OUTPUT_RMS_L + i*4);
		}

		levels->speed = hdspe_speed_mode(hdspe);
		levels->status2 = hdspe_read_status2(hdspe).raw;

		s = copy_to_user(argp, levels, sizeof(*levels));
		if (0 != s) {
			/* dev_err(hdspe->card->dev, "copy_to_user(.., .., %lu): %lu
			 [Levels]\n", sizeof(struct hdspe_peak_rms), s);
			 */
			return -EFAULT;
		}
		break;

#ifdef OLDSTUFF	  
	case SNDRV_HDSPE_IOCTL_GET_LTC:
		ltc.ltc = hdspe_read(hdspe, HDSPE_RD_TCO);
		i = hdspe_read(hdspe, HDSPE_RD_TCO + 4);
		if (i & HDSPE_TCO1_LTC_Input_valid) {
			switch (i & (HDSPE_TCO1_LTC_Format_LSB |
				HDSPE_TCO1_LTC_Format_MSB)) {
			case 0:
				ltc.format = fps_24;
				break;
			case HDSPE_TCO1_LTC_Format_LSB:
				ltc.format = fps_25;
				break;
			case HDSPE_TCO1_LTC_Format_MSB:
				ltc.format = fps_2997;
				break;
			default:
				ltc.format = fps_30;
				break;
			}
			if (i & HDSPE_TCO1_set_drop_frame_flag) {
				ltc.frame = drop_frame;
			} else {
				ltc.frame = full_frame;
			}
		} else {
			ltc.format = format_invalid;
			ltc.frame = frame_invalid;
		}
		if (i & HDSPE_TCO1_Video_Input_Format_NTSC) {
			ltc.input_format = ntsc;
		} else if (i & HDSPE_TCO1_Video_Input_Format_PAL) {
			ltc.input_format = pal;
		} else {
			ltc.input_format = no_video;
		}

		s = copy_to_user(argp, &ltc, sizeof(ltc));
		if (0 != s) {
			/*
			  dev_err(hdspe->card->dev, "copy_to_user(.., .., %lu): %lu [LTC]\n", sizeof(struct hdspe_ltc), s); */
			return -EFAULT;
		}
#endif /*OLDSTUFF*/
		dev_warn(hdspe->card->dev, "IOCTL_GET_LTC: TBD.\n");

		break;

	case SNDRV_HDSPE_IOCTL_GET_CONFIG:

		memset(&info, 0, sizeof(info));
		spin_lock_irq(&hdspe->lock);
		hdspe->m.read_status(hdspe, &status);
		info.pref_sync_ref = status.preferred_ref;
		info.wordclock_sync_check =
			status.sync[HDSPE_CLOCK_SOURCE_WORD];
		snd_BUG_ON(status.sample_rate_denominator == 0);
		info.system_sample_rate = div_u64(status.sample_rate_numerator,
			status.sample_rate_denominator);
		info.autosync_sample_rate =
			hdspe_freq_sample_rate(status.external_freq);
		info.system_clock_mode = status.clock_mode;
		info.clock_source = status.internal_freq;
		info.autosync_ref = status.autosync_ref;
		info.line_out = hdspe->reg.control.common.LineOut;
		info.passthru = 0;

#ifdef OLDSTUFF
		info.pref_sync_ref = hdspe->m.get_pref_sync_ref(hdspe);
		info.wordclock_sync_check = hdspe->m.get_sync_status(
			hdspe, HDSPE_CLOCK_SOURCE_WORD);

		info.system_sample_rate = hdspe_read_system_sample_rate(hdspe);
		info.autosync_sample_rate = hdspe_freq_sample_rate(
			hdspe_autosync_freq(hdspe));
		info.system_clock_mode = hdspe->m.get_clock_mode(hdspe);
		info.clock_source = hdspe_internal_freq(hdspe);
		info.autosync_ref = hdspe->m.get_autosync_ref(hdspe);
		info.line_out = hdspe->reg.control.common.LineOut;
		info.passthru = 0;
#endif /*OLDSTUFF*/
		spin_unlock_irq(&hdspe->lock);
		if (copy_to_user(argp, &info, sizeof(info)))
			return -EFAULT;
		break;

	case SNDRV_HDSPE_IOCTL_GET_VERSION:
		memset(&hdspe_version, 0, sizeof(hdspe_version));

		hdspe_version.card_type = hdspe->io_type;
		strscpy(hdspe_version.cardname, hdspe->card_name,
				sizeof(hdspe_version.cardname));
		hdspe_version.serial = hdspe->serial;
		hdspe_version.firmware_rev = hdspe->firmware_rev;
		hdspe_version.addons = 0;
		if (hdspe->tco)
			hdspe_version.addons |= HDSPE_ADDON_TCO;

		if (copy_to_user(argp, &hdspe_version,
					sizeof(hdspe_version)))
			return -EFAULT;
		break;

	case SNDRV_HDSPE_IOCTL_GET_MIXER:
		if (copy_from_user(&mixer, argp, sizeof(mixer)))
			return -EFAULT;
		if (copy_to_user((void __user *)mixer.mixer, hdspe->mixer,
				 sizeof(*mixer.mixer)))
			return -EFAULT;
		break;

	default:
		dev_dbg(hdspe->card->dev, "%s: %d: cmd=%u EINVAL\n", __func__, __LINE__, cmd);
		return -EINVAL;
	}
	return 0;
}

int snd_hdspe_create_hwdep(struct snd_card *card,
			   struct hdspe *hdspe)
{
	struct snd_hwdep *hw;
	int err;

	err = snd_hwdep_new(card, "HDSPE hwdep", 0, &hw);
	if (err < 0)
		return err;

	hdspe->hwdep = hw;
	hw->private_data = hdspe;
	strcpy(hw->name, "HDSPE hwdep interface");

	hw->ops.open = snd_hdspe_hwdep_dummy_op;
	hw->ops.ioctl = snd_hdspe_hwdep_ioctl;
	hw->ops.ioctl_compat = snd_hdspe_hwdep_ioctl;
	hw->ops.release = snd_hdspe_hwdep_dummy_op;

	return 0;
}



