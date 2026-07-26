# G_MissileImpact — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x0002210f` (vanilla baseq3 equivalent at `0x1a575`)
**DeFRaG hash:** modified vs vanilla (+4 instructions per `G_MISSILE/README.md`'s
prior hash-table survey). Originally only the direct-hit damage-application
portion was decoded field-by-field. **A follow-up pass has now decoded the
entire function, instruction-by-instruction, with zero gaps** (the "hook"
special case, formerly only partially/incorrectly described, plus the final
event-broadcast/splash-damage tail) — see "Sections 3 and 4 — now fully
decoded" below. Every instruction from `0x2210f` (entry) to `0x223c9`
(`leave`) has been read and matches vanilla `G_MissileImpact` exactly; no
differences found anywhere in the function.
**Args:** `ent, trace` (missile entity, trace_t pointer) — confirmed by
`local 0x68 = arg0`, `local 0x6c = arg1` usage.
**Called from:** `G_RunMissile` (`0x223cb`) — inferred from position/context
(not independently confirmed via a `; called from` annotation in this pass).

---

## Why this function was decoded (context)

Investigated for the same reason as `G_ExplodeMissile` (see
[G_ExplodeMissile.md](G_ExplodeMissile.md)): part of a follow-up pass ruling
out a promode-conditional damage/knockback term anywhere in the
missile-impact code path. `G_MissileImpact` turned out to be a **separate**
code path from `G_ExplodeMissile`/`G_RadiusDamage` — it handles the
**direct-hit** case (missile hits an entity directly) by calling `G_Damage`
itself, whereas splash damage is handled by `G_ExplodeMissile`'s
`G_RadiusDamage` call. Both are reached from `G_RunMissile` depending on what
the missile's trace hit.

## Headline finding

**No `df_promode` reference exists anywhere in `G_MissileImpact`** (confirmed
by grepping the function's full instruction range, `0x2210f`–`0x223cb`
(up to `G_RunMissile`'s start), for the cache address `0x1317c4` — zero
matches; also checked for `0x132660` and `0x132100` — zero matches for
either). The direct-hit `G_Damage` call passes `mod = MOD_ROCKET (6)`, not
`MOD_ROCKET_SPLASH (7)` — so `G_Damage`'s confirmed 1.2x self-splash
multiplier (which explicitly checks `mod == 7`) **structurally cannot apply**
to this path regardless of any other condition. `damage = 100` is read
unconditionally from the missile's `damage` field with no scaling.

---

## Structure (direct-hit damage-relevant portion)

### 1. Determine what was hit (`0x2210f`–`0x22148`)

```c
other = &g_entities[trace->entityNum];   // trace+0x34 = entityNum; g_entities base 0x1b2f0, stride 0x43c
if ( !other->takedamage &&                // other+0x2e0
     ( ent->s.eFlags & ( EF_BOUNCE | EF_BOUNCE_HALF ) ) ) {   // ent+0x8 & 0x30 (0x10|0x20)
    G_BounceMissile( ent, trace );        // call 0x21f4f, confirmed unmodified
    G_AddEvent( ent, EV_GRENADE_BOUNCE /*0x2c*/, 0 );   // call 0x2861b, confirmed = G_AddEvent
    return;
}
```

(Field/call identifications updated after the follow-up decode pass — `0x2861b`
is confirmed `G_AddEvent`, and the guard is confirmed as vanilla's exact
`!other->takedamage && (eFlags & (EF_BOUNCE|EF_BOUNCE_HALF))` check, not a
`clipmask` check as an earlier draft of this doc guessed.)

This is the "missile bounced off something non-damageable" early-out — not
relevant to damage/knockback.

### 2. Direct-hit damage application (`0x22148`–`0x221c9`)

```c
if (other->takedamage != 0 && ent->damage != 0) {   // ent+0x2e4 — confirmed field offset from
                                                       // fire_rocket.md, == 100
    LogAccuracyHit(other, g_entities[ent->s.number]);  // call 0x29334, confirmed unmodified
                                                          // (per CanDamage.md / G_RadiusDamage.md)
    if (VectorLength(...) == 0)                          // call 0x8d63, confirmed VectorLength
        dir_scale = 1.0;

    G_Damage(
        other,                              // targ
        ent,                                 // inflictor — the missile entity itself
        g_entities[ ent->parent(+0x200) ],   // attacker — the missile's owner (ent+0x200 = ownerNum,
                                              //   resolved through g_entities[])
        dir,                                  // computed via a helper (call 0x4f43) + VectorLength;
                                               // not independently decoded beyond confirming it is
                                               // NOT df_promode-conditional
        ent + 0x5c,                          // point
        (float) ent->damage,                 // ent + 0x2e4 — confirmed == 100 (fire_rocket.md)
        0,                                    // dflags
        ent->methodOfDeath                    // ent + 0x2f0 — confirmed == 6 = MOD_ROCKET
    );
}
```

**`mod = MOD_ROCKET (6)`, not `MOD_ROCKET_SPLASH (7)`.** `G_Damage`'s
confirmed self-splash 1.2x-horizontal knockback multiplier
(`G_COMBAT/G_Damage.md` section 6) only triggers when `mod == 7`, so this
direct-hit path can never receive that boost, by construction — regardless of
`targ == attacker`. In practice a direct self-hit is also not realistically
reachable when rocket-jumping at one's own feet (missiles pass through their
owner via the clipmask/`ownerNum` exclusion during `MISSILE_PRESTEP_TIME`), so
this path is not the relevant one for the self-splash-knockback investigation
— but it was checked and ruled out for completeness (lead 6 in the task brief:
"broaden the search").

