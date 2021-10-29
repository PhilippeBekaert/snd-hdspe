// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe_ltc_math.c
 * @brief RME HDSPe 32-bit LTC code optimised math for TCO module.
 *
 * 20210930,1001,08 - Philippe.Bekaert@uhasselt.be
 */

#include "hdspe_ltc_math.h"

int hdspe_ltc_fpd(int fps, int df)
{
	return df ? 24*107892 : 24*60*60*fps;
}

void hdspe_ltc32_parse(u32 ltc, int *h, int *m, int *s, int *f)
{
	*h = ((ltc >> 28) & 0x03)*10 + ((ltc >> 24) & 0x0f);
	*m = ((ltc >> 20) & 0x07)*10 + ((ltc >> 16) & 0x0f);
	*s = ((ltc >> 12) & 0x07)*10 + ((ltc >>  8) & 0x0f);
	*f = ((ltc >>  4) & 0x03)*10 + ((ltc >>  0) & 0x0f);
}

u32 hdspe_ltc32_compose(int h, int m, int s, int f)
{
	int H = h/10;
	int M = m/10;
	int S = s/10;
	int F = f/10;
	h -= H*10;
	m -= M*10;
	s -= S*10;
	f -= F*10;
	return ((H&0x03)<<28)|((h&0x0f)<<24)|((M&0x07)<<20)|((m&0x0f)<<16)|
	       ((S&0x07)<<12)|((s&0x0f)<< 8)|((F&0x03)<< 4)|((f&0x0f)    );
}

int hdspe_ltc32_cmp(u32 ltc1, u32 ltc2)
{
	return (int)(ltc1 & 0x3f7f7f3f) - (int)(ltc2 & 0x3f7f7f3f);
}

/**
 * hdspe_ltc32_to_frames_df: Convert LTC to frames since midnight, for  
 * drop-frame 32-bit LTC. 
 * @ltc: 32-bit ltc code to convert to frames since midnight.
 */
static unsigned int hdspe_ltc32_to_frames_df(u32 ltc)
{
	return ((ltc >> 28) & 0x3) * 1078920
	     + ((ltc >> 24) & 0xf) * 107892
	     + ((ltc >> 20) & 0x7) * 17982
	     + ((ltc >> 16) & 0xf) * 1798
	     + ((ltc >> 12) & 0x7) * 300
	     + ((ltc >>  8) & 0xf) * 30
	     + ((ltc >>  4) & 0x3) * 10
	     + ((ltc      ) & 0xf);
}

/**
 * hdspe_ltc32_to_frames_ndf: Convert LTC to frames since midnight, 
 * for non-drop-frame 32-bit LTC. 
 * @ltc: 32-bit LTC code to convert.
 * @fps: LTC frames per second.
 */
static unsigned int hdspe_ltc32_to_frames_ndf(u32 ltc, int fps)
{
	return (((ltc >> 28) & 0x3) * 36000
	      + ((ltc >> 24) & 0xf) * 3600
	      + ((ltc >> 20) & 0x7) * 600
	      + ((ltc >> 16) & 0xf) * 60
	      + ((ltc >> 12) & 0x7) * 10
	      + ((ltc >>  8) & 0xf)) * fps
	      + ((ltc >>  4) & 0x3) * 10
	      + ((ltc      ) & 0xf);
}

unsigned int hdspe_ltc32_to_frames(u32 ltc, int fps, int df)
{
	return df ? hdspe_ltc32_to_frames_df(ltc)
		  : hdspe_ltc32_to_frames_ndf(ltc, fps);
}

u32 hdspe_ltc32_from_frames(int frames, int fps, int df)
{
	int H, h, M, m, S, s, F, f;
	int fpd = hdspe_ltc_fpd(fps, df);
	f = frames % fpd;
	if (f < 0) f += fpd;
	if (!df) {
		s = f / fps;
		f -= s * fps;
		F = f / 10;
		f -= F * 10;
		m = s / 60;
		s -= m * 60;
		S = s / 10;
		s -= S * 10;
		h = m / 60;
		m -= h * 60;
		M = m / 10;
		m -= M * 10;
		H = h / 10;
		h -= H * 10;
	} else {
		H = f / 1078920;
		f -= H * 1078920;
		h = f / 107892;
		f -= h * 107892;
		M = f / 17982;
		f -= M * 17982;
		if (f < 1800) {
			m = 0;
			S = f / 300;
			f -= S * 300;
			s = f / 30;
			f -= s * 30;
			F = f / 10;
			f -= F * 10;
		} else {
			f -= 1800;
			m = f / 1798;
			f -= m * 1798 - 2;
			m ++;
			S = f / 300;
			f -= S * 300;
			s = f / 30;
			f -= s * 30;
			F = f / 10;
			f -= F * 10;
		}
	}
	return ((H&3)<<28)|(h<<24)|(M<<20)|(m<<16)|(S<<12)|(s<<8)|(F<<4)|f;
}

