// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "combat.hpp"
#include "item_types.hpp"
#include <array>
#include <span>
#include <functional>

// Category values stored in the original weapon data.
namespace ItemCategory {
inline constexpr int melee = 1;
inline constexpr int ranged = 2;
inline constexpr int throwable = 3;
}

namespace ItemOwnership {
inline constexpr int unowned = -1;
inline constexpr int detached = -2;
}

enum class ProjectileCollision { none, impact, reflected };

struct ProjectileType {
    int category = 0, motion = 0, effect = 0, damage = 0, knockdown = 0;
    std::array<int, 3> images{};
    int bounce = 1;
};
class Projectile {
public:
    int slot = -1, owner = 0, type = 0, x = 0, y = 0, z = -8, facing = 1;
    int vx = 0, vy = 0, frame = 1, target = -1, lifetime = 30, durability = 1, variant = 0;
    bool active = true, exploding = false, wall_bounce = false, adrift = false;
    static int burst_direction(std::span<const int> used, const std::function<int(int)>& draw);
    ProjectileCollision intercept(const ImpactRegion& hit, Combatant& defender, int defender_index,
                  std::span<const Combatant> fighters, const ProjectileType& definition, bool arrow_can_be_hit);
    ProjectileCollision clash(const Projectile& other, std::span<const Combatant> fighters,
              const ProjectileType& definition, bool arrow_can_be_hit, bool other_can_hit, int other_width = 5, int other_height = 5);
    ImpactRegion advance(const ProjectileType& definition, std::span<const Combatant> fighters, int camera, int upper, int lower,
                         const std::function<int(int)>& draw = {});
};
