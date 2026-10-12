# cgame cvar → consuming function map (method + results)

**Method (reusable):** [cgame_cvar_table_full.md](cgame_cvar_table_full.md) gives each cvar's `vmCvar` storage address `V` (first word of its 16-byte table row). A `vmCvar_t` is
`{handle, modificationCount, value(float), integer, string[256]}`, so code reads `.integer` at `V + 0xc` (or the struct itself at `V`). Searching `cgame.dis` for the instruction
`const 1 0x<V+0xc>` (and `0x<V>`) and attributing each hit to its enclosing `enter` finds every consumer, even though the cvar *name* is never referenced from code (see
[cgame_cvar_table.md](cgame_cvar_table.md)). A ~20-line script over the three `.dis` files does it in a few seconds.

## Results so far (function = `cgame.dis` address; all names are descriptive, none string-confirmed unless stated)
| Cvar(s) | Consumer(s) | Notes |
|---|---|---|
| `df_accel_*` (8 of 9) | `0x1584c`, `0x15a60`, `0x15d20`, `0x15e45`, `0x15ff4`, `0x164d6` | see [accel_meter.md](accel_meter.md) |
| `df_hud_cgaz`, `df_hud_cgaz_ypos` | `0x2a12`, `0x1cd86` | CGAZ strafe-angle HUD; `0x2a12` is a large 2D-draw function that also reads `df_drawBesttime` (so it is the main HUD draw); `0x1cd86` is the CGAZ draw proper (not decoded) |
| `df_drawBesttime` | `0x2a12`, `0x42de` | |
| `df_noEndOfRunStats`, `df_noTargetPrint` | `0x1b10`, `0x3d52` | end-of-run stats / target print suppression (not decoded) |
| `df_checkpoints` | `0x4f9d`, `0x50b6`, `0x5153` | checkpoint display/handling (not decoded) |
| `df_ob_AllSlopes` | `0xb21d` (342 lines) | in the cgame's Pmove copy; `0xb21d` calls `0xb6e9` |
| `df_ob_Fast` | `0xb6e9` (114 lines) | called from `0xb21d`; client-side overbounce *prediction* toggles read straight from cvars (qagame has no equivalent — its overbounce kill switch is `df_ob_KillOBs` → `pm.obs_disabled`) |
| `df_cl_alwaysDrawItems` | `0x1ea0c` | item drawing override |
| `df_player_Sprites` | `0x26191` | player sprite drawing |

Everything above is a *map*, not a decode; none of it is implemented in oDFs's cgame (whose cvar table lacks these cvars).

## Overbounce predictor (second pass: `0xb21d`, `0xb6e9`)
- `0xb21d` (called from `0x798b`, the 2D/world draw pass; guarded by a global flag `*(0xcfcec)`): loops `i = 0..16` over a 17-slot table. For each `i` it clears three per-slot arrays
  (`0x129b88` = -1, `0x129c10` = 0, `0x129bcc` = 0), asks `0xb1e5(i)` whether slot `i` is in the active set (a 28-entry bitmask table at `0x17f4`/`0x17f8`, 8 bytes per entry, entry
  validity via `0xbcf6`), and for active slots evaluates the overbounce check through `0xb72e`, storing the result in `0x129b88[i]`. Before evaluating, a per-slot float (`local 0x80` / `local 0x48`, chosen by whether `i == 0` i.e. the local player)
  must be `> 0`, and is evaluated only if it **equals `1.0f`** or **`df_ob_AllSlopes` (`0x121284`) is non-zero** — i.e. by default only exactly-flat candidates count; `AllSlopes` widens it to any slope.
  After the loop it advances two small ring counters (`0x16e4` mod 4, `0x16e0` mod 3), typical for round-robin history.
- `0xb6e9` is the `df_ob_Fast` switch (`0x121394`): `fast ? 0xb583(7 args) : 0xb66f(7 args)` — two implementations (fast vs full) of the same OB test; `0xb72e` (the wrapper
  `0xb21d` calls) invokes `0xb6e9` twice. The two implementations were not decoded.
So this is a client-side **overbounce prediction/visualisation engine** (not part of `Pmove` prediction), driven by those two cvars; it is separate from the server's `df_ob_KillOBs`/`pm.obs_disabled` logic.

## Sizes of the remaining undecoded pieces (for planning)
`0xb583` ~331 lines (constants `0.5f`, `2.0f`, `1.0f` — geometry tests), `0xb66f` ~203 lines, `0xb72e` ~492 lines (starts with the epsilon `0.001f` = `0x3a83126f`, ~14 traps/calls);
`0x1cd86` (the second `df_hud_cgaz*` consumer) is only ~208 lines of draw-pass calls and constants (`0x30`, `0x130`, `0xe4`) and globals `0x1198a0`/`0x1199ac`/`0x119ac0` — a draw wrapper, not the strafe-angle maths.
These are float-heavy and each needs a dedicated decode pass; none affects oDFs's current feature set.

## `df_cl_alwaysDrawItems` consumer `0x1ea0c` (identified)
`0x1ea0c(cent)` (called from `0x1f222`, the entity-draw switch) is **`CG_Item`** (vanilla string "Bad item index %i on entity"; `bg_itemlist` stride `0x34` at `0x3748`, item count `0x3ecc`, `modelindex` at `cent+0xa0`, `es->eFlags & 0x80` = EF_NODRAW check, tag `0x2c0` origin copy). DeFRaG's addition: when `df_cl_alwaysDrawItems` (`0x14734`, default 1) is non-zero and the item is not `IT_TEAM` (`giType == 8`), it takes an extra branch (zeroes a 0x8c-byte `refEntity_t`-sized block, sets a white `shaderRGBA` 0xff,0xff,0xff,0xff, radius 9.0, and renders from the entity's own origin) — i.e. items stay drawn for a client whose pickup is on its per-client cooldown instead of being hidden/faded. The server side of that per-client pickup visibility is already in oDFs (`perClientPickupTime`, G_ITEMS/Touch_Item.md); oDFs's cgame has no hide path, so the default `df_cl_alwaysDrawItems 1` behaviour is already what oDFs does. Not ported (nothing to add at the default).
