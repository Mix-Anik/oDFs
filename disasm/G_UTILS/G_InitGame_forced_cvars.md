# DeFRaG `G_InitGame` (`0x1b57`) — cvars forced on every level load

`func_0x1b57` is the game-init function (callers' lists elsewhere name it as the place `func_0x3a77`, `func_0x33dc`/`CheckVote` setup and the `0x406d` salt init are reached from).
Unconditionally, early in its body (bytecode `0x1b70`-`0x1c58`, no branches in between), after caching `df_promode` into `0x1317c4` and clearing the max-speed record
(`0x131848 = -1`), it runs `trap_Cvar_Set( name, va("%i", value) )` (`va`-shaped formatter `0xac4e`, or `Com_sprintf` `0x410a`) for:

| cvar | forced value | note |
|---|---|---|
| `g_speed` | `320` (`0x140`) | |
| `g_gravity` | `800` (`0x320`) | |
| `g_knockback` | `1000` (`0x3e8`) | |
| `g_quadfactor` | `3` | |
| `g_weaponrespawn` | `5` | |
| `g_weaponTeamRespawn` | `5` | explains DeFRaG's table default `5` vs oDFs `30` |
| `dmflags` | `0` | |
| `timescale` | `1` | |
| `defrag_bspcrc` | `sv_mapChecksum` (`"%i"`) | resolves the `defrag_bspcrc` cvar: the map's BSP checksum, ROM, exposed to clients |
| `g_doWarmup` | `"0"` | |
| `fraglimit` | `"0"` | later overwritten by `target_fragsFilter` min (see func_0x23f2.md) |
| `defrag_vers` | `19131` (`0x4abb`; Com_sprintf `"%i"`) | the `"1.91.31"` version number used by the authentication hash |
| `sv_pure` | `"0"` | |

Then it lowercases `mapname` (`0x420b`, `func_0xaadf`) into a buffer and loops (`0x1c59`...) — the later part sets `0x1317d0 = 1`; not read further.

## oDFs
Not ported: forcing these overrides the server operator's config on each map load (physics/timescale/dmflags/sv_pure). oDFs's table defaults already match for `g_speed`,
`g_gravity`, `g_knockback`, `g_quadfactor`, `g_weaponrespawn`; the only default that differs is `g_weaponTeamRespawn` (30 vs 5, irrelevant outside team modes).
Candidate for a future decision if exact DeFRaG parity (anti-cheat style forcing) is wanted.

**Iter 5 (full-port loop): PORTED opt-in** in `G_InitGame` (g_main.c): `df_forceCvars 1` (default 0, no behaviour change unless enabled) forces the 11 cvars above on every level load. `defrag_bspcrc` not set (cgame bsp-crc check is gated off for oDFs). Revert: set `df_forceCvars 0`. Syntax-checked, untested.
