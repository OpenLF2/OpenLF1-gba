// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include "combat.hpp"
#include <optional>

class Blast {
public:
    int slot = -1, owner = 0, x = 0, y = 0, z = 0, frame = 1, offset = 0;
    bool active = true;
    std::optional<ImpactRegion> advance(int damage);
};
