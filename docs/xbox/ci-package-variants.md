# CI artifacts: Normal and Diagnostic UWP packages

The **Xbox UWP - Normal and Diagnostic APPX** workflow packages a previously
successful WindowsStore/UWP x64 executable into two signed Developer Mode APPX
packages. It downloads the executable from GitHub Actions run
[38510035231](https://github.com/kuaan08/Switch/actions/runs/38510035231)
(commit `29d1bdcc31fba4fbd574c76d2a7bc5ab7d43b1dd`), instead of rebuilding
the entire emulator for every packaging change.

The signed files are **development test builds, not validated Xbox One Fat
releases**. The upstream project targets Xbox Series X|S; an original Xbox One
must still pass activation, CPU/JIT, D3D12 and performance checks.

- `Eden-Xbox-UWP-Normal-APPX`: `eden-xbox-normal.appx`, `eden-xbox.cer`,
  optionally `Microsoft.VCLibs.x64.14.00.appx`. Runs library UI with
  sampling profiler disabled.
- `Eden-Xbox-UWP-Diagnostic-APPX`: `eden-xbox-diagnostic.appx`,
  `eden-xbox.cer`, optionally VCLibs. Includes `profile=1` in
  `boot.cfg`, generating sampling reports under LocalState/eden/log.
  Detailed per-draw GPU profiling remains disabled to avoid unnecessary
  overhead.

Both APPXs are **signed** with an ephemeral CI-generated self-signed cert.
The workflow produces the public `.cer` for device trust; never publishes
the signing private key. The certificate will be regenerated on future CI
runs, so update/sideload behavior may require uninstalling old test packages
and trusting the new signer.

Install through Xbox Device Portal in **Developer Mode**, trusting the
`.cer` and providing the VCLibs x64 framework package if required. Set
**Game mode**. No games, keys, firmware or copyrighted content are included.
The emulator has not been tested on physical Xbox One (2013).

The package script's `-Variant default` retains the upstream behavior,
including original package identity and name. `normal` and `diagnostic`
use different identities so they can coexist, with independent LocalState.

**Shader dependency gate:** earlier packages from run 38567579994 were
missing Mesa's `spirv_to_dxil.dll`. In the updated workflow, the
`build-spirv-to-dxil.ps1` script compiles a WindowsStore-compatible DLL from
Mesa, packaging requires that DLL and `dxil.dll`, and unpack validation
checks both. If Mesa compilation or shader packaging fails, GitHub Actions
fails instead of uploading incomplete packages. The compiler executable
`eden-uwp.exe` is still reused from run 38510035231 (same source code).

**Hardware caveat:** two shader DLLs are necessary for the intended D3D12
shader path, but do not prove that the Xbox One Fat 2013 GPU supports every
required feature level, shader operation or allocation strategy. Console logs
and testing are still required.
