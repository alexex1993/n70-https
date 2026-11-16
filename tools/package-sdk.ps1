$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$stage=Join-Path $root 'dist\N70HttpsSDK'
New-Item -ItemType Directory -Force "$stage\include","$stage\lib","$stage\examples","$stage\docs","$stage\skills" | Out-Null
foreach ($header in 'n70https.h','n70httpclient.h','n70entropy.h') {
    Copy-Item (Join-Path $root "library\include\$header") "$stage\include" -Force
}
Copy-Item (Join-Path $root 'library\compat\stdint.h') "$stage\include" -Force
Copy-Item (Join-Path $root 'vendor\bearssl-0.6\inc\*.h') "$stage\include" -Force
Copy-Item (Join-Path $root 'dist\n70https.lib') "$stage\lib" -Force
Copy-Item (Join-Path $root 'vendor\bearssl-0.6\LICENSE.txt') "$stage\BearSSL-LICENSE.txt" -Force
Copy-Item (Join-Path $root 'vendor\zlib-1.4\LICENSE') "$stage\zlib-LICENSE.txt" -Force
foreach ($doc in 'SDK_INTEGRATION.md','request-example.cpp','HTTP_API.md','PHONE_TESTS.md','N70_RESULTS_0.9.0.md','N70_RESULTS_0.9.1.md') {
    Copy-Item (Join-Path $root "docs\$doc") "$stage\docs" -Force
}
Copy-Item (Join-Path $root 'examples\N70Test\inc\testanchor.h') "$stage\examples\testanchor.h" -Force
Copy-Item (Join-Path $root 'library\include\n70testsites.h') "$stage\examples\n70testsites.h" -Force
Copy-Item (Join-Path $root 'library\certs') "$stage" -Recurse -Force
Copy-Item (Join-Path $root 'skills\n70-https') "$stage\skills" -Recurse -Force
$readme=@'
# N70 HTTPS SDK 0.9.1

TLS 1.2 and HTTP/1.1 for Nokia N70 / S60 2nd Edition FP3, Symbian OS 8.1a.

- `lib/n70https.lib`: THUMB UREL static library, including BearSSL 0.6 and zlib 1.4.
- `include/`: public and dependency headers needed by consuming applications.
- `examples/testanchor.h`: demo trust roots; verify coverage for your service.
- [Integration guide](docs/SDK_INTEGRATION.md) and [API reference](docs/HTTP_API.md).
- [Latest N70 results](docs/N70_RESULTS_0.9.1.md).
- `skills/n70-https/`: copy to your agent's skills directory, invoke `$n70-https`.

Use a worker thread with a 64 KiB stack. The library is linked into your app;
no separate TLS DLL or library SIS is installed on the phone. Its automatic
entropy initialization is experimental and has not had a cryptographic assessment.
'@
[IO.File]::WriteAllText((Join-Path $stage 'README.md'),$readme,[Text.UTF8Encoding]::new($false))
Compress-Archive -Path $stage -DestinationPath (Join-Path $root 'dist\N70HttpsSDK.zip') -Force
Write-Host 'Created dist\N70HttpsSDK.zip'
