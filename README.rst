NXP MCX Modular DSP Synthesiser
###############################

A high-performance, phase-coherent multi-threaded I2S synthesiser for NXP MCX E and N series microcontrollers. This project implements a Direct Digital Synthesis (DDS) engine with a modular DSP pipeline architecture.

Features
********

*   **Modular DSP Pipeline**: Decoupled "Assembly Line" architecture separating Sources, Mixers, and Effects.
*   **Phase-Locked Oscillators**: Dual-oscillator engine with 32-bit phase accumulation and perfect harmonic alignment.
*   **Advanced Synthesis**: Supports Sine, Square, Sawtooth, Triangle, and 2-operator Phase Modulation (FM).
*   **Global Effects**: Real-time bitcrushing (NES-style) and digital attenuation.
*   **Hardware Accelerated**: Utilizes CMSIS-DSP (SIMD) for all mixing and scaling operations.
*   **Multi-Platform**: Native support for FRDM-MCXE247, FRDM-MCXE31B, and FRDM-MCXN947.

Architecture
************

The audio engine runs in a high-priority co-operative thread (Priority -2) and follows a linear DSP pipeline:

1.  **Source**: Generate primary waveform (DDS).
2.  **Mixer**: Generate and safely combine a phase-locked harmonic (1 octave up).
3.  **Processors**: Apply global effects (e.g., Bitcrush).
4.  **Output**: Master attenuation and I2S DMA transmission.

Hardware Mapping
****************

FRDM-MCXE247 / FRDM-MCXE31B
===========================

*   **SW2 (PTA9)**: Cycle Waveform Type (Sine, Square, FM, etc.)
*   **SW3 (PTC10)**: Cycle Mix/Effect State:
    *   State 0: Clean Solo
    *   State 1: Clean Mix
    *   State 2: Crushed Solo (4-bit)
    *   State 3: Crushed Mix (4-bit)
*   **I2S Output (MikroBUS)**:
    *   BCK: SCK
    *   LRCK: CS
    *   DOUT: MOSI

FRDM-MCXN947
============

*   **SW2 (User SW2)**: Cycle Waveform Type
*   **SW3 (User SW3)**: Cycle Mix/Effect State
*   **I2S Output (SAI1 - J1 Header)**:
    *   BCK: J1-1 (PIO3_16)
    *   DOUT: J1-5 (PIO3_20)
    *   LRCK: J1-11 (PIO3_17)

Building and Running
********************

To build for the FRDM-MCXE247:

.. code-block:: bash

    west build -b frdm_mcxe247 pcm5102a -p always

To build for the FRDM-MCXN947:

.. code-block:: bash

    west build -b frdm_mcxn947/mcxn947/cpu0 pcm5102a -p always

Flashing:

.. code-block:: bash

    west flash

Author
******

Jan-Willem Smaal <usenet@gispen.org>

References & Acknowledgments
****************************

This synthesiser implements **Direct Digital Synthesis (DDS)**, a technique pioneered in the early 1970s. Key references include:

*   **Tierney, J., Rader, C.M., and Gold, B. (1971)**: *"A Digital Frequency Synthesizer"*. IEEE Transactions on Audio and Electroacoustics. This is the seminal paper that defined the phase-accumulator and lookup-table architecture used here.
*   **Analog Devices (1999)**: *"A Technical Tutorial on Digital Signal Synthesis"*. A foundational industry resource for understanding frequency resolution, phase truncation, and DDS spurs.
*   **Chowning, J. M. (1973)**: *"The Synthesis of Complex Audio Spectra by Means of Frequency Modulation"*. Journal of the Audio Engineering Society. Used as the basis for the Phase Modulation (FM) implementation in the `USR2` oscillator.
