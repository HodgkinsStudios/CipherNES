# Created by Jacob Hodgkins
# CipherNES native Windows build (MinGW-w64 / Code::Blocks GCC toolchain)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$CoreDir = Join-Path $Root "CipherNES_Core"
$FrontendDir = Join-Path $Root "CipherNES_Windows"
$OutDir = Join-Path $Root "build\windows"
$BinDir = Join-Path $OutDir "bin"
$ObjDir = Join-Path $OutDir "obj\core"
$Cxx = if ($env:CXX) { $env:CXX } else { "g++" }

function Invoke-Cxx {
    param([string[]]$Arguments)
    & $Cxx @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "C++ compiler failed with exit code $LASTEXITCODE"
    }
}

function Find-OnPath {
    param([string]$Name)

    $searchDirs = [Collections.Generic.List[string]]::new()

    # The compiler directory is the most reliable place to find MinGW runtime
    # DLLs (including MSYS2 SDL2/zlib), even when the calling shell has an
    # unusual PATH representation.
    $compilerCommand = Get-Command $Cxx -ErrorAction SilentlyContinue
    if ($compilerCommand -and $compilerCommand.Source) {
        [void]$searchDirs.Add((Split-Path -Parent $compilerCommand.Source))
    }

    foreach ($dir in ($env:PATH -split [IO.Path]::PathSeparator)) {
        if (-not [string]::IsNullOrWhiteSpace($dir)) {
            [void]$searchDirs.Add($dir)
        }
    }

    foreach ($known in @(
        "C:\msys64\mingw64\bin",
        "C:\msys64\ucrt64\bin",
        "C:\msys64\clang64\bin"
    )) {
        if (Test-Path $known -PathType Container) {
            [void]$searchDirs.Add($known)
        }
    }

    foreach ($dir in ($searchDirs | Select-Object -Unique)) {
        try {
            $candidate = Join-Path $dir $Name
            if (Test-Path $candidate -PathType Leaf) { return $candidate }
        } catch {
            # Ignore malformed PATH entries and continue searching.
        }
    }

    return $null
}

$CoreSources = @(
    "apu2A03.cpp", "bus.cpp", "cartridge.cpp", "cpu6502.cpp", "game_genie.cpp",
    "ciphernes_core.cpp", "ppu2C02.cpp",
    "Mappers/Mapper000.cpp", "Mappers/Mapper001.cpp", "Mappers/Mapper002.cpp",
    "Mappers/Mapper003.cpp", "Mappers/Mapper004.cpp", "Mappers/Mapper005.cpp",
    "Mappers/Mapper006.cpp", "Mappers/Mapper007.cpp", "Mappers/Mapper009.cpp",
    "Mappers/Mapper010.cpp", "Mappers/Mapper011.cpp", "Mappers/Mapper013.cpp", "Mappers/Mapper015.cpp",
    "Mappers/Mapper016.cpp", "Mappers/Mapper017.cpp", "Mappers/Mapper018.cpp",
    "Mappers/Mapper019.cpp", "Mappers/Mapper020.cpp", "Mappers/Mapper210.cpp", "Mappers/Mapper022.cpp",
    "Mappers/Mapper023.cpp", "Mappers/MapperVrc6.cpp", "Mappers/Mapper028.cpp", "Mappers/Mapper032.cpp",
    "Mappers/Mapper033.cpp", "Mappers/Mapper034.cpp", "Mappers/Mapper053.cpp", "Mappers/Mapper064.cpp",
    "Mappers/Mapper065.cpp", "Mappers/Mapper066.cpp", "Mappers/Mapper067.cpp", "Mappers/Mapper068.cpp",
    "Mappers/Mapper069.cpp", "Mappers/Mapper070.cpp", "Mappers/Mapper071.cpp", "Mappers/Mapper075.cpp",
    "Mappers/Mapper077.cpp", "Mappers/Mapper078.cpp", "Mappers/Mapper079.cpp", "Mappers/Mapper080.cpp",
    "Mappers/Mapper082.cpp", "Mappers/Mapper087.cpp", "Mappers/Mapper091.cpp", "Mappers/Mapper097.cpp",
    "Mappers/Mapper099.cpp", "Mappers/Mapper105.cpp", "Mappers/Mapper111.cpp", "Mappers/Mapper113.cpp",
    "Mappers/Mapper130.cpp", "Mappers/Mapper160.cpp", "Mappers/Mapper180.cpp", "Mappers/Mapper184.cpp",
    "Mappers/Mapper188.cpp", "Mappers/Mapper185.cpp", "Mappers/Mapper193.cpp", "Mappers/Mapper222.cpp",
    "Mappers/Mapper232.cpp", "Mappers/Mapper234.cpp", "Mappers/Mapper073.cpp", "Mappers/Mapper083.cpp",
    "Mappers/Mapper085.cpp", "Mappers/MapperCommonBoards.cpp", "Mappers/MapperAdditionalBoards.cpp",
    "Mappers/MapperJY.cpp", "Mappers/MapperKaiser202.cpp", "Mappers/MapperMmc3Variant.cpp",
    "Mappers/MapperJalecoSimple.cpp", "Mappers/MapperMulticart.cpp", "Mappers/MapperNamco108.cpp",
    "Mappers/Mapper252.cpp"
)

