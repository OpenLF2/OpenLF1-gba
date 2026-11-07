// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "combat.hpp"
#include "game_state.hpp"
#include "item_types.hpp"
#include <algorithm>
#include <cstdlib>

int Combatant::recovery_phase_ = 0;

void Combatant::advance(bool slow_phase, bool alternating_phase, bool round_active, int difficulty) {
    if (slow_phase) {
        if (archetype == FighterType::regenerating_fighter && alternating_phase && health > 0) {
            recovery_phase_ = (recovery_phase_ + 1) % CombatTuning::regeneration_cycle;
            if (recovery_phase_ < CombatTuning::regeneration_ticks) ++health;
            health = std::min(health, max_health);
        }
        const bool resting = state <= FighterState::idle_last && velocity_x == 0 && velocity_y == 0 && velocity_z == 0 && round_active;
        if (resting && (health < CombatTuning::rest_health || energy < CombatTuning::rest_energy)) {
            if (++animation_step >= CombatTuning::rest_animation_frames) {
                animation_step = 1;
                if (action < FighterAction::rest || action > FighterAction::rest_alt) action = FighterAction::rest;
                action = FighterAction::rest + FighterAction::rest_alt - action;
                if (health < CombatTuning::rest_health) {
                    ++health;
                    if (health >= CombatTuning::rest_health) action = FighterAction::idle;
                } else {
                    energy += CombatTuning::rest_energy_gain;
                    if (energy >= CombatTuning::rest_energy) action = FighterAction::idle;
                }
            }
        }
    }
    if (health >= CombatTuning::rest_health && energy >= CombatTuning::rest_energy && (action == FighterAction::rest || action == FighterAction::rest_alt)) action = FighterAction::idle;
    if (alternating_phase && (difficulty != GameLimits::max_difficulty || !computer)) ++energy;
    energy = std::min(energy, max_energy);
    if (effect_ticks > 0) --effect_ticks;
    if (invisible_ticks > 0) --invisible_ticks;
    if (balance < max_balance) ++balance;
}

void Combatant::steer(bool left, bool right, bool up, bool down) {
    const bool movable = state <= FighterState::free_last || state == FighterState::jump_start || state == FighterState::jump_launch || state / 10 == FighterStateGroup::lifting || state == FighterState::holding_item || action == FighterAction::forward_roll || action == FighterAction::backward_roll;
    if (!movable) return;
    if (left) { facing = -1; if (z == 0 && velocity_x > -5) velocity_x = -5; }
    if (right) { facing = 1; if (z == 0 && velocity_x < 5) velocity_x = 5; }
    if (up && z == 0 && velocity_y > -3) velocity_y = -3;
    if (down && z == 0 && velocity_y < 3) velocity_y = 3;
}

bool Combatant::request_jump() {
    if (health > 0 && z < 0 && z + velocity_z >= 0) {
        if (action == FighterAction::roll) {
            action = FighterAction::backward_roll; state = FighterState::recovery; velocity_z = -11;
            facing = velocity_x > 0 ? -1 : 1;
            velocity_x = facing * -7; z = 0;
            return true;
        }
        if (state / 10 == FighterStateGroup::thrown || state / 10 == FighterStateGroup::injured) {
            action = FighterAction::roll; state = FighterState::recovery; velocity_z = -2;
            facing = velocity_x > 0 ? -1 : 1;
            velocity_x = facing * -5; health += 5; z = -1;
            return true;
        }
    }
    if ((state > FighterState::free_last && state / 10 != FighterStateGroup::running && action != FighterAction::landing) || z != 0 || energy < 10) return false;
    energy -= 10;
    state = FighterState::jump_start;
    if (action == FighterAction::landing && energy >= 10) {
        energy -= 10;
        velocity_z = -11;
        if (velocity_x > 0 && velocity_x <= 9) velocity_x = 9;
        if (velocity_x < 0 && velocity_x >= -9) velocity_x = -9;
        action = FighterAction::forward_roll; state = FighterState::recovery; velocity_y = 0;
    }
    return true;
}

bool Combatant::dash_ready(bool round_active) {
    const char key = history.back();
    if (!round_active || (key != InputKey::left && key != InputKey::right) || history[2] != key || history[3] != key || energy <= 1) return false;
    if (!computer) --energy;
    return true;
}

