$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    . (Join-Path $PSScriptRoot 'scripts\SecureTunnel-Common.ps1')
    $client = Get-TunnelClient
    & $client runtimes stop $script:TunnelAlias
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao parar o runtime ShardBound.' }
    Write-Output 'Runtime local do ShardBound parado. O tunnel remoto continua cadastrado na OpenAI.'
} finally { Pop-Location }
