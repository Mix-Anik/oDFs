# DeFRaG session persistence — `0x24674` (write) / `0x246ee` (read)

**Address:** `0x24674` = `G_WriteClientSessionData(client)` (called from `0x24765` G_InitSessionData and `0x24825`), **Address:** `0x246ee` = `G_ReadSessionData(client)` (called from `0x1d05a` = ClientConnect). Both use cvar `session<clientNum>` (`clientNum = (client - 0x12a2f0)/0x37c`) with format string `"%i %i %i %i %i %i %i %i %f %f %f %d %d"` (data `0x923d`), `trap_Cvar_Set` (`-6`) / `Cvar_VariableStringBuffer` (`-8`).

Field order (client-relative offsets, from the `arg` list of the write; **corrected** after cross-checking `ClientUserinfoChanged` `0x1ccdf` — its `w\%i\l\%i` args are `0x298`/`0x29c` — and `AddTournamentPlayer` `0x477`, which compares `0x28c` (spectatorNum), `0x290 == 3` (spectatorState == SCOREBOARD), `0x294 >= 0` (spectatorClient)):
| # | offset | meaning |
|---|---|---|
| 1 | `0x284` | `sessionTeam` |
| 2 | `0x288` | **`lastTeam` — not in vanilla**: last non-spectator team seen by ClientBegin |
| 3 | `0x28c` | `spectatorNum` (next-in-line stamp) |
| 4 | `0x290` | `spectatorState` |
| 5 | `0x294` | `spectatorClient` |
| 6 | `0x298` | `wins` |
| 7 | `0x29c` | `losses` |
| 8 | `0x2a0` | `teamLeader` |
| 9-11 | `0x2a4/8/c` | `respawnpoint` floats (`G_CLIENT/respawnpoint.md`) |
| 12-13 | `0x2b4/0x2b8` | 64-bit chat-ignore mask (`G_CLIENT/ignore_command.md`) |
(`0x2b0`, the best-time/rank word, is NOT persisted.)

`lastTeam` use (ClientBegin `0x1d1d6..0x1d20a`): `if sessionTeam != SPECTATOR(3) && client+0x288 != sessionTeam { client+0x288 = sessionTeam; client+0x2a4/8/c = 0 }` — i.e. changing to another real team clears the saved respawn point (this is the "ClientBegin team-change block" that respawnpoint.md said was unported). Other `0x288` hits (`0x219b7`, `0x28151`, `0x280eb`) are **gentity** fields, not client session: `0x280eb` = `G_PickTarget(targetname)` (strings `"G_PickTarget called with NULL targetname"` / `"G_PickTarget: target %s not found"`), `0x2809c` = `G_Find(from, fieldofs, match)` (called with `fieldofs = 0x28c` = `targetname`), `0x28151` = `G_UseTargets` (`G_UTILS/G_UseTargets.md`), `0x219b7` = misc_portal_surface setup (`"Couldn't find target for misc_partal_surface"`). So `ent+0x288` = `target`, `ent+0x28c` = `targetname`. `client+0x288` = lastTeam has exactly one reader (ClientBegin) + the session read/write.
## `0x24825` = `G_WriteSessionData(restart)` (decoded this pass) — respawn point survives map_restart only
Called from `G_ShutdownGame` (`0x3fe`, with its `restart` arg) and from `ExitLevel` (`0x990`, with `0`). Body: `trap_Cvar_Set("session", va("%i", g_gametype))`, then for each `CON_CONNECTED` client: **`if (restart == 0) client+0x2a4/8/c = 0`** (respawn point cleared) and `0x24674(client)` (write client session). So the respawn point persists across `map_restart` (`restart != 0`) but is dropped on a real map change / intermission exit. The ignore mask is always kept.
oDFs: PORTED — `G_WriteSessionData( int restart )` (g_session.c / g_local.h; `G_ShutdownGame` passes `restart`, `ExitLevel` passes `0`). This tightens the earlier port (which would have kept the point across map changes). Revert = delete the `if ( !restart )` block.

## `0x1c79(restart)` (called from `G_ShutdownGame`, `0x431`)
When `restart == 0`: `trap_Cvar_Set` to `""` of `defrag_svsc`, `df_ndm_timer_start`, `df_ndm_timer_checkpoint`, `df_ndm_timer_stop` (the timer-entity advertisement cvars; `defrag_svsc` is built by `func_0x1e60`, see `G_TRIGGER/func_0x1e60.md`). oDFs clears the same cvars at game init (`g_defrag.c`), so no behavioural gap; `defrag_svsc` itself is not read by the DeFRaG cgame (no `"defrag_svsc"` string in cgame.dis).
