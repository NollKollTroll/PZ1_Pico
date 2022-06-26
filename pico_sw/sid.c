/*
 *  sid.cpp - 6581 SID emulation
 *
 *  Frodo (C) Copyright 1994-2004 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdint.h>
#include <stdbool.h>


// Audio output sample frequency
static int32_t sample_frequency = 44100;

// Flag: emulate new SID chip (8580)
static bool emulate_8580 = true;

// Number of SID clocks per sample frame
static uint32_t sid_cycles;    // Integer
static float sid_cycles_float; // With fractional part

// Phi2 clock frequency
const float PAL_CLOCK = 985248.444;
const float NTSC_OLD_CLOCK = 1000000.0;
const float NTSC_CLOCK = 1022727.143;
static float cycles_per_second_float; // With fractional part

// Pseudo-random number generator for SID noise waveform (don't use f_rand()
// because the SID waveform calculation runs asynchronously and the output of
// f_rand() has to be predictable inside the main emulation)
static uint32_t noise_rand_seed = 1;

inline static uint8_t noise_rand()
{
    // This is not the original SID noise algorithm (which is unefficient to
    // implement in software) but this sounds close enough
    noise_rand_seed = noise_rand_seed * 1103515245 + 12345;
    return noise_rand_seed >> 16;
}

// SID waveforms
enum {
    WAVE_NONE,
    WAVE_TRI,
    WAVE_SAW,
    WAVE_TRISAW,
    WAVE_RECT,
    WAVE_TRIRECT,
    WAVE_SAWRECT,
    WAVE_TRISAWRECT,
    WAVE_NOISE
};

// EG states
enum {
    EG_IDLE,
    EG_ATTACK,
    EG_DECAY,
    EG_RELEASE
};

// Filter types
enum {
    FILT_NONE,
    FILT_LP,
    FILT_BP,
    FILT_LPBP,
    FILT_HP,
    FILT_NOTCH,
    FILT_HPBP,
    FILT_ALL
};

// Structure for one voice
typedef struct voice_t voice_t;
struct voice_t {
    int wave;            // Selected waveform
    int eg_state;        // Current state of EG
    voice_t *mod_by;    // Voice that modulates this one
    voice_t *mod_to;    // Voice that is modulated by this one

    uint32_t count;        // Counter for waveform generator, 8.16 fixed
    uint32_t add;            // Added to counter in every sample frame

    uint16_t freq;        // SID frequency value
    uint16_t pw;            // SID pulse-width value

    uint32_t a_add;        // EG parameters
    uint32_t d_sub;
    uint32_t s_level;
    uint32_t r_sub;
    uint32_t eg_level;    // Current EG level, 8.16 fixed

    uint32_t noise;        // Last noise generator output value

    bool gate;            // EG gate bit
    bool ring;            // Ring modulation bit
    bool test;            // Test bit
    bool filter;        // Flag: Voice filtered

                        // The following bit is set for the modulating
                        // voice, not for the modulated one (as the SID bits)
    bool sync;            // Sync modulation bit
    bool mute;            // Voice muted (voice 3 only)
};

// Data structures for SID
typedef struct sid_t sid_t;
struct sid_t {
    voice_t voice[3];                    // Data for 3 voices

    uint8_t regs[29];                    // SID registers
    uint8_t last_written_byte;            // Byte last written to SID (for emulation of read accesses to write-only registers)
    uint8_t volume;                        // Master volume (0..15)

    uint8_t f_type;                        // Filter type
    uint8_t f_freq;                        // SID filter frequency (upper 8 bits)
    uint8_t f_res;                        // Filter resonance (0..15)
};
sid_t sid;

void sid_reset();
uint8_t sid_read(uint8_t addr);
void sid_write(uint8_t addr, uint8_t data);

// Waveform tables
static uint16_t tri_table[0x1000*2];
static const uint16_t *tri_saw_table;
static const uint16_t *tri_rect_table;
static const uint16_t *saw_rect_table;
static const uint16_t *tri_saw_rect_table;

// Sampled from a 6581R4
static const uint16_t tri_saw_table_6581[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0808,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1010, 0x3C3C,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0808,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1010, 0x3C3C
};

static const uint16_t tri_rect_table_6581[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x8080,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x8080,
    0, 0, 0, 0, 0, 0, 0x8080, 0xC0C0, 0, 0x8080, 0x8080, 0xE0E0, 0x8080, 0xE0E0, 0xF0F0, 0xFCFC,
    0xFFFF, 0xFCFC, 0xFAFA, 0xF0F0, 0xF6F6, 0xE0E0, 0xE0E0, 0x8080, 0xEEEE, 0xE0E0, 0xE0E0, 0x8080, 0xC0C0, 0, 0, 0,
    0xDEDE, 0xC0C0, 0xC0C0, 0, 0x8080, 0, 0, 0, 0x8080, 0, 0, 0, 0, 0, 0, 0,
    0xBEBE, 0x8080, 0x8080, 0, 0x8080, 0, 0, 0, 0x8080, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x7E7E, 0x4040, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const uint16_t saw_rect_table_6581[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x7878,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x7878
};

static const uint16_t tri_saw_rect_table_6581[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Sampled from an 8580R5
static const uint16_t tri_saw_table_8580[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0808,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1818, 0x3C3C,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1C1C,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x8080, 0, 0x8080, 0x8080,
    0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xE0E0, 0xF0F0, 0xF0F0, 0xF0F0, 0xF0F0, 0xF8F8, 0xF8F8, 0xFCFC, 0xFEFE
};

static const uint16_t tri_rect_table_8580[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0xFFFF, 0xFCFC, 0xF8F8, 0xF0F0, 0xF4F4, 0xF0F0, 0xF0F0, 0xE0E0, 0xECEC, 0xE0E0, 0xE0E0, 0xC0C0, 0xE0E0, 0xC0C0, 0xC0C0, 0xC0C0,
    0xDCDC, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0x8080, 0x8080, 0xC0C0, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0, 0,
    0xBEBE, 0xA0A0, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0, 0x8080, 0x8080, 0, 0, 0, 0, 0, 0,
    0x8080, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x7E7E, 0x7070, 0x6060, 0, 0x4040, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const uint16_t saw_rect_table_8580[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x8080,
    0, 0, 0, 0, 0, 0, 0x8080, 0x8080, 0, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xB0B0, 0xBEBE,
    0, 0, 0, 0, 0, 0, 0, 0x8080, 0, 0, 0, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0,
    0, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0, 0x8080, 0x8080, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xDCDC,
    0x8080, 0x8080, 0x8080, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xE0E0, 0xECEC,
    0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xE0E0, 0xF0F0, 0xF0F0, 0xF4F4, 0xF0F0, 0xF0F0, 0xF8F8, 0xF8F8, 0xF8F8, 0xFCFC, 0xFEFE, 0xFFFF
};

static const uint16_t tri_saw_rect_table_8580[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x8080, 0x8080,
    0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xF0F0, 0xF8F8, 0xFCFC
};

// Envelope tables
static uint32_t eg_table[16];

static const uint8_t eg_dr_shift[256] = {
    5,5,5,5,5,5,5,5,4,4,4,4,4,4,4,4,
    3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/****************
 *  Reset SID emulation
 ****************/