if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
New-Item -ItemType Directory -Force -Path $BinDir, $ObjDir | Out-Null

$CommonFlags = @(
    "-std=gnu++11", "-O3", "-DNDEBUG", "-DCIPHERNES_STATIC", "-Wall", "-Wextra",
    "-Wno-unused-parameter", "-Wno-unused-function",
    "-Wno-misleading-indentation", "-Wno-missing-field-initializers",
    "-ffunction-sections", "-fdata-sections"
)

$CoreObjects = @()
foreach ($source in $CoreSources) {
    $objectName = ($source -replace "[\\/]", "_") -replace "\.cpp$", ".o"
    $object = Join-Path $ObjDir $objectName
    $CoreObjects += $object
    Invoke-Cxx ($CommonFlags + @(
        "-I$CoreDir",
        "-c", (Join-Path $CoreDir $source),
        "-o", $object
    ))
}

# Core objects link into CipherNES.exe; never ship a second core DLL.
# Emit PE resources containing every available host DLL dependency.
# MSYS2's windres accepts absolute file paths with forward slashes.
$RuntimeNames = @("SDL2.dll", "zlib1.dll", "libwinpthread-1.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll")
$ResourceLines = @()
for ($i = 0; $i -lt $RuntimeNames.Length; $i++) {
    $found = Find-OnPath $RuntimeNames[$i]
    if (-not $found -and $i -lt 2) { throw "Missing mandatory bundled DLL: $($RuntimeNames[$i])" }
    if ($found) {
        $posix = $found.Replace('\', '/')
        $ResourceLines += "$(101 + $i) RCDATA `"$posix`""
    }
}
$ResourcesRc = Join-Path $ObjDir "ciphernes_runtime.rc"
$ResourcesObj = Join-Path $ObjDir "ciphernes_runtime.o"
$ResourceLines | Set-Content -Encoding ASCII $ResourcesRc
& windres -O coff -i $ResourcesRc -o $ResourcesObj
if ($LASTEXITCODE -ne 0) { throw "Failed to embed Windows DLL resources" }

Invoke-Cxx ($CommonFlags + @(
    "-I$CoreDir", "-I$FrontendDir",
    (Join-Path $FrontendDir "main_windows.cpp"),
    (Join-Path $FrontendDir "zip_rom_loader.cpp")
) + $CoreObjects + @(
    $ResourcesObj,
    "-Wl,--gc-sections",
    "-static", "-static-libgcc", "-static-libstdc++",
    "-lshell32", "-lcomdlg32", "-lole32",
    "-o", (Join-Path $BinDir "CipherNES.exe")
))

$HashTargets = @("CipherNES.exe")
$HashLines = foreach ($name in $HashTargets) {
    $file = Join-Path $BinDir $name
    if (Test-Path $file) {
        $hash = (Get-FileHash -Algorithm SHA256 $file).Hash.ToLowerInvariant()
        "$hash  $name"
    }
}
$HashLines | Set-Content -Encoding ASCII (Join-Path $BinDir "SHA256SUMS.txt")

Write-Host "CipherNES Windows build complete: $BinDir"
