// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "grapple.hpp"
#include <algorithm>
#include <cstdlib>

bool Grapple::rush(std::span<Combatant> fighters, int holder, int victim, const ImpactRegion& impact) {
    if (holder < 0 || victim < 0 || holder == victim || holder >= int(fighters.size()) || victim >= int(fighters.size())) return false;
    auto& a = fighters[holder];
    auto& b = fighters[victim];
    if (impact.damage != ImpactCode::grapple || !a.active || !b.active || a.team == b.team || a.priority < b.priority ||
        b.effect_ticks != 0 || b.velocity_z != 0 || b.z != 0 || b.state / 10 == FighterStateGroup::injured || b.state / 10 == FighterStateGroup::angel || b.state / 20 == FighterStateGroup::lifted ||
        std::abs(impact.y - b.y) >= 8 || std::abs(b.x - b.facing * 2 - impact.x) >= impact.width + 10 ||
        std::abs(b.z - 10 - impact.z) >= impact.height + 11) return false;
    a.state = FighterState::clinch_base + victim; a.action = FighterAction::clinch;
    b.state = FighterState::clinched_base + holder; b.action = FighterAction::hit; b.facing = -a.facing;
    a.x = b.x + b.facing * 13; a.y = b.y + 1;
    a.velocity_x = a.velocity_y = 0; b.balance = 1;
    return true;
}

bool Grapple::clinch(std::span<Combatant> fighters, int holder, int victim, bool left_held, const ImpactRegion& marker) {
    if (holder < 0 || victim < 0 || holder == victim || holder >= int(fighters.size()) || victim >= int(fighters.size())) return false;
    auto& a = fighters[holder]; auto& b = fighters[victim];
    const char direction = a.history.back();
    if (!a.active || !b.active || b.priority < a.priority || a.team == b.team || a.z != 0 || b.velocity_z != 0 ||
        a.state / 10 == FighterStateGroup::injured || a.state / 10 == FighterStateGroup::angel || a.state / 20 == FighterStateGroup::lifted ||
        (!left_held && direction != InputKey::left && direction != InputKey::right) || a.x <= b.x ||
        std::abs(a.y - marker.y) >= 8 || std::abs(a.x - a.facing * 2 - marker.x) >= marker.width + 10 ||
        std::abs(a.z - 10 - marker.z) >= marker.height + 11) return false;
    a.state = FighterState::clinch_base + victim; a.action = FighterAction::clinch;
    b.state = FighterState::clinched_base + holder; b.action = FighterAction::hit; b.facing = -a.facing;
    a.x = b.x + b.facing * 13; a.y = b.y + 1; a.velocity_x = a.velocity_y = 0;
    b.balance = 1;
    return true;
}
bool Grapple::strike(std::span<Combatant> fighters, int holder) {
    if (holder < 0 || holder >= int(fighters.size())) return false;
    auto& a = fighters[holder];
    if (a.state < FighterState::clinch_base || a.state > FighterState::clinch_last || a.action == FighterAction::clinch_strike) return false;
    a.state += FighterState::slot_stride;
    return true;
}
bool Grapple::throw_clinched(std::span<Combatant> fighters, int holder) {
    if (holder < 0 || holder >= int(fighters.size())) return false;
    auto& a = fighters[holder];
    if (a.state / 20 != FighterStateGroup::clinching) return false;
    const int index = a.state - FighterState::clinch_base;
    if (index >= 0 && index < int(fighters.size())) {
        auto& b = fighters[index];
        b.state = FighterState::throw_start; b.x += b.facing * 30; b.z -= 8;
    } else if (index >= 0) a.thrown_object = index;
    a.facing *= -1; a.state = FighterState::recovery; a.action = FighterAction::throwing; a.energy = std::min(a.max_energy, a.energy + 10);
    return true;
}
void Grapple::hold(std::span<Combatant> fighters, int holder) {
    auto& a = fighters[holder];
    if (a.state < FighterState::clinch_base || a.state > FighterState::clinch_strike_last) return;
    if (a.state <= FighterState::clinch_last) a.action = FighterAction::clinch;
    if (a.state > FighterState::clinch_last) { a.state -= FighterState::slot_stride; a.action = FighterAction::clinch_strike; }
    if (a.state <= FighterState::clinch_last) {
        const int victim = a.state - FighterState::clinch_base;
        if (victim >= int(fighters.size()) || fighters[victim].state / 20 != 8) { a.state = FighterState::idle; a.action = FighterAction::idle; }
    }
}

