// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe-control.c
 * @brief RME HDSPe sound card driver status and control interface.
 *
 * 20210727,28,29,30,0908,09,10 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORS,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

/* TODO: systematic value range checking */

#include "hdspe.h"
#include "hdspe_core.h"
#include "hdspe_control.h"

/**
 * hdspe_init_autosync_tables: calculates tables needed for the 
 * preferred sync and autosync ref properties below, given the list of 
 * autosync clock sources for a card:
 * Tables calculated:
 * - hdspe::t.autosync_texts : a text label for each clock source,
 * plus HDSPE_CLOCK_SOURCE_INTERN at the end of the list.
 * - hdspe::t.autosync_idx2ref : maps a property index to a 
 * hdspe_clock_source enum.
 * - hdspe::t.autosync_ref2idx : maps a hdspe_clock_source enum to a
 * property index.
 * The number of clock sources, including HDSPE_CLOCK_SOURCE_INTERN,
 * is stored in hdspe::t.autosync_count. 
 * @nr_autosync_opts: the number of clock sources (size of autosync_opts array) 
 * @autosync_opts: the autosync clock sources offered by the card. */
void hdspe_init_autosync_tables(struct hdspe* hdspe,
				int nr_autosync_opts,
				enum hdspe_clock_source* autosync_opts)
{
	int i, n;
	struct hdspe_tables *t = &hdspe->t;

	/* Clear the tables to sane defaults. */
	for (i=0; i<HDSPE_CLOCK_SOURCE_COUNT; i++) {
		t->autosync_texts[i] = "";
		t->autosync_idx2ref[i] = 0;
		t->autosync_ref2idx[i] = 0;
	}

	for (i=0, n=0; i<nr_autosync_opts; i++) {
		enum hdspe_clock_source ref = autosync_opts[i];
//		if (ref == HDSPE_CLOCK_SOURCE_TCO && !hdspe->tco)
//			continue;  // do not offer TCO option if not present
		if (ref == HDSPE_CLOCK_SOURCE_INTERN)
			continue;  // skip unused codes
		t->autosync_texts[n] = hdspe_clock_source_name(hdspe, ref);
		t->autosync_idx2ref[n] = ref;
		t->autosync_ref2idx[ref] = n;
		n++;
	}

	// Add HDSPE_CLOCK_SOURCE_INTERN as the last option
	// for Current AutoSync Reference property.
	t->autosync_texts[n] = hdspe_clock_source_name(hdspe,
					       HDSPE_CLOCK_SOURCE_INTERN);
	t->autosync_idx2ref[n] = HDSPE_CLOCK_SOURCE_INTERN;
	t->autosync_ref2idx[HDSPE_CLOCK_SOURCE_INTERN] = n;
	n++;

	t->autosync_count = n;

	dev_dbg(hdspe->card->dev, "AutoSync tables: %d clock sources:\n", t->autosync_count);
	for (i=0; i<HDSPE_CLOCK_SOURCE_COUNT; i++) {
		dev_dbg(hdspe->card->dev, "Idx %2d idx2ref=%d texts='%s'\n",
			i, t->autosync_idx2ref[i], 
			t->autosync_texts[i]);
	}
	for (i=0; i<HDSPE_CLOCK_SOURCE_COUNT; i++) {
		dev_dbg(hdspe->card->dev, "Ref %2d '%s' ref2idx=%d\n",
			i, hdspe_clock_source_name(hdspe, i),
			t->autosync_ref2idx[i]);
	}
}

const char* const hdspe_clock_source_name(struct hdspe* hdspe, int i)
{
	return (i >= 0 && i < HDSPE_CLOCK_SOURCE_COUNT)
		? hdspe->t.clock_source_names[i] : "???";
}

/* ------------------------------------------------------- */

int hdspe_control_get(struct snd_kcontrol *kcontrol,
		      int (*get)(struct hdspe*),
		      bool lock_req, const char* propname)
{
	int val;
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	dev_dbg(hdspe->card->dev, "hdspe_control_get(%s) ...\n", propname);
	if (lock_req) spin_lock_irq(&hdspe->lock);
	val = get(hdspe);
	if (lock_req) spin_unlock_irq(&hdspe->lock);
	dev_dbg(hdspe->card->dev, "... = %d.\n", val);
	return val;
}

int hdspe_control_put(struct snd_kcontrol *kcontrol, int val,
		      int (*get)(struct hdspe*),
		      int (*put)(struct hdspe*, int val),
		      bool lock_req, bool excl_req, const char* propname)
{
	int oldval = -1, changed = 0, rc = 0;
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	if (excl_req && !snd_hdspe_use_is_exclusive(hdspe)) {
		dev_dbg(hdspe->card->dev,
			"snd_hdspe_put(%s,%d): no exclusive access!\n",
			propname, val);
		return -EBUSY;
	}
	dev_dbg(hdspe->card->dev, "snd_hdspe_put(%s,%d) %s get() ...\n",
		propname, val, get ? "with" : "without");
	if (lock_req) spin_lock_irq(&hdspe->lock);
	oldval = get ? get(hdspe) : val;
	changed = (val != oldval);
	if (!get || changed)
		rc = put(hdspe, val);
	if (lock_req) spin_unlock_irq(&hdspe->lock);
	dev_dbg(hdspe->card->dev,
		"... val = %d, oldval = %d, changed = %d, put rc = %d.\n",
		val, oldval, changed, rc);
	return rc ? rc : changed;
}

