$ErrorActionPreference = "Stop"

$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$GhidraHome = "C:\Users\joshs\Downloads\ghidra_11.3_PUBLIC_20250205\ghidra_11.3_PUBLIC"
$JdkHome = "C:\Users\joshs\Downloads\jdk-21\jdk-21.0.6+7"
$RomPath = "F:\Games\GBA\GB\NBA All-Star Challenge\NBA All-Star Challenge (USA, Europe).gb"
$GhidraProjectsDir = Join-Path $ProjectRoot "build\ghidra_project"
$GhidraScriptDir = Join-Path $ProjectRoot "tools\ghidra"
$BankInventoryPath = Join-Path $ProjectRoot "build\ghidra_bank_inventory.json"
$BankCheckScript = Join-Path $ProjectRoot "tools\check_ghidra_banks.py"

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
    -scriptPath "$GhidraScriptDir" `
    -postScript "setup_banked_rom.py" "$BankInventoryPath" `
    -postScript "decompile_all.py" `
    -overwrite

if ($LASTEXITCODE -ne 0) {
    throw "Ghidra headless analysis failed with exit code $LASTEXITCODE"
}

python "$BankCheckScript" "$BankInventoryPath"
if ($LASTEXITCODE -ne 0) {
    throw "Ghidra bank inventory verification failed with exit code $LASTEXITCODE"
}

Write-Host "Ghidra four-bank setup and preliminary decompilation finished." -ForegroundColor Green
