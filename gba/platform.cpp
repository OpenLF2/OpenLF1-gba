// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#include "hw.hpp"
#include "../logic/fatal.hpp"
#include <cerrno>
#include <cstdint>
#include <sys/stat.h>

extern "C" char __ewram_heap_start[];

// Reports a broken invariant: the backdrop turns red and the program stops.
void fatal(const char*) {
    BG_PALETTE[0] = 0x001F;
    for (;;) {}
}

void wait_vblank() {
    while (REG_VCOUNT >= 160) {}
    while (REG_VCOUNT < 160) {}
}

namespace {
std::uint16_t last_frame = 0;
}

void frame_sync_start() {
    wait_vblank();
    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;
    REG_TM2CNT_L = std::uint16_t(65536 - 4389);
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = 0x0084;    // enable, count timer 2 overflows
    REG_TM2CNT_H = 0x0081;    // enable, 1/64 clock
    last_frame = 0;
}

void wait_frame() {
    std::uint16_t now = REG_TM3CNT_L;
    while (now == last_frame) now = REG_TM3CNT_L;
    last_frame = now;
}

void dma_copy(const void* source, void* destination, unsigned bytes) {
    const auto bits = reinterpret_cast<std::uintptr_t>(source) | reinterpret_cast<std::uintptr_t>(destination) | bytes;
    constexpr std::uint32_t enable = 0x80000000u;
    if (bits & 1) return;
    if (bits & 3) {
        REG_DMA3SAD = reinterpret_cast<std::uint32_t>(source);
        REG_DMA3DAD = reinterpret_cast<std::uint32_t>(destination);
        REG_DMA3CNT = (bytes / 2) | enable;
    } else {
        REG_DMA3SAD = reinterpret_cast<std::uint32_t>(source);
        REG_DMA3DAD = reinterpret_cast<std::uint32_t>(destination);
        REG_DMA3CNT = (bytes / 4) | 0x04000000u | enable;
    }
    while (REG_DMA3CNT & enable) {}
}

// Heap for malloc: one contiguous block in EWRAM, after the data kept there. (A heap split across
// IWRAM and EWRAM does not work: the C library's allocator cannot extend a heap into lower addresses.)
extern "C" void* _sbrk(int increment) {
    static char* top = __ewram_heap_start;
    constexpr std::uintptr_t ewram_end = 0x02040000;
    if (reinterpret_cast<std::uintptr_t>(top + increment) > ewram_end) {
        errno = ENOMEM;
        return reinterpret_cast<void*>(-1);
    }
    char* previous = top;
    top += increment;
    return previous;
}

// The game has no files or console; these keep the C library linked.
extern "C" {
int _write(int, const char*, int length) { return length; }
int _read(int, char*, int) { return 0; }
int _close(int) { return -1; }
int _lseek(int, int, int) { return 0; }
int _fstat(int, struct stat* status) { status->st_mode = S_IFCHR; return 0; }
int _isatty(int) { return 1; }
int _kill(int, int) { errno = EINVAL; return -1; }
int _getpid(void) { return 1; }
// Reached by abort() and unhandled errors: turn the backdrop red so a crash is visible, then stop.
void _exit(int) { BG_PALETTE[0] = 0x001F; REG_DISPCNT = REG_DISPCNT & ~0x0080; REG_BLDY = 0; for (;;) {} }
}

void* __dso_handle = nullptr;

// Called by the C library's constructor and destructor runners; this build has no start files.
extern "C" {
void _init(void) {}
void _fini(void) {}
}

