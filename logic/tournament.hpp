// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include <array>
#include <span>
#include <functional>
#include "combat.hpp"

namespace TournamentFormat {
enum Value { pairs = 1, triples = 2 };
}
namespace TournamentLimits {
inline constexpr int pair_entrants = 10;
inline constexpr int triple_entrants = 9;
inline constexpr int pair_nodes = 19;
inline constexpr int triple_nodes = 13;
inline constexpr int pair_matches = 9;
inline constexpr int triple_matches = 4;
}
enum class TournamentRoute { pending, advanced, eliminated };

struct Entrant {
    int character = 0;
    int player_number = 0;
};
struct Fixture {
    std::array<int, 3> contestants{-1, -1, -1};
    int count = 0;
    int destination = -1;
};
class Tournament {
public:
    Tournament(int format, std::span<const Entrant> participants);
    bool finished() const;
    Fixture fixture() const;
    const Entrant& entrant(int index) const;
    TournamentRoute route(int index) const;
    static int place(std::span<Entrant> participants, Entrant player, const std::function<int(int)>& draw);
    static void fill(std::span<Entrant> participants, int character_count, const std::function<int(int)>& draw);
    static int automatic_winner(std::span<const Entrant> participants, const std::function<int(int)>& draw);
    static int decide(std::span<const Combatant> fighters, int participants, const std::function<int(int)>& draw);
    void advance(int winner);
    int champion() const;
private:
    int format_, step_ = 0;
    std::array<Entrant, TournamentLimits::pair_nodes> entries_{};
    std::array<TournamentRoute, TournamentLimits::pair_nodes> routes_{};
};
