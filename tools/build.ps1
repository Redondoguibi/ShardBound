param([ValidateSet('debug', 'release')][string]$Preset = 'debug', [string]$TestRegex = '',[string[]]$Targets = @())
$ErrorActionPreference = 'Stop'
if (-not $env:CMAKE_BUILD_PARALLEL_LEVEL) { $env:CMAKE_BUILD_PARALLEL_LEVEL = [string][Math]::Min(4,[Environment]::ProcessorCount) }
$root = Split-Path -Parent $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Install the Visual Studio C++ x64 toolset and Windows SDK.' }
& (Join-Path $vs 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
if (-not $env:VCPKG_ROOT -and (Test-Path "$root/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake")) {
    $env:VCPKG_ROOT = "$root/.tools/vcpkg"
}
if (-not $env:VCPKG_ROOT) { throw 'Set VCPKG_ROOT to a bootstrapped vcpkg checkout.' }
$env:PATH = "$root/.tools/python/cmake/data/bin;$root/.tools/python/bin;$env:PATH"
Push-Location $root
try {
    & cmake --preset $Preset
    if ($LASTEXITCODE) { throw 'Configure failed' }
    if ($Targets.Count) { & cmake --build --preset $Preset --target $Targets }
    else { & cmake --build --preset $Preset }
    if ($LASTEXITCODE) { throw 'Build failed' }
    if ($TestRegex) { & ctest --preset $Preset -R $TestRegex }
    else { & ctest --preset $Preset }
    if ($LASTEXITCODE) { throw 'Tests failed' }
} finally { Pop-Location }
