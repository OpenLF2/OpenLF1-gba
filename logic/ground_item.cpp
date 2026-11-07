// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "ground_item.hpp"
#include <algorithm>
#include <cstdlib>

GroundItem GroundItem::create(const std::function<int(int)>& draw) {
    GroundItem result;
    result.variant = -1;
    for (;;) {
        result.type = draw(10);
        const bool rejected = result.type == ItemType::bomb || result.type == ItemType::sword || result.type == ItemType::plus || result.type == ItemType::bow;
        if (!rejected) {
            if (result.type == ItemType::drink) draw(1);
            break;
        }
        result.variant = draw(2);
    }
    result.x = draw(267) + 27;
    result.y = draw(66) + 120;
    result.facing = 1 - draw(2) * 2;
    result.durability = result.type == ItemType::stone || result.type == ItemType::box || result.type == ItemType::claw ? 2 : 6;
    return result;
}
void GroundItem::advance(const ProjectileType& d, int camera, int upper, int lower, const std::function<int(int)>& draw) {
    falling_impact.reset();
    rest_marker = false;
    if (!active || owner >= 0) return;
    z += vz;
    const int ground = type == ItemType::stone || type == ItemType::box ? 0 : 3;
    if (z >= 200) active = false;
    if (z > ground) {
        if (vz > 10) {
            vz = -vz / std::max(1, d.bounce);
            z = ground - 1;
            if (vx == 0) vx = draw(19) - 9;
        } else {
            vz = 0; z = ground; frame = 3; animation = 1;
            if (--durability <= 0) { active = false; vy = 0; }
            if (durability <= 4 && type == ItemType::bomb) { active = false; detonate = true; }
        }
        vx /= 2;
    }
    const bool random_facing = draw(2) != 0;
    rest_marker = z == ground && active;
    settled = z == ground && vz == 0;
    if (rest_marker) vy = 0;
    if (z < ground) {
        do {
            if (++animation > 6) animation = 1;
            facing = animation == 1 || animation == 4 || animation == 5 ? 1 : -1;
            frame = animation == 1 || animation == 6 ? 1 : animation == 3 || animation == 4 ? 2 : 3;
        } while ((d.motion == 2 || d.motion == ItemMotion::homing || d.motion == ItemMotion::ignore_downed) && frame == 3);
        if (ground == 0) {
            const int direction = vx != 0 ? (vx > 0 ? 1 : -1) : (random_facing ? 2 : -1);
            falling_impact = ImpactRegion{x, y, z, 10, 13, direction, vx / 2, -7, d.damage, 500};
        }
        vz += 3;
    }
    if (settled) vx = 0;
    marker_x = x; marker_y = y;
    x += vx; y = std::clamp(y + vy, upper - 3, lower + 2);
    if (x <= camera - 190 || x >= camera + 510) {
        if (durability > 8) {
            settled = false; animation = 1; frame = 3; vz = -17;
            vx = x <= camera - 190 ? 10 : -10;
        } else active = false;
    }
}

bool GroundItem::react_to_fighter_hit(const ProjectileType& d, int damage, int source_action, const std::function<int(int)>& draw) {
    if (damage / ImpactCode::scale >= ImpactCode::fire_kind || d.effect == ImpactCode::piercing_kind || (source_action >= FighterAction::weapon_swing_first && source_action <= FighterAction::weapon_swing_last)) return false;
    if (durability < 500) {
        animation = 1; frame = 3;
        if (d.category == ItemCategory::throwable) { vz = -3; vx /= 2; }
        else { vz = -7; vx = draw(7) - 3; }
    }
    return true;
}

