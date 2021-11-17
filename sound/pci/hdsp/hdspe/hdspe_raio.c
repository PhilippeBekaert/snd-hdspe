// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe_raio.c
 * @brief RME HDSPe RayDAT / AIO / AIO Pro driver methods.
 *
 * 20210728 ... 1117 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORS,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

#include "hdspe.h"
#include "hdspe_core.h"

/* Maps RayDAT WR_SETTINGS / RD_STATUS1 sync ref 4-bit code to 
 * hdspe_clock_source enum: identity mapping except for the unused codes. */
static enum hdspe_clock_source raydat_autosync_ref[16] = {
	HDSPE_CLOCK_SOURCE_WORD,
	HDSPE_CLOCK_SOURCE_AES,
	HDSPE_CLOCK_SOURCE_SPDIF,
	HDSPE_CLOCK_SOURCE_ADAT1,
	HDSPE_CLOCK_SOURCE_ADAT2,
	HDSPE_CLOCK_SOURCE_ADAT3,
	HDSPE_CLOCK_SOURCE_ADAT4,
	HDSPE_CLOCK_SOURCE_INTERN,    // unused codes
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_TCO,
	HDSPE_CLOCK_SOURCE_SYNC_IN,
	HDSPE_CLOCK_SOURCE_INTERN,    // unused codes
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN     // master clock mode
};

const char* const hdspe_raydat_clock_source_names[HDSPE_CLOCK_SOURCE_COUNT] = {
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  0),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  1),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  2),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  3),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  4),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  5),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  6),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  7),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  8),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT,  9),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT, 10),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT, 11),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT, 12),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT, 13),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT, 14),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_RAYDAT, 15),
};

/* Maps AIO / AIO Pro WR_SETTINGS / RD_STATUS1 sync ref 4-bit code to 
 * hdspe_clock_source enum: identity mapping except for the unused codes. */
static enum hdspe_clock_source aio_autosync_ref[16] = {
	HDSPE_CLOCK_SOURCE_WORD,
	HDSPE_CLOCK_SOURCE_AES,
	HDSPE_CLOCK_SOURCE_SPDIF,
	HDSPE_CLOCK_SOURCE_ADAT,
	HDSPE_CLOCK_SOURCE_INTERN,    // unused codes
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_TCO,
	HDSPE_CLOCK_SOURCE_SYNC_IN,
	HDSPE_CLOCK_SOURCE_INTERN,    // unused codes
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN,
	HDSPE_CLOCK_SOURCE_INTERN     // master clock mode
};

const char* const hdspe_aio_clock_source_names[HDSPE_CLOCK_SOURCE_COUNT] = {
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  0),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  1),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  2),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  3),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  4),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  5),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  6),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  7),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  8),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO,  9),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO, 10),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO, 11),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO, 12),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO, 13),	
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO, 14),
	HDSPE_CLOCK_SOURCE_NAME(HDSPE_AIO, 15),
};

/* Number of channels for different Speed Modes */
#define RAYDAT_SS_CHANNELS     36
#define RAYDAT_DS_CHANNELS     20
#define RAYDAT_QS_CHANNELS     12

#define AIO_IN_SS_CHANNELS        14
#define AIO_IN_DS_CHANNELS        10
#define AIO_IN_QS_CHANNELS        8
#define AIO_OUT_SS_CHANNELS        16
#define AIO_OUT_DS_CHANNELS        12
#define AIO_OUT_QS_CHANNELS        10

/* port names */
static const char * const texts_ports_raydat_ss[] = {
	"ADAT1.1", "ADAT1.2", "ADAT1.3", "ADAT1.4", "ADAT1.5", "ADAT1.6",
	"ADAT1.7", "ADAT1.8", "ADAT2.1", "ADAT2.2", "ADAT2.3", "ADAT2.4",
	"ADAT2.5", "ADAT2.6", "ADAT2.7", "ADAT2.8", "ADAT3.1", "ADAT3.2",
	"ADAT3.3", "ADAT3.4", "ADAT3.5", "ADAT3.6", "ADAT3.7", "ADAT3.8",
	"ADAT4.1", "ADAT4.2", "ADAT4.3", "ADAT4.4", "ADAT4.5", "ADAT4.6",
	"ADAT4.7", "ADAT4.8",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R"
};

static const char * const texts_ports_raydat_ds[] = {
	"ADAT1.1", "ADAT1.2", "ADAT1.3", "ADAT1.4",
	"ADAT2.1", "ADAT2.2", "ADAT2.3", "ADAT2.4",
	"ADAT3.1", "ADAT3.2", "ADAT3.3", "ADAT3.4",
	"ADAT4.1", "ADAT4.2", "ADAT4.3", "ADAT4.4",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R"
};

static const char * const texts_ports_raydat_qs[] = {
	"ADAT1.1", "ADAT1.2",
	"ADAT2.1", "ADAT2.2",
	"ADAT3.1", "ADAT3.2",
	"ADAT4.1", "ADAT4.2",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R"
};


static const char * const texts_ports_aio_in_ss[] = {
	"Analog.L", "Analog.R",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R",
	"ADAT.1", "ADAT.2", "ADAT.3", "ADAT.4", "ADAT.5", "ADAT.6",
	"ADAT.7", "ADAT.8",
	"AEB.1", "AEB.2", "AEB.3", "AEB.4"
};