void Combatant::move(int camera, int upper, int lower, bool bridge, int winner, int landing_variant, int revival_velocity) {
    if (state / 10 == FighterStateGroup::angel) {
        velocity_x = 0;
        z -= 3;
        if (++state == FighterState::angel_wrap) state = FighterState::angel;
        action = state - FighterAction::angel_state_offset;
        if (action == FighterAction::angel_alt) action = FighterAction::angel;
        if (z < -130) active = false;
    }
    if (action == FighterAction::forward_roll || action == FighterAction::backward_roll) {
        action = FighterAction::airborne;
        if ((velocity_x > 3 && facing == 1) || (velocity_x < -3 && facing == -1)) action = FighterAction::forward_roll;
        if ((velocity_x > 3 && facing == -1) || (velocity_x < -3 && facing == 1)) action = FighterAction::backward_roll;
    }
    const bool was_running = dispatch_state / 10 == FighterStateGroup::running;
    if (dispatch_state <= FighterState::idle_last && state <= FighterState::idle_last && dash_ready(winner < 0) && z == 0) state = FighterState::run_start;
    if (was_running) {
        if (dash_ready(winner < 0)) facing = history.back() == InputKey::left ? -1 : 1;
        velocity_x = facing * 8;
        if (++state == FighterState::run_wrap) state = FighterState::run_start;
        action = state - FighterAction::run_state_offset;
        if (action == FighterAction::forward_roll) action = FighterAction::run;
        if (!dash_ready(winner < 0) || z != 0) state = FighterState::idle;
    }
    if (state / 10 == FighterStateGroup::recovering && ++state >= FighterState::recovery_end) { state = FighterState::idle; action = FighterAction::idle; }
    if (state / 50 == FighterStateGroup::levitating) {
        ++state;
        velocity_x = velocity_y = 0;
        if (z > -50) velocity_z = -4;
        else if (z > -70) velocity_z -= 2;
        else velocity_z += 2;
        if (velocity_z % 3 == 0) ++velocity_z;
        if (state >= FighterState::levitate_end) { state = FighterState::thrown; health -= 30; }
    }
    if (state / 50 == FighterStateGroup::frozen) {
        --state;
        action = FighterAction::frozen;
        if (state <= FighterState::frozen_end) { action = FighterAction::hit_air; state = FighterState::falling; velocity_x = facing * -3; velocity_z = -4; }
    }
    if (state == FighterState::jump_launch) { velocity_z = -13; state = FighterState::idle; }
    if (state == FighterState::jump_start) { action = FighterAction::landing; ++state; }
    if (state == FighterState::landing) { state = FighterState::idle; action = FighterAction::idle; }
    x += velocity_x;
    y = std::clamp(y + velocity_y, upper, lower);
    z += velocity_z;
    x = std::clamp(x, camera - 190, camera + 510);
    if (bridge) x = std::clamp(x, camera - 22 - y, camera + y + 342);
    if (z > 0) {
        velocity_z = 0;
        z = 0;
        if (state / 50 != FighterStateGroup::frozen) action = FighterAction::landing;
        if (state / 10 != FighterStateGroup::injured && state / 10 != FighterStateGroup::thrown && state / 50 != FighterStateGroup::frozen) state = FighterState::landing;
    }
    if (state / 10 == FighterStateGroup::thrown) {
        const int direction = state == FighterState::thrown_bounce ? -facing : facing;
        if (action == FighterAction::landing && state == FighterState::thrown_bounce) { action = FighterAction::down; state = FighterState::downed_first; }
        if ((action == FighterAction::landing || x == 293 || x == 27) && state == FighterState::thrown) {
            state = FighterState::thrown_bounce; facing *= -1; velocity_z = -5; action = FighterAction::bounce; velocity_x /= 3; health -= 12;
        }
        if (state > FighterState::throw_start && velocity_x != 0)
            body_impact = ImpactRegion{x, y, z - 12, 12, 12, direction, direction * 5, -7, 10, 500};
        if (state == FighterState::throw_start) { velocity_x = facing * 16; velocity_z = -5; action = FighterAction::hit_air; state = FighterState::thrown; }
        if (velocity_z > 0 && state == FighterState::thrown) action = FighterAction::hit_fall;
    }
    if (state / 10 == FighterStateGroup::injured) {
        if (state > FighterState::falling) {
            if (action == FighterAction::down && health <= 0) action = FighterAction::dead;
            ++state;
            if (state == FighterState::downed_end) {
                state = FighterState::landing;
                effect_ticks = 6;
                if (health <= 0) { eliminated = true; just_eliminated = true; death_pose = action; state = FighterState::angel; action = FighterAction::death_pose; effect_ticks = 0; }
                else if (archetype == FighterType::cloning_fighter && health == max_health) {
                    velocity_x = revival_velocity; velocity_z = -20; z = -1;
                    state = FighterState::idle; action = FighterAction::airborne; effect_ticks = 0;
                } else action = FighterAction::landing;
                if (health > 0) fall_effect = 0;
            }
        }
        if (action == FighterAction::landing && state != FighterState::landing) {
            action = health <= 0 ? FighterAction::dead : FighterAction::down;
            state = FighterState::downed_first + (landing_variant & 1);
        }
        if (state == FighterState::falling && action == FighterAction::hit_air && velocity_z > 2) action = FighterAction::hit_fall;
        if (state == FighterState::falling && fall_effect == ImpactCode::fire_kind) action = FighterAction::burning;
    }
    if (balance <= 0) { state = FighterState::staggered; balance = 0; }
    const bool was_staggering = state / 10 == FighterStateGroup::staggering;
    if (was_staggering) {
        if (state > FighterState::hit_reaction) {
            action = FighterAction::hit;
            if (balance >= CombatTuning::balance_recovery) { state = FighterState::idle; action = FighterAction::idle; }
        }
        if (state == FighterState::hit_reaction) {
            if (action == FighterAction::hit) { action = FighterAction::idle; state = FighterState::idle; }
            else action = FighterAction::hit;
        }
    }
    if (!was_staggering && state < FighterState::punch_start) {
        if (action == FighterAction::landing) action = FighterAction::idle;
        if (velocity_x != 0 || velocity_y != 0) {
            if (++animation_step > CombatTuning::animation_cycle_frames) animation_step = 1;
            action = animation_step == CombatTuning::animation_cycle_frames ? FighterAction::walk_first : animation_step;
        }
        if (velocity_x == 0 && team == winner) {
            if (health >= CombatTuning::rest_health) {
                if (action < FighterAction::victory) action = FighterAction::victory;
                action = FighterAction::victory + FighterAction::victory_alt - action;
                if (action == FighterAction::victory) facing *= -1;
            } else if (++animation_step >= CombatTuning::rest_animation_frames) {
                animation_step = 1;
                if (action < FighterAction::rest || action > FighterAction::rest_alt) action = FighterAction::rest;
                action = FighterAction::rest + FighterAction::rest_alt - action;
            }
        }
        if (z < 0) action = FighterAction::airborne;
    }
    auto drag = [&](int& velocity) {
        if (velocity == 1 || velocity == -1) velocity = 0;
        else if (z == 0 && action != FighterAction::landing) velocity = static_cast<int>(velocity / 1.2);
    };
    drag(velocity_x);
    drag(velocity_y);
    if (velocity_z != 0 && z < 0) velocity_z += 3;
}

