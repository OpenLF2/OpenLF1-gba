// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "round.hpp"
#include <array>

void Round::assign_colors(std::span<Combatant> fighters) {
    constexpr std::array colors{1, 16, 31, 46, 65, 108, 126, 144};
    int next = 0;
    for (std::size_t i = 0; i < fighters.size(); ++i) {
        auto& fighter = fighters[i];
        if (!fighter.active) continue;
        bool found = false;
        for (std::size_t j = 0; j < i; ++j) {
            if (fighters[j].active && fighters[j].team == fighter.team) {
                fighter.team_color = fighters[j].team_color;
                found = true;
                break;
            }
        }
        if (!found) fighter.team_color = colors[std::min(next++, int(colors.size()) - 1)];
    }
}

void Round::advance(std::span<const Combatant> fighters) {
    int team = -1;
    winner_ = RoundResult::no_team;
    for (const auto& f : fighters) {
        if (!f.active || f.state / 10 == FighterStateGroup::angel) continue;
        if (team > 0 && f.team != team && winner_ > 0) winner_ = RoundResult::contested;
        if (team < 0) winner_ = team = f.team;
    }
    if (winner_ > 0 || winner_ == RoundResult::no_team) ++ticks_;
}
