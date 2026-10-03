param([switch]$BindCurrentMachine, [string]$RemoteUrl)
$ErrorActionPreference = 'Stop'
if ($BindCurrentMachine -and $RemoteUrl) { throw 'Escolha somente uma configuracao: local ou remota.' }
if ($RemoteUrl) {
    $remoteUri = [uri]$RemoteUrl
    if (-not $remoteUri.IsAbsoluteUri -or $remoteUri.Scheme -ne 'https' -or $remoteUri.AbsolutePath -ne '/mcp' -or $remoteUri.Query -or $remoteUri.Fragment -or $remoteUri.UserInfo) {
        throw 'RemoteUrl deve ser uma URL HTTPS terminada em /mcp, sem credenciais.'
    }
}
$bridgeRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$stage = Join-Path $bridgeRoot ('.local/package-' + [guid]::NewGuid().ToString('N'))
$packageRoot = Join-Path $stage 'shardbound-blockbench'
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
# Explicit allowlist: never package pairing credentials, local models or node_modules.
$entries = @('.gitignore', '.codex-plugin', '.mcp.json', 'plugin.json', 'mcp.json', 'package.json', 'package-lock.json', 'README.md', 'MERGE-REVIEW.md', 'CHATGPT-DESKTOP.md', 'Install.ps1', 'Connect-ChatGPT.ps1', 'mcp', 'shared', 'plugin-src', 'plugin', 'scripts', 'skills', 'tests')
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
if ($RemoteUrl) {
    $binding = @{ '$schema' = 'https://agent-plugins.org/schemas/1.0.0/mcp.schema.json'; mcpServers = @{ shardbound_blockbench = @{ type = 'streamable-http'; url = $RemoteUrl } } }
    [IO.File]::WriteAllText((Join-Path $packageRoot 'mcp.json'), ($binding | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    $legacy = @{ mcpServers = @{ shardbound_blockbench = @{ type = 'http'; url = $RemoteUrl } } }
    [IO.File]::WriteAllText((Join-Path $packageRoot '.mcp.json'), ($legacy | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $packageRoot 'DESKTOP-BINDING.md'), 'This account package uses an HTTPS MCP endpoint protected by OAuth. The source repository and standalone Codex MCP keep their local stdio configuration. If a temporary tunnel restarts, update the endpoint and reconnect. Pairing credentials are never bundled.', [Text.UTF8Encoding]::new($false))
}
$archive = Join-Path $stage 'shardbound-blockbench.zip'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
# ZIP member paths must use '/' on every platform. Compress-Archive on Windows
# can emit backslashes, which the account importer rejects as unsafe paths.
$outputZip = [IO.Compression.ZipFile]::Open($archive, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($sourceFile in (Get-ChildItem -LiteralPath $packageRoot -File -Recurse -Force)) {
        $relativePath = $sourceFile.FullName.Substring($packageRoot.Length + 1).Replace('\', '/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($outputZip, $sourceFile.FullName, ('shardbound-blockbench/' + $relativePath), [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $outputZip.Dispose() }
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    if ($zip.Entries | Where-Object { $_.FullName.Contains('\') }) { throw 'Archive member paths must use forward slashes.' }
    $names = @($zip.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
    foreach ($required in @('plugin.json', 'mcp.json', '.mcp.json', '.codex-plugin/plugin.json', 'mcp/server.mjs', 'README.md')) {
        if ($names -notcontains ('shardbound-blockbench/' + $required)) { throw "Archive missing $required" }
    }
    if ($names | Where-Object { $_ -match '/(node_modules|\.local|outputs|\.reference)/|chatgpt-password|codex-config|config\.json$' }) {
        throw 'Archive includes private or generated runtime files.'
    }
} finally { $zip.Dispose() }
Write-Output "Source package verified: $archive"
