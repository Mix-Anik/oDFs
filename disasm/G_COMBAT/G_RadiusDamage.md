# G_RadiusDamage — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00020773` (vanilla baseq3 equivalent at `0x18ffc`, hash `-2e4777e7`)
**DeFRaG hash:** `73403704` — **MODIFIED** (does not match vanilla)
**Size:** `0x17f` (383) instructions in DeFRaG vs `0x17d` (381) in vanilla —
**+2 instructions only** (a small, targeted change, not a rewrite)
**Called from:** `G_ExplodeMissile` (`0x22066`), specifically the call at
`0x220e9`-`0x220eb` — **corrected this pass**. Previously attributed to
`G_MissileImpact` via order-position inference only; a follow-up pass
independently decoded `G_ExplodeMissile` and confirmed the actual call site
directly (see [G_MISSILE/G_ExplodeMissile.md](../G_MISSILE/G_ExplodeMissile.md)).
`G_MissileImpact` separately calls `G_Damage` directly (not via
`G_RadiusDamage`) for the direct-hit case, with `mod = MOD_ROCKET`.

---

## Headline finding

The core radius-damage **algorithm is unchanged from vanilla**: bounding-box
distance calculation, `dir[2] += 24` bias, and `points = damage * (1 -
dist/radius)` truncated to int are all confirmed byte-for-byte identical in
logic to vanilla Q3. The only structural difference found is a **signature
change**: DeFRaG's `G_RadiusDamage` takes attacker and inflictor as **two
separate parameters** (vanilla passes the same `attacker` entity for both when
calling `G_Damage`). This is plumbing, not a physics change.

---

## Structure

### 1. Radius floor + bbox setup (`0x20773`–`0x207bb`)

```
if (radius < 1.0): radius = 1.0

for (i = 0; i < 3; i++):
    mins[i] = origin[i] - radius
    maxs[i] = origin[i] + radius

entCount = trap_EntitiesInBox(mins, maxs, entList, 1024)   ; call -0x21 = -33 = trap_EntitiesInBox (confirmed)
```

Confirmed use of `trap_EntitiesInBox` (call target `-33`), matching the
fingerprint noted in the task brief. Matches vanilla's `EntitiesInBox` usage
pattern exactly (radius-based mins/maxs box, `MAX_GENTITIES`-class limit).

### 2. Per-entity loop (`0x207c8`–`0x208e1`)

```
for (e = 0; e < entCount; e++):
    ent = &g_entities[entList[e]]        ; base 0x1b2f0, stride 0x43c — confirmed matches G_ACTIVE README

    if (ent == ignore): continue
    if (ent->takedamage(+0x2e0) == 0): continue

    // closest point on ent's absmin/absmax bbox to `origin`, per axis:
    for (axis = 0; axis < 3; axis++):
        if (origin[axis] < ent->absmin[axis](+0x1d0)):
            v[axis] = ent->absmin[axis] - origin[axis]
        elif (origin[axis] > ent->absmax[axis](+0x1dc)):
            v[axis] = origin[axis] - ent->absmax[axis]
        else:
            v[axis] = 0

    dist = VectorLength(v)                ; call 0x8d63, confirmed VectorLength

    if (dist >= radius): continue

    points = damage * (1.0 - dist / radius)     ; float, not yet truncated

    if (!CanDamage(ent, origin)): continue        ; call 0x20648, confirmed unmodified CanDamage

    if (LogAccuracyHit(ent, attacker)): hitClient = true   ; call 0x29334, confirmed unmodified LogAccuracyHit

    dir[0] = ent->currentOrigin[0](+0x1e8) - origin[0]
    dir[1] = ent->currentOrigin[1](+0x1ec) - origin[1]
    dir[2] = ent->currentOrigin[2](+0x1f0) - origin[2]
    dir[2] += 24.0                         ; confirmed literal 0x41c00000 = 24.0

    G_Damage(ent, inflictor, attacker, dir, origin, (int)points, DAMAGE_RADIUS(1), mod)
```

**This is a full, confirmed match to vanilla's radius-damage algorithm**:
- Distance measured to `absmin`/`absmax` bounding box, not origin. **Confirmed.**
- `dir[2] += 24` bias. **Confirmed** (literal float constant `0x41c00000` = `24.0`).
- `points = damage * (1 - dist/radius)`, truncated to int (`cvfi`/`Q_ftol`) before
  being passed to `G_Damage`. **Confirmed** — the truncation happens at the
  `G_Damage` call site (`Arg 0x1c = Q_ftol(points)`), not earlier.
- `dflags = DAMAGE_RADIUS (1)` passed unconditionally. **Confirmed.**

### 3. Signature difference from vanilla

