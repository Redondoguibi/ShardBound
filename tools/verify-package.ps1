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
foreach ($taskRequired in @('game.exe','modcheck.exe','shardbound_editor.exe','editor-studio.mjs','editor-blockbench.js','SDL3.dll','DirectXTex.dll','fastgltf.dll','simdjson.dll','Jolt.dll','msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll','content/foundation/mod.json','content/plains/mod.json','assets/core/shaders/forward.hlsl','sdk/native/shardbound.h')) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskPackage $taskRequired) -PathType Leaf)) { throw "Runtime dependency missing: $taskRequired" }
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
    $taskEditor = Start-Process -FilePath (Join-Path $taskPackage 'shardbound_editor.exe') -ArgumentList '--smoke' -WorkingDirectory $taskRun -WindowStyle Hidden -PassThru
    if (-not $taskEditor.WaitForExit(20000)) { Stop-Process -Id $taskEditor.Id; throw 'Portable editor did not finish its smoke test' }
    if ($taskEditor.ExitCode -ne 0) { throw "Portable editor failed: $($taskEditor.ExitCode)" }
    $taskCommon = '--hidden --no-vsync --fps 0 --frames 120 --performance --user-dir "'+$taskUser+'"'
    $taskCreate = Start-Process -FilePath (Join-Path $taskPackage 'game.exe') -ArgumentList ($taskCommon+' --create-world "Teste portatil" --seed "portable-verification"') -WorkingDirectory $taskRun -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $taskRun 'create.log') -RedirectStandardError (Join-Path $taskRun 'create.err')
    if ($taskCreate.ExitCode -ne 0) { throw "Portable create failed: $($taskCreate.ExitCode). Logs: $taskRun" }
    $taskWorlds = @(Get-ChildItem -LiteralPath (Join-Path $taskUser 'worlds') -Directory)
    if ($taskWorlds.Count -ne 1) { throw 'Portable run did not create exactly one world' }
    $taskCheckpoint = Get-Content -LiteralPath (Join-Path $taskWorlds[0].FullName 'checkpoint.json') -Raw | ConvertFrom-Json
    if (-not $taskCheckpoint.player.entity -or @($taskCheckpoint.entities).Count -lt 1) { throw 'Portable checkpoint has no playable entity' }
    $taskResume = Start-Process -FilePath (Join-Path $taskPackage 'game.exe') -ArgumentList ($taskCommon+' --open-world '+$taskWorlds[0].Name) -WorkingDirectory $taskRun -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $taskRun 'resume.log') -RedirectStandardError (Join-Path $taskRun 'resume.err')
    if ($taskResume.ExitCode -ne 0) { throw "Portable resume failed: $($taskResume.ExitCode). Logs: $taskRun" }
    foreach ($taskPhase in @('create','resume')) {
        $taskLog = Get-Content -LiteralPath (Join-Path $taskRun ($taskPhase+'.log')) -Raw
        $taskErrors = Get-Content -LiteralPath (Join-Path $taskRun ($taskPhase+'.err')) -Raw
        if (-not $taskLog.Contains('[WORLD][INFO] World ready:') -or $taskLog.Contains('[ERROR]') -or ($taskErrors -and $taskErrors.Contains('[ERROR]'))) { throw "Portable $taskPhase did not run clean gameplay. Logs: $taskRun" }
    }
    $taskReports = @(Get-ChildItem -LiteralPath (Join-Path $taskUser 'performance') -Filter '*.json')
    if ($taskReports.Count -lt 2) { throw 'Performance reports were not generated' }
    foreach ($taskReport in $taskReports) { $taskData = Get-Content -LiteralPath $taskReport.FullName -Raw | ConvertFrom-Json; if ($taskData.window_frames -lt 20 -or @($taskData.frames | Where-Object gameplay).Count -lt 20 -or $null -eq $taskData.metrics.gpu -or $taskData.metrics.gpu.samples -lt 1) { throw 'Portable profiling did not capture gameplay CPU/GPU frames' } }
    if (-not $ResultFile) { $ResultFile = Join-Path $taskRun 'verification.json' }
    $taskResult = [ordered]@{ format='shardbound-package-verification'; version=1; created_utc=[DateTimeOffset]::UtcNow.ToString('o'); package=$taskPackage; restricted_path=$true; fresh_windows_vm=$false; modcheck_exit=$taskCheck.ExitCode; editor_exit=$taskEditor.ExitCode; create_exit=$taskCreate.ExitCode; resume_exit=$taskResume.ExitCode; reports=@($taskReports.FullName); logs=$taskRun }
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($ResultFile),($taskResult | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
    Write-Output "Portable runtime verified. Evidence: $ResultFile"
} finally { $env:PATH = $taskOriginalPath }