bool Combatant::can_strike() const {
    return state <= FighterState::free_last || state == FighterState::punch_retract || state == FighterState::punch_finish || state == FighterState::kick_retract || state == FighterState::kick_finish || state / 10 == FighterStateGroup::running;
}

bool Combatant::request_strike(bool kick) {
    if (!can_strike()) return false;
    state = kick ? FighterState::kick_start : FighterState::punch_start;
    if (state == FighterState::punch_start && z < 0) state = FighterState::kick_start;
    return true;
}

std::optional<ImpactRegion> Combatant::strike_frame(bool alternate, bool running) {
    const int kind = state / 10;
    if (kind != 1 && kind != 2) return std::nullopt;
    const bool kick = kind == 2;
    const int start = kick ? FighterState::kick_start : FighterState::punch_start;
    const int active = start + 1;
    const int retract = start + 3;
    const int finish = start + 4;
    int width = kick && z < 0 ? 7 : 5;
    int height = kick ? (z < 0 && velocity_z > 0 ? 12 : 6) : 12;
    int effect = kick ? (z < 0 ? 35 : 25) : 25;
    if (z == 0 && !running) velocity_x /= 4;
    if (action == FighterAction::landing) { action = FighterAction::idle; state = FighterState::reset; }
    if (state == start) action = kick ? (alternate ? FighterAction::kick_windup_alt : FighterAction::kick_windup) : (alternate ? FighterAction::punch_windup_alt : FighterAction::punch_windup);
    if (state == active) ++action;
    if (state == retract) --action;
    const int encoded_damage = (state > active ? 893 : 0) + 7;
    if (kick && state == finish) { state = FighterState::reset; action = FighterAction::idle; }
    std::optional<ImpactRegion> impact;
    if ((kick && (action == FighterAction::kick_hit || action == FighterAction::kick_hit_alt)) || (!kick && (action == FighterAction::punch_hit || action == FighterAction::punch_hit_alt))) {
        impact = ImpactRegion{x + facing * 8, y, z - 4, width, height, facing, facing * 5, -7, encoded_damage, effect};
        if (velocity_x == 0) velocity_x = facing * 2;
    }
    if (!kick && state == finish) { state = FighterState::reset; action = FighterAction::idle; }
    ++state;
    return impact;
}

void Combatant::add_score(int amount) {
    // The DOS game stores scores in 16-bit signed integers.
    const auto value = (static_cast<std::uint32_t>(score) + static_cast<std::uint32_t>(amount)) & 0xffffu;
    score = value < 0x8000u ? int(value) : int(value) - 0x10000;
}

