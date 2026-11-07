// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include <cstdint>
#include <optional>
#include <array>
#include <span>
#include <functional>

// Input codes stored in the original five-key history.
namespace InputKey {
inline constexpr char left = 'a';
inline constexpr char right = 'd';
inline constexpr char up = 'w';
inline constexpr char down = 'x';
inline constexpr char attack = 's';
inline constexpr char jump = 't';
inline constexpr char interrupted = '~';
}

// Values used by the original game's fighter state machine.
namespace FighterState {
inline constexpr int reset = 0;
inline constexpr int idle = 1;
inline constexpr int free_last = 4;
inline constexpr int idle_last = 9;
inline constexpr int punch_start = 11;
inline constexpr int punch_active = 12;
inline constexpr int punch_retract = 13;
inline constexpr int punch_finish = 14;
inline constexpr int kick_start = 21;
inline constexpr int kick_active = 22;
inline constexpr int kick_retract = 23;
inline constexpr int kick_finish = 24;
inline constexpr int hit_reaction = 31;
inline constexpr int staggered = 32;
inline constexpr int falling = 41;
inline constexpr int downed_first = 42;
inline constexpr int downed_last = 49;
inline constexpr int downed_end = 50;
inline constexpr int jump_start = 71;
inline constexpr int jump_launch = 72;
inline constexpr int landing = 75;
inline constexpr int run_start = 91;
inline constexpr int run_step_sound = 93;
inline constexpr int run_wrap = 95;
inline constexpr int recovery_start = 111;
inline constexpr int recovery = 117;
inline constexpr int throw_finish = 118;
inline constexpr int recovery_end = 120;
inline constexpr int throw_start = 121;
inline constexpr int thrown = 122;
inline constexpr int thrown_bounce = 123;
inline constexpr int item_idle_base = 250;
inline constexpr int holding_item = 251;
inline constexpr int item_throw_base = 260;
inline constexpr int throw_item = 261;
inline constexpr int release_item = 263;
inline constexpr int use_weapon = 271;
inline constexpr int weapon_active = 272;
inline constexpr int weapon_finish = 275;
inline constexpr int consume_item = 281;
inline constexpr int consume_last_charge = 287;
inline constexpr int consume_finish = 295;
inline constexpr int frozen_end = 300;
inline constexpr int frozen_start = 330;
inline constexpr int levitate_start = 350;
inline constexpr int levitate_end = 370;
inline constexpr int angel = 400;
inline constexpr int angel_wrap = 404;
// Grapple states encode the other fighter's slot as an offset from a base.
inline constexpr int lift_base = 131;
inline constexpr int lift_last = 138;
inline constexpr int clinched_base = 161;
inline constexpr int clinched_last = 168;
inline constexpr int clinch_base = 181;
inline constexpr int clinch_last = 188;
inline constexpr int clinch_strike_last = 196;
inline constexpr int lifted_base = 201;
inline constexpr int lifted_carry_base = 209;
inline constexpr int lifted_last = 216;
inline constexpr int slot_stride = 8;
}

namespace FighterDefaults {
inline constexpr int battle_health = 200;
inline constexpr int battle_energy = 100;
inline constexpr int battle_max_energy = 250;
inline constexpr int balance = 51;
inline constexpr int spawn_invulnerability = 20;
}

namespace CombatTuning {
inline constexpr int rest_health = 37;
inline constexpr int rest_energy = 60;
inline constexpr int rest_animation_frames = 3;
inline constexpr int rest_energy_gain = 3;
inline constexpr int regeneration_cycle = 4;
inline constexpr int regeneration_ticks = 3;
inline constexpr int animation_cycle_frames = 4;
inline constexpr int balance_recovery = 12;
}

// Attack priority values used by collision checks in the original game.
namespace AttackPriority {
inline constexpr int none = 0;
inline constexpr int grapple = 1;
inline constexpr int blast = 10;
inline constexpr int special = 50;
inline constexpr int strong = 100;
inline constexpr int evasive = 101;
}

// Character IDs from the original data, named for their special behavior.
namespace FighterType {
inline constexpr int bow_user = 4;
inline constexpr int sword_user = 5;
inline constexpr int restricted_ball_user = 8;
inline constexpr int projectile_reflector = 9;
inline constexpr int cloning_fighter = 10;
inline constexpr int regenerating_fighter = 11;
}

