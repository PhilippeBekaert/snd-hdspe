// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe-tco.c
 * @brief RME HDSPe Time Code Option driver status and control interface.
 *
 * 20210728,0812,0902,24,28,1008,13,27 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORS,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

#include "hdspe.h"
#include "hdspe_core.h"
#include "hdspe_control.h"
#include "hdspe_ltc_math.h"

#include <linux/slab.h>
#include <linux/bitfield.h>

#ifdef DEBUG_LTC
#define LTC_TIMER_FREQ 100
#endif /*DEBUG_LTC*/

/**
 * TCO register definitions
 */

#define HDSPE_TCO1_TCO_lock			0x00000001
#define HDSPE_TCO1_WCK_Input_Range_LSB		0x00000002
#define HDSPE_TCO1_WCK_Input_Range_MSB		0x00000004
#define HDSPE_TCO1_LTC_Input_valid		0x00000008
#define HDSPE_TCO1_WCK_Input_valid		0x00000010
#define HDSPE_TCO1_Video_Input_Format_NTSC	0x00000020
#define HDSPE_TCO1_Video_Input_Format_PAL	0x00000040

#define HDSPE_TCO1_set_TC			0x00000100
#define HDSPE_TCO1_set_drop_frame_flag		0x00000200
#define HDSPE_TCO1_LTC_Format_LSB		0x00000400
#define HDSPE_TCO1_LTC_Format_MSB		0x00000800

#define HDSPE_TCO1_STATUS_MASK                  0x00000cff

#define HDSPE_TCO2_TC_run			0x00010000
#define HDSPE_TCO2_WCK_IO_ratio_LSB		0x00020000
#define HDSPE_TCO2_WCK_IO_ratio_MSB		0x00040000
#define HDSPE_TCO2_set_num_drop_frames_LSB	0x00080000  /* unused */
#define HDSPE_TCO2_set_num_drop_frames_MSB	0x00100000  /* unused */
#define HDSPE_TCO2_set_jam_sync			0x00200000  /* unused */
#define HDSPE_TCO2_set_flywheel			0x00400000  /* unused */

#define HDSPE_TCO2_set_01_4			0x01000000
#define HDSPE_TCO2_set_pull_down		0x02000000
#define HDSPE_TCO2_set_pull_up			0x04000000
#define HDSPE_TCO2_set_freq			0x08000000
#define HDSPE_TCO2_set_term_75R			0x10000000
#define HDSPE_TCO2_set_input_LSB		0x20000000
#define HDSPE_TCO2_set_input_MSB		0x40000000
#define HDSPE_TCO2_set_freq_from_app		0x80000000

#ifdef CONFIG_SND_DEBUG
static const char* const tco1_bitNames[32] = {
	"TCO_lock",
	"WCK_Input_Range_LSB",
	"WCK_Input_Range_MSB",
	"LTC_Input_valid",
	"WCK_Input_valid",
	"Video_Input_Format_NTSC",
	"Video_Input_Format_PAL",
	"?7",

	"set_TC",
	"set_drop_frame_flag",
	"LTC_Format_LSB",
	"LTC_Format_MSB",
        "?12",
        "?13",
        "?14",
        "?15",
	
        "off0",	
        "off1",
        "off2",
        "off3",
        "off4",
        "off5",
        "off6",
        "?23",
	
        "off7",
        "off8",
        "off9",
        "off10",
        "off11",
        "off12",
        "off13",
        "?31"
};

static const char* const tco2_bitNames[32] = {
        "?00",	
        "?01",
        "?02",
        "?03",
        "?04",
        "?05",
        "?06",
        "?07",
	
        "?08",
        "?09",
        "?10",
        "?11",
        "?12",
        "?13",
        "?14",
        "?15",

	"TC_run",
	"WCK_IO_ratio_LSB",
	"WCK_IO_ratio_MSB",
	"set_num_drop_frames_LSB",
	"set_num_drop_frames_MSB",
	"set_jam_sync",
	"set_flywheel",
	"?23",

	"set_01_4",
	"set_pull_down",
	"set_pull_up",
	"set_freq",
	"set_term_75R",
	"set_input_LSB",
	"set_input_MSB",
	"set_freq_from_app",
};
#endif /*CONFIG_SND_DEBUG*/

/*
 * The TCO module sends quarter frame MTC messages when valid LTC input is 
 * detected and running. Piece 0 and 4 quarter frame MTC interrupts are 
 * generated at the precise instant a time code ends.
 *
 * Time code can also be queried at any time by reading TCO status register 0.
 * The most significant bits from TCO status register 1 contain the time offset,
 * measured in audio frames, since the start of the current time code. (The
 * offset comes in two groups of 7 bits.)
 *
 * When the cards audio engine is not running (audio interrupts not enabled), 
 * the registers are updated continuously, and can be used e.g. to measure
 * (MTC) interrupt handling latency, or to correlate LTC with system time.
 * Accuracy is about the time to read a register from the card.
 *
 * However, when the cards audio engine is running (audio interrupts enabled),
 * the registers are updated only at audio period interrupt time and certain
 * MTC interrupts (at longer period sizes). They seem trustworthy only at the
 * time of an audio period interrupt.
 *
 * The reported time code is the last time code that was fully received. If
 * time codes are running forward, the current time code at the time of an
 * audio period interrupt, will be one frame ahead of what the status 
 * register 0 tells. If running backward, the current time code is one frame 
 * earlier.
 *
 * Setting LTC for output only works if audio interrupts are enabled. The
 * LTC code set in TCO control register 0 will start running after the 
 * number of audio frames set in TCO control register 1 elapsed, past the 
 * next audio period interrupt time. So, LTC and offset for output need to
 * be queued ahead of time. We do that at audio interrupt time, assuming the
 * command will be certainly processed by the card by the next audio interrupt 
 * time. User specified offsets and time code are adapted accordingly.
 */

static inline __attribute__((always_inline))
u32 hdspe_read_tco(struct hdspe* hdspe, unsigned n)
{
	return le32_to_cpu(hdspe_read(hdspe, HDSPE_RD_TCO+4*n));
}

static inline __attribute__((always_inline))
void hdspe_write_tco(struct hdspe* hdspe, unsigned n, u32 value)
{
	hdspe_write(hdspe, HDSPE_WR_TCO+n*4, cpu_to_le32(value));
}

