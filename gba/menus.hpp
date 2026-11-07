// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "../logic/game_data.hpp"

// Title: the logo over the night road, with fighters walking by.
void menu_title_enter(const GameData& data);
void menu_title_frame(unsigned frame);

// Fighter selection: a grid of heads and names, and the chosen fighter shown large.
void menu_select_enter(const GameData& data);
// 'moves' shows the highlighted fighter's special moves instead of the list.
void menu_select_frame(const GameData& data, int cursor, int fighters, bool moves, unsigned frame);

// Result of a battle. 'outcome': 0 draw, 1 the player won, 2 the player lost. 'winner' is the
// winning fighter's character (1-11) or 0 for none; 'loser' is the player's character when the
// player lost (shown knocked down), otherwise 0.
void menu_result_enter(const GameData& data, int outcome, int winner, int loser);
void menu_result_frame(unsigned frame);