// Groups are derived by dividing the encoded fighter state by 5, 10, 20, or 50.
namespace FighterStateGroup {
inline constexpr int idle = 0;
inline constexpr int punching = 1;
inline constexpr int kicking = 2;
inline constexpr int staggering = 3;
inline constexpr int injured = 4;
inline constexpr int downed = 5;
inline constexpr int item = 5;
inline constexpr int item_idle = 50;
inline constexpr int frozen = 6;
inline constexpr int levitating = 7;
inline constexpr int running = 9;
inline constexpr int clinching = 9;
inline constexpr int lifted = 10;
inline constexpr int recovering = 11;
inline constexpr int thrown = 12;
inline constexpr int lifting = 13;
inline constexpr int consuming = 14;
inline constexpr int holding = 25;
inline constexpr int throwing_item = 26;
inline constexpr int using_weapon = 27;
inline constexpr int angel = 40;
}

// Animation identifiers stored in fighter action fields.
namespace FighterAction {
inline constexpr int idle = 1;
inline constexpr int walk_first = 2;
inline constexpr int walk_second = 3;
inline constexpr int punch_windup = 4;
inline constexpr int punch_hit = 5;
inline constexpr int punch_windup_alt = 6;
inline constexpr int punch_hit_alt = 7;
inline constexpr int weapon_idle = 7;
inline constexpr int kick_windup = 8;
inline constexpr int kick_hit = 9;
inline constexpr int kick_windup_alt = 10;
inline constexpr int kick_hit_alt = 11;
inline constexpr int hit_first = 12;
inline constexpr int hit_variants = 3;
inline constexpr int clinch_hit = 14;
inline constexpr int hit = 15;
inline constexpr int hit_air = 16;
inline constexpr int hit_fall = 17;
inline constexpr int down = 18;
inline constexpr int bounce = 19;
inline constexpr int airborne = 20;
inline constexpr int landing = 21;
inline constexpr int fall_forward = 23;
inline constexpr int clinch = 24;
inline constexpr int clinch_strike = 25;
inline constexpr int throwing = 26;
inline constexpr int lift_start = 28;
inline constexpr int lift = 29;
inline constexpr int lifted = 31;
inline constexpr int item_windup = 32;
inline constexpr int drink_first = 33;
inline constexpr int drink_second = 34;
inline constexpr int burning = 36;
inline constexpr int dead = 37;
inline constexpr int rest = 38;
inline constexpr int rest_alt = 39;
inline constexpr int roll = 40;
inline constexpr int frozen = 42;
inline constexpr int clinch_break = 43;
inline constexpr int run = 45;
inline constexpr int forward_roll = 47;
inline constexpr int backward_roll = 48;
inline constexpr int victory = 50;
inline constexpr int victory_alt = 51;
inline constexpr int death_pose = 58;
inline constexpr int angel = 59;
inline constexpr int normal_pose_count = 60;
inline constexpr int angel_alt = 61;
inline constexpr int make_sword_first = 62;
inline constexpr int make_sword_stride = 20;
inline constexpr int guard = 75;
inline constexpr int bow_use = 83;
inline constexpr int bow_release = 84;
inline constexpr int weapon_swing_first = 87;
inline constexpr int weapon_swing_last = 89;
inline constexpr int summon_weapon = 101;
inline constexpr int summon_weapon_offset = 100;
inline constexpr int invisibility_first = 117;
inline constexpr int invisibility_second = 118;
inline constexpr int invisibility_third = 119;
inline constexpr int carry_offset = 27;
inline constexpr int run_state_offset = 47;
inline constexpr int angel_state_offset = 342;
}

// Hundreds digit of the original special-move state (move ID plus ten).
namespace SpecialState {
enum Value {
    ball_kat = 11, sing_long = 12, jong = 13, shun_kan = 14, chal = 15,
    hung_ball = 16, bow_ball = 17, foot_1000 = 18, chui_that = 19,
    chi_bow = 20, chung_kem = 21, pow_four = 22, bow_four = 23,
    sheun_fung = 24, pow_pick = 25, hei_sheun = 26, heal_self = 27,
    cheun_sun = 28, make_sword = 29, yan_sun = 30, shun_kan_2 = 31,
    kung_bow = 32, lin_keung = 35, make_sword_2 = 36
};
}

// IDs in the move data map to SpecialState by adding ten.
namespace SpecialMoveId {
enum Value {
    ball_kat = 1, sing_long, jong, shun_kan, chal, hung_ball, bow_ball,
    foot_1000, chui_that, chi_bow, chung_kem, pow_four, bow_four,
    sheun_fung, pow_pick, hei_sheun, heal_self, cheun_sun, make_sword,
    yan_sun, shun_kan_2, kung_bow, lin_keung = 25, make_sword_2 = 26
};
inline constexpr int state_offset = 10;
inline constexpr int state_stride = 100;
}

