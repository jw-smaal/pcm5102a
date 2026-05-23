/**
 * @file generators.h
 * @brief Waveform generator definitions
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef GENERATORS_H
#define GENERATORS_H

#include <zephyr/kernel.h>
#include <arm_math.h>

#define PHASE_FULL_CYCLE 32768

/* Waveform selection enum */
enum waveform_type {
	WAVE_SINE = 0,
	WAVE_SQUARE,
	WAVE_SAWTOOTH,
	WAVE_TRIANGLE,
	WAVE_USR1,
	WAVE_USR2,
	WAVE_USR3,
	WAVE_USR4,
	WAVE_COUNT
};

/**
 * @brief Generates a batch of samples for the specified waveform.
 *
 * @param mode The type of waveform to generate.
 * @param buffer Output buffer for raw q15_t samples.
 * @param size Number of samples to generate.
 * @param phase Pointer to the persistent phase accumulator.
 * @param phase_inc The amount to increment phase per sample.
 */
void generate_waveform_batch(enum waveform_type mode, q15_t *buffer, uint32_t size, uint16_t *phase,
			     uint16_t phase_inc);

/**
 * @brief Returns the human-readable name of a waveform.
 */
const char *get_waveform_name(enum waveform_type type);

#endif /* GENERATORS_H */