ItemCollision GroundItem::collide(const ImpactRegion& hit, Combatant& attacker, const ProjectileType& d,
                        std::span<Combatant> fighters, int attacker_index, const std::function<int(int)>& draw) {
    if (!active || hit.damage <= 0 || (owner == attacker_index && d.category != ItemCategory::throwable) || !attacker.active) return ItemCollision::none;
    if (owner >= 0 && owner < int(fighters.size()) && fighters[owner].team == attacker.team && d.category != ItemCategory::throwable) return ItemCollision::none;
    if (attacker.priority <= AttackPriority::none && attacker.action != FighterAction::punch_hit && attacker.action != FighterAction::punch_hit_alt && attacker.action != FighterAction::kick_hit && attacker.action != FighterAction::kick_hit_alt) return ItemCollision::none;
    if (std::abs(attacker.y - y) >= 8 || std::abs(attacker.x - x) >= hit.width + 12 ||
        std::abs(attacker.z - z) >= hit.height + 7) return ItemCollision::none;
    if (d.effect != ImpactCode::none_kind && d.effect != ImpactCode::normal_kind && d.effect != ImpactCode::fire_kind && d.effect != ImpactCode::explosion_kind && d.effect != ImpactCode::piercing_kind && d.effect != ImpactCode::freeze_kind) return ItemCollision::none;
    if (owner >= 0 && owner < int(fighters.size())) {
        auto& holder = fighters[owner];
        holder.carried = ItemSlot::none;
        if (holder.state / 50 == FighterStateGroup::item) { holder.state = FighterState::idle; holder.action = FighterAction::idle; }
    }
    owner = ItemOwnership::unowned;
    last_owner = attacker_index;
    attacker.energy = std::min(attacker.max_energy, attacker.energy + 7);
    if (d.effect == ImpactCode::fire_kind || d.effect == ImpactCode::explosion_kind || d.effect == ImpactCode::piercing_kind || d.effect == ImpactCode::freeze_kind) {
        active = false;
        return ItemCollision::destroyed;
    }
    if (d.effect == ImpactCode::none_kind || d.effect == ImpactCode::normal_kind) {
        settled = false; animation = 1; frame = 3;
        if (d.category == ItemCategory::throwable) { vz = -5; vx = hit.push_x; }
        else { vz = -7; vx = draw(7) - 3; }
        return ItemCollision::bounced;
    }
    return ItemCollision::none;
}
bool GroundItem::claim(Combatant& fighter, int fighter_index, int item_index, const ImpactRegion& marker, std::span<GroundItem> items, const std::function<int(int)>& draw) {
    if (!active || !fighter.active || fighter.priority > AttackPriority::none || fighter.z != 0 ||
        fighter.history.back() != InputKey::attack || (fighter.state != FighterState::punch_active && fighter.state != FighterState::kick_active && fighter.state / 50 != FighterStateGroup::item) ||
        std::abs(fighter.y - marker.y) >= 8 || std::abs(fighter.x - fighter.facing * 2 - marker.x) >= marker.width + 10 ||
        std::abs(fighter.z - 10 - marker.z) >= marker.height + 11) return false;
    if (fighter.state / 50 == FighterStateGroup::item && fighter.carried >= 0 && fighter.carried < int(items.size()) &&
        items[fighter.carried].type == ItemType::summoned_weapon) items[fighter.carried].active = false;
    owner = fighter_index;
    last_owner = fighter_index;
    just_picked = true;
    fighter.carried = item_index;
    fighter.state = FighterState::holding_item;
    fighter.action = FighterAction::landing;
    fighter.remember(InputKey::interrupted);
    return true;
}
void GroundItem::animate_holder(Combatant& fighter, int holder, const ProjectileType& d, int first_special_cost, bool round_active, int held_type,
                                int knocked_type, bool knocked_throwable) {
    if (fighter.state < FighterState::item_throw_base) {
        if (fighter.action == FighterAction::landing) fighter.action = FighterAction::idle;
        if (fighter.velocity_x != 0 || fighter.velocity_y != 0)
            if (++fighter.animation_step > CombatTuning::animation_cycle_frames) fighter.animation_step = 1;
        bool throwable = d.category == ItemCategory::throwable;
        if (!(active && !adrift && owner == holder && fighter.state != FighterState::idle && round_active)) {
            fighter.carried = ItemSlot::none; fighter.state = FighterState::idle; fighter.action = FighterAction::idle;
            held_type = knocked_type; throwable = knocked_throwable;
        }
        fighter.action = fighter.animation_step == CombatTuning::animation_cycle_frames ? FighterAction::walk_first : fighter.animation_step;
        if (held_type == ItemType::summoned_weapon) fighter.action += FighterAction::summon_weapon_offset;
        if (throwable && fighter.action < FighterAction::punch_hit) fighter.action += FighterAction::carry_offset;
    }
    if (fighter.state / 20 == FighterStateGroup::consuming) {
        fighter.action = fighter.state % 2 == 0 ? FighterAction::drink_first : FighterAction::drink_second;
        ++fighter.state;
        if (fighter.state == FighterState::consume_last_charge) durability = 1;
        if (variant == 0) {
            if (fighter.health < fighter.max_health) fighter.health = std::min(fighter.max_health, fighter.health + 3);
            if (fighter.energy < fighter.max_energy) fighter.energy = std::min(fighter.max_energy, fighter.energy + 3);
        } else if (fighter.energy < fighter.max_energy) fighter.energy = std::min(fighter.max_energy, fighter.energy + 11);
        if (fighter.state > FighterState::consume_finish) {
            fighter.state = FighterState::idle; fighter.action = FighterAction::idle; fighter.carried = ItemSlot::none; active = false; owner = ItemOwnership::unowned;
            return;
        }
    }
    if (fighter.state / 10 == FighterStateGroup::using_weapon) {
        if (fighter.state == FighterState::weapon_active && fighter.action == FighterAction::item_windup) {
            fighter.action = FighterAction::weapon_idle; fighter.state = FighterState::item_idle_base;
            if (fighter.archetype == FighterType::sword_user && held_type == ItemType::sword && fighter.energy >= first_special_cost) {
                fighter.energy -= first_special_cost; fighter.launched_projectile = ItemType::sword_projectile; fighter.projectile_count = 1;
            }
        } else if (fighter.state == FighterState::weapon_active && fighter.action == FighterAction::bow_use) {
            if (fighter.energy >= first_special_cost) {
                fighter.energy -= first_special_cost; fighter.action = FighterAction::bow_release; fighter.launched_projectile = ItemType::arrow; fighter.projectile_count = 1;
            } else { fighter.state = FighterState::idle; fighter.action = FighterAction::idle; }
        }
        if (fighter.state == FighterState::weapon_finish) fighter.state = FighterState::item_idle_base;
        ++fighter.state;
    }
}

