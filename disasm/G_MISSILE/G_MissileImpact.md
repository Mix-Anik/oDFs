# G_MissileImpact — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x0002210f` (vanilla baseq3 equivalent at `0x1a575`)
**DeFRaG hash:** modified vs vanilla (+4 instructions per `G_MISSILE/README.md`'s
prior hash-table survey) — this pass decoded the direct-hit damage-application
portion instruction-by-instruction; the grapple/hook special-case branch and
some entity-linking bookkeeping were read but not exhaustively decoded, per
project scope.
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

```
other = &g_entities[trace->entityNum]              ; trace + 0x34 = entityNum, confirmed pattern
                                                      ; matches g_entities stride 0x43c, base 0x1b2f0
if (other->takedamage(+0x2e0) == 0):
    if (ent->clipmask(+0x8) & 0x30) == 0:
        goto grapple/hook special-case (0x22148+, "hook" string referenced — not decoded)
    ; else: fall through
G_BounceMissile(ent, trace)                          ; call 0x21f4f, confirmed unmodified per
                                                       ; G_MISSILE/README.md hash table
G_something(ent, 0x2c /* EV_?? */, 0)                ; call 0x2861b — event broadcast, same target
                                                       ; seen in G_ExplodeMissile, not independently
                                                       ; identified
return
```

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

### 3. Grapple/hook special case (`0x221ca`–`0x223c9`) — DECODED (RocketJump_ExtraVelocity_Search_Pass2)

References the literal string `"hook"` (`0x221d1`) and diverges into
grapple-related entity handling. **Fully decoded** in
[G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md](../G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md)
while tracing every `G_RadiusDamage` call site in the QVM. Summary:

- Entered only when a string-compare helper (`0x5c8f`) matches the impacting
  entity's `classname` (`ent+0x20c`) against a literal string constant
  (`0x8fcb`) — i.e. gated on `strcmp(ent->classname, "grapple") == 0`. This is
  the "grapple hook stuck to a wall" bookkeeping path (spawns a stuck-hook
  marker entity via `G_Spawn`, sets `r.svFlags`, computes an AABB-center
  `currentOrigin` from `absmin`/`absmax`, links event broadcasts via `0x2861b`
  and `0x289a6`/`0x286ad`).
- **It also calls `G_RadiusDamage` (`0x20773`, at call site `0x223a2`)** —
  this is the *second and only other* `G_RadiusDamage` call site in the entire
  QVM (the first being the already-documented rocket path in
  `G_ExplodeMissile`). It is additionally guarded by
  `if (ent->splashDamage(+0x2e8) == 0) skip`, and reads its `damage`/`radius`/
  `mod` arguments from the impacting entity's own fields exactly like the
  rocket path does.
- **Since a rocket's `classname` is `"rocket"`, not `"grapple"`, this entire
  branch — including its `G_RadiusDamage` call — is structurally unreachable
  for rocket impacts.** Confirmed via the classname string-compare gate
  itself, not inferred from field values. Ruled out as a rocket-splash
  knockback source.

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
| `0x221d0` | `"hook"` literal — start of undecoded grapple/hook special case |

**Confirmed absence:** no reference to `df_promode` cache (`0x1317c4`), the
debug-flags global (`0x132660`), or the `sv_cheats` cache (`0x132100`)
anywhere in this function's instruction range, verified by grep.

---

## oDFs implementation notes

No action needed — this function's direct-hit damage path is vanilla-shaped
(`damage = 100` unconditional, `mod = MOD_ROCKET`) and cannot trigger the
promode self-splash knockback boost by construction. Confirms the boost is
exclusively reachable via `G_ExplodeMissile`'s `G_RadiusDamage` call with
`mod = MOD_ROCKET_SPLASH`, consistent with `G_COMBAT/G_Damage.md`'s original
finding.