void sid_reset()
{
    cycles_per_second_float = PAL_CLOCK;            
//    cycles_per_second_int = cycles_per_second_float;
  
    if (emulate_8580) {
        tri_saw_table = tri_saw_table_8580;
        tri_rect_table = tri_rect_table_8580;
        saw_rect_table = saw_rect_table_8580;
        tri_saw_rect_table = tri_saw_rect_table_8580;
    } else {
        tri_saw_table = tri_saw_table_6581;
        tri_rect_table = tri_rect_table_6581;
        saw_rect_table = saw_rect_table_6581;
        tri_saw_rect_table = tri_saw_rect_table_6581;
    }
    
    sid.voice[0].mod_by = &sid.voice[2];
    sid.voice[1].mod_by = &sid.voice[0];
    sid.voice[2].mod_by = &sid.voice[1];
    sid.voice[0].mod_to = &sid.voice[1];
    sid.voice[1].mod_to = &sid.voice[2];
    sid.voice[2].mod_to = &sid.voice[0];

    for(int i = 0; i < 29; i++){
        sid.regs[i] = 0;
    }    
    sid.last_written_byte = 0;

    sid.volume = 15;
    sid.regs[24] = 0x0f;

    int v;
    for (v=0; v<3; v++) {
        sid.voice[v].wave = WAVE_NONE;
        sid.voice[v].eg_state = EG_IDLE;
        sid.voice[v].count = sid.voice[v].add = 0;
        sid.voice[v].freq = sid.voice[v].pw = 0;
        sid.voice[v].eg_level = sid.voice[v].s_level = 0;
        sid.voice[v].a_add = sid.voice[v].d_sub = sid.voice[v].r_sub = eg_table[0];
        sid.voice[v].gate = sid.voice[v].ring = sid.voice[v].test = false;
        sid.voice[v].filter = sid.voice[v].sync = sid.voice[v].mute = false;
    }

    sid.f_type = FILT_NONE;
    sid.f_freq = sid.f_res = 0;
    
    // Compute triangle table
    for (int i=0; i<0x1000; i++) {
        tri_table[i] = (i << 4) | (i >> 8);
        tri_table[0x1fff-i] = (i << 4) | (i >> 8);
    }
    
    // Compute number of cycles per sample frame
    sid_cycles_float = cycles_per_second_float / sample_frequency;
    sid_cycles = sid_cycles_float;

    // Compute envelope table
    static const uint32_t div[16] = {
        9, 32, 63, 95, 149, 220, 267, 313,
        392, 977, 1954, 3126, 3906, 11720, 19531, 31251
    };
    int i;
    for (i=0; i<16; i++)
        eg_table[i] = (sid_cycles << 16) / div[i];

    // Recompute voice_t::add values
    sid_write(0, sid.regs[0]);
    sid_write(7, sid.regs[7]);
    sid_write(14, sid.regs[14]);
}

