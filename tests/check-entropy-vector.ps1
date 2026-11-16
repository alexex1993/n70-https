param([string]$Program=(Join-Path $PSScriptRoot '..\build\host\entropy_vector.exe'))
$ErrorActionPreference='Stop'
# Independent SP800-90A HMAC_DRBG reference using .NET HMACSHA256.
function Mac([byte[]]$key,[byte[]]$data) {
    $h=New-Object Security.Cryptography.HMACSHA256
    try { $h.Key=$key; return ,$h.ComputeHash($data) } finally { $h.Dispose() }
}
$script:k=New-Object byte[] 32
$script:v=[byte[]](1..32 | ForEach-Object { 1 })
function Update([byte[]]$inputData) {
    $script:k=Mac $script:k ([byte[]]($script:v+[byte]0+$inputData))
    $script:v=Mac $script:k $script:v
    if ($inputData.Length) {
        $script:k=Mac $script:k ([byte[]]($script:v+[byte]1+$inputData))
        $script:v=Mac $script:k $script:v
    }
}
Update ([byte[]](0..31))
Update ([Text.Encoding]::ASCII.GetBytes('N70 HTTPS persistent seed v1'))
Update ([Text.Encoding]::ASCII.GetBytes('fixed test context'))
$expected=@()
for($i=0;$i -lt 2;$i++) {
    $script:v=Mac $script:k $script:v
    $expected+=$script:v
}
$expectedHex=($expected | ForEach-Object { '{0:x2}' -f $_ }) -join ''
$actual=(& $Program).Trim()
if ($LASTEXITCODE -ne 0 -or $actual -ne $expectedHex) { throw 'Entropy derivation differs from independent HMAC_DRBG reference' }
Write-Host 'PASS: seed derivation agrees with independent .NET HMAC_DRBG reference.'
