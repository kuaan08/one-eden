// SPDX-FileCopyrightText: Copyright 2026 JulianDr14
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>

namespace VideoCore {
// Frontend preference shared with the render thread; disabled until settings are loaded.
inline std::atomic<bool> performance_overlay_enabled{false};
enum OverlayField : unsigned {
    OverlayFps = 1u << 0, OverlayFrameTime = 1u << 1,
    OverlayCpu = 1u << 2, OverlayGpu = 1u << 3, OverlayMemory = 1u << 4,
};
inline constexpr unsigned DefaultOverlayFields =
    OverlayFps | OverlayFrameTime | OverlayCpu | OverlayGpu;
inline std::atomic_uint performance_overlay_fields{DefaultOverlayFields};
inline std::atomic_uint performance_overlay_position{0}; // 0 = top right, 1 = top left
inline std::atomic_uint performance_overlay_scale{1}; // 0 = small, 1 = normal, 2 = large
} // namespace VideoCore
