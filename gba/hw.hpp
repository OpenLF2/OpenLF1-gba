// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include <cstdint>

// Memory-mapped Game Boy Advance hardware.
#define REG_DISPCNT (*reinterpret_cast<volatile std::uint16_t*>(0x04000000))
#define REG_VCOUNT (*reinterpret_cast<volatile std::uint16_t*>(0x04000006))
#define REG_BG0CNT (*reinterpret_cast<volatile std::uint16_t*>(0x04000008))
#define REG_BG0HOFS (*reinterpret_cast<volatile std::uint16_t*>(0x04000010))
#define REG_BG0VOFS (*reinterpret_cast<volatile std::uint16_t*>(0x04000012))
#define REG_KEYINPUT (*reinterpret_cast<volatile std::uint16_t*>(0x04000130))
#define REG_DMA3SAD (*reinterpret_cast<volatile std::uint32_t*>(0x040000D4))
#define REG_DMA3DAD (*reinterpret_cast<volatile std::uint32_t*>(0x040000D8))
#define REG_DMA3CNT (*reinterpret_cast<volatile std::uint32_t*>(0x040000DC))

#define BG_PALETTE reinterpret_cast<std::uint16_t*>(0x05000000)
#define OBJ_PALETTE reinterpret_cast<std::uint16_t*>(0x05000200)
#define VRAM reinterpret_cast<std::uint8_t*>(0x06000000)
#define OBJ_VRAM reinterpret_cast<std::uint8_t*>(0x06010000)
#define OAM reinterpret_cast<volatile std::uint16_t*>(0x07000000)

namespace Display {
inline constexpr std::uint16_t mode0 = 0x0000;
inline constexpr std::uint16_t bg0 = 0x0100;
inline constexpr std::uint16_t obj = 0x1000;
inline constexpr std::uint16_t obj_1d_mapping = 0x0040;
inline constexpr int width = 240;
inline constexpr int height = 160;
}

// Background control: 8bpp tiles, screen blocks 30-31 for a 512x256 map.
namespace BackgroundControl {
inline constexpr std::uint16_t charblock0 = 0 << 2;
inline constexpr std::uint16_t color256 = 1 << 7;
inline constexpr std::uint16_t screenblock30 = 30 << 8;
inline constexpr std::uint16_t size_wide = 1 << 14;
inline constexpr std::uint16_t size_small = 0 << 14;
}

// Sprite attributes (OAM).
namespace Sprite {
inline constexpr std::uint16_t hidden = 0x0200;
inline constexpr std::uint16_t color256 = 0x2000;
inline constexpr std::uint16_t affine = 0x0100, double_size = 0x0200;
inline constexpr int square = 0, horizontal = 1, vertical = 2;
inline constexpr int size_8 = 0, size_16 = 1, size_32 = 2;
inline constexpr int size_16x8 = 0, size_32x8 = 1, size_32x16 = 2;   // horizontal shape sizes
}

// Keys: the hardware reports pressed buttons as zero bits.
namespace Key {
inline constexpr std::uint16_t a = 1 << 0;
inline constexpr std::uint16_t b = 1 << 1;
inline constexpr std::uint16_t select = 1 << 2;
inline constexpr std::uint16_t start = 1 << 3;
inline constexpr std::uint16_t right = 1 << 4;
inline constexpr std::uint16_t left = 1 << 5;
inline constexpr std::uint16_t up = 1 << 6;
inline constexpr std::uint16_t down = 1 << 7;
inline constexpr std::uint16_t r = 1 << 8;
inline constexpr std::uint16_t l = 1 << 9;
}

// Waits for the start of the next frame's vertical blank.
void wait_vblank();

// Frame pacing. Timers 2 and 3 count frames exactly (a frame is 4389 ticks at 1/64 of the CPU clock),
// so wait_frame() returns once per frame even when the work of the last frame overran: a late
// frame costs a repeated picture, never a slow game.
void frame_sync_start();
void wait_frame();

// Copies bytes with DMA channel 3. Both addresses must be aligned to the transfer size.
void dma_copy(const void* source, void* destination, unsigned bytes);

// Sound: direct sound channel A plays through DMA channel 1 at the rate of timer 0.
#define REG_SOUNDCNT_X (*reinterpret_cast<volatile std::uint16_t*>(0x04000084))
#define REG_SOUNDCNT_H (*reinterpret_cast<volatile std::uint16_t*>(0x04000082))
#define REG_TM0CNT_L (*reinterpret_cast<volatile std::uint16_t*>(0x04000100))
#define REG_TM0CNT_H (*reinterpret_cast<volatile std::uint16_t*>(0x04000102))
#define REG_DMA1SAD (*reinterpret_cast<volatile std::uint32_t*>(0x040000BC))
#define REG_DMA1DAD (*reinterpret_cast<volatile std::uint32_t*>(0x040000C0))
#define REG_DMA1CNT (*reinterpret_cast<volatile std::uint32_t*>(0x040000C4))

// Colour effects: fading the screen to black.
#define REG_BLDCNT (*reinterpret_cast<volatile std::uint16_t*>(0x04000050))
#define REG_BLDY (*reinterpret_cast<volatile std::uint16_t*>(0x04000054))

// Cartridge timing: faster ROM wait states and the prefetch buffer, as every GBA cartridge supports.
#define REG_WAITCNT (*reinterpret_cast<volatile std::uint16_t*>(0x04000204))

#define REG_TM2CNT_L (*reinterpret_cast<volatile std::uint16_t*>(0x04000108))
#define REG_TM2CNT_H (*reinterpret_cast<volatile std::uint16_t*>(0x0400010A))
#define REG_TM3CNT_L (*reinterpret_cast<volatile std::uint16_t*>(0x0400010C))
#define REG_TM3CNT_H (*reinterpret_cast<volatile std::uint16_t*>(0x0400010E))

// Interrupts. The BIOS calls the function whose address is stored at 0x03007FFC (in ARM state).
#define REG_DISPSTAT (*reinterpret_cast<volatile std::uint16_t*>(0x04000004))
#define REG_IE (*reinterpret_cast<volatile std::uint16_t*>(0x04000200))
#define REG_IF (*reinterpret_cast<volatile std::uint16_t*>(0x04000202))
#define REG_IME (*reinterpret_cast<volatile std::uint16_t*>(0x04000208))
#define IRQ_HANDLER (*reinterpret_cast<volatile std::uint32_t*>(0x03007FFC))
#define BIOS_IRQ_FLAGS (*reinterpret_cast<volatile std::uint16_t*>(0x03007FF8))

// Code that must run from the fast IWRAM rather than ROM.
#define IWRAM_CODE __attribute__((section(".iwram.text"), target("arm")))
