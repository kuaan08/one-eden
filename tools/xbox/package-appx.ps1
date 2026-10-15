# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Stages, packs and signs the Phase-2 headless boot appx (src/eden_uwp) for sideloading
# onto an Xbox Series X|S in Dev Mode. See docs/xbox/xbox_deploy.md for the full flow.
#
#   .\tools\xbox\package-appx.ps1 -BootNro C:\path\to\boot.nro
#
# Produces build-uwp\package\eden-xbox.appx plus the .cer you must install on the console.

[CmdletBinding()]
param(
    # Directory the uwp-x64 preset built into (CMakePresets.json binaryDir).
    [string] $BuildDir = "build-uwp",
    # 'normal' and 'diagnostic' have separate package identities so they can be
    # installed side by side, keeping separate LocalState and profiler settings.
    # 'default' retains the upstream name and packaging behavior.
    [ValidateSet("default", "normal", "diagnostic")]
    [string] $Variant = "default",
    # Homebrew NRO bundled as the GATE-2 payload. It must emit the JIT-liveness sentinel
    # EDEN_XBOX_JIT_ALIVE via svcOutputDebugString. NO keys/firmware/commercial ROMs (house rule).
    [string] $BootNro,
    # For payloads without the sentinels (deko3d examples, ...): run the NRO this many seconds,
    # then shut down. 0 keeps the sentinel-driven boot. Written to boot.cfg in the package.
    [int] $RunSeconds = 0,
    # Enables the D3D12 debug layer (PC only: needs the Graphics Tools optional feature).
    [switch] $DebugLayer,
    # The user's own dumps for a game test, bundled under userdata\ and copied into LocalState by
    # the app on its first run (uwp_boot.cpp SeedUserData). They only ever go into this local
    # package: never commit them, never publish the appx. LocalState survives package updates, so
    # once seeded, later packages can pass just -Game <file name> without the file.
    #   -Keys      directory with prod.keys (and title.keys)
    #   -Firmware  directory with the firmware .nca files
    #   -Game      game dump (.nsp/.xci) to boot instead of boot.nro, or the name of one already seeded
    [string] $Keys,
    [string] $Firmware,
    [string] $Game,
    # Folder library for PC/Series; game files live under the app's LocalState\games.
    [switch] $Library,
    # Diagnostic builds: the in-process sampling profiler (boot.cfg profile=1). "full" also turns on
    # the per-draw GPU and per-core CPU timers, which slow the GPU thread. Never for a release.
    [ValidateSet("", "on", "full")]
    [string] $Profile = "",
    # Extra boot.cfg lines for diagnosis: "log_filter=*:Info HW.GPU:Debug", "renderer=null".
    [string[]] $BootCfg = @(),
    # Must match Identity/@Publisher in dist/uwp/AppxManifest.xml, character for character.
    [string] $PublisherCN = "CN=EdenXboxDev",
    # Mesa's SPIR-V -> DXIL translator for the D3D12 renderer, built by build-spirv-to-dxil.ps1.
    # Without it the renderer still presents, through its CPU fallback.
    [string] $SpirvToDxil = "..\mesa-build\build-uwp\src\microsoft\spirv_to_dxil\spirv_to_dxil.dll",
    # Require both shader DLLs in installable CI builds. Missing shader support
    # must stop packaging, not silently publish a broken release artifact.
    [switch] $RequireGpuShaders,
    # Compile minimal DXBC Shader Model 5.0 test shaders using fxc.exe from the
    # Windows SDK and ship them beside the executable for console PSO probes.
    # No DLL compiler is needed on the Xbox to execute the probe.
    [switch] $ShaderModelProbe,
    # Override the package identity version. CI passes an increasing value so
    # Device Portal can upgrade an existing installation (without deleting LocalState).
    [string] $PackageVersion = "",
    [string] $OutDir = "build-uwp\package"
)

$ErrorActionPreference = "Stop"
if ($Library) { $BootCfg += 'library=1'; $BootCfg += 'play=1' }
if ($Profile -eq "on") { $BootCfg += 'profile=1' }
if ($Profile -eq "full") { $BootCfg += 'profile=full' }
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $repo

function Find-SdkTool([string] $name) {
    $roots = @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "${env:ProgramFiles}\Windows Kits\10\bin")
    $hit = $roots | Where-Object { Test-Path $_ } | ForEach-Object {
        Get-ChildItem $_ -Recurse -Filter $name -ErrorAction SilentlyContinue |
            Where-Object { $_.DirectoryName -like '*\x64' }
    } | Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $hit) { throw "$name not found. Install the Windows 10/11 SDK (it ships MakeAppx + SignTool)." }
    return $hit.FullName
}