static void hdspe_tco_read_status1(struct hdspe* hdspe,
				   struct hdspe_tco_status* s)
{
	u32 tco1 = hdspe_read_tco(hdspe, 1);

	s->tco_lock    = FIELD_GET(HDSPE_TCO1_TCO_lock, tco1);
	s->ltc_valid   = FIELD_GET(HDSPE_TCO1_LTC_Input_valid, tco1);
	s->ltc_in_fps  = FIELD_GET(HDSPE_TCO1_LTC_Format_MSB|
				   HDSPE_TCO1_LTC_Format_LSB, tco1);
	s->ltc_in_drop = FIELD_GET(HDSPE_TCO1_set_drop_frame_flag, tco1);
	s->video       = FIELD_GET(HDSPE_TCO1_Video_Input_Format_NTSC|
				   HDSPE_TCO1_Video_Input_Format_PAL, tco1);
	s->wck_valid   = FIELD_GET(HDSPE_TCO1_WCK_Input_valid, tco1);
	s->wck_speed   = FIELD_GET(HDSPE_TCO1_WCK_Input_Range_MSB|
				   HDSPE_TCO1_WCK_Input_Range_LSB, tco1);

	/* Current time code started this many audio frames ago.
	 * Note: offset and time code are updated only at audio period
	 * interrupt time if audio interrupts are enabled. */
	s->ltc_in_offset = ((tco1 >> 16) & 0x7F) | ((tco1 >> 17) & 0x3F80);
}

static void hdspe_tco_copy_control(struct hdspe* hdspe,
				   struct hdspe_tco_status* s)
{
	snd_BUG_ON(!hdspe->tco);
	if (!hdspe->tco)
		return;

	s->input               = hdspe->tco->input;
	s->ltc_fps             = hdspe->tco->ltc_fps;
	s->ltc_drop            = hdspe->tco->ltc_drop;
	s->sample_rate         = hdspe->tco->sample_rate;
	s->pull                = hdspe->tco->pull;
	s->wck_conversion      = hdspe->tco->wck_conversion;
	s->term                = hdspe->tco->term;

	s->ltc_run             = hdspe->tco->ltc_run;
	s->ltc_flywheel        = hdspe->tco->ltc_flywheel;	
}

void hdspe_tco_read_status(struct hdspe* hdspe, struct hdspe_tco_status* s)
{
        spin_lock(&hdspe->tco->lock);
	s->version = HDSPE_VERSION;
	s->ltc_in = hdspe_read_tco(hdspe, 0);
	hdspe_tco_read_status1(hdspe, s);
	hdspe_tco_copy_control(hdspe, s);
        spin_unlock(&hdspe->tco->lock);
}

static void hdspe_tco_write_settings(struct hdspe* hdspe)
{
	static const int pullbits[HDSPE_PULL_COUNT] = {
		0,
		HDSPE_TCO2_set_pull_up,
		HDSPE_TCO2_set_pull_down,
		HDSPE_TCO2_set_pull_up|HDSPE_TCO2_set_01_4,
		HDSPE_TCO2_set_pull_down|HDSPE_TCO2_set_01_4
	};
	
	u32* reg;
	bool sys_48KHz = (hdspe->reg.control.common.freq == 3);
	
	struct hdspe_tco* c = hdspe->tco;
	if (!c) {
		snd_BUG();
		return;
	}
	reg = c->reg;

	reg[0] = reg[1] = reg[2] = reg[3] = 0;

	reg[1] |= FIELD_PREP(HDSPE_TCO1_LTC_Format_MSB|
			     HDSPE_TCO1_LTC_Format_LSB, c->ltc_fps);
	reg[1] |= FIELD_PREP(HDSPE_TCO1_set_drop_frame_flag, c->ltc_drop);

	reg[2] |= FIELD_PREP(HDSPE_TCO2_set_input_MSB|
			     HDSPE_TCO2_set_input_LSB, c->input);
	reg[2] |= FIELD_PREP(HDSPE_TCO2_WCK_IO_ratio_MSB|
			     HDSPE_TCO2_WCK_IO_ratio_LSB, c->wck_conversion);
	reg[2] |= FIELD_PREP(HDSPE_TCO2_set_freq,
			      c->sample_rate == HDSPE_TCO_SAMPLE_RATE_48 ||
			     (c->sample_rate == HDSPE_TCO_SAMPLE_RATE_FROM_APP
			      && sys_48KHz));
	reg[2] |= FIELD_PREP(HDSPE_TCO2_set_freq_from_app,
			     c->sample_rate == HDSPE_TCO_SAMPLE_RATE_FROM_APP);
	reg[2] |= FIELD_PREP(HDSPE_TCO2_set_term_75R, c->term);

	reg[2] |= pullbits[c->pull % HDSPE_PULL_COUNT];

	reg[2] |= FIELD_PREP(HDSPE_TCO2_TC_run, c->ltc_run);
	reg[2] |= FIELD_PREP(HDSPE_TCO2_set_flywheel, c->ltc_flywheel);

	hdspe_write_tco(hdspe, 0, reg[0]);
	hdspe_write_tco(hdspe, 1, reg[1]);
	hdspe_write_tco(hdspe, 2, reg[2]);
	hdspe_write_tco(hdspe, 3, reg[3]);
}

void hdspe_tco_set_app_sample_rate(struct hdspe* hdspe)
{
	/* Set TCO2_set_freq bit when internal frequency
	 * of the sound card is changed to 48KHz or multiple thereof, and
	 * TCO sample rate is "From App". */
	struct hdspe_tco* c = hdspe->tco;
	bool tco_48KHz, sys_48KHz;
	if (!c)
		return;

	if (c->sample_rate != HDSPE_TCO_SAMPLE_RATE_FROM_APP)
		return;
	
	tco_48KHz = FIELD_GET(HDSPE_TCO2_set_freq, c->reg[2]);
	sys_48KHz = (hdspe->reg.control.common.freq == 3);
	
	if (tco_48KHz != sys_48KHz) {
		c->reg[2] &= ~HDSPE_TCO2_set_freq;
		c->reg[2] |= FIELD_PREP(HDSPE_TCO2_set_freq, sys_48KHz);
		hdspe_write_tco(hdspe, 2, c->reg[2]);
		dev_dbg(hdspe->card->dev, "%s: 48KHz %s.\n",
			__func__, sys_48KHz ? "ON" : "OFF");
	}
}

