param([Parameter(Mandatory=$true)][string]$Directory,[string]$ResultFile='',[switch]$HashesOnly)
$ErrorActionPreference = 'Stop'
$taskPackage = [IO.Path]::GetFullPath($Directory).TrimEnd('\','/')
$taskManifest = Get-Content -LiteralPath (Join-Path $taskPackage 'package.json') -Raw | ConvertFrom-Json
if ($taskManifest.format -ne 'shardbound-package' -or $taskManifest.version -ne 1) { throw 'Invalid package manifest' }
foreach ($taskEntry in $taskManifest.files.PSObject.Properties) {
    $taskPath = [IO.Path]::GetFullPath((Join-Path $taskPackage $taskEntry.Name))
    if (-not $taskPath.StartsWith($taskPackage+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Package path escaped its directory' }
    $taskItem = Get-Item -LiteralPath $taskPath -ErrorAction Stop
    if ($taskItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Package contains a linked file' }
    if ((Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskEntry.Value) { throw "Package hash mismatch: $($taskEntry.Name)" }
}
foreach ($taskRequired in @('game.exe','modcheck.exe','shardbound_editor.exe','editor-studio.mjs','editor-studio-web.js','editor-blockbench.js','SDL3.dll','DirectXTex.dll','fastgltf.dll','simdjson.dll','Jolt.dll','msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll','content/README.md','assets/core/shaders/forward.hlsl','sdk/native/shardbound.h')) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskPackage $taskRequired) -PathType Leaf)) { throw "Runtime dependency missing: $taskRequired" }
}
$taskMods = @(Get-ChildItem -LiteralPath $taskPackage -Recurse -Filter mod.json -File)
if ($taskMods.Count) { throw 'The clean package contains a bundled mod' }
foreach ($taskRetired in @('assets/core/audio','assets/core/models','assets/core/textures')) {
    if (Test-Path -LiteralPath (Join-Path $taskPackage $taskRetired)) { throw "Bundled game assets found: $taskRetired" }
}
if ($HashesOnly) { Write-Output 'Archive hashes verified.'; return }
$taskEvidenceRoot = [IO.Path]::GetTempPath()
if ($ResultFile) { $taskEvidenceRoot = Split-Path -Parent ([IO.Path]::GetFullPath($ResultFile)) }
$taskRun = Join-Path $taskEvidenceRoot ('portable-check-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskRun | Out-Null
$taskUser = Join-Path $taskRun 'user'
$taskOriginalPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot;$taskPackage"
    $taskCheck = Start-Process -FilePath (Join-Path $taskPackage 'modcheck.exe') -ArgumentList ('"'+(Join-Path $taskPackage 'content')+'" --json') -WorkingDirectory $taskRun -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $taskRun 'modcheck.json') -RedirectStandardError (Join-Path $taskRun 'modcheck.err')
    if ($taskCheck.ExitCode -ne 0 -or -not (Get-Content -LiteralPath (Join-Path $taskRun 'modcheck.json') -Raw | ConvertFrom-Json).valid) { throw "Portable mod validation failed. Logs: $taskRun" }
    $taskEditor = Start-Process -FilePath (Join-Path $taskPackage 'shardbound_editor.exe') -ArgumentList '--smoke-empty' -WorkingDirectory $taskRun -WindowStyle Hidden -PassThru
    if (-not $taskEditor.WaitForExit(30000)) { Stop-Process -Id $taskEditor.Id; throw 'Portable editor did not finish its smoke test' }
    if ($taskEditor.ExitCode -ne 0) { throw "Portable editor failed: $($taskEditor.ExitCode)" }
    $taskModsReport = Get-Content -LiteralPath (Join-Path $taskRun 'modcheck.json') -Raw | ConvertFrom-Json
    if ($taskModsReport.mods -ne 0) { throw 'Expected zero default mods' }
    $taskCommon = '--hidden --no-vsync --fps 0 --frames 30 --user-dir "'+$taskUser+'"'
    $taskGame = Start-Process -FilePath (Join-Path $taskPackage 'game.exe') -ArgumentList $taskCommon -WorkingDirectory $taskRun -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $taskRun 'game.log') -RedirectStandardError (Join-Path $taskRun 'game.err')
    $taskLog = Get-Content -LiteralPath (Join-Path $taskRun 'game.log') -Raw
    $taskErrors = Get-Content -LiteralPath (Join-Path $taskRun 'game.err') -Raw
    if ($taskGame.ExitCode -ne 0 -or !$taskLog.Contains('Runtime initialized') -or $taskLog.Contains('World ready:') -or $taskLog.Contains('[ERROR]') -or ($taskErrors -and $taskErrors.Contains('[ERROR]')) -or (Test-Path -LiteralPath (Join-Path $taskUser 'worlds'))) { throw "Empty runtime did not start cleanly. Logs: $taskRun" }
    if (-not $ResultFile) { $ResultFile = Join-Path $taskRun 'verification.json' }
    $taskResult = [ordered]@{ format='shardbound-package-verification'; version=1; created_utc=[DateTimeOffset]::UtcNow.ToString('o'); package=$taskPackage; restricted_path=$true; fresh_windows_vm=$false; bundled_mods=$taskModsReport.mods; modcheck_exit=$taskCheck.ExitCode; editor_exit=$taskEditor.ExitCode; game_exit=$taskGame.ExitCode; logs=$taskRun }
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($ResultFile),($taskResult | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
    Write-Output "Portable runtime verified. Evidence: $ResultFile"
} finally { $env:PATH = $taskOriginalPath }
