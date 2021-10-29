// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file hdspe_ltc_math.h
 * @brief RME HDSPe 32-bit LTC code optimised math for TCO module.
 *
 * 20210930,1001,08 - Philippe.Bekaert@uhasselt.be
 *
 * TODO: 24 or 25 fps drop-frame format ??
 */

#ifndef HDSPE_LTC_MATH_H
#define HDSPE_LTC_MATH_H

#include <linux/types.h>

/**
 * hdspe_ltc_fpd: Frames per day. 
 * @fps: LTC frames per second (24,25,30).
 * @df: 30DF drop frame LTC format (TRUE or FALSE).
 * Returns number of frames in a full day.
 */
extern int hdspe_ltc_fpd(int fps, int df);

/**
 * hdspe_ltc32_parse: Parse 32-bit LTC code into hours, minutes, seconds and 
 * frames. 
 * @ltc: LTC code to parse.
 * @h: hours.
 * @m: minutes.
 * @s seconds.
 * @f: frames.
 */
extern void hdspe_ltc32_parse(u32 ltc, int *h, int *m, int *s, int *f);

/**
 * hdspe_ltc32_compose: Convert hours, minutes, seconds and frames quadruple 
 * into a 32-bit LTC code. 
 * @h: hours 0 .. 23
 * @m: minutes 0 .. 59
 * @s seconds 0 .. 59
 * @f: frames 0 .. @fps-1
 * Returns 32-bit LTC code.
 */
extern u32 hdspe_ltc32_compose(int h, int m, int s, int f);

/**
 * hdspe_ltc32_cmp: Compare two 32-bit LTC codes. 
 * @ltc1, @ltc2: two LTC codes to compare.
 * Returns >0 if @ltc1 > @ltc2, <0 if @ltc1 < @ltc2 and 0 if equal. 
 */
extern int hdspe_ltc32_cmp(u32 ltc1, u32 ltc2);

/**
 * hdspe_ltc32_to_frames: Convert 32-bit LTC code to frames since midnight.
 * @ltc: LTC code to convert
 * @fps: LTC frames per second (24,25,30).
 * @df: 30DF drop frame LTC format (TRUE or FALSE).
 * Returns number of frames since midnight.
 */
extern unsigned int hdspe_ltc32_to_frames(u32 ltc, int fps, int df);

/**
 * hdspe_ltc32_from_frames: Convert frame count since midnight to 
 * 32-bit LTC code.
 * @frames: number of frames since midnight. If negative or larger than
 * fpd(), it is brought into 0 ... fpd()-1 range.
 * @fps: LTC frames per second (24,25,30).
 * @df: 30DF drop frame LTC format (TRUE or FALSE).
 * Returns 32-bit LTC code.
 */
extern u32 hdspe_ltc32_from_frames(int frames, int fps, int df);

/**
 * hdspe_ltc32_decr: Decrement 32-bit LTC by one frame.
 * @ltc: LTC code to decrement. Assumed to be in 0 ... fpd()-1 range.
 * @fps: LTC frames per second (24,25,30).
 * @df: 30DF drop frame LTC format (TRUE or FALSE).
 * Returns 32-bit LTC code, one frame less than @ltc. Returns 23:59:59:@fps-1
 * if @ltc is 00:00:00:00.
 */
extern u32 hdspe_ltc32_decr(u32 ltc, int fps, int df);

/**
 * hdspe_ltc32_incr: Increment 32-bit LTC by one frame.
 * @ltc: LTC code to increment. Assumed to be in 0 ... fpd()-1 range.
 * @fps: LTC frames per second (24,25,30).
 * @df: 30DF drop frame LTC format (TRUE or FALSE).
 * Returns 32-bit LTC code, one frame ahead of @ltc. Returns 00:00:00:00
 * if @ltc is 23:59:59:@fps-1.
 */
extern u32 hdspe_ltc32_incr(u32 ltc, int fps, int df);

/**
 * hdspe_ltc32_running: LTC running direction.
 * @ltc1, @ltc2 : 32-bit LTC codes to compare.
 * @fps: LTC frames per second (24,25,30).
 * @df: 30DF drop frame LTC format (TRUE or FALSE).
 * Returns +1 if ltc2 is one frame ahead of ltc1, -1 if ltc2 is
 * one frame before ltc1, and 0 in other cases. Use this to determine LTC
 * running direction (1 = forward, -1 = backward, 0 = stationary or jumping).
 */
extern int hdspe_ltc32_running(u32 ltc1, u32 ltc2, int fps, int df);

/**
 * hdspe_ltc32_add_frames: add n frames to 32-bit LTC code. 
 * @n: number of frames to add (if positive) or subtract (if negative).
 * @ltc: 32-bit LTC code to add frames to.
 * @fps: LTC frames per second (24,25,30).
 * @df: 30DF drop frame LTC format (TRUE or FALSE).
 * Returns @ltc + @n frames. LTC code wraps from 23:59:59:@fps-1 to 
 * 00:00:00:00 and back.
 */
extern u32 hdspe_ltc32_add_frames(int n, u32 ltc, int fps, int df);

/**
 * hdspe_ltc32_diff_frames: return LTC code difference in frames.
 * @ltc1, @ltc2: LTC codes to subtract.
 * @fps: LTC frames per second (24,25,30).
 * @df: 30DF drop frame LTC format (TRUE or FALSE).
 * Returns @ltc1 - @ltc2 in frames. Return value is always in the range 
 * 0 ... fpd()-1. 
 */
extern unsigned int hdspe_ltc32_diff_frames(u32 ltc1, u32 ltc2,
				      int fps, int df);

#endif /* HDSPE_LTC_MATH_H */
