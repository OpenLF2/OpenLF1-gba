// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once

namespace GameLimits {
inline constexpr int human_players = 3;
inline constexpr int fighter_slots = 8;
inline constexpr int characters = 11;
inline constexpr int selectable_characters = 10;
inline constexpr int heads = 55;
inline constexpr int special_moves = 3;
inline constexpr int stages = 5;
inline constexpr int stage_table_entries = 10;
inline constexpr int move_ranges = 50;
inline constexpr int item_definitions = 50;
inline constexpr int battle_sounds = 18;
inline constexpr int max_difficulty = 10;
}

namespace GameTiming {
inline constexpr int tick_ms = 110;
inline constexpr int bracket_delay_ms = 1540;
inline constexpr int selection_delay_ms = 1650;
inline constexpr int overview_delay_ms = 1100;
inline constexpr int demo_idle_ms = 11000;
inline constexpr int opening_frames = 120;
inline constexpr int opening_fade_in_frames = 10;
inline constexpr int opening_fade_out_start = 110;
inline constexpr int opening_level_step = 6;
inline constexpr int opening_skip_frames = 10;
inline constexpr int opening_skip_start_frames = 110;
}

namespace GameMode {
enum Value { free_for_all = 1, teams = 2, tournament = 3 };
}

namespace GameScreen {
enum Value {
    main_menu = 0, mode_selection = 1, character_selection = 2, battle = 3,
    tournament_count = 4, tournament_selection = 5, tournament_bracket = 6,
    tournament_winner = 7, tournament_controls = 8, match_overview = 9
};
}

enum class SelectionPhase { inactive, character, team, ready };
enum class DebugMode { off, battle_controls, character_selection };

// Indices in the original battle sound bank.
namespace BattleSound {
enum Value {
    attack = 1, light_impact = 2, heavy_impact = 3, freeze = 4,
    weapon_impact = 5, explosion = 7, bounce = 8, teleport = 9,
    landing = 10, projectile = 11, round_end = 13, healing = 14,
    drinking = 15, footstep = 16, sword_creation = 18
};
}
