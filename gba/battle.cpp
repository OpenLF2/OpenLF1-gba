// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "battle.hpp"
#include "assets.hpp"
#include "assets.h"
#include "hw.hpp"
#include "video.hpp"
#include "../logic/combat.hpp"
#include "../logic/game_state.hpp"
#include "../logic/ground_item.hpp"
#include "../logic/item_types.hpp"
#include "../logic/roster.hpp"
#include "../logic/round.hpp"
#include <algorithm>
#include <array>
#include <string>

// The original battlefield is 320 pixels wide and 144 pixels tall, starting at y = 56
// below its status panel. The GBA shows it 3/4 as wide (240 pixels) and at full height,
// starting at row 16 under a 16-pixel HUD strip. See tools/convert_assets.py.
namespace {
constexpr int stage_top = 56;
constexpr int hud_height = 16;

int gx(int x) { return (x * 3) >> 2; }
int gy(int y) { return y - (stage_top - hud_height); }

// Head offsets from the original battle view, indexed by fighter action.
struct HeadOffsets {
    std::array<int, FighterAction::normal_pose_count + 1> x{}, y{};
    HeadOffsets() {
        for (int i = 0; i <= FighterAction::normal_pose_count; ++i) {
            if (i == 25) x[i] = 5;
            if (i == 7 || i == 11 || i == 24 || i == 38 || i == 39) x[i] = 4;
            if (i == 44 || i == 45 || i == 46 || i == 50 || i == 51 || i > 57) x[i] = 3;
            if (i == 21) y[i] = 3;
            if ((i >= 12 && i <= 20) || i == 22 || i == 23 || (i >= 26 && i <= 31) ||
                (i >= 33 && i <= 43) || (i >= 52 && i <= 56) || i == 47 || i == 48) y[i] = 50;
            if (i == 50 || i == 51 || i == 38) y[i] = 60;
            if (i == 39) y[i] = 61;
        }
    }
};
const HeadOffsets head_offsets;

// Body cells: 8 colour variants, the fixed-colour default, then the second figure sheet.
constexpr int default_variant = DEFAULT_VARIANT;
constexpr int extra_variant = DEFAULT_VARIANT + 1;
int body_index(int variant, int pose) { return variant * BODY_POSES + pose; }

// Dynamic sprite slots for items, projectiles and effects, shared between objects.
template <int Count>
struct Pool {
    unsigned base, cell_bytes;
    std::array<unsigned, Count> id{};
    std::array<bool, Count> claimed{};
    int next = 0;

