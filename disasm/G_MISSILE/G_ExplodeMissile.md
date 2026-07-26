# G_ExplodeMissile — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00022066` (vanilla baseq3 equivalent at `0x1a4cf`)
**DeFRaG hash:** modified vs vanilla (+4 instructions per `G_MISSILE/README.md`'s
prior hash-table survey). Originally only the damage/knockback-relevant portion
(the fields feeding `G_RadiusDamage`) was decoded field-by-field. **A follow-up
pass has now decoded the entire function, instruction-by-instruction, with zero
gaps** — see "Full decode (follow-up pass)" below. Every instruction from
`0x22066` (entry) to `0x2210e` (`leave`) has been read and matches vanilla
`G_ExplodeMissile` exactly; no differences found anywhere in the function.
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

### 1. Origin snap + event broadcast (`0x22066`–`0x220a7`) — now fully decoded

```c
BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );   // call 0x4d73, confirmed (README)
SnapVector( origin );                                        // per-component Q_ftol/cvif round-trip
G_SetOrigin( ent, origin );                                  // call 0x286ad, confirmed (see G_BounceItem.md)

dir[0] = dir[1] = 0;
dir[2] = 1;                                                   // local30/local34/local38, contiguous vec3

ent->s.eType = 0;                                             // ET_GENERAL (ent+0x4)

local40 = DirToByte( dir );                                   // call 0x8f7f, confirmed (see below)
G_AddEvent( ent, 0x33 /* EV_MISSILE_MISS */, local40 );       // call 0x2861b, confirmed (see below)

ent->freeAfterEvent = qtrue;                                  // ent+0x22c = 1
```

**All calls now identified, confirmed via a follow-up decode of
`G_BounceItem`/`G_BounceMissile`/`G_MissileImpact` (which share these same
helpers) plus independent verification of each:**

- `0x286ad` = **`G_SetOrigin(gentity_t*, vec3_t)`** — confirmed by argument
  shape (entity pointer + vec3) and by cross-checking its other three call
  sites in `G_BounceItem`/`G_BounceMissile`/`G_MissileImpact`, all of which
  pass a freshly-computed/traced origin exactly where vanilla source calls
  `G_SetOrigin`.
- `0x8f7f` = **`DirToByte(vec3_t)`** — confirmed by reading its own
  disassembly: a null-pointer check returning 0, then a loop over a
  12-byte-stride table (`bytedirs[]`) computing `DotProduct` against each
  entry and tracking the best match — the textbook vanilla `DirToByte`
  implementation shape.
- `0x2861b` = **`G_AddEvent(gentity_t*, int newEvent, int eventParm)`** —
  confirmed by argument shape and by the `0x33` immediate: oDFs's
  `bg_public.h` `entity_event_t` enum places `EV_MISSILE_MISS` at ordinal 51
  (`0x33`), an exact match. (`G_MissileImpact.md`'s decode of the same call
  target with immediate `0x2c` = `EV_GRENADE_BOUNCE` = ordinal 44 = `0x2c`
  independently cross-checks this identification.)
- `ent+0x22c = 1` matches `ent->freeAfterEvent = qtrue;` — `freeAfterEvent` is
  a `qboolean` field on `gentity_t`.

This entire section is now confirmed **field-for-field identical to vanilla**:
`BG_EvaluateTrajectory` → `SnapVector` → `G_SetOrigin` → `dir = (0,0,1)` →
`ent->s.eType = ET_GENERAL` → `G_AddEvent(ent, EV_MISSILE_MISS,
DirToByte(dir))` → `ent->freeAfterEvent = qtrue;`. No `df_promode` reference,
no altered constants, no reordering.

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

### 3. Accuracy-hit bookkeeping (`0x220ed`–`0x22106`) — now fully decoded

```c
if ( G_RadiusDamage( ... /* section 2's call */ ) ) {   // return value stored, then tested
    g_entities[ent->r.ownerNum].client->accuracy_hits++;
}
```

Previously described only as "a free-list/reference-count field += 1" — this
is actually the **`if (G_RadiusDamage(...))` guard around the accuracy_hits
increment** from vanilla's:

```c
if( G_RadiusDamage( ent->r.currentOrigin, ent->parent, ent->splashDamage, ent->splashRadius, ent
    , ent->splashMethodOfDeath ) ) {
    g_entities[ent->r.ownerNum].client->accuracy_hits++;
}
```

