[CmdletBinding()]
param([switch]$Verify)

$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    . (Join-Path $PSScriptRoot 'scripts\SecureTunnel-Common.ps1')
    $client = Get-TunnelClient
    $settings = Get-TunnelSettings
    if (-not $settings) { throw 'Secure MCP Tunnel ainda nao foi configurado. Rode .\Connect-ChatGPT.ps1.' }

    $raw = (& $client runtimes status $script:TunnelAlias --json | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao consultar o runtime ShardBound.' }
    Write-Output $raw

    if ($Verify) {
        $status = $raw | ConvertFrom-Json
        foreach ($name in @('process_running', 'healthy', 'ready')) {
            $property = $status.PSObject.Properties[$name]
            if (-not $property) { throw "A versao atual do tunnel-client nao retornou '$name' no status JSON." }
            if ($property.Value -ne $true) { throw "Secure MCP Tunnel nao esta pronto: $name = $($property.Value)" }
        }
        Write-Output 'PASS: process_running=true, healthy=true, ready=true.'
    }
} finally { Pop-Location }
