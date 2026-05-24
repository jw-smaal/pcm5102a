/**
 * @file generators.c
 * @brief Waveform generator implementation
 * @author Jan-Willem Smaal <usenet@gispen.org>
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
		return "USR2";
	case WAVE_USR3:
		return "USR3";
	case WAVE_USR4:
		return "USR4";
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
		default:
			buffer[i] = 0;
			break;
		}

		/* Global 32-bit phase increment with natural wrapping */
		*phase += phase_inc;
	}
}
