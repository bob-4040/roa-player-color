param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = Join-Path $ProjectRoot "build"
$CMake = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source

if (-not $CMake) {
    $VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $VsWhere)) {
        throw "Visual Studio Installer (vswhere.exe) was not found."
    }

    $VisualStudio = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $VisualStudio) {
        throw "Visual Studio 2022 with the x86/x64 C++ build tools was not found."
    }

    $CMake = Join-Path $VisualStudio "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}

if (-not (Test-Path -LiteralPath $CMake)) {
    throw "CMake was not found: $CMake"
}

& $CMake -S $ProjectRoot -B $BuildDirectory -G "Visual Studio 17 2022" -A Win32
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

& $CMake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

$Dll = Join-Path $BuildDirectory "$Configuration\player-color.dll"
& (Join-Path $PSScriptRoot "verify_dll.ps1") -Path $Dll

$RegistrySmoke = Join-Path $BuildDirectory "$Configuration\rwp-registry-smoke.exe"
& $RegistrySmoke
if ($LASTEXITCODE -ne 0) {
    throw "Registry smoke test failed with exit code $LASTEXITCODE."
}

$ColorMappingSmoke = Join-Path $BuildDirectory "$Configuration\rwp-color-mapping-smoke.exe"
& $ColorMappingSmoke
if ($LASTEXITCODE -ne 0) {
    throw "Color mapping smoke test failed with exit code $LASTEXITCODE."
}

$AnimationSmoke = Join-Path $BuildDirectory "$Configuration\rwp-animation-smoke.exe"
& $AnimationSmoke
if ($LASTEXITCODE -ne 0) {
    throw "Animation smoke test failed with exit code $LASTEXITCODE."
}

$SmokeTest = Join-Path $BuildDirectory "$Configuration\rwp-load-smoke.exe"
& $SmokeTest $Dll
if ($LASTEXITCODE -ne 0) {
    throw "DLL smoke test failed with exit code $LASTEXITCODE."
}

Write-Host "Build completed: $Dll"
