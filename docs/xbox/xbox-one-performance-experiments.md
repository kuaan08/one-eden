# Xbox One Fat — resolution, Fastmem, cache and presentation tests

The implementation is intentionally limited to supported settings and
existing renderer paths. **All new combinations are unverified on hardware**
until tested using the Xbox One Fat and diagnostic logs.

## Implementation

- Resolution: `SystemSettings.xbox_resolution` persists as an existing
  `Settings::ResolutionSetup` enum. The boot path invokes
  `Settings::UpdateRescalingInfo()` before constructing the renderer.
  Supported choices are 50%, 75%, 100%. Some D3D12 paths may need extra
  work to respect fractional resolution in every shader or image copy.
- Xbox presets: Balanced (100%); 2D (75%); experimental 3D (50%,
  dedicated depth). None turn on Fastmem or frame skipping by default.
- Fastmem: default **Off**. Explicit Hybrid mode tests the 384 MiB
  arena already implemented in HostMemory. Prior experiments showed
  partial mappings could trigger excessive faults and JIT recompilation.
- Frame skip: `RendererD3D12::Composite` decimates the **final
  displayed** frames by selecting one of every two/three guest frames.
  This does not omit execution of guest CPU instructions or GPU draws.
  It can make presentation stutter and does not promise a speedup.
- Disk shader cache: connects the existing
  `Settings::values.use_disk_shader_cache` option to persistent UWP
  settings. Pipeline serialization is **not** equivalent to persisting
  compiled native Dynarmic CPU instructions.
- Speed cap: 100% or Uncapped connects the existing emulation limiter.
  It does not unlock frame rates locked by games and may affect timing.

## Tests

1. Install the **diagnostic** APPX for baseline testing, backed up separately.
2. Launch Sonic Mania with 100% resolution, Fastmem Off, frame skip Off
   and cache On. Verify orientation and frame rate.
3. Quit the game and test 75% and 50% resolution separately, checking
   sharpness and whether FPS/CPU/GPU time actually change.
4. Try Fastmem Hybrid **only as a separate test**, restart the game,
   and check for increased UWP faults, memory usage or crashes.
5. Keep presentation skip Off unless testing the behavior itself.
6. Test A Short Hike with the existing dedicated-depth correction both
   On and Off; compare the last `AV-PROBE` / `DEPTH-PROBE` operations.

For each run collect `eden_uwp_diag.txt` and `eden/log/eden_log.txt`
without keys, game files or firmware.

## Not implemented

- Genuine CPU/GPU workload-skipping frame skip with guaranteed speed gains;
- Automatic frame pacing prediction/frame generation;
- Safe, measured Fastmem acceleration on original Xbox One;
- Persistent host-native JIT executable code cache;
- Game-specific 30-to-60 FPS patches;
- A verified complete 3D crash fix.
