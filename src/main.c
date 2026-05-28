/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief Multi-Threaded Waveform Generator for PCM5102A (I2S)
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <arm_math.h>

#include "generators.h"
#include "effects.h"
#include "mixer.h"

LOG_MODULE_REGISTER(waveform_gen, CONFIG_LOG_DEFAULT_LEVEL);

/* --- Configuration --- */
#define I2S_NODE DT_ALIAS(i2s_tx)
#define SW_WAVE_NODE DT_ALIAS(sw_wave)
#define SW_MIX_NODE  DT_ALIAS(sw_mix)

#define SAMPLE_RATE CONFIG_AUDIO_SAMPLE_RATE
#define SAMPLE_BIT_WIDTH CONFIG_AUDIO_BIT_WIDTH
#define CHANNELS 2
#define SAMPLES_PER_BLOCK 128
#define BLOCK_SIZE (SAMPLES_PER_BLOCK * CHANNELS * sizeof(int16_t))
#define BLOCK_COUNT 16

/* Constants for Phase Math */
#define NCO_RANGE_F 4294967296.0f

/* Attenuation factor: ~0.15 (approx -16dB reduction to target -10dBV) */
#define ATTENUATION_FACTOR 4915

/* Audio Thread Settings */
#define AUDIO_STACK_SIZE 2048
#define AUDIO_PRIORITY -2

/* Slab for TX */
K_MEM_SLAB_DEFINE(audio_slab, BLOCK_SIZE, BLOCK_COUNT, 32);
/* Dummy slab for RX */
K_MEM_SLAB_DEFINE(rx_slab, BLOCK_SIZE, 4, 32); 

enum effect_mode {
	EFFECT_BYPASS = 0,
	EFFECT_BITCRUSH,
	EFFECT_BITBEEF,
	EFFECT_COUNT
};

struct synth_state {
	struct k_mutex lock;
	enum waveform_type current_waveform;
	uint32_t frequency;
	uint32_t phase_inc;
	bool mode_changed;
	bool mix_mode;
	enum effect_mode effect;
};

static struct synth_state g_state = {
	.current_waveform = WAVE_SINE,
	.frequency = 440,
	.mode_changed = true,
	.mix_mode = false,
	.effect = EFFECT_BYPASS,
};

static const struct device *const i2s_dev = DEVICE_DT_GET(I2S_NODE);

/* --- Helpers --- */

static void update_phase_inc(struct synth_state *state)
{
	/* Constant for 32-bit phase accumulator: (2^32 / SAMPLE_RATE) */
	const float phase_inc_factor = NCO_RANGE_F / SAMPLE_RATE;
	state->phase_inc = (uint32_t)(state->frequency * phase_inc_factor);
}

static void interleave_stereo(q15_t *mono, int16_t *stereo, uint32_t samples)
{
	for (uint32_t i = 0; i < samples; i++) {
		int16_t val = mono[i];
		stereo[i * 2] = val;     /* Left */
		stereo[i * 2 + 1] = val; /* Right */
	}
}

/* --- Threads --- */

void audio_thread_fn(void *p1, void *p2, void *p3)
{
	struct synth_state *state = (struct synth_state *)p1;
	ARG_UNUSED(p2); ARG_UNUSED(p3);
	uint32_t master_phase = 0;
	q15_t mono_buffer[SAMPLES_PER_BLOCK];
	q15_t mono_buffer2[SAMPLES_PER_BLOCK];
	void *mem_block;
	bool started = false;
	int ret;

	LOG_INF("Audio Engine Thread Started (Priority %d)", AUDIO_PRIORITY);

	while (1) {
		enum waveform_type mode;
		uint32_t phase_inc;
		bool mix_active;
		enum effect_mode effect;
		uint32_t local_phase;

		/* Synchronize control parameters */
		k_mutex_lock(&state->lock, K_FOREVER);
		mode = state->current_waveform;
		phase_inc = state->phase_inc;
		mix_active = state->mix_mode;
		effect = state->effect;

		if (state->mode_changed) {
			const char *fx_name = "BYPASS";

			if (effect == EFFECT_BITCRUSH) {
				fx_name = "BITCRUSH (4-bit)";
			} else if (effect == EFFECT_BITBEEF) {
				fx_name = "BITBEEF (0xBEEF)";
			}

			if (mix_active) {
				LOG_INF("Audio Mode -> %s (Mixed) [Effect: %s]", 
					get_waveform_name(mode), fx_name);
			} else {
				LOG_INF("Audio Mode -> %s [Effect: %s]", 
					get_waveform_name(mode), fx_name);
			}
			state->mode_changed = false;
		}
		k_mutex_unlock(&state->lock);

		/* DSP Pipeline */
		local_phase = master_phase;
		generate_waveform_batch(mode, mono_buffer, SAMPLES_PER_BLOCK, &local_phase, phase_inc);

		if (mix_active) {
			uint32_t harmonic_phase = master_phase * 2;
			uint32_t harmonic_inc = phase_inc * 2;
			uint32_t dummy_phase = harmonic_phase;
			
			generate_waveform_batch(mode, mono_buffer2, SAMPLES_PER_BLOCK, &dummy_phase, harmonic_inc);
			mixer_combine(mono_buffer, mono_buffer2, SAMPLES_PER_BLOCK);
		}

		switch (effect) {
		case EFFECT_BITCRUSH:
			effect_bitcrush(mono_buffer, SAMPLES_PER_BLOCK, 4);
			break;
		case EFFECT_BITBEEF:
			effect_bitbeef(mono_buffer, SAMPLES_PER_BLOCK);
			break;
		default:
			break;
		}

		effect_attenuate(mono_buffer, SAMPLES_PER_BLOCK, ATTENUATION_FACTOR);
		master_phase = local_phase;

		/* Output Processing */
		ret = k_mem_slab_alloc(&audio_slab, &mem_block, K_FOREVER);
		if (ret < 0) {
			continue;
		}

		interleave_stereo(mono_buffer, (int16_t *)mem_block, SAMPLES_PER_BLOCK);

		/* 
		 * Real-time audio: If the queue is full (-EAGAIN), do not block or retry.
		 * The time window has passed, so we simply drop this block and move on.
		 */
		ret = i2s_write(i2s_dev, mem_block, BLOCK_SIZE);
		if (ret < 0) {
			k_mem_slab_free(&audio_slab, mem_block);
			if (ret != -EAGAIN) {
				LOG_ERR("I2S write failed: %d", ret);
				return;
			}
			/* If EAGAIN, we dropped the block. Proceed to next loop immediately. */
			continue;
		}

		if (!started) {
			ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
			if (ret < 0) {
				LOG_ERR("I2S TX start failed: %d", ret);
				return;
			}
			started = true;
		}
	}
}

