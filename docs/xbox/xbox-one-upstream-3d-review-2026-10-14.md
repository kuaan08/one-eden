# Original Eden UWP renderer: 3D compatibility review (14 October 2026)

This is a **source and commit-history review**, not a claim that any 3D
game works on original Xbox One hardware. The reviewed implementation is
[JulianDr14/eden-xbox, branch xbox](https://github.com/JulianDr14/eden-xbox/tree/xbox).
The original Xbox One Fat exposes D3D12 **Shader Model 5.1**, requiring the
separate SPIRV-Cross/HLSL/DXBC path added in this fork; upstream primarily
developed a Mesa/DXIL pipeline for newer shader-model devices.

## Specific recent upstream commits

| Commit | Technique | Xbox One Fat relevance |
| --- | --- | --- |
| [67dd0a6](https://github.com/JulianDr14/eden-xbox/commit/67dd0a6c213088b6b02b447b8a5160ca0fde8e32) | Clip-safe lowering of NoPerspective interpolation (with bounded GPU probes) | **Study only**. Its fix addresses interpolated attributes crossing clip-space W=0, not the observed device-side crash during the *first* render-target creation. Different shader backend; porting could break SM5 stage linking. |
| [b702b3d](https://github.com/JulianDr14/eden-xbox/commit/b702b3d925d48e322db924a5a7f060f36269f82c) | Geometry passthrough and SPIR-V stage interface corrections | Relevant for later 3D stage correctness; most of these changes are already in the imported source. Needs targeted per-stage validation on SM5.1. |
| [5e75cd1](https://github.com/JulianDr14/eden-xbox/commit/5e75cd1fa31a007e519f91f5d2d3fa6eb251f1f6) | Resource SRV dimensions and disabled/inactive color attachment bindings | Relevant broadly. Present in the imported D3D12 renderer; this is not proof all formats work on Xbox One. |
| [b78df96](https://github.com/JulianDr14/eden-xbox/commit/b78df964a0cca12cf57fc1606ac54cb2ee6274a0) | Compressed-texture block row strides, odd texture extents | Already reflected in imported texture and block-copy paths. No established link to the current depth crash. |
| [c0efaf3](https://github.com/JulianDr14/eden-xbox/commit/c0efaf30f3bff4fb7b9d0f8b3dc35dc23775634b) | Driver-specific shader loop safety counter workaround | Developed for a different shader-model/driver configuration; do not assume it is safe or necessary on SM5.1. |

## Actual *A Short Hike* fault on original Xbox One

Previously provided user logs:
- CPU JIT initialization completes.
- A minimal graphics pipeline compiles successfully (`vs_5_1` and `ps_5_1`).
- The driver accepts early color resources and a typeless depth resource
  (`1920x1080`) with `CreatePlacedResource` returning `S_OK`.
- The process then faults reproducibly in
  `umd12ddi.dll+0x3732d`, reading near `0x78`. This identifies the
  *driver code that faulted*, **not necessarily the root cause**.
- Memory near the crash was well below the UWP process budget.

The prior changes in this fork add bounded logging of view/resource creation
and a **switchable experiment using committed D3D12 depth textures** only for
SM5.1. This attempts to isolate pooling/aliasing as a possible trigger; it is
not a confirmed remedy. The global settings page explicitly says to restart
the game when toggling it, and the allocator latches its mode at creation.

### Follow-up test order

1. Confirm 2D regression: *Sonic Mania* still renders upright and its
   controls, overlays and textures still work.
2. Launch *A Short Hike* with **Experimental 3D depth fix = On**; collect
   `eden_uwp_diag.txt` and `eden/log/eden_log.txt`.
3. If it crashes, try a **new game launch** with that switch Off; capture
   both logs and compare `D3D12 DEPTH-PROBE` and `AV-PROBE` messages.
4. Do not apply unrelated upstream shader patches until the last successful
   native D3D12 operation is known. Never assume successful C++ build equals
   3D gameplay or higher FPS.
5. If driver crashes still occur in identical positions using both allocation
   policies, focus next on depth view compatibility, `GetCopyableFootprints`,
   and PSO attachment binding, not arbitrary shader-based speed hacks.

## UI and game image changes

- PT-BR translations cover **478/478 current interface messages**.
- Visual themes: default Eden, Xbox green and AMOLED; apply live.
- Final-frame nearest-neighbor sampler for pixel-art 2D; switch live from
  the in-game upscaling option. Existing bilinear/FSR options remain.
- Persistent on/off committed-depth experiment; applies after restarting
  the game, not partway through existing resource lifetimes.
- Separate signed normal/no-profiler UWP APPX and diagnostic APPX in CI,
  for lower-instrumentation FPS comparisons. No numeric performance gain
  is claimed without console benchmarking.

