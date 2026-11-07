// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "../logic/field.hpp"
#include "../logic/game_data.hpp"
#include "../logic/random.hpp"

// Loads the sprite graphics that never change. Call once at start-up.
void battle_init();

// Sets up a free-for-all battle: the player's character against computer opponents.
void battle_start(Field& field, const GameData& data, int fighters, int character, GameRandom& random);

// Loads the colours, heads and stage art a battle needs. Call once after battle_start.
void battle_load(const Field& field, const GameData& data);

// Draws the battle into sprite memory and the background. Call during vertical blank.
void battle_draw(Field& field, const GameData& data);

// Reacts to a battle sound: explosions and heavy hits shake the screen, explosions also flash it.
void battle_impact(int sound);

// Shows or hides the "PAUSED" banner. The caller stops the logic itself.
void battle_set_paused(bool paused);