# --- 1. locate the built exe -------------------------------------------------------------
$exe = Join-Path $repo "$BuildDir\bin\eden-uwp.exe"
if (-not (Test-Path $exe)) {
    $found = Get-ChildItem (Join-Path $repo $BuildDir) -Recurse -Filter eden-uwp.exe -ErrorAction SilentlyContinue |
             Select-Object -First 1
    if (-not $found) {
        throw "eden-uwp.exe not found under $BuildDir. Build it first from a 'vcvarsall.bat x64 uwp' shell:`n" +
              "  cmake --preset uwp-x64`n  cmake --build --preset uwp-x64 --target eden-uwp"
    }
    $exe = $found.FullName
}
Write-Host "exe      : $exe"

# --- 2. stage the package layout ---------------------------------------------------------
$layout = Join-Path $repo "$OutDir\layout"
if (Test-Path $layout) { Remove-Item $layout -Recurse -Force }
New-Item -ItemType Directory -Path $layout -Force | Out-Null

Copy-Item $exe $layout
Copy-Item (Join-Path $repo "dist\uwp\AppxManifest.xml") $layout
Copy-Item (Join-Path $repo "dist\uwp\Assets") $layout -Recurse
# Use Eden's existing artwork in the launcher, preserving the upstream asset.
Copy-Item -LiteralPath (Join-Path $repo 'dist\qt_themes\default\icons\256x256\eden.png') `
    -Destination (Join-Path $layout 'Assets\EdenLogo.png')

# Any runtime DLLs the link produced land next to the exe; carry them along.
Get-ChildItem (Split-Path $exe) -Filter *.dll -ErrorAction SilentlyContinue |
    ForEach-Object { Copy-Item $_.FullName $layout }

# The D3D12 renderer's shader path: spirv_to_dxil.dll translates, dxil.dll (Windows SDK, freely
# redistributable) signs the DXIL. Both are loaded at runtime from the package root.
$s2d = if ([IO.Path]::IsPathRooted($SpirvToDxil)) { $SpirvToDxil } else { Join-Path $repo $SpirvToDxil }
if (Test-Path $s2d) {
    Copy-Item $s2d $layout
    Write-Host "shaders  : $s2d"
    try {
        $dxil = Find-SdkTool "dxil.dll"
        Copy-Item $dxil $layout
        Write-Host "signing  : $dxil"
    } catch {
        if ($RequireGpuShaders) { throw "Required dxil.dll is missing: $($_.Exception.Message)" }
        Write-Warning "dxil.dll not found in the Windows SDK: the D3D12 renderer will present through the CPU."
    }
} else {
    if ($RequireGpuShaders) {
        throw "Required spirv_to_dxil.dll not found ($s2d). Compile it with tools/xbox/build-spirv-to-dxil.ps1 before packaging."
    }
    Write-Warning "spirv_to_dxil.dll not found ($s2d): the D3D12 renderer will present through the CPU."
}

# On the Xbox One Fat the runtime shader compiler converts HLSL to DXBC
# Shader Model 5.1. The Windows 10 SDK redistributes d3dcompiler_47.dll
# for AppContainer/Store; never copy a random system or desktop DLL.
$compilerCandidates = @()
foreach ($base in @(${env:ProgramFiles(x86)}, ${env:ProgramFiles})) {
    if ($base) {
        $compilerCandidates += (Join-Path $base 'Windows Kits\10\Redist\D3D\x64\d3dcompiler_47.dll')
        $compilerCandidates += (Join-Path $base 'Windows Kits\8.1\Redist\D3D\x64\d3dcompiler_47.dll')
    }
}
$compilerRuntime = $compilerCandidates | Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if ($compilerRuntime) {
    Copy-Item -LiteralPath $compilerRuntime -Destination (Join-Path $layout 'd3dcompiler_47.dll') -Force
    Write-Host "Xbox SM5.1: bundled Windows SDK compiler $compilerRuntime"
} else {
    Write-Warning "Xbox SM5.1: no redistributable D3DCompiler_47 in SDK; the console must provide it."
}

if ($ShaderModelProbe) {
    $fxc = Find-SdkTool "fxc.exe"
    foreach ($stage in @(
        @{ Name = "vs"; Target = "vs_5_0" },
        @{ Name = "ps"; Target = "ps_5_0" }
    )) {
        $source = Join-Path $repo ("tools\\xbox\\shader-probe\\sm5_" + $stage.Name + ".hlsl")
        $compiled = Join-Path $layout ("eden_probe_" + $stage.Name + ".cso")
        & $fxc /nologo /E main /T $stage.Target /Fo $compiled $source
        if ($LASTEXITCODE -ne 0 -or !(Test-Path $compiled)) {
            throw "FXC compilation failed for $($stage.Target), source $source"
        }
        Write-Host "SM5 probe: $compiled SHA256=$((Get-FileHash $compiled -Algorithm SHA256).Hash)"
    }
}

# User data (keys, firmware, game): see the parameters above.
$userdata = Join-Path $layout "userdata"
if ($Keys) {
    New-Item -ItemType Directory -Force "$userdata\keys" | Out-Null
    foreach ($name in @("prod.keys", "title.keys")) {
        $k = Join-Path $Keys $name
        if (Test-Path -LiteralPath $k) { Copy-Item -LiteralPath $k "$userdata\keys" }
    }
    if (-not (Test-Path "$userdata\keys\prod.keys")) { throw "prod.keys not found in $Keys" }
    Write-Host "keys     : $Keys"
}
if ($Firmware) {
    $ncas = Get-ChildItem -LiteralPath $Firmware -Filter *.nca -File
    if (-not $ncas) { throw "no .nca files in $Firmware" }
    New-Item -ItemType Directory -Force "$userdata\firmware" | Out-Null
    $ncas | ForEach-Object { Copy-Item -LiteralPath $_.FullName "$userdata\firmware" }
    Write-Host "firmware : $($ncas.Count) NCAs from $Firmware"
}
$gameName = $null
if ($Game) {
    $gameName = Split-Path -Leaf $Game
    if (Test-Path -LiteralPath $Game -PathType Leaf) {
        New-Item -ItemType Directory -Force "$userdata\games" | Out-Null
        Copy-Item -LiteralPath $Game "$userdata\games\$gameName"
        Write-Host "game     : $Game"
    } else {
        Write-Host "game     : $gameName (expected already in LocalState\games)"
    }
}

# uwp_boot.cpp reads Package.InstalledLocation\boot.nro - the payload must sit at the layout root.
if ($BootNro -or $gameName -or $BootCfg.Count -gt 0) {
    if ($BootNro) {
        if (-not (Test-Path $BootNro)) { throw "BootNro not found: $BootNro" }
        Copy-Item $BootNro (Join-Path $layout "boot.nro")
        Write-Host "payload  : $BootNro -> boot.nro"
    }
    $cfg = ""
    if ($gameName) {
        $cfg += "game=$gameName`n"
    }
    # powershell -File hands "-BootCfg a=1,b=2" over as one string: split it here.
    foreach ($line in ($BootCfg | ForEach-Object { $_ -split ',' } | Where-Object { $_ })) {
        $cfg += "$line`n"
        Write-Host "boot.cfg : $line"
    }
    if ($RunSeconds -gt 0) {
        $cfg += "run_seconds=$RunSeconds`n"
        Write-Host "mode     : run $RunSeconds s (no sentinels)"
    }
    if ($DebugLayer) {
        $cfg += "debug_layer=1`n"
        Write-Host "debug    : D3D12 debug layer on"
    }
    if ($cfg) {
        [IO.File]::WriteAllText((Join-Path $layout "boot.cfg"), $cfg)
    }
} else {
    Write-Warning "No -BootNro given. The app will activate, fail to load the NRO and log status 2 to eden_uwp_diag.txt."
}

