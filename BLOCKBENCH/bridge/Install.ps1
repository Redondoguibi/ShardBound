param([switch]$RegisterCodex)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & npm.cmd ci --no-fund --no-audit
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao instalar dependencias.' }
    $setupArgs = @('scripts/setup.mjs')
    if ($RegisterCodex) { $setupArgs += '--register-codex' }
    & node @setupArgs
    if ($LASTEXITCODE -ne 0) { throw 'Falha na configuracao.' }
} finally { Pop-Location }
