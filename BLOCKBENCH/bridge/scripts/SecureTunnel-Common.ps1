$ErrorActionPreference = 'Stop'

$script:BridgeRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$script:LocalDir = Join-Path $script:BridgeRoot '.local'
$script:TunnelClientPath = Join-Path $script:LocalDir 'bin\tunnel-client.exe'
$script:TunnelSettingsPath = Join-Path $script:LocalDir 'secure-tunnel.json'
$script:TunnelKeyPath = Join-Path $script:LocalDir 'secure-tunnel-key.dpapi'
$script:TunnelProfileDir = Join-Path $script:LocalDir 'tunnel-profiles'
$script:TunnelAlias = 'shardbound-blockbench'

function Ensure-LocalDirectory {
    New-Item -ItemType Directory -Force -Path $script:LocalDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $script:TunnelClientPath) | Out-Null
    New-Item -ItemType Directory -Force -Path $script:TunnelProfileDir | Out-Null
}

function Get-TunnelSettings {
    if (-not (Test-Path -LiteralPath $script:TunnelSettingsPath)) { return $null }
    return Get-Content -LiteralPath $script:TunnelSettingsPath -Raw | ConvertFrom-Json
}

function Save-TunnelSettings([string]$TunnelId) {
    Ensure-LocalDirectory
    [ordered]@{
        tunnel_id = $TunnelId
        alias = $script:TunnelAlias
        configured_at = (Get-Date).ToUniversalTime().ToString('o')
    } | ConvertTo-Json | Set-Content -LiteralPath $script:TunnelSettingsPath -Encoding UTF8
}

function Save-RuntimeApiKey([Security.SecureString]$Key) {
    Ensure-LocalDirectory
    $encrypted = $Key | ConvertFrom-SecureString
    if ([string]::IsNullOrWhiteSpace($encrypted)) {
        throw 'Falha ao criptografar a Runtime API key.'
    }

    # ConvertFrom-SecureString outputs an ASCII-safe DPAPI blob on Windows.
    # WriteAllText avoids BOM/newline characters that break ConvertTo-SecureString.
    [IO.File]::WriteAllText(
        $script:TunnelKeyPath,
        $encrypted,
        [Text.Encoding]::ASCII
    )
}

function Convert-SecureStringToPlainText([Security.SecureString]$Secure) {
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Secure)
    try { return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
}

function Get-RuntimeApiKey {
    if ($env:CONTROL_PLANE_API_KEY) { return $env:CONTROL_PLANE_API_KEY }
    if (-not (Test-Path -LiteralPath $script:TunnelKeyPath)) {
        throw 'Runtime API key nao configurada. Rode .\scripts\Configure-SecureTunnel.ps1.'
    }

    # Be tolerant of files produced by the previous implementation, which
    # could contain a UTF-8 BOM and/or a trailing newline.
    $encrypted = [IO.File]::ReadAllText($script:TunnelKeyPath)
    $encrypted = $encrypted.Trim().TrimStart([char]0xFEFF)
    if ([string]::IsNullOrWhiteSpace($encrypted)) {
        throw 'Arquivo local da Runtime API key esta vazio. Rode .\Connect-ChatGPT.ps1 -Reconfigure.'
    }

    try {
        $secure = $encrypted | ConvertTo-SecureString -ErrorAction Stop
    } catch {
        throw 'Nao foi possivel descriptografar a Runtime API key local. Rode .\Connect-ChatGPT.ps1 -Reconfigure para grava-la novamente.'
    }
    return Convert-SecureStringToPlainText $secure
}

function Get-NodeExecutable {
    $node = Get-Command node -CommandType Application -ErrorAction Stop | Select-Object -First 1
    return $node.Source
}

function Get-McpCommand {
    # tunnel-client parses stdio commands with a POSIX-like parser where
    # backslashes are escape characters. Native Windows paths such as
    # C:\Program Files\... therefore break into an invalid executable.
    #
    # Resolve Node now instead of depending on the detached runtime's PATH.
    # Single quotes are consumed by tunnel-client's command parser, not a shell.
    $nodePath = (Get-NodeExecutable).Replace('\', '/')
    $server = Join-Path $script:BridgeRoot 'mcp\server.mjs'
    if (-not (Test-Path -LiteralPath $server)) { throw "MCP server nao encontrado: $server" }

    $serverUri = ([System.Uri]$server).AbsoluteUri
    $tick = [char]96
    $code = 'import(' + $tick + $serverUri + $tick + ')'
    return (Quote-McpArgument $nodePath) + ' -e ' + (Quote-McpArgument $code)
}

function Quote-McpArgument([string]$Value) {
    $single = [string][char]39
    $double = [string][char]34
    $escapedQuote = $single + $double + $single + $double + $single
    return $single + $Value.Replace($single, $escapedQuote) + $single
}

function Get-SafeTunnelStatus {
    $client = Get-TunnelClient
    $raw = (& $client runtimes status $script:TunnelAlias --json 2>$null | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel consultar o runtime ShardBound.' }
    $native = $raw | ConvertFrom-Json
    $safe = [ordered]@{ checkedAt = (Get-Date).ToUniversalTime().ToString('o') }
    foreach ($field in @('process_running', 'healthy', 'ready', 'runtime_state', 'tunnel_id')) {
        $safe[$field] = $native.$field
    }
    $safe.pid = $native.process.pid
    $safe.log_path = $native.process.log_path
    try {
        $health = Invoke-RestMethod -Uri $native.health_details_url -TimeoutSec 5
        $safe.control_plane = $health.components.'control-plane'
        $safe.mcp = $health.components.mcp
        $safe.external_proxy_enabled = $health.components.cloudflared.details.enabled
    } catch { $safe.health_error = $_.Exception.Message }
    Ensure-LocalDirectory
    [IO.File]::WriteAllText((Join-Path $script:LocalDir 'tunnel-status-safe.json'), ($safe | ConvertTo-Json -Depth 9), [Text.UTF8Encoding]::new($false))
    return [pscustomobject]$safe
}

function Assert-TunnelId([string]$TunnelId) {
    if ([string]::IsNullOrWhiteSpace($TunnelId) -or $TunnelId -notmatch '^tunnel_[A-Za-z0-9_-]+$') {
        throw 'Tunnel ID invalido. Ele deve comecar com tunnel_.'
    }
}

function Get-TunnelClient {
    if (-not (Test-Path -LiteralPath $script:TunnelClientPath)) {
        throw 'tunnel-client nao instalado. Rode .\scripts\Install-TunnelClient.ps1.'
    }
    return $script:TunnelClientPath
}
