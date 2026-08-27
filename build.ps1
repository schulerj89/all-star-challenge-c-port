param(
    [string]$RomPath = ""
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build"
$ObjDir = Join-Path $BuildDir "obj"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $VsWhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools with Desktop C++ workload."
}

$VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$VsPath) {
    throw "MSVC C++ tools were not found."
}

$VcVars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $VcVars)) {
    throw "vcvars64.bat was not found under $VsPath."
}

$ConsoleExePath = Join-Path $BuildDir "allstar_port.exe"
$GameExePath = Join-Path $BuildDir "allstar_port_game.exe"

$CommonSources = @(
    "src\allstar_cli.c",
    "src\allstar_rom.c",
    "src\allstar_asset_pack.c",
    "src\allstar_font.c",
    "src\allstar_renderer.c",
    "src\allstar_controls.c",
    "src\allstar_roster.c",
    "src\allstar_game.c",
    "src\gameplay\allstar_physics.c",
    "src\gameplay\allstar_rng.c",
    "src\gameplay\allstar_ai.c",
    "src\gameplay\allstar_one_on_one.c",
    "src\gameplay\allstar_free_throw.c",
    "src\gameplay\allstar_horse.c",
    "src\gameplay\allstar_accuracy.c",
    "src\gameplay\allstar_tournament.c",
    "src\gameplay\allstar_postgame.c",
    "src\gameplay\allstar_select.c",
    "src\gameplay\allstar_shot_result.c",
    "src\gameplay\allstar_court_state.c",
    "src\gameplay\allstar_game_clock.c",
    "src\gameplay\allstar_status_panel.c",
    "src\gameplay\allstar_menu.c",
    "src\gameplay\allstar_settings_screen.c",
    "src\gameplay\allstar_system.c",
    "src\gameplay\allstar_link.c",
    "src\gameplay\allstar_cpu_target.c",
    "src\gameplay\allstar_boot.c",
    "src\gameplay\allstar_handshake.c",
    "src\gameplay\allstar_session.c",
    "src\gameplay\allstar_pad.c",
    "src\audio\allstar_voice_state.c",
    "src\audio\allstar_apu_program.c",
    "src\audio\allstar_audio.c",
    "src\scenes\scene_intro.c",
    "src\scenes\scene_menu.c",
    "src\scenes\scene_settings.c",
    "src\scenes\scene_roster_select.c",
    "src\scenes\scene_one_on_one.c",
    "src\scenes\scene_three_point.c",
    "src\scenes\scene_free_throw.c",
    "src\scenes\scene_horse.c",
    "src\scenes\scene_tournament.c"
)

$IncludePath = Join-Path $Root "include"

$CompileScript = Join-Path $BuildDir "compile.bat"
$CompileBatchContent = @"
@echo off
call "$VcVars" > nul
echo Compiling allstar_port.exe (CLI/Test runner)...
cl.exe /nologo /W3 /O2 /MD /I "$IncludePath" /Fe"$ConsoleExePath" /Fo"$ObjDir\\" "$Root\src\main.c" $(($CommonSources | ForEach-Object { "`"$Root\$_`"" }) -join ' ') user32.lib gdi32.lib winmm.lib
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo Compiling allstar_port_game.exe (Win32 GUI Game)...
cl.exe /nologo /W3 /O2 /MD /I "$IncludePath" /Fe"$GameExePath" /Fo"$ObjDir\\" "$Root\src\win32_game_main.c" $(($CommonSources | ForEach-Object { "`"$Root\$_`"" }) -join ' ') user32.lib gdi32.lib winmm.lib /link /SUBSYSTEM:WINDOWS
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
"@

Set-Content -Path $CompileScript -Value $CompileBatchContent -Encoding ASCII
Write-Host "Building executables..."
& cmd.exe /c $CompileScript

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

Write-Host "Build complete:" -ForegroundColor Green
Write-Host "  CLI:  $ConsoleExePath"
Write-Host "  Game: $GameExePath"

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