void Grapple::clinched(std::span<Combatant> fighters, int victim) {
    auto& b = fighters[victim];
    b.velocity_x = 0;
    const int holder = b.state - FighterState::clinched_base;
    if (b.state >= FighterState::clinched_base && b.state < FighterState::clinched_last + 1 && holder >= 0 && holder < int(fighters.size()) &&
        fighters[holder].action == FighterAction::clinch_strike) {
        auto& a = fighters[holder];
        b.action = FighterAction::clinch_hit; b.balance += 7; b.health -= 6;
        if (a.invisible_ticks == 0) { a.energy = std::min(a.max_energy, a.energy + 8); a.add_score(8); }
    } else if (holder >= 0 && holder < int(fighters.size()) && fighters[holder].action == FighterAction::clinch && fighters[holder].state - FighterState::clinch_base == victim) {
        b.action = FighterAction::hit;
    } else { b.state = FighterState::idle; b.action = FighterAction::idle; }
    if (b.balance > 40) { b.action = FighterAction::hit_air; b.state = FighterState::falling; b.velocity_x = -b.facing * 6; b.velocity_z = -4; }
    else if (b.balance == 40 && holder >= 0 && holder < int(fighters.size())) {
        auto& a = fighters[holder];
        a.action = FighterAction::hit_air; a.state = FighterState::falling; a.velocity_x = b.facing * 8; a.velocity_z = -4;
        b.action = FighterAction::clinch_break; b.state = FighterState::recovery_start;
    }
}
bool Grapple::lift(std::span<Combatant> fighters, int holder, int victim, const ImpactRegion& marker) {
    if (holder < 0 || victim < 0 || holder == victim || holder >= int(fighters.size()) || victim >= int(fighters.size())) return false;
    auto& a = fighters[holder];
    auto& b = fighters[victim];
    if (!a.active || !b.active || b.priority < a.priority || a.state / 10 == FighterStateGroup::injured || a.state / 10 == FighterStateGroup::angel || a.state / 20 == FighterStateGroup::lifted ||
        a.z != 0 || a.history.back() != InputKey::attack || (a.state != FighterState::punch_active && a.state != FighterState::kick_active && a.state / 50 != FighterStateGroup::item) ||
        std::abs(a.y - marker.y) >= 8 || std::abs(a.x - a.facing * 2 - marker.x) >= marker.width + 10 ||
        std::abs(a.z - 10 - marker.z) >= marker.height + 11) return false;
    a.remember(InputKey::interrupted);
    a.state = FighterState::lift_base + victim; a.action = FighterAction::landing;
    a.carried = ItemSlot::none;
    b.state = FighterState::lifted_base + holder; b.balance = 1;
    return true;
}
void Grapple::carry(std::span<Combatant> fighters, int holder) {
    auto& a = fighters[holder];
    if (a.state < FighterState::lift_base || a.state > FighterState::lift_last) return;
    if (a.action == FighterAction::landing) a.action = FighterAction::lift_start;
    if (a.velocity_x != 0 || a.velocity_y != 0) {
        if (++a.animation_step > CombatTuning::animation_cycle_frames) a.animation_step = 1;
        a.action = a.animation_step + FighterAction::lift_start - 1;
        if (a.action == FighterAction::lifted) a.action = FighterAction::lift;
    }
    const int victim = a.state - FighterState::lift_base;
    if (victim >= int(fighters.size()) || fighters[victim].state / 20 != FighterStateGroup::lifted) { a.state = FighterState::idle; a.action = FighterAction::idle; }
}

void Grapple::lifted(std::span<Combatant> fighters, int victim, const std::function<int(int)>& draw) {
    auto& b = fighters[victim];
    if (b.state >= FighterState::lifted_carry_base) {
        const int holder = b.state - FighterState::lifted_carry_base;
        if (holder < int(fighters.size())) {
            auto& a = fighters[holder];
            b.x = a.x - a.facing * 2; b.facing = a.facing; b.y = a.y + 1;
            b.z = -10; b.velocity_z = 0; b.action = FighterAction::lifted;
            if (b.balance > 40 || a.state - FighterState::lift_base != victim || draw(30) == 0) {
                b.state = FighterState::idle; b.action = FighterAction::airborne; b.velocity_z = -2;
            }
        }
    }
    if (b.state < FighterState::lifted_carry_base && b.state != FighterState::idle) {
        const int holder = b.state - FighterState::lifted_base;
        b.state += FighterState::slot_stride;
        if (holder >= 0 && holder < int(fighters.size())) fighters[holder].action = FighterAction::landing;
    }
}
void Grapple::release_abandoned(std::span<Combatant> fighters) {
    for (std::size_t i = 0; i < fighters.size(); ++i) {
        auto& victim = fighters[i];
        if (!victim.active) continue;
        if (victim.state >= FighterState::clinched_base && victim.state <= FighterState::clinched_last) {
            const int holder = victim.state - FighterState::clinched_base;
            if (holder >= int(fighters.size()) || !fighters[holder].active ||
                fighters[holder].state != FighterState::clinch_base + int(i)) {
                victim.state = FighterState::idle; victim.action = FighterAction::idle;
            }
        } else if (victim.state >= FighterState::lifted_base && victim.state <= FighterState::lifted_last) {
            const int holder = (victim.state - FighterState::lifted_base) % FighterState::slot_stride;
            if (holder >= int(fighters.size()) || !fighters[holder].active ||
                fighters[holder].state != FighterState::lift_base + int(i)) {
                if (holder < int(fighters.size()) && fighters[holder].active) {
                    const auto& carrier = fighters[holder];
                    victim.x = carrier.x - carrier.facing * 2; victim.y = carrier.y + 1;
                    victim.facing = carrier.facing; victim.z = -10;
                }
                victim.state = FighterState::idle; victim.action = FighterAction::airborne; victim.velocity_z = -2;
            }
        }
    }
}
bool Grapple::throw_held(std::span<Combatant> fighters, int holder) {
    if (holder < 0 || holder >= int(fighters.size())) return false;
    auto& a = fighters[holder];
    if (a.state < FighterState::lift_base || a.state > FighterState::lift_last || a.action == FighterAction::landing) return false;
    const int index = a.state - FighterState::lift_base;
    if (index >= int(fighters.size()) || !fighters[index].active) return false;
    auto& b = fighters[index];
    b.facing = a.facing; b.state = FighterState::throw_start; b.x += b.facing * 17; b.z -= 8;
    if (b.team == a.team) { b.state = FighterState::idle; b.action = FighterAction::airborne; b.velocity_z = -5; b.velocity_x = b.facing * 10; }
    a.state = FighterState::recovery; a.action = FighterAction::throwing; a.energy = std::min(a.max_energy, a.energy + 10);
    return true;
}