static const char * const texts_ports_aio_out_ss[] = {
	"Analog.L", "Analog.R",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R",
	"ADAT.1", "ADAT.2", "ADAT.3", "ADAT.4", "ADAT.5", "ADAT.6",
	"ADAT.7", "ADAT.8",
	"Phone.L", "Phone.R",
	"AEB.1", "AEB.2", "AEB.3", "AEB.4"
};

static const char * const texts_ports_aio_in_ds[] = {
	"Analog.L", "Analog.R",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R",
	"ADAT.1", "ADAT.2", "ADAT.3", "ADAT.4",
	"AEB.1", "AEB.2", "AEB.3", "AEB.4"
};

static const char * const texts_ports_aio_out_ds[] = {
	"Analog.L", "Analog.R",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R",
	"ADAT.1", "ADAT.2", "ADAT.3", "ADAT.4",
	"Phone.L", "Phone.R",
	"AEB.1", "AEB.2", "AEB.3", "AEB.4"
};

static const char * const texts_ports_aio_in_qs[] = {
	"Analog.L", "Analog.R",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R",
	"ADAT.1", "ADAT.2",
	"AEB.1", "AEB.2", "AEB.3", "AEB.4"
};

static const char * const texts_ports_aio_out_qs[] = {
	"Analog.L", "Analog.R",
	"AES.L", "AES.R",
	"SPDIF.L", "SPDIF.R",
	"ADAT.1", "ADAT.2",
	"Phone.L", "Phone.R",
	"AEB.1", "AEB.2", "AEB.3", "AEB.4"
};


/* These tables map the ALSA channels 1..N to the channels that we
   need to use in order to find the relevant channel buffer. RME
   refers to this kind of mapping as between "the ADAT channel and
   the DMA channel." We index it using the logical audio channel,
   and the value is the DMA channel (i.e. channel buffer number)
   where the data for that channel can be read/written from/to.
*/
static const char channel_map_raydat_ss[HDSPE_MAX_CHANNELS] = {
	4, 5, 6, 7, 8, 9, 10, 11,	/* ADAT 1 */
	12, 13, 14, 15, 16, 17, 18, 19,	/* ADAT 2 */
	20, 21, 22, 23, 24, 25, 26, 27,	/* ADAT 3 */
	28, 29, 30, 31, 32, 33, 34, 35,	/* ADAT 4 */
	0, 1,			/* AES */
	2, 3,			/* SPDIF */
	-1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
};

static const char channel_map_raydat_ds[HDSPE_MAX_CHANNELS] = {
	4, 5, 6, 7,		/* ADAT 1 */
	8, 9, 10, 11,		/* ADAT 2 */
	12, 13, 14, 15,		/* ADAT 3 */
	16, 17, 18, 19,		/* ADAT 4 */
	0, 1,			/* AES */
	2, 3,			/* SPDIF */
	-1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
};

static const char channel_map_raydat_qs[HDSPE_MAX_CHANNELS] = {
	4, 5,			/* ADAT 1 */
	6, 7,			/* ADAT 2 */
	8, 9,			/* ADAT 3 */
	10, 11,			/* ADAT 4 */
	0, 1,			/* AES */
	2, 3,			/* SPDIF */
	-1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
};

static const char channel_map_aio_in_ss[HDSPE_MAX_CHANNELS] = {
	0, 1,			/* line in */
	8, 9,			/* aes in, */
	10, 11,			/* spdif in */
	12, 13, 14, 15, 16, 17, 18, 19,	/* ADAT in */
	2, 3, 4, 5,		/* AEB */
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
};

static const char channel_map_aio_out_ss[HDSPE_MAX_CHANNELS] = {
	0, 1,			/* line out */
	8, 9,			/* aes out */
	10, 11,			/* spdif out */
	12, 13, 14, 15, 16, 17, 18, 19,	/* ADAT out */
	6, 7,			/* phone out */
	2, 3, 4, 5,		/* AEB */
	-1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
};

static const char channel_map_aio_in_ds[HDSPE_MAX_CHANNELS] = {
	0, 1,			/* line in */
	8, 9,			/* aes in */
	10, 11,			/* spdif in */
	12, 13, 14, 15,         /* adat in */
	2, 3, 4, 5,		/* AEB */
	-1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};

static const char channel_map_aio_out_ds[HDSPE_MAX_CHANNELS] = {
	0, 1,			/* line out */
	8, 9,			/* aes out */
	10, 11,			/* spdif out */
	12, 13, 14, 15,         /* adat out */
	6, 7,			/* phone out */
	2, 3, 4, 5,		/* AEB */
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};

static const char channel_map_aio_in_qs[HDSPE_MAX_CHANNELS] = {
	0, 1,			/* line in */
	8, 9,			/* aes in */
	10, 11,			/* spdif in */
	12, 13,			/* adat in */
	2, 3, 4, 5,		/* AEB */
	-1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};

static const char channel_map_aio_out_qs[HDSPE_MAX_CHANNELS] = {
	0, 1,			/* line out */
	8, 9,			/* aes out */
	10, 11,			/* spdif out */
	12, 13,			/* adat out */	
	6, 7,			/* phone out */
	2, 3, 4, 5,		/* AEB */
	-1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};


