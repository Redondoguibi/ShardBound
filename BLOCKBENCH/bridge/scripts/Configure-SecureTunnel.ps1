[CmdletBinding()]
param(
    [string]$TunnelId,
    [switch]$ResetKey
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'SecureTunnel-Common.ps1')
Ensure-LocalDirectory

if (-not (Test-Path -LiteralPath $script:TunnelClientPath)) {
    & (Join-Path $PSScriptRoot 'Install-TunnelClient.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao instalar tunnel-client.' }
}

$existing = Get-TunnelSettings
if ([string]::IsNullOrWhiteSpace($TunnelId)) { $TunnelId = $env:CONTROL_PLANE_TUNNEL_ID }
if ([string]::IsNullOrWhiteSpace($TunnelId) -and $existing) { $TunnelId = $existing.tunnel_id }

if ([string]::IsNullOrWhiteSpace($TunnelId)) {
    Write-Host ''
    Write-Host 'Falta apenas o tunnel_id da OpenAI. Vou abrir a pagina oficial de Tunnels.'
    Start-Process 'https://platform.openai.com/settings/organization/tunnels'
    $TunnelId = Read-Host 'Crie/abra um tunnel e cole aqui o tunnel_id (tunnel_...)'
}
Assert-TunnelId $TunnelId
Save-TunnelSettings $TunnelId

if (-not $env:CONTROL_PLANE_API_KEY -and ($ResetKey -or -not (Test-Path -LiteralPath $script:TunnelKeyPath))) {
    Write-Host ''
    Write-Host 'Agora precisamos da Runtime API key do tunnel. Ela sera guardada criptografada pelo Windows para este usuario.'
    Start-Process 'https://platform.openai.com/settings/organization/api-keys'
    $secure = Read-Host 'Cole a Runtime API key (a entrada fica oculta)' -AsSecureString
    if ($secure.Length -lt 20) { throw 'A chave informada parece curta demais.' }
    Save-RuntimeApiKey $secure
}

Write-Host ''
Write-Output 'Configuracao local concluida.'
Write-Output "Tunnel: $TunnelId"
Write-Output "Alias: $script:TunnelAlias"
Write-Output 'A Runtime API key nao foi gravada em texto puro no repositorio.'
