# `func_0x6cc` — `CalculateRanks` (DeFRaG)

**Address:** `0x6cc` (`enter 0x3c`), no args. Callers listed: `0x35ce`, `0x1aea7`, `0x1d05a` (`ClientConnect`),
`0x1d18b` (`ClientBegin`), `0x1d6dc` (`ClientDisconnect`), `0x1f846` — the same
places vanilla calls `CalculateRanks`.

Body (prologue decoded, bytecode `0x6cc`-`0x792`): resets `level` globals —
`0x12a444 = 0x12a448 = -1` (`follow1`/`follow2`), `0x12a338 = 0` (`numConnectedClients`),
`0x12a33c = 0` (`numNonSpectatorClients`), `0x12a340 = 0` (`numPlayingClients`),
`0x12ac64 = 0` (`numVotingClients`); loops `i < level.maxclients` (`0x12a308`) over
`g_clients` (`0x12a2f0`, stride `0x37c`): if `pers.connected (+0x1d4) != 0` →
`level.sortedClients[numConnectedClients++] = i` (`0x12a344`); if
`sess.sessionTeam (+0x284) != 3 (TEAM_SPECTATOR)` → `numNonSpectatorClients++`, and if
`pers.connected == 2 (CON_CONNECTED)` → `numPlayingClients++` and, if `follow1 == -1` then `follow1 = i`,
else if `follow2 == -1` then `follow2 = i`; **`numVotingClients++` when
`!(g_entities[i].r.svFlags & 8 /*SVF_BOT*/)`** (`0x1b498 = 0x1b2f0 + 0x1a8`); then
`qsort(sortedClients, numConnectedClients, 4, 0x599 /*SortRanks*/)` via `0x5843`.
Structure and constants match vanilla `CalculateRanks` exactly in the part read;
the remaining tail (rank/score assignment, tournament/team handling, configstring
update) was **not** read this pass. No difference from oDFs's vanilla `CalculateRanks` in the part read (vanilla also excludes bots from `numVotingClients`, `g_main.c:844-861`).

Side result: `ent+0x1a8` = `gentity_t.r.svFlags` in this build.
