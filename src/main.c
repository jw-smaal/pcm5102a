/**
 * @file main.c
 * @brief Multi-Waveform Generator for PCM5102A (I2S)
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <arm_math.h>

#include "generators.h"

LOG_MODULE_REGISTER(waveform_gen, CONFIG_LOG_DEFAULT_LEVEL);

#define I2S_NODE DT_ALIAS(i2s_tx)
#define SW0_NODE DT_ALIAS(sw0)

#define SAMPLE_RATE CONFIG_AUDIO_SAMPLE_RATE
#define SAMPLE_BIT_WIDTH CONFIG_AUDIO_BIT_WIDTH
#define CHANNELS 2
#define SAMPLES_PER_BLOCK 128
#define BLOCK_SIZE (SAMPLES_PER_BLOCK * CHANNELS * sizeof(int16_t))
#define BLOCK_COUNT 16

/* Attenuation factor: ~0.15 (approx -16dB reduction to target -10dBV) */
#define ATTENUATION_FACTOR 4915

K_MEM_SLAB_DEFINE(audio_slab, BLOCK_SIZE, BLOCK_COUNT, 32);
/* Dummy slab for RX */
K_MEM_SLAB_DEFINE(rx_slab, BLOCK_SIZE, 4, 32);

struct app_state {
	volatile enum waveform_type current_waveform;
	volatile bool mode_changed;
	struct gpio_callback button_cb_data;
};

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct app_state *state = CONTAINER_OF(cb, struct app_state, button_cb_data);
	state->current_waveform = (state->current_waveform + 1) % WAVE_COUNT;
	state->mode_changed = true;
}

/**
 * @brief Interleave mono samples into stereo 16-bit words
 */
static void interleave_stereo(q15_t *mono, int16_t *stereo, uint32_t samples)
{
	for (uint32_t i = 0; i < samples; i++) {
		int16_t val = mono[i];
		stereo[i * 2] = val;     /* Left */
		stereo[i * 2 + 1] = val; /* Right */
	}
}

int main(void)
{
	struct app_state state = {
		.current_waveform = WAVE_SINE,
		.mode_changed = true,
	};
	uint32_t phase = 0;
	struct i2s_config config;
	int ret;

	const struct device *const i2s_dev = DEVICE_DT_GET(I2S_NODE);
	const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

	LOG_INF("Starting I2S Waveform Generator");
	LOG_INF("Configuration: %d Hz, %d-bit, %d channels", 
		SAMPLE_RATE, SAMPLE_BIT_WIDTH, CHANNELS);
	LOG_INF("Output level: ~ -10dBV (not full scale)");

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("I2S device not ready");
		return -ENODEV;
	}

	if (!device_is_ready(button.port)) {
		LOG_ERR("Button GPIO not ready");
		return -ENODEV;
	}

	config.word_size = SAMPLE_BIT_WIDTH;
	config.channels = CHANNELS;
	config.format = I2S_FMT_DATA_FORMAT_I2S;
	config.options = I2S_OPT_FRAME_CLK_CONTROLLER | I2S_OPT_BIT_CLK_CONTROLLER;
	config.frame_clk_freq = SAMPLE_RATE;
	config.block_size = BLOCK_SIZE;
	config.timeout = 2000;

	/* Configure TX */
	config.mem_slab = &audio_slab;
	ret = i2s_configure(i2s_dev, I2S_DIR_TX, &config);
	if (ret < 0) {
		LOG_ERR("I2S TX config failed: %d", ret);
		return ret;
	}

	/* Configure RX */
	config.mem_slab = &rx_slab;
	ret = i2s_configure(i2s_dev, I2S_DIR_RX, &config);
	if (ret < 0) {
		LOG_ERR("I2S RX config failed: %d", ret);
		return ret;
	}

	/* Pre-calculate the frequency-to-phase-increment factor */
	/* Constant for 32-bit phase accumulator: (2^32 / SAMPLE_RATE) */
	const float phase_inc_factor = 4294967296.0f / SAMPLE_RATE;
	uint32_t phase_inc = (uint32_t)(CONFIG_WAVEFORM_FREQUENCY * phase_inc_factor);

	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&state.button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &state.button_cb_data);

	/* Start RX first to enable clock generator */
	ret = i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
	if (ret < 0) {
		LOG_WRN("I2S RX start failed: %d", ret);
	}

	q15_t mono_buffer[SAMPLES_PER_BLOCK];
	void *mem_block;
	bool started = false;

	while (1) {
		enum waveform_type mode = state.current_waveform;

		if (state.mode_changed) {
			LOG_INF("Active Waveform: %s", get_waveform_name(mode));
			state.mode_changed = false;
		}

		/* Generate Raw Waveform Batch */
		generate_waveform_batch(mode, mono_buffer, SAMPLES_PER_BLOCK, &phase, phase_inc);

		/* Apply digital attenuation using CMSIS-DSP (scale Q15 by ~0.15) */
		arm_scale_q15(mono_buffer, ATTENUATION_FACTOR, 0, mono_buffer, SAMPLES_PER_BLOCK);

		/* Get memory block from slab */
		ret = k_mem_slab_alloc(&audio_slab, &mem_block, K_FOREVER);
		if (ret < 0) {
			continue;
		}

		/* Interleave into memory block */
		interleave_stereo(mono_buffer, (int16_t *)mem_block, SAMPLES_PER_BLOCK);

		/* Send to I2S */
		ret = i2s_write(i2s_dev, mem_block, BLOCK_SIZE);
		if (ret < 0) {
			k_mem_slab_free(&audio_slab, mem_block);
			k_sleep(K_MSEC(100));
			continue;
		}

		/* Start TX after first block is queued */
		if (!started) {
			ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
			if (ret < 0) {
				LOG_ERR("I2S TX start failed: %d", ret);
				return ret;
			}
			LOG_INF("Multi-Waveform Engine Started");
			started = true;
		}
	}
	return 0;
}