#ifdef CONFIG_SND_DEBUG
/* WR_CONTROL register bit names for AIO, AIO Pro and RayDAT */
static const char* const raio_control_bitNames[32] = {
        "START",
        "LAT_0",
        "LAT_1",
        "LAT_2",
        "(Master)",
        "IE_AUDIO",
        "freq0",
        "freq1",
        "freq2",
        "?09",
        "?10",
        "?11",
        "?12",
        "?13",
        "?14",
        "?15",
        "?16",
        "?17",
        "?18",
        "?19",
        "?20",
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

/* WR_SETTINGS register bit names for RayDAT, AIO, AIO Pro */
static const char* const raio_settings_bitNames[32] = {
	"Master",
	"SyncRef0",
	"SyncRef1",
	"SyncRef2",
	"SyncRef3",
	"Wck48",
	"?06",
	"?07",
	"?08",
	"?09",
	"?10",
	"?11",
	"Input0",
	"Input1",
	"Spdif_Opt",
	"Pro",
	"clr_tms",
	"AEB1",
	"AEB2",
	"LineOut",
	"AD_GAIN0",
	"AD_GAIN1",
	"DA_GAIN0",
	"DA_GAIN1",
	"PH_GAIN0",
	"PH_GAIN1",
	"Sym6db",
	"?27",
	"?28",
	"?29",
	"?30",
	"?31",	
};

/* RD_STATUS1 register bit names for RayDAT, AIO, AIO Pro */
static const char* const raio_status1_bitNames[32] = {
	"lock0",
	"lock1",
	"lock2",
	"lock3",
	"lock4",
	"lock5",
	"lock6",
	"lock7",
	"sync0",
	"sync1",
	"sync2",
	"sync3",
	"sync4",
	"sync5",
	"sync6",
	"sync7",
	"wclk_freq0",
	"wclk_freq1",
	"wclk_freq2",
	"wclk_freq3",
	"tco_freq0",
	"tco_freq1",
	"tco_freq2",
	"tco_freq3",
	"wclk_lock",
	"wclk_sync",
	"tco_lock",
	"tco_sync",
	"sync_ref0",
	"sync_ref1",
	"sync_ref2",
	"sync_ref3"
};

/* RD_STATUS2 register bit names for RayDAT, AIO, AIO Pro */
static const char* const raio_status2_bitNames[32] = {
	"?00",
	"?01",
	"?02",
	"?03",
	"?04",
	"?05",
	"tco_detect",
	"AEBO_D",
	"AEBI_D",
	"?09",
	"sync_in_lock",
	"sync_in_sync",
	"sync_in_freq0",
	"sync_in_freq1",
	"sync_in_freq2",
	"sync_in_freq3",
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

static void hdspe_raio_read_status(struct hdspe* hdspe,
				   struct hdspe_status* status)
{
	int i;
	struct hdspe_settings_reg_raio settings = hdspe->reg.settings.raio;
	struct hdspe_status1_reg_raio status1 = hdspe_read_status1(hdspe).raio;
	struct hdspe_status2_reg_raio status2 = hdspe_read_status2(hdspe).raio;
	u32 fbits = hdspe_read_fbits(hdspe);

	status->version = HDSPE_VERSION;

	hdspe_read_sample_rate_status(hdspe, status);

	status->clock_mode = settings.Master;
	status->internal_freq = hdspe_internal_freq(hdspe);
	status->speed_mode = hdspe_speed_mode(hdspe);
	status->preferred_ref = settings.SyncRef;
	status->autosync_ref = status1.sync_ref;

	/* Word Clock Module (WCM) and Time Code Option (TCO)
	 * share the same way of communicating status: as if WCM is a TCO */
	if (!hdspe->tco) {
		if (status->preferred_ref == HDSPE_CLOCK_SOURCE_TCO)
			status->preferred_ref = HDSPE_CLOCK_SOURCE_WORD;
		if (status->autosync_ref == HDSPE_CLOCK_SOURCE_TCO)
			status->autosync_ref = HDSPE_CLOCK_SOURCE_WORD;
	}

	for (i = 0; i < HDSPE_CLOCK_SOURCE_COUNT; i++) {
		hdspe_set_sync_source(status, i, 0, false, false, false);
	}

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_WORD,
		status2.tco_detect ? HDSPE_FREQ_NO_LOCK : status1.tco_freq,
		status1.tco_lock,
		status1.tco_sync,
		!status2.tco_detect);

	for (i = 0; i < (hdspe->io_type == HDSPE_RAYDAT ? 6 : 3); i++) {
		hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_1+i,
			HDSPE_FBITS_FREQ(fbits, i),
			status1.lock & (1<<i),
			status1.sync & (1<<i), true);
	}

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_TCO,
		status2.tco_detect ? status1.tco_freq : HDSPE_FREQ_NO_LOCK,
		status1.tco_lock,
		status1.tco_sync,
		status2.tco_detect);

	hdspe_set_sync_source(status, HDSPE_CLOCK_SOURCE_SYNC_IN,
		status2.sync_in_freq,
		status2.sync_in_lock,
		status2.sync_in_sync, true);

	status->external_freq = hdspe_speed_adapt(
		status->freq[status->autosync_ref],
		status->speed_mode);

	status->wck48 = settings.Wck48;
	status->clr_tms = settings.clr_tms;

	status->raio.aebo = !status2.AEBO_D;
	status->raio.aebi = !status2.AEBI_D;
	status->raio.spdif_in = settings.Input;
	status->raio.spdif_opt = settings.Spdif_Opt;
	status->raio.spdif_pro = settings.Pro;

	switch (hdspe->io_type) {
	case HDSPE_AIO:
		status->raio.aio.input_level = settings.AD_GAIN;
		status->raio.aio.output_level = settings.DA_GAIN;
		status->raio.aio.phones_level = settings.PH_GAIN;		
		status->raio.aio.xlr = settings.Sym6db;
		break;

	case HDSPE_AIO_PRO:
		status->raio.aio_pro.input_level = settings.AD_GAIN;
		status->raio.aio_pro.output_level = settings.DA_GAIN +
			(settings.Sym6db ? 4 : 0);
		status->raio.aio_pro.phones_level = settings.PH_GAIN;
		break;

	default:
		snd_BUG();
	}
}