/* --- User Interface / Control --- */

void wave_button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);

	k_mutex_lock(&g_state.lock, K_NO_WAIT);
	g_state.current_waveform = (g_state.current_waveform + 1) % WAVE_COUNT;
	g_state.mode_changed = true;
	k_mutex_unlock(&g_state.lock);
}

void mix_button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);

	k_mutex_lock(&g_state.lock, K_NO_WAIT);
	/* Cycle through effects: BYPASS -> BITCRUSH -> BITBEEF */
	g_state.effect = (g_state.effect + 1) % EFFECT_COUNT;
	g_state.mode_changed = true;
	k_mutex_unlock(&g_state.lock);
}

K_THREAD_DEFINE(audio_tid, AUDIO_STACK_SIZE, audio_thread_fn, &g_state, NULL, NULL,
		AUDIO_PRIORITY, 0, K_TICKS_FOREVER);

int main(void)
{
	struct i2s_config config;
	const struct gpio_dt_spec btn_wave = GPIO_DT_SPEC_GET(SW_WAVE_NODE, gpios);
	struct gpio_callback wave_cb_data;
	int ret;

	/* Mix button is optional (e.g. MCXE31B only has one button) */
	bool has_mix_btn = DT_NODE_EXISTS(SW_MIX_NODE);
	struct gpio_dt_spec btn_mix;
	struct gpio_callback mix_cb_data;

	LOG_INF("Starting Multi-Threaded I2S Audio Engine");
	k_mutex_init(&g_state.lock);
	update_phase_inc(&g_state);

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("I2S device not ready");
		return -ENODEV;
	}

	/* Configure Hardware ... */

	config.word_size = SAMPLE_BIT_WIDTH;
	config.channels = CHANNELS;
	config.format = I2S_FMT_DATA_FORMAT_I2S;
	config.options = I2S_OPT_FRAME_CLK_CONTROLLER | I2S_OPT_BIT_CLK_CONTROLLER;
	config.frame_clk_freq = SAMPLE_RATE;
	config.block_size = BLOCK_SIZE;
	config.timeout = 2000;

	config.mem_slab = &audio_slab;
	ret = i2s_configure(i2s_dev, I2S_DIR_TX, &config);
	if (ret < 0) {
		LOG_ERR("I2S TX config failed: %d", ret);
		return ret;
	}

	config.mem_slab = &rx_slab;
	ret = i2s_configure(i2s_dev, I2S_DIR_RX, &config);
	if (ret < 0) {
		LOG_ERR("I2S RX config failed: %d", ret);
		return ret;
	}

	/* Initialize UI */
	gpio_pin_configure_dt(&btn_wave, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&btn_wave, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&wave_cb_data, wave_button_pressed, BIT(btn_wave.pin));
	
	/* Setup CONTAINER_OF pointer logic for callbacks */
	wave_cb_data.handler = (gpio_callback_handler_t)wave_button_pressed;
	
	gpio_add_callback(btn_wave.port, &wave_cb_data);

	if (has_mix_btn) {
		btn_mix = (struct gpio_dt_spec)GPIO_DT_SPEC_GET(SW_MIX_NODE, gpios);
		gpio_pin_configure_dt(&btn_mix, GPIO_INPUT);
		gpio_pin_interrupt_configure_dt(&btn_mix, GPIO_INT_EDGE_TO_ACTIVE);
		gpio_init_callback(&mix_cb_data, mix_button_pressed, BIT(btn_mix.pin));
		
		/* Setup CONTAINER_OF pointer logic for callbacks */
		mix_cb_data.handler = (gpio_callback_handler_t)mix_button_pressed;
		
		gpio_add_callback(btn_mix.port, &mix_cb_data);
	}

	/* Start Clocks (via RX) */
	ret = i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
	if (ret < 0) {
		LOG_WRN("I2S RX start failed: %d", ret);
	}

	/* Launch Audio Engine */
	k_thread_start(audio_tid);

	LOG_INF("Control Plane Ready.");
	LOG_INF("SW2: Cycle Waveform Type");
	LOG_INF("SW3: Toggle Bitcrush Effect (A/B Comparison)");

	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