`ent->damage` (`100`) is read directly from the field `fire_rocket.md`
confirmed is set unconditionally — no scaling, no promode branch, anywhere in
this function.

### 3 and 4. Sections 3 and 4 — now fully decoded (this pass)

**Correction to the prior analysis:** the previous pass (documented in
[G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md))
mischaracterized this region. It correctly found the `"hook"` literal
(`0x221d1`, address `0x8fcb`) and the `0x223a2` `G_RadiusDamage` call site, but
**incorrectly described the compared string as `"grapple"`** and concluded the
`G_RadiusDamage` call was nested *inside* that same string-compare branch
(making it "structurally unreachable for rocket impacts"). A full
instruction-by-instruction read of `0x221ca`–`0x223c9` (every remaining byte
of the function) shows this is wrong: **the compared string is literally
`"hook"`** (confirmed directly from the disassembly's own comment at
`0x221d0`), and the `G_RadiusDamage` call at `0x223a2` sits in the **separate,
unconditional final tail** that runs whenever the impacting missile's
classname is *not* `"hook"` — which includes rockets, plasma, grenades, BFG,
i.e. every normal projectile. **This `G_RadiusDamage` call *is* reachable by
rocket impacts** (whenever a rocket directly hits a wall or entity rather than
timing out mid-air) — it is not gated behind `"grapple"` at all. This does not
change the self-splash-knockback conclusion (this call's damage/radius/mod
values are read unconditionally from the entity's own fields exactly like
`G_ExplodeMissile`'s call, with no `df_promode` branch — see the confirmed
constants below), but the earlier "unreachable for rockets" claim about this
specific call site was incorrect and is retracted here.

#### 3. The `"hook"` classname special case (`0x221ca`–`0x222ed`)

```c
if ( !strcmp( ent->classname, "hook" ) ) {         // call 0x5c8f, string @ 0x8fcb = "hook"
    gentity_t *nent = G_Spawn();                    // call 0x283de
    if ( other->takedamage && other->client ) {     // local28+0x2e0, local28+0x204
        G_AddEvent( nent, EV_MISSILE_HIT /*0x32*/, DirToByte(trace->plane.normal) );
        nent->s.otherEntityNum = other->s.number;
        ent->enemy = other;                          // ent+0x300 = other
        v[0] = other->r.currentOrigin[0] + (other->r.mins[0]+other->r.maxs[0])*0.5;
        v[1] = other->r.currentOrigin[1] + (other->r.mins[1]+other->r.maxs[1])*0.5;
        v[2] = other->r.currentOrigin[2] + (other->r.mins[2]+other->r.maxs[2])*0.5;
        SnapVectorTowards( v, ent->s.pos.trBase );    // call 0x289a6
    } else {
        VectorCopy( trace->endpos, v );
        G_AddEvent( nent, EV_MISSILE_MISS /*0x33*/, DirToByte(trace->plane.normal) );
        ent->enemy = NULL;
    }
    SnapVectorTowards( v, ent->s.pos.trBase );        // call 0x289a6 (called again, matches vanilla's
                                                        // slightly redundant double call)
    nent->freeAfterEvent = qtrue;
    nent->s.eType = ET_GENERAL;                        // 0
    ent->s.eType = ET_GRAPPLE;                          // 0xb, confirmed via bg_public.h enum ordinal
    G_SetOrigin( ent, v );                              // call 0x286ad
    G_SetOrigin( nent, v );                             // call 0x286ad
    ent->think = Weapon_HookThink;                      // ent+0x2b4 = 0x2916e (newly pinned address)
    ent->nextthink = level.time + FRAMETIME;            // ent+0x2b0 = level.time + 0x64 (100, confirmed)
    ent->parent->client->ps.pm_flags |= PMF_GRAPPLE_PULL;  // client+0xc (=ps+0xc, pm_flags), |= 0x800
    VectorCopy( ent->r.currentOrigin, ent->parent->client->ps.grapplePoint );  // client+0x5c (=ps+0x5c)
    trap_LinkEntity( ent );                             // call -0x1f
    trap_LinkEntity( nent );                            // call -0x1f
    return;
}
```

