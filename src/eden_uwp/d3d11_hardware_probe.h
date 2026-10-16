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
D3D11HardwareReport ProbeD3D11Warp(); // CPU software rasterizer, never called implicitly.
// Adapter layer for the existing UWP System page. The D3D11 test is independent
// from the Nintendo Switch renderer; success must never auto-switch games.
namespace D3D11Probe {
enum class Status : std::uint8_t { Passed, NoHardware, Failed };
struct Result {
    Status status{Status::Failed};
    bool warp{};
    unsigned feature_level{};
    unsigned vendor_id{};
    unsigned device_id{};
    std::uint32_t error{};
    std::string stage;
};
inline Result RunSmokeTest(bool use_warp) {
    const auto report = use_warp ? ProbeD3D11Warp() : ProbeD3D11Hardware();
    Result r;
    r.warp = use_warp;
    r.feature_level = report.feature_level;
    r.vendor_id = report.vendor_id;
    r.device_id = report.device_id;
    r.error = report.error;
    r.stage = report.stage;
    r.status = report.passed ? Status::Passed :
        (report.stage == "D3D11CreateDevice HARDWARE" ||
         report.stage == "D3D11CreateDevice WARP" ||
         report.stage == "Software adapter detected" ||
         report.stage == "DXGI hardware adapter")
            ? Status::NoHardware : Status::Failed;
    return r;
}
inline Result RunHardwareSmokeTest() { return RunSmokeTest(false); }
inline Result RunWarpSmokeTest() { return RunSmokeTest(true); }
} // namespace D3D11Probe
} // namespace EdenXbox
