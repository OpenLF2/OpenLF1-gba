// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "roster.hpp"
#include <algorithm>
#include "fatal.hpp"

MatchSetup Roster::create(int mode, int option, std::array<Participant, GameLimits::human_players> people, int upper, int lower, const Draw& draw, bool rare_character) {
    if ((mode != GameMode::free_for_all && mode != GameMode::teams) || (mode == GameMode::free_for_all && (option < 2 || option > 8)) ||
        (mode == GameMode::teams && (option < 1 || option > 5))) fatal("Invalid match settings");
    MatchSetup setup{};
    setup.controls.fill(-1);
    std::array<int, GameLimits::fighter_slots> kind{}, team{}, px{}, py{};
    std::array<bool, GameLimits::fighter_slots> active{};
    std::array<int, 4> cursor{};
    if (mode == GameMode::free_for_all) {
        for (int i = 0; i < GameLimits::fighter_slots; ++i) { active[i] = i < option; team[i] = i + 1; px[i] = draw(320) & 0xff; py[i] = draw(80) + 60; }
    } else if (option <= 3) {
        const int offset = option == 1 ? 2 : 4;
        cursor = {0, offset, 0, 0};
        for (int i = 0; i < GameLimits::fighter_slots; ++i) {
            active[i] = option == 1 ? i < 4 : option == 2 ? i != 3 && i != 7 : true;
            if (option == 1 && i >= 4) continue;
            team[i] = i < offset ? 1 : 2;
            px[i] = i < offset ? 40 : 216;
            py[i] = i % 4 * 20 + 70;
        }
    } else {
        cursor = {0, 2, 4, 6};
        constexpr int xpos[8] = {35, 45, 221, 211, 35, 45, 221, 211};
        constexpr int ypos[8] = {90, 85, 90, 85, 110, 115, 110, 115};
        for (int i = 0; i < GameLimits::fighter_slots; ++i) {
            active[i] = option == 5 || i < 6;
            if (!active[i]) continue;
            team[i] = i / 2 + 1; px[i] = xpos[i]; py[i] = ypos[i];
        }
        if (option == 4) px[4] = px[5] = 160;
    }
    int next = 0;
    for (int player = 0; player < GameLimits::human_players; ++player) if (people[player].joined) {
        const int t = people[player].team;
        if (people[player].character < 1 || people[player].character > GameLimits::characters || (mode == GameMode::teams && (t < 1 || t > (option < 4 ? 2 : option - 1))))
            fatal("Invalid participant");
        const int slot = mode == GameMode::free_for_all ? next++ : cursor[t - 1]++;
        if (slot >= GameLimits::fighter_slots || !active[slot] || setup.controls[slot] >= 0 || (mode == GameMode::teams && team[slot] != t))
            fatal("Team is full");
        setup.controls[slot] = player;
        kind[slot] = people[player].character;
    }
    for (int i = 0; i < GameLimits::fighter_slots; ++i) if (!kind[i]) {
        int chosen;
        do { chosen = 1 + draw(GameLimits::selectable_characters); }
        while (std::find(kind.begin(), kind.end(), chosen) != kind.end());
        kind[i] = chosen;
    }
    if (std::find(kind.begin(), kind.end(), GameLimits::characters) == kind.end() && (rare_character || draw(8) == 1)) {
        for (int i = 0; i < GameLimits::fighter_slots; ++i) {
            const int groups = option < 4 ? 2 : option - 1;
            const int checked = groups == 2 ? i / 4 : i / 2;
            const bool human_team = mode == GameMode::teams && groups <= 4 && setup.controls[checked] >= 0;
            if (setup.controls[i] < 0 && !human_team) { kind[i] = GameLimits::characters; break; }
        }
    }
    for (int i = 0; i < GameLimits::fighter_slots; ++i) {
        auto& f = setup.fighters[i];
        f.active = active[i]; f.team = team[i]; f.archetype = kind[i];
        f.computer = setup.controls[i] < 0;
        f.marker = f.computer ? 0 : setup.controls[i] + 1;
        f.score = f.active ? 1 : 0;
        f.x = px[i] + 64; f.facing = f.x < 160 ? 1 : -1;
        f.y = py[i] > 100 ? upper + py[i] : lower - py[i];
        f.health = f.max_health = FighterDefaults::battle_health; f.energy = FighterDefaults::battle_energy; f.max_energy = FighterDefaults::battle_max_energy; f.effect_ticks = FighterDefaults::spawn_invulnerability;
    }
    return setup;
}

MatchSetup Roster::create_contest(std::span<const Entrant> contenders, const std::array<int, 3>& controls, int floor) {
    MatchSetup setup{};
    setup.controls.fill(-1);
    for (auto& fighter : setup.fighters) { fighter.active = false; fighter.x = 64; fighter.y = floor; }
    const int count = int(contenders.size());
    for (int i = 0; i < count; ++i) {
        const int slot = count == 2 && i == 1 ? 3 : i;
        auto& fighter = setup.fighters[slot];
        fighter.active = true; fighter.archetype = contenders[i].character; fighter.team = i + 1;
        fighter.x = 104 + i * (count == 2 ? 176 : 88);
        fighter.y = floor - 90;
        fighter.facing = fighter.x < 160 ? 1 : -1;
        fighter.health = fighter.max_health = FighterDefaults::battle_health; fighter.energy = FighterDefaults::battle_energy; fighter.max_energy = FighterDefaults::battle_max_energy;
        fighter.effect_ticks = FighterDefaults::spawn_invulnerability; fighter.computer = contenders[i].player_number == 0;
        fighter.marker = contenders[i].player_number; fighter.score = 1;
        setup.controls[slot] = controls[i];
    }
    return setup;
}
