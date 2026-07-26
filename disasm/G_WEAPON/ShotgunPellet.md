# ShotgunPellet / ShotgunPattern — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

Both functions were previously only spot-checked (damage=10.0 for
`ShotgunPellet`; spread=700.0/×16 for `ShotgunPattern`). This pass reads both
in full, instruction-by-instruction.

## ShotgunPellet — MODIFIED (confirmed new finding)

**DeFRaG address:** `0x28bd0` (vanilla `0x201fb`, hash `5dbcc0c8` **both
sides** — exact hash match).
**Size:** `0x88` (136) instructions vs. instruction range `0x28bd0`–`0x28c56`
(0x87 = 135, the usual "+1" drift on unmodified functions in this module).

### Structure (matches vanilla step-for-step, except where noted)

```c
qboolean ShotgunPellet( vec3_t start, vec3_t end, gentity_t *ent ) {
    passent = ent->s.number;                 // 0x28bd1: *(int*)ent, gentity_t+0x0
    VectorCopy(start, tr_start);              // 0x28bd6 block_copy 0xc
    VectorCopy(end, tr_end);                  // 0x28bda block_copy 0xc
    for (i = 0; i < 10; i++) {                // 0x28be1 loop top ("from 0x28c52")
        trap_Trace(&tr, tr_start, NULL, NULL, tr_end, passent, MASK_SHOT);  // 0x28bf5, trap -25
        traceEnt = &g_entities[tr.entityNum];  // 0x28bf8: 0x1b2f0 + entityNum*0x43c
        if ( tr.surfaceFlags & SURF_NOIMPACT ) return qfalse;   // 0x28c00-0x28c09
        if ( traceEnt->takedamage ) {          // 0x28c0a-0x28c10, gentity_t+0x2e0
            damage = Q_ftol( 10.0f * s_quadFactor );   // 0x28c11, confirmed 0x41200000=10.0
            G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_SHOTGUN /*1*/ );  // 0x28c18-0x28c32, call 0x2015b
            hitClient = LogAccuracyHit( traceEnt, ent );         // 0x28c33-0x28c3c, call 0x29334 — CALLED AFTER G_Damage, SEE BELOW
            return hitClient;                                   // 0x28c3d-0x28c44
        }
        return qfalse;                          // 0x28c45-0x28c47 (unconditional, matches vanilla's dead "loop that never loops" quirk)
    }
    return qfalse;   // 0x28c49-0x28c56: i++/i<10 back-edge and trailing return are present but unreachable, same dead-code shape as vanilla
}
```

MOD_SHotgun value confirmed: `const 1 0x1` = `MOD_SHOTGUN` (1) per
`oDFs`'s `bg_public.h` `MOD_*` enum (`MOD_UNKNOWN=0, MOD_SHOTGUN=1, ...`).
The 10-iteration "loop" is dead code in both vanilla and DeFRaG (every path
inside the loop body unconditionally returns during the first iteration,
including the `if(traceEnt->takedamage)` false path) — this is a known
vanilla-Q3 quirk, not a DeFRaG difference.

### CONFIRMED NEW FINDING — `G_Damage` and `LogAccuracyHit` calls are swapped

Vanilla `code/game/g_weapon.c`'s `ShotgunPellet`:

```c
if( LogAccuracyHit( traceEnt, ent ) ) {
    hitClient = qtrue;
}
G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_SHOTGUN);
return hitClient;
```

`LogAccuracyHit` is called **before** `G_Damage`, so its
`target->client->ps.stats[STAT_HEALTH] <= 0` check always sees the target's
**pre-hit** health.

DeFRaG's compiled order is the reverse — verified by reading the raw
`call` instructions directly (not inferred from argument-push order, which
can't be reordered independently of the call itself in QVM bytecode, a
simple sequential stack machine with no out-of-order execution):

```
00028c30  const  1  0x2015b   ; G_Damage        <- CALLED FIRST
00028c31  call   -1
00028c32  pop    -1
...
00028c3a  const  1  0x29334   ; LogAccuracyHit  <- CALLED SECOND
00028c3b  call   -1
00028c3c  store4 -2
```

