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

static struct synth_state global_state = {
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
	const float phase_inc_factor = 4294967296.0f / SAMPLE_RATE;
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
	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
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

		/* Safely copy control parameters */
		k_mutex_lock(&global_state.lock, K_FOREVER);
		mode = global_state.current_waveform;
		phase_inc = global_state.phase_inc;
		mix_active = global_state.mix_mode;
		effect = global_state.effect;

		if (global_state.mode_changed) {
			const char *fx_name = "BYPASS";
			if (effect == EFFECT_BITCRUSH) fx_name = "BITCRUSH (4-bit)";
			if (effect == EFFECT_BITBEEF) fx_name = "BITBEEF (0xBEEF)";

			LOG_INF("Audio Mode -> %s %s [Effect: %s]", 
				get_waveform_name(mode), 
				mix_active ? "(Mixed)" : "",
				fx_name);
			global_state.mode_changed = false;
		}
		k_mutex_unlock(&global_state.lock);

		/* DSP PIPELINE ASSEMBLY LINE */

		/* 1. Generate Osc 1 (Main Frequency) */
		local_phase = master_phase;
		generate_waveform_batch(mode, mono_buffer, SAMPLES_PER_BLOCK, &local_phase, phase_inc);

		/* 2. Generate and Mix Osc 2 (Harmonic) */
		if (mix_active) {
			uint32_t harmonic_phase = master_phase * 2;
			uint32_t harmonic_inc = phase_inc * 2;
			uint32_t dummy_phase = harmonic_phase;
			
			generate_waveform_batch(mode, mono_buffer2, SAMPLES_PER_BLOCK, &dummy_phase, harmonic_inc);

			/* Modular Mixer: Dest = (Dest*0.5) + (Src*0.5) */
			mixer_combine(mono_buffer, mono_buffer2, SAMPLES_PER_BLOCK);
		}

		/* 3. Apply Global Effects */
		switch (effect) {
		case EFFECT_BITCRUSH:
			effect_bitcrush(mono_buffer, SAMPLES_PER_BLOCK, 4);
			break;
		case EFFECT_BITBEEF:
			effect_bitbeef(mono_buffer, SAMPLES_PER_BLOCK);
			break;
		default:
			/* BYPASS: Do nothing, zero overhead */
			break;
		}

		/* 4. Apply Master Output Attenuation */
		effect_attenuate(mono_buffer, SAMPLES_PER_BLOCK, ATTENUATION_FACTOR);

		/* Update master phase for the next block */
		master_phase = local_phase;

		/* --- End of Pipeline --- */

		/* 3. Get memory block from slab */
		ret = k_mem_slab_alloc(&audio_slab, &mem_block, K_FOREVER);
		if (ret < 0) {
			continue;
		}

		/* 4. Interleave into memory block */
		interleave_stereo(mono_buffer, (int16_t *)mem_block, SAMPLES_PER_BLOCK);

		/* 5. Send to I2S */
		ret = i2s_write(i2s_dev, mem_block, BLOCK_SIZE);
		if (ret < 0) {
			k_mem_slab_free(&audio_slab, mem_block);
			k_sleep(K_MSEC(100));
			continue;
		}

		/* 6. Start I2S Trigger on first block */
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

K_THREAD_DEFINE(audio_tid, AUDIO_STACK_SIZE, audio_thread_fn, NULL, NULL, NULL,
		AUDIO_PRIORITY, 0, K_TICKS_FOREVER);

/* --- User Interface / Control --- */

void wave_button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);

	k_mutex_lock(&global_state.lock, K_NO_WAIT);
	global_state.current_waveform = (global_state.current_waveform + 1) % WAVE_COUNT;
	global_state.mode_changed = true;
	k_mutex_unlock(&global_state.lock);
}

void mix_button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);

	k_mutex_lock(&global_state.lock, K_NO_WAIT);
	/* Cycle through effects: BYPASS -> BITCRUSH -> BITBEEF */
	global_state.effect = (global_state.effect + 1) % EFFECT_COUNT;
	global_state.mode_changed = true;
	k_mutex_unlock(&global_state.lock);
}

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
	k_mutex_init(&global_state.lock);
	update_phase_inc(&global_state);

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
	gpio_add_callback(btn_wave.port, &wave_cb_data);

	if (has_mix_btn) {
		btn_mix = (struct gpio_dt_spec)GPIO_DT_SPEC_GET(SW_MIX_NODE, gpios);
		gpio_pin_configure_dt(&btn_mix, GPIO_INPUT);
		gpio_pin_interrupt_configure_dt(&btn_mix, GPIO_INT_EDGE_TO_ACTIVE);
		gpio_init_callback(&mix_cb_data, mix_button_pressed, BIT(btn_mix.pin));
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
