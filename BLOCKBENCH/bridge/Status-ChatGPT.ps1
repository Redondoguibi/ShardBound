[CmdletBinding()]
param([switch]$Verify, [switch]$Json, [switch]$RequireChatHost)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'Status-ChatGPT-Desktop-Plugin.ps1') -Json:$Json -RequireChatHost:$RequireChatHost
exit $LASTEXITCODE
