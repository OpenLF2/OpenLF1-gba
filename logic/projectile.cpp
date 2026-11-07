// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "projectile.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>

int Projectile::burst_direction(std::span<const int> used, const std::function<int(int)>& draw) {
    int direction;
    do { direction = draw(7) - 3; }
    while (std::find(used.begin(), used.end(), direction) != used.end());
    return direction;
}

ProjectileCollision Projectile::clash(const Projectile& other, std::span<const Combatant> fighters,
                      const ProjectileType& d, bool arrow_can_be_hit, bool other_can_hit, int other_width, int other_height) {
    const int damage_kind = (d.damage + d.effect * ImpactCode::scale) / ImpactCode::scale;
    if (this == &other || !active || exploding || !other.active ||
        owner < 0 || other.owner < 0 || owner >= int(fighters.size()) || other.owner >= int(fighters.size()) ||
        fighters[owner].team == fighters[other.owner].team || !other_can_hit ||
        (type == ItemType::arrow && other.type != ItemType::arrow && !arrow_can_be_hit) ||
        std::abs(y - other.y) >= 8 || std::abs(x - other.x) >= other_width + 12 || std::abs(z - other.z) >= other_height + 7 ||
        (damage_kind != ImpactCode::fire_kind && damage_kind != ImpactCode::explosion_kind && damage_kind != ImpactCode::piercing_kind && damage_kind != ImpactCode::freeze_kind)) return ProjectileCollision::none;
    if (other.type == ItemType::reflected_projectile || other.type == ItemType::summoned_sword) {
        type = ItemType::reflected_projectile;
        facing = other.facing;
        owner = other.owner;
        return ProjectileCollision::reflected;
    }
    if (type == ItemType::reflected_projectile || type == ItemType::summoned_sword) return ProjectileCollision::none;
    return ProjectileCollision::impact;
}

ProjectileCollision Projectile::intercept(const ImpactRegion& hit, Combatant& defender, int defender_index,
                          std::span<const Combatant> fighters, const ProjectileType& d, bool arrow_can_be_hit) {
    if (!active || exploding || owner < 0 || owner >= int(fighters.size()) || (defender_index == owner && d.category != ItemCategory::throwable) ||
        !defender.active || hit.damage <= 0 || (type == ItemType::arrow && !arrow_can_be_hit) ||
        (fighters[owner].team == defender.team && d.category != ItemCategory::throwable) ||
        (defender.priority <= AttackPriority::none && defender.action != FighterAction::punch_hit && defender.action != FighterAction::punch_hit_alt && defender.action != FighterAction::kick_hit && defender.action != FighterAction::kick_hit_alt) ||
        std::abs(defender.y - y) >= 8 || std::abs(defender.x - x) >= hit.width + 12 ||
        std::abs(defender.z - z) >= hit.height + 7) return ProjectileCollision::none;
    const int damage_kind = (d.damage + d.effect * ImpactCode::scale) / ImpactCode::scale;
    if (damage_kind != ImpactCode::fire_kind && damage_kind != ImpactCode::explosion_kind && damage_kind != ImpactCode::piercing_kind && damage_kind != ImpactCode::freeze_kind) return ProjectileCollision::none;
    const bool guarded = defender.action == FighterAction::guard && ((facing == 1 && x > defender.x) || (facing == -1 && x < defender.x));
    if (defender.archetype == FighterType::regenerating_fighter || (!guarded && (defender.action == FighterAction::guard || facing != defender.facing) && type != ItemType::arrow)) {
        defender.energy = std::min(defender.max_energy, defender.energy + 7);
        if (defender.archetype == FighterType::projectile_reflector || type == ItemType::reflected_projectile || hit.damage / ImpactCode::scale == ImpactCode::freeze_kind) type = ItemType::reflected_projectile;
        facing = defender.action == FighterAction::guard ? (x > defender.x ? 1 : -1) : defender.facing;
        owner = defender_index;
        return ProjectileCollision::reflected;
    }
    if (type == ItemType::reflected_projectile || type == ItemType::summoned_sword) return ProjectileCollision::none;
    defender.energy = std::min(defender.max_energy, defender.energy + 7);
    owner = defender_index;
    return ProjectileCollision::impact;
}

ImpactRegion Projectile::advance(const ProjectileType& d, std::span<const Combatant> fighters, int camera, int upper, int lower,
                                 const std::function<int(int)>& draw) {
    if (type == ItemType::sword_projectile && --lifetime <= 0) { active = false; return {}; }
    if (d.motion == ItemMotion::homing || ((type == ItemType::summoned_weapon || type == ItemType::random_target_projectile) && lifetime > 0)) {
        const bool fighter_owner = owner >= 0 && owner < int(fighters.size());
        const int owner_team = fighter_owner ? fighters[owner].team : owner == ItemOwnership::detached ? 0 : -1;
        if ((type == ItemType::summoned_weapon || type == ItemType::random_target_projectile) && lifetime > 0) --lifetime;
        if (target < 0 || target == owner || target >= int(fighters.size()) || !fighters[target].active || fighters[target].just_eliminated) {
            target = -1;
            int nearest = std::numeric_limits<int>::max();
            std::array<int, 8> candidates{};
            int count = 0;
            for (int i = 0; i < int(fighters.size()); ++i) {
                const auto& f = fighters[i];
                if (!f.active || f.just_eliminated || i == owner || f.team == owner_team || (type != ItemType::random_target_projectile && f.state / 10 == FighterStateGroup::angel)) continue;
                if (type == ItemType::random_target_projectile && count < int(candidates.size())) { candidates[count++] = i; continue; }
                const int distance = std::abs(f.x - x) + std::abs(f.y - y);
                if (distance < nearest) { nearest = distance; target = i; }
            }
            if (type == ItemType::random_target_projectile && count > 0) target = candidates[draw ? draw(count) : 0];
            if (target < 0 && fighter_owner) target = owner;
            vx = facing * 17;
        }
        if (target >= 0) {
            const auto& aim = fighters[target];
            vx = std::clamp(vx + (x > aim.x ? -2 : 2), -17, 17);
            vy = std::clamp(vy + (y > aim.y ? -1 : 1), -3, 3);
            facing = vx > 0 ? 1 : -1;
        }
    } else {
        vx = facing * ((d.effect == ImpactCode::fire_kind || d.effect == ImpactCode::freeze_kind) && type != ItemType::firen_weapon && type != ItemType::random_target_projectile ? 12 : 17);
    }
    z = -7;
    if (d.motion != ItemMotion::fixed_frame) { frame = 3 - frame; if (!frame) frame = 1; }
    int push = d.knockdown == ItemKnockdown::none ? 69 : ImpactCode::knockdown;
    if (type == ItemType::plus) push = 25;
    if (type == ItemType::sword_projectile) push = 35;
    if (d.effect == ImpactCode::normal_kind || type == ItemType::sword_projectile || type == ItemType::summoned_weapon) push += 1000;
    if (type == ItemType::firen_weapon) push += 2000;
    const ImpactRegion area{x + facing * 13, y, z, 5, 5, facing, facing * 5, -7, d.damage + d.effect * ImpactCode::scale, push};
    x += vx;
    y = std::clamp(y + vy, upper - 3, lower + 2);
    if (x <= camera - 190 || x >= camera + 510) {
        wall_bounce = durability > 8;
        active = false;
    }
    return area;
}
