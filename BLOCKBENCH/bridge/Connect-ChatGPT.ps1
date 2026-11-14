[CmdletBinding()]
param([string]$TunnelId, [switch]$Reconfigure)
$ErrorActionPreference = 'Stop'
$previousKey = $env:CONTROL_PLANE_API_KEY
$previousProfiles = $env:TUNNEL_CLIENT_PROFILE_DIR
Push-Location $PSScriptRoot
try {
    . (Join-Path $PSScriptRoot 'scripts\SecureTunnel-Common.ps1')
    $nodePath = Get-NodeExecutable
    $major = [int](((& $nodePath --version).TrimStart('v')).Split('.')[0])
    if ($major -lt 22) { throw 'Node.js 22+ necessario.' }
    if (-not (Test-Path -LiteralPath 'node_modules\@modelcontextprotocol\sdk')) {
        & npm ci
        if ($LASTEXITCODE -ne 0) { throw 'npm ci falhou.' }
    }
    if (-not (Test-Path -LiteralPath '.local\config.json') -or -not (Test-Path -LiteralPath '.local\shardbound_bridge.js')) {
        & $nodePath scripts/setup.mjs --local-only
        if ($LASTEXITCODE -ne 0) { throw 'Pairing do Blockbench falhou.' }
    }
    # Refresh the paired file after updates without rotating pairing/tunnel credentials.
    & $nodePath scripts/build.mjs --paired
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao atualizar a bridge pareada.' }
    if (-not (Test-Path -LiteralPath $script:TunnelClientPath)) {
        & (Join-Path $PSScriptRoot 'scripts\Install-TunnelClient.ps1')
    }
    $settings = Get-TunnelSettings
    if ($Reconfigure -or -not $settings -or (-not $previousKey -and -not (Test-Path -LiteralPath $script:TunnelKeyPath))) {
        & (Join-Path $PSScriptRoot 'scripts\Configure-SecureTunnel.ps1') -TunnelId $TunnelId -ResetKey:$Reconfigure
        $settings = Get-TunnelSettings
    } elseif ($TunnelId -and $TunnelId -ne $settings.tunnel_id) {
        Assert-TunnelId $TunnelId
        Save-TunnelSettings $TunnelId
        $settings = Get-TunnelSettings
    }
    Assert-TunnelId $settings.tunnel_id
    $client = Get-TunnelClient
    try { $existing = Get-SafeTunnelStatus } catch { $existing = $null }
    if ($existing.process_running -and $existing.healthy -and $existing.ready -and $existing.tunnel_id -eq $settings.tunnel_id -and -not $Reconfigure) {
        Write-Output 'Runtime existente pronto; nenhum processo duplicado foi iniciado.'
    } else {
        if ($existing.process_running) {
            & $client runtimes stop $script:TunnelAlias | Out-Null
            if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel parar o runtime anterior antes de reconfigurar.' }
        }
        $runtimeKey = Get-RuntimeApiKey
        $env:CONTROL_PLANE_API_KEY = $runtimeKey
        $env:TUNNEL_CLIENT_PROFILE_DIR = $script:TunnelProfileDir
        $connectArgs = @('runtimes', 'connect', '--alias', $script:TunnelAlias,
            '--tunnel-id', $settings.tunnel_id, '--runtime-api-key', 'env:CONTROL_PLANE_API_KEY',
            '--profile-dir', $script:TunnelProfileDir, '--mcp-command', (Get-McpCommand),
            '--name', 'ShardBound Blockbench', '--description', 'Blockbench Generic Model, ShardBound Studio and MTools', '--json')
        $output = (& $client @connectArgs 2>&1 | Out-String).Trim()
        if ($LASTEXITCODE -ne 0) {
            $safeError = $output.Replace($runtimeKey, '[REDACTED]') -replace 'sk-[A-Za-z0-9_-]+', '[REDACTED]'
            [IO.File]::WriteAllText((Join-Path $script:LocalDir 'tunnel-connect-error.log'), $safeError, [Text.UTF8Encoding]::new($false))
            throw "Conexao recusada: $safeError"
        }
    }
    $status = Get-SafeTunnelStatus
    $deadline = (Get-Date).AddSeconds(45)
    while ($status.process_running -and $status.control_plane.status -ne 'ok' -and (Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        $status = Get-SafeTunnelStatus
    }
    $status | ConvertTo-Json -Depth 9
    if (-not ($status.process_running -and $status.healthy -and $status.ready -and $status.control_plane.status -eq 'ok')) {
        throw 'Runtime iniciou mas ainda nao esta pronto. Consulte o log_path acima.'
    }
    Write-Output "Runtime pronto. No ChatGPT use Conexao = Tunel: $($settings.tunnel_id)"
    Write-Output 'Isso confirma o runtime; a descoberta das ferramentas no Chat deve ser verificada separadamente.'
} finally {
    $env:CONTROL_PLANE_API_KEY = $previousKey
    $env:TUNNEL_CLIENT_PROFILE_DIR = $previousProfiles
    Pop-Location
}
