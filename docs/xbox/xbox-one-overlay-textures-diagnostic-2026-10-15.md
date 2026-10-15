# Xbox One Fat — telemetry, memory policy and 3D diagnostic status

## Reproducible 3D crash evidence

User-supplied UWP diagnostic archives from the orientation-fixed build `fc1810a66f-main`
show *A Short Hike* reaching GPU pipeline creation but **0 presented guest frames**.
After the graphics setup the Xbox driver `umd12ddi.dll+0x3732d` reports an
access violation reading `0x78`; the app commit was about 890 MiB of a 5120 MiB
UWP budget. Earlier `CreatePlacedResource` calls returned `S_OK`. The trace does
not establish whether the driver fault is caused by a bad descriptor, invalid
resource lifetime, incompatible shader/input bindings or another issue.

**The archive corresponds to old commit `fc1810a66f-main`.** It is not a test
of the later experimental committed-depth workaround, per-game resolution or HUD.
Do not announce that the new workaround fixed the crash until tested on-device.

The diagnostic archive may include users' Switch keys; retain diagnostic
TXT/log files only when sharing issue reports. Never commit keys or game data.

## Customizable overlay

Settings > System:

- Show performance statistics: master switch.
- Show FPS, frame time, CPU process load in equivalent cores, D3D12 GPU queue
  busy time, UWP app memory usage: **independent** switches.
- Move overlay between upper-right and upper-left corners.
- Small / medium / large sizing.

The app memory panel reports current app memory usage / UWP app memory limit,
**not Xbox system RAM**. GPUQ is not overall GPU utilization. HUD CPU data
includes profiler overhead in the diagnostic APPX.

## Safe vs experimental memory switches

- Normal texture GC is the default and retains current tested cache-pressure
  thresholds. Critical/emergency recovery is unchanged.
- Experimental early texture GC lowers the age threshold for **idle clean**
  images from 120 frames to 45 frames, with at most 8 attempts and the existing
  1 ms per-frame time budget. It does not delete currently bound images, force
  texture uploads, or skip necessary guest write-back.
- D3D12 pipeline eviction defaults to On. Off can increase memory usage.
- JIT code memory ceiling can be selected as 64/128/192/256 MiB per emulated
  core. It does not persist compiled native code on disk and is not a 60 FPS hack.

Performance, stability and 3D compatibility remain **unverified on physical
original Xbox One** after these new options. Verify baseline first, then
change **one** experimental switch at a time. Restart the game for JIT ceiling,
pipeline eviction and settings that alter resource allocation; HUD fields and
size are live. Record build SHA, game, time until failure, FPS and diagnostic
logs for every comparison.
