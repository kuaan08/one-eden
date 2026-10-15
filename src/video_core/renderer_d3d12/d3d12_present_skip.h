// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
namespace D3D12 {
// Only presentation frames are omitted. Guest CPU/GPU cycles and audio continue.
inline std::atomic_uint xbox_present_interval{1};
inline void SetXboxPresentInterval(unsigned interval) {
    xbox_present_interval.store(interval == 2 || interval == 3 ? interval : 1, std::memory_order_release);
}
inline unsigned XboxPresentInterval() {
    return xbox_present_interval.load(std::memory_order_acquire);
}
} // namespace D3D12
