# `func_0xd1f` = `CheckTournament` (called from `G_RunFrame` region, `0x118c`)

**Address:** `0xd1f` (size `0x15e`). Not hash-matched to the vanilla table, but a full read of the body (this pass) shows the vanilla `CheckTournament` structure line for line:

Globals: `0x12a340` = `level.numPlayingClients` (see `G_CLIENT/func_0x6cc.md`), `0x12a300` = `level.warmupTime`, `0x12a310` = `level.time`, `0x12a334` = `level.restarted`, `0x12a450` = `level.warmupModificationCount`, `0x19644` / `0x1964c` = `g_warmup.modificationCount` / `g_warmup.integer`, `0x1b1ec` = `g_gametype`. Callees: `0x477` = AddTournamentPlayer (by position/role; not verified in its own body), `0x1cb3b(-1, team)` = TeamCount, `0xac4e` = va, `0xa1a` = G_LogPrintf; trap `-0x13` = SetConfigstring (CS_WARMUP = 5), `-6` = Cvar_Set, `-0xf` = SendConsoleCommand.

| Step (as decoded) | Vanilla equivalent |
|---|---|
| `numPlayingClients == 0` -> return | same |
| `g_gametype == 1` (tournament): `< 2` players -> `0x477`; `!= 2` -> `warmupTime = -1`, CS_WARMUP, log `"Warmup:\n"`, return | same |
| `warmupTime == 0` -> return; `g_warmup.modificationCount != level.warmupModificationCount` -> store, `warmupTime = -1` | same |
| `warmupTime < 0`: need exactly 2 players; `g_warmup > 1` ? `time + (g_warmup-1)*1000` : `0`; CS_WARMUP | same |
| `level.time > warmupTime` -> `warmupTime += 10000`, `g_restarted 1`, `map_restart 0`, `restarted = 1` | same |
| other gametypes (`!= 2` and `warmupTime != 0`): `notEnough` = (`gametype > 3` ? either team's TeamCount < 1 : `numPlayingClients < 2`); same warmup state machine | same |

No behavioural delta found; why the hash differs (compiler layout / the duplicated restart block) is **not determined**. oDFs `CheckTournament` (`g_main.c`) already equals this. Note: DeFRaG forces `g_doWarmup 0` every map load (`G_UTILS/G_InitGame_forced_cvars.md`), so this path is effectively idle in DeFRaG runs.
Method note: the "undocumented function" worklist script must read docs with a native Windows path (`D:/...`), not `/d/...`, or it lists ~270 phantom functions. After this pass 48 remain (largest next: `0xf11`, `0x629b`, `0x1e3b8`, `0x1972`, `0x25881`).
