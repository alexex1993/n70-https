$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio C++ Build Tools first' }
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC C++ tools not found' }
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
New-Item -ItemType Directory -Force (Join-Path $root 'build\host'),(Join-Path $root 'build\logs') | Out-Null
$commands=@('@echo off',('call "'+$vcvars+'" >nul'),'if errorlevel 1 exit /b 1',('cd /d "'+$root+'\vendor\bearssl-0.6"'),'nmake 2>&1','exit /b %errorlevel%')
$cmdPath=Join-Path $root 'build\host\build-bearssl.cmd'
[IO.File]::WriteAllText($cmdPath,($commands -join "`r`n"),[Text.Encoding]::ASCII)
& cmd.exe /d /c $cmdPath 2>&1 | Tee-Object -FilePath (Join-Path $root 'build\logs\bearssl-host.log')
if ($LASTEXITCODE) { throw 'Native BearSSL build failed' }