bool Combatant::receive(const ImpactRegion& impact, Combatant& source, const std::function<int(int)>& draw, int score_multiplier, ImpactOrigin origin, int depth_tolerance, std::array<int, 2>* hit_offsets, const std::function<void()>& on_impact) {
    const bool unowned = origin == ImpactOrigin::unowned_item;
    const bool thrown_target = state == FighterState::thrown || state == FighterState::thrown_bounce;
    const bool friendly = !unowned && origin != ImpactOrigin::friendly_item && origin != ImpactOrigin::explosion && source.team == team &&
        (origin == ImpactOrigin::fighter || origin == ImpactOrigin::blast ? source.state != FighterState::thrown && source.state != FighterState::thrown_bounce : !thrown_target);
    if (!active || (origin == ImpactOrigin::fighter && (!source.active || &source == this)) || friendly ||
        (origin == ImpactOrigin::fighter || origin == ImpactOrigin::blast ? source.priority : 0) < priority || effect_ticks != 0 ||
        state / 10 == FighterStateGroup::downed || (state / 10 == FighterStateGroup::injured && (state != FighterState::falling || origin == ImpactOrigin::fighter || origin == ImpactOrigin::explosion || origin == ImpactOrigin::blast)) ||
        state / 10 == FighterStateGroup::angel || state / 20 == FighterStateGroup::lifted || (impact.damage <= 0 && impact.damage != ImpactCode::levitate) || impact.damage == ImpactCode::non_damaging ||
        std::abs(impact.y - y) >= (impact.damage == ImpactCode::levitate ? 30 : depth_tolerance) || std::abs(x - facing * 2 - impact.x) >= impact.width + 10 ||
        std::abs(z - 10 - impact.z) >= impact.height + 11) return false;
    if (impact.damage == ImpactCode::levitate) {
        source.add_score(score_multiplier * 8);
        state = FighterState::levitate_start; action = FighterAction::lifted;
        return true;
    }
    const int damage = impact.damage % ImpactCode::scale;
    fall_effect = impact.effect / ImpactCode::scale;
    const int damage_kind = impact.damage / ImpactCode::scale;
    if (damage_kind == ImpactCode::normal_kind || damage_kind == ImpactCode::explosion_kind || damage_kind == ImpactCode::piercing_kind) {
        const std::array<int, 2> offsets{draw(3) - 1, draw(5)};
        if (hit_offsets) *hit_offsets = offsets;
    }
    if (damage_kind == ImpactCode::normal_kind || damage_kind == ImpactCode::explosion_kind || damage_kind == ImpactCode::piercing_kind || damage_kind == ImpactCode::blunt_kind)
        velocity_x = impact.push_x;
    health = std::max(0, health - damage);
    carried = ItemSlot::knocked;
    if (state / 50 == FighterStateGroup::frozen) state = std::max(FighterState::frozen_end, state - damage);
    if (!unowned && origin != ImpactOrigin::explosion && ((origin != ImpactOrigin::fighter && origin != ImpactOrigin::blast) || (source.state / 10 != FighterStateGroup::thrown && source.invisible_ticks == 0))) {
        source.add_score(damage * score_multiplier);
        source.energy = std::min(source.energy + damage * score_multiplier, source.max_energy);
    }
    if (on_impact) on_impact();
    remember(InputKey::interrupted);
    balance -= impact.effect % ImpactCode::scale;
    if (impact.effect % ImpactCode::scale == ImpactCode::recoil && origin == ImpactOrigin::fighter) source.velocity_x = -impact.push_x;
    if (impact.effect % ImpactCode::scale == ImpactCode::knockdown || velocity_z != 0 || z < 0 || state >= FighterState::staggered && state < FighterState::falling) {
        balance = max_balance;
        if (state / 50 != FighterStateGroup::frozen || state == FighterState::frozen_end) {
            action = impact.direction == facing ? FighterAction::fall_forward : FighterAction::hit_air;
            velocity_x = impact.push_x;
            velocity_z = impact.push_z;
            if (impact.damage / ImpactCode::scale == ImpactCode::freeze_kind && state != FighterState::frozen_end) { state = FighterState::frozen_start; action = FighterAction::frozen; }
            else state = FighterState::falling;
        } else {
            velocity_x = impact.push_x / 3;
            velocity_z = -4;
        }
        if (impact.direction == 2) {
            z -= 6;
            if (x > impact.x) { velocity_x = impact.push_x; facing = -1; }
            else { velocity_x = -impact.push_x; facing = 1; }
            if (y > impact.y + 3) velocity_y = 3;
            else if (y < impact.y - 3) velocity_y = -3;
        }
    } else if (state / 50 != FighterStateGroup::frozen) {
        state = FighterState::hit_reaction;
        int reaction;
        do { reaction = FighterAction::hit_first + draw(FighterAction::hit_variants); } while (reaction == action);
        action = reaction;
    } else {
        velocity_x = impact.direction * 2;
    }
    return true;
}

