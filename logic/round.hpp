// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "combat.hpp"
#include <span>

namespace RoundResult {
inline constexpr int contested = -1;
inline constexpr int no_team = -2;
}

namespace RoundTiming {
inline constexpr int finish_delay_ticks = 30;
inline constexpr int signal_interval_ticks = 5;
inline constexpr int signal_phase_ticks = 2;
inline constexpr int signal_duration_ticks = 20;
}

class Round {
public:
    // Negative winner means the outcome is not yet settled or nobody remains.
    int winner() const { return winner_; }
    bool ongoing() const { return winner_ < 0; }
    bool finished() const { return ticks_ > RoundTiming::finish_delay_ticks; }
    bool end_signal() const { return winner_ != RoundResult::contested && ticks_ > 0 && (ticks_ - 1) % RoundTiming::signal_interval_ticks == RoundTiming::signal_phase_ticks && ticks_ - 1 < RoundTiming::signal_duration_ticks; }
    static void assign_colors(std::span<Combatant> fighters);
    void advance(std::span<const Combatant> fighters);
private:
    int winner_ = RoundResult::no_team;
    int ticks_ = 0;
};
