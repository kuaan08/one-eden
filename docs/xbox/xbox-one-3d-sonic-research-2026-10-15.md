# Xbox One Fat — upstream/Reddit research: 3D D3D12 and Sonic Mania CPU (2026-10-15)

## What was actually reproduced

From user-supplied diagnostic archive `EdenXboxDiagnostic (...6).zip` (extract **only text logs**, never keys, save data, firmware or games), the old orientation-fixed **`fc1810a66f-main`** reports:

- **Sonic Mania**: `CPUCore_0` 19,328 ms CPU time in a 20-second profiler window (~96.6% of a logical core). GPU emulation host thread 15.6 ms CPU time in same window; sampling-profiler overhead is enabled at 250 Hz. Therefore a guest **CPU/JIT bottleneck is plausible**, not proven as exclusive bottleneck of the renderer or system. The diagnostic APPX is not a performance baseline.
- **A Short Hike**: game builds first VS/PS D3D12 pipeline, then sets up first draw and textures. `GetResourceAllocationInfo` and `CreatePlacedResource` for a 1920x1080 depth-like target return S_OK. Driver faults in `umd12ddi.dll+0x3732d`, reading 0x78, with zero guest frames. Fatal operation is not yet pinpointed; stack in driver alone **does not establish a driver bug**.
- ASTC `cs_5_1` compilation fails; CPU fallback is available. This is separate from the later 3D fault.
- **These logs predate** the experimental committed-depth allocation, current presets, overlay fields and 3D probe additions. Any conclusion about the latest build requires new on-console logs.

## Concrete upstream corrections to examine, not blindly cherry-pick

- SRV view compatibility / null descriptors and inactive RTVs: https://github.com/JulianDr14/eden-xbox/commit/5e75cd1fa31a007e519f91f5d2d3fa6eb251f1f6 (already imported). Original author's PC validation found a wrong-dimension null SRV could yield undefined shader access; duplicate descriptor ownership was also corrected. Check **SM5.1-specific descriptor binding** in `d3d12_image_view.cpp`, descriptor heaps and `d3d12_stage_bindings` at the first draw.
- Geometry passthrough interfaces: https://github.com/JulianDr14/eden-xbox/commit/b702b3d925d48e322db924a5a7f060f36269f82 (largely imported; Series SPIR-V/DXIL assumptions can differ from SM5.1 SPIRV-Cross HLSL/DXBC).
- Identical PSO reuse / dependent draw integrity: https://github.com/JulianDr14/eden-xbox/commit/bb343af7c035dbc495d487096cd4a5728861a4e0 (already imported). Can reduce repeated pipeline creation, **does not remove CPU guest emulation cost**, nor establish FPS gains here.
- BC block row strides: https://github.com/JulianDr14/eden-xbox/commit/b78df964a0cca12cf57fc1606ac54cb2ee6274a0 (already imported).
- Clip-safe NoPerspective: https://github.com/JulianDr14/eden-xbox/commit/67dd0a6c213088b6b02b447b8a5160ca0fde8e32 (Series graphical correctness, unproven relation to the crash).
- The original branch's Oct 15 shader helper/discard work, e.g. https://github.com/JulianDr14/eden-xbox/commit/909beadb902895dd5d7fc5d53401dda77945ba5c, is for shader semantics and **not** evidence of a solution to the SM5.1 Xbox One crash.

## Independent documentation

- Microsoft D3D12 null/typed descriptors and tier limits: https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptors-overview ; https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support . Lower-tier hardware can require **all descriptors within referenced tables to be populated**, even unused ones, and null descriptors must match the shader's expected texture dimension.
- Microsoft D3D12 debug layer/GBV: https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation . Can identify descriptor lifetime, heap-index or state issues **if available** in that UWP environment. Heavy instrumentation is diagnostic-only, not a gameplay optimization.
- D3D12 root signature and PSO consistency: https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-a-root-signature . Binding mismatch is undefined at draw.
- Dynarmic fastmem requires valid host-mapped guest address space and can recompile on fastmem page faults: https://github.com/azahar-emu/dynarmic/blob/master/src/dynarmic/interface/A64/config.h ; https://github.com/eden-emulator/mirror/blob/master/docs/dynarmic/Design.md . Hybrid 384 MiB can cause repeated faults and regressions on this constrained UWP host.
- Reddit reports of better graphics settings on PC/Android are **hardware-specific anecdotes**. Example: https://www.reddit.com/r/yuzu/comments/1fjvyjj/yuzu_optimization_guide_for_newbies_or_beginners/ . The OpenGL/NVIDIA-specific VSync advice **does not map directly** to Xbox D3D12. Anecdotal Sonic Mania flicker fix on Android Mali using reactive flushing (https://www.reddit.com/r/EdenEmulator/comments/1rymsqs/help_with_sonic_mania_graphical_issues/) addresses flicker, **not** Xbox CPU throughput.

## Priority order / exact testing procedure

1. **Do not change defaults before getting a baseline.** On the latest installable `normal` Game APPX (no sampling profiler), benchmark the **same Sonic Mania screen for 60 s** using 100% resolution, Fastmem Off, Frame Skip Off, shader cache On, standard texture GC, standard PSO eviction, CPU JIT 128 MiB/core. Capture average/min FPS and frametime in an identical scene; screenshot the HUD. Compare with diagnostic version only as a way to estimate profiler overhead, never assume it is fixed.
2. Change only internal resolution 100% -> 75% -> 50%, restarting the game each time. Flat FPS indicates CPU-bound; improving FPS implies graphics costs mattered.
3. Separately compare Fastmem Hybrid vs Off only with diagnostic logs and expect possible crashes/slowdowns. Do not set as default without observing faster steady-state CPU guest work **and** fault counts.
4. Profile CPU emulation/clock waits, JIT block-link misses/recompilations, `CPUCore_0` time, and slow HLE calls; avoid disabling safety optimizations merely because a setting sounds faster. Texture release does not intrinsically accelerate Sonic Mania's guest CPU.
5. On latest **diagnostic** APPX, run A Short Hike at 100%, dedicated depth On. Compare with Off in a separate launch. Identify first **unclosed** `AV-PROBE` / `DEPTH-PROBE` around resource/DSV/binding/command-list operations. Add narrowly-scoped probes at the first subsequent unlogged operation, not arbitrary patching to shader/caches. Save `eden_uwp_diag.txt` and `eden_log.txt` only.
6. For a minimal independent D3D12 GPU validation test, verify SRV/RTV/DSV, null descriptors, root signature and shader table coherence on a Tier 1-like SM5.1 path. If validation tools are unavailable on console, use a matching feature-level desktop test where possible **without claiming desktop tests certify console**.

## Current options

As of this research the fork already exposes selectable FPS/frametime/CPU-equivalent-cores/GPU-queue/RAM HUD fields, overlay corner and scale; experimental early unused-clean-texture GC; PSO eviction; CPU JIT cache size; and experimental dedicated depth. Keep experimental optimizations opt-in, and distinguish **compiled**, **running in Xbox hardware**, and **actually faster**.

No public upstream commit or Reddit post was found that reproducibly resolves the exact `umd12ddi.dll+0x3732d` Xbox One Fat access violation. The first required experiment is a **new diagnostic trace of the latest build**, not a claim of a fixed 3D renderer.