std::optional<ImpactRegion> GroundItem::follow(Combatant& fighter, const ProjectileType& d, bool holding, const std::function<int(int)>& draw) {
    if (owner < 0 || !active) return std::nullopt;
    if (just_picked) { just_picked = false; z += vz; fighter.action = FighterAction::landing; return std::nullopt; }
    std::optional<ImpactRegion> swing;
    const int step = fighter.animation_step == CombatTuning::animation_cycle_frames ? 2 : fighter.animation_step;
    const int act = fighter.action;
    facing = fighter.facing;
    if (d.category == ItemCategory::melee) {
        if (act >= FighterAction::weapon_swing_first && act <= FighterAction::weapon_swing_last) { x = fighter.x + facing * 12; y = fighter.y - 1; z = -1; }
        else if (act == FighterAction::item_windup) { x = fighter.x - facing * 19; y = fighter.y + 1; z = -14; }
        else if (act == FighterAction::weapon_idle) {
            x = fighter.x + facing * 18; y = fighter.y + 1; z = -7;
            const int effect = (d.knockdown == ItemKnockdown::knockdown ? ImpactCode::knockdown : 25) + (d.effect == ImpactCode::normal_kind ? 1000 : 0);
            swing = ImpactRegion{x, y, z, 9, 17, facing, facing * 7, -7, d.damage / 2 + d.effect * ImpactCode::scale, effect};
        } else { x = fighter.x; y = fighter.y + 1; z = step - 11; }
    } else if (d.category == ItemCategory::ranged) {
        if (act > FighterAction::summon_weapon_offset) { x = fighter.x - facing * 7; y = fighter.y + 1; z = -23; }
        else if (act == FighterAction::item_windup) {
            x = fighter.x - facing * (type == ItemType::summoned_weapon ? 12 : 14); y = fighter.y + 1; z = type == ItemType::summoned_weapon ? -20 : -10;
        } else if (act == FighterAction::drink_first || act == FighterAction::drink_second) {
            x = fighter.x - facing * (variant == 1 ? 1 : 4); y = fighter.y - 1;
            z = -(act == FighterAction::drink_first ? (variant == 1 ? 13 : 11) : (variant == 1 ? 12 : 10));
        } else { x = fighter.x - facing * 6; y = fighter.y + 1; z = step - 7; }
    } else if (d.category == ItemCategory::throwable) { x = fighter.x - facing * 2; y = fighter.y + 1; z = -16; }
    vz = 0;
    frame = type == ItemType::summoned_weapon ? 3 - frame : 2;
    if (act == FighterAction::item_windup) facing = -facing;
    if (!holding || !fighter.active || (fighter.state / 50 != FighterStateGroup::item && fighter.state / SpecialMoveId::state_stride != SpecialState::chung_kem)) {
        owner = ItemOwnership::unowned; last_owner = ItemOwnership::detached; settled = false; animation = 1;
        if (type == ItemType::summoned_weapon) active = false;
        frame = 3; vz = -7; vx = draw ? draw(7) - 3 : 0;
        return swing;
    }
    if (act == FighterAction::weapon_idle) frame = 3;
    if (d.category == ItemCategory::melee && act >= FighterAction::weapon_swing_first && act <= FighterAction::weapon_swing_last) {
        frame = 3;
        swing = ImpactRegion{x + facing * 13, y, z, 5, 14, facing, facing * 5, -7, d.damage * 2 / 3 + d.effect * ImpactCode::scale, 1059};
    }
    if (act == FighterAction::drink_first || act == FighterAction::drink_second) frame = 1;
    if (act == FighterAction::bow_use || act == FighterAction::bow_release) frame = 4;
    return swing;
}

void equip_initial_bows(std::span<Combatant> fighters, std::span<const int> controls, std::vector<GroundItem>& items) {
    while (items.size() < 5) { GroundItem empty; empty.active = false; items.push_back(empty); }
    for (int i = 0; i < int(fighters.size()) && i < int(controls.size()); ++i) {
        auto& fighter = fighters[i];
        if (!fighter.active || fighter.archetype != FighterType::bow_user || controls[i] < 0) continue;
        const auto slot = std::find_if(items.begin(), items.end(), [](const auto& item) { return !item.active; });
        if (slot == items.end()) break;
        const int index = int(slot - items.begin());
        *slot = GroundItem{};
        slot->slot = 7 + int(std::count_if(items.begin(), items.end(), [](const auto& item) { return item.active; }));
        slot->type = ItemType::bow; slot->owner = slot->last_owner = i; slot->durability = 500;
        slot->x = fighter.x; slot->y = fighter.y + 1; slot->z = fighter.animation_step - 11; slot->facing = fighter.facing;
        fighter.carried = index; fighter.state = FighterState::holding_item; fighter.action = FighterAction::idle;
    }
}
