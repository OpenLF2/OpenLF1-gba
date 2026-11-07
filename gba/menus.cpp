// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "menus.hpp"
#include "assets.h"
#include "assets.hpp"
#include "hw.hpp"
#include "video.hpp"
#include <cstring>

namespace {
constexpr int body_cell = 1024, head_cell = 256;
constexpr int walk_poses[4] = {0, 1, 0, 2};   // idle, first and second walking pose (action - 1)

int gx(int x) { return (x * 3) >> 2; }

int text_length(const char* text) { return int(std::strlen(text)); }

// Puts 'text' centred within columns [column, column + width).
void put_centered(int row, int column, int width, const char* text) {
    put_text(column + (width - text_length(text)) / 2, row, text);
}

void load_screen(const std::uint8_t* tiles, int count, const std::uint16_t* map) {
    load_bg_tiles(tiles, count * 64);
    set_bg_map(map);
    set_bg_scroll(0, 0);
    hide_sprites(0);
}

// Writes a character's ten colours into colour set 'set' of the sprite palette.
void set_colors(const GameData& data, int set, int character) {
    const auto& look = data.appearance[character - 1];
    for (int c = 0; c < TINT_COUNT; ++c) {
        const int index = c == TINT_COUNT - 1 ? data.hair[look.head - 1] : look.color[c];
        OBJ_PALETTE[TINT_INDEX[set][c]] = pal_bg_bin[index];
    }
}

// A character's head and colours loaded into fighter slot 'slot' (so its body can be drawn).
void load_fighter(const GameData& data, int slot, int character) {
    const auto& look = data.appearance[character - 1];
    dma_copy(heads_bin + (look.head - 1) * head_cell, OBJ_VRAM + Vram::head_base + slot * head_cell, head_cell);
    set_colors(data, slot, character);
}

// Body pose in a slot's VRAM, reloaded only when it changes.
int slot_pose[FIGHTER_SLOTS];
void load_pose(int slot, int pose) {
    if (slot_pose[slot] == pose) return;
    slot_pose[slot] = pose;
    dma_copy(body_bin + (slot * BODY_POSES + pose) * body_cell, OBJ_VRAM + Vram::body_base + slot * body_cell, body_cell);
}

// The title's walkers: a head, a body and a shadow each, back to front.
struct Walker { int character, row, speed, offset; };
constexpr Walker walkers[4] = {{1, 80, 3, 20}, {2, 91, 4, 95}, {3, 102, 5, 170}, {4, 113, 6, 245}};

// The select screen preview, at 2.67x horizontally to undo the 3/4 squeeze, and 2x vertically.
constexpr int preview_pa = 96, preview_pd = 128;
int preview_character = 0;
int result_loser = 0;

void draw_preview(int center_x, int feet_y, unsigned frame, int first_sprite) {
    const int pose = walk_poses[(frame / 8) % 4];
    load_pose(0, pose);
    set_affine(0, preview_pa, preview_pd);
    const int origin_x = center_x - 25, origin_y = feet_y - 50;
    put_affine_sprite(first_sprite, origin_x + 5, origin_y, Sprite::square, Sprite::size_16, Vram::head_base, 0);
    put_affine_sprite(first_sprite + 1, origin_x + 11, origin_y, Sprite::square, Sprite::size_32, Vram::body_base, 0);
}

constexpr int grid_columns = 2, grid_rows = 5;
int last_cursor = -1, last_fighters = -1, last_moves = -1;
}

void menu_title_enter(const GameData& data) {
    load_screen(title_bin, TITLE_TILES, title_map_bin);
    for (int i = 0; i < FIGHTER_SLOTS; ++i) slot_pose[i] = -1;
    for (int k = 0; k < 4; ++k) load_fighter(data, k, walkers[k].character);
    dma_copy(shadow_bin, OBJ_VRAM + Vram::shadow_base, 256);
    put_panel(8, 17, 14, 3);
}

void menu_title_frame(unsigned frame) {
    put_centered(18, 8, 14, (frame / 30) % 2 == 0 ? "PRESS START" : "           ");
    for (int k = 0; k < 4; ++k) {
        const auto& w = walkers[k];
        const int x = int((frame * w.speed / 4 + w.offset) % 340) - 40;
        load_pose(k, walk_poses[(frame / 6 + k) % 4]);
        const int base = (3 - k) * 3, top = w.row;
        put_sprite(base, gx(x), top, Sprite::square, Sprite::size_16, Vram::head_base + k * head_cell);
        put_sprite(base + 1, gx(x), top, Sprite::square, Sprite::size_32, Vram::body_base + k * body_cell);
        put_sprite(base + 2, gx(x) - 1, top + 23, Sprite::horizontal, Sprite::size_32x8, Vram::shadow_base);
    }
    hide_sprites(12);
}

void menu_select_enter(const GameData& data) {
    load_screen(menu_bin, MENU_TILES, menu_map_bin);
    for (int i = 0; i < FIGHTER_SLOTS; ++i) slot_pose[i] = -1;
    for (int k = 0; k < 10; ++k) {
        // Grid icons (head over the top of the body) use colour sets 2-11 and the marker area of sprite
        // memory; colour sets 0 and 1 belong to the preview. Battles load their own data again.
        const int set = k + 2;
        set_colors(data, set, k + 1);
        dma_copy(icons_bin + (set * HEAD_COUNT + data.appearance[k].head - 1) * 512, OBJ_VRAM + Vram::marker_base + k * 512, 512);
    }
    last_moves = -1;
    last_cursor = last_fighters = -1;
    preview_character = 0;
}

