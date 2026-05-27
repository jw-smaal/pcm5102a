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

/* This function can be optimized with CMCIS this is the old version */
void effect_bitcrush_old(q15_t *buffer, uint32_t size, uint8_t bits)
{
	if (bits >= 16) {
		return;
	}

	uint16_t mask = 0xFFFF << (16 - bits);

	for (uint32_t i = 0; i < size; i++) {
		buffer[i] = (q15_t)(buffer[i] & mask);
	}
}

void effect_bitcrush(q15_t *buffer, uint32_t size, uint8_t bits)
{
    if (bits >= 16 || bits == 0) {
        return;
    }

	/* For shift right we need a negative number */ 
    int8_t shift_right = (int8_t)(bits - 16); 
	/* For shift left we need positive number */ 
    int8_t shift_left  = (int8_t)(16 - bits); 

    /* Shift to the right we discard precision but we keep the sign bit. */
	arm_shift_q15(buffer, shift_right, buffer, size);

    /* Shift back to the left to restore the amplitude */ 
    arm_shift_q15(buffer, shift_left, buffer, size);
}


void effect_attenuate(q15_t *buffer, uint32_t size, q15_t factor)
{
	arm_scale_q15(buffer, factor, 0, buffer, size);
}
