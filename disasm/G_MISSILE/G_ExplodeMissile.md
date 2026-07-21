# G_ExplodeMissile — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00022066` (vanilla baseq3 equivalent at `0x1a4cf`)
**DeFRaG hash:** modified vs vanilla (+4 instructions per `G_MISSILE/README.md`'s
prior hash-table survey) — this pass decoded the function's damage/knockback-relevant
portion instruction-by-instruction (full field-by-field decode of the fields that
feed `G_RadiusDamage`); the remainder (event/origin-snap bookkeeping, entity
free-list update) was read but not exhaustively cross-checked against vanilla
line-for-line, per project scope (task only required ruling out a
promode-conditional damage/radius term).
**Args:** `ent` (the missile entity) — single-argument function, confirmed by
`local 0x58 = arg0` usage throughout.
**Called from:** `G_RunMissile` (`0x223cb`) — confirmed via the trailing
`; called from 0x223cb` annotation in the disassembly.

---

## Why this function was decoded (context)

Investigated as part of a follow-up pass chasing an unexplained ~11.7%
knockback-speed gap in DeFRaG's self-splash-rocket-jump formula (see
[G_COMBAT/G_Damage.md](../G_COMBAT/G_Damage.md) re-examination section). The
specific hypothesis: does the `radius` (and/or `damage`) value fed into
`G_RadiusDamage`'s falloff formula differ from the missile's stored
`splashRadius`/`splashDamage` fields under `df_promode`? This required finding
`G_RadiusDamage`'s actual caller and its exact argument values — which turned
out to be `G_ExplodeMissile`, not `G_MissileImpact` as previously assumed by
order-position inference (see correction in
[G_COMBAT/G_RadiusDamage.md](../G_COMBAT/G_RadiusDamage.md)).

## Headline finding

**No `df_promode` reference exists anywhere in `G_ExplodeMissile`** (confirmed
by grepping the function's full instruction range for the cache address
`0x1317c4` — zero matches). The `G_RadiusDamage` call passes `damage` and
`radius` read **directly and unconditionally** from the missile entity's
`splashDamage`/`splashRadius` fields, with no intervening computation. This
rules out a promode-specific radius/damage variant at this call site.

---

## Structure (damage/knockback-relevant portion)

### 1. Origin snap + event broadcast (`0x22066`–`0x220a7`)

```
G_TouchTriggers-or-similar(ent+0xc, level.time, &local24)   ; call 0x4d73, not independently identified
local24..2c = SnapVector-style rounding (Q_ftol/cvif per axis)
G_something(ent, &local24)                                   ; call 0x286ad, not independently identified
                                                               ; (plausibly G_AddEvent / explosion FX trigger)
ent->s.eType (offset +0x4 relative to some pointer) = 0       ; likely resets to ET_GENERAL after explosion
local40 = G_something(&local30)                               ; call 0x8f7f — same target also seen
                                                               ; used elsewhere as a VectorCopy/util-style
                                                               ; helper; not independently identified
G_something(ent, 0x33 /* EV_?? */, local40)                   ; call 0x2861b — event-broadcast helper,
                                                               ; also called elsewhere with an event-id-like
                                                               ; second argument; not independently identified
ent->something(+0x22c) = 1
```

Not fully identified against vanilla source line-for-line; none of these calls
reference `df_promode`, damage, knockback, or radius fields. Out of scope for
further decode per project rule (task only required ruling out a
promode-conditional damage/radius term, which this section does not contain).

### 2. Splash radius damage — the confirmed call site (`0x220b9`–`0x220eb`)

```c
if (ent->splashDamage != 0) {           // ent + 0x2e8, confirmed field offset from fire_rocket.md
    other = ent;                         // local44 = ent
    G_RadiusDamage(
        other->r.currentOrigin,          // ent + 0x1e8
        g_entities[ other->parent /* ent+0x258 */ ],   // "attacker" — the missile's owner
        other,                            // "inflictor" — the missile entity itself
        (float) other->splashDamage,      // ent + 0x2e8 — confirmed == 100 (fire_rocket.md)
        (float) other->splashRadius,      // ent + 0x2ec — confirmed == 120 (fire_rocket.md)
        other,                            // "ignore" — the missile entity itself
        other->splashMethodOfDeath        // ent + 0x2f4 — confirmed == 7 = MOD_ROCKET_SPLASH
    );
}
```

Every one of `splashDamage`, `splashRadius`, and `splashMethodOfDeath` is read
straight from the entity struct at the field offsets `fire_rocket.md` already
confirmed are set unconditionally (not `df_promode`-gated) to `100`, `120`, and
`7` respectively. **No arithmetic, branch, or promode check occurs between
reading these fields and pushing them as `G_RadiusDamage` arguments.** This
directly confirms `G_RadiusDamage`'s inputs for a rocket's splash are
vanilla-identical in every case, including under `df_promode`.

The `if (ent->splashDamage != 0)` guard (`0x220bd`-`0x220be`) is the only
condition on this call — a plain "does this missile type even have splash
damage" check (all 100/120/7 values are non-zero for rockets, so this is
always taken for `fire_rocket`'s bolts).

### 3. Entity-list bookkeeping (`0x220ed`–`0x22106`)

```
g_entities[ent->s.number]-derived free-list/reference-count field += 1
```

Not further decoded (unrelated to damage/knockback; consistent with vanilla's
post-explosion entity accounting).

### 4. Cleanup (`0x22107`–`0x2210e`)

```
trap_UnlinkEntity(ent)     ; call -0x1f = -31, plausible trap_UnlinkEntity based on single-arg
                             ; (entity pointer) signature; not independently verified against
                             ; g_syscalls enum ordering
return
```

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x22066` | Function entry |
| `0x220b9`–`0x220be` | `if (ent->splashDamage != 0)` guard |
| `0x220c8` | Arg0 = `ent->r.currentOrigin` (`+0x1e8`) |
| `0x220ce` | Arg1 = `g_entities[ent->parent->s.number]`-equivalent (attacker = owner), via `ent+0x258` |
| `0x220d1` | Arg2 = `ent` itself (inflictor) |
| `0x220d8` | Arg3 = `(float)ent->splashDamage` (`+0x2e8`, confirmed `100`) |
| `0x220df` | Arg4 = `(float)ent->splashRadius` (`+0x2ec`, confirmed `120`) |
| `0x220e2` | Arg5 = `ent` itself (ignore) |
| `0x220e8` | Arg6 = `ent->splashMethodOfDeath` (`+0x2f4`, confirmed `7` = MOD_ROCKET_SPLASH) |
| `0x220e9` | `G_RadiusDamage(...)` call (`0x20773`, confirmed modified-but-vanilla-algorithm — see [G_COMBAT/G_RadiusDamage.md](../G_COMBAT/G_RadiusDamage.md)) |

**Confirmed absence:** no reference to `df_promode` cache (`0x1317c4`) anywhere
in this function's instruction range (`0x22066`-`0x2210e`), verified by grep.

---

## oDFs implementation notes

No action needed beyond what `G_RadiusDamage.md`/`fire_rocket.md` already
cover — this function's only physics-relevant behavior (which fields feed
`G_RadiusDamage`) is vanilla-identical, sourced from `splashDamage`/
`splashRadius`/`splashMethodOfDeath` with no promode branch. Confirms no
change needed to oDFs's `G_ExplodeMissile`-equivalent code beyond what was
already fixed in `fire_rocket`.
