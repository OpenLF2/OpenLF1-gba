// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "video.hpp"
#include "assets.h"
#include <cstring>

namespace {
constexpr unsigned screen_block_bytes = 0x800;
constexpr std::uintptr_t screen_block30 = 0x06000000 + 30 * screen_block_bytes;
}

void video_init() {
    REG_DISPCNT = Display::mode0 | Display::bg0 | Display::obj | Display::obj_1d_mapping;
    REG_BG0CNT = BackgroundControl::charblock0 | BackgroundControl::color256 |
                 BackgroundControl::screenblock30 | BackgroundControl::size_wide;
    hide_sprites(0);
}

void load_bg_palette(const std::uint16_t* colors) {
    dma_copy(colors, BG_PALETTE, 512);
}

void load_obj_palette(const std::uint16_t* colors) {
    dma_copy(colors, OBJ_PALETTE, 512);
}

void load_bg_tiles(const std::uint8_t* tiles, unsigned bytes, unsigned first) {
    dma_copy(tiles, VRAM + first * 64, bytes);
}

void load_obj_tiles(const std::uint8_t* tiles, unsigned vram_offset, unsigned bytes) {
    dma_copy(tiles, OBJ_VRAM + vram_offset, bytes);
}

void clear_bg_map() {
    auto* map = reinterpret_cast<volatile std::uint16_t*>(screen_block30);
    for (unsigned i = 0; i < 2 * screen_block_bytes / 2; ++i) map[i] = 0;
}

void set_bg_map(const std::uint16_t* blocks) {
    dma_copy(blocks, reinterpret_cast<void*>(screen_block30), 2 * screen_block_bytes);
}

void set_bg_scroll(int x, int y) {
    REG_BG0HOFS = std::uint16_t(x & 0x1FF);
    REG_BG0VOFS = std::uint16_t(y & 0x1FF);
}

void put_text(int column, int row, const char* text) {
    auto* map = reinterpret_cast<volatile std::uint16_t*>(screen_block30);
    for (int i = 0; text[i] && column + i < 32; ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        map[row * 32 + column + i] = (c >= 32 && c < 128) ? std::uint16_t(c - 32) : 0;
    }
}

void hide_sprites(int first) {
    for (int i = first; i < oam_count; ++i) OAM[i * 4] = Sprite::hidden;
}

void put_sprite(int index, int x, int y, int shape, int size, unsigned vram_offset, bool flip_x) {
    OAM[index * 4] = std::uint16_t((y & 0xFF) | Sprite::color256 | (shape << 14));
    OAM[index * 4 + 1] = std::uint16_t((x & 0x1FF) | (size << 14) | (flip_x ? 0x1000 : 0));
    OAM[index * 4 + 2] = std::uint16_t(vram_offset / 32);
}

void put_text_highlighted(int column, int row, const char* text) {
    auto* map = reinterpret_cast<volatile std::uint16_t*>(screen_block30);
    for (int i = 0; text[i] && column + i < 32; ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        map[row * 32 + column + i] = (c >= 32 && c < 128) ? std::uint16_t(UI_HILITE_BASE + c - 32) : std::uint16_t(UI_HILITE_BASE);
    }
}

void put_panel(int column, int row, int width, int height) {
    auto* map = reinterpret_cast<volatile std::uint16_t*>(screen_block30);
    for (int r = 0; r < height; ++r) for (int c = 0; c < width; ++c) {
        std::uint16_t tile = 0;
        if (width >= 2 && height >= 2) {
            const bool top = r == 0, bottom = r == height - 1, left = c == 0, right = c == width - 1;
            if (top) tile = left ? 0 : right ? 2 : 1;
            else if (bottom) tile = left ? 5 : right ? 7 : 6;
            else if (left) tile = 3;
            else if (right) tile = 4;
            if (tile || top || bottom || left || right) tile += UI_FRAME_BASE;
        }
        map[(row + r) * 32 + column + c] = tile;
    }
}

void fill_highlight(int column, int row, int width, int height) {
    auto* map = reinterpret_cast<volatile std::uint16_t*>(screen_block30);
    for (int r = 0; r < height; ++r) for (int c = 0; c < width; ++c) map[(row + r) * 32 + column + c] = UI_HILITE_BASE;
}

void set_fade(int level) {
    // Brightness decrease on the backgrounds, sprites and backdrop.
    REG_BLDCNT = 0x00F1;
    REG_BLDY = std::uint16_t(level < 0 ? 0 : level > 16 ? 16 : level);
}

void set_affine(int group, int pa, int pd) {
    OAM[(group * 4 + 0) * 4 + 3] = std::uint16_t(pa);
    OAM[(group * 4 + 1) * 4 + 3] = 0;
    OAM[(group * 4 + 2) * 4 + 3] = 0;
    OAM[(group * 4 + 3) * 4 + 3] = std::uint16_t(pd);
}

void put_affine_sprite(int index, int x, int y, int shape, int size, unsigned vram_offset, int group) {
    OAM[index * 4] = std::uint16_t((y & 0xFF) | Sprite::color256 | Sprite::affine | Sprite::double_size | (shape << 14));
    OAM[index * 4 + 1] = std::uint16_t((x & 0x1FF) | (group << 9) | (size << 14));
    OAM[index * 4 + 2] = std::uint16_t(vram_offset / 32);
}

void put_tiles(int column, int row, int width, int height, int first_tile) {
    auto* map = reinterpret_cast<volatile std::uint16_t*>(screen_block30);
    for (int r = 0; r < height; ++r) for (int c = 0; c < width; ++c) map[(row + r) * 32 + column + c] = std::uint16_t(first_tile + r * width + c);
}
