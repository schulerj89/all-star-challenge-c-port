# The Joypad Poll, `$2639`

Added **2026-08-26**. Ported from `$2639..$267D`.

## This settles the button packing

Every earlier chunk that touched input derived the layout *backwards*, from the
masks routines use — `$0C` confirming, `$33` moving, `$22` stepping back, `$CB`
toggling, `$0F` resetting. Those were consistent with each other, but they were
inference. This routine settles it from the hardware read:

- `$263B` writes `$20`, pulling P14 low, which selects the **directions**. The
  four bits come back inverted, are complemented and masked, and are then
  `swap`ped into the **high** nibble.
- `$2649` writes `$10`, selecting the **buttons**. Those four bits stay in the
  **low** nibble and are ORed in.

So the assembled byte is:

| bit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| | A | B | Select | Start | Right | Left | Up | Down |

**This is not `AllStarButtonMask`**, which puts Right at bit 0 and Start at bit
7 — very nearly the reverse. Every raw mask in the port is in the terms above.

The test does not merely restate that: it asserts each of the previously ported
masks against the constants derived here, so `$410F` really does confirm on
Select or Start, `$4119` really does move on A, B, Right or Left, `$2BC6` really
does pause on Start, and `$2D25` really is the four-button combo. If any of them
had been wrong, this is where it would have shown.

The two rows are not settled the same way. The direction row takes **two** dummy
reads and the button row takes **six**.

## Where the byte goes

`$2660` checks the role in `$C199`.

A solo game keeps the byte locally. A link game swaps the fresh byte into
`$C16E` and carries the previous one on in C — and `$C16E` is exactly what
`$2FD0` transmits for roles `$02` and `$03`.

That completes the input round trip:

```
$2639 polls -> $C16E -> $2FD0 sends -> the peer's $007B receives into $C19B
      -> $267F injects it into the other pad slot
```

`$266F` calls `$2EA8` on the way through when `$C18B` says this is a link game.
`$2EA8` itself is not ported.

Run:

```powershell
.\build\allstar_port.exe --test-pad
```
