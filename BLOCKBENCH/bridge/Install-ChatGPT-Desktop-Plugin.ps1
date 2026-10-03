[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$BridgeRoot = (Resolve-Path $PSScriptRoot).Path
$PluginName = 'shardbound-blockbench-local'
$DisplayName = 'ShardBound Blockbench Local'
$PersonalPluginRoot = Join-Path $HOME ".codex\plugins\$PluginName"
$MarketplacePath = Join-Path $HOME '.agents\plugins\marketplace.json'

function Write-Utf8NoBom([string]$Path, [string]$Content) {
    $parent = Split-Path -Parent $Path
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    [IO.File]::WriteAllText($Path, $Content, [Text.UTF8Encoding]::new($false))
}

Write-Output 'Preparando o runtime local do ShardBound...'

$node = Get-Command node -CommandType Application -ErrorAction Stop | Select-Object -First 1
$nodePath = $node.Source

Push-Location $BridgeRoot
try {
    if (-not (Test-Path -LiteralPath 'node_modules\@modelcontextprotocol\sdk')) {
        Write-Output 'Instalando dependencias do bridge...'
        & npm ci
        if ($LASTEXITCODE -ne 0) { throw 'npm ci falhou.' }
    }

    if (-not (Test-Path -LiteralPath '.local\config.json') -or -not (Test-Path -LiteralPath '.local\shardbound_bridge.js')) {
        Write-Output 'Preparando pairing local do Blockbench...'
        & node scripts/setup.mjs
        if ($LASTEXITCODE -ne 0) { throw 'setup do ShardBound falhou.' }
    }
} finally {
    Pop-Location
}

Write-Output 'Criando plugin local pessoal para o ChatGPT Desktop...'
if (Test-Path -LiteralPath $PersonalPluginRoot) {
    Remove-Item -LiteralPath $PersonalPluginRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path (Join-Path $PersonalPluginRoot 'skills\blockbench') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $PersonalPluginRoot '.codex-plugin') | Out-Null

$pluginManifest = [ordered]@{
    '$schema' = 'https://agent-plugins.org/schemas/1.0.0/plugin.schema.json'
    name = $PluginName
    version = '0.3.3'
    description = 'Local ChatGPT Desktop bridge for controlling the owner''s Blockbench Generic Model.'
    author = [ordered]@{ name = 'ShardBound' }
    extensions = [ordered]@{
        'com.openai' = [ordered]@{
            interface = [ordered]@{
                displayName = $DisplayName
                shortDescription = 'Blockbench local via MCP'
                longDescription = 'Conecta o ChatGPT Desktop diretamente ao ShardBound MCP local para inspecionar e editar Generic Models no Blockbench.'
                developerName = 'ShardBound'
                category = 'Developer Tools'
                capabilities = @('Read', 'Write')
                defaultPrompt = 'Execute status no meu Blockbench e depois inspecione o Generic Model aberto.'
            }
        }
    }
}
Write-Utf8NoBom (Join-Path $PersonalPluginRoot 'plugin.json') ($pluginManifest | ConvertTo-Json -Depth 12)

$mcp = [ordered]@{
    '$schema' = 'https://agent-plugins.org/schemas/1.0.0/mcp.schema.json'
    mcpServers = [ordered]@{
        shardbound_blockbench = [ordered]@{
            type = 'stdio'
            command = $nodePath
            args = @((Join-Path $BridgeRoot 'mcp\server.mjs'))
            cwd = $BridgeRoot
        }
    }
}
Write-Utf8NoBom (Join-Path $PersonalPluginRoot 'mcp.json') ($mcp | ConvertTo-Json -Depth 10)

$legacyMcp = [ordered]@{
    mcpServers = [ordered]@{
        shardbound_blockbench = [ordered]@{
            command = $nodePath
            args = @((Join-Path $BridgeRoot 'mcp\server.mjs'))
            cwd = $BridgeRoot
        }
    }
}
Write-Utf8NoBom (Join-Path $PersonalPluginRoot '.mcp.json') ($legacyMcp | ConvertTo-Json -Depth 10)

$compat = [ordered]@{
    name = $PluginName
    version = '0.3.3'
    description = 'Local ChatGPT Desktop bridge for controlling the owner''s Blockbench Generic Model.'
    author = [ordered]@{ name = 'ShardBound' }
    skills = './skills/'
    mcpServers = './.mcp.json'
    interface = [ordered]@{
        displayName = $DisplayName
        shortDescription = 'Blockbench local via MCP'
        longDescription = 'Conecta o ChatGPT Desktop diretamente ao ShardBound MCP local para inspecionar e editar Generic Models no Blockbench.'
        developerName = 'ShardBound'
        category = 'Developer Tools'
        capabilities = @('Read', 'Write')
        defaultPrompt = 'Execute status no meu Blockbench e depois inspecione o Generic Model aberto.'
    }
}
Write-Utf8NoBom (Join-Path $PersonalPluginRoot '.codex-plugin\plugin.json') ($compat | ConvertTo-Json -Depth 12)

Copy-Item -LiteralPath (Join-Path $BridgeRoot 'skills\blockbench\SKILL.md') -Destination (Join-Path $PersonalPluginRoot 'skills\blockbench\SKILL.md') -Force

Write-Output 'Registrando marketplace pessoal do ChatGPT Desktop...'
$marketplaceDir = Split-Path -Parent $MarketplacePath
New-Item -ItemType Directory -Force -Path $marketplaceDir | Out-Null

$existingPlugins = @()
$marketplaceName = 'personal-local'
$marketplaceDisplayName = 'Personal Local Plugins'

if (Test-Path -LiteralPath $MarketplacePath) {
    try {
        $existing = Get-Content -LiteralPath $MarketplacePath -Raw | ConvertFrom-Json
        if ($existing.PSObject.Properties['name'] -and -not [string]::IsNullOrWhiteSpace([string]$existing.name)) {
            $marketplaceName = [string]$existing.name
        }
        if ($existing.PSObject.Properties['interface'] -and
            $existing.interface -and
            $existing.interface.PSObject.Properties['displayName'] -and
            -not [string]::IsNullOrWhiteSpace([string]$existing.interface.displayName)) {
            $marketplaceDisplayName = [string]$existing.interface.displayName
        }
        if ($existing.PSObject.Properties['plugins'] -and $existing.plugins) {
            $existingPlugins = @($existing.plugins | Where-Object { $_.name -ne $PluginName })
        }
    } catch {
        $backup = "$MarketplacePath.backup-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
        Copy-Item -LiteralPath $MarketplacePath -Destination $backup -Force
        Write-Warning "marketplace.json existente era invalido; backup criado em $backup"
    }
}

$entry = [ordered]@{
    name = $PluginName
    source = [ordered]@{
        source = 'local'
        path = "./.codex/plugins/$PluginName"
    }
    policy = [ordered]@{
        installation = 'AVAILABLE'
        authentication = 'ON_INSTALL'
    }
    category = 'Developer Tools'
}

$marketplaceOut = [ordered]@{
    name = $marketplaceName
    interface = [ordered]@{
        displayName = $marketplaceDisplayName
    }
    plugins = @($existingPlugins + $entry)
}

Write-Utf8NoBom $MarketplacePath ($marketplaceOut | ConvertTo-Json -Depth 12)

# Re-read what was actually written. Do not claim success unless the entry exists.
$verifyMarketplace = Get-Content -LiteralPath $MarketplacePath -Raw | ConvertFrom-Json
$verifyEntry = @($verifyMarketplace.plugins | Where-Object { $_.name -eq $PluginName })
if ($verifyEntry.Count -ne 1) {
    throw "Falha ao registrar $PluginName em $MarketplacePath. O arquivo foi gravado, mas a entrada nao foi encontrada na verificacao."
}
Write-Output "Marketplace registrado: $PluginName"
Write-Host ''
Write-Output 'ShardBound Blockbench Local foi registrado como plugin local do ChatGPT Desktop.'
Write-Output "Plugin source: $PersonalPluginRoot"
Write-Output "Marketplace: $MarketplacePath"
Write-Output "MCP runtime: $nodePath $((Join-Path $BridgeRoot 'mcp\server.mjs'))"
Write-Host ''
Write-Output 'Agora feche completamente o ChatGPT Desktop, abra novamente e procure por:'
Write-Output "  $DisplayName"
Write-Output 'Nao use o plugin de conta "ShardBound Blockbench" neste teste.'