u32 hdspe_ltc32_decr(u32 tci, int fps, int df)
{
	u32 tco = 0;
	int f = tci & 0xf;
	f --;
	if (df && f < 2 && (tci & 0x00007ff0) == 0 && (tci & 0x000f0000) != 0) {
		f -= 2;
	}
	if (f < 0) {
		int F = (tci >> 4) & 0x3;
		F --; f = 9;
		if (F < 0) {
			int s = (tci >> 8) & 0xf;
			s --; F = (fps-1)/10; f = fps-1 - F*10;
			if (s < 0) {
				int S = (tci >> 12) & 0x7;
				S --; s = 9;
				if (S < 0) {
					int m = (tci >> 16) & 0xf;
					m --; S = 5;
					if (m < 0) {
						int M = (tci >> 20) & 0x7;
						M --; m = 9;
						if (M < 0) {
							int h = (tci >> 24) & 0xf;
							h --; M = 5;
							if (h < 0) {
								int H = (tci >> 28) & 0x3;
								H --; h = 9;
								if (H < 0) {
									H = 2; h = 3;
								}
								tco = H << 28;
							} else {
								tco = tci & 0x30000000;
							}
							tco |= h << 24;
						} else {
							tco = tci & 0x3f000000;
						}
						tco |= M << 20;
					} else {
						tco = tci & 0x3f700000;
					}
					tco |= m << 16;
				} else {
					tco = tci & 0x3f7f0000;
				}
				tco |= S << 12;
			} else {
				tco = tci & 0x3f7f7000;
			}
			tco |= s << 8;
		} else {
			tco = tci & 0x3f7f7f00;
		}
		tco |= F << 4;
	} else {
		tco = tci & 0x3f7f7f30;
	}
	return tco | f;
}

u32 hdspe_ltc32_incr(u32 tci, int fps, int df)
{
	int tco = 0;
	int f = (tci >> 0) & 0xf;
	int F = (tci >> 4) & 0x3;
	f ++;
	if (f >= 10) {
		F ++; f = 0;
	}
	if (10*F + f >= fps) {
		int s = (tci >> 8) & 0xf;
		s ++; F = f = 0;
		if (s >= 10) {
			int S = (tci >> 12) & 0x7;
			S ++; s = 0;
			if (S >= 6) {
				int m = (tci >> 16) & 0xf;
				m ++; S = 0;
				if (m >= 10) {
					int M = (tci >> 20) & 0x7;
					M ++; m = 0;
					if (M >= 6) {
						int h = (tci >> 24) & 0xf;
						int H = (tci >> 28) & 0x3;
						h ++; M = 0;
						if (h >= 10) {
							H ++; h = 0;
						}
						if (10*H + h >= 24) {
							H = h = 0;
						}
						tco |= (H << 28)|(h << 24);
					} else {
						tco = tci & 0x3f000000;
					}
					tco |= (M << 20);
				} else {
					if (df) f = 2;
					tco = tci & 0x3f700000;
				}
				tco |= (m << 16);
			} else {
				tco = tci & 0x3f7f0000;				
			}
			tco |= (S << 12);
		} else {
			tco = tci & 0x3f7f7000;
		}
		tco |= (s << 8);
	} else {
		tco = tci & 0x3f7f7f00;
	}
	tco |= (F << 4)|(f << 0);
	return tco;
}

int hdspe_ltc32_running(u32 ltc1, u32 ltc2, int fps, int df)
{
	if (((ltc1+1) & 0x3f7f7f3f) == (ltc2 & 0x3f7f7f3f) ||
	    hdspe_ltc32_cmp(hdspe_ltc32_incr(ltc1, fps, df), ltc2) == 0)
		return +1;
	if (hdspe_ltc32_cmp(hdspe_ltc32_incr(ltc2, fps, df), ltc1) == 0)
		return -1;
	return 0;
}

u32 hdspe_ltc32_add_frames(int n, u32 ltc, int fps, int df)
{
	int frames = hdspe_ltc32_to_frames(ltc, fps, df);
	return hdspe_ltc32_from_frames(frames+n, fps, df);
}

unsigned int hdspe_ltc32_diff_frames(u32 ltc1, u32 ltc2, int fps, int df)
{
	int frames1 = hdspe_ltc32_to_frames(ltc1, fps, df);
	int frames2 = hdspe_ltc32_to_frames(ltc2, fps, df);
	int diff = frames1 - frames2;
	int fpd = hdspe_ltc_fpd(fps, df);
	diff = diff % fpd;
	return diff < 0 ? diff + fpd : diff;
}

#ifdef UNIT_TESTING
/////////////////////////////////////////////////////////////////////////////
// Unit testing.

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int test_compose_parse(int h, int m, int s, int f, int fps, int df)
{
	int h1, m1, s1, f1;	
	u32 ltc = hdspe_ltc32_compose(h, m, s, f);
	hdspe_ltc32_parse(ltc, &h1, &m1, &s1, &f1);
	return ltc_cmp(h, m, s, f, h1, m1, s1, f1) == 0;
}

int test_to_from_frames_32(int h, int m, int s, int f, int fps, int df)
{
	u32 ltc = hdspe_ltc32_compose(h, m, s, f);
	int frames = hdspe_ltc32_to_frames(ltc, fps, df);
	u32 ltc1 = hdspe_ltc32_from_frames(frames, fps, df);
	return hdspe_ltc32_cmp(ltc, ltc1) == 0;
}