void Combatant::remember(char key) {
    std::move(history.begin() + 1, history.end(), history.begin());
    history.back() = key;
}

bool Combatant::activate_special(const SpecialMove& move) {
    const int kind = move.frames[4];
    const bool implemented = (kind >= SpecialMoveId::ball_kat && kind <= SpecialMoveId::kung_bow) || kind == SpecialMoveId::lin_keung || kind == SpecialMoveId::make_sword_2;
    if (move.cost > energy || !implemented) return false;
    state = (kind + SpecialMoveId::state_offset) * SpecialMoveId::state_stride;
    special_frames = move.frames;
    energy -= move.cost;
    remember(InputKey::interrupted);
    return true;
}

bool Combatant::try_sword(bool holding_melee) {
    const bool eligible = state / 10 == FighterStateGroup::idle || state / 10 == FighterStateGroup::running || state / 10 == FighterStateGroup::kicking || state / 10 == FighterStateGroup::punching || state / 20 == FighterStateGroup::clinching || state / 10 == FighterStateGroup::lifting || state / 50 == FighterStateGroup::item;
    if (!eligible || history[4] != InputKey::attack) return false;
    if (archetype == FighterType::sword_user && z == 0 && energy > 50 && !(holding_melee && state / 50 == FighterStateGroup::item) &&
        ((history[1] == InputKey::right && history[2] == InputKey::left && history[3] == InputKey::right) ||
         (history[1] == InputKey::left && history[2] == InputKey::right && history[3] == InputKey::left))) {
        std::fill(history.begin() + 1, history.end(), ' ');
        energy -= 50;
        state = special_state(SpecialState::make_sword_2, 0);
        return true;
    }
    return false;
}

bool Combatant::try_special(std::span<const SpecialMove> moves, bool holding_melee) {
    const bool eligible = state / 10 == FighterStateGroup::idle || state / 10 == FighterStateGroup::running || state / 10 == FighterStateGroup::kicking || state / 10 == FighterStateGroup::punching || state / 20 == FighterStateGroup::clinching || state / 10 == FighterStateGroup::lifting || state / 50 == FighterStateGroup::item;
    if (!eligible || history[4] != InputKey::attack) return false;
    const char forward = facing == 1 ? InputKey::right : InputKey::left;
    const char back = facing == 1 ? InputKey::left : InputKey::right;
    for (const auto& move : moves) {
        if (move.cost > energy) continue;
        const bool a = move.form == 'A' && z == 0 && history[1] == back && history[2] == forward && history[3] == forward;
        const bool b = move.form == 'B' && z == 0 && history[1] == forward && history[2] == InputKey::down && history[3] == InputKey::up;
        const bool c = move.form == 'C' && z == 0 && history[1] == forward && history[2] == back && history[3] == forward;
        const bool d = move.form == 'D' && z < -20 && history[2] == forward && history[3] == forward;
        const bool e = move.form == 'E' && z == 0 && history[0] == InputKey::up && history[1] == InputKey::down && history[2] == back && history[3] == forward;
        const bool h = move.form == 'H' && z == 0 && carried >= 0 && holding_melee && history[1] == forward && history[2] == back && history[3] == forward;
        if (!(a || b || c || d || e || h)) continue;
        if (activate_special(move)) return true;
    }
    return false;
}

bool Combatant::teleport(std::span<const Combatant> targets, const std::function<int(int)>& draw, bool include_defeated) {
    const Combatant* nearest = nullptr;
    for (const auto& target : targets) {
        if (&target == this || !target.active || target.just_eliminated || target.team == team || (!include_defeated && target.state / 10 == FighterStateGroup::angel)) continue;
        if (!nearest || std::abs(target.x - x) + std::abs(target.y - y) < std::abs(nearest->x - x) + std::abs(nearest->y - y)) nearest = &target;
    }
    if (!nearest) return false;
    const int backward = draw(20);
    const int lateral = draw(21);
    facing = nearest->facing;
    x = nearest->x - (9 + backward) * facing;
    y = nearest->y + lateral - 10;
    return true;
}