////////////////////////////////////////////////////////////////////////////

static u32 hdspe_tco_get_sample_rate(struct hdspe* hdspe)
{
	struct hdspe_tco* c = hdspe->tco;
	bool tco_48KHz = FIELD_GET(HDSPE_TCO2_set_freq, c->reg[2]);
	return tco_48KHz ? 48000 : 44100;
}

static void hdspe_tco_set_timecode(struct hdspe* hdspe,
				   u32 timecode, u16 offset)
{
	struct hdspe_tco* c = hdspe->tco;
	hdspe_write_tco(hdspe, 0, timecode);
	hdspe_write_tco(hdspe, 1, (offset << 16) | HDSPE_TCO1_set_TC |
			(c->reg[1] & 0xffff));
	c->ltc_set = true;

	dev_dbg(hdspe->card->dev,
		"%s: timecode=%02x:%02x:%02x:%02x, offset=%d\n",
		__func__,
		(timecode>>24)&0x3f, (timecode>>16)&0x7f, (timecode>>8)&0x7f,
		timecode&0x3f,
		offset);
}

static void hdspe_tco_reset_timecode(struct hdspe* hdspe)
{
	struct hdspe_tco* c = hdspe->tco;
	
	hdspe_write_tco(hdspe, 1, c->reg[1] & 0xffff & ~HDSPE_TCO1_set_TC);
	c->ltc_set = false;

	dev_dbg(hdspe->card->dev, "%s\n", __func__);
}

/* Linear Time Code and associated status */
struct hdspe_ltc {
	u64  fc;         /* frame count at start */
	u32  tc;         /* 32-bit LTC code */
	u16  scale;      /* 999 or 1000 */
	u8   fps;        /* 24, 25 or 30 */
	bool df;         /* drop frame format */
};

static const u32 hdspe_fps_tab[4] = { 24, 25, 30, 30 };
static const u32 hdspe_scale_tab[4] = {1000, 1000, 999, 1000 };

/* Offsets needed when starting time code, experimentally determined and verified. */
static u32 hdspe_ltc_offset(u32 fps, enum hdspe_freq f)
{
    u32 offset = 0;
    if (fps == 24) {
        switch (f) {
	case 2: offset = 13; break; // 16; break;
	case 3: offset = 16; break; // 17; break;
#ifdef NEVER		
	case 5: offset = 31; break; // unused
	case 6: offset = 34; break; // unused
	case 8: offset = 62; break; // unused
	case 9: offset = 68; break; // unused
#endif /*NEVER*/
	default: offset = 0;
        }
    } else if (fps == 25) {
        switch (f) {
	case 2: offset = 15; break;
	case 3: offset = 16; break;
#ifdef NEVER		
	case 5: offset = 30; break; // unused
	case 6: offset = 32; break; // unused
	case 8: offset = 60; break; // unused
	case 9: offset = 64; break; // unused
#endif /*NEVER*/		
	default: offset = 0;		
        }
    } else if (fps == 30) {
        switch (f) {
	case 2: offset = 13; break; // 14; break;
	case 3: offset = 14; break;
#ifdef NEVER		
	case 5: offset = 28; break; // unused
	case 6: offset = 28; break; // unused
	case 8: offset = 56; break; // unused
	case 9: offset = 56; break; // unused
#endif /*NEVER*/		
	default: offset = 0;		
        }
    }
    return offset;
}

static void hdspe_tco_start_timecode(struct hdspe* hdspe)
{
	struct hdspe_tco* c = hdspe->tco;
	u64 cfc = hdspe->frame_count;           /* current frame count       */
	u32 fs;                                 /* LTC frame size in samples */
	u32 ps = hdspe_period_size(hdspe);      /* period size in samples    */
	int n;   /* compensate this many frames w.r.t. pickup at next period */
	s32 offset;       /* nr of samples to delay LTC start at next period */
	u32 sr = hdspe_tco_get_sample_rate(hdspe);            /* sample rate */
	u32 speedfactor = hdspe_speed_factor(hdspe);

	struct hdspe_ltc ltc;
	ltc.tc = c->ltc_out;
	ltc.fc = c->ltc_out_frame_count;
	ltc.fps = hdspe_fps_tab[c->ltc_fps];
	ltc.scale = hdspe_scale_tab[c->ltc_fps];
	ltc.df = c->ltc_drop;

	ltc.fc /= speedfactor;   /* need single speed offset */
	cfc /= speedfactor;
	ps /= speedfactor;

	fs = sr * 1000 / (ltc.fps * ltc.scale);

	if ((ltc.tc & 0x3f7f7f3f) == 0x3f7f7f3f) {
		/* this invalid time code means "real clock time"
		 * frame count contains an offset in seconds, typically
		 * timezone seconds east of UTC */
		struct timespec64 ts;
		struct tm tm;
		ktime_get_real_ts64(&ts);
		time64_to_tm(ts.tv_sec + ltc.fc, 0, &tm);
		ltc.tc = hdspe_ltc32_compose(tm.tm_hour, tm.tm_min, tm.tm_sec, 0);
		ltc.fc = cfc - ts.tv_nsec / (1000000000 / sr);
	}

	if (ltc.fc == (u64)-1)    /* means 'now' */
		ltc.fc = cfc;

	/* reduce ltc.fc to valid offset, taking into account it will be picked
	 * up by the hardware only at the next period interrupt */
	n = 0;
	if (ltc.fc > cfc + 2 * ps + fs) {
		n = -(ltc.fc - (cfc + 2 * ps)) / fs;
	} else if (ltc.fc < cfc + 2 * ps) {
		n = ((cfc + 2 * ps) - ltc.fc) / fs + 1;
	}
	ltc.fc += n * fs;
	ltc.tc = hdspe_ltc32_add_frames(n, ltc.tc, ltc.fps, ltc.df);
	offset = ltc.fc - (cfc + ps);      /* pickup at next audio period */
	dev_dbg(hdspe->card->dev,
		"%s: compensate %d frames: tc=%08x, fc=%llu, offset=%d\n",
		__func__, n, ltc.tc&0x3f7f7f3f, ltc.fc, offset);

	offset -= hdspe_ltc_offset(ltc.fps, hdspe_sample_rate_freq(sr));

	if (offset < 0 || (offset & ~0x3fff) != 0) { 
		dev_warn(hdspe->card->dev,
			 "%s: offset %d out of range 0..%d.\n",
			 __func__, offset, 0x3fff);
	}

	hdspe_tco_set_timecode(hdspe, ltc.tc, offset);
	c->ltc_out = 0xffffffff;
	
	hdspe_write_tco(hdspe, 2, c->reg[2] |= HDSPE_TCO2_TC_run);
	c->ltc_run = true;
	HDSPE_CTL_NOTIFY(ltc_run);
}