int16_t sid_calc()
{
    int32_t sum_output = 0;
    //int32_t sum_output_filter = 0;

    // Loop for all three voices
    int j;
    for (j=0; j<3; j++) {
        voice_t *v = sid.voice + j;

        // Envelope generator
        uint16_t envelope;

        switch (v->eg_state) {
            case EG_ATTACK:
                v->eg_level += v->a_add;
                if (v->eg_level > 0xffffff) {
                    v->eg_level = 0xffffff;
                    v->eg_state = EG_DECAY;
                }
                break;
            case EG_DECAY:
                if (v->eg_level <= v->s_level || v->eg_level > 0xffffff)
                    v->eg_level = v->s_level;
                else {
                    v->eg_level -= v->d_sub >> eg_dr_shift[v->eg_level >> 16];
                    if (v->eg_level <= v->s_level || v->eg_level > 0xffffff)
                        v->eg_level = v->s_level;
                }
                break;
            case EG_RELEASE:
                v->eg_level -= v->r_sub >> eg_dr_shift[v->eg_level >> 16];
                if (v->eg_level > 0xffffff) {
                    v->eg_level = 0;
                    v->eg_state = EG_IDLE;
                }
                break;
            case EG_IDLE:
                v->eg_level = 0;
                break;
        }
        envelope = v->eg_level >> 8; //u16 = u24 >> 8
        
        // Waveform generator
        uint16_t output;

        if (!v->test)
            v->count += v->add;

        if (v->sync && (v->count >= 0x1000000))
            v->mod_to->count = 0;

        v->count &= 0xffffff;

        switch (v->wave) {
            case WAVE_TRI:
                if (v->ring)
                    output = tri_table[(v->count ^ (v->mod_by->count & 0x800000)) >> 11];
                else
                    output = tri_table[v->count >> 11];
                break;
            case WAVE_SAW:
                output = v->count >> 8;
                break;
            case WAVE_RECT:
                if (v->count > (uint32_t)(v->pw << 12))
                    output = 0xffff;
                else
                    output = 0;
                break;
            case WAVE_TRISAW:
                output = tri_saw_table[v->count >> 16];
                break;
            case WAVE_TRIRECT:
                if (v->count > (uint32_t)(v->pw << 12))
                    output = tri_rect_table[v->count >> 16];
                else
                    output = 0;
                break;
            case WAVE_SAWRECT:
                if (v->count > (uint32_t)(v->pw << 12))
                    output = saw_rect_table[v->count >> 16];
                else
                    output = 0;
                break;
            case WAVE_TRISAWRECT:
                if (v->count > (uint32_t)(v->pw << 12))
                    output = tri_saw_rect_table[v->count >> 16];
                else
                    output = 0;
                break;
            case WAVE_NOISE:
                if (v->count >= 0x100000) {
                    output = v->noise = noise_rand() << 8;
                    v->count &= 0xfffff;
                } else
                    output = v->noise;
                break;
            default:
                output = 0x8000;
                break;
        }
        sum_output += ((output - 32768) * envelope) / (3 * 16); //3 SID-voices * 16 levels volume -> 48
    }    
    return((sum_output * sid.volume) >> 16);
}