The function's return value is computed directly from the (now
post-`G_Damage`) `LogAccuracyHit` result (`0x28c3d`-`0x28c44`), i.e. there is
no separate pre-damage snapshot of `hitClient` — DeFRaG's version doesn't
merely reorder two independent statements, it changes what `LogAccuracyHit`
observes.

**Behavioral consequence:** because `G_Damage` may reduce `target`'s health
to `<= 0` (a killing or already-below-zero-inducing hit), and
`LogAccuracyHit` treats `stats[STAT_HEALTH] <= 0` as "don't count this hit",
**a shotgun pellet that drops its target's health to 0 or below is *not*
counted toward `ent->client->accuracy_hits` in DeFRaG**, whereas vanilla Q3
always counts it (health is checked before the damage is applied). Damage
itself is unaffected — `G_Damage` still runs with the same arguments either
way; only the accuracy-tracking bookkeeping changes, and only for the
specific pellet that brings the target to `<=0` health.

This is an isolated change: the same `LogAccuracyHit`+`G_Damage` pattern
appears at 3 other confirmed sites in this build (`weapon_railgun_fire`
below, `G_RadiusDamage`, `G_MissileImpact` — see
[LogAccuracyHit.md](LogAccuracyHit.md)) and **all three** call
`LogAccuracyHit` before `G_Damage`, matching vanilla order. `ShotgunPellet`
is the only one of the four found to reverse it. `Bullet_Fire`
(machinegun) also calls both, but its own ordering was **not** independently
re-verified in this pass (out of scope of the 5 target functions) — flagged
here as worth checking in a future pass, not confirmed either way.

