// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
namespace D3D12 {
inline std::atomic_bool nearest_present_filter_enabled{false};
inline void SetNearestPresentFilterEnabled(bool enabled) noexcept {
    nearest_present_filter_enabled.store(enabled, std::memory_order_release);
}
inline bool NearestPresentFilterEnabled() noexcept {
    return nearest_present_filter_enabled.load(std::memory_order_acquire);
}
} // namespace D3D12
