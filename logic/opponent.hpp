// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "combat.hpp"
// Tactic values are also loaded indirectly from negative move range centers.
namespace OpponentTactic {
inline constexpr int none = 0;
inline constexpr int teleport = 1;
inline constexpr int heal = 2;
inline constexpr int make_sword = 3;
inline constexpr int invisibility = 4;
inline constexpr int teleport_sequence = 5;
inline constexpr int summon_weapon = 6;
inline constexpr int aerial_attack = 7;
inline constexpr int ranged_followup = 51;
inline constexpr int align_after_summon = 61;
inline constexpr int aerial_followup = 71;
}
#include "ground_item.hpp"
#include <functional>
#include <span>

struct MoveRange { int center = 0, width = 0, depth = 0, strength = 0; };

// Selects the closest opponent and applies the ordinary approach strategy.
class Opponent {
public:
    using Draw = std::function<int(int)>;
    static bool weapon_attacking(std::span<Combatant> fighters, std::size_t index, int difficulty, const Draw& draw, std::span<const Projectile> projectiles, int upper, int lower);
    static void process_event(std::span<Combatant> fighters, std::size_t index, int difficulty, const Draw& draw, std::span<const Projectile> projectiles, int upper, int lower, char key = 0, std::span<GroundItem> items = {}, std::span<const ProjectileType> types = {});
    static void act(std::span<Combatant> fighters, std::size_t index, int difficulty, const Draw& draw, std::span<GroundItem> items = {}, std::span<const ProjectileType> types = {}, std::span<const SpecialMove> moves = {}, std::span<const MoveRange> ranges = {}, std::span<const Projectile> projectiles = {}, int upper = 0, int lower = 200, std::span<const std::array<SpecialMove, 3>> profiles = {}, bool process_input_event = false, const std::function<std::span<GroundItem>()>& create_pending_item = {});
};
