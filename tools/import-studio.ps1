<#
.SYNOPSIS
Validate and install an exported Studio package into an existing ShardBound mod.
.EXAMPLE
pwsh -File tools/import-studio.ps1 -Package C:/Exports/Sword -Mod content/my_mod
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Package,
    [Parameter(Mandatory)][string]$Mod,
    [string]$Validator = "$PSScriptRoot/../build/release/modcheck.exe",
    [switch]$Update
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $Package).Path
if (Test-Path -LiteralPath $source -PathType Container) { $source = Join-Path $source 'studio.manifest.json' }
$source = (Resolve-Path -LiteralPath $source).Path
$sourceRoot = Split-Path -Parent $source
$modRoot = (Resolve-Path -LiteralPath $Mod).Path
$validatorPath = (Resolve-Path -LiteralPath $Validator).Path
$manifest = Get-Content -Raw -LiteralPath $source | ConvertFrom-Json -AsHashtable
$modInfo = Get-Content -Raw -LiteralPath (Join-Path $modRoot 'mod.json') | ConvertFrom-Json -AsHashtable
$project = $manifest.project
if ($project.Split(':')[0] -cne $modInfo.id) { throw "O namespace do projeto '$project' deve ser '$($modInfo.id)'. Corrija o ID no Studio e exporte novamente." }
& $validatorPath --studio $source
if ($LASTEXITCODE -ne 0) { throw 'O pacote foi rejeitado. Nenhum arquivo do mod foi alterado.' }
$events = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot $manifest.events) | ConvertFrom-Json -AsHashtable
foreach ($id in (@($manifest.models | ForEach-Object { $_.id }) + @($events.sounds | ForEach-Object { $_.id }))) {
    if ($id.Split(':')[0] -cne $modInfo.id) { throw "O recurso '$id' deve usar o namespace '$($modInfo.id)'." }
}
$registryPath = Join-Path $modRoot 'assets.json'
$registry = if (Test-Path -LiteralPath $registryPath) { Get-Content -Raw -LiteralPath $registryPath | ConvertFrom-Json -AsHashtable } else { @{} }
if (-not $registry.ContainsKey('studio')) { $registry.studio = @{} }
if ($registry.studio.ContainsKey($project) -and -not $Update) { throw 'Esse projeto já está registrado. Use -Update para instalar uma nova revisão e preservar a anterior.' }
foreach ($model in $manifest.models) {
    if ($registry.models -and $registry.models.ContainsKey($model.id)) { throw "Modelo '$($model.id)' já declarado individualmente em assets.json. Resolva a duplicação antes de importar." }
}
$newIds = @($manifest.models | ForEach-Object { $_.id }) + @($events.sounds | ForEach-Object { $_.id })
foreach ($sound in $events.sounds) { if ($registry.sounds -and $registry.sounds.ContainsKey($sound.id)) { throw "Som '$($sound.id)' já registrado individualmente." } }
foreach ($key in $registry.studio.Keys) {
    if ($key -ceq $project) { continue }
    $otherFile = [IO.Path]::GetFullPath((Join-Path $modRoot $registry.studio[$key].source))
    if (-not $otherFile.StartsWith($modRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Registro Studio existente fora da pasta do mod.' }
    $other = Get-Content -Raw -LiteralPath $otherFile | ConvertFrom-Json -AsHashtable
    $otherEvents = Get-Content -Raw -LiteralPath (Join-Path (Split-Path -Parent $otherFile) $other.events) | ConvertFrom-Json -AsHashtable
    foreach ($id in (@($other.models | ForEach-Object { $_.id }) + @($otherEvents.sounds | ForEach-Object { $_.id }))) {
        if ($newIds -ccontains $id) { throw "O recurso '$id' já pertence ao pacote '$key'." }
    }
}
$suffix = [Guid]::NewGuid().ToString('N').Substring(0, 8)
$relative = "studio/$($project.Split(':')[1].Replace('/','-'))-$suffix"
$destination = Join-Path $modRoot $relative
New-Item -ItemType Directory -Path $destination | Out-Null
$effects = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot $manifest.effects) | ConvertFrom-Json -AsHashtable
$events = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot $manifest.events) | ConvertFrom-Json -AsHashtable
$files = @((Split-Path -Leaf $source), $manifest.effects, $manifest.events, $manifest.playerAnimations)
$files += @($manifest.models | ForEach-Object { $_.file })
$files += @($effects.definitions | Where-Object { $_.texture } | ForEach-Object { $_.texture })
$files += @($events.sounds | ForEach-Object { $_.file })
foreach ($file in ($files | Select-Object -Unique)) {
    $target = [IO.Path]::GetFullPath((Join-Path $destination $file))
    if (-not $target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Caminho de destino fora do pacote.' }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRoot $file) -Destination $target
}
& $validatorPath --studio (Join-Path $destination (Split-Path -Leaf $source))
if ($LASTEXITCODE -ne 0) { throw "Falha ao verificar a cópia em $destination. O registro anterior foi preservado." }
$registry.studio[$project] = @{ source = "$relative/$(Split-Path -Leaf $source)" }
$pending = "$registryPath.$suffix.tmp"
$registry | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $pending -Encoding utf8NoBOM
if (Test-Path -LiteralPath $registryPath) {
    [IO.File]::Replace($pending, $registryPath, "$registryPath.$suffix.bak")
} else { [IO.File]::Move($pending, $registryPath) }
Write-Host "Instalado: $project em $destination"
Write-Host 'Os modelos estão disponíveis pelos IDs exportados. Reinicie o jogo para recarregar o registro; os scripts definem quando criar entidades e tocar animações.'
