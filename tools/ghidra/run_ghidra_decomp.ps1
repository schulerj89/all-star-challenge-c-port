$ErrorActionPreference = "Stop"

$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$GhidraHome = "C:\Users\joshs\Downloads\ghidra_11.3_PUBLIC_20250205\ghidra_11.3_PUBLIC"
$JdkHome = "C:\Users\joshs\Downloads\jdk-21\jdk-21.0.6+7"
$RomPath = "F:\Games\GBA\GB\NBA All-Star Challenge\NBA All-Star Challenge (USA, Europe).gb"
$GhidraProjectsDir = Join-Path $ProjectRoot "build\ghidra_project"
$DecompScript = Join-Path $ProjectRoot "tools\ghidra\decompile_all.py"

New-Item -ItemType Directory -Force -Path $GhidraProjectsDir | Out-Null

$env:JAVA_HOME = $JdkHome
$env:PATH = "$JdkHome\bin;$env:PATH"

$AnalyzeHeadless = Join-Path $GhidraHome "support\analyzeHeadless.bat"

$CleanRomPath = Join-Path $ProjectRoot "build\nba_allstar.gb"
Copy-Item $RomPath -Destination $CleanRomPath -Force

Write-Host "Running Ghidra Headless Decompiler on NBA All-Star Challenge ($CleanRomPath)..."
& $AnalyzeHeadless "$GhidraProjectsDir" "NBA_AllStar_GB" `
    -import "$CleanRomPath" `
    -processor "SM83:LE:16:default" `
    -postScript "$DecompScript" `
    -overwrite

Write-Host "Ghidra Headless Decompilation Run Finished!" -ForegroundColor Green
