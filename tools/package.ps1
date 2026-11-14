param([string]$OutputDirectory = '', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBuild) {
    & "$PSScriptRoot/build.ps1" -Preset release
    if ($LASTEXITCODE) { throw 'Release validation failed' }
}
$taskCmake = Join-Path $taskRoot '.tools/python/cmake/data/bin/cmake.exe'
if (-not (Test-Path -LiteralPath $taskCmake)) { $taskCmake = (Get-Command cmake -ErrorAction Stop).Source }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $taskRoot ('dist/ShardBound-win64-' + [Guid]::NewGuid().ToString('N').Substring(0,8)) }
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $taskOutput) { throw 'Choose a new output directory; existing packages are preserved.' }
& $taskCmake --install "$taskRoot/build/release" --prefix $taskOutput --component Runtime
if ($LASTEXITCODE) { throw 'Runtime install failed' }
$taskFiles = [ordered]@{}
Get-ChildItem -LiteralPath $taskOutput -Recurse -File | Sort-Object FullName | ForEach-Object {
    $taskRelative = $_.FullName.Substring($taskOutput.Length+1).Replace('\','/')
    $taskFiles[$taskRelative] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$taskManifest = [ordered]@{ format='shardbound-package'; version=1; platform='Windows x64'; created_utc=[DateTimeOffset]::UtcNow.ToString('o'); files=$taskFiles }
[IO.File]::WriteAllText((Join-Path $taskOutput 'package.json'),($taskManifest | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
& "$PSScriptRoot/verify-package.ps1" -Directory $taskOutput -ResultFile ($taskOutput + '-verification.json')
if (-not $?) { throw 'Portable runtime verification failed' }
$taskArchive = $taskOutput + '.zip'
if (Test-Path -LiteralPath $taskArchive) { throw 'Archive already exists' }
Compress-Archive -LiteralPath $taskOutput -DestinationPath $taskArchive
$taskExtract = Join-Path ([IO.Path]::GetTempPath()) ('ShardBound-archive-' + [Guid]::NewGuid().ToString('N'))
Expand-Archive -LiteralPath $taskArchive -DestinationPath $taskExtract
& "$PSScriptRoot/verify-package.ps1" -Directory (Join-Path $taskExtract (Split-Path $taskOutput -Leaf)) -HashesOnly
if (-not $?) { throw 'Archive integrity verification failed' }
[IO.File]::WriteAllText(($taskArchive+'.sha256'),(Get-FileHash -LiteralPath $taskArchive -Algorithm SHA256).Hash.ToLowerInvariant()+"  "+[IO.Path]::GetFileName($taskArchive)+[Environment]::NewLine)
Write-Output "Portable archive: $taskArchive"
