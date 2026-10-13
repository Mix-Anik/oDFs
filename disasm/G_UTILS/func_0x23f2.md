# `func_0x23f2` — DeFRaG per-frame rules/housekeeping (called from the `G_RunFrame` region, `0x118c`)

No args. Decoded structurally from bytecode `0x23f2`-`0x2570` (strings are pool literals; globals as named in other docs). `0x1317c0` = dedicated/variant flag
([func_0x21fc.md](func_0x21fc.md)).

## 1. Speed award log (flag != 0 only)
`if ( flag && 0x1318cc != 0 && level.time >= 0x1318cc )`: pick the max-speed holder (`0x131848`, `-1` if none, record string `0x13184c`/netname `client+0x200`) →
`G_LogPrintf( "ClientSpeedAward: %i %i \"%s\"\n", clientNum, *(int*)0x131844 /*speed*/, name )` (`0xac4e` formats, `0xa1a` logs — `G_LogPrintf` confirmed earlier);
`0x1318cc = 0`. This ties together the `0x131848`/`0x13184c` max-speed record invalidated in `ClientDisconnect`
([../G_CLIENT/ClientDisconnect.md](../G_CLIENT/ClientDisconnect.md)).

## 2. Non-dedicated path (`flag == 0`): rule enforcement every frame
- If `sv_cheats == 0` (`func_0x3e0a`): read `sv_fps`; `> 140` → set it to `140` and broadcast `"print \"^1sv_fps highest value restricted to %i.^7\n\""`;
  `< 60` → set `60` and broadcast `"print \"^1sv_fps lowest value restricted to %i.^7\n\""`. (The init-time clamp in `func_0x21fc` uses `125` as the fallback;
  this per-frame check clamps to the nearest bound.)
- If the `df_promode` cvar differs from the cached `0x1317c4`, reset the cvar to the cached value (changing promode at runtime is undone).
- Mirror `com_maxfps` into the SYSTEMINFO cvar `defrag_clfps` (mirror NOT needed for oDFs: cgame checks gated by defrag_gametype>=5, see CGAME/cheat_report.md) and `sv_fps` into `defrag_svfps` (set only when they change) — these are the writers of those ROM cvars.

## 3. Dedicated path (`flag != 0`): idle script
Uses `df_sv_script_idleCfg` (string at `0x130e7c`, flags ARCHIVE) and `df_sv_script_idleDelay` (`0x130d68`, default 50, seconds):
if the cfg is non-empty: with no connected clients (`0x12a338 == 0`) arm `0x130854 = level.time + 1000 * idleDelay`; when `level.time > 0x130854` and `defrag_idle` is not already set →
`trap_SendConsoleCommand( EXEC_APPEND, va("exec %s", idleCfg) )` and set `defrag_idle = "1"`; with clients present `defrag_idle` is reset to `"0"` and the timer cleared.
(Pairs with the `defrag_idle` cvar seen in the string pool: an "empty server → run a config" feature.)

## 4. `fraglimit` from `target_fragsFilter` entities
`if ( 0x131804 != 0 && level.time > 0x131804 ) { 0x131804 = 0; trap_Cvar_Set( "fraglimit", va("%i", *(int*)0x131800) ); ... }` — `0x131800`/`0x131804` are written by the
`target_fragsFilter` think function (`0x2919`, see [../G_TRIGGER/target_fragsFilter.md](../G_TRIGGER/target_fragsFilter.md): `0x131800 = min nonzero ent->count`,
`0x131804 = level.time + 200`), so DeFRaG publishes the smallest `frags` threshold among those entities as the `fraglimit` cvar. The tail after the `fraglimit` set was not read.

## oDFs
Not ported (oDFs has none of these cvars/features except through `df_*` basics). Candidates: the `defrag_clfps`/`defrag_svfps` mirrors and the `sv_fps` clamp are simple; the idle-config script needs the two cvars.
