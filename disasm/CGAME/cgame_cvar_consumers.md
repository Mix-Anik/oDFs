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

## `df_checkpoints` consumer `0x4f9d` — DECODED (checkpoint summary value)
Storage check: cvar-table entry at data `0x690` has vmCvar `0x11c644`, so `.integer` = `0x11c650` (the global tested below). Arrays: `A = 0x11d5b0` (reference/best checkpoint times, int per checkpoint — the "start-state" buffer in README), `B = 0x11d6b8` (current-run checkpoint times; `B[-1]` at `0x11d6b4` is the slot before it), `N = *0x11d738` (checkpoint count). `0x4f9d()` (no args, called from `0x53ac`, i.e. the HUD code) returns an int:
```c
int c = 0;
for ( i = 0; i <= N; i++ ) {                              // inclusive
    if ( df_checkpoints == 2 && A[0] != 0 ) {             // compare-to-reference mode
        d = A[i] - B[i];                                   // signed difference
        if ( abs(c) < abs(d) ) c = d;                      // keep the largest-magnitude difference
    } else if ( df_checkpoints == 3 ) {
        return B[N];                                       // final (last) checkpoint time
    } else if ( i > 0 ) {
        if ( c < B[i] - B[i-1] ) c = B[i] - B[i-1];        // longest split
    } else {
        c = B[0];                                          // first checkpoint time
    }
}
return c;
```
(The `==3 return B[N]` branch sits in the loop's first iteration path by the control flow at `0x5018`, so it returns immediately; for modes other than 2/3 — and mode 2 without a reference — the result is the longest split time.) Interpretation of `df_checkpoints` values (2 = vs-reference worst delta, 3 = total, else longest split) is from this arithmetic only; the on-screen label strings were not traced. Siblings `0x50b6` (called from `0x5248` timer-state update, 2 args) and `0x5153` (3 args, called from `0x2a12` and `0x1c0b2`) are the writers/drawers of the same arrays — still undecoded. No oDFs impact: players run the original DeFRaG cgame.

### `0x50b6(i)` — checkpoint popup (called from the timer-state update `0x5248`, once per new checkpoint index `i`) — DECODED
Returns nothing useful; its job is `CG_CenterPrint( text, 200, 10 )` (`0x1c707`, the same small-print routine the `cpsm` server command uses, see `G_TRIGGER/target_smallprint.md`) with `text` chosen by `df_checkpoints` (`0x11c650`):
- guard: `*0x11bee0 != 0` (checkpoint display enabled/available) else return;
- mode `2` **and** a reference exists (`0x11d5a4 != 0`, `A[0] != 0`): text = `0x10c8( A[i] - B[i], 0, 0, 1, 1, 0 )` — signed-delta formatter (`0x10c8` embeds the literals `"+"` and `"^2"`, i.e. a `+`/colour-coded `-1.234`-style delta);
- mode `3`: text = `0x1024( B[i] )` — plain time formatter (`0x1024`: ~23 callers, the general ms→string function) of the absolute checkpoint time;
- otherwise: if `N != 0` (`*0x11d738`): text = `0x1024( B[i] - B[i-1] )` (split; `B[-1]` = `0x11d6b4`), else `0x1024( B[i] )`.
Net: **`df_checkpoints` 2 = show delta vs your reference run, 3 = show total time, other = show split time**, consistent with the summary function `0x4f9d` above (largest delta / total / longest split). Remaining undecoded sibling: `0x5153` (3 args: also uses `0x10c8` + `0x1024`; probably the HUD-line variant of the same text). Both functions are cgame-only: no oDFs change.

### `0x5153(x, y, …)` — checkpoint list on the HUD — DECODED (structure)
Called from the HUD draw `0x2a12` (twice) and `0x1c0b2`. Returns immediately when `*0x11bff0 == 0` (draw-enable cvar, name not traced) or `B[0] == 0` (no checkpoint recorded). Otherwise: `first = (N > 7) ? N - 7 : 0` (`N = *0x11d738`), then `for ( i = first; i <= N; i++ )`:
1. text = the same three-way string as the popup `0x50b6` (`df_checkpoints` 2 → `0x10c8(A[i]-B[i], …)` delta, 3 → `0x1024(B[i])`, else split `B[i]-B[i-1]` / `B[i]`);
2. `w = 0x3265e(text)` (a string-measure call — `strlen`-like by its single `text` argument and its use below);
3. draw with `0x1d330( (int)(x - 16.0f * w), (int)y, text, 0x40f4, 0, 1, 16, …)` — right-aligned by `16 px × length`, char size 16 (constants `0x41800000` = 16.0f, `0x10` = 16);
4. `y -= 16.0f` (the list grows upward from the given y).
So the HUD shows **the last 8 checkpoints (a sliding window), newest at the bottom, each as delta / total / split per `df_checkpoints`**. The exact draw-flag meaning of `0x40f4` and the third argument were not decoded. cgame-only; no oDFs impact.

### Iter 8 (full-port loop): checkpoint popup + list PORTED to oDFs cgame
`code/cgame/cg_defrag.c` (new, added to CMakeLists): `CG_DF_SnapshotTransition` (called from `CG_TransitionSnapshot`; stats[12] bit 0x4 set -> reset, bit 0x10 toggle -> record `ps.stats[7]<<16|stats[8]` and popup via `CG_CenterPrint(text,200,10)`), `CG_DF_DrawCheckpointList` (called from `CG_Draw2D`), cvars `df_checkpoints` (2), `df_drawCheckpointEvents` (1) and `df_drawCheckpointStats` (1) — the two guards `0x11bee0`/`0x11bff0` are those cvars (resolved from the cvar table at data 0x6f0/0x700). Time format (`0x1024`) matches the qagame `S:mmm`/`M:SS:mmm` formatter (threshold 0xea60 verified in bytecode). Limits: only servers with `defrag_gametype >= 5` (plain stats[7]/[8]) — the encoded form (`0x5fd7`) is not decoded cgame-side; no reference run (mode 2 behaves like DeFRaG with `A[0]==0`: split); list x/y is our choice (caller args undecoded); the doc's 'newest at the bottom' contradicts its own loop, implemented as the loop reads (oldest at y, moving up). Syntax-checked, untested; DeFRaG cgame already runs these for players using the original cgame.qvm. Revert: remove cg_defrag.c + its 4 hook lines.

## Iter 106: cgame init cvar sequence `0x3985` (df_name, df_cl_respawnMsec, defrag_demoPlayback, defrag_version_*)
`0x3985` (called from CG_Init at `0xe5c`, after glconfig/gamestate): sets `defrag_demoPlayback` to `cg.demoPlayback` ("1"/"0"; the only writer in cgame; read by ui `0x1a92`), copies `name` into `df_name` when empty, publishes `defrag_version_server` (parsed defrag_vers as `%i.%02i.%02i`) and `defrag_version_client` (`1.91.31`), and registers sprites/notAvailable, gfx/unavailableItem, df_gfx/scoreboard/{award_speed,cam,eye}.tga. `df_name` and `df_cl_respawnMsec` (default "250") are storage-less USERINFO|ARCHIVE cvars (table rows with storage 0 / `0x11b104`); both are consumed by qagame `ClientUserinfoChanged` (already in oDFs `g_client.c`: `demoName`, `respawnMsec` clamp 50..1700). Ported: `CG_DF_InitCvars` (cg_defrag.c, called after CG_ParseServerinfo) + the two registrations in `CG_RegisterCvars`. Revert: remove those calls. BEHAVIOUR: the client now sends df_name / df_cl_respawnMsec in userinfo, so the server's demoName / respawn delay start working (default 250 ms = unchanged).

## Iter 114: `cg_muzzleFlash` (`0x2b523` = CG_AddPlayerWeapon)
Value `1131544`, default 0, ARCHIVE. At the start of the "add the flash" block (`0x2b6ac`): `if (cg_muzzleFlash != 0 && weaponNum == 8 /*WP_PLASMAGUN*/) skip the flash` — i.e. a non-zero value HIDES the plasma gun's muzzle flash (the name suggests the opposite; read literally from the bytecode). Ported in cg_weapons.c. Revert: remove the 3-line check + cvar row.

## Iter 115: `cg_drawDamage` (`0x29a4a` = CG_DamageBlendBlob)
Value `1218932`, default 1, ARCHIVE. First test of the function: `if (!cg_drawDamage) return;` — it REPLACES vanilla's `cg_blood` test here (`1008120 == 3` is the RagePro hardware check, `976440` = cg.damageValue, `976428` = cg.damageTime). Ported in cg_view.c (cg_blood still gates the blood effects elsewhere). BEHAVIOUR: `cg_blood 0` no longer hides the damage blob; `cg_drawDamage 0` does. Revert: restore `cg_blood.integer` in CG_DamageBlendBlob.

## Iter 116: `cg_drawBBox` (`0x1cecd`, called at the end of CG_Player `0x268f2` (at `0x26a4c`) and of the second player drawer `0x265a9`)
Value `71796`, default 0. Draws six translucent faces (`trap_R_AddPolyToScene`, 4 verts each, st (0,0) (0,1) (1,1) (1,0)) around the player's lerpOrigin: top and bottom with shader `bbox`, the four sides with `bbox_nocull` (both from the DeFRaG pk3). Box: default mins (-15,-15,-24), maxs (15,15,32); other clients use `currentState.solid` (x half width = byte0, y = byte1, max z = byte2 - 32), the local client maxs z = predicted viewheight + 6. Vertex colour by `clientinfo.team`: red (160,0,0,255), blue (0,0,192,255), else (0,128,0,255). Skipped for EF_DEAD; the local client's own box is drawn only when `cg_nopredict` or health <= 0 (`958772` = that flag, set in CG_DrawActiveFrame `0x29d5a`). Ported as `CG_DF_DrawBBox` (cg_players.c). Revert: remove the call at the end of CG_Player. Needs the DeFRaG pk3 shaders (nothing is drawn if they fail to register).

## Iter 117 — cg_crosshairColor (`0x8fe1`)
`0x8fe1(vec4_t out)`: out = palette[(cg_crosshairColor & 15) % 10], alpha 1. Palette (data 16708, 16 B each): black, red, green, yellow, blue, cyan, magenta, white, orange (1,.5,0), grey (.5,.5,.5). Called from the DeFRaG crosshair (`0x2029`) when cg_crosshairHealth is 0. PORTED in cg_draw.c CG_DrawCrosshair.

## Iter 118 — cg_lagometer_x/y (`0x1c4eb`)
Storage 0x11473c = cg_lagometer_x, 0x11462c = cg_lagometer_y (value at +12); other reads: 0x13fb8 cg_lagometer, 0x15938 cg_nopredict, 0x13d98 g_synchronousClients (draws "snc"). Body is vanilla CG_DrawLagometer with x,y = (float)integer of the two cvars (both default 1). PORTED in cg_draw.c.

## Iter 119 — s_ambient (storage 0x11451c)
`0x1e8a9` (entity effects): loopSound with eType==7 (ET_SPEAKER) uses AddRealLoopingSound only if s_ambient != 0 (other types unconditional). `0x1f222` (CG_AddCEntity switch) calls CG_Speaker (`0x1e9a9`) only if s_ambient != 0. PORTED in cg_ents.c. Third listed consumer `0x1f7e0` not re-checked.

## Iter 120 — df_cl_alwaysDrawItems CORRECTION (`0x1ea0c`)
The earlier paragraph ("nothing to add at the default") was wrong. Full decode: availability = `0x4e14(state, snap.ps.clientNum)` (uses nextState when nextState.eType==ET_ITEM). Unavailable item: hidden if gate `0x11d2ac`==0 (non-multiplayer) or cvar==0; simple-items branch (`0x14734` is cvar storage 1159712; simple = cg_simpleItems 83764): also hidden for 2, else icon sprite + extra sprite with shader `0x11d268` (`sprites/notAvailable`, RegisterShaderNoMip); 3D branch: hidden for 3, else customShader `0x11d26c` (`gfx/unavailableItem`). PORTED in cg_ents.c.

## Iter 121 — ch_recordMessage (`0x19e` = CG_UpdateCvars)
Storage 0x11538 (modCount at +4 = 70972, integer 70980). Tail of CG_UpdateCvars: `if (cachedCount(data 0x8, init -1) != ch_recordMessage.modificationCount) { cached = it; trap_SendConsoleCommand(va("set ui_recordSPDemo %i", integer ? 0 : 1)); }` (string 0x5b7b, no newline in original). Preceding blocks are vanilla (teamoverlay `0x10504` cache, force model `0x4` cache -> `0x173` CG_ForceModelChange). PORTED in cg_main.c.
