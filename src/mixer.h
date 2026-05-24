/**
 * @file mixer.h
 * @brief Audio mixing utilities
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef MIXER_H
#define MIXER_H

#include <zephyr/kernel.h>
#include <arm_math.h>

/**
 * @brief Safely mixes two buffers into the first buffer.
 * Both buffers are scaled by 0.5 before addition to prevent clipping.
 * 
 * @param dest The destination buffer (also the first source).
 * @param src The second source buffer.
 * @param size Number of samples in the buffers.
 */
void mixer_combine(q15_t *dest, q15_t *src, uint32_t size);

#endif /* MIXER_H */
