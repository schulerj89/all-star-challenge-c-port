# Mode Routing Parity Evidence

Last verified: **2026-08-14**

## Scope

This check covers the route from the five-item mode menu to the matching native gameplay scene. It does not claim that the gameplay inside those scenes is ROM-equivalent.

## ROM evidence

Bank 0 uses HRAM `$FF8F` as the selected mode ID:

- `$03A1` initializes `$FF8F` to `0` and draws the menu at `$2ABA`.
- `$03B9` uses `$FF8F` as the cursor-table index.
- `$03F3..$0414` increments/decrements the value and wraps it across `0..4`.
- `$22EF` reads the same `$FF8F` value to select the mode-specific settings path and display table at `$256E`.

The ROM-derived menu image displays the choices in this order:

| `$FF8F` | ROM menu label | Native mode | Final native scene | Opponent selection |
|---:|---|---|---|:---:|
| 0 | One On One | `ALLSTAR_MODE_ONE_ON_ONE` | `ALLSTAR_SCENE_ONE_ON_ONE` | Yes |
| 1 | Free Throws | `ALLSTAR_MODE_FREE_THROW` | `ALLSTAR_SCENE_FREE_THROW` | No |
| 2 | Horse | `ALLSTAR_MODE_HORSE` | `ALLSTAR_SCENE_HORSE` | No |
| 3 | Accuracy Shootout | `ALLSTAR_MODE_ACCURACY` | `ALLSTAR_SCENE_THREE_POINT` | Yes |
| 4 | Tournament | `ALLSTAR_MODE_TOURNAMENT` | `ALLSTAR_SCENE_TOURNAMENT` | Yes |

`ALLSTAR_SCENE_THREE_POINT` retains its old internal name, but it is the native scene currently assigned to the ROM's Accuracy Shootout menu entry. Its rules remain unverified and are tracked separately.

## Former defect

The roster dispatcher previously routed IDs 1, 2, and 3 to Three Point, Free Throw, and Horse respectively. That contradicted the menu order, so three of five selections entered the wrong native mode.

## Automated check

Run:

```powershell
.\build\allstar_port.exe --test-mode-routing
```

The test checks all five menu IDs, names, opponent-selection paths, final scene IDs, scene construction, and the invalid-ID fallback. The routing table is shared by the menu and roster flow, preventing the two stages from drifting independently.
