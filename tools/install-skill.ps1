param([string]$SkillsDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent)).TrimEnd('\')
if (!$SkillsDirectory) {
    $codexDirectory=if ($env:CODEX_HOME) { $env:CODEX_HOME } else { Join-Path $env:USERPROFILE '.codex' }
    $SkillsDirectory=Join-Path $codexDirectory 'skills'
}
$skillsBase=[IO.Path]::GetFullPath($SkillsDirectory).TrimEnd('\')
$target=[IO.Path]::GetFullPath((Join-Path $skillsBase 'n70-https'))
if (!$target.StartsWith($skillsBase+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid skill destination' }
if (Test-Path -LiteralPath $target) {
    $backup=[IO.Path]::GetFullPath((Join-Path $root ('Trash\previous-skill-'+[Guid]::NewGuid().ToString('N'))))
    if (!$backup.StartsWith($root+'\Trash\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid backup destination' }
    New-Item -ItemType Directory -Force (Split-Path $backup -Parent) | Out-Null
    Move-Item -LiteralPath $target -Destination $backup
    Write-Host "Previous skill preserved: $backup"
}
New-Item -ItemType Directory -Force $skillsBase | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'skills\n70-https') -Destination $target -Recurse
# Machine-specific locator is installed locally, never added to the portable SDK.
$hint=@{root=$root} | ConvertTo-Json
[IO.File]::WriteAllText((Join-Path $target 'local-library.json'),$hint,[Text.UTF8Encoding]::new($false))
Write-Host "Installed n70-https: $target"
