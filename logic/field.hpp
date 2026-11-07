// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "blast.hpp"
#include "combat.hpp"
#include "action.hpp"
#include "game_state.hpp"
#include "grapple.hpp"
#include "ground_item.hpp"
#include "opponent.hpp"
#include "projectile.hpp"
#include "random.hpp"
#include "round.hpp"
#include <array>
#include <functional>
#include <vector>

struct StageBounds { int top, bottom; };

struct FieldTables {
    std::array<std::array<SpecialMove, GameLimits::special_moves>, GameLimits::characters> specials{};
    std::array<MoveRange, GameLimits::move_ranges> ranges{};
    std::array<ProjectileType, GameLimits::item_definitions> item_types{};
    std::array<StageBounds, GameLimits::stage_table_entries> bounds{};
};

namespace Slots {
inline constexpr int objects_end = 30;
inline constexpr int first_spawn = 25;
}
struct HitEffect { int x, y, z, facing, frame = 1, age = 0, slot = -1; bool doomed = false, as_item = false; };

class Field {
public:
    struct SoundRequest { int sample; float gain; };
    static constexpr std::size_t hit_slots = 30;

    Field(const FieldTables& tables, GameRandom& rng) : random(rng), tables_(tables) {}

    std::function<bool(int, Action)> held;
    int pressed_by = 0;

    std::vector<Combatant> fighters;
    std::vector<int> controllers;
    std::vector<Projectile> projectiles;
    std::vector<GroundItem> ground_items;
    std::vector<Blast> blasts;
    std::vector<HitEffect> hit_effects;
    std::vector<SoundRequest> plays;
    Round round;
    GameRandom& random;
    unsigned tick = 0;
    int stage = 0, camera = 0, difficulty = 5;
    bool power_phase = false;
    bool spawn_items = true;

    void press(Action input);
    bool step();
    void clear();
    bool slot_used(int slot) const;
    int free_slot(int first) const;

private:
    std::array<int, hit_slots> hit_belong_{};
    bool thing_in_slot(int slot) const;
    struct SlotMemory { int type = 0, y = 0, owner = 0, facing = 1; };
    std::array<SlotMemory, hit_slots + 1> slot_memory_{};
    std::array<int, hit_slots + 1> slot_life_{};
    struct Input {
        Field& field;
        bool held(int player, Action direction) const { return field.held && field.held(player, direction); }
        int player() const { return field.pressed_by; }
        void sound(int sample, float gain = 0.6f) { field.plays.push_back({sample, gain}); }
    };
    Input display{*this};
    const FieldTables& tables_;
};
