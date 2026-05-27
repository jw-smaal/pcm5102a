/**
 * @file generators.h
 * @brief Waveform generator definitions
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef GENERATORS_H
#define GENERATORS_H

#include <zephyr/kernel.h>
#include <arm_math.h>

#define HALF_CYCLE 0x80000000 
#define Q15T_MAX 32767
#define Q15T_MIN -32768

/* Waveform selection enum */
enum waveform_type {
	WAVE_SINE = 0,
	WAVE_SQUARE,
	WAVE_SAWTOOTH,
	WAVE_TRIANGLE,
	WAVE_USR1,
	WAVE_USR2, /* FM SINE */
	WAVE_PWM,
	WAVE_COUNT
};

/**
 * @brief Generates a batch of samples for the specified waveform.
 *
 * @param mode The type of waveform to generate.
 * @param buffer Output buffer for raw q15_t samples.
 * @param size Number of samples to generate.
 * @param phase Pointer to the persistent 32-bit phase accumulator.
 * @param phase_inc The 32-bit amount to increment phase per sample.
 */
void generate_waveform_batch(enum waveform_type mode, q15_t *buffer, uint32_t size, uint32_t *phase,
			     uint32_t phase_inc);

/**
 * @brief Returns the human-readable name of a waveform.
 */
const char *get_waveform_name(enum waveform_type type);

#endif /* GENERATORS_H */