int test_incr_decr_32(int h, int m, int s, int f, int fps, int df)
{
	u32 ltc = hdspe_ltc32_compose(h, m, s, f);
	int frames = hdspe_ltc32_to_frames(ltc, fps, df);
	u32 ltc1 = hdspe_ltc32_incr(ltc, fps, df);
	int frames1 = hdspe_ltc32_to_frames(ltc1, fps, df);
	if (frames1 != frames+1) {
		fprintf(stderr, "%08x %d %sfps (frames=%d) +1 = %08x (frames %d)\n",
			ltc, fps, df ? "d" : "", frames,
			ltc1, frames1);
	}
	u32 ltc2 = hdspe_ltc32_add_frames(1, ltc, fps, df);
	if (hdspe_ltc32_cmp(ltc1, ltc2) != 0) {
		fprintf(stderr, "hdspe_ltc32_add_frames +1: %08x != %08x\n",
			ltc2, ltc1);
		return 0;
	}
	ltc2 = hdspe_ltc32_add_frames(-1, ltc2, fps, df);
	if (hdspe_ltc32_cmp(ltc, ltc2) != 0) {
		fprintf(stderr, "hdspe_ltc32_add_frames -1: %08x - 1 = %08x != %08x\n",
			ltc1, ltc2, ltc);
		return 0;
	}
	ltc2 = hdspe_ltc32_decr(ltc1, fps, df);
	if (hdspe_ltc32_cmp(ltc, ltc2) != 0) {
		fprintf(stderr, "hdspe_ltc32_decr: %08x -> %08x != %08x\n",
			ltc1, ltc2, ltc);
		return 0;
        }
	return 1;
}

int test_add_diff_single_32(int n, int h, int m, int s, int f, int fps, int df)
{
	u32 ltc = hdspe_ltc32_compose(h, m, s, f);
	u32 ltc1 = hdspe_ltc32_add_frames(n, ltc, fps, df);
	int diff = hdspe_ltc32_diff_frames(ltc1, ltc, fps, df);
	int fpd = hdspe_ltc_fpd(fps, df);
	int expectdiff = n >= 0 ? n : n + fpd;
	if (diff != expectdiff) {
		fprintf(stderr, "hdspe_ltc32_add_frames: %08x + %d = %08x - . = %d != %d.\n",
			ltc, n, ltc1, diff, n);
		return 0;
	}
	int running = hdspe_ltc32_running(ltc, ltc1, fps, df);
	int expect = (n == 1)||(n == -fpd+1) ? 1 : (n == -1)||(n == fpd-1) ? -1 : 0;
	if (running != expect) {
		fprintf(stderr, "hdspe_ltc32_running: n=%d, %08x -> %08x = %d != %d.\n",
			n, ltc, ltc1, running, expect);
		return 0;
	}
	return 1;
}

int test_add_diff_32(int h, int m, int s, int f, int fps, int df)
{
	int n[] = { 0, 1, fps, fps*60, fps*3600,
		    lrand48() % hdspe_ltc_fpd(fps, df) };
	for (int i=0; i<sizeof(n)/sizeof(n[0]); i++) {
		if (!test_add_diff_single_32(n[i], h, m, s, f, fps, df) ||
		    !test_add_diff_single_32(-n[i], h, m, s, f, fps, df))
			return 0;
	};
	return 1;
}

int test_format(int fps, int df,
		int (*testfun)(int h, int m, int s, int f, int fps, int df),
		char* testname)
{
	fprintf(stderr, "Testing %s @ %d %sfps ...\n",
		testname, fps, df ? "d" : "");
	int h, m, s, f;
	for (h=0; h<24; h++) {
		for (m=0; m<60; m++) {
			for (s=0; s<60; s++) {
				for (f=0; f<fps; f++) {
					if (df && s==0 && f<2 && m%10!=0)
						continue;
					if (!testfun(h, m, s, f, fps, df)) {
						fprintf(stderr,
				"%02d:%02d:%02d:%02d %d %sfps %s test fails.\n",
						h, m, s, f, fps, df ? "d" : "",
						testname);
						return 0;
					}
				}
			}
		}
	}
	return 1;
}

double get_time(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t);
	return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

int test(int (*testfun)(int h, int m, int s, int f, int fps, int df),
	 char* testname)
{
	double t = get_time();	
	int ok =   test_format(24, 0, testfun, testname)
		&& test_format(25, 0, testfun, testname)
		&& test_format(30, 0, testfun, testname)
		&& test_format(30, 1, testfun, testname);
	fprintf(stderr, "%s test took %.3f msec.\n", testname, (get_time()-t)*1e3);
	return ok;
}

int main(int argc, char** agrv)
{
	test(test_compose_parse, "32-bit LTC compose/parse");
	test(test_to_from_frames_32, "32-bit LTC to/from frames conversion");
	test(test_incr_decr_32, "32-bit LTC increment/decrement");
	test(test_add_diff_32, "32-bit LTC add/diff/running");	
	return 0;
}
#endif /*UNIT_TESTING*/