# Distinct package identities let the normal and diagnostic builds coexist.
# The original manifest remains unchanged in the repository.
$stagedManifest = Join-Path $layout "AppxManifest.xml"
[xml]$mf = Get-Content $stagedManifest -Raw
if ($Variant -ne "default") {
    $identity = $mf.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Identity']")
    $propsName = $mf.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Properties']/*[local-name()='DisplayName']")
    $visuals = $mf.SelectSingleNode("//*[local-name()='VisualElements']")
    if (-not $identity -or -not $propsName -or -not $visuals) {
        throw "Expected identity/display-name elements missing from AppxManifest.xml."
    }
    $displayName = if ($Variant -eq "diagnostic") { "Eden Xbox Diagnostic" } else { "Eden Xbox" }
    $identityName = if ($Variant -eq "diagnostic") { "EdenEmuProject.EdenXboxDiagnostic" } else { "EdenEmuProject.EdenXbox" }
    $identity.SetAttribute("Name", $identityName)
    $propsName.InnerText = $displayName
    $visuals.SetAttribute("DisplayName", $displayName)
    $mf.Save($stagedManifest)
    Write-Host "variant  : $Variant ($identityName)"
}
# The repository manifest stays at its baseline. CI versions each package so the
# console can reliably replace an existing installation while retaining LocalState.
if ($PackageVersion) {
    $parts = $PackageVersion.Split('.')
    if ($parts.Count -ne 4) { throw "APPX version needs four numeric components." }
    foreach ($part in $parts) {
        if ($part -notmatch '^[0-9]+$') { throw "APPX version contains non-numeric components." }
        if ([long]$part -gt 65535) { throw "APPX version component exceeds 65535." }
    }
    $versionIdentity = $mf.SelectSingleNode("/*[local-name()='Package']/*[local-name()='Identity']")
    if (-not $versionIdentity) { throw "APPX manifest Identity not found." }
    $versionIdentity.SetAttribute("Version", $PackageVersion)
    $mf.Save($stagedManifest)
    Write-Host "appx version: $PackageVersion"
}
# Publisher must match the signing certificate's subject.
if ($mf.Package.Identity.Publisher -ne $PublisherCN) {
    throw "Publisher mismatch: manifest has '$($mf.Package.Identity.Publisher)' but signing with '$PublisherCN'."
}

