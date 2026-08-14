param(
    [string]$ShortcutName = "NBA All-Star Challenge Native Port.lnk",
    [string]$ShortcutPath
)

$ErrorActionPreference = "Stop"

$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$ExePath = Join-Path $ProjectRoot "build\allstar_port_game.exe"
$IconPath = Join-Path $ProjectRoot "assets\nba_allstar_challenge.ico"

if (!(Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Game executable was not found: $ExePath. Run .\build.ps1 first."
}

if (!(Test-Path -LiteralPath $IconPath -PathType Leaf)) {
    throw "Icon was not found: $IconPath"
}

if (!$ShortcutPath) {
    $Desktop = [Environment]::GetFolderPath([Environment+SpecialFolder]::Desktop)
    $ShortcutPath = Join-Path $Desktop $ShortcutName
}

$ShortcutPath = [System.IO.Path]::GetFullPath($ShortcutPath)
$ShortcutDirectory = Split-Path -Parent $ShortcutPath
if (!(Test-Path -LiteralPath $ShortcutDirectory -PathType Container)) {
    throw "Shortcut directory was not found: $ShortcutDirectory"
}

$Shell = New-Object -ComObject WScript.Shell
$Shortcut = $Shell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = $ExePath
$Shortcut.WorkingDirectory = $ProjectRoot
$Shortcut.IconLocation = "$IconPath,0"
$Shortcut.Description = "Launch the native C port of NBA All-Star Challenge (Game Boy)."
$Shortcut.Save()

[void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($Shortcut)
[void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($Shell)

Write-Host "Successfully created Desktop shortcut:" -ForegroundColor Green
Write-Host "  Shortcut:  $ShortcutPath"
Write-Host "  Target:    $ExePath"
Write-Host "  Icon:      $IconPath"