static bool hdspe_raio_has_status_changed(struct hdspe* hdspe)
{
	__le32 status1 = hdspe_read_status1(hdspe).raw;
	__le32 status2 = hdspe_read_status2(hdspe).raw;
	u32 fbits = hdspe_read_fbits(hdspe);
	const __le32 mask2 = cpu_to_le32(0x0000fc00); // sync_in lock,sync,freq

	bool changed = ((status1 != hdspe->t.status1) ||
			(status2 & mask2) != (hdspe->t.status2 & mask2) ||
			(fbits != hdspe->t.fbits));

#ifdef NEVER	
	if (changed)
	dev_dbg(hdspe->card->dev, "Status change: status1=%08x, status2=%08x, fbits=%08x\n",
		(status1 ^ hdspe->t.status1),
		(status2 ^ hdspe->t.status2) & mask2,
		(fbits ^ hdspe->t.fbits));
#endif /*NEVER*/

	hdspe->t.status1 = status1;
	hdspe->t.status2 = status2;
	hdspe->t.fbits = fbits;
	return changed;
}

static void hdspe_raio_set_float_format(struct hdspe* hdspe, bool val)
{
	hdspe->reg.control.raio.FloatFmt = val;
	hdspe_write_control(hdspe);
}

static bool hdspe_raio_get_float_format(struct hdspe* hdspe)
{
	return hdspe->reg.control.raio.FloatFmt;
}

static enum hdspe_clock_mode hdspe_raio_get_clock_mode(struct hdspe* hdspe)
{
	return hdspe->reg.settings.raio.Master;
}

static void hdspe_raio_set_clock_mode(struct hdspe* hdspe,
				      enum hdspe_clock_mode master)
{
	hdspe->reg.settings.raio.Master = master;
	hdspe_write_settings(hdspe);
}

static enum hdspe_clock_source hdspe_raio_get_preferred_sync_ref(
	struct hdspe* hdspe)
{
	enum hdspe_clock_source *autosync_ref =	hdspe->io_type == HDSPE_RAYDAT
		? raydat_autosync_ref : aio_autosync_ref;
	enum hdspe_clock_source ref =
		autosync_ref[hdspe->reg.settings.raio.SyncRef];
	if (ref == HDSPE_CLOCK_SOURCE_TCO && !hdspe->tco)
		ref = HDSPE_CLOCK_SOURCE_WORD;
	return ref;
}

static void hdspe_raio_set_preferred_sync_ref(struct hdspe* hdspe,
					      enum hdspe_clock_source ref)
{
	enum hdspe_clock_source *autosync_ref =	hdspe->io_type == HDSPE_RAYDAT
		? raydat_autosync_ref : aio_autosync_ref;
	if (autosync_ref[ref] == HDSPE_CLOCK_SOURCE_INTERN) ref = 0;
	if (ref == HDSPE_CLOCK_SOURCE_WORD)
		ref = HDSPE_CLOCK_SOURCE_TCO;
	hdspe->reg.settings.raio.SyncRef = ref;
	hdspe_write_settings(hdspe);
}

static enum hdspe_clock_source hdspe_raio_get_autosync_ref(struct hdspe* hdspe)
{
	enum hdspe_clock_source *autosync_ref =	hdspe->io_type == HDSPE_RAYDAT
		? raydat_autosync_ref : aio_autosync_ref;
	enum hdspe_clock_source ref =
		autosync_ref[hdspe_read_status1(hdspe).raio.sync_ref];
	if (ref == HDSPE_CLOCK_SOURCE_TCO && !hdspe->tco)
		ref = HDSPE_CLOCK_SOURCE_WORD;
	return ref;
}

static enum hdspe_sync_status hdspe_raio_get_sync_status(
	struct hdspe* hdspe, enum hdspe_clock_source src)
{
	struct hdspe_status1_reg_raio status1;
	struct hdspe_status2_reg_raio status2;
	int i;

