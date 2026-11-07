// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "projectile.hpp"
#include <functional>
#include <vector>

enum class ItemCollision { none, bounced, destroyed };

class GroundItem {
public:
    int slot = -1, last_slot = -1, type = 0, x = 0, y = 0, z = -120;
    int vx = 0, vy = 0, vz = 0, facing = 1, frame = 1, animation = 1, durability = 6, variant = 0;
    bool active = true, settled = false;
    bool just_picked = false;
    bool rest_marker = false;
    int marker_x = 0, marker_y = 0;
    bool detonate = false;
    bool adrift = false;
    int owner = ItemOwnership::unowned;
    // -1 denotes a newly spawned item; -2 denotes an item deliberately disowned.
    int last_owner = ItemOwnership::unowned;
    std::optional<ImpactRegion> falling_impact;
    static GroundItem create(const std::function<int(int)>& draw);
    ItemCollision collide(const ImpactRegion& impact, Combatant& attacker, const ProjectileType& definition,
                std::span<Combatant> fighters, int attacker_index, const std::function<int(int)>& draw);
    bool react_to_fighter_hit(const ProjectileType& definition, int damage, int source_action, const std::function<int(int)>& draw);
    bool claim(Combatant& fighter, int fighter_index, int item_index, const ImpactRegion& marker,
               std::span<GroundItem> items = {}, const std::function<int(int)>& draw = {});
    void animate_holder(Combatant& fighter, int holder, const ProjectileType& definition, int first_special_cost, bool round_active, int held_type,
                        int knocked_type, bool knocked_throwable);
    std::optional<ImpactRegion> follow(Combatant& fighter, const ProjectileType& definition, bool holding, const std::function<int(int)>& draw = {});
    void advance(const ProjectileType& definition, int camera, int upper, int lower, const std::function<int(int)>& draw);
};

void equip_initial_bows(std::span<Combatant> fighters, std::span<const int> controls, std::vector<GroundItem>& items);
