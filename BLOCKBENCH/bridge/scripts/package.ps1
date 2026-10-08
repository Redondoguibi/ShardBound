param([switch]$BindCurrentMachine, [string]$RemoteUrl)
$ErrorActionPreference = 'Stop'
if ($BindCurrentMachine -or $RemoteUrl) { throw 'Este pacote privado usa .app.json + Secure MCP Tunnel. Para um cliente stdio use .local/codex-config.toml, nao publique caminhos de maquina no plugin de conta.' }
$bridgeRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$manifest = Get-Content -LiteralPath (Join-Path $bridgeRoot 'plugin.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$compat = Get-Content -LiteralPath (Join-Path $bridgeRoot '.codex-plugin/plugin.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$app = Get-Content -LiteralPath (Join-Path $bridgeRoot '.app.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($manifest.extensions.'com.openai'.apps -ne './.app.json' -or $compat.apps -ne './.app.json' -or $app.apps.shardbound_blockbench.required -ne $true -or $app.apps.shardbound_blockbench.id -notmatch '^asdk_app_[a-f0-9]{32}$') {
    throw 'Vinculo de app ausente ou invalido. Registre a conexao Tunnel e use seu ID verificado.'
}
if ($manifest.version -ne $compat.version -or $manifest.name -ne $compat.name) { throw 'Manifests portatil/compat divergentes.' }
$stage = Join-Path $bridgeRoot ('.local/package-' + [guid]::NewGuid().ToString('N'))
$packageRoot = Join-Path $stage 'shardbound-blockbench'
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
$entries = @('.gitignore', '.codex-plugin', '.app.json', '.mcp.json', 'plugin.json', 'mcp.json', 'package.json', 'package-lock.json', 'README.md', 'CHATGPT-DESKTOP.md', 'INTEGRATION-AUDIT.md', 'TOOLS-REFERENCE.md', 'Install.ps1', 'Install-ChatGPT-Desktop-Plugin.ps1', 'Status-ChatGPT-Desktop-Plugin.ps1', 'Status-ChatGPT.ps1', 'Disconnect-ChatGPT.ps1', 'Connect-ChatGPT.ps1', 'mcp', 'shared', 'plugin-src', 'plugin', 'scripts', 'skills', 'tests')
foreach ($entry in $entries) { Copy-Item -LiteralPath (Join-Path $bridgeRoot $entry) -Destination $packageRoot -Recurse }
[IO.File]::WriteAllText((Join-Path $packageRoot 'DESKTOP-BINDING.md'), 'This private account plugin binds the registered ShardBound OpenAI Tunnel app through .app.json. Node and Blockbench run on the owner PC. Secrets remain in the original local .local directory. Run Install-ChatGPT-Desktop-Plugin.ps1 there.', [Text.UTF8Encoding]::new($false))
$archive = Join-Path $stage 'shardbound-blockbench.zip'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$outputZip = [IO.Compression.ZipFile]::Open($archive, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($sourceFile in (Get-ChildItem -LiteralPath $packageRoot -File -Recurse -Force)) {
        $relativePath = $sourceFile.FullName.Substring($packageRoot.Length + 1).Replace('\', '/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($outputZip, $sourceFile.FullName, ('shardbound-blockbench/' + $relativePath), [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $outputZip.Dispose() }
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $names = @($zip.Entries | ForEach-Object { $_.FullName })
    if ($names | Where-Object { $_.Contains('\') }) { throw 'ZIP paths must use forward slashes.' }
    foreach ($required in @('plugin.json', '.app.json', 'mcp.json', '.mcp.json', '.codex-plugin/plugin.json', 'mcp/server.mjs', 'mcp/audit.mjs', 'skills/blockbench/SKILL.md')) {
        if ($names -notcontains ('shardbound-blockbench/' + $required)) { throw "Archive missing $required" }
    }
    if ($names | Where-Object { $_ -match '/(node_modules|\.local|outputs|\.reference)/|chatgpt-password|codex-config|config\.json$|\.dpapi$' }) { throw 'Archive includes private/generated files.' }
} finally { $zip.Dispose() }
Write-Output "Source package verified: $archive"
