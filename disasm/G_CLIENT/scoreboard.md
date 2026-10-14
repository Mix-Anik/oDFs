# DeFRaG scoreboard protocol — qagame `0x1d880`, cgame `0x3fb7` (`DF_SVCmd_ParseScores`), `SortRanks` `0x599`

**Found from the cgame side** (contract audit): cgame `0x3fb7` (called from the DeFRaG server-command dispatcher `0x28d70`) carries the debug strings `"DF_SVCmd_ParseScores() failed: number of clients out of range."` and `"... client number out of range."`. This showed that the DeFRaG `scores` command is NOT the vanilla one. (Earlier survey notes in `G_UTILS/vanilla_identified_survey.md` that called `0x1d880` "vanilla `DeathmatchScoreboardMessage`" were **wrong** — it is a DeFRaG rewrite.)

## Wire format
```
scores <n> <speedAwardUps> <speedAwardClient> "<speedAwardName>" { <client> <bestTimeMs> <ping> <followClient> } * n
```
qagame `0x1d880`: `numSorted = level.numConnectedClients (0x12a338)`; per client of `level.sortedClients (0x12a344)`: `ping = CON_CONNECTING ? -1 : min(ps.ping (client+0x1c4), 999)`; `follow = (sessionTeam == SPECTATOR(3) && spectatorState == FOLLOW(2)) ? sess.spectatorClient (client+0x294) : -1`; entry `" %i %i %i %i"` = `{ sortedClients[i], client+0x2b0, ping, follow }`; stop when the 1024-byte buffer would overflow; then `va("scores %i %i %i \"%s\"%s", i, *0x131844, *0x131848, 0x13184c, string)` through `trap_SendServerCommand(-0x12)`.
cgame `0x3fb7`: `argv(1)` = n (0..64 else the "number of clients out of range" error and n=0), `argv(2)` → `0xec1a8`, `argv(3)` → `0xec1ac` (must be -1..64 else "client number out of range"), `argv(4)` → name (`0xec1b0`), then for i<n: fields `argv(5+4i .. 8+4i)` into a 0x44-stride array at `0xeac58` (+0 client, +4 `client+0x2b0` value, +8 ping, +0x40 follow client). The scoreboard `0x668c` draws columns Player / Score / Ping, `Spectators Info`, and the footer `Speed Award: <name> ^7: %i ups`, `sv:<version> cl:<version>` (`%i.%02i.%02i`).
The vanilla oDFs message (`scores n redScore blueScore` + 14 ints per client) is parsed by this cgame with shifted fields: speed award = team scores, per-client records misaligned (some range checks fail → empty scoreboard + console error).

## The per-client value `client+0x2b0` = best finish time
Written by the finish handler `0x35ce` (`0x36a7..0x36c6`): `old = client+0x2b0; if (old == 0 || old > newTime) { client+0x2b0 = newTime; CalculateRanks(); }`; read by `SortRanks` and `0x1d880`; reset on ClientConnect (not part of the session string). (`newTime` is the finisher's corrected time — assumed the sub-ms-corrected value; the raw/est split in the handler was not separately traced.)

## `SortRanks` (qagame `0x599`) — ranking by best time
Order: (1) `spectatorState == SCOREBOARD || spectatorClient < 0` last; (2) CON_CONNECTING last; **(3) best time: both 0 → equal (returns 0 immediately); the one with 0 last; otherwise smaller time first, equal → 0**; the vanilla spectator-number / team / score tests that follow are unreachable in the DeFRaG build (every path returned above; the block at `0x646..0x6c9` has no incoming branch).

## Speed award (globals `0x131844` value, `0x131848` holder client or -1, `0x13184c` name buffer, `0x1318cc` expire time)
- Track (qagame ClientThink `0x1ac53..0x1ac83`): copy `ps.velocity`, zero z, `speed = (int)length`; `if (speed > 800 && speed > value) { value = speed; holder = ps.clientNum (client+0x8c); name[0] = 0; expire = level.time + 5000 }`. The **guard around this block** (which players/states are tracked) was not decoded.
- Award (`0x23f2`, only when `0x1317c0 != 0` i.e. `g_dfVariant`): when `expire != 0 && level.time >= expire`: log `ClientSpeedAward: <client|-1> <value> "<name>"` (name = stored name if set, else the holder's netname; client = -1 when the stored name is used) and `expire = 0`.
- Disconnect (`0x1d863..0x1d87e`): if the holder disconnects → `holder = -1`, `name = netname` (so the award text survives).
- Init: `G_InitGame` sets `holder = -1`.

## oDFs — PORTED (BEHAVIOUR CHANGE: scoreboard protocol + ordering)
`g_cmds.c` `DeathmatchScoreboardMessage` (new format), `g_main.c` `SortRanks` (best-time ranking; old tail under `#if 0`), `pers.bestFinishTime` (g_local.h), best-time update + `CalculateRanks()` in `G_RaceFinished` (g_defrag.c; uses the est. time `t->bestTime`), speed award `G_SpeedAward{Init,Track,Frame,Disconnect}` (g_defrag.c; hooks: `G_InitRaceMode`, `ClientThink_real` after Pmove for non-spectator live players, `G_RunFrame`, `ClientDisconnect`). Syntax-checked, UNTESTED. Effects: the original DeFRaG cgame now shows a sane scoreboard; players are ranked by best time (no score ordering) — to revert, restore the vanilla `scores` string / entry and the score tail of `SortRanks`. Also affects `CalculateRanks`-driven things (PERS_RANK, follow1/2, `target_fragsFilter` is unaffected as it reads PERS_SCORE).

## Parser constraints verified against the oDFs output (cgame `0x3fb7`)
`n` must be 0..64; speed-award client `-1..63`; every entry's client `0..63`; every entry's follow client `-1` or `0..63` (otherwise `"DF_SVCmd_ParseScores() failed: client number out of range."` and the scoreboard is dropped). The oDFs implementation satisfies all of these: `level.numConnectedClients <= 64`, `g_speedAwardClient` is -1 or a valid client, `follow` is `-1` unless a spectator follows a valid client. (The per-entry best time and ping have no range checks.)

## Iter 10 (full-port loop): oDFs cgame PORTED to the same protocol
Without this the oDFs cgame would have misparsed the oDFs qagame's own (already ported) DeFRaG `scores` command. `CG_ParseScores` (cg_servercmds.c) now reads the DeFRaG format incl. the range errors; `score_t.score` carries the best time in ms (0 = none), `scoreFlags` the follow client; new `cg.speedAwardUps/Client/Name`. `CG_DrawClientScore` shows the best time (`CG_DF_FormatTime`) in the score column (spectators: no time), and `CG_DrawOldScoreboard` draws `Speed Award: <name> ^7: N ups` at the bottom. Column alignment vs the header pictures and the footer position are our layout (cgame `0x668c` draw details undecoded). Not ported: `sv:<version> cl:<version>` footer. Syntax-checked, untested. Revert: restore the vanilla parser/draw lines in cg_servercmds.c / cg_scoreboard.c (and the vanilla server `scores`).
