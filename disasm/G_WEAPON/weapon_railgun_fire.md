# weapon_railgun_fire — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

**DeFRaG address:** `0x28ebf` (vanilla `0x20492`, hash `603f229a` **both
sides** — exact hash match).
**Size:** `0x211` (529) instructions vs. instruction range `0x28ebf`–`0x290ce`
(0x210 = 528, +1 drift, consistent with confirmed-unmodified functions in
this module). This is the largest of the 5 functions in this pass; read in
full, top to bottom (previously only spot-checked: damage=100.0,
range=8192.0, offset=4.0).

## Full decode

```c
void weapon_railgun_fire (gentity_t *ent) {
    damage = Q_ftol( 100.0f * s_quadFactor );          // 0x28ec0-0x28ec6, 0x42c80000=100.0
    VectorMA( muzzle, 8192, forward, end );             // 0x28ec7-0x28ee6, 0x46000000=8192.0 (3rd component re-embeds
                                                          // the literal instead of reusing the cached local — cosmetic
                                                          // codegen difference only, still 8192.0)
    unlinked = 0; hits = 0;                             // 0x28ee7-0x28eec
    passent = ent->s.number;                            // 0x28eed-0x28ef1, *(ent)

    do {                                                 // loop top 0x28ef2 ("from 0x28f6d")
        trap_Trace(&trace, muzzle, NULL, NULL, end, passent, MASK_SHOT);  // 0x28f06, trap -25
        if ( trace.entityNum >= ENTITYNUM_MAX_NORMAL /*0x3fe*/ ) break;   // 0x28f09-0x28f0e
        traceEnt = &g_entities[trace.entityNum];         // 0x28f0f-0x28f16, 0x1b2f0 + entityNum*0x43c
        if ( traceEnt->takedamage ) {                     // 0x28f17-0x28f1d, +0x2e0
            if( LogAccuracyHit( traceEnt, ent ) ) hits++;  // 0x28f1e-0x28f31, call 0x29334 — BEFORE G_Damage (correct vanilla order)
            G_Damage( traceEnt, ent, ent, forward, trace.endpos,
                      damage, 0, MOD_RAILGUN /*0xa*/ );    // 0x28f32-0x28f4c, call 0x2015b
        }
        if ( trace.contents & CONTENTS_SOLID ) break;      // 0x28f4d-0x28f54
        trap_UnlinkEntity( traceEnt );                      // 0x28f55-0x28f5a, trap -32
        unlinkedEntities[unlinked] = traceEnt;               // 0x28f5b-0x28f63
        unlinked++;                                          // 0x28f64-0x28f69
    } while ( unlinked < MAX_RAIL_HITS /*4*/ );              // 0x28f6a-0x28f6d

    for ( i = 0 ; i < unlinked ; i++ )                        // 0x28f6e-0x28f88
        trap_LinkEntity( unlinkedEntities[i] );                // 0x28f7b, trap -31

    SnapVectorTowards( trace.endpos, muzzle );                 // 0x28f89-0x28f8f, call 0x289a6
    tent = G_TempEntity( trace.endpos, EV_RAILTRAIL /*0x35*/ ); // 0x28f90-0x28f97, call 0x284e6
    tent->s.clientNum = ent->s.clientNum;                       // 0x28f9c-0x28faa, both +0xa8
    VectorCopy( muzzle, tent->s.origin2 );                       // 0x28fab-0x28fb0, tent+0x68, block_copy 0xc
    VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );      // 0x28fb1-0x29016 (x/y/z), 0x40800000=4.0
    VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );        // (interleaved above), -0x40800000 bit-pattern = 0xbf800000 = -1.0

    if ( trace.surfaceFlags & SURF_NOIMPACT )                    // 0x29017-0x2901c, trace+0x2c
        tent->s.eventParm = 255;                                  // 0x2901d-0x29024
    else
        tent->s.eventParm = DirToByte( trace.plane.normal );      // 0x29025-0x29031, call 0x8f7f (candidate DirToByte, consistent
                                                                    //   call shape/position, not independently named elsewhere)
    tent->s.clientNum = ent->s.clientNum;                          // 0x29032-0x29040 — SAME redundant duplicate assignment
                                                                    //   present in vanilla source itself (oDFs g_weapon.c line 568,
                                                                    //   right after the eventParm branch) — not a DeFRaG addition

    if ( hits == 0 ) {
        ent->client->accurateCount = 0;                             // 0x29041-0x2904f, +0x318
    } else {
        ent->client->accurateCount += hits;                         // 0x29050-0x29061
        if ( ent->client->accurateCount >= 2 ) {                     // 0x29062-0x2906b
            ent->client->accurateCount -= 2;                          // 0x2906c-0x2907c
            ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;       // 0x2907d-0x2908d, +0x11c
            ent->client->ps.eFlags &= ~AWARD_MASK;                     // 0x2908e-0x2909e, +0x68, band ~0x38849
            ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE /*0x8000*/;   // 0x2909f-0x290af
            ent->client->rewardTime = level.time + 2000 /*REWARD_SPRITE_TIME*/; // 0x290b0-0x290bb, +0x33c, level.time@0x12a310
        }
        ent->client->accuracy_hits++;                                  // 0x290bc-0x290cc, +0x320 — unconditional within this branch,
                                                                         //   matches vanilla's placement outside the >=2 sub-if
    }
}
```

