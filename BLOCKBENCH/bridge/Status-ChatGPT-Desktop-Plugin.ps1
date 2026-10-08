[CmdletBinding()]
param([switch]$Json, [switch]$RequireChatHost)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'scripts\SecureTunnel-Common.ps1')
Ensure-LocalDirectory
try { $null = Get-SafeTunnelStatus }
catch {
    $safe = @{ checkedAt = (Get-Date).ToUniversalTime().ToString('o'); process_running = $false; healthy = $false; ready = $false; error = $_.Exception.Message }
    [IO.File]::WriteAllText((Join-Path $script:LocalDir 'tunnel-status-safe.json'), ($safe | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
}
$desktop = [ordered]@{}
try {
    $installed = Get-AppxPackage -Name 'OpenAI.Codex' | Select-Object -First 1
    $desktop.version = [string]$installed.Version
    $desktop.installLocation = $installed.InstallLocation
    $desktop.appRunning = [bool](Get-Process ChatGPT -ErrorAction SilentlyContinue)
    $desktop.bridgeNodes = @(Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'node.exe' -and $_.CommandLine -like '*ShardBound*bridge*mcp*server.mjs*' } | Select-Object ProcessId,ParentProcessId,ExecutablePath)
    $logRoot = Join-Path $env:LOCALAPPDATA 'Packages\OpenAI.Codex_2p2nqsd0c76g0\LocalCache\Local\Codex\Logs'
    $files = @(Get-ChildItem -LiteralPath $logRoot -Filter '*.log' -Recurse -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 15)
    $desktop.recentLogs = @($files | ForEach-Object {
        $matches = @(Select-String -LiteralPath $_.FullName -Pattern 'server=shardbound_blockbench\b.*status=ready|server=shardbound_blockbench-2\b' -ErrorAction SilentlyContinue)
        if ($matches.Count) { @{ path = $_.FullName; readyStdioEvents = @($matches | Where-Object { $_.Line -match 'server=shardbound_blockbench\b.*status=ready' }).Count; obsoleteConnectionEvents = @($matches | Where-Object { $_.Line -match 'shardbound_blockbench-2' }).Count } }
    })
} catch { $desktop.error = $_.Exception.Message }
[IO.File]::WriteAllText((Join-Path $script:LocalDir 'desktop-installation-safe.json'), ($desktop | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
$node = Get-NodeExecutable
$arguments = @((Join-Path $PSScriptRoot 'scripts\diagnose-chatgpt.mjs'))
if ($Json) { $arguments += '--json' }
if ($RequireChatHost) { $arguments += '--require-chat-host' }
& $node @arguments
exit $LASTEXITCODE
