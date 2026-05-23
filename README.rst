PCM5102A Multi-Waveform Generator (FRDM-MCXN947 - SAI1)
#########################################################

Overview
********

This project implements a real-time waveform generator using the **PCM5102A 32-bit I2S DAC** (WCMCU-5102 breakout) and the **CMSIS-DSP** library. It features a cycling waveform engine that can generate **Sine**, **Square**, **Sawtooth**, and **Triangle** waves at 48 kHz stereo.

Hardware Setup
**************

- **Board:** FRDM-MCXN947 (Dual-core Cortex-M33)
- **DAC:** PCM5102A (I2S) - WCMCU-5102 Breakout
- **Connection:** Arduino Header J1 (Inner Row - Odd Pins)
- **User Interface:** **SW2** (User Button) used to cycle through waveforms.

Verified Wiring Diagram (Measurement-Confirmed)
==============================================

Connect the WCMCU-5102 breakout to the FRDM-MCXN947 headers as follows:

*   **VCC** -> 5V (**J3 Pin 10**)
*   **GND** -> GND (**J3 Pin 12**)
*   **BCK** -> **J1 Pin 1** (Inner row) - 1.536 MHz Bit Clock
*   **DIN** -> **J1 Pin 5** (Inner row) - Serial Data
*   **LCK (LRCK)** -> **J1 Pin 11** (Inner row) - 48 kHz Word Clock
*   **XMT** -> 3.3V (**J3 Pin 8**) [Unmute]
*   **SCK** -> Disconnected (Internal PLL mode)

**Note on J1 Pin 3:** This pin is "dead" by default as it requires moving jumper **SJ10**. Use **J1 Pin 11** for the Frame Sync signal instead.

Breakout Configuration (WCMCU-5102)
===================================

The following pins on the PCM5102A breakout must be tied to **GND**:
- **SCL**: Enables internal PLL (Master Clock generation from BCK).
- **FMT**: Selects I2S format.
- **FLT**: Selects normal filter.
- **DMP**: Disables de-emphasis.

Building and Running
********************

To build the project:

.. code-block:: bash

    west build -b frdm_mcxn947/mcxn947/cpu0 i2s-dac -p always

Interactive Control:
Press **SW2** to cycle through: **SINE -> SQUARE -> SAWTOOTH -> TRIANGLE -> SINE**.
The application logs heartbeats to the console to confirm DMA activity.
