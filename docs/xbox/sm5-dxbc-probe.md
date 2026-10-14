# Xbox One Fat: first Shader Model compatibility probe

Previous on-console logs: SraKmd adapter reports D3D12 Shader Model 5.1,
while the Eden Mesa DXIL translator is hardcoded to target Shader Model 6.4.
The spirv_to_dxil.dll now loads, but CreateGraphicsPipelineState returns
0x80004005 for thousands of draws.

This change DOES NOT claim a SM5-compatible Switch graphics renderer.
It introduces an isolated D3D12 pipeline creation diagnostic:
1. When device caps report a shader model below 6.0, try loading
   eden_probe_vs.cso and eden_probe_ps.cso from the installed APPX directory.
2. Those are simple DXBC Shader Model 5.0 shaders compiled OFFLINE using
   the Windows SDK fxc.exe, so the console does not need d3dcompiler_47.dll.
3. Create a minimal graphics pipeline with those shaders, no bound resources,
   a trivial root signature and RGBA8 target. Then try vertex-only PSO.
4. Log two precise SUCCESS/FAILED HRESULT lines:
   - D3D12 SM5 probe: minimal DXBC VS+PS graphics PSO -> ...
   - D3D12 SM5 probe: vertex-only DXBC graphics PSO -> ...

If a simple DXBC PSO succeeds while Eden's SM6.4 DXIL game pipelines fail,
the shader model/backend mismatch becomes a substantially better-founded
hypothesis. If DXBC fails too, device/driver, root signature and PSO state need
further investigation. No draws are submitted by this probe.

The 'Xbox One Fat - UWP x64 build attempt' workflow now also packages the
diagnostic APPX after a successful native UWP build. It reuses the proven
shader translator from run 38597070329 to avoid recompiling Mesa. Its APPX
artifact is 'Eden-Xbox-OneFat-SM5-DXBC-Probe-APPX', containing the installable
APPX, a new development signing certificate and Microsoft.VCLibs.x64.14.00.
Neither game data nor console-specific keys are included.
