# `arena` command and multi-arena spawn tagging

**Resolves** `client+0x374` (the "tag" passed to `func_0x1c6be`) and `spot+0x2f8` (the tag filter field): both are the
**arena number** of DeFRaG's multi-arena map support.

## Evidence
- `func_0x4a5f` (`ClientCommand` handler for `"arena"`, call at `0x1f7d6`): strings `"print \"Not a multiarena map\n\""` (`0x4515`)
  and `"print \"Arena: %i of %i\n\""` (`0x44fc`).
- Spawn side (`0x1c501`-`0x1c55c`, the DeFRaG `SP_info_player_deathmatch`; `0x1c55d` is the `info_player_start` wrapper that sets
  `classname = "info_player_deathmatch"` `0x8115`): `G_SpawnInt("arena" /*0x812c*/, "0" /*0x813b*/, &v)`; `ent->+0x2f8 (count) = v`;
  `if (*(int*)0x12c6ec < v) *(int*)0x12c6ec = v;` — `0x12c6ec` = number of arenas (max arena key seen). The same function also
  sets `ent->flags (+0x218) |= 0x4000` for another key before it (`0x1c523`; key not identified).

## `func_0x4a5f(ent)` (arena)
```c
if ( numArenas /*0x12c6ec*/ == 0 ) { send "Not a multiarena map"; return; }
n = atoi( argv(1) );                                          // 0x5f67 (atoi-shaped)
if ( trap_Argc() != 2 || client->+0x374 == n || n > numArenas ) { send va("Arena: %i of %i", client->+0x374, numArenas); return; }
client->+0x374 = n;
ClientBegin( ent - g_entities );                              // 0x1d18b
client->+0x350 = level.time + 5000;                           // same field/timeout as written after Cmd_Team (0x1e44c); NO reader of client+0x350 exists anywhere in qagame (only these two writes — every `const 0x350` was checked), so it is a write-only vestige
```
(no lower bound check on `n` besides `!= current` and `<= numArenas`.)

## Spawn use
`func_0x1c6be(..., tag = ent->client->+0x374)` keeps only spawn candidates with `spot->+0x2f8 == tag` (see [func_0x1c6be.md](func_0x1c6be.md)),
so `arena N` + `ClientBegin` respawns the player at one of arena N's spawn points; untagged maps use arena 0 for both sides.
`ClientSpawn` also copies `0x12c6ec`-related logic at `0x1d298` (non-spectator `sessionTeam == 3` path) — not decoded.

## oDFs
**Ported** (syntax-check clean, not runtime-tested): `level.numMultiArenas` (named to avoid oDFs's unrelated bot-arena `g_numArenas`),
`clientPersistant_t.arena`, `arena` key parsing in `SP_info_player_deathmatch`, spawn filter `spot->count == arena` in
`SelectRandomFurthestSpawnPoint` (via a file-static set at the top of `ClientSpawn`), and `Cmd_Arena_f`. Not ported: the
`client+0x350 = level.time + 5000` stamp (its consumer was not located) and the `0x1d298` `ClientSpawn` branch. Note oDFs's other
spawn selectors (`SelectNearest...`, CTF, initial spawn) do not apply the arena filter.
