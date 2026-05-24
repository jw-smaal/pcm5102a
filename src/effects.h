/**
 * @file effects.h
 * @brief DSP effects for the audio pipeline
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef EFFECTS_H
#define EFFECTS_H

#include <zephyr/kernel.h>
#include <arm_math.h>

/** 
 * @brief Simply AND with 0xBEEF in the buffer. 
 * @param size Number of samples in the buffer.
 */
void effect_bitbeef(q15_t *buffer, uint32_t size);

/**
 * @brief Bitcrushes the buffer in-place to the specified bit depth.
 * 
 * @param buffer The q15_t buffer to process.
 * @param size Number of samples in the buffer.
 * @param bits The bit depth to crush to (e.g., 4, 8, 12, 16). 16 = no crush.
 */
void effect_bitcrush(q15_t *buffer, uint32_t size, uint8_t bits);

/**
 * @brief Applies digital attenuation/volume scaling to the buffer.
 * 
 * @param buffer The q15_t buffer to process.
 * @param size Number of samples in the buffer.
 * @param factor The attenuation factor in Q15 (0..32767).
 */
void effect_attenuate(q15_t *buffer, uint32_t size, q15_t factor);

#endif /* EFFECTS_H */
