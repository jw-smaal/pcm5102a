/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file mixer.c
 * @brief Audio mixing implementation
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include "mixer.h"

void mixer_combine(q15_t *dest, q15_t *src, uint32_t size)
{
	arm_scale_q15(dest, 0x4000, 0, dest, size);
	arm_scale_q15(src, 0x4000, 0, src, size);
	arm_add_q15(dest, src, dest, size);
}
