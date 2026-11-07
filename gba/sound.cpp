// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

// Software mixer for the GBA's direct sound channel A.
//
// The sounds are 8-bit samples at various rates. Each frame sound_update() resamples and adds up
// the active voices into one of two 224-sample buffers; 224 samples at one per 1254 cycles is
// exactly one frame (280896 cycles). A vertical-blank interrupt then restarts the DMA that feeds
// the sound FIFO on the buffer mixed during the previous frame (or on silence if there is none),
// so playback stays in step with the display even when the main loop is late.

#include "sound.hpp"
#include "assets.h"
#include "assets.hpp"
#include "hw.hpp"

namespace {
constexpr int buffer_size = 224;
constexpr int voice_count = 4;
constexpr std::uint32_t mix_rate = 16777216 / 1254;      // 13379 Hz
constexpr std::uint32_t fifo_a = 0x040000A0;
// DMA: enabled, start on sound request, repeat, 32-bit words, destination fixed.
constexpr std::uint32_t dma_control = 0x80000000u | 0x30000000u | 0x02000000u | 0x04000000u | 0x00400000u;

alignas(4) std::int8_t buffers[2][buffer_size];
alignas(4) std::int8_t silence[buffer_size];
volatile int ready_buffer = -1;          // buffer to play at the next vertical blank, or -1 for silence
int mix_buffer = 0;

struct Voice {
    const std::int8_t* data = nullptr;
    std::uint32_t position = 0;          // 16.16 fixed point index into data
    std::uint32_t step = 0;              // 16.16 samples consumed per output sample
    std::uint32_t length = 0;            // in samples
};
Voice voices[voice_count];
}

// Runs at every vertical blank: starts the audio DMA again so that the buffer plays in step with the frame.
extern "C" IWRAM_CODE void sound_vblank_handler() {
    REG_IF = 1;
    BIOS_IRQ_FLAGS = BIOS_IRQ_FLAGS | 1;
    const std::int8_t* source = ready_buffer >= 0 ? buffers[ready_buffer] : silence;
    ready_buffer = -1;
    REG_DMA1CNT = 0;
    REG_TM0CNT_H = 0;
    REG_SOUNDCNT_H = REG_SOUNDCNT_H | 0x0800;           // clear the FIFO
    REG_DMA1SAD = reinterpret_cast<std::uint32_t>(source);
    REG_DMA1DAD = fifo_a;
    REG_DMA1CNT = dma_control;
    REG_TM0CNT_L = std::uint16_t(65536 - 1254);
    REG_TM0CNT_H = 0x80;                                  // enable, prescaler 1
}

void sound_init() {
    REG_SOUNDCNT_X = 0x80;      // master enable
    // Channel A: full volume, left and right, timer 0.
    REG_SOUNDCNT_H = 0x0304;
    REG_IME = 0;
    IRQ_HANDLER = reinterpret_cast<std::uint32_t>(&sound_vblank_handler);
    REG_DISPSTAT = REG_DISPSTAT | 0x0008;               // vertical blank interrupt on
    REG_IE = REG_IE | 1;
    REG_IME = 1;
}

void sound_play(int id) {
    if (id < 0 || id >= SOUND_COUNT) return;
    // A free voice, otherwise the one with the least left to play.
    int chosen = 0;
    std::uint32_t least = 0xFFFFFFFFu;
    for (int i = 0; i < voice_count; ++i) {
        const std::uint32_t left = voices[i].data ? voices[i].length - (voices[i].position >> 16) : 0;
        if (left < least) { least = left; chosen = i; }
    }
    Voice& voice = voices[chosen];
    voice.data = reinterpret_cast<const std::int8_t*>(sounds_bin + SOUND_OFFSET[id]);
    voice.position = 0;
    voice.step = (SOUND_RATE[id] << 16) / mix_rate;
    voice.length = SOUND_LENGTH[id];
}

void sound_update() {
    bool any = false;
    for (const auto& voice : voices) any |= voice.data != nullptr;
    if (!any) return;
    std::int8_t* out = buffers[mix_buffer];
    for (int i = 0; i < buffer_size; ++i) {
        int sum = 0;
        for (auto& voice : voices) {
            if (!voice.data) continue;
            const std::uint32_t index = voice.position >> 16;
            if (index >= voice.length) { voice.data = nullptr; continue; }
            sum += voice.data[index];
            voice.position += voice.step;
        }
        sum = sum - sum / 4;    // leave some room when several sounds overlap
        out[i] = std::int8_t(sum > 127 ? 127 : sum < -128 ? -128 : sum);
    }
    ready_buffer = mix_buffer;
    mix_buffer ^= 1;
}
