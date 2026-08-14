param(
    [string]$Emulator = "auto"
)

$RomPath = "F:\Games\GBA\GB\NBA All-Star Challenge\NBA All-Star Challenge (USA, Europe).gb"
$MesenPath = "C:\Users\joshs\AppData\Local\Microsoft\WinGet\Packages\SourMesen.Mesen2_Microsoft.Winget.Source_8wekyb3d8bbwe\Mesen.exe"
$mGBAPath = "C:\Program Files\mGBA\mGBA.exe"

if (-not (Test-Path $RomPath)) {
    Write-Error "ROM not found at: $RomPath"
    exit 1
}

if ($Emulator -eq "mgba" -or ($Emulator -eq "auto" -and (Test-Path $mGBAPath))) {
    Write-Host "Launching mGBA with original Game Boy ROM..." -ForegroundColor Cyan
    Start-Process -FilePath $mGBAPath -ArgumentList "`"$RomPath`""
} elseif (Test-Path $MesenPath) {
    Write-Host "Launching Mesen2 with original Game Boy ROM..." -ForegroundColor Cyan
    Start-Process -FilePath $MesenPath -ArgumentList "`"$RomPath`""
} else {
    Write-Error "No supported Game Boy emulator found."
}