	switch (src) {
	case HDSPE_CLOCK_SOURCE_WORD   :
		status1 = hdspe_read_status1(hdspe).raio;
		return HDSPE_MAKE_SYNC_STATUS(status1.tco_lock,
					      status1.tco_sync, !hdspe->tco);

	case HDSPE_CLOCK_SOURCE_AES    :
	case HDSPE_CLOCK_SOURCE_SPDIF  :
	case HDSPE_CLOCK_SOURCE_ADAT1  :
		i = src - HDSPE_CLOCK_SOURCE_1;
		status1 = hdspe_read_status1(hdspe).raio;		
		return HDSPE_MAKE_SYNC_STATUS(status1.lock & (1<<i),
					      status1.sync & (1<<i), true);

	case HDSPE_CLOCK_SOURCE_ADAT2  :
	case HDSPE_CLOCK_SOURCE_ADAT3  :
	case HDSPE_CLOCK_SOURCE_ADAT4  :
		i = src - HDSPE_CLOCK_SOURCE_1;
		status1 = hdspe_read_status1(hdspe).raio;
		return HDSPE_MAKE_SYNC_STATUS(status1.lock & (1<<i),
					      status1.sync & (1<<i),
					      hdspe->io_type == HDSPE_RAYDAT);

	case HDSPE_CLOCK_SOURCE_TCO    :
		status1 = hdspe_read_status1(hdspe).raio;
		return HDSPE_MAKE_SYNC_STATUS(status1.tco_lock,
					      status1.tco_sync,
					      hdspe->tco);

	case HDSPE_CLOCK_SOURCE_SYNC_IN:
		status2 = hdspe_read_status2(hdspe).raio;
		return HDSPE_MAKE_SYNC_STATUS(status2.sync_in_lock,
					      status2.sync_in_sync, true);

	default                :
		return HDSPE_SYNC_STATUS_NOT_AVAILABLE;
	}
}

static enum hdspe_freq hdspe_raio_get_freq(
	struct hdspe* hdspe, enum hdspe_clock_source src)
{
	switch (src) {
	case HDSPE_CLOCK_SOURCE_WORD   :
		return hdspe->tco ? 0 : hdspe_read_status1(hdspe).raio.tco_freq;
		
	case HDSPE_CLOCK_SOURCE_AES    :
	case HDSPE_CLOCK_SOURCE_SPDIF  :
	case HDSPE_CLOCK_SOURCE_ADAT1  :
	case HDSPE_CLOCK_SOURCE_ADAT2  :
	case HDSPE_CLOCK_SOURCE_ADAT3  :
	case HDSPE_CLOCK_SOURCE_ADAT4  :
		return HDSPE_FBITS_FREQ(hdspe_read_fbits(hdspe),
					src - HDSPE_CLOCK_SOURCE_AES);

	case HDSPE_CLOCK_SOURCE_TCO    :
		return hdspe->tco ? hdspe_read_status1(hdspe).raio.tco_freq : 0;

	case HDSPE_CLOCK_SOURCE_SYNC_IN:
		return hdspe_read_status2(hdspe).raio.sync_in_freq;

	default                :
		return HDSPE_FREQ_NO_LOCK;
	}
}

static enum hdspe_freq hdspe_raio_get_external_freq(struct hdspe* hdspe)
{
	enum hdspe_clock_source src = hdspe_raio_get_autosync_ref(hdspe);
	return hdspe_speed_adapt(hdspe_raio_get_freq(hdspe, src),
				 hdspe_speed_mode(hdspe));
}