    void reset() { id.fill(0); claimed.fill(false); }
    void release_claims() { claimed.fill(false); }
    // Returns the VRAM offset holding cell 'key' (loading it from 'source' if needed), or -1 if none is free.
    int get(unsigned key, const std::uint8_t* source) {
        for (int i = 0; i < Count; ++i) if (id[i] == key && !claimed[i]) { claimed[i] = true; return int(base + i * cell_bytes); }
        for (int k = 0; k < Count; ++k) {
            const int i = (next + k) % Count;
            if (claimed[i]) continue;
            dma_copy(source, OBJ_VRAM + base + i * cell_bytes, cell_bytes);
            id[i] = key;
            claimed[i] = true;
            next = (i + 1) % Count;
            return int(base + i * cell_bytes);
        }
        return -1;
    }
};
const std::uint8_t* const stage_tiles[STAGE_COUNT] = {stage0_bin, stage1_bin, stage2_bin, stage3_bin, stage4_bin};
const std::uint16_t* const stage_maps[STAGE_COUNT] = {stage0_map_bin, stage1_map_bin, stage2_map_bin, stage3_map_bin, stage4_map_bin};

Pool<20> small_pool{Vram::pool16_base, 256};
Pool<3> large_pool{Vram::pool32_base, 1024};
int body_key_in_slot[FIGHTER_SLOTS];

// Screen shake and flash after explosions, and how long the round banners have been shown.
int shake_frames = 0, flash_frames = 0, banner_age = 0, knockout_age = 0;
bool flash_showing = false;
bool battle_paused = false;
int shake_x() { return shake_frames > 0 ? ((shake_frames & 1) ? (shake_frames > 6 ? 2 : 1) : (shake_frames > 6 ? -2 : -1)) : 0; }
int shake_y() { return shake_frames > 0 ? ((shake_frames & 2) ? 1 : -1) : 0; }

// Sprites are collected back to front and written so that the front-most has the lowest index.
class Sprites {
public:
    // 'box' is the sprite's drawn size, 'content' the width of the picture inside it. A flipped
    // sprite mirrors the whole box, so it is shifted to keep the picture where it was.
    void add(int x, int y, int shape, int size, int box, int content, unsigned vram, bool flip = false) {
        if (count_ >= oam_count) return;
        entries_[count_++] = {std::int16_t(flip ? x - (box - content) : x), std::int16_t(y), std::uint16_t(vram), std::uint8_t(shape), std::uint8_t(size), flip, -1};
    }
    // A sprite drawn through affine matrix 'group' (see set_affine); (x, y) is its double-size box.
    void add_affine(int x, int y, int shape, int size, unsigned vram, int group) {
        if (count_ >= oam_count) return;
        entries_[count_++] = {std::int16_t(x), std::int16_t(y), std::uint16_t(vram), std::uint8_t(shape), std::uint8_t(size), false, std::int8_t(group)};
    }
    void add_plain(int x, int y, int shape, int size, unsigned vram) { add(x, y, shape, size, 0, 0, vram); }
    void flush() {
        for (int i = 0; i < count_; ++i) {
            const auto& e = entries_[i];
            if (e.group >= 0) put_affine_sprite(count_ - 1 - i, e.x + shake_x(), e.y + shake_y(), e.shape, e.size, e.vram, e.group);
            else put_sprite(count_ - 1 - i, e.x + shake_x(), e.y + shake_y(), e.shape, e.size, e.vram, e.flip);
        }
        hide_sprites(count_);
        count_ = 0;
    }
private:
    struct Entry { std::int16_t x, y; std::uint16_t vram; std::uint8_t shape, size; bool flip; std::int8_t group; };
    std::array<Entry, oam_count> entries_;   // filled before use; not cleared each frame
    int count_ = 0;
};

// Shadow under a figure: 19 pixels of the 32x8 shadow sprite.
void add_shadow(Sprites& out, int x, int y) {
    out.add(gx(x), gy(y), Sprite::horizontal, Sprite::size_32x8, 32, 19, Vram::shadow_base);
}

// Item or projectile at its position, plus the shadow when it is on the battlefield.
void add_item(Sprites& out, const Field& field, const GameData& data, int x, int y, int z, int facing, int image, bool shadow) {
    if (image < 0 || image >= ITEM_COUNT) return;
    const auto& bounds = data.boundaries[field.stage + 1];
    if (shadow && x > bounds.left + 10 && x < bounds.right - 10) add_shadow(out, x - 12, y - 1);
    const int offset = small_pool.get(image + 1, items_bin + image * 256);
    if (offset >= 0) out.add(gx(x - 8), gy(y + z - 12), Sprite::square, Sprite::size_16, 16, 13, offset, facing < 0);
}

// Body or halo of a fighter, using the fighter's own slot (or the pool for extra figures).
unsigned body_vram(int slot, int key) {
    if (body_key_in_slot[slot] != key) {
        dma_copy(body_bin + key * 1024, OBJ_VRAM + Vram::body_base + slot * 1024, 1024);
        body_key_in_slot[slot] = key;
    }
    return Vram::body_base + slot * 1024;
}

int marker_variant(int tint) {
    if (tint == 0) return MARKER_VARIANTS - 1;
    for (int j = 0; j < 8; ++j) if (TEAM_COLORS[j] - 15 == tint) return j;
    return MARKER_VARIANTS - 1;
}

// Player number above a fighter ("c" for computer players), with the team's outline colour.
void add_marker(Sprites& out, int marker, int x, int y, int tint) {
    char text[3];
    int length = 0;
    if (marker == 0) text[length++] = 'c';
    else {
        if (marker >= 10) text[length++] = char('0' + marker / 10);
        text[length++] = char('0' + marker % 10);
    }
    const int variant = marker_variant(tint);
    for (int k = 0; k < length; ++k) {
        const int glyph = text[k] == 'c' ? 10 : text[k] - '0';
        out.add(gx(x), gy(y), Sprite::square, Sprite::size_8, 8, 5, Vram::marker_base + (variant * MARKER_GLYPHS + glyph) * 64);
        x += 5;
    }
}

void draw_fighter(Sprites& out, Field& field, int i, const GameData& data) {
    auto& f = field.fighters[i];
    if (!f.active) return;
    if (f.death_pose > 0) {
        const int pose = f.death_pose - 1;
        out.add(gx(f.x - 12), gy(f.y - 24), Sprite::square, Sprite::size_32, 32, 19, body_vram(i, body_index(default_variant, pose)), f.facing < 0);
        f.death_pose = 0;
        return;
    }
    const auto& look = data.appearance[f.archetype - 1];
    const bool angel = f.state / 10 == FighterStateGroup::angel;
    if (!angel) add_shadow(out, f.x - 12 - f.facing * 2, f.y - 1);
    else if (f.z > -50 && (!(field.tick & 1) || f.z > -25)) {
        const int key = body_index(default_variant, 36);
        const int offset = large_pool.get(key + 1, body_bin + key * 1024);
        if (offset >= 0) out.add(gx(f.x - 12), gy(f.y - 24), Sprite::square, Sprite::size_32, 32, 19, offset, f.facing < 0);
    }
    if (f.invisible_ticks > 10 || f.invisible_ticks % 2 != 0) return;
    if (angel && f.archetype == FighterType::regenerating_fighter) return;

    const int marker_tint = f.energy >= look.specials[2].cost && (field.tick & 1) ? 0 : f.team_color - 15;
    add_marker(out, f.marker, f.x - (f.marker == 10 ? 7 : 2) - f.facing * 2, f.y + f.z - 30, marker_tint);
    if (f.effect_ticks > 0 && f.effect_ticks % 2 != 0) return;

    const bool extra = f.action >= FighterAction::angel_alt;
    const int pose = std::clamp(f.action - (extra ? FighterAction::normal_pose_count : 0), 1, FighterAction::normal_pose_count) - 1;
    const int head_cell = Vram::head_base + i * 256;
    const auto head_at = [&](int y) {
        const int x = f.x - f.facing * 4 + head_offsets.x[f.action] * f.facing - 8;
        out.add(gx(x), gy(y), Sprite::square, Sprite::size_16, 16, 13, head_cell, f.facing < 0);
    };
    if (!extra && head_offsets.y[f.action] >= 60) head_at(f.y + head_offsets.y[f.action] + f.z - 84);

    const bool fixed_color = (pose >= 35 && pose <= 41 && pose != 37 && pose != 38 && pose != 39) || (pose >= 51 && pose <= 55);
    const bool tinted = !extra && f.action < FighterAction::death_pose && !fixed_color;
    const int key = extra ? body_index(extra_variant, pose) : body_index(tinted ? i : default_variant, pose);
    out.add(gx(f.x - 12), gy(f.y + f.z - 24), Sprite::square, Sprite::size_32, 32, 19, body_vram(i, key), f.facing < 0);

    if (!extra && head_offsets.y[f.action] < 50) head_at(f.y + head_offsets.y[f.action] + f.z - 24);
}

// Health and energy in the strip above the battlefield: each fighter has an icon (the head over the
// top of the body, so the whole face shows) and two bars.
void draw_hud(Sprites& out, const Field& field) {
    for (int i = 0; i < int(field.fighters.size()) && i < FIGHTER_SLOTS; ++i) {
        const auto& f = field.fighters[i];
        if (!f.active && !f.eliminated) continue;
        const int x = i * 30;
        out.add_plain(x, 0, Sprite::horizontal, Sprite::size_32x16, Vram::icon_base + i * 512);
        if (f.eliminated) continue;
        // The player's own slot carries a "1" badge in white.
        if (i < int(field.controllers.size()) && field.controllers[i] == 0)
            out.add_plain(x + 11, 7, Sprite::square, Sprite::size_8, Vram::marker_base + ((MARKER_VARIANTS - 1) * MARKER_GLYPHS + 1) * 64);
        const int health = std::clamp(f.health * BAR_FILLS / std::max(1, f.max_health), 0, BAR_FILLS - 1);
        const int energy = std::clamp(f.energy * BAR_FILLS / std::max(1, f.max_energy), 0, BAR_FILLS - 1);
        out.add_plain(x + 20, 6, Sprite::horizontal, Sprite::size_16x8, Vram::bar_base + (BAR_FILLS + energy) * 128);
        // A fighter below a quarter of its health blinks its health bar.
        if (health * 4 < BAR_FILLS && (field.tick & 1)) continue;
        out.add_plain(x + 20, 1, Sprite::horizontal, Sprite::size_16x8, Vram::bar_base + health * 128);
    }
}

// Large letters made from scaled glyph sprites, centred on the battlefield. 'letters' index BANNER_GLYPHS.
void draw_banner(Sprites& out, const int* letters, int count, int age) {
    // The text lands from large to normal size over its first frames.
    const int magnification = age < 3 ? 6 : age < 6 ? 5 : age < 9 ? 4 : 3;
    set_affine(1, 256 / magnification, 256 / magnification);
    const int pitch = 6 * magnification;
    const int left = Display::width / 2 - pitch * count / 2 + pitch / 2;
    for (int i = 0; i < count; ++i) {
        const int offset = small_pool.get(500 + letters[i], banner_bin + letters[i] * 256);
        if (offset < 0) continue;
        // A 16x16 cell is shown in a 32x32 box centred on the cell's centre; the letter is at the centre.
        out.add_affine(left + i * pitch - 16, 100 - 16, Sprite::square, Sprite::size_16, offset, 1);
    }
}

}  // namespace

