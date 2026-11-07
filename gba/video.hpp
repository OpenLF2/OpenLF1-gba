// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "hw.hpp"
#include <cstdint>

// Sprite tile memory, as byte offsets from the start of OBJ VRAM.
// Fighter bodies and heads have one slot per fighter; the rest are static
// or shared in a small pool that is refilled as objects appear.
namespace Vram {
inline constexpr unsigned body_base = 0;          // 8 fighter bodies, 1 KiB each (32x32, 8bpp)
inline constexpr unsigned head_base = 8192;       // 8 head overlays, 256 bytes each (16x16, 8bpp)
inline constexpr unsigned shadow_base = 10240;    // one 32x8 shadow
inline constexpr unsigned bar_base = 10496;       // 22 health and energy bars, 128 bytes each (16x8)
inline constexpr unsigned marker_base = 13312;    // 99 marker glyphs, 64 bytes each (8x8); the menus use
                                                  // this area for ten 512-byte icons instead
inline constexpr unsigned pool16_base = 19648;    // 20 items and projectiles, 256 bytes each (16x16)
inline constexpr unsigned pool32_base = 24768;    // 3 blasts and halos, 1 KiB each (32x32)
inline constexpr unsigned icon_base = 27840;      // 8 HUD icons, 512 bytes each (32x16)
inline constexpr unsigned end = 31936;
}

// OAM entry indices. Lower indices are drawn in front.
inline constexpr int oam_count = 128;

// Clears the display state and hides every sprite.
void video_init();

// Loads 256 BGR555 colours.
void load_bg_palette(const std::uint16_t* colors);
void load_obj_palette(const std::uint16_t* colors);

// Copies 8bpp tiles to the background character block 0, at tile index 'first'.
void load_bg_tiles(const std::uint8_t* tiles, unsigned bytes, unsigned first = 0);
void load_obj_tiles(const std::uint8_t* tiles, unsigned vram_offset, unsigned bytes);

// Background map: 512x256 with screen blocks 30 and 31.
void clear_bg_map();
void set_bg_map(const std::uint16_t* blocks);
void set_bg_scroll(int x, int y);
void put_text(int column, int row, const char* text);

// Sprites: shape and size as in Sprite::, tiles addressed by VRAM offset.
void hide_sprites(int first);
void put_sprite(int index, int x, int y, int shape, int size, unsigned vram_offset, bool flip_x = false);

// Menu text and panels. Text and panels use the UI tiles at the start of the menu and title tile sets.
void put_text_highlighted(int column, int row, const char* text);
void put_panel(int column, int row, int width, int height);
void fill_highlight(int column, int row, int width, int height);

// Fades the whole screen towards black: 0 is normal, 16 is black.
void set_fade(int level);

// A 2D-scaled sprite. 'group' selects one of the affine matrices set with set_affine.
// Scales are in 1/256 units of source pixels per screen pixel (256 is unscaled, 128 is doubled).
void set_affine(int group, int pa, int pd);
void put_affine_sprite(int index, int x, int y, int shape, int size, unsigned vram_offset, int group);

// Writes a width x height block of tiles numbered first, first + 1, ... in rows to the map.
void put_tiles(int column, int row, int width, int height, int first_tile);
