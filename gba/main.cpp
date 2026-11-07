// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

// Front end: title, fighter selection, battles against the computer, and results.
// The battle rules come from the OpenLF1 logic in logic/.

#include "assets.h"
#include "assets.hpp"
#include "battle.hpp"
#include "hw.hpp"
#include "menus.hpp"
#include "sound.hpp"
#include "video.hpp"
#include "../logic/field.hpp"
#include "../logic/game_data.hpp"
#include "../logic/random.hpp"
#include "../logic/round.hpp"
#include <cstdint>
#include <new>
#include <span>

namespace {

enum class Screen { title, select, battle, result };

constexpr int min_fighters = 2, max_fighters = 8;
// The logic runs at the original 110 ms tick: 6.6 frames at 60 Hz, kept in tenths.
constexpr int frames_per_tick_tenths = 66;
constexpr int fade_step = 2;   // brightness levels per frame; a fade takes 8 frames

std::uint16_t keys_now = 0;
bool held(int player, Action direction) {
    if (player != 0) return false;
    switch (direction) {
    case Action::left: return keys_now & Key::left;
    case Action::right: return keys_now & Key::right;
    case Action::up: return keys_now & Key::up;
    case Action::down: return keys_now & Key::down;
    default: return false;
    }
}

Screen screen = Screen::title;
std::uint16_t keys_before = 0;
unsigned frame = 0;
GameRandom rng(1);
// The parsed game data is large and read mostly when drawing, so it lives in the slower EWRAM.
__attribute__((section(".ewram"), aligned(4))) unsigned char game_data_storage[sizeof(GameData)];
GameData& game_data = *reinterpret_cast<GameData*>(game_data_storage);
FieldTables tables;
Field field(tables, rng);
int tick_budget = 0;
int cursor = 0;
int fighter_count = 4;
int chosen_character = 1;
bool moves_view = false;
bool paused = false;

// What the current screen shows each frame. Called during vertical blank.
void draw_current_screen() {
    switch (screen) {
    case Screen::title: menu_title_frame(frame); break;
    case Screen::select: menu_select_frame(game_data, cursor, fighter_count, moves_view, frame); break;
    case Screen::battle: battle_draw(field, game_data); break;
    case Screen::result: menu_result_frame(frame); break;
    }
}

void fade_out() {
    for (int level = 0; level <= 16; level += fade_step) {
        set_fade(level);
        wait_frame();
        sound_update();
    }
}

// The new screen has been set up behind the black: reveal it, keeping it animated.
void fade_in() {
    for (int level = 16; level >= 0; level -= fade_step) {
        ++frame;
        wait_frame();
        sound_update();
        if (REG_VCOUNT >= 160) draw_current_screen();
        set_fade(level);
    }
}

void enter_select() {
    fade_out();
    screen = Screen::select;
    moves_view = false;
    menu_select_enter(game_data);
    draw_current_screen();
    fade_in();
}

void start_battle() {
    fade_out();
    tables = {};
    for (std::size_t i = 0; i < game_data.appearance.size(); ++i) tables.specials[i] = game_data.appearance[i].specials;
    tables.ranges = game_data.ranges;
    tables.item_types = game_data.item_types;
    for (std::size_t i = 0; i < game_data.boundaries.size(); ++i) tables.bounds[i] = {game_data.boundaries[i].top, game_data.boundaries[i].bottom};
    field.clear();
    battle_start(field, game_data, fighter_count, chosen_character, rng);
    battle_load(field, game_data);
    tick_budget = 0;
    paused = false;
    screen = Screen::battle;
    draw_current_screen();
    fade_in();
}

void finish_battle() {
    const int winner = field.round.winner();
    int player_team = 0, winner_character = 0, player_character = 0;
    for (std::size_t i = 0; i < field.fighters.size(); ++i) {
        if (field.controllers[i] == 0) { player_team = field.fighters[i].team; player_character = field.fighters[i].archetype; }
        if (winner > 0 && field.fighters[i].team == winner && !winner_character) winner_character = field.fighters[i].archetype;
    }
    const int outcome = winner <= 0 ? 0 : winner == player_team ? 1 : 2;
    fade_out();
    screen = Screen::result;
    menu_result_enter(game_data, outcome, winner_character, outcome == 2 ? player_character : 0);
    draw_current_screen();
    fade_in();
}

// The player's current health, or 0 when the player's fighter is not in the battle.
int player_health() {
    for (std::size_t i = 0; i < field.fighters.size() && i < field.controllers.size(); ++i)
        if (field.controllers[i] == 0) return field.fighters[i].health;
    return 0;
}

// Plays the sounds the battle logic asked for during the last press or tick. The screen shake and
// flash that go with explosions and heavy hits happen only if the player was hurt by them.
void flush_sounds(bool player_hurt) {
    for (const auto& play : field.plays) {
        sound_play(play.sample - 1);
        if (player_hurt) battle_impact(play.sample);
    }
    field.plays.clear();
}

void run_battle(std::uint16_t pressed) {
    if (pressed & Key::select) { finish_battle(); return; }
    if (pressed & Key::start) {
        paused = !paused;
        battle_set_paused(paused);
        sound_play(SOUND_MENU_TOGGLE);
    }
    if (paused) return;
    field.pressed_by = 0;
    field.held = held;
    int health = player_health();
    if (pressed & Key::left) field.press(Action::left);
    if (pressed & Key::right) field.press(Action::right);
    if (pressed & Key::up) field.press(Action::up);
    if (pressed & Key::down) field.press(Action::down);
    if (pressed & Key::a) field.press(Action::accept);
    if (pressed & Key::b) field.press(Action::jump);
    flush_sounds(player_health() < health);
    tick_budget += 10;
    if (tick_budget < frames_per_tick_tenths) return;
    tick_budget -= frames_per_tick_tenths;
    health = player_health();
    const bool running = field.step();
    flush_sounds(player_health() < health);
    if (!running) finish_battle();
}
}  // namespace