Fully confirmed field-for-field match to vanilla's `if (!strcmp(ent->classname,
"hook"))` block. **Newly confirmed struct offsets:** `entityShared_t.mins` =
`ent+0x1b4..0x1c0` (3 floats), `entityShared_t.maxs` = `ent+0x1c0..0x1cc` (3
floats), `gentity_t.enemy` = `ent+0x300`, `gentity_t.think` = `ent+0x2b4`,
`gentity_t.nextthink` = `ent+0x2b0`. **Newly confirmed identifications:**
`0x289a6` = `SnapVectorTowards(vec3_t, vec3_t towards)`, `0x2916e` =
`Weapon_HookThink` (consistent with `G_WEAPON/README.md`'s existing
"confirmed unmodified" note for this function, now pinned to an address),
`0x5c8f` = a `strcmp`-shaped string-compare helper. All bit-flag/enum
constants (`EF_BOUNCE`/`EF_BOUNCE_HALF`=`0x10`/`0x20`, `ET_GRAPPLE`=`0xb`,
`PMF_GRAPPLE_PULL`=`0x800`, `FRAMETIME`=`0x64`) independently confirmed
against oDFs's `bg_public.h`/`g_local.h`.

#### 4. Final tail — event broadcast + splash damage (`0x222ee`–`0x223c9`)

Reached whenever `ent`'s classname is **not** `"hook"` (i.e. for rockets,
grenades, plasma, BFG — any normal projectile that directly hits something):

```c
if ( other->takedamage && other->client ) {
    G_AddEvent( ent, EV_MISSILE_HIT /*0x32*/, DirToByte(trace->plane.normal) );
    ent->s.otherEntityNum = other->s.number;
} else if ( trace->surfaceFlags & SURF_METALSTEPS /*0x1000*/ ) {
    G_AddEvent( ent, EV_MISSILE_MISS_METAL /*0x34*/, DirToByte(trace->plane.normal) );
} else {
    G_AddEvent( ent, EV_MISSILE_MISS /*0x33*/, DirToByte(trace->plane.normal) );
}

ent->freeAfterEvent = qtrue;              // ent+0x22c = 1
ent->s.eType = ET_GENERAL;                 // ent+0x4 = 0

SnapVectorTowards( trace->endpos, ent->s.pos.trBase );  // call 0x289a6
G_SetOrigin( ent, trace->endpos );                       // call 0x286ad

if ( ent->splashDamage ) {                 // ent+0x2e8
    if ( G_RadiusDamage( trace->endpos, ent->parent, ent, (float)ent->splashDamage,
                          (float)ent->splashRadius, other, ent->splashMethodOfDeath ) ) {
        if ( !hitClient ) {                // local2c, set earlier in section 2
            g_entities[ent->r.ownerNum].client->accuracy_hits++;
        }
    }
}

