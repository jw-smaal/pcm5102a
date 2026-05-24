/**
 * @file generators.c
 * @brief Waveform generation using Direct Digital Synthesis (DDS)
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * 
 * This implementation is based on the seminal DDS architecture described in:
 * "A Digital Frequency Synthesizer" by J. Tierney, C. M. Rader, and B. Gold (1971).
 * 
 * Phase Modulation (FM) implementation in calc_usr2 is based on:
 * "The Synthesis of Complex Audio Spectra by Means of Frequency Modulation" 
 * by John M. Chowning (1973).
 */

#include "generators.h"

const char *get_waveform_name(enum waveform_type type)
{
	switch (type) {
	case WAVE_SINE:
		return "SINE";
	case WAVE_SQUARE:
		return "SQUARE";
	case WAVE_SAWTOOTH:
		return "SAWTOOTH";
	case WAVE_TRIANGLE:
		return "TRIANGLE";
	case WAVE_USR1:
		return "USR1";
	case WAVE_USR2:
		return "FM SINE";
	default:
		return "UNKNOWN";
	}
}

/**
 * @brief Single Sine point generator
 */
static inline q15_t calc_sine(uint32_t phase)
{
	/* Use top 15 bits for arm_sin_q15 (expects 0..32767) */
	return arm_sin_q15((q15_t)(phase >> 17));
}

/**
 * @brief Single Square point generator
 */
static inline q15_t calc_square(uint32_t phase)
{
	/* MSB determines the half-cycle */
	if (phase < 0x80000000) {
		return 32767;
	} else {
		return -32768;
	}
}

/**
 * @brief Single Sawtooth point generator
 */
static inline q15_t calc_sawtooth(uint32_t phase)
{
	/* Map 32-bit phase to q15_t range [-32768, 32767] */
	return (q15_t)((phase >> 16) - 32768);
}

/**
 * @brief Single Triangle point generator
 */
static inline q15_t calc_triangle(uint32_t phase)
{
	if (phase < 0x80000000) {
		/* Rising edge: 0..0x7FFFFFFF -> -32768..32767 */
		return (q15_t)((phase >> 15) - 32768);
	} else {
		/* Falling edge: 0x80000000..0xFFFFFFFF -> 32767..-32768 */
		return (q15_t)(32767 - ((phase - 0x80000000) >> 15));
	}
}

static inline q15_t calc_usr1(uint32_t phase)
{
	/* Simple example pulse wave / specialized ramp */
	return (q15_t)(32768 - (phase >> 16));
}

/**
 * @brief Simple Phase Modulation (FM)
 */
static inline q15_t calc_usr2(uint32_t phase)
{
	/* 2x frequency modulator */
	q15_t mod = arm_sin_q15((q15_t)((phase * 2) >> 17));

	/* Scale modulator for phase offset (modulation index) */
	uint32_t phase_offset = (uint32_t)mod << 14;

	return arm_sin_q15((q15_t)((phase + phase_offset) >> 17));
}

void generate_waveform_batch(enum waveform_type mode, q15_t *buffer, uint32_t size, uint32_t *phase,
			     uint32_t phase_inc)
{
	for (uint32_t i = 0; i < size; i++) {
		switch (mode) {
		case WAVE_SINE:
			buffer[i] = calc_sine(*phase);
			break;
		case WAVE_SQUARE:
			buffer[i] = calc_square(*phase);
			break;
		case WAVE_SAWTOOTH:
			buffer[i] = calc_sawtooth(*phase);
			break;
		case WAVE_TRIANGLE:
			buffer[i] = calc_triangle(*phase);
			break;
		case WAVE_USR1:
			buffer[i] = calc_usr1(*phase);
			break;
		case WAVE_USR2:
			buffer[i] = calc_usr2(*phase);
			break;
		default:
			buffer[i] = 0;
			break;
		}

		*phase += phase_inc;
	}
}

