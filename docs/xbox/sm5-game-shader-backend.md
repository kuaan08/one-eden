# Xbox One Fat: experimental Shader Model 5.1 game shader backend

**Status: source implementation committed, NOT console-validated.**

Hardware observations from October 2026 logs:

* The Xbox One Fat D3D12 adapter reports Shader Model 5.1.
* Eden's previous Mesa SPIR-V-to-DXIL path targeted SM6.4; console rejects
  game graphics/compute pipelines with HRESULT 0x80004005.
* An isolated DXBC Shader Model 5.0 graphics pipeline (with and without PS)
  succeeded on this same console. This does NOT prove game shaders work.

This experimental build adds a new shader translation route, selected when
D3D12 reports less than Shader Model 6.0:

  Nintendo guest shader -> Eden SPIR-V -> SPIRV-Cross HLSL SM5.1 ->
  D3DCompile -> DXBC -> D3D12 graphics/compute pipeline.

The existing DXIL backend is preserved for consoles capable of SM6+. The
translator is pinned to Khronos SPIRV-Cross revision
300681bbcb8365a8d1eacdacdbdd0249bbf1d6b1 and built statically into the
UWP frontend.

Remaining limitations to validate and improve:

* SPIRV-Cross does not perform Mesa's linked-pipeline processing; adjacent
  stage signature mismatches can still produce PSO failures.
* The Mesa-specific RuntimeData SV semantics, conditional clip-space Y flip,
  integer texture lowering and some Vulkan features are not yet reproduced.
* UWP must be able to load d3dcompiler_47.dll; packages try to use the official
  Windows SDK redist (otherwise the Xbox OS must supply it).
* This does NOT grant full Switch compatibility. Games may still show black
  frames or fail shader compilation.

Log checkpoints:

* D3D12 SM5.1: experimental SPIR-V -> SPIRV-Cross HLSL -> DXBC backend ACTIVATED
* D3D12 SM5.1: ... compiled ... bytes DXBC ...
* If shader translation fails, errors describe the stage and compiler issue.

The main experimental Actions workflow 'Xbox One Fat - UWP x64 build attempt'
compiles the changed source and packages a diagnostic APPX. A successful CI
run only proves compilation/packaging; it does not prove playable graphics.
