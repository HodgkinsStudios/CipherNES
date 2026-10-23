# Created by Jacob Hodgkins
# Package the native CipherNES Windows x86_64 release.

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Version = (Get-Content (Join-Path $Root "VERSION") -Raw).Trim()
$Dist = Join-Path $Root "dist"
$Stage = Join-Path $Dist ("CipherNES-" + $Version + "-Windows-x86_64")

& (Join-Path $Root "build_windows.ps1")
if ($LASTEXITCODE -ne 0) { throw "Windows build failed." }

if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Force -Path $Stage, (Join-Path $Stage "docs") | Out-Null

$Bin = Join-Path $Root "build\windows\bin"
$Required = @("CipherNES.exe")
foreach ($name in $Required) {
    $source = Join-Path $Bin $name
    if (-not (Test-Path $source)) { throw "Required release file missing: $name" }
    Copy-Item $source $Stage
}

foreach ($name in @("LICENSE", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md", "RELEASE_NOTES_1.0.0.md", "VERSION")) {
    Copy-Item (Join-Path $Root $name) $Stage
}
Copy-Item (Join-Path $Root "docs\VALIDATION.md") (Join-Path $Stage "docs")
Copy-Item (Join-Path $Root "docs\WINDOWS.md") (Join-Path $Stage "docs")

@"
# CipherNES $Version — Windows x86_64

Created by Jacob Hodgkins.

This is the native Windows release of CipherNES $Version, targeting NTSC timing.

## Run

Launch CipherNES.exe directly or pass a ROM path:

    CipherNES.exe C:\path\to\game.nes

SDL2, zlib and the emulator core are embedded in the single executable. Its first
launch creates %APPDATA%\HodgkinsStudios\CipherNES and the user-data folders.

## Famicom Disk System

Supply your own BIOS in %APPDATA%\HodgkinsStudios\CipherNES\bios\disksys.rom.

## Accuracy

See docs\VALIDATION.md for the release certification record.

## License

See LICENSE and THIRD_PARTY_NOTICES.md.
"@ | Set-Content -Encoding UTF8 (Join-Path $Stage "README.md")

$HashNames = @("CipherNES.exe")

$HashLines = foreach ($name in $HashNames) {
    $hash = (Get-FileHash -Algorithm SHA256 (Join-Path $Stage $name)).Hash.ToLowerInvariant()
    "$hash  $name"
}
$HashLines | Set-Content -Encoding ASCII (Join-Path $Stage "SHA256SUMS.txt")

$Zip = Join-Path $Dist ("CipherNES-" + $Version + "-Windows-x86_64.zip")
Compress-Archive -Path $Stage -DestinationPath $Zip -CompressionLevel Optimal -Force
Write-Host "Windows release archive created: $Zip"
