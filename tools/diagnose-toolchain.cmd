@echo off
setlocal
cd /d "%~dp0.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\prepare.ps1
if errorlevel 1 exit /b 1
call build\generated\diagnose-toolchain.cmd
exit /b %errorlevel%
