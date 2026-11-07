// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once
#include <cstdint>

class GameRandom {
public:
    explicit GameRandom(std::uint16_t seed) : state_(seed) {}
    void seed(std::uint16_t seed) { state_ = seed; }
    int draw(int limit) {
        state_ = state_ * multiplier + increment;
        const auto value = (state_ >> output_shift) & output_mask;
        return int((std::uint32_t(value) * std::uint32_t(limit)) / output_range);
    }
private:
    static constexpr std::uint32_t multiplier = 0x015a4e35u;
    static constexpr std::uint32_t increment = 1u;
    static constexpr int output_shift = 16;
    static constexpr std::uint32_t output_mask = 0x7fffu;
    static constexpr std::uint32_t output_range = 32768u;
    std::uint32_t state_;
};
