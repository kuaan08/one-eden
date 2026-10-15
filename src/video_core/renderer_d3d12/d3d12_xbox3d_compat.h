// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>

namespace D3D12 {
// Two independent, opt-in tests for the Xbox One 3D first-frame driver AV.
// Mode 0 is the previous known 2D baseline. Applied before launching a game;
// never intentionally switch modes while a D3D12 renderer is alive.
inline std::atomic_uint xbox_depth_compat_mode{0};
constexpr bool SkipPostDepthDeviceQuery(unsigned mode) noexcept { return (mode & 1u) != 0; }
constexpr bool UseSingleLayerDepthView(unsigned mode) noexcept { return (mode & 2u) != 0; }
inline void SetXboxDepthCompatMode(unsigned mode) noexcept {
    xbox_depth_compat_mode.store(mode & 3u, std::memory_order_release);
}
inline unsigned XboxDepthCompatMode() noexcept {
    return xbox_depth_compat_mode.load(std::memory_order_acquire);
}
} // namespace D3D12