extern "C" int main() {
    REG_DISPCNT = 0x0080;   // forced blank while the first screen is set up
    REG_WAITCNT = 0x4317;   // ROM: 3 wait states first, 1 after; prefetch on
    video_init();
    REG_DISPCNT = REG_DISPCNT | 0x0080;
    load_bg_palette(pal_bg_bin);
    load_obj_palette(pal_obj_bin);
    battle_init();
    sound_init();
    frame_sync_start();
    new (game_data_storage) GameData(parse_game_data(std::span<const std::uint8_t>(data_bin, data_bin_end)));
    set_fade(16);
    menu_title_enter(game_data);
    draw_current_screen();
    REG_DISPCNT = REG_DISPCNT & ~0x0080;     // show the screen, fully faded to black
    fade_in();

    for (;;) {
        // The start of a frame: the display is in vertical blank, so this is the time to draw.
        // If the last frame's work overran, the picture is skipped once rather than torn.
        wait_frame();
        if (REG_VCOUNT >= 160) draw_current_screen();

        const std::uint16_t keys = ~REG_KEYINPUT & 0x3FF;
        const std::uint16_t pressed = keys & ~keys_before;
        keys_before = keys;
        keys_now = keys;
        ++frame;
        sound_update();

        switch (screen) {
        case Screen::title:
            if (pressed & Key::start) {
                sound_play(SOUND_MENU_CONFIRM);
                rng.seed(std::uint16_t(frame * 2654435761u));
                enter_select();
            }
            break;
        case Screen::select:
            if (!moves_view) {
                if ((pressed & (Key::left | Key::right)) || ((pressed & Key::up) && cursor % 5 > 0) || ((pressed & Key::down) && cursor % 5 < 4)) sound_play(SOUND_MENU_CHANGE);
                if ((pressed & Key::left) || (pressed & Key::right)) cursor = (cursor + 5) % 10;
                if ((pressed & Key::up) && cursor % 5 > 0) --cursor;
                if ((pressed & Key::down) && cursor % 5 < 4) ++cursor;
                if ((pressed & Key::r) && fighter_count < max_fighters) { ++fighter_count; sound_play(SOUND_MENU_TOGGLE); }
                if ((pressed & Key::l) && fighter_count > min_fighters) { --fighter_count; sound_play(SOUND_MENU_TOGGLE); }
                if (pressed & Key::b) { moves_view = true; sound_play(SOUND_MENU_TOGGLE); }
            } else {
                // In the moves view the D-pad steps through the fighters one by one.
                if (pressed & (Key::left | Key::up)) { cursor = (cursor + 9) % 10; sound_play(SOUND_MENU_CHANGE); }
                if (pressed & (Key::right | Key::down)) { cursor = (cursor + 1) % 10; sound_play(SOUND_MENU_CHANGE); }
                if (pressed & Key::b) { moves_view = false; sound_play(SOUND_MENU_TOGGLE); }
            }
            if (pressed & Key::a) {
                sound_play(SOUND_MENU_CONFIRM);
                chosen_character = cursor + 1;
                start_battle();
            }
            break;
        case Screen::battle:
            run_battle(pressed);
            break;
        case Screen::result:
            if (pressed & Key::a) { sound_play(SOUND_MENU_CONFIRM); enter_select(); }
            break;
        }
    }
}
