param(
    [string]$RomPath = ""
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$CmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if (!$CmakeCmd) {
    throw "cmake was not found on PATH. Install CMake (Visual Studio 2022 includes it) and re-run."
}

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $VsWhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools with Desktop C++ workload."
}

$VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$VsPath) {
    throw "MSVC C++ tools were not found."
}

# Keep executables at build\<name>.exe (as before) instead of per-config
# subdirectories, so existing scripts and docs keep working.
$RuntimeOutDir = $BuildDir

Write-Host "Configuring CMake build..."
& cmake -S $Root -B $BuildDir "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$RuntimeOutDir"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

Write-Host "Building executables..."
& cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

$ConsoleExePath = Join-Path $BuildDir "allstar_port.exe"
$GameExePath = Join-Path $BuildDir "allstar_port_game.exe"

Write-Host "Build complete:" -ForegroundColor Green
Write-Host "  CLI:  $ConsoleExePath"
Write-Host "  Game: $GameExePath (SDL3)"

$DefaultAssetPack = Join-Path $BuildDir "allstar.assetpack"
$LocalRomPath = Join-Path $BuildDir "nba_allstar.gb"
$AssetRomPath = $null
if ($RomPath -and (Test-Path -LiteralPath $RomPath)) {
    $AssetRomPath = (Resolve-Path -LiteralPath $RomPath).Path
} elseif ($env:ALLSTAR_ROM_PATH -and
          (Test-Path -LiteralPath $env:ALLSTAR_ROM_PATH)) {
    $AssetRomPath = (Resolve-Path -LiteralPath $env:ALLSTAR_ROM_PATH).Path
} elseif (Test-Path -LiteralPath $LocalRomPath) {
    $AssetRomPath = $LocalRomPath
}

if ($AssetRomPath) {
    Write-Host "Building gameplay asset pack from the local ROM..."
    & $ConsoleExePath --build-assetpack $AssetRomPath $DefaultAssetPack
    if ($LASTEXITCODE -ne 0) {
        throw "Asset-pack build failed with exit code $LASTEXITCODE"
    }
} elseif (Test-Path -LiteralPath $DefaultAssetPack) {
    & $ConsoleExePath --play $DefaultAssetPack | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "build\allstar.assetpack is stale or invalid. Re-run with -RomPath <game.gb>."
    }
} else {
    Write-Warning "No gameplay asset pack was built. Re-run with -RomPath <game.gb> or set ALLSTAR_ROM_PATH."
}