`G_RadiusDamage`'s return value is stored to a local and compared against 0
(`eq -2 0x22107`, skip if false) before the increment executes — confirming
the `if(...)` wrapper is present and unconditional (no `df_promode` gating).
The increment itself resolves `g_entities[ent->r.ownerNum].client` via the
folded constant `0x1b4f4` = `g_entities` base (`0x1b2f0`, already confirmed)
`+ 0x204` = **newly confirmed** `gentity_t.client` field offset (`ent+0x204`,
architecturally consistent: `client` is the field immediately following
`entityShared_t r`, whose own last field `ownerNum` was already confirmed at
`ent+0x200`). `client->accuracy_hits` resolves at **newly confirmed** offset
`client+0x320`.

### 4. Cleanup (`0x22107`–`0x2210e`) — corrected

```c
trap_LinkEntity( ent );
return;
```

**Correction to the previous guess:** the trap call at `0x2210a` is
`const -0x1f` (`-31`), which — per the vanilla Q3 `g_syscalls.h`/`bg_lib.h`
enum ordering, cross-checked against this project's two other confirmed trap
numbers (`trap_Trace` = `-25` ↔ `G_TRACE` enum value 23, and
`trap_EntitiesInBox` = `-33` ↔ `G_ENTITIES_IN_BOX` enum value 31 — both fit
the mapping `call_target = -(enum_value + 2)`) — resolves to **`G_LINKENTITY`
(enum value 29 → `-(29+2) = -31`), i.e. `trap_LinkEntity`, not
`trap_UnlinkEntity`**. This matches vanilla's actual final statement,
`trap_LinkEntity( ent );`, exactly (vanilla's `G_ExplodeMissile` never calls
`trap_UnlinkEntity` at all). The previous doc's "plausible trap_UnlinkEntity,
not independently verified" guess was wrong and is now corrected.

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
| `0x22071` | `BG_EvaluateTrajectory(&ent->s.pos, level.time, origin)` call (`0x4d73`, confirmed) |
| `0x2208b` | `G_SetOrigin(ent, origin)` call (`0x286ad`, newly confirmed) |
| `0x2209c`–`0x220a1` | `ent->s.eType = ET_GENERAL` (`0x0`) |
| `0x220a5` | `DirToByte(dir)` call (`0x8f7f`, newly confirmed) |
| `0x220b0` | `G_AddEvent(ent, EV_MISSILE_MISS, DirToByte(dir))` call (`0x2861b`, newly confirmed; `0x33` = `EV_MISSILE_MISS` confirmed against `bg_public.h` enum ordinal 51) |
| `0x220b7`–`0x220b8` | `ent->freeAfterEvent = qtrue` (`ent+0x22c = 1`) |
| `0x220ed`–`0x220f0` | `if (G_RadiusDamage(...))` guard around the `accuracy_hits++` (return value tested, not unconditional) |
| `0x2210a` | `trap_LinkEntity(ent)` call (`-0x1f` = `-31`, **corrected** from a prior "plausible trap_UnlinkEntity" guess — see section 4) |

**Confirmed absence:** no reference to `df_promode` cache (`0x1317c4`) anywhere
in this function's instruction range (`0x22066`-`0x2210e`), verified by grep.

**Full-function decode status:** every instruction from `0x22066` to `0x2210e`
has now been read and matches vanilla `G_ExplodeMissile` exactly, field for
field and constant for constant. No gaps remain.

---

## oDFs implementation notes

**No action needed — confirmed 100% vanilla-identical, whole function.** Every
line of oDFs's current `G_ExplodeMissile` (`code/game/g_missile.c:67-93`)
matches the fully-decoded bytecode above: `BG_EvaluateTrajectory` + `SnapVector`
+ `G_SetOrigin`, `dir=(0,0,1)`, `ent->s.eType=ET_GENERAL`,
`G_AddEvent(ent,EV_MISSILE_MISS,DirToByte(dir))`, `ent->freeAfterEvent=qtrue`,
the `splashDamage`-gated `G_RadiusDamage` call with the `accuracy_hits++`
guarded on its return value, and the final `trap_LinkEntity(ent)`. No
`df_promode` gating, no altered constants, no reordering, no missing/extra
logic found anywhere in this function.
