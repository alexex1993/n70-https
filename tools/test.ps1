param([switch]$Live)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio C++ build tools (MSVC and AddressSanitizer)' }
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC C++ tools not found' }
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
if (!(Test-Path 'vendor\zlib-1.4\zlib.h')) { throw 'Run tools\prepare.ps1 first' }
if (!(Test-Path 'vendor\bearssl-0.6\build\bearssls.lib')) { throw 'Build the native Windows BearSSL static library first; see tests/README.md' }
New-Item -ItemType Directory -Force build\host,tests\results\host | Out-Null
$zsources='vendor\zlib-1.4\inflate.c vendor\zlib-1.4\inftrees.c vendor\zlib-1.4\inffast.c vendor\zlib-1.4\zutil.c vendor\zlib-1.4\adler32.c vendor\zlib-1.4\crc32.c'
$zflags='/DZ_SOLO /DZLIB_CONST /DZ_PREFIX /Ivendor\zlib-1.4'
$asan='/Fdbuild\host\vc.pdb /nologo /O1 /fsanitize=address /Zi'
$lines=@('@echo off',('call "'+$vcvars+'" >nul'),('cd /d "'+$root+'"'))
foreach ($entry in @(
    @('gzip',"$zflags /Ilibrary\src tests\gzip.c library\src\n70gzip.c $zsources"),
    @('headers','/Ilibrary\src tests\headers.c library\src\n70headers.c'),
    @('wait','/std:c++17 /EHsc /Itests\wait_shim /Ilibrary\src tests\wait.cpp library\src\n70wait.cpp'),
    @('cancel','/std:c++17 /EHsc /Itests\cancel_shim /Itests\wait_shim /Ilibrary\src tests\cancel.cpp library\src\n70wait.cpp examples\N70Test\src\testcancel.cpp'),
    @('http_core','/Ilibrary\src tests\http_core.c library\src\n70http.c'),
    @('policy','/Ilibrary\src tests\policy.c library\src\n70policy.c'),
    @('connect','/Ilibrary\src tests\connect.c library\src\n70connect.c library\src\n70policy.c')
)) {
    $lines+="cl $asan $($entry[1]) /Fobuild\host\ /Febuild\host\$($entry[0]).exe"
    $lines+='if errorlevel 1 exit /b 1'
    $lines+="build\host\$($entry[0]).exe"
    $lines+='if errorlevel 1 exit /b 1'
}
$lines+='cl '+$asan+' /Ilibrary\src /Iexamples\N70Test\inc /Ivendor\bearssl-0.6\inc tests\trust.c library\src\n70trust.c /Fobuild\host\ /Febuild\host\trust.exe /link vendor\bearssl-0.6\build\bearssls.lib'
$lines+='if errorlevel 1 exit /b 1'
$lines+='build\host\trust.exe'
$lines+='if errorlevel 1 exit /b 1'
if ($Live) {
    $lines+="cl /nologo /O2 $zflags /Ivendor\bearssl-0.6\inc /Iexamples\N70Test\inc /Ilibrary\include /Ilibrary\src tests\live_http.c library\src\n70entropy_core.c library\src\n70http.c library\src\n70headers.c library\src\n70gzip.c $zsources /Fobuild\host\ /Febuild\host\live_http.exe /link vendor\bearssl-0.6\build\bearssls.lib"
    $lines+='if errorlevel 1 exit /b 1'
    $lines+='build\host\live_http.exe'
    $lines+='if errorlevel 1 exit /b 1'
    $lines+="cl /nologo /O2 $zflags /Ivendor\bearssl-0.6\inc /Iexamples\N70Test\inc /Ilibrary\include /Ilibrary\src tests\live_reuse.c library\src\n70entropy_core.c library\src\n70http.c library\src\n70headers.c library\src\n70gzip.c library\src\n70policy.c $zsources /Fobuild\host\ /Febuild\host\live_reuse.exe /link vendor\bearssl-0.6\build\bearssls.lib"
    $lines+='if errorlevel 1 exit /b 1'
    $lines+='build\host\live_reuse.exe'
    $lines+='if errorlevel 1 exit /b 1'
}
$lines+='exit /b 0'
$cmdPath=Join-Path $root 'build\host\test.cmd'
[IO.File]::WriteAllText($cmdPath,($lines -join "`r`n"),[Text.Encoding]::ASCII)
& cmd.exe /d /c $cmdPath 2>&1 | Tee-Object -FilePath (Join-Path $root 'tests\results\host\host-http09-results.txt')
if ($LASTEXITCODE) { throw "Host checks failed ($LASTEXITCODE)" }