# --- 3. pack -----------------------------------------------------------------------------
$makeappx = Find-SdkTool "MakeAppx.exe"
$packageName = if ($Variant -eq "default") { "eden-xbox.appx" } else { "eden-xbox-$Variant.appx" }
$appx = Join-Path $repo (Join-Path $OutDir $packageName)
if (Test-Path $appx) { Remove-Item $appx -Force }
# A bundled game dump is gigabytes of encrypted data that does not compress: skip compression.
$packArgs = @("pack", "/d", $layout, "/p", $appx, "/o")
if (Test-Path "$userdata\games") { $packArgs += "/nc" }
& $makeappx @packArgs
if ($LASTEXITCODE -ne 0) { throw "MakeAppx failed ($LASTEXITCODE)." }

# --- 4. sign -----------------------------------------------------------------------------
# Reuse a matching cert if one is already in the user store, else mint a self-signed one.
$cert = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq $PublisherCN -and $_.NotAfter -gt (Get-Date) -and $_.HasPrivateKey } |
        Select-Object -First 1
if (-not $cert) {
    Write-Host "minting a self-signed code-signing cert for $PublisherCN"
    $cert = New-SelfSignedCertificate -Type Custom -Subject $PublisherCN `
        -KeyUsage DigitalSignature -FriendlyName "Eden Xbox sideload" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
}

$signtool = Find-SdkTool "SignTool.exe"
& $signtool sign /fd SHA256 /sha1 $cert.Thumbprint /tr http://timestamp.digicert.com /td SHA256 $appx
if ($LASTEXITCODE -ne 0) {
    # Offline CI runners can fail timestamping. A locally signed development
    # package is still useful; the certificate expires normally without a stamp.
    Write-Warning "RFC3161 timestamp failed; retrying the developer signature without timestamp."
    & $signtool sign /fd SHA256 /sha1 $cert.Thumbprint $appx
    if ($LASTEXITCODE -ne 0) { throw "SignTool failed even without timestamp ($LASTEXITCODE)." }
}

# The console must trust the signer: upload this .cer alongside the appx in the Device Portal.
$cer = Join-Path $repo "$OutDir\eden-xbox.cer"
Export-Certificate -Cert $cert -FilePath $cer -Type CERT | Out-Null

# --- 5. the VCLibs framework package ------------------------------------------------------
# The exe hard-imports the Store CRT, which lives in the Microsoft.VCLibs.140.00 framework
# package rather than ours (see the PackageDependency in the manifest). The console needs it
# installed too, so hand it over next to our package instead of leaving the user to find it.
# It ships with the VS "C++ (v143) UWP tools" component, as an Extension SDK.
$vclibs = $null
foreach ($pf in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
    if (-not $pf) { continue }
    $cand = Join-Path $pf "Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.appx"
    if (Test-Path -LiteralPath $cand) { $vclibs = $cand; break }
}
if ($vclibs) {
    $vcOut = Join-Path $repo "$OutDir\Microsoft.VCLibs.x64.14.00.appx"
    Copy-Item -LiteralPath $vclibs -Destination $vcOut -Force
} else {
    Write-Warning ("Microsoft.VCLibs.x64.14.00.appx not found. The app will FAIL TO ACTIVATE on-console " +
                   "without it. Install the VS component Microsoft.VisualStudio.ComponentGroup.UWP.VC.")
}

Write-Host ""
Write-Host "package  : $appx"
Write-Host "cert     : $cer"
if ($vclibs) { Write-Host "framework: $vcOut  (upload as a dependency package)" }
Write-Host "Next: Device Portal https://<xbox-ip>:11443 -> Add -> upload all of the above -> set the app to Game mode."
