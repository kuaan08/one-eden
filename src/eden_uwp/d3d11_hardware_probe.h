// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
namespace EdenXbox {
struct D3D11HardwareReport {
    bool passed{};
    unsigned feature_level{};
    unsigned vendor_id{};
    unsigned device_id{};
    std::uint32_t error{};
    std::string stage;
};
// An opt-in hardware capability probe, NOT a Switch game renderer.
// Does not fall back to D3D11 WARP and never touches the D3D12 game device.
D3D11HardwareReport ProbeD3D11Hardware();
} // namespace EdenXbox