static void hdspe_tco_stop_timecode(struct hdspe* hdspe)
{
	struct hdspe_tco* c = hdspe->tco;
	dev_dbg(hdspe->card->dev, "%s\n", __func__);
	
	hdspe_write_tco(hdspe, 2, c->reg[2] &= ~HDSPE_TCO2_TC_run);
	c->ltc_run = false;
}

static void hdspe_tco_read_ltc(struct hdspe* hdspe, struct hdspe_ltc *ltc,
			       const char* where)
{
	u32 tco1, offset, framerate, tc;

	tc = hdspe_read_tco(hdspe, 0);
	tco1 = hdspe_read_tco(hdspe, 1);
	if ((ltc->tc = hdspe_read_tco(hdspe, 0)) != tc) {
		/* time code changed while we were reading tco1 */
		tco1 = hdspe_read_tco(hdspe, 1);
	}
	
	/* the offset comes in two groups of 7 bits indeed */
	offset = ((tco1 >> 16) & 0x7F) | ((tco1 >> 17) & 0x3F80);
	ltc->fc = hdspe->frame_count - offset * hdspe_speed_factor(hdspe);

	framerate = FIELD_GET(HDSPE_TCO1_LTC_Format_MSB|
			      HDSPE_TCO1_LTC_Format_LSB, tco1);
	ltc->fps = hdspe_fps_tab[framerate];
	ltc->scale = hdspe_scale_tab[framerate];
	ltc->df = FIELD_GET(HDSPE_TCO1_set_drop_frame_flag, tco1);
	
#ifdef DEBUG_LTC	
	{
		struct timespec64 t;
		ktime_get_raw_ts64(&t);
		dev_dbg(hdspe->card->dev,
			"%lld.%05ld: %s: TC %02x:%02x:%02x:%02x, TC frame count=%lld, period frame count=%lld, TC offset=%u.\n",
			t.tv_sec, t.tv_nsec / 10000,
			where,
			(ltc->tc>>24)&0x3f, (ltc->tc>>16)&0x7f,
			(ltc->tc>>8)&0x7f, (ltc->tc&0x3f),
			ltc->frame_count,
			hdspe->frame_count, ltc_offset);
	}
#endif /*DEBUG_LTC*/	
}

#ifdef DEBUG_MTC
void hdspe_tco_qmtc(struct hdspe* hdspe, u8 quarter_frame_msg)
{
	u8 piecenr = (quarter_frame_msg >> 4) & 0x0f;
	u8 bits = quarter_frame_msg & 0x0f;
	u32 mtc = (hdspe->tco->mtc & ~(0x0f << (4*piecenr))) |
		(bits << (4*piecenr));
	hdspe->tco->mtc = mtc;

	{
		struct timespec64 t;
		ktime_get_raw_ts64(&t);
		dev_dbg(hdspe->card->dev,
			"%lld.%05ld: %s: MTC %02d:%02d:%02d:%02d piece %d.\n",
			t.tv_sec, t.tv_nsec / 10000,
			__func__,
			(mtc>>24)&0x1f, (mtc>>16)&0x3f,
			(mtc>> 8)&0x3f, (mtc    )&0x1f,
			piecenr);
	}
}
#endif /*DEBUG_MTC*/

void hdspe_tco_mtc(struct hdspe* hdspe, const u8* buf, int count)
{
	struct hdspe_tco *c = hdspe->tco;
	bool newtc = false;
	
	if (count == 10 &&
	    buf[0] == 0xf0 && buf[1] == 0x7f && buf[2] == 0x7f &&
	    buf[3] == 0x01 && buf[4] == 0x01 && buf[9] == 0xf7) {
		/* full time code message */
		newtc = true;
	}
	if (count == 2 && buf[0] == 0xf1) {
		/* quarter frame message */
		int piecenr = (buf[1]>>4) & 0xf;
		newtc = (piecenr == 0 || piecenr == 4);

#ifdef DEBUG_MTC
		hdspe_tco_qmtc(hdspe, buf[1]);
#endif /*DEBUG_MTC*/
	}

	if (newtc) {
		uint64_t now = ktime_get_real_ns();
		if (c->prev_ltc_time > 0) {
			int n = c->ltc_count % LTC_CACHE_SIZE;
			c->ltc_duration_sum -= c->ltc_duration[n];
			c->ltc_duration[n] = now - c->prev_ltc_time;
			c->ltc_duration_sum += c->ltc_duration[n];
		}
		c->prev_ltc_time = now;
		c->ltc_count ++;
		
#ifdef DEBUG_LTC		
		struct hdspe_ltc ltc;
#endif /*DEBUG_LTC*/		
		spin_lock(&hdspe->tco->lock);
#ifdef DEBUG_LTC
		hdspe_tco_read_ltc(hdspe, &ltc, __func__);
#endif /*DEBUG_LTC*/
		hdspe->tco->ltc_changed = true;		
		spin_unlock(&hdspe->tco->lock);
	}
}