std::optional<ImpactRegion> Combatant::special_frame(bool alternate_phase) {
    const int kind = state / SpecialMoveId::state_stride;
    if (kind == SpecialState::make_sword_2) {
        ++state;
        velocity_x = velocity_y = velocity_z = 0;
        action = FighterAction::make_sword_first + (state - special_state(SpecialState::make_sword_2, 1)) * FighterAction::make_sword_stride;
        if (state >= special_state(SpecialState::make_sword_2, 3)) { item_pending = ItemType::sword; state = FighterState::holding_item; action = FighterAction::idle; }
        return std::nullopt;
    }
    if (kind == SpecialState::pow_pick) {
        ++state;
        priority = AttackPriority::grapple;
        velocity_x = facing * 12;
        velocity_y = velocity_z = 0;
        action = special_frames[state % 2 == 0 ? 0 : 1];
        const ImpactRegion hit{x + facing * 8, y, z - 12, 5, 23, facing, facing * 5, -7, ImpactCode::grapple, ImpactCode::knockdown};
        if (state >= special_state(SpecialState::pow_pick, 4)) action = state = FighterState::idle;
        return hit;
    }
    if (kind == SpecialState::heal_self) {
        ++state;
        velocity_x = velocity_y = velocity_z = 0;
        action = special_frames[state % 2 == 0 ? 0 : 1];
        if (health < max_health) health = std::min(max_health, health + (alternate_phase ? 3 : 2));
        if (state >= special_state(SpecialState::heal_self, 15)) action = state = FighterState::idle;
        return std::nullopt;
    }
    if (kind == SpecialState::ball_kat && archetype != FighterType::cloning_fighter && archetype != FighterType::regenerating_fighter) {
        ++state;
        velocity_x = velocity_y = velocity_z = 0;
        if (state == special_state(SpecialState::ball_kat, 1)) action = special_frames[0];
        if (state >= special_state(SpecialState::ball_kat, 2)) action = special_frames[1];
        if (state == special_state(SpecialState::ball_kat, 2)) { launched_projectile = special_frames[2]; projectile_count = 1; }
        if (state >= special_state(SpecialState::ball_kat, 4) || (state == special_state(SpecialState::ball_kat, 3) && archetype == FighterType::bow_user)) action = state = FighterState::idle;
        return std::nullopt;
    }
    if (kind == SpecialState::ball_kat) {
        ++state;
        velocity_x = velocity_y = velocity_z = 0;
        action = special_frames[0];
        if (state >= special_state(SpecialState::ball_kat, 3) && state <= special_state(SpecialState::ball_kat, 11) && state % 2 == 1) {
            projectile_count = 0;
            for (int i = 0; i < 3; ++i) {
                if (state > special_state(SpecialState::ball_kat, 3)) {
                    if (energy >= 25) energy -= 25;
                    else if (archetype != FighterType::regenerating_fighter) { state = special_state(SpecialState::ball_kat, 12); break; }
                }
                ++projectile_count;
            }
            if (projectile_count) { action = special_frames[1]; launched_projectile = special_frames[2]; burst_launch = true; }
        }
        if ((state == special_state(SpecialState::ball_kat, 12) && archetype == FighterType::cloning_fighter) || state == special_state(SpecialState::ball_kat, 15)) action = state = FighterState::idle;
        return std::nullopt;
    }
    if (kind == SpecialState::sing_long) {
        ++state;
        if (state == special_state(SpecialState::sing_long, 2)) { velocity_x = facing * 7; velocity_y = 0; velocity_z = -11; action = special_frames[0]; }
        if (state == special_state(SpecialState::sing_long, 5)) action = special_frames[1];
        if (state == special_state(SpecialState::sing_long, 7)) action = special_frames[2];
        if (state == special_state(SpecialState::sing_long, 9)) action = special_frames[3];
        if (state < special_state(SpecialState::sing_long, 9)) { priority = AttackPriority::strong; return ImpactRegion{x, y, z - 12, 12, 17, facing, facing * 5, -14, 35, 500}; }
        return std::nullopt;
    }
    if (kind == SpecialState::jong) {
        ++state;
        if (state == special_state(SpecialState::jong, 2)) { velocity_x = facing * 13; velocity_y = velocity_z = 0; action = special_frames[0]; }
        if (state == special_state(SpecialState::jong, 6)) { action = state = FighterState::idle; return std::nullopt; }
        priority = AttackPriority::special;
        return ImpactRegion{x + facing * 8, y, z - 12, 8, 17, facing, facing * 5, -7, 25, 500};
    }
    if (kind == SpecialState::shun_kan) {
        ++state; velocity_x = velocity_y = velocity_z = 0; priority = AttackPriority::evasive;
        if (state == special_state(SpecialState::shun_kan, 2)) action = special_frames[0];
        if (state > special_state(SpecialState::shun_kan, 2)) action = special_frames[1];
        if (state == special_state(SpecialState::shun_kan, 4)) teleport_pending = true;
        if (state == special_state(SpecialState::shun_kan, 5)) action = state = FighterState::idle;
        return std::nullopt;
    }
    if (kind == SpecialState::yan_sun) {
        ++state; velocity_x = velocity_y = velocity_z = 0;
        action = special_frames[state % 2 == 0 ? 0 : 1];
        if (state > special_state(SpecialState::yan_sun, 11)) priority = AttackPriority::evasive;
        if (state == special_state(SpecialState::yan_sun, 12)) action = FighterAction::invisibility_first;
        if (state == special_state(SpecialState::yan_sun, 13)) action = FighterAction::invisibility_second;
        if (state == special_state(SpecialState::yan_sun, 14)) action = FighterAction::invisibility_third;
        if (state == special_state(SpecialState::yan_sun, 15)) { effect_ticks = invisible_ticks = 100; action = state = FighterState::idle; }
        return std::nullopt;
    }
    if (kind == SpecialState::shun_kan_2) {
        ++state; velocity_x = velocity_y = velocity_z = 0; priority = AttackPriority::evasive;
        if (state == special_state(SpecialState::shun_kan_2, 2) || state == special_state(SpecialState::shun_kan_2, 5)) action = special_frames[0];
        if (state == special_state(SpecialState::shun_kan_2, 3) || state == special_state(SpecialState::shun_kan_2, 6)) action = special_frames[1];
        if (state == special_state(SpecialState::shun_kan_2, 4) || state == special_state(SpecialState::shun_kan_2, 7)) action = special_frames[2];
        if (state == special_state(SpecialState::shun_kan_2, 5)) teleport_pending = true;
        if (state == special_state(SpecialState::shun_kan_2, 8)) action = state = FighterState::idle;
        return std::nullopt;
    }
    if (kind == SpecialState::chal) {
        ++state;
        priority = AttackPriority::strong;
        if (state == special_state(SpecialState::chal, 2)) { velocity_y = 0; velocity_z = -10; action = special_frames[0]; }
        if (state == special_state(SpecialState::chal, 4)) { velocity_x = facing * 13; action = special_frames[1]; }
        if (state == special_state(SpecialState::chal, 9)) velocity_x /= 3;
        if (state >= special_state(SpecialState::chal, 4)) return ImpactRegion{x + facing * 8, y, z - 12, 8, 17, facing, facing * 18, -10, 30, 500};
        return std::nullopt;
    }
    if (kind == SpecialState::hung_ball) {
        ++state;
        if (state == special_state(SpecialState::hung_ball, 2)) { velocity_x = facing * -2; velocity_y = 0; velocity_z = -2; action = special_frames[0]; }
        if (state == special_state(SpecialState::hung_ball, 5)) { velocity_x = facing * 13; velocity_y = 0; velocity_z = 8; action = special_frames[1]; }
        if (state >= special_state(SpecialState::hung_ball, 5)) { priority = AttackPriority::special; return ImpactRegion{x + facing * 12, y, z, 12, 17, facing, facing * 5, -7, 20, 500}; }
        return std::nullopt;
    }
    if (kind == SpecialState::chung_kem) {
        ++state; velocity_y = velocity_z = 0; velocity_x = facing * 8; priority = AttackPriority::special;
        constexpr int frame_order[3] = {0, 2, 1};
        action = special_frames[frame_order[state % 3]];
        if (state >= special_state(SpecialState::chung_kem, 10)) { action = FighterAction::idle; state = FighterState::holding_item; velocity_x /= 2; }
        return std::nullopt;
    }
    if (kind == SpecialState::hei_sheun) {
        item_pending = ItemType::summoned_weapon; state = FighterState::holding_item; action = FighterAction::summon_weapon;
        return std::nullopt;
    }
    if (kind == SpecialState::make_sword) {
        ++state; velocity_x = velocity_y = velocity_z = 0;
        action = special_frames[state % 2 == 0 ? 0 : 1];
        if (state >= special_state(SpecialState::make_sword, 5)) { item_pending = ItemType::summoned_sword; state = FighterState::holding_item; action = FighterAction::idle; }
        return std::nullopt;
    }
    if (kind == SpecialState::lin_keung) {
        ++state; action = special_frames[state % 2];
        velocity_x = facing * 9; priority = AttackPriority::special;
        const ImpactRegion hit{x + facing * 8, y, z - 12, 8, 7, facing, velocity_x, -7, 5014, 500};
        if (state == special_state(SpecialState::lin_keung, 7)) action = state = FighterState::idle;
        return hit;
    }
    if (kind == SpecialState::chi_bow) {
        ++state; velocity_x = velocity_y = velocity_z = 0;
        if (state == special_state(SpecialState::chi_bow, 2)) action = special_frames[0];
        if (state == special_state(SpecialState::chi_bow, 5)) { action = special_frames[1]; blast_pending = BlastPattern::forward; }
        if (state >= special_state(SpecialState::chi_bow, 10)) action = state = FighterState::idle;
        return std::nullopt;
    }
    if (kind == SpecialState::bow_four) {
        ++state; velocity_x = velocity_y = velocity_z = 0; priority = AttackPriority::blast;
        if (state == special_state(SpecialState::bow_four, 2)) action = special_frames[0];
        if (state == special_state(SpecialState::bow_four, 5)) {
            x -= facing * 2; action = special_frames[1];
            blast_pending = archetype == FighterType::regenerating_fighter ? BlastPattern::spread : BlastPattern::centered;
        }
        if (state >= special_state(SpecialState::bow_four, 10)) action = state = FighterState::idle;
        return std::nullopt;
    }
    if (kind == SpecialState::pow_four) {
        ++state;
        velocity_y = velocity_z = 0; velocity_x = facing * 8; priority = AttackPriority::special;
        constexpr int frame_order[4] = {0, 1, 2, 1};
        action = special_frames[frame_order[(4 - state % 4) % 4]];
        const ImpactRegion hit{x, y, z - 12, 10, 7, 2, 7, -7, 30, 2500};
        if (state >= special_state(SpecialState::pow_four, 15)) { action = state = FighterState::idle; velocity_x /= 2; }
        return hit;
    }
    if (kind == SpecialState::cheun_sun) {
        ++state;
        velocity_x = facing * 5; velocity_y = velocity_z = 0; priority = AttackPriority::special;
        constexpr int frame_order[4] = {2, 1, 0, 3};
        action = special_frames[frame_order[state % 4]];
        const ImpactRegion hit{x, y, z - 12, 8, 7, 2, 7, -7, 6014, 500};
        if (state >= special_state(SpecialState::cheun_sun, 15)) action = state = FighterState::idle;
        return hit;
    }
    if (kind == SpecialState::bow_ball) {
        ++state; velocity_x = velocity_y = velocity_z = 0;
        if (state == special_state(SpecialState::bow_ball, 2)) action = special_frames[0];
        std::optional<ImpactRegion> first;
        if (state == special_state(SpecialState::bow_ball, 4)) {
            priority = AttackPriority::strong; x -= facing * 2; action = special_frames[1];
            first = ImpactRegion{x + 85, y, z, 75, 17, 1, 25, -7, 5022, 500};
            secondary_impact = ImpactRegion{x - 85, y, z, 75, 17, -1, -25, -7, 5022, 500};
        }
        if (state >= special_state(SpecialState::bow_ball, 5)) action = special_frames[2];
        if (state >= special_state(SpecialState::bow_ball, 10)) action = state = FighterState::idle;
        return first;
    }
    if (kind == SpecialState::chui_that) {
        ++state; velocity_x = velocity_y = velocity_z = 0;
        action = special_frames[(state / 2) % 2 == 0 ? 0 : 1];
        std::optional<ImpactRegion> hit;
        if (state > special_state(SpecialState::chui_that, 8)) hit = ImpactRegion{x, y, z, 50, 40, facing, facing * 5, -7, ImpactCode::levitate, ImpactCode::knockdown};
        if (state >= special_state(SpecialState::chui_that, 20)) action = state = FighterState::idle;
        return hit;
    }
    if (kind == SpecialState::foot_1000) {
        ++state;
        if (state == special_state(SpecialState::foot_1000, 2)) { velocity_x = facing * 6; velocity_y = velocity_z = 0; z = -4; }
        action = special_frames[state % 2 == 0 ? 0 : 1];
        std::optional<ImpactRegion> hit;
        const int last_active = special_state(SpecialState::foot_1000, 9);
        if (state <= last_active) { priority = AttackPriority::special; hit = ImpactRegion{x + facing * 8, y, z - 12, 8, 5, facing, facing * 5, -7, state < last_active ? 5005 : 12, state < last_active ? 10 : 500}; }
        if (state > last_active) { action = state = FighterState::idle; velocity_z = -1; }
        return hit;
    }
    if (kind == SpecialState::sheun_fung) {
        ++state;
        velocity_x = facing * 8;
        if (state == special_state(SpecialState::sheun_fung, 2)) { velocity_y = velocity_z = 0; z = -4; }
        priority = AttackPriority::strong;
        constexpr int frames[4] = {2, 3, 0, 1};
        action = special_frames[frames[state % 4]];
        const ImpactRegion hit{x + facing * 8, y, z - 12, 8, 7, facing, velocity_x, -7, 5009, 25};
        if (state >= special_state(SpecialState::sheun_fung, 16)) { action = state = FighterState::idle; velocity_z = -1; }
        return hit;
    }
    if (kind == SpecialState::kung_bow) {
        ++state;
        velocity_x = velocity_y = velocity_z = 0;
        if (state == special_state(SpecialState::kung_bow, 2)) action = special_frames[0];
        std::optional<ImpactRegion> hit;
        if (state == special_state(SpecialState::kung_bow, 4)) {
            priority = AttackPriority::strong; action = special_frames[1];
            hit = ImpactRegion{x + facing * 80, y, z, 80, 22, facing, facing * 20, -7, 5024, 500};
        }
        if (state >= special_state(SpecialState::kung_bow, 8)) action = state = FighterState::idle;
        return hit;
    }
    return std::nullopt;
}
