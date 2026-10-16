# Xbox One Fat: D3D11 / D3D12 investigation (2026-10-15)

## What is real today

- **Switch game renderer:** Direct3D 12, Feature Level 11.0 in UWP Game Mode; native SM5.1 DXBC conversion, with an experimental driver/depth compatibility path.
- **D3D11 experimental test:** Settings > System > "Test DirectX 11 GPU hardware". It creates a *separate hardware-only* D3D11 device, negotiates FL11.0/FL10.1/FL10.0, compiles SM4.1 (or SM4.0) VS+PS, draws an offscreen 4x4 triangle, copies the result into a staging texture, and verifies an RGBA pixel via GPU readback.
- **No WARP fallback.** A software rasterizer on AMD Jaguar would invalidate performance comparisons.
- **Not implemented:** a Switch-game D3D11 renderer. The test does not boot a game via D3D11, and neither the performance overlay nor the game menu may label it a selectable renderer yet.

## Why it is not as simple as Cemu's selector

Cemu's UWP fork has its own native D3D11 game renderer (for GX2) plus an experimental alternate renderer. Eden emulates a different GPU and its D3D12 runtime assumes feature availability missing in Xbox One UWP D3D11 FL10.1.

The March 2022 Xbox UWP update exposed **D3D11 hardware FL10.1** on original Xbox One, which supports **Shader Model 4.1** rather than SM5. The existing game's SM5.1 resource, compute, shader and descriptor paths cannot just be passed to a D3D11 device. A functional second backend needs its own render targets, sampler/resource bindings, shader translation (SM4.1), texture formats and fallbacks, barriers/synchronization mapping, GPU memory and pipeline caches.

On the original Xbox One in UWP Game Mode, hardware **D3D12 FL11.0** remains the fastest plausible primary backend until a real D3D11 renderer has been built and benchmarked.

Sources:
- https://walbourn.github.io/directx-and-uwp-on-xbox-series-x-s/
- https://walbourn.github.io/directx-and-uwp-on-xbox-one/
- Microsoft Learn: Direct3D feature levels and D3D11CreateDevice

## Test instructions

1. Install the new experimental APPX using Xbox Device Portal in **Game** mode.
2. Open **Settings > System > Test DirectX 11 GPU hardware**.
3. Let the background test finish. **PASS** confirms that D3D11 shaders and an offscreen draw worked on a hardware-type device. **No Hardware** means D3D11CreateDevice(HARDWARE) failed at the selected levels. **Failed** means a shader, texture, draw/readback or pixel validation failed.
4. Export `eden_uwp_diag.txt` / `eden_log.txt` and look for `Xbox D3D11 hardware smoke:`. This includes Feature Level, hardware path, vendor/device ID, first failure HRESULT, and whether the readback passed.
5. Compare the separate 3D game trace. A D3D11 PASS does **not** prove the current D3D12 `umd12ddi.dll+0x3732d` crash has been fixed.

The menu action can be repeated without restarting or changing saved renderer preferences. The current D3D12 game backend is unaffected by a failed D3D11 test.

## Roadmap before adding an honest Dual Mode selector

1. Pass hardware D3D11 FL10.1 / SM4.1 draw and readback on actual Xbox One Fat (this build).
2. Build a separate, source-controlled D3D11 rasterizer for a minimal guest GPU color buffer. **Never swap in WARP silently.**
3. Add shader and texture compatibility fallbacks, small 2D guest title boot, then depth and 3D attachments.
4. Only enable a persistent D3D11/D3D12 game selector when both options can actually draw Switch game frames. Preserve D3D12 as the safe default and keep per-backend shader caches separate.
5. Compare the same Sonic Mania scene at 60 seconds, frametimes, CPU-core use and RAM; do not imply the D3D11 path is faster before measurement.
