// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "combat.hpp"
#include <span>

class Grapple {
public:
    static bool rush(std::span<Combatant> fighters, int holder, int victim, const ImpactRegion& impact);
    static bool clinch(std::span<Combatant> fighters, int holder, int victim, bool left_held, const ImpactRegion& marker);
    static bool strike(std::span<Combatant> fighters, int holder);
    static bool throw_clinched(std::span<Combatant> fighters, int holder);
    static void hold(std::span<Combatant> fighters, int holder);
    static void clinched(std::span<Combatant> fighters, int victim);
    static bool lift(std::span<Combatant> fighters, int holder, int victim, const ImpactRegion& marker);
    static void carry(std::span<Combatant> fighters, int holder);
    static void lifted(std::span<Combatant> fighters, int victim, const std::function<int(int)>& draw);
    static void release_abandoned(std::span<Combatant> fighters);
    static bool throw_held(std::span<Combatant> fighters, int holder);
};