static void hdspe_raio_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	int i;
	struct hdspe *hdspe = entry->private_data;
	struct hdspe_status s;

	hdspe_proc_read_common(buffer, hdspe, &s);

	snd_iprintf(buffer, "Input AEB\t\t: %d %s\n", s.raio.aebi,
		    HDSPE_BOOL_NAME(s.raio.aebi));
	snd_iprintf(buffer, "Output AEB\t\t: %d %s\n", s.raio.aebo,
		    HDSPE_BOOL_NAME(s.raio.aebo));
	snd_iprintf(buffer, "S/PDIF Input\t\t: %d %s\n", s.raio.spdif_in,
		    HDSPE_RAIO_SPDIF_INPUT_NAME(s.raio.spdif_in));
	snd_iprintf(buffer, "S/PDIF Optical output\t: %d %s\n", s.raio.spdif_opt,
		    HDSPE_BOOL_NAME(s.raio.spdif_opt));
	snd_iprintf(buffer, "S/PDIF Professional\t: %d %s\n", s.raio.spdif_pro,
		    HDSPE_BOOL_NAME(s.raio.spdif_pro));

	if (hdspe->io_type == HDSPE_AIO) {
		snd_iprintf(buffer, "Input Level\t\t: %d %s\n",
			    s.raio.aio.input_level,
			    HDSPE_AIO_LEVEL_NAME(s.raio.aio.input_level));
		snd_iprintf(buffer, "Output Level\t\t: %d %s\n",
			    s.raio.aio.output_level,
			    HDSPE_AIO_LEVEL_NAME(s.raio.aio.output_level));
		snd_iprintf(buffer, "Phones Level\t\t: %d %s\n",
			    s.raio.aio.phones_level,
			    HDSPE_AIO_LEVEL_NAME(s.raio.aio.phones_level));
		snd_iprintf(buffer, "XLR\t\t: %d %s\n",
			    s.raio.aio.xlr,
			    HDSPE_BOOL_NAME(s.raio.aio.xlr));
	}

	if (hdspe->io_type == HDSPE_AIO_PRO) {
		snd_iprintf(buffer, "Input Level\t\t: %d %s\n",
			    s.raio.aio_pro.input_level,
			    HDSPE_AIO_PRO_INPUT_LEVEL_NAME(
				    s.raio.aio_pro.input_level));
		snd_iprintf(buffer, "Output Level\t\t: %d %s\n",
			    s.raio.aio_pro.output_level,
			    HDSPE_AIO_PRO_OUTPUT_LEVEL_NAME(
				    s.raio.aio_pro.output_level));
		snd_iprintf(buffer, "Phones Level\t\t: %d %s\n",
			    s.raio.aio_pro.phones_level,
			    HDSPE_AIO_PRO_PHONES_LEVEL_NAME(
				    s.raio.aio_pro.phones_level));
	}

	snd_iprintf(buffer, "\n");
	IPRINTREG(buffer, "CONTROL", hdspe->reg.control.raw,
		  raio_control_bitNames);
	IPRINTREG(buffer, "SETTINGS", hdspe->reg.settings.raw,
		  raio_settings_bitNames);

	{
		union hdspe_status1_reg status1 = hdspe_read_status1(hdspe);
		union hdspe_status2_reg status2 = hdspe_read_status2(hdspe);
		u32 fbits = hdspe_read_fbits(hdspe);
		
		IPRINTREG(buffer, "STATUS1", status1.raw,
			  raio_status1_bitNames);
		IPRINTREG(buffer, "STATUS2", status2.raw,
			  raio_status2_bitNames);
		hdspe_iprint_fbits(buffer, "FBITS", fbits);
	}

	{
		union hdspe_status0_reg status0 = hdspe_read_status0(hdspe);
		snd_iprintf(buffer, "\n");
		snd_iprintf(buffer, "BUF_PTR\t: %05d\nBUF_ID\t: %d\n",
			    le16_to_cpu(status0.common.BUF_PTR)<<6,
			    status0.common.BUF_ID);
		snd_iprintf(buffer, "LAT\t: %d\n", hdspe->reg.control.common.LAT);
	}
	
	snd_iprintf(buffer, "\n");
	snd_iprintf(buffer, "Running     \t: %d\n", hdspe->running);
	snd_iprintf(buffer, "Capture PID \t: %d\n", hdspe->capture_pid);
	snd_iprintf(buffer, "Playback PID\t: %d\n", hdspe->playback_pid);
	
	snd_iprintf(buffer, "\n");
	snd_iprintf(buffer, "Capture channel mapping:\n");
	for (i = 0 ; i < hdspe->max_channels_in; i ++) {
		snd_iprintf(buffer, "Logical %d DMA %d '%s'\n",
			    i, hdspe->channel_map_in[i], hdspe->port_names_in[i]);
	}
	snd_iprintf(buffer, "\nPlayback channel mapping:\n");
	for (i = 0 ; i < hdspe->max_channels_out; i ++) {
		snd_iprintf(buffer, "Logical %d DMA %d '%s'\n",
			    i, hdspe->channel_map_out[i], hdspe->port_names_out[i]);
	}
}

static void hdspe_raio_get_card_info(struct hdspe* hdspe,
				     struct hdspe_card_info *s)
{
	struct hdspe_status2_reg_raio status2 = hdspe_read_status2(hdspe).raio;

	hdspe_get_card_info(hdspe, s);
	if (!status2.AEBI_D)
		s->expansion |= HDSPE_EXPANSION_AI4S;
	if (!status2.AEBO_D)
		s->expansion |= HDSPE_EXPANSION_AO4S;	
}

static const struct hdspe_methods hdspe_raio_methods = {
	.get_card_info = hdspe_raio_get_card_info,
	.read_status = hdspe_raio_read_status,
	.get_float_format = hdspe_raio_get_float_format,
	.set_float_format = hdspe_raio_set_float_format,
	.read_proc = hdspe_raio_proc_read,
	.get_freq = hdspe_raio_get_freq,
	.get_autosync_ref = hdspe_raio_get_autosync_ref,
	.get_external_freq = hdspe_raio_get_external_freq,
	.get_clock_mode = hdspe_raio_get_clock_mode,
	.set_clock_mode = hdspe_raio_set_clock_mode,
	.get_pref_sync_ref = hdspe_raio_get_preferred_sync_ref,
	.set_pref_sync_ref = hdspe_raio_set_preferred_sync_ref,
	.get_sync_status = hdspe_raio_get_sync_status,
	.has_status_changed = hdspe_raio_has_status_changed
};

