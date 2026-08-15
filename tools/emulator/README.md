# Headless emulator parity traces

`trace_one_on_one_input.lua` drives the original cartridge from boot into a
One-on-One match, chooses distinct default players, and checks the bank-1
`$702D` human shot-input paths in Mesen 2.

The script captures a gameplay savestate and checks both release sequences:

- A then A: the first A enters action `$0A`; the second new-A bit releases
  directly from shot phase `0`.
- A then B: held B sets phase `1` and `$C16A=1`; the following player update
  clears the latch, advances to phase `2`, and releases.

Run it with a valid user-supplied ROM (the ROM is never copied into the repo):

```powershell
$mesen = 'C:\path\to\Mesen.exe'
$script = (Resolve-Path '.\tools\emulator\trace_one_on_one_input.lua').Path
$rom = 'C:\path\to\NBA All-Star Challenge (USA, Europe).gb'
$arguments = "--testRunner --enableStdout --timeout=30 `"$script`" `"$rom`""
$process = Start-Process -FilePath $mesen -ArgumentList $arguments `
    -WindowStyle Hidden -Wait -PassThru
exit $process.ExitCode
```

Exit code `0` and the final `TRACE PASSED` line are required. The assertions
cover `$FFAE` new input, `$FFAF` held input, player action/shot phase,
possession `$FFCF`, release latch `$C16A`, and nonzero ball vertical velocity.
