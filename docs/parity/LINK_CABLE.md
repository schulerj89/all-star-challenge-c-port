# The Serial Link Layer

Added **2026-08-26**. Ported from `$267F` (receive), `$2FD0` with its table at
`$2FDA` (transmit), and `$2718` (pad refresh).

Two-player play is not a peripheral feature in this cartridge; it is threaded
through six separate places. This document collects them, because each one was
found in a different chunk and none of them makes sense alone.

## How the pieces fit

| where | what it does |
|---|---|
| `$038F` | a two-player Free Throw or Accuracy game writes the mode into `$C18B` |
| `$2FD0` | sends a byte, choosing between `$D5` and `$C16E` by the role in `$C199` |
| `$007B` | the serial interrupt receives into `$C19B` and raises `$C19C` |
| `$0B44` | the game spins until `$C19C` is raised, then clears it |
| `$267F` | injects `$C19B` as the remote pad's held state |
| `$1121`, `$1209` | the postgame screens hand-shake on the `$F0` score-high flags |
| `$2BC6` | pause posts `$CC` into `$C18E` rather than pausing directly |
| `$0322` | confirming two players on the title screen assigns `$C199 = $01` |
| `$0156` | a soft reset posts `$C3` into `$C18E` and waits five vblanks |

`$C199` is this cartridge's role. `$03` means it is player 2, and `$01` is
assigned at the title screen when two players are chosen — see `BOOT.md`.

`$C18E` carries two protocol bytes: `$CC` for a pause request and `$C3` for a
reset notice.

## Receive, `$267F`

The received byte is the **other** player's input, so it is written into the
other player's pad slot:

| `$C199` | writes |
|---|---|
| `$00` | nothing — a solo game returns at `$269F` |
| `$03` | pad 1, `$FFAE`/`$FFAF` |
| anything else | pad 2, `$FFC7`/`$FFC8` |

That is what makes the "whose pad do I read" logic in `$4100`, `$1521` and
`$2BED` line up: each side reads its own pad directly and the other side's
through this injection.

The newly-pressed byte is computed as `(held ^ received) & received`, so only
bits that are set now and were clear before survive. A release can never appear
as a press.

`$C19A` going clear means the link is down, and `$2685` zeroes the received byte
before anything reads it. A link game (`$C18B`) that is mid-update — either
`$C16F` or `$FF90` set — hands off to `$2EE5` instead of injecting.

## Transmit, `$2FD0`

`$2FDA` is indexed by the same role byte:

| `$C199` | handler | sends |
|---|---|---|
| `$00` | `$2FE2` | nothing |
| `$01` | `$2FE8` | the `$D5` sync byte, **unless** `$FF0F` bit 3 says a serial interrupt is still pending |
| `$02` | `$2FF1` | the state byte in `$C16E` |
| `$03` | `$2FF6` | the same |

A dropped link (`$C19A` clear) short-circuits to sending zero. The send itself
clears `$FF02` before writing `$FF01`.

`$D5` is one of three protocol bytes the serial interrupt recognises at `$007B`,
alongside `$B3` and `$EE`.

## Scope

The port models the protocol and the input injection. It does not open a
transport — there is no socket, pipe or cable behind it — so a native two-player
session would need one supplied. The value here is that the rules are now
written down and tested rather than implicit.

Run:

```powershell
.\build\allstar_port.exe --test-link
```

## The outgoing half starts at `$2639`

`$2FD0` transmits `$C16E`, and `$C16E` is written by the joypad poll at
`$2666`: in a link game the freshly polled byte is swapped into it and the
previous byte is carried on in C. See `JOYPAD.md`. The full round trip is

```
$2639 polls -> $C16E -> $2FD0 sends -> the peer's $007B receives into $C19B
      -> $267F injects it into the other pad slot
```

`--test-pad` proves the coupling by feeding the dispatch result straight into
`allstar_link_transmit` rather than by comparing constants.
