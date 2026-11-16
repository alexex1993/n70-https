param([string]$Sdk='C:\Symbian\8.1a\S60_2nd_FP3', [string]$Perl='C:\Perl\bin')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
function Ascii($path,$text) { [IO.File]::WriteAllText((Join-Path $root $path),$text,[Text.Encoding]::ASCII) }
New-Item -ItemType Directory -Force 'build/generated','build/logs','dist','vendor/archives' | Out-Null
$bear=Join-Path $root 'vendor\bearssl-0.6'
if (!(Test-Path "$bear\inc\bearssl.h")) {
    $archive=Join-Path $root 'vendor\archives\bearssl-0.6.tar.gz'
    if (!(Test-Path $archive)) { Invoke-WebRequest -UseBasicParsing 'https://bearssl.org/bearssl-0.6.tar.gz' -OutFile $archive }
    if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne '6705BBA1714961B41A728DFC5DEBBE348D2966C117649392F8C8139EFC83FF14') { throw 'Unexpected BearSSL archive hash' }
    & tar -xzf $archive -C vendor
    if ($LASTEXITCODE) { throw 'BearSSL extraction failed' }
}
if (!(Test-Path "$Sdk\Epoc32\tools\bldmake.bat")) { throw 'S60 2nd FP3 SDK not found' }
$zlib=Join-Path $root 'vendor\zlib-1.4'
if (!(Test-Path "$zlib\zlib.h")) {
    $zarchive=Join-Path $root 'vendor\archives\zlib-1.4.tar.gz'
    if (!(Test-Path $zarchive)) { Invoke-WebRequest -UseBasicParsing 'https://zlib.net/zlib-1.4.tar.gz' -OutFile $zarchive }
    if ((Get-FileHash $zarchive -Algorithm SHA256).Hash -ne '02DD159783677A749C04ADBDD02B55B8A2BCC7E054F51B76B549199A916AD584') { throw 'Unexpected zlib archive hash' }
    & tar -xzf $zarchive -C vendor
    if ($LASTEXITCODE) { throw 'zlib extraction failed' }
}
# S60 2nd application DLLs cannot contain writable global data. These
# lookup tables are read-only; qualify the pointers as well as their targets.
foreach ($patch in @(
    @('src\ssl\ssl_client_full.c','static const br_hash_class *hashes[]','static const br_hash_class * const hashes[]'),
    @('src\ssl\ssl_hs_client.c','static const unsigned char *HASH_OID[]','static const unsigned char * const HASH_OID[]'),
    @('src\ssl\ssl_hs_client.t0','static const unsigned char *HASH_OID[]','static const unsigned char * const HASH_OID[]')
)) {
    $path=Join-Path $bear $patch[0]
    if (Test-Path $path) {
        $old=[IO.File]::ReadAllText($path)
        $new=$old.Replace($patch[1],$patch[2])
        if ($new -ne $old) { [IO.File]::WriteAllText($path,$new,[Text.Encoding]::ASCII) }
    }
}
if ($root.Contains(' ')) { throw 'Project path must have no spaces' }
# Use a private SDK copy: all compiler outputs stay in the workspace.
$local=Join-Path $root 'build\sdk'
New-Item -ItemType Directory -Force "$local\Epoc32" | Out-Null
foreach ($part in 'include','gcc','tools','Data','release') {
    & robocopy "$Sdk\Epoc32\$part" "$local\Epoc32\$part" /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "SDK copy failed: $part" }
}
# Legacy make compares timestamps only: archive sources can be older than cached
# objects even when the dependency version changed. Invalidate only this port's
# affected object files once per zlib version; never delete SDK source trees.
$zstamp=Join-Path $local 'n70-zlib-version.txt'
$zversion='1.4:02DD159783677A749C04ADBDD02B55B8A2BCC7E054F51B76B549199A916AD584'
if (!(Test-Path $zstamp) -or ([IO.File]::ReadAllText($zstamp) -ne $zversion)) {
    $buildRoot=Join-Path $local 'Epoc32\BUILD'
    if (Test-Path $buildRoot) {
        $affected=@('inflate','inftrees','inffast','zutil','adler32','crc32','n70gzip','n70request')
        Get-ChildItem -LiteralPath $buildRoot -Recurse -Filter '*.o' | Where-Object {
            $_.FullName -match '\\GROUP\\N70HTTPS\\THUMB\\' -and $affected -contains $_.BaseName
        } | ForEach-Object { Remove-Item -LiteralPath $_.FullName }
    }
    [IO.File]::WriteAllText($zstamp,$zversion,[Text.Encoding]::ASCII)
}
# Use explicit pinned CA roots, never the server leaf or a downloaded intermediate.
$rootPins=@(
    @('isrg-root-x1.der','CABD2A79A1076A31F21D253635CB039D4329A5E8'),
    @('digicert-global-root-g2.der','DF3C24F9BFD666761B268073FE06D1CC8D4F82A4'),
    @('sslcom-tls-ecc-root-2022.der','9F5FD91A546DF50C71F0EE7ABD1749988473E239'),
    @('globalsign-root-ca.der','B1BC968BD4F49D622AA89A81F2150152A41D829C'),
    @('globalsign-root-r3.der','D69B561148F01C77C54578C10926DF5B856976AD'),
    @('usertrust-ecc.der','D1CBCA5DB2D52A7F693B674DE5F05A1D0C957DF0'),
    @('usertrust-rsa.der','2B8F1B57330DBBA2D07A6C51F70EE90DDAB9AD8E'),
    @('gts-root-r4.der','2A1D6027D94AB10A1C4D915CCD33A0CB3E2D54CB')
)
$certs=@(foreach ($pin in $rootPins) {
    $cert=New-Object Security.Cryptography.X509Certificates.X509Certificate2 (Join-Path $root ('library\certs\'+$pin[0]))
    if ($cert.Thumbprint -ne $pin[1]) { throw "Unexpected root certificate: $($pin[0])" }
    $cert
})
function Bytes($b) { ($b | ForEach-Object { '0x{0:x2}' -f $_ }) -join ',' }
$header=''; $setup=@(); $i=0
foreach ($cert in $certs) {
    $header+="static const unsigned char dn$i[] = { $(Bytes $cert.SubjectName.RawData) };`r`n"
    $setup+="a[$i].dn.data=(unsigned char*)dn$i; a[$i].dn.len=sizeof dn$i; a[$i].flags=BR_X509_TA_CA;"
    if ($cert.PublicKey.Oid.Value -eq '1.2.840.113549.1.1.1') {
        $rsa=$cert.PublicKey.Key.ExportParameters($false)
        foreach ($item in @(@('n',$rsa.Modulus),@('e',$rsa.Exponent))) {
            $header+="static const unsigned char $($item[0])$i[] = { $(Bytes $item[1]) };`r`n"
        }
        $setup+="a[$i].pkey.key_type=BR_KEYTYPE_RSA; a[$i].pkey.key.rsa.n=(unsigned char*)n$i; a[$i].pkey.key.rsa.nlen=sizeof n$i; a[$i].pkey.key.rsa.e=(unsigned char*)e$i; a[$i].pkey.key.rsa.elen=sizeof e$i;"
    } elseif ($cert.PublicKey.Oid.Value -eq '1.2.840.10045.2.1') {
        $oid=[BitConverter]::ToString($cert.PublicKey.EncodedParameters.RawData)
        $curve=switch ($oid) {
            '06-08-2A-86-48-CE-3D-03-01-07' { 'BR_EC_secp256r1' }
            '06-05-2B-81-04-00-22' { 'BR_EC_secp384r1' }
            '06-05-2B-81-04-00-23' { 'BR_EC_secp521r1' }
            default { throw "Unsupported root EC curve: $oid" }
        }
        $header+="static const unsigned char q$i[] = { $(Bytes $cert.PublicKey.EncodedKeyValue.RawData) };`r`n"
        $setup+="a[$i].pkey.key_type=BR_KEYTYPE_EC; a[$i].pkey.key.ec.curve=$curve; a[$i].pkey.key.ec.q=(unsigned char*)q$i; a[$i].pkey.key.ec.qlen=sizeof q$i;"
    } else {
        throw "Unsupported root key: $($cert.PublicKey.Oid.Value)"
    }
    $i++
}
$header+="#define TEST_ANCHOR_COUNT $i`r`nstatic void InitTestAnchors(br_x509_trust_anchor* a) {`r`n"+($setup -join "`r`n")+"`r`n}`r`n"
Ascii 'examples\N70Test\inc\testanchor.h' $header
$mmp=@('TARGET n70https.lib','TARGETTYPE lib','SOURCEPATH ..\src','SOURCE n70https.cpp n70entropy.cpp n70entropy_core.c n70http.c n70request.cpp n70httpclient.cpp n70control.c n70headers.c n70gzip.c n70wait.cpp n70policy.c n70connect.c n70trust.c n70socket.cpp',
    'USERINCLUDE ..\include ..\..\vendor\bearssl-0.6\inc ..\..\vendor\bearssl-0.6\src ..\compat',
    'SYSTEMINCLUDE ..\compat ..\..\vendor\bearssl-0.6\inc ..\..\vendor\zlib-1.4 \epoc32\include \epoc32\include\libc','MACRO BR_NO_ASM=1 BR_USE_URANDOM=0 BR_USE_WIN32_RAND=0 Z_SOLO ZLIB_CONST Z_PREFIX')
$mmp+=@('SOURCEPATH ..\..\vendor\zlib-1.4','SOURCE inflate.c inftrees.c inffast.c zutil.c adler32.c crc32.c')
Get-ChildItem "$bear\src" -Recurse -Filter *.c | Sort-Object FullName | ForEach-Object {
    $relative=$_.DirectoryName.Substring($root.Length+1)
    $mmp+="SOURCEPATH ..\..\$relative"; $mmp+="SOURCE $($_.Name)"
}
Ascii 'library\group\n70https.mmp' ($mmp -join "`r`n")
Ascii 'library\group\bld.inf' "PRJ_MMPFILES`r`nn70https.mmp`r`n"
# The reference application MMP uses paths relative to examples/N70Test/group.
$epoc=$local.Substring(2)+'\'
$envBlock="@echo off`r`nsetlocal`r`nset EPOCROOT=$epoc`r`nset PATH=$Perl;$local\Epoc32\gcc\bin;$local\Epoc32\tools;%PATH%`r`n"
$diagnostic=$envBlock+"cd /d `"$root`"`r`nthumb-epoc-pe-gcc.exe -dumpmachine > build\logs\toolchain-diagnostic.txt 2>&1`r`necho GCC exit: %errorlevel% >> build\logs\toolchain-diagnostic.txt`r`ncpp.exe -P tools\preprocess-probe.c >> build\logs\toolchain-diagnostic.txt 2>&1`r`necho CPP preprocessing exit: %errorlevel% >> build\logs\toolchain-diagnostic.txt`r`ntype build\logs\toolchain-diagnostic.txt`r`npause`r`n"
Ascii 'build\generated\diagnose-toolchain.cmd' $diagnostic
# Verify the unchanged SDK example before compiling the port.
$smoke=Join-Path $root 'build\sdk-smoke'
if (!(Test-Path "$smoke\group\helloworldbasic.mmp")) {
    Copy-Item "$Sdk\Series60Ex\helloworldbasic" $smoke -Recurse
}
$build=$envBlock
$build+="thumb-epoc-pe-gcc.exe -dumpmachine`r`nif not %errorlevel%==0 exit /b 1`r`n"
$build+="cd /d `"$root`"`r`ncpp.exe -P tools\preprocess-probe.c`r`nif not %errorlevel%==0 exit /b 1`r`n"
foreach ($group in @('build\sdk-smoke\group','library\group','examples\N70Test\group')) {
    $artifact=switch ($group) {
        'build\sdk-smoke\group' { "$local\Epoc32\release\thumb\urel\HelloWorldBasic.app" }
        'library\group' { "$local\Epoc32\release\thumb\urel\n70https.lib" }
        'examples\N70Test\group' { "$local\Epoc32\release\thumb\urel\N70Test.app" }
    }
    $abldDir=$root.Substring(2)+'\'+$group
    $build+="echo Building $group`r`ncd /d `"$root\$group`"`r`nif exist `"$artifact`" del /q `"$artifact`"`r`n`"$Perl\perl.exe`" -S bldmake.pl bldfiles`r`nif not %errorlevel%==0 exit /b 1`r`n`"$Perl\perl.exe`" -S abld.pl `"$abldDir\\`" build thumb urel`r`nif not %errorlevel%==0 exit /b 1`r`nif not exist `"$artifact`" exit /b 1`r`n"
}
$pkg="&EN`r`n#{`"N70 HTTPS`"},(0x70007121),0,9,1`r`n(0x101F7960),0,0,0,{`"Series60ProductID`"}`r`n"
foreach ($ext in @('app','rsc','aif')) {
    $source=if ($ext -eq 'app') { "$local\Epoc32\release\thumb\urel\N70Test.app" } else { "$local\Epoc32\Data\z\system\apps\n70test\N70Test.$ext" }
    $pkg+="`"$source`"-`"!:\system\apps\N70Test\N70Test.$ext`"`r`n"
}
$pkg+="`"$local\Epoc32\Data\z\system\apps\n70test\N70Test_caption.rsc`"-`"!:\system\apps\N70Test\N70Test_caption.rsc`"`r`n"
Ascii 'examples\N70Test\sis\n70test.pkg' $pkg
$build+="cd /d `"$root\examples\N70Test\sis`"`r`nmakesis n70test.pkg `"$root\dist\N70Test.sis`"`r`nif not %errorlevel%==0 exit /b 1`r`ncopy /y `"$local\Epoc32\release\thumb\urel\n70https.lib`" `"$root\dist\n70https.lib`"`r`n"
$build+="if not %errorlevel%==0 exit /b 1`r`npowershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$root\tools\package-sdk.ps1`"`r`nexit /b %errorlevel%`r`n"
Ascii 'build\generated\build.cmd' $build
Write-Host 'Prepared. Run .\build.cmd'