Vanilla: `qboolean G_RadiusDamage( vec3_t origin, gentity_t *attacker, float
damage, float radius, gentity_t *ignore, int mod )` — 6 params, and calls
`G_Damage(ent, attacker, attacker, dir, NULL, points, DAMAGE_RADIUS, mod)`
(same entity for both `inflictor` and `attacker`, and `point = NULL`).

DeFRaG: takes **7 params** — an explicit `attacker` (arg1) *and* a separate
`inflictor` (arg2), confirmed distinct values passed into the `G_Damage` call's
`attacker` and `inflictor` slots respectively. Also passes the radius-damage
**`origin`** pointer as `point` (not `NULL`) into `G_Damage`.

Exact DeFRaG parameter order (by local-arg offset): `arg0=origin, arg1=attacker,
arg2=inflictor, arg3=damage, arg4=radius, arg5=ignore, arg6=mod`.

This is very likely plumbing to let the *missile entity* be recorded as
`inflictor` for death-message/inflictor-tracking purposes while `attacker`
stays the player — it does not touch the distance/points/dir-bias formula
verified above. **Confirmed** this pass by decoding the actual call site in
`G_ExplodeMissile`: `attacker` = the missile's owner entity (`ent->s.ownerNum`
resolved to a `g_entities[]` pointer), `inflictor` = the missile entity itself.

### 4. Radius/damage arguments — confirmed sourced unconditionally from the missile entity (this pass)

A specific hypothesis was checked and ruled out: that the `radius` value fed
into the `points = damage * (1 - dist/radius)` falloff might differ from the
missile's `splashRadius` field under `df_promode` (i.e. a promode-specific
*effective* radius, separate from the vanilla-formula check already confirmed
above). The actual call site in `G_ExplodeMissile` (`0x220e9`) was decoded
argument-by-argument:

```
G_RadiusDamage(
    origin   = missile->r.currentOrigin,        ; ent+0x1e8
    attacker = g_entities[missile->r.ownerNum],  ; ent+0x258 -> ownerNum -> g_entities[]
    inflictor= missile (self),
    damage   = (float)missile->splashDamage,     ; ent+0x2e8, confirmed == 100 (fire_rocket)
    radius   = (float)missile->splashRadius,     ; ent+0x2ec, confirmed == 120 (fire_rocket)
    ignore   = missile (self),
    mod      = missile->splashMethodOfDeath       ; ent+0x2f4, confirmed == 7 (MOD_ROCKET_SPLASH)
)
```

Both `damage` and `radius` are read **directly** from the missile entity's
`splashDamage`/`splashRadius` fields, with **no intervening computation,
branch, or `df_promode` check** between the field read and the argument push
(confirmed by grepping the full `G_ExplodeMissile` instruction range for the
`df_promode` cache address `0x1317c4` — zero matches). Since `fire_rocket`
already confirmed these fields are set unconditionally to `100`/`120`
regardless of promode, this closes the loop: **the radius-falloff formula's
inputs are vanilla-identical in every case, promode included.**

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x20773` | Function entry |
| `0x207bc` | `trap_EntitiesInBox` call (`-0x21` = -33) |
| `0x2085f` | `VectorLength` call (`0x8d63`) |
| `0x20878` | `points = damage * (1.0 - dist/radius)` |
| `0x20880` | `CanDamage(ent, origin)` call (`0x20648`, confirmed unmodified) |
| `0x2088e` | `LogAccuracyHit(ent, attacker)` call (`0x29334`, confirmed unmodified) |
| `0x208c4` | `dir[2] += 24.0` (literal `0x41c00000`) |
| `0x208df` | `G_Damage(...)` call (`0x2015b`, confirmed **modified** — see [G_Damage.md](G_Damage.md)) |
| `ent + 0x1d0` | `ent->r.absmin[0]` (per-axis, +4 per component) |
| `ent + 0x1dc` | `ent->r.absmax[0]` (per-axis, +4 per component) |
| `ent + 0x1e8` | `ent->r.currentOrigin[0]` (per-axis, +4 per component) |
| `ent + 0x2e0` | `ent->takedamage` |

---

## oDFs implementation notes

**Full match on the physics-relevant algorithm — no change needed.** oDFs's
`G_RadiusDamage` (vanilla-derived, in `code/game/g_combat.c` presumably) already
implements: bbox-distance, `dir[2]+=24`, and `points = damage*(1-dist/radius)`
truncated to int. These were not modified.

The signature/parameter-plumbing difference (separate `inflictor` argument,
non-NULL `point`) was **not** ported — oDFs's `G_RadiusDamage` call sites
(missile impact code) already pass semantically appropriate `inflictor` values
where needed via its own vanilla-style call convention, and this difference has
no confirmed effect on damage/knockback magnitude. Not changed, per CRITICAL
RULE (no confirmed physics impact identified).
