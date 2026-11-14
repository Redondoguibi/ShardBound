param([string]$OutputPath)
$ErrorActionPreference = 'Stop'
$studioRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$metadata = Get-Content -LiteralPath (Join-Path $studioRoot 'package.json') -Raw | ConvertFrom-Json
if (-not $OutputPath) { $OutputPath = Join-Path $studioRoot "dist/ShardBound-Studio-$($metadata.version).zip" }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$stagePath = Join-Path $studioRoot ('.local/package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stagePath -Force | Out-Null
Push-Location -LiteralPath $studioRoot
try {
    & node scripts/build.mjs
    if ($LASTEXITCODE -ne 0) { throw 'A compilação falhou.' }
    foreach ($entry in @('src', 'scripts', 'tests', 'docs', 'schemas', 'examples', 'README.md', 'package.json', 'package-lock.json')) {
        Copy-Item -LiteralPath (Join-Path $studioRoot $entry) -Destination $stagePath -Recurse
    }
    $stageDist = Join-Path $stagePath 'dist'
    New-Item -ItemType Directory -Path $stageDist | Out-Null
    Copy-Item -LiteralPath (Join-Path $studioRoot 'dist/shardbound_studio.js') -Destination $stageDist
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead((Join-Path $stageDist 'shardbound_studio.js'))
    try { $digest = [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $stream.Dispose(); $algorithm.Dispose() }
    Set-Content -LiteralPath (Join-Path $stageDist 'shardbound_studio.sha256') -Value "$digest  shardbound_studio.js" -Encoding ascii
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($OutputPath)) -Force | Out-Null
    [Reflection.Assembly]::LoadWithPartialName('System.IO.Compression.FileSystem') | Out-Null
    $temporaryZip = Join-Path $studioRoot ('.local/package-' + [guid]::NewGuid().ToString('N') + '.zip')
    [IO.Compression.ZipFile]::CreateFromDirectory($stagePath, $temporaryZip)
    [IO.File]::Copy($temporaryZip, $OutputPath, $true)
    Write-Output "Pacote: $OutputPath"
    Write-Output "SHA256 do plugin: $digest"
} finally { Pop-Location }