inline constexpr int special_state(SpecialState::Value move, int frame) {
    return static_cast<int>(move) * SpecialMoveId::state_stride + frame;
}

struct SpecialMove {
    char form = 0;
    std::array<int, 5> frames{};
    int cost = 0;
};

// Damage and effect fields combine a numeric amount with an encoded kind.
namespace ImpactCode {
inline constexpr int scale = 1000;
inline constexpr int grapple = -5;
inline constexpr int levitate = -6;
inline constexpr int non_damaging = 900;
inline constexpr int knockdown = 500;
inline constexpr int recoil = 35;
inline constexpr int none_kind = 0;
inline constexpr int normal_kind = 1;
inline constexpr int fire_kind = 2;
inline constexpr int explosion_kind = 3;
inline constexpr int piercing_kind = 4;
inline constexpr int blunt_kind = 5;
inline constexpr int freeze_kind = 6;
}

struct ImpactRegion {
    int x, y, z;
    int width, height;
    int direction, push_x, push_z;
    int damage, effect;
};
enum class ImpactOrigin { fighter, item, friendly_item, unowned_item, explosion, blast };
enum class BlastPattern { none, forward, centered, spread };
namespace ItemSlot {
inline constexpr int none = -1;
// Left behind by a hit; it happens to equal the row of the ninth object slot.
inline constexpr int knocked = -2;
inline constexpr int knocked_slot = 9;
}

// State and rules independent of video, audio and input devices.
class Combatant {
public:
    int health = 500;
    int max_health = 500;
    int energy = 500;
    int max_energy = 500;
    int balance = FighterDefaults::balance;
    int max_balance = FighterDefaults::balance;
    int x = 0, y = 0, z = 0;
    int facing = 1;
    int team = 0, priority = AttackPriority::none, score = 0;
    int marker = 0, team_color = 1;
    int carried = ItemSlot::none;
    int velocity_x = 0;
    int velocity_y = 0;
    int velocity_z = 0;
    int action = FighterAction::idle;
    int state = FighterState::idle;
    int animation_step = 1;
    int effect_ticks = 0;
    int invisible_ticks = 0;
    int ranged_cooldown = 0;
    int tactic = 0;
    int opponent_target = -1;
    bool coordinated = false;
    int fall_effect = 0;
    int archetype = 1;
    bool computer = false;
    bool active = true;
    bool eliminated = false;
    bool just_eliminated = false;
    int death_pose = 0;
    int thrown_object = -1;
    int held_object_type = -1;
    std::array<char, 5> history{};
    std::array<int, 5> special_frames{};
    std::optional<int> launched_projectile;
    std::optional<ImpactRegion> secondary_impact;
    std::optional<ImpactRegion> body_impact;
    bool teleport_pending = false;
    int projectile_count = 0;
    bool burst_launch = false;
    int dispatch_state = 0;
    BlastPattern blast_pending = BlastPattern::none;
    std::optional<int> item_pending;

    // Advance the passive part of one original simulation step.
    void advance(bool slow_phase, bool alternating_phase, bool round_active, int difficulty);
    void steer(bool left, bool right, bool up, bool down);
    bool request_jump();
    bool dash_ready(bool round_active);
    void remember(char key);
    bool teleport(std::span<const Combatant> targets, const std::function<int(int)>& draw, bool include_defeated = false);
    bool activate_special(const SpecialMove& move);
    bool try_sword(bool holding_melee);
    bool try_special(std::span<const SpecialMove> moves, bool holding_melee = false);
    std::optional<ImpactRegion> special_frame(bool alternate_phase);
    bool can_strike() const;
    bool request_strike(bool kick);
    void add_score(int amount);
    std::optional<ImpactRegion> strike_frame(bool alternate, bool running);
    bool receive(const ImpactRegion& impact, Combatant& source, const std::function<int(int)>& draw, int score_multiplier = 2, ImpactOrigin origin = ImpactOrigin::fighter, int depth_tolerance = 8, std::array<int, 2>* hit_offsets = nullptr, const std::function<void()>& on_impact = {});
    void move(int camera, int upper, int lower, bool bridge, int winner = -2, int landing_variant = 0, int revival_velocity = 0);
private:
    static int recovery_phase_;
};
