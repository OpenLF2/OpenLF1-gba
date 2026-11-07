// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

#pragma once

// Reports a broken invariant and stops. Supplied by the platform layer.
[[noreturn]] void fatal(const char* message);
