[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$PluginName = 'shardbound-blockbench-local'
$MarketplacePath = Join-Path $HOME '.agents\plugins\marketplace.json'
$SourcePath = Join-Path $HOME ".codex\plugins\$PluginName"

Write-Output '=== ShardBound ChatGPT Desktop Local Plugin ==='
Write-Output "Marketplace: $MarketplacePath"
Write-Output "Source:      $SourcePath"
Write-Host ''

if (-not (Test-Path -LiteralPath $MarketplacePath)) {
    Write-Output 'marketplace_exists: false'
    exit 2
}
Write-Output 'marketplace_exists: true'

$marketplace = Get-Content -LiteralPath $MarketplacePath -Raw | ConvertFrom-Json
$entry = @($marketplace.plugins | Where-Object { $_.name -eq $PluginName } | Select-Object -First 1)
if (-not $entry) {
    Write-Output 'marketplace_entry: false'
    exit 3
}
Write-Output 'marketplace_entry: true'
Write-Output "marketplace_name: $($marketplace.name)"
Write-Output "source_path: $($entry.source.path)"

if (-not (Test-Path -LiteralPath $SourcePath)) {
    Write-Output 'source_exists: false'
    exit 4
}
Write-Output 'source_exists: true'

foreach ($required in @('plugin.json', 'mcp.json', '.mcp.json', '.codex-plugin\plugin.json', 'skills\blockbench\SKILL.md')) {
    Write-Output ("source_{0}: {1}" -f $required.Replace('\','_'), (Test-Path -LiteralPath (Join-Path $SourcePath $required)))
}

$cacheBase = Join-Path $HOME ".codex\plugins\cache\$($marketplace.name)\$PluginName"
Write-Output "cache_base: $cacheBase"

if (-not (Test-Path -LiteralPath $cacheBase)) {
    Write-Output 'cache_exists: false'
    Write-Output 'diagnosis: ChatGPT Desktop ainda nao instalou/atualizou o plugin local a partir do marketplace.'
    exit 5
}

Write-Output 'cache_exists: true'
$cached = Get-ChildItem -LiteralPath $cacheBase -Directory -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($cached) {
    Write-Output "cache_version_dir: $($cached.FullName)"
    Write-Output ("cache_plugin_json: {0}" -f (Test-Path -LiteralPath (Join-Path $cached.FullName 'plugin.json')))
    Write-Output ("cache_mcp_json: {0}" -f (Test-Path -LiteralPath (Join-Path $cached.FullName 'mcp.json')))
} else {
    Write-Output 'cache_version_dir: none'
}

Write-Output 'diagnosis: marketplace e source locais estao presentes; se as tools ainda nao aparecem, o proximo ponto e o carregamento MCP pelo host Desktop.'