/* -------------- status polling ------------------- */

HDSPE_RW_INT1_HDSPE_METHODS(status_polling, 0, HZ, 1)

void hdspe_status_work(struct work_struct *work)
{
	struct hdspe *hdspe = container_of(work, struct hdspe, status_work);
	int64_t sr_delta;
	int i;
	bool changed = false;
	struct hdspe_status o = hdspe->last_status;
	struct hdspe_status n;
	hdspe->m.read_status(hdspe, &n);

	for (i = 0; i < HDSPE_CLOCK_SOURCE_COUNT; i++) {
		if (n.sync[i] != o.sync[i]) {
			dev_dbg(hdspe->card->dev,
				"sync source %d status changed %d -> %d.\n",
				i, o.sync[i], n.sync[i]);
			HDSPE_CTL_NOTIFY(autosync_status);
			changed = true;
			break;
		}
	}
	
	for (i = 0; i < HDSPE_CLOCK_SOURCE_COUNT; i++) {
		if (n.freq[i] != o.freq[i]) {
			dev_dbg(hdspe->card->dev,
				"sync source %d freq changed %d -> %d.\n",
				i, o.freq[i], n.freq[i]);
			HDSPE_CTL_NOTIFY(autosync_freq);
			changed = true;
			break;
		}
	}
	
	if (n.autosync_ref != o.autosync_ref) {
		dev_dbg(hdspe->card->dev, "autosync ref changed %d -> %d.\n",
			o.autosync_ref, n.autosync_ref);
		HDSPE_CTL_NOTIFY(autosync_ref);
		changed = true;
	}
	
	if (n.external_freq != o.external_freq && hdspe->cid.external_freq) {
		dev_dbg(hdspe->card->dev, "external freq changed %d -> %d.\n",
			o.external_freq, n.external_freq);		
		HDSPE_CTL_NOTIFY(external_freq);
		changed = true;
	}

	sr_delta = (int64_t)n.sample_rate_denominator
		 - (int64_t)o.sample_rate_denominator;
	if (n.sample_rate_numerator != o.sample_rate_numerator ||
	    (sr_delta<0 ? -sr_delta : sr_delta) >
	    (n.sample_rate_denominator / 1000000)) {
		dev_dbg(hdspe->card->dev,
			"sample rate changed %llu/%u -> %llu/%u.\n",
			o.sample_rate_numerator, o.sample_rate_denominator,
			n.sample_rate_numerator, n.sample_rate_denominator);
		HDSPE_CTL_NOTIFY(raw_sample_rate);
		changed = true;
	}

	// TODO: madi / aes specific status changes

	if (hdspe->tco && hdspe_tco_notify_status_change(hdspe)) {
		changed = true;
	}
	
	if (changed || jiffies > hdspe->last_status_jiffies * 2*HZ) {
		hdspe->last_status = n;
		hdspe->status_polling = 0; /* disable - user must re-enable */
		HDSPE_CTL_NOTIFY(status_polling);
	}
}


/* ------------------ raw sample rate and DDS -------------------- */

static int snd_hdspe_info_raw_sample_rate(struct snd_kcontrol* kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER64;
	uinfo->count = 2;
	return 0;
}

static int snd_hdspe_get_raw_sample_rate(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	struct hdspe_status s;
	hdspe_read_sample_rate_status(hdspe, &s);
	ucontrol->value.integer64.value[0] = s.sample_rate_numerator;
	ucontrol->value.integer64.value[1] = s.sample_rate_denominator;
	return 0;
}

static int snd_hdspe_info_dds(struct snd_kcontrol* kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);	
	u32 ddsmin, ddsmax;
	hdspe_dds_range(hdspe, &ddsmin, &ddsmax);
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = ddsmin;
	uinfo->value.integer.max = ddsmax;
	return 0;
}

static int hdspe_write_dds_i(struct hdspe* hdspe, int val)
{
	return hdspe_write_dds(hdspe, (u32)val);
}

HDSPE_INT1_GET(dds, hdspe_get_dds, true)
HDSPE_INT1_PUT(dds, NULL, hdspe_write_dds_i, true, false)

#ifdef OLDSTUFF
/* -------------- system sample rate ---------------- */

HDSPE_RO_INT1_METHODS(sample_rate, 27000, 207000, 1,
		     hdspe_read_system_sample_rate)

HDSPE_RO_INT1_METHODS(pitch, 650000, 1350000, 1,
		     hdspe_read_system_pitch)

HDSPE_RW_INT1_METHODS(internal_pitch, 950000, 1050000, 1,
		     hdspe_internal_pitch, hdspe_write_internal_pitch,
		     false);
#endif /*OLDSTUFF*/

/* -------------- system clock mode ----------------- */

