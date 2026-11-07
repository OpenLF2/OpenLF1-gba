// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "blast.hpp"
std::optional<ImpactRegion> Blast::advance(int damage) {
    if (!active) return std::nullopt;
    if (++frame == 5) { active = false; return std::nullopt; }
    return ImpactRegion{x, y, z, 20, 17, 2, 7, -13, damage, 2500};
}
