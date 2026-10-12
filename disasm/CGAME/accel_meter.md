# DeFRaG cgame acceleration meter (`df_accel_*`)

Structural map of the cgame code behind the nine `df_accel_*` cvars ([cgame_cvar_table_full.md](cgame_cvar_table_full.md)): `Draw`, `FontSize`, `Mode`, `Output`, `OffsetX`, `OffsetY`,
`Scale`, `AdjustMaxSpeed`, `HistoryResetSpeed`. Consumers were found by resolving each cvar's storage address from the table (`vmCvar` base + `0xc` = `.integer`) and
searching `cgame.dis` for those addresses — only these six functions read them:

| Function | Reads | Role (from calls, constants and strings; bodies not fully decoded) |
|---|---|---|
| `0x15ff4` (called from `0x79ac`, the 2D draw pass) | `Draw`, `Mode` | **Entry point** (`CG_DrawAccel`-shaped): if `Draw` (via helper `0x8f3a`) is off, return; `0x15f83()` is an availability gate (its body calls two traps and reads a global at `0x12a2b4`; returns 0 to skip drawing); then `Mode == 0` → `0x15e45`, `Mode != 0` → `0x15d20`. |
| `0x15e45` (467 lines) | `OffsetX`, `OffsetY` | **Mode 0 draw**: positions relative to the screen (globals `0x114050`, `0x125f04`, `0x125df4`), calls the meter computation `0x15a60`, then several draw calls. |
| `0x15d20` (476 lines) | `FontSize`, `Mode`, `OffsetX`, `OffsetY` | **Other modes draw**: text glyphs `"O"` and `"+"` with the constants `320.0f`/`240.0f` (`0x43a00000`/`0x43700000`, the screen centre), i.e. a centred marker/graph; also calls `0x15a60`. |
| `0x15a60` (625 lines) | `AdjustMaxSpeed` | **Per-frame computation**: float-heavy (`0x43340000` = 180.0, `0x40490fdb` = π → angle conversions, `sqrt`/`atan2`-style traps), writes results into a block at `0x125bd4`, calls `0x1584c`. |
| `0x1584c` (448 lines) | `AdjustMaxSpeed`, `Output` | **Percentage + optional log output**: `percent = value * 100.0f` (`0x42c80000`) clamps; when `Output` is set, prints the debug lines `"SPEED:   ^%istart ^7= ^%i%8.3f ^%ispeed ^7= ^%i%8.3f ^%imax ^7= ^%i%8.3f\n"` and `"PERCENT: ^%iaccel ^7= ^%i%8.3f ^%ispeed ^7= ^%i%8.3f\n"` (start/current/max speed and acceleration vs. speed, each with a colour index). |
| `0x164d6` (called from `0x15c24`) | `HistoryResetSpeed` | History reset: when speed falls below the cvar the stored history (`0x125ac4`) is cleared. |

What the meter measures (from the printed names): **start speed, current speed, max speed** and the **percentage of the maximum possible acceleration** achieved over a frame.
`Scale` (cvar) is read by no function in the list above, so its consumer is unlocated (may be reached through a different access path).

## oDFs
Nothing ported — oDFs's cgame has none of these cvars or functions (see the cgame cvar table doc). The numeric details (exact formula for the accel percentage, how `Mode` selects
between the two draw paths, colour thresholds) need a full decode of `0x15a60`/`0x1584c`.