static int snd_hdspe_info_clock_mode(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[] = {
		HDSPE_CLOCK_MODE_NAME(0),
		HDSPE_CLOCK_MODE_NAME(1)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

static int hdspe_get_clock_mode(struct hdspe* hdspe)
{
	return hdspe->m.get_clock_mode(hdspe);
}
static int hdspe_set_clock_mode(struct hdspe* hdspe, int val)
{
	hdspe->m.set_clock_mode(hdspe, val);
	return 0;
}

HDSPE_RW_ENUM_METHODS(clock_mode,
		      hdspe_get_clock_mode, hdspe_set_clock_mode, false);

/* --------------- preferred sync reference ------------------ */

static int snd_hdspe_info_pref_sync_ref(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);

	// Last autosync option ("intern") does not apply here.
	snd_ctl_enum_info(uinfo, 1, hdspe->t.autosync_count-1,
			  hdspe->t.autosync_texts);

	return 0;
}

static int hdspe_get_pref_sync_ref_idx(struct hdspe* hdspe)
{
	return hdspe->t.autosync_ref2idx[hdspe->m.get_pref_sync_ref(hdspe)];
}
static int hdspe_put_pref_sync_ref_idx(struct hdspe* hdspe, int idx)
{
	hdspe->m.set_pref_sync_ref(hdspe, hdspe->t.autosync_idx2ref[idx]);
	return 0;
}

HDSPE_RW_ENUM_METHODS(pref_sync_ref,
		      hdspe_get_pref_sync_ref_idx, hdspe_put_pref_sync_ref_idx,
		      false);

/* --------------- AutoSync reference ----------------- */

static int snd_hdspe_info_autosync_ref(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	snd_ctl_enum_info(uinfo, 1, hdspe->t.autosync_count,
			  hdspe->t.autosync_texts);
	return 0;
}

static int hdspe_get_autosync_ref_idx(struct hdspe* hdspe)
{
	return hdspe->t.autosync_ref2idx[hdspe->m.get_autosync_ref(hdspe)];
}

HDSPE_RO_ENUM_METHODS(autosync_ref, hdspe_get_autosync_ref_idx);

/* --------------- AutoSync Status ------------------- */

#ifdef OLDSTUFF
#define HDSPE_SYNC_STATUS(xname, xindex) \
{	.iface = SNDRV_CTL_ELEM_IFACE_CARD, \
	.name = xname, \
	.private_value = xindex, \
	.access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
	.info = snd_hdspe_info_sync_status, \
	.get = snd_hdspe_get_sync_status \
}

int snd_hdspe_info_sync_status(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[] = {
		HDSPE_SYNC_STATUS_NAME(0),
		HDSPE_SYNC_STATUS_NAME(1),
		HDSPE_SYNC_STATUS_NAME(2),
		HDSPE_SYNC_STATUS_NAME(3)		
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}
#endif /*OLDSTUFF*/

static int snd_hdspe_get_sync_status(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	enum hdspe_clock_source syncref = kcontrol->private_value;
	ucontrol->value.enumerated.item[0] = hdspe->m.get_sync_status(
		hdspe, syncref);
	return 0;
}


static int snd_hdspe_info_autosync_status(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);	
	static const char *const texts[] = {
		HDSPE_SYNC_STATUS_NAME(0),
		HDSPE_SYNC_STATUS_NAME(1),
		HDSPE_SYNC_STATUS_NAME(2),
		HDSPE_SYNC_STATUS_NAME(3)		
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	uinfo->count = hdspe->t.autosync_count - 1;  /* last item is "internal" */
	return 0;	
}

static int snd_hdspe_get_autosync_status(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe* hdspe = snd_kcontrol_chip(kcontrol);
	int i;

	for (i=0; i<hdspe->t.autosync_count-1; i++) {
		int ref = hdspe->t.autosync_idx2ref[i];
		ucontrol->value.enumerated.item[i] = hdspe->m.get_sync_status(hdspe, ref);
	}

	return 0;
}

/* -------------- AutoSync Frequency ------------ */

static const char *const texts_freq[HDSPE_FREQ_COUNT] = {
	HDSPE_FREQ_NAME(HDSPE_FREQ_NO_LOCK),
	HDSPE_FREQ_NAME(HDSPE_FREQ_32KHZ),
	HDSPE_FREQ_NAME(HDSPE_FREQ_44_1KHZ),
	HDSPE_FREQ_NAME(HDSPE_FREQ_48KHZ),
	HDSPE_FREQ_NAME(HDSPE_FREQ_64KHZ),
	HDSPE_FREQ_NAME(HDSPE_FREQ_88_2KHZ),
	HDSPE_FREQ_NAME(HDSPE_FREQ_96KHZ),
	HDSPE_FREQ_NAME(HDSPE_FREQ_128KHZ),
	HDSPE_FREQ_NAME(HDSPE_FREQ_176_4KHZ),
	HDSPE_FREQ_NAME(HDSPE_FREQ_192KHZ)
};

const char* const hdspe_freq_name(enum hdspe_freq i)
{
	return (i >= 0 && i < HDSPE_FREQ_COUNT)
		? texts_freq[i] : "???";
}

#ifdef OLDSTUFF
#define HDSPE_SYNC_FREQ(xname, xindex) \
{	.iface = SNDRV_CTL_ELEM_IFACE_CARD, \
	.name = xname, \
	.private_value = xindex, \
	.access = SNDRV_CTL_ELEM_ACCESS_READ|SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
	.info = snd_hdspe_info_autosync_freq, \
	.get = snd_hdspe_get_autosync_freq \
}

int snd_hdspe_info_autosync_freq(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	ENUMERATED_CTL_INFO(uinfo, texts_freq);
	return 0;
}

int snd_hdspe_get_autosync_freq(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	int syncref = kcontrol->private_value;
	ucontrol->value.enumerated.item[0] =
		hdspe->m.get_freq(hdspe, syncref);
	return 0;
}
#endif /*OLDSTUFF*/

static int snd_hdspe_info_autosync_freq(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);	
	ENUMERATED_CTL_INFO(uinfo, texts_freq);
	uinfo->count = hdspe->t.autosync_count - 1;  /* skip last item "internal" */
	return 0;	
}

static int snd_hdspe_get_autosync_freq(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe* hdspe = snd_kcontrol_chip(kcontrol);
	int i;

	for (i=0; i<hdspe->t.autosync_count-1; i++) {
		int ref = hdspe->t.autosync_idx2ref[i];
		ucontrol->value.enumerated.item[i] = hdspe->m.get_freq(hdspe, ref);
	}

	return 0;
}

/* --------------- External frequency --------------- */

static int snd_hdspe_info_external_freq(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	ENUMERATED_CTL_INFO(uinfo, texts_freq);
	return 0;
}

static enum hdspe_freq hdspe_get_external_freq(struct hdspe* hdspe)
{
	return hdspe->m.get_external_freq(hdspe);
}

HDSPE_RO_ENUM_METHODS(external_freq, hdspe_get_external_freq);

/* ---------------- Internal frequency ------------------ */

static int snd_hdspe_info_internal_freq(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	/* skip the first ("No Lock") frequency option */
	return snd_ctl_enum_info(uinfo, 1, ARRAY_SIZE(texts_freq)-1,
				 texts_freq + 1);
	return 0;
}

static int hdspe_get_internal_freq_idx(struct hdspe* hdspe)
{
	return hdspe_internal_freq(hdspe)-1;
}
static int hdspe_put_internal_freq_idx(struct hdspe* hdspe, int val)
{
	int pitch = hdspe_internal_pitch(hdspe);
	dev_dbg(hdspe->card->dev, "%s(%d): idx %d -> freq %d, pitch = %d\n",
		__func__, val, val, val+1, pitch);
	hdspe_write_internal_freq(hdspe, val+1);
	/* Preserve pitch (essentially ratio of effective (intenral) sample rate
	 * over standard (internal) sample rate) */
	if (hdspe_write_internal_pitch(hdspe, pitch)) {
		HDSPE_CTL_NOTIFY(dds);
	}
	return 0;
}

HDSPE_RW_ENUM_METHODS(internal_freq,
		      hdspe_get_internal_freq_idx, hdspe_put_internal_freq_idx,
		      false);

/* ---------------------- MADI specific ------------------- */

HDSPE_RW_ENUM_REG_METHODS(madi_sswclk, control, madi, WCK48, false)
HDSPE_RW_ENUM_REG_METHODS(madi_LineOut, control, madi, LineOut, false)
HDSPE_RW_ENUM_REG_METHODS(madi_tx_64ch, control, madi, tx_64ch, false)
HDSPE_RO_ENUM_REG_METHODS(madi_rx_64ch, status0, madi, rx_64ch)
HDSPE_RW_ENUM_REG_METHODS(madi_smux, control, madi, SMUX, false)
HDSPE_RW_ENUM_REG_METHODS(madi_clr_tms, control, madi, CLR_TMS, false)
HDSPE_RW_ENUM_REG_METHODS(madi_autoinput, control, madi, AutoInp, false)
	
static int snd_hdspe_info_madi_input_source(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[] = {
		HDSPE_MADI_INPUT_NAME(0),
		HDSPE_MADI_INPUT_NAME(1)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}
#define snd_hdspe_info_madi_input_select snd_hdspe_info_madi_input_source

HDSPE_RW_ENUM_REG_METHODS(madi_input_select, control, madi, inp_0, false)
HDSPE_RO_ENUM_REG_METHODS(madi_input_source, status0, madi, AB_int)

/* ------------------------ AES specific ------------------- */

HDSPE_RW_ENUM_REG_METHODS(aes_LineOut, control, aes, LineOut, false)
HDSPE_RW_ENUM_REG_METHODS(aes_clr_tms, control, aes, CLR_TMS, false)
HDSPE_RW_ENUM_REG_METHODS(aes_emp, control, aes, EMP, false)
HDSPE_RW_ENUM_REG_METHODS(aes_dolby, control, aes, Dolby, false)
HDSPE_RW_ENUM_REG_METHODS(aes_pro, control, aes, PRO, false)

HDSPE_RW_ENUM_REG_METHODS(aes_ds_mode, control, aes, ds_mode, false)
HDSPE_RW_ENUM_REG_METHODS(aes_qs_mode, control, aes, qs_mode, false)	

static int snd_hdspe_info_aes_ds_mode(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[] = {
		HDSPE_DS_MODE_NAME(0),
		HDSPE_DS_MODE_NAME(1)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}
	
static int snd_hdspe_info_aes_qs_mode(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_QS_MODE_COUNT] = {
		HDSPE_QS_MODE_NAME(0),
		HDSPE_QS_MODE_NAME(1),
		HDSPE_QS_MODE_NAME(2)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

/* ---------------------- RayDAT / AIO / AIO Pro ---------------- */

HDSPE_RW_ENUM_REG_METHODS(raio_spdif_opt, settings, raio, Spdif_Opt, false)
HDSPE_RW_ENUM_REG_METHODS(raio_spdif_pro, settings, raio, Pro, false)
HDSPE_RW_ENUM_REG_METHODS(raio_aeb1, settings, raio, AEB1, false)
HDSPE_RW_ENUM_REG_METHODS(raio_sswclk, settings, raio, Wck48, false)
HDSPE_RW_ENUM_REG_METHODS(raio_clr_tms, settings, raio, clr_tms, false)
HDSPE_RW_ENUM_REG_METHODS(aio_xlr, settings, raio, Sym6db, false)

/* --------------------- AIO S/PDIF input ---------------------- */

static int snd_hdspe_info_aio_spdif_in(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_RAIO_SPDIF_INPUT_COUNT] = {
		HDSPE_RAIO_SPDIF_INPUT_NAME(0),
		HDSPE_RAIO_SPDIF_INPUT_NAME(1),
		HDSPE_RAIO_SPDIF_INPUT_NAME(2)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

HDSPE_RW_ENUM_REG_METHODS(aio_spdif_in, settings, raio, Input, false)

/* --------------------- AIO AD / DA / Phones level ----------------- */

static int snd_hdspe_info_aio_input_level(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_AIO_LEVEL_COUNT] = {
		"Lo Gain",
		"+4 dBu",
		"-10 dBV"
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

static int snd_hdspe_info_aio_out_level(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_AIO_LEVEL_COUNT] = {
		"Hi Gain",
		"+4 dBu",
		"-10 dBV"
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}
#define snd_hdspe_info_aio_output_level snd_hdspe_info_aio_out_level
#define snd_hdspe_info_aio_phones_level snd_hdspe_info_aio_out_level

HDSPE_RW_ENUM_REG_METHODS(aio_input_level, settings, raio, AD_GAIN, false)
HDSPE_RW_ENUM_REG_METHODS(aio_output_level, settings, raio, DA_GAIN, false)
HDSPE_RW_ENUM_REG_METHODS(aio_phones_level, settings, raio, PH_GAIN, false)

/* ------------------ AIO PRO AD / DA / Phones level ---------------*/

static int snd_hdspe_info_aio_pro_input_level(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_AIO_PRO_INPUT_LEVEL_COUNT] = {
		HDSPE_AIO_PRO_INPUT_LEVEL_NAME(0),
		HDSPE_AIO_PRO_INPUT_LEVEL_NAME(1),
		HDSPE_AIO_PRO_INPUT_LEVEL_NAME(2),
		HDSPE_AIO_PRO_INPUT_LEVEL_NAME(3)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

#define snd_hdspe_get_aio_pro_input_level   snd_hdspe_get_aio_input_level
#define snd_hdspe_put_aio_pro_input_level   snd_hdspe_put_aio_input_level

/* AIO Pro output level here combines DA_GAIN and Sym6db. The output 
 * voltage range is no longer a simple 6dB different whether Sym6db 
 * is on or off. */
static int snd_hdspe_info_aio_pro_output_level(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_AIO_PRO_OUTPUT_LEVEL_COUNT] = {
		HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(0),
		HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(1),
		HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(2),
		HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(3),
		HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(4),
		HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(5),
		HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(6),
		HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(7)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

static int hdspe_get_aio_pro_output_level(struct hdspe* hdspe)
{
	struct hdspe_settings_reg_raio settings = hdspe->reg.settings.raio;
	return settings.DA_GAIN + (settings.Sym6db ? 4 : 0);
}
static int hdspe_put_aio_pro_output_level(struct hdspe* hdspe, int val)
{
	struct hdspe_settings_reg_raio *settings = &hdspe->reg.settings.raio;
	settings->DA_GAIN = val%4;
	settings->Sym6db = val/4;
	hdspe_write_settings(hdspe);
	return 0;
}

HDSPE_RW_ENUM_METHODS(aio_pro_output_level,
		      hdspe_get_aio_pro_output_level,
		      hdspe_put_aio_pro_output_level, false)

static int snd_hdspe_info_aio_pro_phones_level(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_AIO_PRO_PHONES_LEVEL_COUNT] = {
		HDSPE_AIO_PRO_PHONES_LEVEL_NAME(0),
		HDSPE_AIO_PRO_PHONES_LEVEL_NAME(1)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

#define snd_hdspe_get_aio_pro_phones_level   snd_hdspe_get_aio_phones_level
#define snd_hdspe_put_aio_pro_phones_level   snd_hdspe_put_aio_phones_level

/* --------------------------------------------------------- */

/* Common control elements, except those for which we need to keep the id. The
 * ones we need to keep the id for, are added explicitly in snd_hdspe_create_controls(). */
static const struct snd_kcontrol_new snd_hdspe_controls_common[] = {
	HDSPE_RW_KCTL(CARD, "Clock Mode", clock_mode),
#ifdef OLDSTUFF
	HDSPE_RW_KCTL(CARD, "Internal Pitch", internal_pitch),
#endif /*OLDSTUFF*/
	HDSPE_RW_KCTL(CARD, "Preferred AutoSync Reference", pref_sync_ref),
};

static const struct snd_kcontrol_new snd_hdspe_controls_madi[] = {
#ifdef OLDSTUFF
	// TODO
	HDSPE_SYNC_STATUS("WordClk Status", HDSPE_CLOCK_SOURCE_WORD),
	HDSPE_SYNC_STATUS("MADI Status", HDSPE_CLOCK_SOURCE_MADI),
	HDSPE_SYNC_STATUS("TCO Status", HDSPE_CLOCK_SOURCE_TCO),
	HDSPE_SYNC_STATUS("SyncIn Status", HDSPE_CLOCK_SOURCE_SYNC_IN),
	HDSPE_SYNC_FREQ("MADI Frequency", HDSPE_CLOCK_SOURCE_MADI),
#endif /*OLDSTUFF*/
	HDSPE_RW_BOOL_KCTL(CARD, "Single Speed WordClk Out", madi_sswclk),
	HDSPE_RW_BOOL_KCTL(CARD, "Line Out", madi_LineOut),
	HDSPE_RW_BOOL_KCTL(CARD, "TX 64 Channels Mode", madi_tx_64ch),
	HDSPE_RV_BOOL_KCTL(CARD, "RX 64 Channels Mode", madi_rx_64ch),
	HDSPE_RW_BOOL_KCTL(CARD, "Double Wire Mode", madi_smux),
	HDSPE_RW_BOOL_KCTL(CARD, "Clear Track Marker", madi_clr_tms),
	HDSPE_RW_BOOL_KCTL(CARD, "Safe Mode", madi_autoinput),
	HDSPE_RW_KCTL(CARD, "Input Select", madi_input_select),
	HDSPE_RV_KCTL(CARD, "Input Source", madi_input_source),
//	HDSPE_RV_KCTL(CARD, "Speed Mode", speed_mode)
};

static const struct snd_kcontrol_new snd_hdspe_controls_madiface[] = {
	HDSPE_RW_KCTL(CARD, "Clock Mode", clock_mode),
#ifdef OLDSTUFF	
	HDSPE_RV_KCTL(CARD, "Sample Rate", sample_rate),
	HDSPE_RW_KCTL(CARD, "Internal Frequency", internal_freq),
#endif /*OLDSTUFF*/
	HDSPE_RV_KCTL(CARD, "External Frequency", external_freq),
#ifdef OLDSTUFF
	// TODO
	HDSPE_SYNC_STATUS("MADI Status", HDSPE_CLOCK_SOURCE_MADI),
#endif /*OLDSTUFF*/
	HDSPE_RW_BOOL_KCTL(CARD, "TX 64 Channels Mode", madi_tx_64ch),
	HDSPE_RV_BOOL_KCTL(CARD, "RX 64 Channels Mode", madi_rx_64ch),
	HDSPE_RW_BOOL_KCTL(CARD, "Safe Mode", madi_autoinput),
//	HDSPE_RV_KCTL(CARD, "Speed Mode", speed_mode)
};

static const struct snd_kcontrol_new snd_hdspe_controls_aes[] = {
	HDSPE_RW_BOOL_KCTL(CARD, "Line Out", aes_LineOut),
	HDSPE_RW_BOOL_KCTL(CARD, "Emphasis", aes_emp),
	HDSPE_RW_BOOL_KCTL(CARD, "Non Audio", aes_dolby),
	HDSPE_RW_BOOL_KCTL(CARD, "Professional", aes_pro),
	HDSPE_RW_BOOL_KCTL(CARD, "Clear Track Marker", aes_clr_tms),
	HDSPE_RW_KCTL(CARD, "Double Speed Wire Mode", aes_ds_mode),
	HDSPE_RW_KCTL(CARD, "Quad Speed Wire Mode", aes_qs_mode)
};

static const struct snd_kcontrol_new snd_hdspe_controls_raydat[] = {
	HDSPE_RW_BOOL_KCTL(CARD, "S/PDIF Out Optical", raio_spdif_opt),
	HDSPE_RW_BOOL_KCTL(CARD, "S/PDIF Out Professional", raio_spdif_pro),
	HDSPE_RW_BOOL_KCTL(CARD, "Single Speed WordClk Out", raio_sswclk),
	HDSPE_RW_BOOL_KCTL(CARD, "Clear TMS", raio_clr_tms),	
};

static const struct snd_kcontrol_new snd_hdspe_controls_aio[] = {
	HDSPE_RW_KCTL(CARD, "S/PDIF In", aio_spdif_in),
	HDSPE_RW_BOOL_KCTL(CARD, "S/PDIF Out Optical", raio_spdif_opt),
	HDSPE_RW_BOOL_KCTL(CARD, "S/PDIF Out Professional", raio_spdif_pro),
	HDSPE_RW_BOOL_KCTL(CARD, "ADAT Internal", raio_aeb1),
	HDSPE_RW_BOOL_KCTL(CARD, "Single Speed WordClk Out", raio_sswclk),
	HDSPE_RW_BOOL_KCTL(CARD, "Clear TMS", raio_clr_tms),
	HDSPE_RW_BOOL_KCTL(CARD, "XLR Breakout Cable",  aio_xlr),
	HDSPE_RW_KCTL(CARD, "Input Level", aio_input_level),
	HDSPE_RW_KCTL(CARD, "Output Level", aio_output_level),
	HDSPE_RW_KCTL(CARD, "Phones Level", aio_phones_level)
};

static const struct snd_kcontrol_new snd_hdspe_controls_aio_pro[] = {
	HDSPE_RW_KCTL(CARD, "S/PDIF In", aio_spdif_in),
	HDSPE_RW_BOOL_KCTL(CARD, "S/PDIF Out Optical", raio_spdif_opt),
	HDSPE_RW_BOOL_KCTL(CARD, "S/PDIF Out Professional", raio_spdif_pro),
	HDSPE_RW_BOOL_KCTL(CARD, "ADAT Internal", raio_aeb1),
	HDSPE_RW_BOOL_KCTL(CARD, "Single Speed WordClk Out", raio_sswclk),
	HDSPE_RW_BOOL_KCTL(CARD, "Clear TMS", raio_clr_tms),
	HDSPE_RW_KCTL(CARD, "Input Level", aio_pro_input_level),
	HDSPE_RW_KCTL(CARD, "Output Level", aio_pro_output_level),
	HDSPE_RW_KCTL(CARD, "Phones Level", aio_pro_phones_level),
};

HDSPE_RO_INT1_HDSPE_METHODS(firmware_rev, 0, 0, 1)
HDSPE_RO_INT1_HDSPE_METHODS(fw_build, 0, 0, 1)
HDSPE_RO_INT1_HDSPE_METHODS(serial, 0, 0, 1)

#define snd_hdspe_info_running snd_ctl_boolean_mono_info
HDSPE_RO_ENUM_HDSPE_METHODS(running)
HDSPE_RO_INT1_HDSPE_METHODS(capture_pid, 0, 0, 1)
HDSPE_RO_INT1_HDSPE_METHODS(playback_pid, 0, 0, 1)

HDSPE_RO_INT1_METHODS(buffer_size, 64, 4096, 1, hdspe_period_size)

static int hdspe_is_tco_present(struct hdspe* hdspe)
{
	return hdspe->tco != NULL;
}
#define snd_hdspe_info_tco_present snd_ctl_boolean_mono_info
HDSPE_RO_ENUM_METHODS(tco_present, hdspe_is_tco_present)
	
static const struct snd_kcontrol_new snd_hdspe_controls_cardinfo[] = {
	HDSPE_RO_KCTL(CARD, "Card Revision", firmware_rev),
	HDSPE_RO_KCTL(CARD, "Firmware Build", fw_build),
	HDSPE_RO_KCTL(CARD, "Serial", serial),
	HDSPE_RO_KCTL(CARD, "TCO Present", tco_present),
	HDSPE_RV_KCTL(CARD, "Capture PID", capture_pid),
	HDSPE_RV_KCTL(CARD, "Playback PID", playback_pid),
};

struct snd_kcontrol* hdspe_add_control(struct hdspe* hdspe,
				       const struct snd_kcontrol_new* newctl)
{
  struct snd_kcontrol* ctl = snd_ctl_new1(newctl, hdspe);
  int err = snd_ctl_add(hdspe->card, ctl);
  return (err < 0) ? ERR_PTR(err) : ctl;
}

int hdspe_add_controls(struct hdspe *hdspe,
		       int count, const struct snd_kcontrol_new *list)
{
	int i;
	for (i = 0; i < count; i++) {
		int err = snd_ctl_add(hdspe->card,
				      snd_ctl_new1(&list[i], hdspe));
		if (err < 0)
			return err;
	}
	return 0;
}

int hdspe_add_control_id(struct hdspe* hdspe,
			 const struct snd_kcontrol_new* nctl,
			 struct snd_ctl_elem_id** ctl_id)
{
	struct snd_kcontrol* ctl = hdspe_add_control(hdspe, nctl);
	if (IS_ERR(ctl))
		return -PTR_ERR(ctl);
	*ctl_id = &ctl->id;
	return 0;
}

#ifdef OLDSTUFF
static int hdspe_create_syncstatus_control(struct hdspe* hdspe,
					    enum hdspe_clock_source src)
{
	struct snd_kcontrol_new kctl = HDSPE_SYNC_STATUS(0, src);

	char buf[100];
	snprintf(buf, sizeof(buf), "%s Status",
		 hdspe_clock_source_name(hdspe, src));
	kctl.name = buf;

	return hdspe_add_control_id(hdspe, &kctl, &hdspe->cid.sync[src]);
}

static int hdspe_create_freq_control(struct hdspe* hdspe,
				     enum hdspe_clock_source src)
{
	struct snd_kcontrol_new kctl = HDSPE_SYNC_FREQ(0, src);

	char buf[100];
	snprintf(buf, sizeof(buf), "%s Frequency",
		 hdspe_clock_source_name(hdspe, src));
	kctl.name = buf;

	return hdspe_add_control_id(hdspe, &kctl, &hdspe->cid.freq[src]);
}

static int hdspe_create_autosync_controls(struct hdspe* hdspe)
{
	int i, err = 0;
	
	/* Create sync status item for every available clock source,
	 * except internal clock (the last item in the list). */
	for (i = 0; i < hdspe->t.autosync_count-1 && err >= 0; i++) {
		err = hdspe_create_syncstatus_control(
			hdspe, hdspe->t.autosync_idx2ref[i]);
	}
	if (!hdspe->tco && err >= 0) {  /* Create a control telling there is no TCO */
		err = hdspe_create_syncstatus_control(
			hdspe, HDSPE_CLOCK_SOURCE_TCO);
	}

	/* Create frequency status item for every available clock source,
	 * except internal clock (the last item in the list). */
	for (i = 0; i < hdspe->t.autosync_count-1 && err >= 0; i++) {
		err = hdspe_create_freq_control(
			hdspe, hdspe->t.autosync_idx2ref[i]);
	}
	if (!hdspe->tco && err >= 0) {  /* For completeness */
		err = hdspe_create_freq_control(
			hdspe, HDSPE_CLOCK_SOURCE_TCO);
	}

	return err;
}
#endif /*OLDSTUFF*/

int snd_hdspe_create_controls(struct snd_card *card,
			      struct hdspe *hdspe)
{
	unsigned int limit;
	int err;
	const struct snd_kcontrol_new *list = NULL;

	/* Card info controls */
	err = hdspe_add_controls(hdspe,
				 ARRAY_SIZE(snd_hdspe_controls_cardinfo),
				 snd_hdspe_controls_cardinfo);
	if (err < 0)
		return err;

	HDSPE_ADD_RV_CONTROL_ID(CARD, "Running", running);
	HDSPE_ADD_RV_CONTROL_ID(CARD, "Buffer Size", buffer_size);

	HDSPE_ADD_RW_CONTROL_ID(CARD, "Status Polling", status_polling);
	HDSPE_ADD_RV_CONTROL_ID(HWDEP, "Raw Sample Rate", raw_sample_rate);
	HDSPE_ADD_RW_CONTROL_ID(HWDEP, "DDS", dds);
	HDSPE_ADD_RW_CONTROL_ID(CARD, "Internal Frequency", internal_freq);
	
	/* Common controls: sample rate etc ... */
	if (hdspe->io_type != HDSPE_MADIFACE) {
#ifdef OLDSTUFF	
		HDSPE_ADD_RO_CONTROL_ID(CARD, "Sample Rate", sample_rate);
		HDSPE_ADD_RO_CONTROL_ID(CARD, "Pitch", pitch);
#endif /*OLDSTUFF*/
		HDSPE_ADD_RV_CONTROL_ID(CARD, "Current AutoSync Reference", autosync_ref);
		HDSPE_ADD_RV_CONTROL_ID(CARD, "External Frequency", external_freq);
		
		err = hdspe_add_controls(hdspe,
					 ARRAY_SIZE(snd_hdspe_controls_common),
					 snd_hdspe_controls_common);
		if (err < 0)
			return err;
	}

	/* AutoSync status and frequency */
	if (hdspe->io_type != HDSPE_MADI && hdspe->io_type != HDSPE_MADIFACE) {
		HDSPE_ADD_RV_CONTROL_ID(CARD, "AutoSync Status",
					autosync_status);
		HDSPE_ADD_RV_CONTROL_ID(CARD, "AutoSync Frequency",
					autosync_freq);

#ifdef OLDSTUFF		
		err = hdspe_create_autosync_controls(hdspe);
		if (err)
			return err;
#endif /*OLDSTUFF*/
	}

	/* Card specific controls */
	switch (hdspe->io_type) {
	case HDSPE_MADI:
		list = snd_hdspe_controls_madi;
		limit = ARRAY_SIZE(snd_hdspe_controls_madi);
		break;
	case HDSPE_MADIFACE:
		list = snd_hdspe_controls_madiface;
		limit = ARRAY_SIZE(snd_hdspe_controls_madiface);
		break;
	case HDSPE_AES:
		list = snd_hdspe_controls_aes;
		limit = ARRAY_SIZE(snd_hdspe_controls_aes);
		break;
	case HDSPE_RAYDAT:
		list = snd_hdspe_controls_raydat;
		limit = ARRAY_SIZE(snd_hdspe_controls_raydat);
		break;
	case HDSPE_AIO:
		list = snd_hdspe_controls_aio;
		limit = ARRAY_SIZE(snd_hdspe_controls_aio);
		break;
	case HDSPE_AIO_PRO:
		list = snd_hdspe_controls_aio_pro;
		limit = ARRAY_SIZE(snd_hdspe_controls_aio_pro);
		break;
	default:
		snd_BUG();
	}
	err = hdspe_add_controls(hdspe, limit, list);
	if (err < 0)
		return err;

	/* Mixer controls, in hdspe_mixer.c */
	err = hdspe_create_mixer_controls(hdspe);
	if (err < 0)
		return err;

	/* TCO controls, in hdspe_tco.c */
	if (hdspe->tco) {
		err = hdspe_create_tco_controls(hdspe);
		if (err < 0)
			return err;
	}

	return 0;
}