void battle_set_paused(bool paused) { battle_paused = paused; }

void battle_impact(int sound) {
    if (sound == BattleSound::explosion) { shake_frames = 12; flash_frames = 5; }
    else if (sound == BattleSound::heavy_impact && shake_frames < 5) shake_frames = 5;
}

void battle_start(Field& field, const GameData& data, int fighters, int character, GameRandom& random) {
    field.clear();
    field.stage = random.draw(4);
    field.camera = 0;
    field.tick = 0;
    field.round = {};
    const std::array<Participant, GameLimits::human_players> people{Participant{true, character, 1}, {}, {}};
    const auto draw = [&](int limit) { return random.draw(limit); };
    const auto& bounds = data.boundaries[0];
    const auto match = Roster::create(GameMode::free_for_all, fighters, people, bounds.top, bounds.bottom, draw);
    field.fighters.assign(match.fighters.begin(), match.fighters.end());
    Round::assign_colors(field.fighters);
    field.controllers.assign(match.controls.begin(), match.controls.end());
    equip_initial_bows(field.fighters, field.controllers, field.ground_items);
}

void battle_init() {
    load_obj_tiles(shadow_bin, Vram::shadow_base, 256);
    load_obj_tiles(bars_bin, Vram::bar_base, 2 * BAR_FILLS * 128);
    load_obj_tiles(markers_bin, Vram::marker_base, MARKER_VARIANTS * MARKER_GLYPHS * 64);
}

