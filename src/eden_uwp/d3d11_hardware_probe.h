// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace EdenXbox::D3D11Probe {
// Real GPU-only offscreen hardware test, NOT a Nintendo Switch renderer.
// A successful D3D11 FL10.1 triangle does not establish game compatibility.
enum class Status : std::uint8_t { Passed, NoHardware, ShaderFailed, SetupFailed, PixelMismatch };
struct Result {
    Status status{Status::NoHardware};
    std::uint32_t feature_level{};   // D3D_FEATURE_LEVEL or zero
    std::uint32_t vendor_id{};       // DXGI adapter vendor, if provided
    std::uint32_t device_id{};       // DXGI device id, if provided
    std::uint32_t hresult{};         // First failure, if any
    bool hardware_driver{};         // No WARP path is ever requested
};
Result RunHardwareSmokeTest();       // Offscreen 4x4 draw + readback, call only off the UI thread
} // namespace EdenXbox::D3D11Probe
