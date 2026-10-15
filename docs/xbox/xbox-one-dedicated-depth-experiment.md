# Xbox One Fat: experimental dedicated depth texture allocation

The October 14 hardware logs of *A Short Hike* show that SM5.1 graphics shaders
compile and the first D3D12 graphics PSO is created, followed by a native driver
access violation in `umd12ddi.dll+0x3732d` (read from address `0x78`).
The final complete resource checkpoint is a successful `CreatePlacedResource`
for the first 1920x1080 typeless depth/stencil image, created in a shared
64 MiB render/depth texture heap.

## Experiment

On a GPU reporting Shader Model lower than 6.0 (the original Xbox One Fat), the
texture allocator now allocates depth/stencil resources as **committed resources**
instead of using its normal pooled placed-resource path.

- This is a focused compatibility experiment, **not** a confirmed crash fix.
- It does not alter color buffers, other textures, shaders, game presentation,
  or the previously successful SM5.1 upside-down image correction.
- Dedicated depth textures can use **more memory**; re-evaluate before enabling
  as a permanent default or shipping a release.
- The ordinary OOM reclamation path of the allocator remains available.
- Additional `D3D12 DEPTH-PROBE` logs bracket `CheckRemovedAfter` and
  `GetCopyableFootprints`, which are executed immediately after texture creation.

## Xbox hardware test

Use the current `one-eden/main` SM5.1 diagnostic APPX. Confirm *Sonic Mania*
still renders upright, then try *A Short Hike*. If the game closes, export only
`eden_uwp_diag.txt` and `eden/log/eden_log.txt`; remove all keys and game
content from shared diagnostics.

In the new log look for:

```text
D3D12 AV-PROBE RESOURCE #N SM5.1 dedicated depth allocation enabled
D3D12 AV-PROBE RESOURCE #N BEGIN CreateCommittedResource
D3D12 AV-PROBE RESOURCE #N END CreateCommittedResource ...
D3D12 DEPTH-PROBE BEGIN CheckRemovedAfter ...
D3D12 DEPTH-PROBE END CheckRemovedAfter
D3D12 DEPTH-PROBE BEGIN GetCopyableFootprints ...
D3D12 DEPTH-PROBE END GetCopyableFootprints ...
D3D12 AV-PROBE VIEW #N BEGIN CreateDepthStencilView ...
```

The final **BEGIN without END** may reveal which D3D12 driver-facing
operation triggers the access violation. If committed depth resources still
fail, investigate view descriptors and GPU command sequencing next.