/*
 *  Read from SID register
 */
uint8_t sid_read(uint8_t addr)
{
    switch (addr) 
    {
        case 0x19:    // A/D converters
        case 0x1a:
        {
            sid.last_written_byte = 0;
            return 0xff;
        }
        case 0x1b:    // Voice 3 oscillator/EG readout
        case 0x1c:
        {
            sid.last_written_byte = 0;
            //return f_rand();
            return noise_rand();
        }
        default:      // Write-only register: return last value written to SID
        {
            uint8_t ret = sid.last_written_byte;
            sid.last_written_byte = 0;
            return ret;
        }
    }
}

/*
 *  Write to SID register
 */
void sid_write(uint8_t addr, uint8_t data)
{
    sid.last_written_byte = sid.regs[addr] = data;
    int v = addr / 7;    // Voice number

    switch (addr) 
    {
        case 0:
        case 7:
        case 14:
        {
            sid.voice[v].freq = (sid.voice[v].freq & 0xff00) | data;
            sid.voice[v].add = (uint32_t) (sid.voice[v].freq * sid_cycles_float);
            break;
        }
        case 1:
        case 8:
        case 15:
        {
            sid.voice[v].freq = (sid.voice[v].freq & 0xff) | (data << 8);
            sid.voice[v].add = (uint32_t) (sid.voice[v].freq * sid_cycles_float);
            break;
        }
        case 2:
        case 9:
        case 16:
        {
            sid.voice[v].pw = (sid.voice[v].pw & 0x0f00) | data;
            break;
        }
        case 3:
        case 10:
        case 17:
        {
            sid.voice[v].pw = (sid.voice[v].pw & 0xff) | ((data & 0xf) << 8);
            break;
        }
        case 4:
        case 11:
        case 18:
        {
            sid.voice[v].wave = (data >> 4) & 0xf;
            if ((data & 1) != sid.voice[v].gate) 
            {
                if (data & 1)    // Gate turned on
                {
                    sid.voice[v].eg_state = EG_ATTACK;
                }
                else if (sid.voice[v].eg_state != EG_IDLE)
                {
                    sid.voice[v].eg_state = EG_RELEASE;
                }
            }
            sid.voice[v].gate = data & 1;
            sid.voice[v].mod_by->sync = data & 2;
            sid.voice[v].ring = data & 4;
            if ((sid.voice[v].test = data & 8))
            {
                sid.voice[v].count = 0;
            }
            break;
        }
        case 5:
        case 12:
        case 19:
        {
            sid.voice[v].a_add = eg_table[data >> 4];
            sid.voice[v].d_sub = eg_table[data & 0xf];
            break;
        }
        case 6:
        case 13:
        case 20:
        {
            sid.voice[v].s_level = (data >> 4) * 0x111111;
            sid.voice[v].r_sub = eg_table[data & 0xf];
            break;
        }
        case 22:
        {
            sid.f_freq = data;
            break;
        }
        case 23:
        {
            sid.voice[0].filter = data & 1;
            sid.voice[1].filter = data & 2;
            sid.voice[2].filter = data & 4;
            sid.f_res = data >> 4;
            break;
        }
        case 24:
        {
            sid.volume = data & 0xf;
            sid.voice[2].mute = data & 0x80;
            sid.f_type = (data >> 4) & 7;
            break;
        }
    }
}
