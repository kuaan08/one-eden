# Original Xbox One / SM5.1: vertically inverted game output

## Symptom

The experimental SPIR-V -> SPIRV-Cross HLSL SM5.1 -> DXBC pipeline
successfully renders CLU CLU LAND and Sonic Mania on the original
Xbox One (2013), but the **game content is upside down** while the FPS
overlay and frontend UI remain upright.

## Root cause hypothesis

The original Mesa SPIR-V -> DXIL pipeline configures Vulkan-to-D3D
Y-coordinate translation through `PipelineOptions.yz_flip` and guest
viewport runtime data. The experimental `ShaderCompiler::CompileSm5`
path currently does **not** apply that translation to graphics stages.
This is consistent with the observed inverted GPU image, but the exact
relationship with every guest viewport still needs on-console testing.

## Targeted correction

This change is deliberately confined to the **accelerated game
framebuffer's final composite** in `d3d12_present_blit.cpp`.

- `ShaderCompiler::IsSm5Active()` selects the correction only when the
  SM5.1 backend is active.
- `CorrectXboxOneGuestCrop` exchanges the normalized crop's `top` and
  `bottom` only for that GPU path.
- The same corrected crop feeds the bilinear fullscreen blit and FSR EASU.
  `Fsr::MakePlan` already handles signed crop heights.
- FPS overlay, shader notification, game menu and independent UWP
  frontend are composed **after** the blit; their orientation is unchanged.
- CPU fallback already flips/crops its own staging image and remains
  untouched. The modern DXIL shader path remains untouched.

The new log line on a successful accelerated present is:

```text
D3D12 SM5.1: Xbox One guest image vertical orientation correction ACTIVE at GPU composite (FPS overlay and menus unchanged, FSR aware)
```

A small source-level C++20 regression test is at
`tools/xbox/tests/xbox-one-present-orientation.cpp`, covering the
normal crop, flipped crop, no-op for other backends and reversibility.

## Hardware test

Build the newest `one-eden/main` signed diagnostic APPX, install it
in Xbox Developer Mode and first test a previously rendering 2D game.

1. Verify the game appears upright while FPS/menus stay upright.
2. Verify colors, crop and aspect ratio stay correct.
3. If FSR is available, compare bilinear and FSR output orientation.
4. Send `eden_uwp_diag.txt` and `eden/log/eden_log.txt` if the game
   remains upside down or shows a new orientation error.

**Status:** implementation committed; image orientation and any 3D
compatibility improvements **are not verified on physical hardware yet**.
This change does not attempt to fix the A Short Hike driver crash.
