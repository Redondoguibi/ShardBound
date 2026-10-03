$ErrorActionPreference = 'Stop'
$bridgeRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$binDir = Join-Path $bridgeRoot '.local/bin'
New-Item -ItemType Directory -Path $binDir -Force | Out-Null
$release = Invoke-RestMethod 'https://api.github.com/repos/cloudflare/cloudflared/releases/latest'
$asset = $release.assets | Where-Object name -eq 'cloudflared-windows-amd64.exe'
if (-not $asset.digest.StartsWith('sha256:')) { throw 'A publicacao nao fornece checksum SHA-256.' }
$binary = Join-Path $binDir 'cloudflared.exe'
$download = Join-Path $binDir 'cloudflared.download'
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $download
if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -ne $asset.digest.Substring(7)) {
    throw 'Checksum incorreto; binario nao instalado.'
}
Move-Item -LiteralPath $download -Destination $binary -Force
Write-Output "Cloudflared $($release.tag_name) instalado e verificado em $binary"
