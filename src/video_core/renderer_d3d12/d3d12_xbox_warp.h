// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
namespace D3D12 {
// Selected before creating a game's D3D12 renderer. Default preserves hardware.
// WARP may not be available on Xbox UWP; failure must be reported rather than
// silently pretending software rendering was selected.
inline std::atomic_bool xbox_warp_enabled{false};
inline void SetXboxWarpEnabled(bool enabled) noexcept {
    xbox_warp_enabled.store(enabled, std::memory_order_release);
}
inline bool XboxWarpEnabled() noexcept {
    return xbox_warp_enabled.load(std::memory_order_acquire);
}
} // namespace D3D12
