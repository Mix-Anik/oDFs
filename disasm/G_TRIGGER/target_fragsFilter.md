# `target_fragsFilter` (spawn fn `0x2a58`) and its per-frame hook in `func_0x259a`

Found while decoding the tail of `func_0x259a` (see [../G_CLIENT/auth_handshake.md](../G_CLIENT/auth_handshake.md)).
**Not present in oDFs** (`g_spawn.c` has no such entry; grep for `fragsFilter` finds nothing).

## Evidence
Spawn table entry at data `0x2728`/`0x272c`: `{ name = 0x95c0 "target_fragsFilter", fn = 0x2a58 }` — the classname string is in the pool, so this is a name-confirmed identification.

## Spawn function `0x2a58(ent)`
```c
ent->use = 0x2963;                                   // ent+0x2c4 — "fire if score reached" handler (below)
G_SpawnInt( "frags", "0", &ent->count );             // ent+0x2f8 (key 0x3e77; fn 0x248eb = G_SpawnInt, same as entity_mode_exclusion.md)
found = func_0x24885( "targetname", "decoy", &s );   // key 0x3e6c, default 0x3e66 "decoy"; 0x24885 = spawn-string fetch (returns found flag)
if ( !found && *(int*)0x1317d4 < 10 ) {              // entities WITHOUT a real targetname are auto-registered, max 10
    ((gentity_t**)0x1317d8)[ (*(int*)0x1317d4)++ ] = ent;
    ent->+0xcc = 1;
}
ent->think = 0x2919;                                 // ent+0x2b4
ent->nextthink = level.time + 400;                   // ent+0x2b0, +0x190
```
(`0x24885` returning "found" is inferred from how its result is branched on; not hash-checked.)

## Per-frame hook (tail of `func_0x259a(ent)`, reached from `ClientEndFrame`)
```c
for ( i = 0; i < *(int*)0x1317d4; i++ ) {
    f = ((gentity_t**)0x1317d8)[i];
    if ( client->ps.persistant[0] /* client+0xf8 */ >= f->count )
        f->use( f, NULL, ent );                      // call through f+0x2c4
}
if ( !(*(int*)0x13182c & 6) ) {                      // weaponsPickup disabled (see G_TRIGGER/func_0x274b.md)
    w = client->ps.weapon;                           // client+0x90
    if ( w != 1 && w != 10 ) client->ps.ammo[w] = 200;   // client+0x178 + w*4
}
func_0x3d37( client );                               // not decoded
```
**Resolves** the long-open `client+0xf8`: it is `ps.persistant[0]` = `PERS_SCORE` (stats base `+0xb8`, 16 entries → `+0xf8`);
`func_0x274b` (spawn loadout) clearing it at spawn = resetting the score, and the registry fires every `target_fragsFilter`
whose `frags` threshold the player's score has reached.

## `use` handler `0x2963` (partially read)
`use(ent=arg0, other=arg1, activator=arg2)`: returns if `activator->client == NULL`; tests `ent->spawnflags (+0x210) & 0x10` together with
`activator->client->ps.persistant[0] == ent->count` (exact-match mode when bit `0x10` set), otherwise `score >= count`, with `spawnflags & 4`
as another gate; the rest (what it fires) was **not decoded**. `think 0x2919` (**resolved later**): loops entities via `G_Find(…, "targetname"?, string 0x3eb6)` (`0x2809c`), keeps the smallest non-zero `ent->count` in `0x131800` (`gti`/`ne` guards at `0x2938`-`0x293c`) and stamps `0x131804 = level.time + 200`; `func_0x23f2` then publishes `0x131800` as the `fraglimit` cvar ([../G_UTILS/func_0x23f2.md](../G_UTILS/func_0x23f2.md)). Earlier docs called `0x2919` "stopTimer entity lookup" — that label was wrong.

## `use` handler `0x2963` and think `0x2919` — FULLY DECODED, entity PORTED
`use(ent, other, activator)` (activator needs a client; `score = client->ps.persistant[0]`, `ent->count` = `frags`, spawnflags `ent+0x210`):
1. `if (f&0x10 && score != count) return` (exact-match mode).
2. `if (score < count)`: `if (!(f&4)) return;` else `SendServerCommand(activator, "cpsm \"%i more frag needed\"")` when `count-score == 1`, otherwise `"... more frags needed"` (small centre print, see target_smallprint.md) and return.
3. Registered ents (`ent+0xcc != 0`, the auto-registry flag) are rate-limited: `if (ent->wait (+0x31c) <= level.time) wait = level.time+1000 (then fire) else { wait = level.time+1000; return }` — fires once, then stays quiet until the per-frame polling has stopped for a second.
4. `if (f&1) score -= count; if (f&8) score = 0;` then `G_UseTargets(ent, activator)` (`0x28151`).
Think `0x2919` (re-confirmed): `G_Find(.., targetname, ent->target)` loop; for each found entity whose classname is `"target_stopTimer"`: `0x131800 = min nonzero(count)` and `0x131804 = level.time + 200`; published as `fraglimit` by `0x23f2` (see func_0x23f2.md).
**oDFs — PORTED:** `g_target.c` (`SP_target_fragsFilter`, `Use_Target_FragsFilter`, think, registry of <=10 unnamed filters, `G_FragsFilterCheck` called from `ClientEndFrame`, `G_FragsFilterPublish` from `G_RunFrame` before `CheckCvars`, `G_FragsFilterInit` from `G_InitRaceMode`), `gentity_t.fragsRegistered`.
Not ported: the same hook's forced `ammo[weapon] = 200` when `df_ndm_weaponsPickup` is off (oDFs already refills via its own rules; unverified) and the undecoded `0x3d37` part? — `0x3d37` is now decoded: sets/clears `ps.stats[12]` bit `0x20` from global `0x1320f8` and bit `0x40` from `0x1320f4` (unknown writers).
**Behaviour change:** only maps using `target_fragsFilter` (new entity). A filter feeding a `target_stopTimer` sets the `fraglimit` cvar, so reaching that score ends the map exactly like DeFRaG. Revert = remove the spawn entry / `G_FragsFilterPublish` call. Syntax-checked, untested.