static const struct hdspe_tables hdspe_raydat_tables = {
	.ss_in_channels = RAYDAT_SS_CHANNELS,
	.ss_out_channels = RAYDAT_SS_CHANNELS,
	.ds_in_channels = RAYDAT_DS_CHANNELS,
	.ds_out_channels = RAYDAT_DS_CHANNELS,		
	.qs_in_channels = RAYDAT_QS_CHANNELS,
	.qs_out_channels = RAYDAT_QS_CHANNELS,

	.channel_map_in_ss = channel_map_raydat_ss,
	.channel_map_out_ss = channel_map_raydat_ss,
	.channel_map_in_ds = channel_map_raydat_ds,
	.channel_map_out_ds = channel_map_raydat_ds,
	.channel_map_in_qs = channel_map_raydat_qs,
	.channel_map_out_qs = channel_map_raydat_qs,

	.port_names_in_ss = texts_ports_raydat_ss,
	.port_names_out_ss = texts_ports_raydat_ss,
	.port_names_in_ds = texts_ports_raydat_ds,
	.port_names_out_ds = texts_ports_raydat_ds,
	.port_names_in_qs = texts_ports_raydat_qs,
	.port_names_out_qs = texts_ports_raydat_qs,

	.clock_source_names = hdspe_raydat_clock_source_names
};

static void hdspe_raydat_init_tables(struct hdspe* hdspe)
{
	hdspe->t = hdspe_raydat_tables;
	
	hdspe_init_autosync_tables(
		hdspe, ARRAY_SIZE(raydat_autosync_ref), raydat_autosync_ref);
}

static const struct hdspe_tables hdspe_aio_tables = {
	.ss_in_channels = AIO_IN_SS_CHANNELS,
	.ds_in_channels = AIO_IN_DS_CHANNELS,
	.qs_in_channels = AIO_IN_QS_CHANNELS,
	.ss_out_channels = AIO_OUT_SS_CHANNELS,
	.ds_out_channels = AIO_OUT_DS_CHANNELS,
	.qs_out_channels = AIO_OUT_QS_CHANNELS,
	
	.channel_map_out_ss = channel_map_aio_out_ss,
	.channel_map_out_ds = channel_map_aio_out_ds,
	.channel_map_out_qs = channel_map_aio_out_qs,
	
	.channel_map_in_ss = channel_map_aio_in_ss,
	.channel_map_in_ds = channel_map_aio_in_ds,
	.channel_map_in_qs = channel_map_aio_in_qs,
	
	.port_names_in_ss = texts_ports_aio_in_ss,
	.port_names_out_ss = texts_ports_aio_out_ss,
	.port_names_in_ds = texts_ports_aio_in_ds,
	.port_names_out_ds = texts_ports_aio_out_ds,
	.port_names_in_qs = texts_ports_aio_in_qs,
	.port_names_out_qs = texts_ports_aio_out_qs,

	.clock_source_names = hdspe_aio_clock_source_names
};

static void hdspe_aio_init_tables(struct hdspe* hdspe)
{
	hdspe->t = hdspe_aio_tables;
	
	if (!hdspe_read_status2(hdspe).raio.AEBI_D) {
		dev_info(hdspe->card->dev, "AEB input board found\n");
		hdspe->t.ss_in_channels += 4;
		hdspe->t.ds_in_channels += 4;
		hdspe->t.qs_in_channels += 4;
	}
	
	if (!hdspe_read_status2(hdspe).raio.AEBO_D) {
		dev_info(hdspe->card->dev, "AEB output board found\n");
		hdspe->t.ss_out_channels += 4;
		hdspe->t.ds_out_channels += 4;
		hdspe->t.qs_out_channels += 4;
	}
	
	hdspe_init_autosync_tables(
		hdspe, ARRAY_SIZE(aio_autosync_ref), aio_autosync_ref);
}

static struct hdspe_midi hdspe_raydat_midi_ports[3] = {
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
	 .irq = HDSPE_midi2IRQPending,
	}
};

static struct hdspe_midi hdspe_aio_midi_ports[3] = {
	{.portname = "MIDI",
	 .dataIn = HDSPE_midiDataIn0,
	 .statusIn = HDSPE_midiStatusIn0,
	 .dataOut = HDSPE_midiDataOut0,
	 .statusOut = HDSPE_midiStatusOut0,
	 .ie = HDSPE_Midi0InterruptEnable,
	 .irq = HDSPE_midi0IRQPending,
	},
	{.portname = "MTC",
	 .dataIn = HDSPE_midiDataIn1,
	 .statusIn = HDSPE_midiStatusIn1,
	 .dataOut = -1,
	 .statusOut = -1,
	 .ie = HDSPE_Midi1InterruptEnable,
	 .irq = HDSPE_midi1IRQPending,
	}
};

int hdspe_init_raio(struct hdspe* hdspe)
{
	int midi_count = 0;
	struct hdspe_midi *midi_ports = NULL;
	
	hdspe->reg.settings.raio.Master = true;
	hdspe->reg.settings.raio.Input = HDSPE_RAIO_SPDIF_INPUT_COAXIAL;
	hdspe->reg.settings.raio.LineOut = true;
	hdspe_write_settings(hdspe);
	
	hdspe->m = hdspe_raio_methods;

	switch (hdspe->io_type) {
	case HDSPE_RAYDAT:
		hdspe->card_name = "RME RayDAT";
		midi_count = 2;
		midi_ports = hdspe_raydat_midi_ports;
		hdspe_raydat_init_tables(hdspe);
		break;

	case HDSPE_AIO:
		hdspe->card_name = "RME AIO";
		midi_count = 1;
		midi_ports = hdspe_aio_midi_ports;
		hdspe_aio_init_tables(hdspe);
		break;

	case HDSPE_AIO_PRO:
		hdspe->card_name = "RME AIO Pro";
		midi_count = 1;
		midi_ports = hdspe_aio_midi_ports;
		hdspe_aio_init_tables(hdspe);
		break;

	default:
		snd_BUG();
	}
	hdspe_init_midi(hdspe, midi_count + (hdspe->tco ? 1 : 0), midi_ports);
	
	return 0;
}

