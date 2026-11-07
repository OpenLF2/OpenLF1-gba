// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "opponent.hpp"
#include "grapple.hpp"
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <limits>

bool Opponent::weapon_attacking(std::span<Combatant> fighters, std::size_t index, int difficulty, const Draw& draw, std::span<const Projectile> projectiles, int upper, int lower) {
    if (index >= fighters.size() || draw(difficulty) > 0) return false;
    auto& self = fighters[index];
    for (const auto& projectile : projectiles) {
        const int owner_team = projectile.owner >= 0 && projectile.owner < int(fighters.size()) ? fighters[projectile.owner].team : 0;
        if (!projectile.active || projectile.exploding || projectile.adrift || projectile.slot < 8 || projectile.slot >= 30 ||
            owner_team == self.team || std::abs(self.y - projectile.y) >= 7 || std::abs(self.x - projectile.x) >= 60) continue;
        if (self.state <= FighterState::free_last) self.velocity_y = self.y > upper + lower ? -3 : 3;
        return true;
    }
    return false;
}

static void apply_opponent_key(std::span<Combatant> fighters, std::size_t index, const Opponent::Draw& draw, std::span<GroundItem> items, std::span<const ProjectileType> types, char key) {
    auto& self = fighters[index];
    if (key == InputKey::attack && self.can_strike()) self.request_strike(draw(2) != 0);
    if (key == InputKey::attack) Grapple::strike(fighters, int(index));
    if (key == InputKey::jump) Grapple::throw_clinched(fighters, int(index));
    if (key == InputKey::jump || key == InputKey::attack) Grapple::throw_held(fighters, int(index));
    if ((key != InputKey::attack && key != InputKey::jump) || self.state / 10 != FighterStateGroup::holding || self.action == FighterAction::landing) return;
    const bool has_item = self.carried >= 0 && self.carried < int(items.size());
    const bool held_reference = ((has_item && !items[self.carried].active) || self.carried == ItemSlot::knocked) && self.held_object_type >= 0;
    if (!has_item && !self.item_pending && !held_reference) return;
    const int type = held_reference ? self.held_object_type : has_item ? items[self.carried].type : *self.item_pending;
    if (type < 0 || type >= int(types.size())) return;
    const int category = types[type].category;
    if (key == InputKey::attack && type == ItemType::drink && category == ItemCategory::ranged) { self.state = FighterState::consume_item; self.action = FighterAction::drink_first; self.velocity_x /= 3; self.remember(InputKey::interrupted); }
    else if (key == InputKey::attack && category == ItemCategory::melee && self.action != FighterAction::bow_release) { self.state = FighterState::use_weapon; self.action = type == ItemType::bow && self.archetype == FighterType::bow_user ? FighterAction::bow_use : FighterAction::item_windup; self.velocity_x /= 3; }
    else if (category == ItemCategory::ranged || (key == InputKey::jump && category == ItemCategory::melee)) { self.state = FighterState::throw_item; self.action = FighterAction::item_windup; self.velocity_x /= 3; }
    else if (category == ItemCategory::throwable && has_item) {
        auto& item = items[self.carried];
        item.owner = ItemOwnership::unowned; item.last_owner = int(index); item.settled = false; item.animation = 1; item.facing = self.facing;
        item.x += self.facing * 12; item.vx = self.facing * 8; item.vz = -4; item.z = -16;
        self.carried = ItemSlot::none; self.state = FighterState::recovery; self.action = FighterAction::throwing; self.velocity_x /= 3;
    }
}

void Opponent::process_event(std::span<Combatant> fighters, std::size_t index, int difficulty, const Draw& draw, std::span<const Projectile> projectiles, int upper, int lower, char key, std::span<GroundItem> items, std::span<const ProjectileType> types) {
    if (index >= fighters.size()) return;
    auto& self = fighters[index];
    const bool threat = weapon_attacking(fighters, index, difficulty, draw, projectiles, upper, lower);
    const bool roll = draw(difficulty) == 0;
    if (threat && roll && self.action == FighterAction::roll && self.z < 0 && self.z + self.velocity_z >= 0 && self.health > 0) self.request_jump();
    if (key == InputKey::jump && self.z == 0) self.request_jump();
    if (draw(difficulty * 5 + 20) == 0 && (self.state / 10 == FighterStateGroup::thrown || self.state / 10 == FighterStateGroup::injured) &&
        self.z < 0 && self.z + self.velocity_z >= 0 && self.health > 0) self.request_jump();
    apply_opponent_key(fighters, index, draw, items, types, key);
}

