[CmdletBinding()]
param([string]$TunnelId, [switch]$Reconfigure, [switch]$OpenPlugin)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    . (Join-Path $PSScriptRoot 'scripts\SecureTunnel-Common.ps1')
    $node = Get-NodeExecutable
    & $node scripts/migrate-chatgpt-config.mjs
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao migrar configuracoes antigas; os arquivos originais foram preservados.' }
    & (Join-Path $PSScriptRoot 'Connect-ChatGPT.ps1') -TunnelId $TunnelId -Reconfigure:$Reconfigure
    & (Join-Path $PSScriptRoot 'Status-ChatGPT-Desktop-Plugin.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Uma camada local falhou. Leia o diagnostico acima.' }
    Write-Output 'Runtime local validado. O plugin de conta deve estar conectado e selecionado em um Chat normal.'
    Write-Output 'Carregue .local\shardbound_bridge.js no Blockbench. Para Studio, carregue tambem ../studio/dist/shardbound_studio.js (1.1+).'
    Write-Output 'Plugin: https://chatgpt.com/plugins/plugins_6abfe848600c8191876653a097dd8d53'
    Write-Output 'Cache local e runtime pronto nao comprovam tools disponiveis no Chat. Veja normal_chat_host no diagnostico.'
    if ($OpenPlugin) { Start-Process 'https://chatgpt.com/plugins/plugins_6abfe848600c8191876653a097dd8d53' }
} finally { Pop-Location }
