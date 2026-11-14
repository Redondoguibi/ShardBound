[CmdletBinding()]
param([switch]$RestartConnection)
$ErrorActionPreference = 'Stop'
$bridgePath = [IO.Path]::GetFullPath($PSScriptRoot)
$studioPath = [IO.Path]::GetFullPath((Join-Path $bridgePath '../studio'))
foreach ($projectPath in @($studioPath, $bridgePath)) {
    if (-not (Test-Path -LiteralPath (Join-Path $projectPath 'package.json'))) { throw "Projeto ausente: $projectPath" }
    Push-Location -LiteralPath $projectPath
    try {
        & npm ci
        if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar dependencias: $projectPath" }
        & npm run build
        if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar: $projectPath" }
    } finally { Pop-Location }
}
Push-Location -LiteralPath $bridgePath
try {
    & node scripts/setup.mjs
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao atualizar a bridge pareada.' }
    if ($RestartConnection) {
        . (Join-Path $bridgePath 'scripts/SecureTunnel-Common.ps1')
        if (Get-TunnelSettings) {
            & (Join-Path $bridgePath 'Disconnect-ChatGPT.ps1')
            if ($LASTEXITCODE -ne 0) { throw 'Falha ao parar o runtime anterior.' }
        }
        # Restart the owning daemon too; an old daemon retains the old tool schemas.
        try { & node scripts/stop.mjs --if-running; if ($LASTEXITCODE -ne 0) { throw 'Falha ao parar daemon.' } }
        catch { throw 'Nao foi possivel reiniciar o daemon local. Execute Status-ChatGPT-Desktop-Plugin.ps1 e verifique a conexao antes de tentar novamente.' }
        & (Join-Path $bridgePath 'Connect-ChatGPT.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'Falha ao reconectar o runtime.' }
    }
    Write-Output 'No Blockbench, salve o trabalho e recarregue os dois plugins de arquivo:'
    Write-Output (Join-Path $studioPath 'dist/shardbound_studio.js')
    Write-Output (Join-Path $bridgePath '.local/shardbound_bridge.js')
    Write-Output 'Crie/abra um projeto ShardBound Studio. No ChatGPT, abra uma conversa nova e selecione @ShardBound Blockbench.'
} finally { Pop-Location }