namespace {
// First tile used by the moves card, after the UI and backdrop tiles.
constexpr int moves_first_tile = 400;
constexpr int moves_card_bytes = MOVES_COLUMNS * MOVES_ROWS * 64;

void build_select_panels(bool moves) {
    put_panel(0, 0, 30, 1);
    put_panel(0, 17, 30, 3);
    if (moves) {
        put_panel(0, 1, 12, 16);
        put_panel(12, 1, 18, 16);
        put_centered(18, 0, 30, "A FIGHT  B BACK  D-PAD NEXT");
    } else {
        put_panel(0, 1, 20, 16);
        put_panel(20, 1, 10, 16);
        put_centered(18, 0, 30, "A: FIGHT   B: SPECIAL MOVES");
    }
}
}

void menu_select_frame(const GameData& data, int cursor, int fighters, bool moves, unsigned frame) {
    if (int(moves) != last_moves) {
        last_moves = int(moves);
        last_cursor = last_fighters = -1;
        preview_character = 0;
        build_select_panels(moves);
    }
    if (moves) {
        if (cursor != last_cursor) {
            last_cursor = cursor;
            put_panel(0, 0, 30, 1);
            put_centered(0, 0, 30, data.appearance[cursor].name.data());
            load_fighter(data, 0, cursor + 1);
            slot_pose[0] = -1;
            load_bg_tiles(moves_bin + cursor * moves_card_bytes, moves_card_bytes, moves_first_tile);
            put_tiles(16, 3, MOVES_COLUMNS, MOVES_ROWS, moves_first_tile);
        }
        draw_preview(48, 104, frame, 0);
        hide_sprites(2);
        return;
    }
    if (cursor != last_cursor) {
        for (int k = 0; k < 10; ++k) {
            const int column = k / grid_rows, row = k % grid_rows;
            const int name_column = 4 + column * 9, name_row = 2 + row * 3;
            const int block = 6;
            const char* name = data.appearance[k].name.data();
            // Clear the name area, then draw it highlighted or plain.
            put_panel(name_column, name_row, block, 1);
            put_panel(name_column, name_row + 1, block, 1);
            if (k == cursor) {
                fill_highlight(name_column, name_row, block, 2);
                put_text_highlighted(name_column, name_row, name);
            } else {
                put_text(name_column, name_row, name);
            }
        }
        if (cursor + 1 != preview_character) {
            preview_character = cursor + 1;
            load_fighter(data, 0, preview_character);
            slot_pose[0] = -1;
            put_panel(21, 14, 8, 1);
            put_centered(14, 20, 10, data.appearance[cursor].name.data());
        }
        last_cursor = cursor;
    }
    if (fighters != last_fighters) {
        char count[20] = "FIGHTERS L/R: 4";
        count[14] = char('0' + fighters);
        put_panel(0, 0, 30, 1);
        put_text(1, 0, "CHOOSE");
        put_text(14, 0, count);
        last_fighters = fighters;
    }
    for (int k = 0; k < 10; ++k) {
        const int column = k / grid_rows, row = k % grid_rows;
        put_sprite(2 + k, 8 + column * 72, 16 + row * 24, Sprite::horizontal, Sprite::size_32x16, Vram::marker_base + k * 512);
    }
    draw_preview(200, 100, frame, 0);
    hide_sprites(12);
}

void menu_result_enter(const GameData& data, int outcome, int winner, int loser) {
    load_screen(menu_bin, MENU_TILES, menu_map_bin);
    for (int i = 0; i < FIGHTER_SLOTS; ++i) slot_pose[i] = -1;
    put_panel(0, 0, 30, 3);
    put_centered(1, 0, 30, "BATTLE OVER");
    const char* verdict = outcome == 0 ? "DRAW" : outcome == 1 ? "YOU WIN!" : "YOU LOSE";
    if (winner > 0) {
        put_panel(0, 3, 20, 14);
        put_panel(20, 3, 10, 14);
        put_centered(5, 0, 20, verdict);
        put_centered(7, 0, 20, "WINNER");
        put_centered(8, 0, 20, data.appearance[winner - 1].name.data());
        load_fighter(data, 0, winner);
        preview_character = winner;
    } else {
        put_panel(0, 3, 30, 14);
        put_centered(9, 0, 30, verdict);
        preview_character = 0;
    }
    result_loser = loser;
    if (loser > 0) load_fighter(data, 1, loser);
    put_panel(0, 17, 30, 3);
    put_centered(18, 0, 30, "PRESS A");
}

void menu_result_frame(unsigned frame) {
    int sprites = 0;
    if (preview_character > 0) {
        draw_preview(200, 124, frame, 0);
        sprites = 2;
    }
    if (result_loser > 0) {
        // The player's fighter lying on the ground (the "down" pose), at the same scale as the winner.
        load_pose(1, 17);
        const int origin_x = 80 - 25, origin_y = 124 - 50;
        put_affine_sprite(sprites, origin_x + 11, origin_y, Sprite::square, Sprite::size_32, Vram::body_base + body_cell, 0);
        ++sprites;
    }
    hide_sprites(sprites);
}
