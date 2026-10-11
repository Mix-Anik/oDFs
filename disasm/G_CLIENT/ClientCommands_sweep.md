# DeFRaG `ClientCommand` (`0x1f4ba`) — command table sweep

String-compare chain inside `ClientCommand`. Commands present (in order): `say say_team tell vsay vsay_team vtell vosay vosay_team votell vtaunt score give god notarget noclip kill teamtask levelshot follow follownext followprev team where callvote cv vote callteamvote teamvote gc setviewpos stats placeplayer respawnpoint firemissile arena ignore`, then
`"unknown cmd %s"`. Plus a pre-dispatch (`func_0x2d45`) for `auth_reply`/`notify`
([auth_handshake.md](auth_handshake.md)).

Compared with oDFs's `ClientCommand`: oDFs lacked **`respawnpoint`** (ported, [respawnpoint.md](respawnpoint.md)),
**`firemissile`** (ported, below), **`cv`** (alias of `callvote`, handler `0x3142`; decoded, NOT ported — see [callvote.md](callvote.md)), **`arena`** (handler `0x4a5f`; decoded + ported, see [arena_command.md](arena_command.md)) and **`ignore`** (handler `0x1de5e`; decoded + ported, see [ignore_command.md](ignore_command.md)).
Handlers: `placeplayer` = `0x468e` (in oDFs already), `respawnpoint` = `0x47b1`, `firemissile` = `0x489b`.

## `firemissile` (`func_0x489b`) — cheat command, DECODED
Cheat gate: cached `sv_cheats` (`func_0x3e0a`), else `"Cheats are not enabled on this server."` (`0x46bd`); requires `trap_Argc() == 10`,
else usage `"Usage: firemissile (start) x y z (forward) x y z weapon_num (4|5|8|9) quaded (0|1) self-style (0|1)"` (`0x4533`).
```c
start = argv(1..3); forward = argv(4..6); weapon = (int)atof(argv(7));
quad  = atof(argv(8)) > 0 ? 3.0f : 1.0f;                 // 0x40400000 / 0x3f800000
self  = atof(argv(9));
if ( self == 0 ) { saved = ent->s.number; ent->s.number = -1; }   // ownerless missile
for i<3: start[i] += 14.0f * forward[i]; start[i] = (int)start[i];  // 0x41600000 = 14.0, then truncation
switch ( weapon ) {
 case 4: forward[2] += 0.2f; VectorNormalize(forward) /*0x9d12*/; m = fire_grenade /*0x2259a*/(ent,start,forward); break;
 case 5: m = fire_rocket  /*0x2276a*/(ent,start,forward); break;
 case 8: m = fire_plasma  /*0x224b5*/(ent,start,forward); break;
 case 9: m = fire_bfg     /*0x22685*/(ent,start,forward); break;
}
if ( self == 0 ) ent->s.number = saved;
if ( m ) { m->damage = (int)(m->damage * quad); m->splashDamage = (int)(m->splashDamage * quad); }  // m+0x2e4, +0x2e8
```
The four `fire_*` identities are by weapon-number correspondence (4=GL, 5=RL, 8=PG, 9=BFG) and vanilla `(ent,start,dir)` arity; they
were not hash-matched individually in this pass. Differences from `CheatsOk` in oDFs: DeFRaG checks only `sv_cheats`
(oDFs's `CheatsOk` also refuses dead players).

**Ported** to oDFs as `Cmd_FireMissile_f` (`g_cmds.c`); syntax-check clean, not runtime-tested.
