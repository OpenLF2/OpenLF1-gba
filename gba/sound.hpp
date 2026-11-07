// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once

// Starts the sound hardware and the vertical-blank interrupt that feeds it. Call once at start-up,
// after frame_sync_start().
void sound_init();

// Starts one sound from the table in assets.h. Up to four sounds play at once; when all four
// voices are busy the one nearest its end is replaced.
void sound_play(int id);

// Mixes the next frame's audio. Call once per frame.
void sound_update();