void battle_load(const Field& field, const GameData& data) {
    load_bg_tiles(stage_tiles[field.stage], STAGE_TILES[field.stage] * 64);
    set_bg_map(stage_maps[field.stage]);
    // The menus borrow the marker area for icons, so the marker glyphs are loaded again.
    load_obj_tiles(markers_bin, Vram::marker_base, MARKER_VARIANTS * MARKER_GLYPHS * 64);
    small_pool.reset();
    large_pool.reset();
    shake_frames = flash_frames = banner_age = knockout_age = 0;
    battle_paused = false;
    flash_showing = false;
    for (int i = 0; i < FIGHTER_SLOTS; ++i) body_key_in_slot[i] = -1;
    for (int i = 0; i < int(field.fighters.size()) && i < FIGHTER_SLOTS; ++i) {
        const auto& f = field.fighters[i];
        if (!f.active) continue;
        const auto& look = data.appearance[f.archetype - 1];
        dma_copy(heads_bin + (look.head - 1) * 256, OBJ_VRAM + Vram::head_base + i * 256, 256);
        dma_copy(icons_bin + (i * HEAD_COUNT + look.head - 1) * 512, OBJ_VRAM + Vram::icon_base + i * 512, 512);
        for (int c = 0; c < TINT_COUNT; ++c) {
            const int index = c == TINT_COUNT - 1 ? data.hair[look.head - 1] : look.color[c];
            OBJ_PALETTE[TINT_INDEX[i][c]] = pal_bg_bin[index];
        }
    }
}