void hdspe_terminate_raio(struct hdspe* hdspe)
{
	if (hdspe->io_type == HDSPE_AIO_PRO) {
		hdspe->reg.settings.raio.LineOut = false;
		hdspe_write_settings(hdspe);
	}	
}

#ifdef FROM_WIN_DRIVER
//TODO


void hwInitSettings(PDEVICE_EXTENSION deviceExtension)
{
	if (deviceExtension->AutoSync) deviceExtension->Register0 |= c0_Master;
	else deviceExtension->Register0 &= ~c0_Master;

	deviceExtension->Register0 &= ~(c0_SyncRef0*0xF);
	deviceExtension->Register0 |= c0_SyncRef0*(deviceExtension->SyncRef & 0xF);

	if (deviceExtension->SingleSpeed) deviceExtension->Register0 |= c0_Wck48;
	else deviceExtension->Register0 &= ~c0_Wck48;

	if (deviceExtension->DoubleSpeed) deviceExtension->Register0 |= c0_DS_DoubleWire;
	else deviceExtension->Register0 &= ~c0_DS_DoubleWire;

	deviceExtension->Register0 &= ~(c0_QS_DoubleWire+c0_QS_QuadWire);
	switch (deviceExtension->QuadSpeed) {
	case 1: deviceExtension->Register0 |= c0_QS_DoubleWire; break;
	case 2: deviceExtension->Register0 |= c0_QS_QuadWire; break;
	}

	if (deviceExtension->Frame) deviceExtension->Register0 &= ~c0_Madi_Smux;
	else deviceExtension->Register0 |= c0_Madi_Smux;

	if (deviceExtension->Channels) deviceExtension->Register0 |= c0_Madi_64_Channels;
	else deviceExtension->Register0 &= ~c0_Madi_64_Channels;

	if (deviceExtension->AutoInput) deviceExtension->Register0 |= c0_Madi_AutoInput;
	else deviceExtension->Register0 &= ~c0_Madi_AutoInput;

	deviceExtension->Register0 &= ~(c0_Input0+c0_Input1);
	deviceExtension->Register0 |= c0_Input0*(deviceExtension->InputSource & 0x3);

	if (deviceExtension->OpticalOut) deviceExtension->Register0 |= c0_Spdif_Opt;
	else deviceExtension->Register0 &= ~c0_Spdif_Opt;

	if (deviceExtension->Professional) deviceExtension->Register0 |= c0_Pro;
	else deviceExtension->Register0 &= ~c0_Pro;

	if (!deviceExtension->Tms) deviceExtension->Register0 |= c0_clr_tms;
	else deviceExtension->Register0 &= ~c0_clr_tms;

	if (deviceExtension->Aeb) deviceExtension->Register0 |= c0_AEB1;
	else deviceExtension->Register0 &= ~c0_AEB1;

	if (deviceExtension->Emphasis) deviceExtension->Register0 |= c0_AEB2; // Emphasis == AEB2
	else deviceExtension->Register0 &= ~c0_AEB2;

	if (deviceExtension->PhonesLevel == 2) deviceExtension->Register0 &= ~c0_LineOut;
	else deviceExtension->Register0 |= c0_LineOut;

	deviceExtension->Register0 &= ~(c0_AD_GAIN1+c0_AD_GAIN0);
	switch (deviceExtension->InputLevel) {
	case 0: break;
	case 1: deviceExtension->Register0 |= c0_AD_GAIN0; break;
	case 2: deviceExtension->Register0 |= c0_AD_GAIN1; break;
	case 3: deviceExtension->Register0 |= c0_AD_GAIN1+c0_AD_GAIN0; break;
	}

	deviceExtension->Register0 &= ~(c0_DA_GAIN1+c0_DA_GAIN0);
	switch (deviceExtension->OutputLevel) {
	case 0: break;
	case 1: deviceExtension->Register0 |= c0_DA_GAIN0; break;
	case 2: deviceExtension->Register0 |= c0_DA_GAIN1; break;
	case 3: deviceExtension->Register0 |= c0_DA_GAIN1+c0_DA_GAIN0; break;
	}

	deviceExtension->Register0 &= ~(c0_PH_GAIN1+c0_PH_GAIN0);
	switch (deviceExtension->PhonesLevel) {
	case 0: break;
	case 1: deviceExtension->Register0 |= c0_PH_GAIN0; break;
	case 2: deviceExtension->Register0 |= c0_PH_GAIN1; break;
	case 3: deviceExtension->Register0 |= c0_PH_GAIN1+c0_PH_GAIN0; break;
	}

	if (deviceExtension->Balanced) deviceExtension->Register0 |= c0_Sym6db;
	else deviceExtension->Register0 &= ~c0_Sym6db;

	WriteRegister(deviceExtension, WR_SETTINGS, deviceExtension->Register0);
}
#endif
