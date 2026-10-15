# Xbox One cross-emulator D3D12 depth experiments — 15 Oct 2026

The user supplied two **byte-identical** packages named
`CEMU_XBOX_ONE_UWP_COMPLETO_2026-10-15.skill` and `.zip`
(SHA-256 `5424f3ab45fa915edfd8fe14f50d0c7ed254c94f16f8df8b6c35339bbfe0ff66`).
They document the Cemu Xbox One PrismFox/SM5.1 crash while starting
*NES Remix Pack* and describe experiments with depth resources and DRED.

## Shared observations, without asserting a shared root cause

- Cemu Xbox One: a driver-side access violation at
  `umd12ddi.dll+0x3732d`, with success (`S_OK`) after creating two
  **committed** resources for the first 1920x1080 color and depth images.
- Eden Xbox One: *A Short Hike* crashes at `umd12ddi.dll+0x3732d`
  with an access violation reading near `0x78`, after success creating
  the 1920x1080 typeless depth resource in a **placed** heap.
- The common driver-offset is a diagnostic clue, **not** proof of
  driver defect, identical cause, or exact failing API call. A native
  GPU driver can crash asynchronously on an earlier invalid descriptor.
- Both emulators use **different** guest GPUs and video implementations.
  Cemu GX2 shader/texture implementations cannot be directly merged
  into the Switch NVN/Tegra backend.

## Implemented A/B experiments (opt-in, Xbox One SM5.1 only)

In **Settings → System → 3D driver experiments**:

| Mode | Meaning | Controls |
| --- | --- | --- |
| Normal (default) | Baseline post-resource device query and DSV array descriptor | None |
| Skip post-depth driver query | Bypasses the extra `CheckRemovedAfter` device query **only** after successfully creating a depth image; the normal HRESULT/error checks remain | Bit 1 |
| Single-layer depth view (2D) | Uses `D3D12_DSV_DIMENSION_TEXTURE2D` instead of a one-slice `TEXTURE2DARRAY` DSV if and only if the underlying 2D image is one layer, mip-appropriate and non-MSAA | Bit 2 |
| Both experiments | Enables both tests for comparison only | Both bits |

**The old baseline is the default**, to preserve 2D games and to avoid
promising an unverified 3D fix. Changing the mode requires closing and
restarting the **game**. These probes do not change the shader model,
presentation orientation, 2D textures, render target color descriptors,
or normal/diagnostic APPX identities.

For the DSV branch, normal and read-only DSVs share the same chosen
dimension. Logs contain `D3D12 DEPTH-PROBE` bounded markers. The skip
experiment bypasses a *diagnostic* query, not device creation failure
detection; this is still potentially risky, and only for focused tests.

## Reproducible next test

1. On the Xbox One Fat with the **diagnostic** APPX, test Sonic Mania
   with mode Normal and confirm controls/orientation still work.
2. Run A Short Hike with mode Normal and record the final depth marker,
   `D3D12 AV-PROBE VIEW`, and the first exception.
3. Repeat separately with SkipQuery and SingleDsv, **restarting the game**
   after each setting. Record the final BEGIN/END before the crash.
4. Only if one of these changes improves the first-frame behavior,
   run Both, then retest Sonic Mania and memory usage.
5. Report shaders used (SM5.1 DXBC), GPU driver metadata and DRED when
   supported. Do not assume a low GPU memory budget or WARP without a
   DXGI adapter/capabilities report.
6. If all fail at identical addresses, prioritize native crash dumps
   and CPU thread stacks and a minimal D3D12 depth/DSV smoke test; do not
   stack additional speculative hacks.

**Cemu-only material not copied:** D3D11 FL10.1/SM4.1 Wii U GX2
renderer; `OpenFromCallbacks` ZArchive fix (a Cemu compile issue);
PPC JIT, Wii U title loaders, and shaders requiring different guest
semantics.

Microsoft documentation: `ID3D12Device::GetDeviceRemovedReason` is a
device status query, not a mandatory texture-creation API:
https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getdeviceremovedreason
