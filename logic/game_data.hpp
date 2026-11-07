// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "field.hpp"
#include "game_state.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

struct Appearance {
    int head;
    std::array<int, 9> color;
    std::array<SpecialMove, GameLimits::special_moves> specials;
    std::array<char, 16> name{};
};

struct StageLimits { int left, right, top, bottom, edge_left, edge_right; };

// Everything DATA.DAT describes.
struct GameData {
    std::array<Appearance, GameLimits::characters> appearance{};
    std::array<int, GameLimits::heads> hair{};
    std::array<StageLimits, GameLimits::stage_table_entries> boundaries{};
    std::array<MoveRange, GameLimits::move_ranges> ranges{};
    std::array<ProjectileType, GameLimits::item_definitions> item_types{};
};

GameData parse_game_data(std::span<const std::uint8_t> data);
