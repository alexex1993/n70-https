$ErrorActionPreference='Continue'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
New-Item -ItemType Directory -Force 'build/logs' | Out-Null
Write-Host 'Building N70 HTTPS. Output is also saved to build\logs\build.log.'
& $env:ComSpec /d /c 'call build.cmd 2>&1' | Tee-Object -FilePath (Join-Path $root 'build\logs\build.log')
$result=$LASTEXITCODE
Write-Host "Build exit: $result"
Add-Content (Join-Path $root 'build\logs\build.log') "Build exit: $result"
if ($result -eq 0) {
    Write-Host 'Ready: dist\N70Test.sis and dist\n70https.lib'
} else {
    Write-Host 'Build failed. See build\logs\build.log.'
}
exit $result