MOD/enum values confirmed against `oDFs`'s `bg_public.h`:
`MOD_RAILGUN = 0xa` (10) matches the enum position
(`MOD_UNKNOWN..MOD_PLASMA_SPLASH` = 0–9, `MOD_RAILGUN` = 10). `EF_AWARD_IMPRESSIVE`
bit value `0x8000` not independently re-derived from `bg_public.h` in this
pass but consistent with the expected reward-sprite-flag pattern already
used identically in vanilla.

`gclient_t.accuracy_hits` confirmed at `+0x320` — same offset independently
confirmed via `ShotgunPattern` (`disasm/G_WEAPON/ShotgunPellet.md`),
cross-validating both decodes.

### `LogAccuracyHit`/`G_Damage` ordering — confirmed CORRECT (vanilla order)

Unlike `ShotgunPellet` (see
[ShotgunPellet.md](ShotgunPellet.md#confirmed-new-finding--g_damage-and-logaccuracyhit-calls-are-swapped)),
`weapon_railgun_fire` calls `LogAccuracyHit` (`0x28f25`, call target
`0x29334`) **before** `G_Damage` (`0x28f4a`, call target `0x2015b`) —
verified directly from the raw call-instruction order, matching vanilla
exactly. `hits` is incremented from `LogAccuracyHit`'s pre-damage-health
result, so the accuracy/`accurateCount`/reward-sound chain that follows is
unaffected by this issue.

## Result: confirmed vanilla-identical

Every constant (`100.0` damage, `8192.0` range, `4.0`/`-1.0` muzzle-trail
offsets, `MAX_RAIL_HITS=4`, `ENTITYNUM_MAX_NORMAL` check, `REWARD_SPRITE_TIME=2000`),
every branch direction, and every struct-field offset used in this function
matches vanilla `weapon_railgun_fire` exactly, including the `#else`
(non-`MISSIONPACK`) `LogAccuracyHit`+`G_Damage` path and vanilla's own
already-present redundant double `tent->s.clientNum` assignment. No `LAG
CHEAT` gate present (confirmed absent via full read). **Confirmed
unmodified.**

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x28ebf` | `weapon_railgun_fire` entry |
| `0x28ec0` | `damage = Q_ftol(100.0 * s_quadFactor)` |
| `0x28ec7` | `VectorMA(muzzle, 8192.0, forward, end)` |
| `0x28f06` | `trap_Trace(...)` (trap `-25`) |
| `0x28f09` | `ENTITYNUM_MAX_NORMAL` (`0x3fe`) break check |
| `0x28f1e`/`0x28f4a` | `LogAccuracyHit`/`G_Damage` calls — confirmed correct (vanilla) order |
| `0x28f4d` | `CONTENTS_SOLID` break check |
| `0x28f55` | `trap_UnlinkEntity` (trap `-32`) |
| `0x28f7b` | `trap_LinkEntity` (trap `-31`) |
| `0x28f8d` | `SnapVectorTowards` call (`0x289a6`) |
| `0x28f95` | `G_TempEntity(trace.endpos, EV_RAILTRAIL)` call (`0x284e6`) |
| `0x29028` | Candidate `DirToByte(trace.plane.normal)` call (`0x8f7f`) |
| `0x29041` | `hits == 0` branch (`accurateCount` reset vs. accumulate) |
| `0x290bc` | `ent->client->accuracy_hits++` (`+0x320`) |

## oDFs implementation notes

No change needed — confirmed vanilla-identical. `oDFs`'s
`weapon_railgun_fire` (`code/game/g_weapon.c`) already matches this exactly.
