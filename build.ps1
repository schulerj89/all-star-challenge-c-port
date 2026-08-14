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

$Sources = @(
    "src\main.c",
    "src\allstar_cli.c",
    "src\allstar_rom.c",
    "src\allstar_asset_pack.c",
    "src\allstar_renderer.c",
    "src\allstar_controls.c",
    "src\allstar_roster.c",
    "src\allstar_game.c",
    "src\gameplay\allstar_physics.c",
    "src\gameplay\allstar_ai.c",
    "src\audio\allstar_audio.c",
    "src\scenes\scene_intro.c",
    "src\scenes\scene_menu.c",
    "src\scenes\scene_roster_select.c",
    "src\scenes\scene_one_on_one.c",
    "src\scenes\scene_three_point.c",
    "src\scenes\scene_free_throw.c",
    "src\scenes\scene_horse.c"
)

$IncludePath = Join-Path $Root "include"

$ObjFiles = @()
foreach ($src in $Sources) {
    $srcPath = Join-Path $Root $src
    $objName = ([System.IO.Path]::GetFileNameWithoutExtension($src) + "_" + [System.IO.Path]::GetRandomFileName().Substring(0,4) + ".obj")
    $objPath = Join-Path $ObjDir $objName
    $ObjFiles += $objPath
}

$CompileScript = Join-Path $BuildDir "compile.bat"
$CompileBatchContent = @"
@echo off
call "$VcVars" > nul
cl.exe /nologo /W3 /O2 /MD /I "$IncludePath" /Fe"$ConsoleExePath" /Fo"$ObjDir\\" $(($Sources | ForEach-Object { "`"$Root\$_`"" }) -join ' ') user32.lib gdi32.lib
"@

Set-Content -Path $CompileScript -Value $CompileBatchContent -Encoding ASCII
Write-Host "Compiling allstar_port.exe..."
& cmd.exe /c $CompileScript

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

Write-Host "Build complete: $ConsoleExePath" -ForegroundColor Green
