[CmdletBinding()]
param([switch]$Force)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'SecureTunnel-Common.ps1')
Ensure-LocalDirectory

if ((Test-Path -LiteralPath $script:TunnelClientPath) -and -not $Force) {
    Write-Output "tunnel-client ja instalado em $script:TunnelClientPath"
    exit 0
}

$arch = switch ($env:PROCESSOR_ARCHITECTURE.ToUpperInvariant()) {
    'AMD64' { 'amd64' }
    'ARM64' { 'arm64' }
    default { throw "Arquitetura Windows nao suportada automaticamente: $env:PROCESSOR_ARCHITECTURE" }
}

Write-Output 'Consultando a release oficial mais recente de openai/tunnel-client...'
$headers = @{ 'User-Agent' = 'ShardBound-Blockbench-Bridge' }
$release = Invoke-RestMethod -Uri 'https://api.github.com/repos/openai/tunnel-client/releases/latest' -Headers $headers
$assetPattern = "^tunnel-client-$([regex]::Escape($release.tag_name))-windows-$arch\.zip$"
$asset = $release.assets | Where-Object { $_.name -match $assetPattern } | Select-Object -First 1
if (-not $asset) {
    $asset = $release.assets | Where-Object { $_.name -match "^tunnel-client-v.*-windows-$arch\.zip$" } | Select-Object -First 1
}
if (-not $asset) { throw "Nao encontrei o binario Windows $arch na release $($release.tag_name)." }

$checksumAsset = $release.assets | Where-Object { $_.name -eq 'SHA256SUMS.txt' } | Select-Object -First 1
if (-not $checksumAsset) { throw 'A release oficial nao publicou SHA256SUMS.txt.' }

$temp = Join-Path ([IO.Path]::GetTempPath()) ('shardbound-tunnel-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $temp | Out-Null
try {
    $zip = Join-Path $temp $asset.name
    $sumFile = Join-Path $temp 'SHA256SUMS.txt'
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip -Headers $headers
    Invoke-WebRequest -Uri $checksumAsset.browser_download_url -OutFile $sumFile -Headers $headers

    $sumLine = Get-Content -LiteralPath $sumFile | Where-Object { $_ -match [regex]::Escape($asset.name) } | Select-Object -First 1
    if (-not $sumLine) { throw "Checksum de $($asset.name) nao encontrado." }
    $expected = (($sumLine -split '\s+')[0]).ToLowerInvariant()
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw "SHA-256 invalido para $($asset.name)." }

    $expanded = Join-Path $temp 'expanded'
    Expand-Archive -LiteralPath $zip -DestinationPath $expanded -Force
    $exe = Get-ChildItem -LiteralPath $expanded -Filter 'tunnel-client.exe' -File -Recurse | Select-Object -First 1
    if (-not $exe) { throw 'tunnel-client.exe nao encontrado dentro do pacote oficial.' }

    Copy-Item -LiteralPath $exe.FullName -Destination $script:TunnelClientPath -Force
    $release.tag_name | Set-Content -LiteralPath (Join-Path $script:LocalDir 'tunnel-client-version.txt') -Encoding ASCII
    Write-Output "Instalado e verificado: $($release.tag_name) -> $script:TunnelClientPath"
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
