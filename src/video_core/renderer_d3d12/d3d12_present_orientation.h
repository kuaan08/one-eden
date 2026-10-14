// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Presentation-only correction for the original Xbox One / SM5.1 backend.
//
// Unlike the original DXIL translator, the experimental SPIRV-Cross/HLSL path
// does not implement Eden's conditional Vulkan-to-D3D Y flip for guest draws.
// Those renders therefore land vertically inverted in the GPU framebuffer.
// Flip only the sampling direction during the accelerated guest image
// composite. Neither the UWP frontend overlay nor already-flipped CPU guest
// uploads should be changed.
//
// A swapped top/bottom is safe for bilinear RecordBlit (tex_scale_offset) and
// FSR::MakePlan: EASU uses signed region dimensions and absolute scale checks.

#include <utility>

#include "common/math_util.h"

namespace D3D12 {

[[nodiscard]] constexpr Common::Rectangle<f32> CorrectXboxOneGuestCrop(
    Common::Rectangle<f32> crop, bool sm5_active) noexcept {
    if (sm5_active) {
        std::swap(crop.top, crop.bottom);
    }
    return crop;
}

} // namespace D3D12
