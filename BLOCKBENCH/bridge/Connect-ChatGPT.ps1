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
    $connectOutput = (& $client @connectArgs 2>&1 | Out-String).Trim()
    $connectExitCode = $LASTEXITCODE

    if (-not [string]::IsNullOrWhiteSpace($connectOutput)) {
        Write-Output $connectOutput
    }

    if ($connectExitCode -ne 0) {
        $diagnosticPath = Join-Path $script:LocalDir 'tunnel-connect-error.log'
        $safeDiagnostic = @(
            "UTC: $((Get-Date).ToUniversalTime().ToString('o'))",
            "Tunnel: $($settings.tunnel_id)",
            "ExitCode: $connectExitCode",
            '',
            $connectOutput
        ) -join [Environment]::NewLine
        [IO.File]::WriteAllText($diagnosticPath, $safeDiagnostic, [Text.Encoding]::UTF8)

        Write-Host ''
        Write-Warning 'O tunnel-client recusou a conexao. Causa detectada:'
        if ($connectOutput -match '(?i)\b401\b|unauthori[sz]ed|invalid.*api.?key|incorrect.*api.?key') {
            Write-Warning '401/AUTENTICACAO: a Runtime API key e invalida, foi revogada ou nao esta sendo aceita.'
        } elseif ($connectOutput -match '(?i)\b403\b|forbidden|permission|tunnels?.*use|access required') {
            Write-Warning '403/PERMISSAO: a chave/principal provavelmente nao tem Tunnels Read + Use para este tunnel, ou o tunnel e a chave pertencem a contextos de organizacao diferentes.'
        } elseif ($connectOutput -match '(?i)\b404\b|not found|unknown tunnel') {
            Write-Warning '404/TUNNEL: o tunnel_id nao foi encontrado no contexto acessivel pela chave.'
        } elseif ($connectOutput -match '(?i)mcp|stdio|command|spawn|process|health|ready') {
            Write-Warning 'RUNTIME LOCAL: o tunnel foi aceito, mas houve falha ao iniciar/verificar o MCP stdio local.'
        } else {
            Write-Warning 'Nao foi possivel classificar automaticamente. A mensagem nativa acima e o diagnostico abaixo contem a causa real.'
        }

        Write-Host ''
        Write-Output 'Tentando obter estado parcial do runtime...'
        $statusOutput = (& $client runtimes status $script:TunnelAlias --json 2>&1 | Out-String).Trim()
        if (-not [string]::IsNullOrWhiteSpace($statusOutput)) {
            Write-Output $statusOutput
            [IO.File]::AppendAllText(
                $diagnosticPath,
                [Environment]::NewLine + [Environment]::NewLine + 'STATUS:' + [Environment]::NewLine + $statusOutput,
                [Text.Encoding]::UTF8
            )
        }

        throw "tunnel-client runtimes connect falhou. Diagnostico salvo em $diagnosticPath"
    }

    Write-Host ''
    Write-Output 'Estado do runtime:'
    $statusOutput = (& $client runtimes status $script:TunnelAlias --json 2>&1 | Out-String).Trim()
    $statusExitCode = $LASTEXITCODE
    if (-not [string]::IsNullOrWhiteSpace($statusOutput)) {
        Write-Output $statusOutput
    }
    if ($statusExitCode -ne 0) { throw 'Nao foi possivel consultar o runtime criado.' }

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
