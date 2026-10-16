# Xbox One Fat: Direct3D 11 + Direct3D 12 investigation

The existing **Switch game renderer remains Direct3D 12 (SM5.1/DXBC)**.
No Direct3D 11 backend for Switch games is claimed or silently simulated.

## Why the Cemu approach cannot be copied wholesale

The Wii U/GX2 backend in Cemu and the Switch Maxwell/Tegra shader and
texture paths in Eden are different. A D3D11 device and a UI selector
do not supply a working D3D11 Switch renderer.

The Xbox One UWP hardware capability documented by DirectX engineers is:

- D3D11 UWP hardware: Feature Level 10.1, Shader Model 4.1.
- D3D12 UWP Game Mode hardware: Feature Level 11.0, with the tested
  emulator path using SM5.1 DXBC.
- Falling back to D3D11 WARP FL11.0 would be software rendering and
  should not be described as GPU acceleration or an FPS optimization.

See https://walbourn.github.io/directx-and-uwp-on-xbox-series-x-s/.

## First milestone implemented: isolated native D3D11 probe

Under **Settings > System > Test DirectX 11 hardware**, the diagnostic
performs the following on a worker thread, outside running games:

1. Call D3D11CreateDevice with HARDWARE only (never fallback to WARP).
2. Record feature level, DXGI adapter VendorID and DeviceID.
3. Compile actual HLSL vs_4_1 and ps_4_1 DXBC with FXC.
4. Create a 32x32 RGBA8 render target and D24S8 depth-stencil view.
5. Submit a triangle draw, copy to a staging texture, Map and verify a
   colored center pixel.
6. Report success/failing stage on the screen (also suitable for logs).

This is a minimal render test. It cannot demonstrate compatibility
with real games, BC6/BC7, ASTC, texture cache or other Switch GPU behavior.
The probe is manually triggered and has no effect on the game's D3D12
pipeline, performance, settings, presentation or shader cache.

## Required next steps before shipping an actual graphics API selector

1. Design a concrete D3D11 rasterizer/backend implementation plugged into
   `Settings::RendererBackend`, rather than drawing a second swapchain
   while still running games on D3D12.
2. Port guest GPU command handling, texture/SRV/RTV/DSV management,
   descriptors, framebuffer resolution, barriers/synchronization, and
   invalidation/aliasing to D3D11 resources.
3. Introduce a SM4.1 shader path (SPIR-V/HLSL translation + FXC) with
   explicit unsupported-feature reporting and CPU/graphics fallbacks
   for compute/UAV/tessellation needs absent on D3D11 FL10.1.
4. Validate 2D frame output and the depth path in a 3D homebrew scene;
   use A Short Hike only after the minimal geometry/depth tests work.
5. Allow switching only at game startup; persist per-game choice and
   keep D3D12 as a safe default and rollback.
6. Record DXGI adapter, actual driver type, FL, compiler profiles,
   frametime, FPS, process memory, and each first failing call. Compare
   in identical scenes. Do not presume a second API improves Sonic Mania.

## Safety and debugging rules

- Probe and backend work must not overwrite the confirmed D3D12
  orientation correction in Sonic Mania.
- No unlabelled WARP fallback; if the hardware probe fails, say so.
- Keep DX11 code in a separate implementation so driver failures can
  be isolated rather than disguised.
- No confirmed Xbox One Fat DX11 game compatibility until a signed APPX
  is installed and tested on the actual hardware.