**Hash-caveat relevance:** `ShotgunPellet`'s hash matches vanilla exactly
(`5dbcc0c8`) despite this reordering. This reinforces the project's
documented hashing caveat (opcode-shape hash, not order/content-sensitive in
a way that would necessarily catch a call-block swap) — a hash match here
did **not** indicate byte-for-byte behavioral identity, consistent with
prior findings (`Bullet_Fire`'s removed `mod` param,
`Weapon_GrapplingHook_Fire`'s redundant re-aim).

## ShotgunPattern — anomaly found (inert, no observed behavioral effect)

**DeFRaG address:** `0x28c57` (vanilla `0x20282`, hash `-51e848f3` **both
sides** — exact hash match).
**Size:** `0xe6` (230) instructions vs. instruction range `0x28c57`–`0x28d3b`
(0xe4 = 228, +1 drift, consistent).

### Structure — matches vanilla exactly for all observable behavior

```c
void ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent ) {
    hitClient = qfalse;                                          // 0x28c58
    VectorNormalize2( origin2, localForward );                    // 0x28c5b, call 0x9d82
    PerpendicularVector( localRight, localForward );               // 0x28c63, call 0xa27e
    CrossProduct( localForward, localRight, localUp );             // 0x28c6a, call 0x8ed8
    /* anomaly: extra dead field read here, see below */
    for ( i = 0 ; i < 11 ; i++ ) {                                 // 0x28c7d init, 0xb=11 loop bound at 0x28d38 confirmed DEFAULT_SHOTGUN_COUNT
        r = Q_crandom(&seed) * 700.0f * 16.0f;                     // 0x28c80-0x28c8d, call 0x8d1a (Q_crandom)
        u = Q_crandom(&seed) * 700.0f * 16.0f;                     // 0x28c8e-0x28c9b
        end = origin + (8192*16) * localForward;                   // 0x28c9c-0x28cad, 0x48000000=131072.0
        end = end + r * localRight;                                // ...
        end = end + u * localUp;                                   // ...
        if ( ShotgunPellet( origin, end, ent ) && !hitClient ) {   // 0x28d03-0x28d1b, call 0x28bd0
            hitClient = qtrue;
            ent->client->accuracy_hits++;                          // 0x28d1f-0x28d2f, gclient_t+0x320 (new confirmed offset)
        }
    }
}
```

Spread constant `700.0` (`0x442f0000`) and `×16` (`0x41800000`) confirmed
directly (matches earlier spot-check). `DEFAULT_SHOTGUN_COUNT` confirmed
hardcoded as literal `0xb` (11) in the loop-bound compare at `0x28d38`/`0x28d39`
— not read from any cvar. `accuracy_hits` field offset confirmed as
`gclient_t+0x320` (also independently confirmed via `weapon_railgun_fire`
below, same offset used there).

### Anomaly — extra dead-code field read (present in DeFRaG only, no observed effect)

Immediately before the `i = 0` loop-counter initialization, DeFRaG's
`ShotgunPattern` contains an 11-instruction block with no vanilla-source
counterpart:

```
00028c73  local  1  0x54
00028c74  local  1  0x90     ; arg3 (ent)
00028c75  load4
00028c76  const  1  0x204    ; gentity_t.client offset (confirmed elsewhere)
00028c77  add    -1
00028c78  load4
00028c79  const  1  0xf8
00028c7a  add    -1
00028c7b  load4
00028c7c  store4 -2
    ;; local54 = *(int*)( (char*)ent->client + 0xf8 )
```

This reads a 4-byte field at `gclient_t+0xf8` into a local variable
(`local54`) that — after an exhaustive read of the entire remaining function
body (`0x28c7d`–`0x28d3b`) — is **never referenced again**. It has no
observable effect: it doesn't feed a branch, a store, or an argument
anywhere else in the function.

Offset `+0xf8` falls immediately after the confirmed `playerState_t.stats[]`
array (`stats[0]` at `+0xb8` through `stats[13]` at `+0xec`, per
`disasm/README.md`; `gclient_t.ps` is embedded at offset 0, so
`client+0xf8` = `ps+0xf8`). In vanilla `playerState_t`, `persistant[]`
immediately follows `stats[MAX_STATS]`, which would put `persistant[0]`
(`PERS_SCORE`) at this exact offset — **this specific field identity is not
independently confirmed** (no read of `MAX_STATS` or a `persistant[]` array
declaration was performed), reported as a plausible-but-unconfirmed
candidate, not a fact.

**Assessment:** dead/inert code — no behavioral difference identified from
this specific instruction block. It is nonetheless a genuine bytecode-level
difference from vanilla (11 extra instructions not accounted for by any
vanilla source statement), and is presumably why this project's hashing
caveat applies here too: despite these extra instructions, the hash still
matched vanilla's exactly.

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x28bd0` | `ShotgunPellet` entry |
| `0x28c00` | `SURF_NOIMPACT` check (`tr.surfaceFlags`) |
| `0x28c0a` | `traceEnt->takedamage` check (`+0x2e0`) |
| `0x28c11` | `damage = Q_ftol(10.0 * s_quadFactor)` |
| `0x28c18` | `G_Damage(...)` call (`0x2015b`) — **now runs before `LogAccuracyHit`** |
| `0x28c33` | `LogAccuracyHit(...)` call (`0x29334`) — **new: runs after `G_Damage`** |
| `0x28c57` | `ShotgunPattern` entry |
| `0x28c73` | Anomalous dead field read: `*(ent->client + 0xf8)`, unused |
| `0x28d03` | `ShotgunPellet(origin, end, ent)` call (`0x28bd0`) |
| `0x28d1f` | `ent->client->accuracy_hits++` (`gclient_t+0x320`) |
| `0x28d38` | Loop-bound compare, confirms `DEFAULT_SHOTGUN_COUNT=11` hardcoded |

## oDFs implementation notes

**Implemented.** `ShotgunPellet` in `code/game/g_weapon.c` now calls
`G_Damage(...)` before `LogAccuracyHit(...)`, matching DeFRaG's confirmed
call order. The `ShotgunPattern` dead-field-read anomaly requires no code
change (no observed behavioral effect) and remains documented for
completeness only.
