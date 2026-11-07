// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once

// IDs in the original weapon data, shared by ground items and projectiles.
namespace ItemType {
inline constexpr int sword = 0;
inline constexpr int stick = 1;
inline constexpr int seven = 2;
inline constexpr int plus = 3;
inline constexpr int drink = 4;
inline constexpr int bomb = 5;
inline constexpr int claw = 6;
inline constexpr int bow = 7;
inline constexpr int stone = 8;
inline constexpr int box = 9;
inline constexpr int head = 10;
inline constexpr int summoned_sword = 11;
inline constexpr int davis_weapon = 12;
inline constexpr int woody_weapon = 13;
inline constexpr int john_weapon = 14;
inline constexpr int sword_projectile = 15;
inline constexpr int firen_weapon = 16;
inline constexpr int arrow = 17;
inline constexpr int reflected_projectile = 18;
inline constexpr int summoned_weapon = 19;
inline constexpr int dennis_weapon = 20;
inline constexpr int hit_effect = 21;
inline constexpr int clear_effect = 22;
inline constexpr int freeze_effect = 23;
inline constexpr int short_effect = 24;
inline constexpr int knife = 25;
inline constexpr int random_target_projectile = 26;
}

// Motion modes encoded in the original weapon data.
namespace ItemMotion {
inline constexpr int fixed_frame = 1;
inline constexpr int homing = 4;
inline constexpr int ignore_downed = 5;
}

// Knockdown strength encoded in the original weapon data.
namespace ItemKnockdown {
inline constexpr int none = 0;
inline constexpr int knockdown = 2;
}
