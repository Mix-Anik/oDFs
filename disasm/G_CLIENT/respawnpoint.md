# `respawnpoint` client command and `client+0x2a4/0x2a8/0x2ac`

**Resolves** the long-open identity of `client+0x2a4` (vec3; `+0x2a8`/`+0x2ac` are its y/z): it is the
player's **saved respawn reference point**. All-zero = unset.

## Evidence (embedded strings, bytecode `0x47b1`-`0x489a`)
`func_0x47b1(ent)` is a client command, dispatched from `ClientCommand` (`0x1f4ba`; compare at `0x1f7a1`
against the string `"respawnpoint"` at pool `0x8175`, call at `0x1f7ae`). Strings: `"set"` (`0x4647`), `"clear"` (`0x4641`),
`"print \"^2Respawnpoint Set:^7 ( %.3f %.3f %.3f )\n\""` (`0x45a0`), `"print \"^2Respawnpoint Cleared\n\""` (`0x4621`),
`"print \"Usage: respawnpoint set \n respawnpoint set x y z \n respawnpoint clear\n\""` (`0x45d2`).

```c
argv1 = trap_Argv(1);
if ( !Q_stricmp(argv1,"set") && trap_Argc() == 5 )  point = (atof(argv2), atof(argv3), atof(argv4));   // atof = 0x5dd7
else if ( !Q_stricmp(argv1,"set") )                 point = client->ps.origin;                         // block_copy client+0x14
else if ( !Q_stricmp(argv1,"clear") ) { client->+0x2a4..0x2ac = 0; print("Respawnpoint Cleared"); return; }
else { print(usage); return; }
client->+0x2a4 = point;  print("Respawnpoint Set: ( x y z )");        // trap -0x12 = SendServerCommand
```
No cheat gate (unlike the neighbouring `placeplayer`, `0x489b`, which checks the cached `sv_cheats`).

## Use in spawning
`ClientSpawn` (3 call sites `0x1d2ff`/`0x1d34c`/`0x1d37b`): `if ( VectorLength(client->+0x2a4) != 0 ) spot =
O3J_SelectSpawnPoint_UseClosest(ent,&origin,&angles); else spot = func_0x270ee(...)` (the CTF/furthest dispatch).
`O3J_SelectSpawnPoint_UseClosest` picks the in-use spawn entity (classnames `info_player_start`/`info_player_deathmatch`/
`team_CTF_{red,blue}{player,spawn}`) whose `r.currentOrigin` is closest to the point, outputs `s.origin` with **z + 9.0f**
(`0x45bf`, `0x41100000`) and `s.angles`.

## Corrections made to older docs
- `O3J_SelectSpawnPoint_UseClosest.md` said the early-out is taken when `VectorLength(+0x2a4) != 0`. **Wrong polarity**:
  bytecode `0x44d2 nef → continue` means it returns NULL when the length **is 0** (no point set) and proceeds otherwise;
  this agrees with the caller's guard. Also the per-candidate skip is `if (!cand->inuse) continue` (`0x44e3 ne → process`),
  not "skip when inuse"; and the +9 z offset was missing from the decode.
- `ClientBegin.md`'s team-change block that zeroes `+0x2a4/+0x2a8/+0x2ac` therefore **clears the player's saved respawn point
  when they change team** (PORTED: `sess.lastTeam`, see session_data.md).

## oDFs
**Ported**: `clientPersistant_t.respawnPoint` (`g_local.h`; survives `ClientSpawn`'s pers save/restore, zeroed on connect),
`Cmd_RespawnPoint_f` (`g_cmds.c`, dispatched next to `placeplayer`), `SelectClosestSpawnPoint` + hook in `ClientSpawn`
(`g_client.c`, placed before the CTF/FFA selection for non-spectators; falls back to the normal selection if the point is
unset or no spawn entity exists — DeFRaG's own fallback chain after a NULL result is `func_0x1c6be`, not replicated).
Syntax-check clean (`g_client.c`, `g_cmds.c`, `g_active.c`, `g_main.c`). Not runtime-tested.
