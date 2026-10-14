// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lightweight regression check for the Xbox One Fat inverted game output.
// Run with a C++20 compiler using -Isrc:
//   cl /std:c++20 /EHsc /Isrc tools/xbox/tests/xbox-one-present-orientation.cpp
// Does not require D3D12 or an Xbox.

#include <cassert>
#include "video_core/renderer_d3d12/d3d12_present_orientation.h"

int main() {
    using D3D12::CorrectXboxOneGuestCrop;

    // The original GPU path preserves the crop on non-SM5 devices.
    constexpr Common::Rectangle<f32> normal{0.15f, 0.20f, 0.85f, 0.80f};
    constexpr auto unchanged = CorrectXboxOneGuestCrop(normal, false);
    static_assert(unchanged.top == 0.20f && unchanged.bottom == 0.80f);
    static_assert(unchanged.left == 0.15f && unchanged.right == 0.85f);

    // Only the guest image's vertical sampling direction is reversed.
    constexpr auto sm5 = CorrectXboxOneGuestCrop(normal, true);
    static_assert(sm5.top == 0.80f && sm5.bottom == 0.20f);
    static_assert(sm5.left == normal.left && sm5.right == normal.right);

    // Source-level inverted crops should be corrected, not discarded.
    constexpr Common::Rectangle<f32> already_flipped{0.0f, 1.0f, 1.0f, 0.0f};
    constexpr auto corrected = CorrectXboxOneGuestCrop(already_flipped, true);
    static_assert(corrected.top == 0.0f && corrected.bottom == 1.0f);

    // Applying this twice restores exactly the same crop. That lets a
    // future manual "invert guest image" override remain reversible.
    constexpr auto twice = CorrectXboxOneGuestCrop(sm5, true);
    static_assert(twice.top == normal.top && twice.bottom == normal.bottom);
    assert(CorrectXboxOneGuestCrop(normal, false).top == normal.top);
}