void battle_draw(Field& field, const GameData& data) {
    set_bg_scroll(((3 * (160 - field.camera)) >> 2) + shake_x(), shake_y());
    small_pool.release_claims();
    large_pool.release_claims();
    static Sprites out;

    struct Visible { int y; int kind; std::size_t index; };
    enum Kind { fighter, item, projectile, blast, hit_effect };
    static std::array<Visible, 64> depth;
    int count = 0;
    const auto add = [&](int y, int kind, std::size_t index) { if (count < int(depth.size())) depth[count++] = {y, kind, index}; };
    for (std::size_t i = 0; i < field.fighters.size(); ++i) if (field.fighters[i].active) add(field.fighters[i].y, fighter, i);
    for (std::size_t i = 0; i < field.ground_items.size(); ++i) if (field.ground_items[i].active) add(field.ground_items[i].y, item, i);
    for (std::size_t i = 0; i < field.projectiles.size(); ++i) if (field.projectiles[i].active) add(field.projectiles[i].y, projectile, i);
    for (std::size_t i = 0; i < field.blasts.size(); ++i) if (field.blasts[i].active) add(field.blasts[i].y, blast, i);
    for (std::size_t i = 0; i < field.hit_effects.size(); ++i) add(field.hit_effects[i].y, hit_effect, i);
    std::stable_sort(depth.begin(), depth.begin() + count, [](const auto& a, const auto& b) { return a.y < b.y; });

    for (int d = 0; d < count; ++d) {
        const auto& visible = depth[d];
        switch (visible.kind) {
        case fighter:
            draw_fighter(out, field, int(visible.index), data);
            break;
        case item: {
            const auto& g = field.ground_items[visible.index];
            if (g.frame == 4) break;
            const int image = data.item_types[g.type].images[g.frame - 1] - 1 + (g.type == ItemType::drink && g.variant == 1 ? 59 : 0);
            add_item(out, field, data, g.x, g.y, g.z, g.facing, image, true);
            break;
        }
        case projectile: {
            const auto& p = field.projectiles[visible.index];
            add_item(out, field, data, p.x, p.y, p.z, p.facing, data.item_types[p.type].images[p.frame - 1] - 1, true);
            break;
        }
        case blast: {
            const auto& b = field.blasts[visible.index];
            const int frame = b.frame - 1 - b.offset;
            if (frame < 0 || frame >= BLAST_FRAMES) break;
            const int offset = large_pool.get(1000 + frame, blasts_bin + frame * 1024);
            if (offset >= 0) out.add(gx(b.x - 17), gy(b.y + b.z - 27), Sprite::square, Sprite::size_32, 32, 26, offset);
            break;
        }
        case hit_effect: {
            const auto& e = field.hit_effects[visible.index];
            const int image = data.item_types[ItemType::hit_effect].images[e.frame - 1] - 1;
            add_item(out, field, data, e.x, e.y, e.z, e.facing, image, true);
            break;
        }
        }
    }
    draw_hud(out, field);
    static const int fight[] = {0, 1, 2, 3, 4, 5};      // F I G H T !
    static const int knockout[] = {6, 7, 8, 7};         // K . O .
    static const int paused[] = {9, 10, 11, 12, 13, 14};   // P A U S E D
    if (battle_paused) {
        draw_banner(out, paused, 6, 100);
    } else if (field.round.ongoing()) {
        knockout_age = 0;
        if (field.tick < 12) draw_banner(out, fight, 6, banner_age++);
    } else {
        draw_banner(out, knockout, 4, knockout_age++);
    }
    out.flush();
    // A white flash fading out, then back to the normal (not faded) display.
    if (flash_frames > 0) {
        REG_BLDCNT = 0x00B1;
        REG_BLDY = std::uint16_t(flash_frames * 2);
        --flash_frames;
        flash_showing = true;
    } else if (flash_showing) {
        set_fade(0);
        flash_showing = false;
    }
    if (shake_frames > 0) --shake_frames;
}
