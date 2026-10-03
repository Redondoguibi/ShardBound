$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if (-not (Test-Path -LiteralPath '.local/config.json')) { throw 'Execute Install.ps1 primeiro.' }
    if (-not (Test-Path -LiteralPath '.local/bin/cloudflared.exe')) {
        & (Join-Path $PSScriptRoot 'scripts/Install-Cloudflared.ps1')
    }
    & node scripts/chatgpt.mjs start
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao iniciar a ponte para ChatGPT.' }
    Write-Output 'Use a URL mcpUrl em Plugins > Adicionar > Adicionar servidor MCP, com OAuth.'
    Write-Output 'A URL e temporaria: muda quando o tunel e reiniciado. Para encerrar: npm run chatgpt:stop.'
} finally { Pop-Location }