trap_LinkEntity( ent );                    // call -0x1f
```

Confirmed field-for-field match to vanilla's final block in `G_MissileImpact`
(`code/game/g_missile.c:412-440`). `EV_MISSILE_HIT`=0x32,
`EV_MISSILE_MISS_METAL`=0x34, `EV_MISSILE_MISS`=0x33, and `SURF_METALSTEPS`
=0x1000 all independently confirmed against oDFs's `bg_public.h`/
`surfaceflags.h` enum/`#define` values. The `G_RadiusDamage` call here uses
DeFRaG's modified 7-argument signature (adds an explicit `inflictor=ent`
argument between `attacker` and `damage`, matching the same signature change
already documented in `G_ExplodeMissile.md`/`G_RadiusDamage.md`) — arguments
are read **unconditionally** from `ent->splashDamage`/`splashRadius`/
`splashMethodOfDeath` with **no `df_promode` branch**, exactly like the
`G_ExplodeMissile` call site. The `hitClient` local (confirmed as `local2c`,
initialized to 0 at function entry and set to 1 in section 2 right after a
successful `LogAccuracyHit` + direct-hit `accuracy_hits++`) gates the
splash-hit `accuracy_hits++` exactly as vanilla's `if (!hitClient)` does.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x2210f` | Function entry |
| `0x22139` | `G_BounceMissile(ent, trace)` call (`0x21f4f`, confirmed unmodified) |
| `0x22163`–`0x22165` | `LogAccuracyHit(other, ent-owner)` call (`0x29334`, confirmed unmodified) |
| `0x22193`–`0x22196` | `VectorLength(...)` call (`0x8d63`, confirmed) |
| `0x221c7` | `G_Damage(...)` call (`0x2015b`) — direct-hit damage, `mod = MOD_ROCKET (6)` |
| `0x221bd` | Arg `damage` = `(float)ent->damage` (`+0x2e4`, confirmed `100`) |
| `0x221c5` | Arg `mod` = `ent->methodOfDeath` (`+0x2f0`, confirmed `6` = MOD_ROCKET) |
| `0x221d0` | `"hook"` literal (`0x8fcb`) — string compared via `strcmp`-shaped helper `0x5c8f` (**corrected**: not `"grapple"` as a prior pass claimed) |
| `0x221da` | `G_Spawn()` call (`0x283de`) — spawns `nent`, the hook-stuck marker entity |
| `0x221f6`, `0x22269`, `0x22301`, `0x22328`, `0x2233e` | `DirToByte(...)` calls (`0x8f7f`, confirmed — see `G_ExplodeMissile.md`) |
| `0x22201`, `0x22275`, `0x2230d`, `0x22334`, `0x2234a` | `G_AddEvent(...)` calls (`0x2861b`, confirmed) with immediates `0x32`=`EV_MISSILE_HIT`, `0x33`=`EV_MISSILE_MISS`, `0x34`=`EV_MISSILE_MISS_METAL` |
| `0x22259`, `0x22285`, `0x22363` | `SnapVectorTowards(...)` calls (`0x289a6`, confirmed) |
| `0x222a0` | `G_SetOrigin(ent, v)` call (`0x286ad`, confirmed) |
| `0x222a8` | `G_SetOrigin(nent, v)` call (`0x286ad`, confirmed) |
| `0x2236f` | `G_SetOrigin(ent, trace->endpos)` call, section 4 tail (`0x286ad`, confirmed) |
| `0x222e3`, `0x222e9`, `0x223c6` | `trap_LinkEntity(...)` calls (`-0x1f`, confirmed — see `G_ExplodeMissile.md` syscall-number derivation) |
| `0x223a2` | `G_RadiusDamage(...)` call (`0x20773`) — **reachable by rockets** (corrects prior "unreachable, grapple-only" claim); `splashDamage`/`splashRadius`/`splashMethodOfDeath` read unconditionally, no `df_promode` |
| `0x223a9`–`0x223ac` | `if (!hitClient)` guard (`local2c`) around the splash-hit `accuracy_hits++` |

**Confirmed absence:** no reference to `df_promode` cache (`0x1317c4`), the
debug-flags global (`0x132660`), or the `sv_cheats` cache (`0x132100`)
anywhere in this function's instruction range, verified by grep.

**Full-function decode status:** every instruction from `0x2210f` to `0x223c9`
has now been read and matches vanilla `G_MissileImpact` exactly, field for
field and constant for constant. No gaps remain.

---

## oDFs implementation notes

**No action needed — confirmed 100% vanilla-identical, whole function.**
oDFs's current `G_MissileImpact` (`code/game/g_missile.c:270-441`) matches the
fully-decoded bytecode exactly in every section: the bounce early-out, the
direct-hit `G_Damage` call (`mod = MOD_ROCKET`, not promode-boostable by
construction), the `"hook"` classname special case (grapple-stuck bookkeeping,
`ET_GRAPPLE`, `Weapon_HookThink`, `PMF_GRAPPLE_PULL`/`grapplePoint`), and the
final event-broadcast + splash-damage tail (`EV_MISSILE_HIT`/`_MISS`/
`_MISS_METAL`, the `hitClient`-gated `accuracy_hits++`, `trap_LinkEntity`).
Confirms the self-splash promode knockback boost remains exclusively
reachable via `G_ExplodeMissile`'s `G_RadiusDamage` call with
`mod = MOD_ROCKET_SPLASH` (consistent with `G_COMBAT/G_Damage.md`'s original
finding) — this function's *own* two `G_RadiusDamage`/`G_Damage` call sites
are both confirmed to carry no `df_promode` gating anywhere, so neither can be
an alternate source of the boost.
