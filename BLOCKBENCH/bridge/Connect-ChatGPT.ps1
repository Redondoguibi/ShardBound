[CmdletBinding()]
param(
    [string]$TunnelId,
    [switch]$Reconfigure
)

$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    . (Join-Path $PSScriptRoot 'scripts\SecureTunnel-Common.ps1')

    if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
        throw 'Node.js 22+ nao encontrado no PATH.'
    }
    if (-not (Test-Path -LiteralPath 'node_modules')) {
        Write-Output 'Instalando dependencias locais...'
        & npm ci
        if ($LASTEXITCODE -ne 0) { throw 'npm ci falhou.' }
    }
    if (-not (Test-Path -LiteralPath '.local\config.json') -or -not (Test-Path -LiteralPath '.local\shardbound_bridge.js')) {
        Write-Output 'Preparando o plugin pareado do Blockbench...'
        & node scripts/setup.mjs
        if ($LASTEXITCODE -ne 0) { throw 'setup do ShardBound falhou.' }
    }
    if (-not (Test-Path -LiteralPath $script:TunnelClientPath)) {
        & (Join-Path $PSScriptRoot 'scripts\Install-TunnelClient.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'Instalacao do tunnel-client falhou.' }
    }

    $settings = Get-TunnelSettings
    if ($Reconfigure -or -not $settings -or (-not $env:CONTROL_PLANE_API_KEY -and -not (Test-Path -LiteralPath $script:TunnelKeyPath))) {
        & (Join-Path $PSScriptRoot 'scripts\Configure-SecureTunnel.ps1') -TunnelId $TunnelId -ResetKey:$Reconfigure
        if ($LASTEXITCODE -ne 0) { throw 'Configuracao do Secure MCP Tunnel falhou.' }
        $settings = Get-TunnelSettings
    } elseif (-not [string]::IsNullOrWhiteSpace($TunnelId) -and $TunnelId -ne $settings.tunnel_id) {
        Assert-TunnelId $TunnelId
        Save-TunnelSettings $TunnelId
        $settings = Get-TunnelSettings
    }

    Assert-TunnelId $settings.tunnel_id
    $client = Get-TunnelClient
    $runtimeKey = Get-RuntimeApiKey
    $mcpCommand = Get-McpCommand
    $env:CONTROL_PLANE_API_KEY = $runtimeKey
    $env:TUNNEL_CLIENT_PROFILE_DIR = $script:TunnelProfileDir

    Write-Host ''
    Write-Output "Conectando ShardBound ao Secure MCP Tunnel $($settings.tunnel_id)..."
    $connectArgs = @(
        'runtimes', 'connect',
        '--alias', $script:TunnelAlias,
        '--tunnel-id', $settings.tunnel_id,
        '--runtime-api-key', 'env:CONTROL_PLANE_API_KEY',
        '--profile-dir', $script:TunnelProfileDir,
        '--mcp-command', $mcpCommand,
        '--name', 'ShardBound Blockbench',
        '--description', 'Blockbench Generic Model bridge for ChatGPT',
        '--json'
    )
    & $client @connectArgs
    if ($LASTEXITCODE -ne 0) { throw 'tunnel-client runtimes connect falhou.' }

    Write-Host ''
    Write-Output 'Estado do runtime:'
    & $client runtimes status $script:TunnelAlias --json
    if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel consultar o runtime criado.' }

    Write-Host ''
    Write-Output 'Secure MCP Tunnel iniciado. Nao existe URL temporaria, Cloudflare ou login OAuth do ShardBound.'
    Write-Output "No ChatGPT, a conexao deve usar Connection = Tunnel e o tunnel_id: $($settings.tunnel_id)"
    Write-Output 'Para verificar depois: .\Status-ChatGPT.ps1'
    Write-Output 'Para parar: .\Disconnect-ChatGPT.ps1'
} finally {
    Remove-Item Env:CONTROL_PLANE_API_KEY -ErrorAction SilentlyContinue
    Remove-Item Env:TUNNEL_CLIENT_PROFILE_DIR -ErrorAction SilentlyContinue
    Pop-Location
}