/* Invoked at every audio interrupt */
void hdspe_tco_period_elapsed(struct hdspe* hdspe)
{
	struct hdspe_tco* c = hdspe->tco;

	spin_lock(&hdspe->tco->lock);
	/* clock by which LTC frame start is measured. */
	c->ltc_time = hdspe->frame_count;

	/* Incoming time code and offset are accurate only at this time of an
	 * audio period interrupt, when audio interrupts are enabled.
	 * Check for changes and notify here. */
	if (c->ltc_changed) {   /* time code changed */
		s32 realfps1k;
		struct hdspe_ltc ltc;
		hdspe_tco_read_ltc(hdspe, &ltc, __func__);

		/* Add 1 frame, which is correct if running forward. 
  		 * The windows driver does that too. */
		ltc.tc = hdspe_ltc32_incr(ltc.tc, ltc.fps, ltc.df);

		c->ltc_in = ltc.tc;
		c->ltc_in_frame_count = ltc.fc;
		
		snd_ctl_notify(hdspe->card, SNDRV_CTL_EVENT_MASK_VALUE,
		               hdspe->cid.ltc_in);
		c->ltc_changed = false;

		/* Estimate actual LTC input "pull factor", based on the
		 * average duration in audio frames of the past LTC_CACHE_SIZE
		 * incoming LTC frames. Pull factor 1000 = nominal speed,
		 * 999 = NTSC pulldown. */
		realfps1k = c->ltc_duration_sum == 0 ? ltc.fps*1000 : 1000000000
		     / (u32)div_u64(c->ltc_duration_sum, (LTC_CACHE_SIZE*1000));
		c->ltc_in_pullfac = (realfps1k + ltc.fps/2) / ltc.fps;
/*
		dev_dbg(hdspe->card->dev, "%s: realfps=%u/1000, setfps=%u, pull=%d\n", __func__, realfps1k, ltc.fps, c->ltc_in_pull);
*/
		if (c->ltc_in_pullfac != c->last_ltc_in_pullfac)
			snd_ctl_notify(hdspe->card, SNDRV_CTL_EVENT_MASK_VALUE,
				       hdspe->cid.ltc_in_pullfac);
		c->last_ltc_in_pullfac = c->ltc_in_pullfac;
	}
	spin_unlock(&hdspe->tco->lock);

	if (c->ltc_set) {
		/* Output time code set at the previous audio interrupt
		 * is now picked up by the hardware. Reset the TCO1_set_TC 
		 * control bit and frame offset. */
		spin_lock(&hdspe->tco->lock);
		hdspe_tco_reset_timecode(hdspe);
		spin_unlock(&hdspe->tco->lock);
		/* c->ltc_set is reset to false at this time. */
	}

	if (c->ltc_out != 0xffffffff) { /* set timecode and start running LTC */
		spin_lock(&hdspe->tco->lock);		
		hdspe_tco_start_timecode(hdspe);
		spin_unlock(&hdspe->tco->lock);
		/* Output time code is picked up by the hardware at the next 
		 * audio period interrupt. 
		 * c->ltc_set is true at this point. 
		 * ltc_out is reset to 0xffffffff. */
	}
}

#ifdef DEBUG_LTC
static void hdspe_tco_timer(struct timer_list *t)
{
	struct hdspe* hdspe = container_of(t, struct hdspe, tco_timer);
	
	struct hdspe_ltc ltc;
	hdspe_tco_read_ltc(hdspe, &ltc, __func__);

	mod_timer(&hdspe->tco_timer, jiffies+HZ/LTC_TIMER_FREQ);
}
#endif /*DEBUG_LTC*/

////////////////////////////////////////////////////////////////////////////

void snd_hdspe_proc_read_tco(struct snd_info_entry *entry,
			     struct snd_info_buffer *buffer)
{
	struct hdspe *hdspe = entry->private_data;
	struct hdspe_tco *c = hdspe->tco;
	struct hdspe_tco_status s;
	u32 tco1 = hdspe_read_tco(hdspe, 1);
	u32 ltc = hdspe_read_tco(hdspe, 0);

	if (!c) {
		snd_BUG();
		return;
	}

	snd_iprintf(buffer, "TCO Status:\n\n");
	hdspe_tco_read_status(hdspe, &s);
	snd_iprintf(buffer, "LTC               : %02x:%02x:%02x%c%02x\n",
		    (s.ltc_in>>24)&0x3f, (s.ltc_in>>16)&0x7f, (s.ltc_in>>8)&0x7f,
		    s.ltc_in_drop ? '.' : ':',
		    (s.ltc_in&0x3f));
	snd_iprintf(buffer, "TCO Lock          : %d %s\n",
		    s.tco_lock, HDSPE_BOOL_NAME(s.tco_lock));
	snd_iprintf(buffer, "LTC Valid         : %d %s\n",
		    s.ltc_valid, HDSPE_BOOL_NAME(s.ltc_valid));
	snd_iprintf(buffer, "LTC In Frame Rate : %d %s\n",
		    s.ltc_in_fps, HDSPE_LTC_FRAME_RATE_NAME(s.ltc_in_fps));
	snd_iprintf(buffer, "LTC In Drop Frame : %d %s\n",
		    s.ltc_in_drop, HDSPE_BOOL_NAME(s.ltc_in_drop));
	snd_iprintf(buffer, "Video Input       : %d %s\n",
		    s.video, HDSPE_VIDEO_FORMAT_NAME(s.video));
	snd_iprintf(buffer, "WordClk Valid     : %d %s\n",
		    s.wck_valid, HDSPE_BOOL_NAME(s.wck_valid));
	snd_iprintf(buffer, "WordClk Speed     : %d %s\n",
		    s.wck_speed, HDSPE_SPEED_NAME(s.wck_speed));

	snd_iprintf(buffer, "\n");
	snd_iprintf(buffer, "LTC\t: 0x%08x\n", ltc);
	IPRINTREG(buffer, "TCO1", tco1, tco1_bitNames);

	snd_iprintf(buffer, "\nTCO Control:\n\n");
	snd_iprintf(buffer, "Sync Source       : %d %s\n",
		    c->input, HDSPE_TCO_SOURCE_NAME(c->input));
	snd_iprintf(buffer, "LTC Frame Rate    : %d %s\n",
		    c->ltc_fps, HDSPE_LTC_FRAME_RATE_NAME(c->ltc_fps));
	snd_iprintf(buffer, "LTC Drop Frame    : %d %s\n",
		    c->ltc_drop, HDSPE_BOOL_NAME(c->ltc_drop));
	snd_iprintf(buffer, "LTC Sample Rate   : %d %s\n",
		    c->sample_rate, HDSPE_TCO_SAMPLE_RATE_NAME(c->sample_rate));
	snd_iprintf(buffer, "WordClk Conversion: %d %s\n", c->wck_conversion,
		    HDSPE_WCK_CONVERSION_NAME(c->wck_conversion));
	snd_iprintf(buffer, "Pull Up / Down    : %d %s\n",
		    c->pull, HDSPE_PULL_NAME(c->pull));
	snd_iprintf(buffer, "75 Ohm Termination: %d %s\n",
		    c->term, HDSPE_BOOL_NAME(c->term));

