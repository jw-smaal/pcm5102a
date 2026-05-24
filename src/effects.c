/**
 * @file effects.c
 * @brief DSP effects implementation
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include "effects.h"


void effect_bitbeef(q15_t *buffer, uint32_t size)
{
	for (uint32_t i = 0; i < size; i++) {
		buffer[i] = (q15_t)(buffer[i] & 0xBEEF);
	}
}

void effect_bitcrush(q15_t *buffer, uint32_t size, uint8_t bits)
{
	if (bits >= 16) {
		return;
	}

	uint16_t mask = 0xFFFF << (16 - bits);

	for (uint32_t i = 0; i < size; i++) {
		buffer[i] = (q15_t)(buffer[i] & mask);
	}
}

void effect_attenuate(q15_t *buffer, uint32_t size, q15_t factor)
{
	arm_scale_q15(buffer, factor, 0, buffer, size);
}
