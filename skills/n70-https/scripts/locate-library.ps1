param([string]$Root)
$ErrorActionPreference='Stop'
$candidates=New-Object 'Collections.Generic.List[string]'
if ($Root) { $candidates.Add($Root) }
if ($env:N70_HTTPS_ROOT) { $candidates.Add($env:N70_HTTPS_ROOT) }
foreach ($start in @((Get-Location).Path,$PSScriptRoot)) {
    $cursor=[IO.DirectoryInfo]$start
    while ($cursor) { $candidates.Add($cursor.FullName); $cursor=$cursor.Parent }
}
$hint=Join-Path (Split-Path $PSScriptRoot -Parent) 'local-library.json'
if (Test-Path -LiteralPath $hint) {
    $configured=Get-Content -LiteralPath $hint -Raw | ConvertFrom-Json
    if ($configured.root) { $candidates.Add([string]$configured.root) }
}
foreach ($candidate in $candidates) {
    foreach ($suffix in @('','dist\N70HttpsSDK','N70HttpsSDK')) {
        $path=if ($suffix) { Join-Path $candidate $suffix } else { $candidate }
        if (Test-Path -LiteralPath (Join-Path $path 'library\include\n70https.h')) {
            [pscustomobject]@{Kind='Source';Root=[IO.Path]::GetFullPath($path);SDK=(Join-Path $path 'dist\N70HttpsSDK');Header=(Join-Path $path 'library\include\n70https.h')}
            return
        }
        if ((Test-Path -LiteralPath (Join-Path $path 'include\n70https.h')) -and (Test-Path -LiteralPath (Join-Path $path 'lib\n70https.lib'))) {
            [pscustomobject]@{Kind='SDK';Root=[IO.Path]::GetFullPath($path);SDK=[IO.Path]::GetFullPath($path);Header=(Join-Path $path 'include\n70https.h')}
            return
        }
    }
}
throw 'N70 HTTPS not found. Supply -Root with the source repository or extracted N70HttpsSDK path, or set N70_HTTPS_ROOT.'