void Opponent::act(std::span<Combatant> fighters, std::size_t index, int difficulty, const Draw& draw, std::span<GroundItem> items, std::span<const ProjectileType> types, std::span<const SpecialMove> moves, std::span<const MoveRange> ranges, std::span<const Projectile> projectiles, int upper, int lower, std::span<const std::array<SpecialMove, 3>> profiles, bool process_input_event, const std::function<std::span<GroundItem>()>& create_pending_item) {
    if (index >= fighters.size()) return;
    auto& self = fighters[index];
    self.opponent_target = -1;
    if (self.ranged_cooldown > 0) --self.ranged_cooldown;
    if (difficulty == 10 && self.z < 0) {
        if (process_input_event) process_event(fighters, index, difficulty, draw, projectiles, upper, lower);
        return;
    }
    Combatant* target = nullptr;
    int nearest = std::numeric_limits<int>::max();
    for (auto& candidate : fighters) {
        if (&candidate == &self || candidate.team == self.team || !candidate.active || candidate.state / 10 == FighterStateGroup::angel) continue;
        const auto dx = std::abs(candidate.x - self.x);
        if (candidate.state / 10 == FighterStateGroup::injured && dx >= 40) continue;
        const int distance = dx + std::abs(candidate.y - self.y);
        if (distance < nearest) { nearest = distance; target = &candidate; }
    }
    const Combatant* companion = nullptr;
    int companion_distance = std::numeric_limits<int>::max();
    for (const auto& candidate : fighters) {
        if (&candidate == &self || candidate.team != self.team || !candidate.active || candidate.state / 10 == FighterStateGroup::angel) continue;
        const int distance = std::abs(candidate.x - self.x) + std::abs(candidate.y - self.y);
        if (distance < companion_distance) { companion_distance = distance; companion = &candidate; }
    }
    const GroundItem* object = nullptr;
    int object_distance = companion ? companion_distance : target ? nearest : std::abs(self.x) + std::abs(self.y);
    if (self.tactic != OpponentTactic::heal && self.state / 50 != FighterStateGroup::item && (!target || (std::abs(self.y - target->y) > 6 && std::abs(self.x - target->x) > 25))) {
        bool aiming = target != nullptr;
        std::vector<const GroundItem*> by_slot;
        for (const auto& item : items) if (item.active && item.animation == 1 && item.owner < 0) by_slot.push_back(&item);
        std::stable_sort(by_slot.begin(), by_slot.end(), [](const auto* a, const auto* b) { return a->slot < b->slot; });
        for (const auto* item : by_slot) {
            const int distance = std::abs(item->x - self.x) + std::abs(item->y - self.y);
            if (!aiming) { object = item; aiming = true; }
            else if (object_distance > distance) { object_distance = distance; object = item; }
        }
    }
    char requested_key = self.state / 5 == FighterStateGroup::item_idle && self.carried >= 0 && self.carried < int(items.size()) && items[self.carried].type == ItemType::drink ? InputKey::attack : 0;
    const auto apply_key = [&] {
        if (self.item_pending && create_pending_item) items = create_pending_item();
        if (process_input_event) process_event(fighters, index, difficulty, draw, projectiles, upper, lower, requested_key, items, types);
        else {
            if (requested_key == InputKey::jump) self.request_jump();
            apply_opponent_key(fighters, index, draw, items, types, requested_key);
        }
    };
    struct KeyOnExit {
        const decltype(apply_key)& apply;
        ~KeyOnExit() { apply(); }
    } key_on_exit{apply_key};
    const auto resolve_clinch = [&] {
        if (self.state < FighterState::clinch_base || self.state > FighterState::clinch_last || self.action == FighterAction::clinch_strike || draw(4) == 0) return;
        const int opponent = self.state - FighterState::clinch_base;
        self.remember(InputKey::attack);
        requested_key = opponent < int(fighters.size()) && fighters[opponent].balance < 32 ? InputKey::attack : InputKey::jump;
    };
    if (object) {
        resolve_clinch();
        const int speed_x = draw(3) + 3;
        const int speed_y = draw(2) + 2;
        if (self.state / 10 == FighterStateGroup::running) self.history.back() = object->y > self.y ? InputKey::down : InputKey::up;
        if (!(self.state <= FighterState::free_last || self.state == FighterState::jump_start || self.state == FighterState::jump_launch || self.state / 10 == FighterStateGroup::lifting || self.state == FighterState::holding_item)) return;
        if (draw(3) > 0) {
            if (self.x > object->x + 9) self.velocity_x = -speed_x;
            if (self.x < object->x - 9) self.velocity_x = speed_x;
            if (self.y > object->y + 6) self.velocity_y = -speed_y;
            if (self.y < object->y - 6) self.velocity_y = speed_y;
            if (std::abs(self.x - object->x) < 10 && std::abs(self.y - object->y) < 7) {
                self.remember(InputKey::attack); requested_key = InputKey::attack;
            }
        }
        return;
    }
    if (!target) return;
    self.opponent_target = int(target - fighters.data());
    struct ClinchOnExit {
        const decltype(resolve_clinch)& resolve;
        ~ClinchOnExit() { resolve(); }
    } clinch_on_exit{resolve_clinch};
    const auto held_type = [&] { return self.held_object_type >= 0 ? self.held_object_type : items[self.carried].type; };
    const auto select_special = [&] {
        if (moves.empty() || ranges.empty()) return;
        bool activated = false;
        int highest = -1;
        for (int i = 0; i < int(moves.size()); ++i) if (moves[i].cost <= self.energy) highest = i;
        for (int i = highest; i >= 0; --i) {
            if (moves[i].frames[4] == SpecialMoveId::pow_pick && draw(4) > 0 && --i < 0) break;
            const auto& move = moves[i];
            const int kind = move.frames[4];
            if (kind <= 0 || kind >= int(ranges.size()) || move.form > 'H') continue;
            if (i != highest && self.energy < 225 && self.energy - move.cost < moves[i + 1].cost) continue;
            if (activated) continue;
            const auto& range = ranges[kind];
            const bool out_of_range = range.center < 0 || (kind == SpecialMoveId::ball_kat && self.ranged_cooldown > 0) ||
                range.width + difficulty < std::abs(target->x - self.x - self.facing * range.center) || range.depth + difficulty < std::abs(target->y - self.y);
            if (out_of_range && (kind != SpecialMoveId::lin_keung || !weapon_attacking(fighters, index, difficulty, draw, projectiles, upper, lower))) {
                if (range.center < 0) self.tactic = -range.center;
                continue;
            }
            if (self.state / 10 == FighterStateGroup::running) {
                requested_key = target->y > self.y ? InputKey::down : InputKey::up;
                self.history.back() = requested_key;
            }
            if (self.state / 50 == FighterStateGroup::item && self.carried >= 0 && self.carried < int(items.size())) {
                const int type = held_type();
                if (type >= 0 && type < int(types.size()) && types[type].category < ItemCategory::throwable &&
                    !(self.archetype == FighterType::bow_user && type == ItemType::bow) && !(self.archetype == FighterType::sword_user && type == ItemType::sword) &&
                    (kind == SpecialMoveId::ball_kat || kind == SpecialMoveId::chi_bow || kind == SpecialMoveId::chung_kem || kind == SpecialMoveId::pow_four || kind == SpecialMoveId::hei_sheun || kind == SpecialMoveId::cheun_sun || kind == SpecialMoveId::kung_bow)) {
                    requested_key = InputKey::jump; return;
                }
            }
            if (move.form == 'F' || move.form == 'G' || move.form == 'H') {
                const bool existing_item = self.carried >= 0 && self.carried < int(items.size());
                const bool holding = self.state / 50 == FighterStateGroup::item;
                const int type = holding ? (existing_item ? held_type() : self.item_pending ? *self.item_pending : -1) : (self.archetype == FighterType::bow_user ? 7 : 0);
                self.facing = self.x > target->x ? -1 : 1;
                if (type < 0 || type >= int(types.size())) continue;
                if (!holding) {
                    self.item_pending = type;
                    self.state = FighterState::holding_item;
                    self.action = FighterAction::idle;
                }
                if ((move.form == 'F' && type != ItemType::bow) || (move.form == 'G' && type != ItemType::sword) ||
                    (move.form == 'H' && types[type].category != ItemCategory::melee)) continue;
                if (move.form == 'H') {
                    if (self.activate_special(move)) activated = true;
                } else {
                    self.remember(InputKey::attack); requested_key = InputKey::attack;
                }
                continue;
            }
            self.facing = self.x > target->x ? -1 : 1;
            if (kind == SpecialMoveId::ball_kat && self.archetype == FighterType::restricted_ball_user && self.health <= self.health / 8) continue;
            if (self.activate_special(move)) activated = true;
        }
        if (activated) return;
        if (self.archetype == FighterType::cloning_fighter && std::abs(self.x - target->x) > 100 && self.energy > 200 && draw(5) > 0)
            self.tactic = OpponentTactic::invisibility;
    };
    bool afraid = false;
    if (difficulty == 0 && target->effect_ticks == 0 && target->state / 50 != FighterStateGroup::levitating &&
        target->archetype > 0 && target->archetype <= int(profiles.size())) {
        int own_strength = 0;
        for (const auto& move : moves) {
            const int kind = move.frames[4];
            // The original indexes the character table by cost here; only fighter slots are represented in this span.
            if (move.cost >= 0 && move.cost < int(fighters.size()) && fighters[move.cost].energy >= move.cost &&
                kind >= 0 && kind < int(ranges.size()))
                own_strength = std::max(own_strength, ranges[kind].strength);
        }
        const auto& enemy_moves = profiles[target->archetype - 1];
        int highest = -1;
        for (int i = 0; i < int(enemy_moves.size()); ++i) if (enemy_moves[i].cost <= target->energy) highest = i;
        for (int i = highest; i >= 0; --i) {
            if (enemy_moves[i].frames[4] == SpecialMoveId::pow_pick && draw(4) > 0 && --i < 0) break;
            const int kind = enemy_moves[i].frames[4];
            if (kind <= 0 || kind >= int(ranges.size())) continue;
            const auto& range = ranges[kind];
            if (range.center >= 0 && kind != SpecialMoveId::ball_kat && range.strength > own_strength && target->state / 50 != FighterStateGroup::frozen &&
                std::abs(self.x - target->x - target->facing * range.center) <= range.width + 7 &&
                std::abs(self.y - target->y) <= range.depth + 10) afraid = true;
        }
    }
    if (target->effect_ticks != 0 || target->state / 50 == FighterStateGroup::levitating || afraid) {
        if (draw(difficulty / 2 + 2) != 0) return;
        const int vx = draw(3) + 3, vy = draw(2) + 2;
        const bool free = self.state <= FighterState::free_last || self.state == FighterState::jump_start || self.state == FighterState::jump_launch || self.state / 10 == FighterStateGroup::lifting || self.state == FighterState::holding_item;
        if (!free) return;
        bool retreat = draw(3) == 0;
        if (!retreat) retreat = draw(2) > 0 && self.coordinated;
        if (retreat) {
            if (self.x > target->x) self.velocity_x = vx;
            if (self.x < target->x) self.velocity_x = -vx;
            self.facing = self.x > target->x ? -1 : 1;
            if (self.y > target->y) self.velocity_y = vy;
            if (self.y < target->y) self.velocity_y = -vy;
            if (self.y == target->y) self.velocity_y = self.y < 150 ? vy : -vy;
            if (draw(3) == 0) requested_key = InputKey::jump;
        }
        const bool try_move = self.z == 0 && draw(difficulty / 2 + 1) == 0;
        if (try_move && self.state / 10 != FighterStateGroup::lifting && target->state / 10 != FighterStateGroup::injured) select_special();
        return;
    }
    if (weapon_attacking(fighters, index, difficulty, draw, projectiles, upper, lower) && self.velocity_z == 0) {
        requested_key = InputKey::jump;
        return;
    }
    const auto run_tactic = [&] {
        int tactic_x = 0, tactic_y = 0;
        if (self.tactic == OpponentTactic::aerial_attack || self.tactic == OpponentTactic::aerial_followup) tactic_x = draw(3) + 3;
        if (self.tactic == OpponentTactic::heal) tactic_y = draw(2) + 4;
        else if (self.tactic == OpponentTactic::summon_weapon || self.tactic == OpponentTactic::align_after_summon || self.tactic == OpponentTactic::aerial_attack || self.tactic == OpponentTactic::aerial_followup) tactic_y = draw(2) + 2;
        if (self.tactic != OpponentTactic::heal && self.state / 10 == FighterStateGroup::running) {
            requested_key = target->y > self.y ? InputKey::down : InputKey::up;
            self.history.back() = requested_key;
        }
        const bool mobile = self.state <= FighterState::free_last || self.state == FighterState::jump_start || self.state == FighterState::jump_launch ||
                            self.state / 10 == FighterStateGroup::lifting || self.state == FighterState::holding_item || (self.tactic == OpponentTactic::heal && self.state / 10 == FighterStateGroup::running);
        if (!mobile) return;
        const auto activate = [&](int kind) {
            bool activated = false;
            for (const auto& move : moves)
                if (move.frames[4] == kind && (self.z == 0 || kind == SpecialMoveId::hung_ball) && self.activate_special(move)) activated = true;
            return activated;
        };
        const int distance = std::abs(self.x - target->x);
        int nearby_pressure = 0;
        for (const auto& fighter : fighters) {
            if (&fighter == target || fighter.team == self.team) continue;
            const int distance_to_target = std::abs(fighter.x - target->x) + std::abs(fighter.y - target->y);
            if (distance_to_target < 120) nearby_pressure += (120 - distance_to_target) * 4;
        }
        if (self.tactic == OpponentTactic::teleport) {
            if (target->energy > 125 || distance < 100 || nearby_pressure > 200) self.tactic = OpponentTactic::none;
            else activate(SpecialMoveId::shun_kan);
        } else if (self.tactic == OpponentTactic::heal) {
            if (self.health > self.max_health * 2 / 3) { self.tactic = OpponentTactic::none; return; }
            self.facing = target->x > self.x ? -1 : 1;
            self.history[2] = self.history[3] = self.history[4] = self.facing == 1 ? InputKey::right : InputKey::left;
            self.velocity_y = (self.y > target->y ? 1 : -1) * tactic_y;
            if (distance > 95 || target->just_eliminated || (self.health < self.health / 11 && std::abs(target->y - self.y) > 18)) {
                if (self.state / 10 == FighterStateGroup::running) {
                    requested_key = target->y > self.y ? InputKey::down : InputKey::up;
                    self.history.back() = requested_key;
                }
                activate(SpecialMoveId::heal_self);
                self.tactic = OpponentTactic::none;
            }
        } else if (self.tactic == OpponentTactic::make_sword) {
            if (distance >= 70 && self.state / 50 != FighterStateGroup::item && draw(10) == 0) activate(SpecialMoveId::make_sword);
            self.tactic = OpponentTactic::none;
        } else if (self.tactic == OpponentTactic::invisibility) {
            if (self.invisible_ticks == 0 && distance >= 70) activate(SpecialMoveId::yan_sun);
            self.tactic = OpponentTactic::none;
        } else if (self.tactic == OpponentTactic::summon_weapon) {
            if (self.health < self.max_health * 2 / 5 || self.state / 50 == FighterStateGroup::item) self.tactic = OpponentTactic::none;
            else {
                self.facing = self.x > target->x ? -1 : 1;
                activate(SpecialMoveId::hei_sheun);
                self.tactic = OpponentTactic::align_after_summon;
            }
        } else if (self.tactic == OpponentTactic::align_after_summon) {
            self.facing = self.x > target->x ? -1 : 1;
            self.velocity_y = (self.y > target->y ? -1 : 1) * tactic_y;
            if (std::abs(self.y - target->y) < 27) {
                self.tactic = OpponentTactic::none;
                requested_key = InputKey::jump;
            }
        } else if (self.tactic == OpponentTactic::aerial_attack || self.tactic == OpponentTactic::aerial_followup) {
            if (std::abs(self.y - target->y) > 30 || distance > 70) self.tactic = OpponentTactic::none;
            else {
                self.velocity_x = self.x > target->x ? -tactic_x : tactic_x;
                self.velocity_y = self.y > target->y ? -tactic_x : tactic_y;
                self.facing = self.x > target->x ? -1 : 1;
                requested_key = InputKey::jump;
                if (self.velocity_z > 0) { activate(SpecialMoveId::hung_ball); self.tactic = OpponentTactic::none; }
            }
        } else if ((target->energy > 125 && self.invisible_ticks < 10) || distance < 75 || nearby_pressure > 200) {
            self.tactic = OpponentTactic::none;
        } else if (self.tactic == OpponentTactic::teleport_sequence) {
            activate(SpecialMoveId::shun_kan_2);
            if (self.energy > 100 && draw(3) > 0) self.tactic = OpponentTactic::ranged_followup;
        } else {
            activate(SpecialMoveId::ball_kat);
            self.tactic = OpponentTactic::none;
        }
    };
    const auto has_tactic = [&] { return (self.tactic >= OpponentTactic::teleport && self.tactic <= OpponentTactic::aerial_attack) || self.tactic == OpponentTactic::ranged_followup || self.tactic == OpponentTactic::align_after_summon || self.tactic == OpponentTactic::aerial_followup; };
    if (has_tactic()) { run_tactic(); return; }
    int speed_x = draw(3) + 3 - difficulty / 5;
    int speed_y = draw(2) + 2 - difficulty / 5;
    if (self.invisible_ticks > 10) { speed_x *= 3; speed_y *= 3; }
    if (self.z < 0 && self.velocity_z > 6) requested_key = InputKey::attack;
    const bool free = self.state <= FighterState::free_last || self.state == FighterState::jump_start || self.state == FighterState::jump_launch || self.state / 10 == FighterStateGroup::running || self.state / 10 == FighterStateGroup::lifting || self.state == FighterState::holding_item;
    if (!free) return;
    const int dx = std::abs(self.x - target->x);
    const int dy = std::abs(self.y - target->y);
    if (draw(2) > 0) {
        constexpr std::array ranks{0, 10, 30, 29, 50, 51, 34, 33, 49, 32, 5, 100};
        const int own_rank = self.archetype >= 1 && self.archetype <= 11 ? ranks[self.archetype] : 0;
        const int target_rank = target->archetype >= 1 && target->archetype <= 11 ? ranks[target->archetype] : 0;
        int aim_x = self.x, aim_y = self.y;
        bool approach_target = companion == nullptr;
        if (companion) {
            aim_x = (self.x - companion->x) / 3 * 2 + companion->x;
            aim_y = (self.y - companion->y) / 3 * 2 + companion->y;
            if (!companion->computer) aim_x -= companion->facing * 35;
            approach_target = std::abs(aim_x - self.x) + std::abs(aim_y - self.y) < 35;
        }
        if (approach_target) {
            aim_x = target->x - target->facing * (difficulty == 0 ? 25 : 1);
            if (target->state / 10 == FighterStateGroup::injured) aim_x = target->x;
            aim_y = target->y;
            if (difficulty == 0 && dx > 27 && own_rank <= target_rank)
                aim_y += self.y > target->y ? 14 : -14;
        }
        if (std::abs(aim_x - self.x) > 70 || (target->state / 10 == FighterStateGroup::injured && std::abs(aim_x - self.x) < 40)) {
            self.facing = self.x < aim_x ? 1 : -1;
            const char direction = self.facing == 1 ? InputKey::right : InputKey::left;
            self.history[2] = self.history[3] = self.history[4] = direction;
            speed_y = draw(2) + 4;
        } else if (self.state / 10 == FighterStateGroup::running) self.history.back() = target->y > self.y ? InputKey::down : InputKey::up;
        if (self.x > aim_x + 3) self.velocity_x = -speed_x;
        if (self.x < aim_x - 3) self.velocity_x = speed_x;
        self.facing = self.x > target->x ? -1 : 1;
        if ((dx < 27 || (own_rank > target_rank && !companion) || target->state / 10 == FighterStateGroup::injured) && target->state / SpecialMoveId::state_stride != SpecialState::hung_ball) {
            if (self.y > aim_y + 3) self.velocity_y = -speed_y;
            if (self.y < aim_y - 3) self.velocity_y = speed_y;
        }
        if ((self.coordinated && draw(8) == 0 && dx < 70 && dy < 20) ||
            (draw(3) == 0 && target->action == FighterAction::throwing) || self.state / SpecialMoveId::state_stride == SpecialState::ball_kat)
            requested_key = InputKey::jump;
        if (self.state >= FighterState::lift_base && self.state <= FighterState::lift_last) {
            const int held = self.state - FighterState::lift_base;
            if (held < int(fighters.size())) {
                const int type = fighters[held].archetype;
                if (type >= 0 && type < int(types.size()) && types[type].category < ItemCategory::throwable && dx < 150 && dy < 9)
                    requested_key = InputKey::jump;
            }
        }
    }
    if (target->state / 10 != FighterStateGroup::injured && std::abs(target->x - target->facing * 2 - self.x + self.facing * 2) < 24 && dy < 8) {
        if (draw(5) > 1 && target->state / SpecialMoveId::state_stride != SpecialState::hung_ball) requested_key = InputKey::attack;
    } else if (target->state / 10 == FighterStateGroup::injured && dx < 10 && dy < 4) {
        if (self.state / 10 == FighterStateGroup::running) self.history.back() = target->y > self.y ? InputKey::down : InputKey::up;
        requested_key = InputKey::attack;
        self.remember(InputKey::attack);
    }
    if (self.z == 0 && !target->just_eliminated && draw(difficulty / 2 + 1) == 0 && target->state / 10 != FighterStateGroup::injured && self.state / 10 != FighterStateGroup::lifting) select_special();
    if (has_tactic()) run_tactic();
}