	snd_iprintf(buffer, "\n");
	snd_iprintf(buffer, "LTC Out           : 0x%08x %02x:%02x:%02x%c%02x\n",
		    c->ltc_out, 
		    (c->ltc_out>>24) & 0x3f,
		    (c->ltc_out>>16) & 0x7f,
		    (c->ltc_out>> 8) & 0x7f,
		    (c->ltc_drop) ? '.' : ':',
		    (c->ltc_out    ) & 0x3f);
	snd_iprintf(buffer, "LTC Run           : %d %s\n",
		    c->ltc_run, HDSPE_BOOL_NAME(c->ltc_run));
	snd_iprintf(buffer, "LTC Flywheel      : %d %s\n",
		    c->ltc_flywheel, HDSPE_BOOL_NAME(c->ltc_flywheel));
	snd_iprintf(buffer, "LTC Set           : %d %s\n",
		    c->ltc_set, HDSPE_BOOL_NAME(c->ltc_set));
}

////////////////////////////////////////////////////////////////////////

static int hdspe_tco_get_status(struct hdspe* hdspe,
				int (*getter)(struct hdspe_tco_status*),
				const char* propname)		
{
	struct hdspe_tco_status status;
	int val;
	hdspe_tco_read_status1(hdspe, &status);
	val = getter(&status);
	dev_dbg(hdspe->card->dev, "%s(%s) = %d.\n", __func__, propname, val);
	return val;
}

static int hdspe_tco_put_control(struct hdspe* hdspe,
				 int val, int maxrange,
				 int (*putter)(struct hdspe_tco*, int val),
				 const char* propname)
{
	int changed;
	dev_dbg(hdspe->card->dev, "%s(%s,%d) ...\n", __func__, propname, val);
	if (val < 0 || val >= maxrange) {
		dev_warn(hdspe->card->dev, "%s value %d out of range 0..%d\n",
			 propname, val, maxrange-1);
		return -EINVAL;
	}
	spin_lock_irq(&hdspe->tco->lock);
	changed = putter(hdspe->tco, val);
	if (changed)
		hdspe_tco_write_settings(hdspe);
	spin_unlock_irq(&hdspe->tco->lock);
	dev_dbg(hdspe->card->dev, "... changed=%d.\n", changed);
	return changed;
}

#define HDSPE_TCO_STATUS_GET_WITHOUT_GETTER(prop, item, field)		\
static int snd_hdspe_get_##prop(struct snd_kcontrol *kcontrol,	\
				struct snd_ctl_elem_value *ucontrol)	\
{									\
	struct hdspe* hdspe = snd_kcontrol_chip(kcontrol);		\
	ucontrol->value.item[0] = hdspe_tco_get_status(			\
		hdspe, hdspe_tco_get_status_##field, #prop);		\
	return 0;							\
}

#define HDSPE_TCO_STATUS_GET(prop, item, field)				\
static int hdspe_tco_get_status_##field(struct hdspe_tco_status* s) \
{									\
	return s->field;						\
}									\
HDSPE_TCO_STATUS_GET_WITHOUT_GETTER(prop, item, field)

#define HDSPE_TCO_STATUS_ENUM_METHODS(prop, field)		\
	HDSPE_TCO_STATUS_GET(prop, enumerated.item, field)

#define HDSPE_TCO_STATUS_INT_METHODS(prop, field)		\
	HDSPE_TCO_STATUS_GET(prop, integer.value, field)


#define HDSPE_TCO_CONTROL_GET_WITHOUT_GETTER(prop, item, field)		\
static int snd_hdspe_get_##prop(struct snd_kcontrol *kcontrol,	\
				struct snd_ctl_elem_value *ucontrol)	\
{									\
	struct hdspe* hdspe = snd_kcontrol_chip(kcontrol);		\
	int val = ucontrol->value.item[0] =				\
		hdspe_tco_get_control_##field(hdspe->tco);		\
	dev_dbg(hdspe->card->dev, "%s = %d.\n", __func__, val);		\
	return 0;							\
}

#define HDSPE_TCO_CONTROL_GET(prop, item, field)			\
static int hdspe_tco_get_control_##field(struct hdspe_tco* c)	\
{									\
	return c->field;						\
}									\
HDSPE_TCO_CONTROL_GET_WITHOUT_GETTER(prop, item, field)

