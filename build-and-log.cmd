@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\build-visible.ps1
set N70_BUILD_RESULT=%errorlevel%
echo.
echo Build exit: %N70_BUILD_RESULT%
echo Log: %~dp0build\logs\build.log
if not "%N70_BUILD_RESULT%"=="0" goto failed
echo Outputs: dist\N70Test.sis and dist\n70https.lib
goto done
:failed
echo Build failed. Please share build\logs\build.log.
:done
pause
exit /b %N70_BUILD_RESULT%
