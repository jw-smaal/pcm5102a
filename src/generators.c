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
static inline q15_t calc_sine(uint16_t phase)
{
	return arm_sin_q15((q15_t)phase);
}

/**
 * @brief Single Square point generator
 */
static inline q15_t calc_square(uint16_t phase)
{
	if (phase < (PHASE_FULL_CYCLE / 2)) {
		return 32767;
	} else {
		return -32768;
	}
}

/**
 * @brief Single Sawtooth point generator
 */
static inline q15_t calc_sawtooth(uint16_t phase)
{
	/* Linear ramp: 0..32767 maps to -32768..32767 */
	return (q15_t)(((int32_t)phase * 2) - 32768);
}

/**
 * @brief Single Triangle point generator
 */
static inline q15_t calc_triangle(uint16_t phase)
{
	if (phase < (PHASE_FULL_CYCLE / 2)) {
		/* Rising edge: 0..16383 maps to -32768..32767 */
		return (q15_t)(((int32_t)phase * 4) - 32768);
	} else {
		/* Falling edge: 16384..32767 maps to 32767..-32768 */
		return (q15_t)(32767 - (((int32_t)phase - 16384) * 4));
	}
}

static inline q15_t calc_usr1(uint16_t phase)
{
	return (q15_t)(32768 - (((int32_t)phase - 32768) * 2));
}

void generate_waveform_batch(enum waveform_type mode, q15_t *buffer, uint32_t size, uint16_t *phase,
			     uint16_t phase_inc)
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

		/* Global phase increment and wrapping */
		*phase = (*phase + phase_inc) % PHASE_FULL_CYCLE;
	}
}
