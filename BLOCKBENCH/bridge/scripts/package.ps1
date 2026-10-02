param([switch]$BindCurrentMachine)
$ErrorActionPreference = 'Stop'
$bridgeRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$stage = Join-Path $bridgeRoot ('.local/package-' + [guid]::NewGuid().ToString('N'))
$packageRoot = Join-Path $stage 'shardbound-blockbench'
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
# Explicit allowlist: never package pairing credentials, local models or node_modules.
$entries = @('.gitignore', '.codex-plugin', '.mcp.json', 'plugin.json', 'mcp.json', 'package.json', 'package-lock.json', 'README.md', 'CHATGPT-DESKTOP.md', 'Install.ps1', 'mcp', 'shared', 'plugin-src', 'plugin', 'scripts', 'skills', 'tests')
foreach ($entry in $entries) {
    Copy-Item -LiteralPath (Join-Path $bridgeRoot $entry) -Destination $packageRoot -Recurse
}
if ($BindCurrentMachine) {
    # A private Desktop install reuses this prepared runtime and its local pairing.
    # Only paths are serialized; credentials stay in the original .local folder.
    $mcpBinding = [ordered]@{
        '$schema' = 'https://agent-plugins.org/schemas/1.0.0/mcp.schema.json'
        mcpServers = [ordered]@{
            shardbound_blockbench = [ordered]@{
                type = 'stdio'
                command = (Get-Command node -CommandType Application | Select-Object -First 1).Source
                args = @((Join-Path $bridgeRoot 'mcp/server.mjs'))
            }
        }
    }
    $json = $mcpBinding | ConvertTo-Json -Depth 8
    [IO.File]::WriteAllText((Join-Path $packageRoot 'mcp.json'), $json, [Text.UTF8Encoding]::new($false))
    $legacyBinding = [ordered]@{ mcpServers = [ordered]@{ shardbound_blockbench = [ordered]@{ command = $mcpBinding.mcpServers.shardbound_blockbench.command; args = $mcpBinding.mcpServers.shardbound_blockbench.args } } }
    [IO.File]::WriteAllText((Join-Path $packageRoot '.mcp.json'), ($legacyBinding | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $packageRoot 'DESKTOP-BINDING.md'), "This private Desktop package uses the prepared Node runtime at $bridgeRoot. Keep that folder in place. Pairing secrets remain local and are excluded from this archive.", [Text.UTF8Encoding]::new($false))
}
$archive = Join-Path $stage 'shardbound-blockbench.zip'
Compress-Archive -LiteralPath $packageRoot -DestinationPath $archive
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $names = @($zip.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
    foreach ($required in @('plugin.json', 'mcp.json', '.mcp.json', '.codex-plugin/plugin.json', 'mcp/server.mjs', 'README.md')) {
        if ($names -notcontains ('shardbound-blockbench/' + $required)) { throw "Archive missing $required" }
    }
    if ($names | Where-Object { $_ -match '/(node_modules|\.local|outputs|\.reference)/|chatgpt-password|codex-config|config\.json$' }) {
        throw 'Archive includes private or generated runtime files.'
    }
} finally { $zip.Dispose() }
Write-Output "Source package verified: $archive"
