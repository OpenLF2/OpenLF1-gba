// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once

// Player intents, mapped from GBA buttons by the front end.
enum class Action { none, close, pause, up, down, left, right, accept, jump, end_round, unlock, key,
    cheat_mode2, cheat_1, cheat_2, cheat_3, cheat_4, cheat_5, cheat_6, cheat_7, cheat_8,
    cheat_previous_stage, cheat_next_stage, cheat_spawn_items, cheat_clear_items,
    cheat_heal1, cheat_heal2, cheat_heal3, cheat_timer };
