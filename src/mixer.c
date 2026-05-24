/**
 * @file mixer.c
 * @brief Audio mixing implementation
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include "mixer.h"

void mixer_combine(q15_t *dest, q15_t *src, uint32_t size)
{
	/* Scale both by 0.5 (0x4000 in Q15) to prevent overflow during addition */
	arm_scale_q15(dest, 0x4000, 0, dest, size);
	arm_scale_q15(src, 0x4000, 0, src, size);
	
	/* Vector addition */
	arm_add_q15(dest, src, dest, size);
}
