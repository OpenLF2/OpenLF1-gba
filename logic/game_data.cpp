// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "game_data.hpp"
#include "fatal.hpp"
#include <string_view>

namespace {
// Reads the whitespace-separated words of DATA.DAT in order, without iostreams.
class Tokens {
public:
    explicit Tokens(std::span<const std::uint8_t> data) : next_(reinterpret_cast<const char*>(data.data())), end_(next_ + data.size()) {}
    std::string_view word() {
        while (next_ < end_ && space(*next_)) ++next_;
        const char* start = next_;
        while (next_ < end_ && !space(*next_)) ++next_;
        if (next_ == start) ok_ = false;
        return {start, std::size_t(next_ - start)};
    }
    int number() {
        const auto text = word();
        int value = 0, sign = 1;
        std::size_t i = 0;
        if (!text.empty() && text[0] == '-') { sign = -1; i = 1; }
        if (i == text.size()) ok_ = false;
        for (; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9') { ok_ = false; break; }
            value = value * 10 + (text[i] - '0');
        }
        return sign * value;
    }
    bool ok() const { return ok_; }
private:
    static bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
    const char* next_;
    const char* end_;
    bool ok_ = true;
};
}

GameData parse_game_data(std::span<const std::uint8_t> data) {
    GameData result;
    auto& appearance = result.appearance; auto& hair = result.hair; auto& boundaries = result.boundaries; auto& ranges = result.ranges; auto& item_types = result.item_types;
    Tokens source(data);
    for (auto& item : appearance) {
        source.word();
        const auto name = source.word();
        for (std::size_t i = 0; i < name.size() && i + 1 < item.name.size(); ++i) item.name[i] = name[i];
        source.word();
        item.head = source.number();
        source.word(); source.number(); source.number();
        for (int j = 0; j < 3; ++j) { source.word(); item.color[j * 2] = source.number(); item.color[j * 2 + 1] = source.number(); }
        item.color[6] = 15; item.color[7] = 31; item.color[8] = 10;
        source.word();
        for (auto& move : item.specials) {
            const auto form = source.word();
            if (form.size() != 1) fatal("Invalid special move");
            move.form = form[0];
            for (auto& frame : move.frames) frame = source.number();
            move.cost = source.number();
        }
        if (!source.ok() || item.head < 1 || item.head > GameLimits::heads) fatal("Invalid character data");
    }
    const auto end_marker = source.word();
    for (auto& color : hair) color = source.number();
    if (!source.ok() || end_marker != "E") fatal("Invalid character data");
    source.word(); source.word();
    for (auto& row : boundaries) {
        source.word();
        row.left = source.number(); row.right = source.number(); row.top = source.number();
        row.bottom = source.number(); row.edge_left = source.number(); row.edge_right = source.number();
    }
    if (!source.ok()) fatal("Invalid stage data");
    for (int i = 1;; ++i) {
        const auto header = source.word();
        if (!source.ok()) fatal("Invalid special data");
        if (header == "E") break;
        if (i >= int(ranges.size())) fatal("Too many special moves");
        ranges[i].center = source.number(); ranges[i].width = source.number();
        ranges[i].depth = source.number(); ranges[i].strength = source.number();
    }
    source.word();
    for (;;) {
        const auto header = source.word();
        if (!source.ok()) fatal("Invalid item data");
        if (header == "E") break;
        const int index = source.number();
        if (index < 0 || index >= int(item_types.size())) fatal("Invalid item index");
        auto& item = item_types[index];
        item.category = source.number(); item.motion = source.number(); item.effect = source.number();
        item.damage = source.number(); item.knockdown = source.number();
        for (int& image : item.images) image = source.number();
        item.bounce = source.number();
    }
    return result;
}
