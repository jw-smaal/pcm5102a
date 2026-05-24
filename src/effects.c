/**
 * @file effects.c
 * @brief DSP effects implementation
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include "effects.h"

void effect_bitcrush(q15_t *buffer, uint32_t size, uint8_t bits)
{
	if (bits >= 16) {
		return;
	}

	/* Calculate a bitmask to keep 'bits' of precision.
	 * e.g., if bits = 4, we keep the top 4 bits of a 16-bit sample.
	 * mask = 0xFFFF << (16 - 4)
	 */
	uint16_t mask = 0xFFFF << (16 - bits);

	for (uint32_t i = 0; i < size; i++) {
		buffer[i] = (q15_t)(buffer[i] & mask);
	}
}

void effect_attenuate(q15_t *buffer, uint32_t size, q15_t factor)
{
	/* CMSIS-DSP SIMD accelerated scaling */
	arm_scale_q15(buffer, factor, 0, buffer, size);
}
