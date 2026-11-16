param([switch]$Live)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'test.ps1') -Live:$Live