#define HDSPE_TCO_CONTROL_PUT_WITHOUT_PUTTER(prop, item, field, maxrange)\
static int snd_hdspe_put_##prop(struct snd_kcontrol *kcontrol,	\
				struct snd_ctl_elem_value *ucontrol)	\
{									\
	struct hdspe* hdspe = snd_kcontrol_chip(kcontrol);		\
	int val = ucontrol->value.item[0];				\
	return hdspe_tco_put_control(hdspe, val, maxrange,		\
				     hdspe_tco_put_control_##field, #prop); \
}
	
#define HDSPE_TCO_CONTROL_PUT(prop, item, field, maxrange)		\
static int hdspe_tco_put_control_##field(struct hdspe_tco* c, int val)	\
{									\
	int oldval = c->field;						\
	c->field = val;							\
	return val != oldval;						\
}									\
HDSPE_TCO_CONTROL_PUT_WITHOUT_PUTTER(prop, item, field, maxrange)

#define HDSPE_TCO_CONTROL_ENUM_METHODS(prop, field, maxrange)		\
	HDSPE_TCO_CONTROL_GET(prop, enumerated.item, field)		\
	HDSPE_TCO_CONTROL_PUT(prop, enumerated.item, field, maxrange)

/* ------------------------------------------------------------------- */

static int snd_hdspe_info_ltc_in_fps(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[] = {
		"24 fps", "25 fps", "29.97 fps", "30 fps"
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

HDSPE_TCO_STATUS_ENUM_METHODS(ltc_in_fps, ltc_in_fps)
HDSPE_TCO_STATUS_ENUM_METHODS(ltc_in_drop, ltc_in_drop)
HDSPE_TCO_STATUS_ENUM_METHODS(ltc_valid, ltc_valid)

static int snd_hdspe_info_video(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_VIDEO_FORMAT_COUNT] = {
		HDSPE_VIDEO_FORMAT_NAME(0),
		HDSPE_VIDEO_FORMAT_NAME(1),
		HDSPE_VIDEO_FORMAT_NAME(2)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

HDSPE_TCO_STATUS_ENUM_METHODS(video, video)
	
HDSPE_TCO_STATUS_ENUM_METHODS(wck_valid, wck_valid)

static int snd_hdspe_info_wck_speed(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_SPEED_COUNT] = {
		HDSPE_SPEED_NAME(0),
		HDSPE_SPEED_NAME(1),
		HDSPE_SPEED_NAME(2)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

HDSPE_TCO_STATUS_ENUM_METHODS(wck_speed, wck_speed)

HDSPE_TCO_STATUS_ENUM_METHODS(tco_lock, tco_lock)

static int snd_hdspe_info_ltc_in_pullfac(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	return 0;
}

static int snd_hdspe_get_ltc_in_pullfac(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = hdspe->tco->ltc_in_pullfac;
	return 0;
}


HDSPE_TCO_CONTROL_ENUM_METHODS(word_term, term, 2)
	
static int snd_hdspe_info_sample_rate(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_TCO_SAMPLE_RATE_COUNT] = {
		HDSPE_TCO_SAMPLE_RATE_NAME(0),
		HDSPE_TCO_SAMPLE_RATE_NAME(1),
		HDSPE_TCO_SAMPLE_RATE_NAME(2)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

HDSPE_TCO_CONTROL_ENUM_METHODS(sample_rate, sample_rate,
			       HDSPE_TCO_SAMPLE_RATE_COUNT)

static int snd_hdspe_info_pull(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_PULL_COUNT] = {
		HDSPE_PULL_NAME(0),
		HDSPE_PULL_NAME(1),
		HDSPE_PULL_NAME(2),
		HDSPE_PULL_NAME(3),
		HDSPE_PULL_NAME(4)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

HDSPE_TCO_CONTROL_ENUM_METHODS(pull, pull, HDSPE_PULL_COUNT)
	
static int snd_hdspe_info_wck_conversion(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_WCK_CONVERSION_COUNT] = {
		HDSPE_WCK_CONVERSION_NAME(0),
		HDSPE_WCK_CONVERSION_NAME(1),
		HDSPE_WCK_CONVERSION_NAME(2)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

HDSPE_TCO_CONTROL_ENUM_METHODS(wck_conversion, wck_conversion,
	HDSPE_WCK_CONVERSION_COUNT)

static int snd_hdspe_info_frame_rate(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[] = {
		"24 fps", "25 fps", "29.97 fps",
		"29.97 dfps", "30 fps", "30 dfps"
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

static int hdspe_tco_get_control_frame_rate(struct hdspe_tco* c)
{
	static int fr[8] = { 0, 1, 2, 4,   0, 1, 3, 5 };
	return 	fr[(c->ltc_drop!=0)*4 + c->ltc_fps%4];
}

static int hdspe_tco_put_control_frame_rate(struct hdspe_tco* c, int val)
{
	static int fps[6] = { 0, 1, 2, 2, 3, 3 };
	static int df[6]  = { 0, 0, 0, 1, 0, 1 };
	int rc = 0;
	if (c->ltc_fps != fps[val]) {
		c->ltc_fps = fps[val];
		rc = 1;
	}
	if (c->ltc_drop != df[val]) {
		c->ltc_drop = df[val];
		rc = 1;
	}
	return rc;
}

HDSPE_TCO_CONTROL_GET_WITHOUT_GETTER(frame_rate, enumerated.item, frame_rate)
HDSPE_TCO_CONTROL_PUT_WITHOUT_PUTTER(frame_rate, enumerated.item, frame_rate, 6)

static int snd_hdspe_info_sync_source(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[HDSPE_TCO_SOURCE_COUNT] = {
		HDSPE_TCO_SOURCE_NAME(0),
		HDSPE_TCO_SOURCE_NAME(1),
		HDSPE_TCO_SOURCE_NAME(2)
	};
	ENUMERATED_CTL_INFO(uinfo, texts);
	return 0;
}

HDSPE_TCO_CONTROL_ENUM_METHODS(sync_source, input, HDSPE_TCO_SOURCE_COUNT)

#ifdef NEVER	
HDSPE_TCO_CONTROL_ENUM_METHODS(ltc_jam_sync, ltc_jam, 2)
HDSPE_TCO_CONTROL_ENUM_METHODS(ltc_flywheel, ltc_flywheel, 2)	
#endif /*NEVER*/
HDSPE_TCO_CONTROL_ENUM_METHODS(ltc_run, ltc_run, 2)

static int snd_hdspe_info_ltc_in(struct snd_kcontrol* kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER64;
	uinfo->count = 2;
	return 0;
}

static int snd_hdspe_get_ltc_in(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);
	
	u32 ltc = hdspe->tco->ltc_in;
	u64 tc;
	
	spin_lock_irq(&hdspe->tco->lock);
	//	dev_dbg(hdspe->card->dev, "%s ...\n", __func__);
	/* The TCO module reports no user bits. They will be 0. */
	tc = ((u64)(ltc&0xf0000000) << 28) |
	     ((u64)(ltc&0x0f000000) << 24) |
	     ((u64)(ltc&0x00f00000) << 20) |
	     ((u64)(ltc&0x000f0000) << 16) |
	     ((u64)(ltc&0x0000f000) << 12) |
	     ((u64)(ltc&0x00000f00) <<  8) |
	     ((u64)(ltc&0x000000f0) <<  4) |
	     ((u64)(ltc&0x0000000f) <<  0);

	ucontrol->value.integer64.value[0] = tc;
	ucontrol->value.integer64.value[1] = hdspe->tco->ltc_in_frame_count;
	spin_unlock_irq(&hdspe->tco->lock);

	return 0;
}

static int snd_hdspe_info_ltc_time(struct snd_kcontrol* kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER64;
	uinfo->count = 1;
	return 0;
}

static int snd_hdspe_get_ltc_time(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);	
	spin_lock_irq(&hdspe->tco->lock);
	ucontrol->value.integer64.value[0] = hdspe->tco->ltc_time;
	spin_unlock_irq(&hdspe->tco->lock);
	return 0;
}

static int snd_hdspe_info_ltc_out(struct snd_kcontrol* kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER64;
	uinfo->count = 2;
	return 0;
}

static int snd_hdspe_put_ltc_out(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hdspe* hdspe = snd_kcontrol_chip(kcontrol);
	u64 tc = ucontrol->value.integer64.value[0];
	spin_lock_irq(&hdspe->tco->lock);
	/* Discard the user bits. The TCO module does not handle them. */
	hdspe->tco->ltc_out =
		((tc >> 28) & 0xf0000000) |
		((tc >> 24) & 0x0f000000) |
		((tc >> 20) & 0x00f00000) |
		((tc >> 16) & 0x000f0000) |
		((tc >> 12) & 0x0000f000) |
		((tc >>  8) & 0x00000f00) |
		((tc >>  4) & 0x000000f0) |
		((tc >>  0) & 0x0000000f);
	hdspe->tco->ltc_out_frame_count = ucontrol->value.integer64.value[1];
	spin_unlock_irq(&hdspe->tco->lock);
	return 0;    /* do not notify */
}

/* Control elements for the optional TCO module */
static const struct snd_kcontrol_new snd_hdspe_controls_tco[] = {
	HDSPE_RW_KCTL(CARD, "LTC Sample Rate", sample_rate),
	HDSPE_RW_KCTL(CARD, "TCO Pull", pull),
	HDSPE_RW_KCTL(CARD, "TCO WCK Conversion", wck_conversion),
	HDSPE_RW_KCTL(CARD, "LTC Frame Rate", frame_rate),
	HDSPE_RW_KCTL(CARD, "TCO Sync Source", sync_source),
	HDSPE_RW_BOOL_KCTL(CARD, "TCO Word Term", word_term),
	HDSPE_WO_KCTL(CARD, "LTC Out", ltc_out),
	HDSPE_RV_KCTL(CARD, "LTC Time", ltc_time)
};

#define CHECK_STATUS_CHANGE(prop)				 \
if (n.prop != o.prop) {						 \
	dev_dbg(hdspe->card->dev, "%s changed %d -> %d\n",	 \
		#prop, o.prop, n.prop);				 \
	HDSPE_CTL_NOTIFY(prop);					 \
	changed = true;						 \
}								 \

bool hdspe_tco_notify_status_change(struct hdspe* hdspe)
{
	bool changed = false;
	struct hdspe_tco_status o = hdspe->tco->last_status;
	struct hdspe_tco_status n;
	hdspe_tco_read_status1(hdspe, &n);

	CHECK_STATUS_CHANGE(ltc_valid);
	CHECK_STATUS_CHANGE(ltc_in_fps);
	CHECK_STATUS_CHANGE(ltc_in_drop);
	CHECK_STATUS_CHANGE(video);
	CHECK_STATUS_CHANGE(wck_valid);
	CHECK_STATUS_CHANGE(wck_speed);
	CHECK_STATUS_CHANGE(tco_lock);

	hdspe->tco->last_status = n;
	return changed;
}

int hdspe_create_tco_controls(struct hdspe* hdspe)
{
	if (!hdspe->tco)
		return 0;

	HDSPE_ADD_RV_CONTROL_ID(CARD, "LTC In", ltc_in);
	
	HDSPE_ADD_RV_BOOL_CONTROL_ID(CARD, "LTC In Valid", ltc_valid);
	HDSPE_ADD_RV_CONTROL_ID(CARD, "LTC In Frame Rate", ltc_in_fps);
	HDSPE_ADD_RV_BOOL_CONTROL_ID(CARD, "LTC In Drop Frame", ltc_in_drop);
	HDSPE_ADD_RV_CONTROL_ID(CARD, "LTC In Pull Factor", ltc_in_pullfac);
	HDSPE_ADD_RV_CONTROL_ID(CARD, "TCO Video Format", video);
	HDSPE_ADD_RV_BOOL_CONTROL_ID(CARD, "TCO WordClk Valid", wck_valid);
	HDSPE_ADD_RV_CONTROL_ID(CARD, "TCO WordClk Speed", wck_speed);
	HDSPE_ADD_RV_BOOL_CONTROL_ID(CARD, "TCO Lock", tco_lock);

	HDSPE_ADD_RW_BOOL_CONTROL_ID(CARD, "LTC Run", ltc_run);
	
	return hdspe_add_controls(
		hdspe, ARRAY_SIZE(snd_hdspe_controls_tco),
		snd_hdspe_controls_tco);
}

//////////////////////////////////////////////////////////////////////////

/* Return whether the optional TCO module is present or not. */
static bool hdspe_tco_detect(struct hdspe* hdspe)
{
	switch (hdspe->io_type) {
	case HDSPE_MADI:
	case HDSPE_AES:   // (AES and MADI have the same tco_detect bit)
		return hdspe_read_status0(hdspe).madi.tco_detect;

	case HDSPE_RAYDAT:
	case HDSPE_AIO:
	case HDSPE_AIO_PRO:
		return hdspe_read_status2(hdspe).raio.tco_detect;

	default:
		return false;
	};
}

int hdspe_init_tco(struct hdspe* hdspe)
{
	hdspe->tco = NULL;
	if (!hdspe_tco_detect(hdspe))
		goto bailout;

	hdspe->tco = kzalloc(sizeof(*hdspe->tco), GFP_KERNEL);
	if (!hdspe->tco)
		goto bailout;

	spin_lock_init(&hdspe->tco->lock);
	
	hdspe->midiPorts++;
	dev_info(hdspe->card->dev, "TCO module found\n");

/*	hdspe->tco->ltc_out = 0xffffffff;      would not set LTC output */
	hdspe_tco_write_settings(hdspe);

#ifdef DEBUG_LTC
	timer_setup(&hdspe->tco_timer, hdspe_tco_timer, 0);
	mod_timer(&hdspe->tco_timer, jiffies+HZ/LTC_TIMER_FREQ);
#endif /*DEBUG_LTC*/
	
bailout:	
	return 0;
}

void hdspe_terminate_tco(struct hdspe* hdspe)
{
	if (!hdspe->tco)
		return;

#ifdef DEBUG_LTC
	del_timer_sync(&hdspe->tco_timer);
#endif /*DEBUG_LTC*/

	hdspe_tco_stop_timecode(hdspe);
	hdspe_tco_reset_timecode(hdspe);
	
	kfree(hdspe->tco);
}
