# LogAccuracyHit — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

**DeFRaG address:** `0x29334` (vanilla `0x2087f`, hash `-7d91da6` **both sides** —
exact hash match).
**Size:** `0x50` (80) instructions vs. instruction range `0x29334`–`0x29382`
(0x4f = 79, the usual "+1" size-field/instruction-count drift seen throughout
this module on confirmed-unmodified functions).

This function had never been read at all prior to this pass (previously only
hash-confirmed). Read in full, top to bottom.

## Full decode

```c
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker ) {   // arg0=target (local 0x1c), arg1=attacker (local 0x20)
    if ( !target-><+0x2e0> )               return qfalse;   // 0x29337-0x2933f: takedamage check
    if ( target != attacker )              /* continue */   // 0x29340-0x29348: raw pointer compare
    else                                    return qfalse;
    if ( !target-><+0x204> )               return qfalse;   // 0x29349-0x29353: target->client
    if ( !attacker-><+0x204> )             return qfalse;   // 0x29354-0x2935e: attacker->client
    if ( target-><+0x204>-><+0xb8> <= 0 )  return qfalse;   // 0x2935f-0x2936c: target->client->ps.stats[STAT_HEALTH] <= 0
    if ( OnSameTeam( target, attacker ) )  return qfalse;   // 0x2936d-0x2937e: call 0x26562
    return qtrue;                                            // 0x2937f-0x29380
}
```

Instruction-level notes:

- `0x29337`: `*(target + 0x2e0)` compared `!= 0` via `ne -2 0x29340` (jumps to
  the *next* check when true/nonzero); fallthrough (zero) returns `qfalse` —
  matches `if( !target->takedamage ) return qfalse;`. Confirms `gentity_t`
  offset `+0x2e0` = `takedamage` (same field already used in `CheckGauntletAttack`
  and `ShotgunPellet`, see below).
- `0x29340`: raw pointer equality of the two arguments (`target == attacker`,
  no dereference) via `ne -2 0x29349`; fallthrough (equal) returns `qfalse` —
  matches `if ( target == attacker ) return qfalse;`.
- `0x29349`/`0x29354`: `+0x204` on `target`/`attacker` compared `!=0`,
  matching the already-confirmed `gentity_t.client` pointer offset — matches
  `if ( !target->client ) return qfalse;` / `if ( !attacker->client )
  return qfalse;`.
- `0x2935f`: `*(*(target+0x204) + 0xb8)` (i.e. `target->client->ps.stats[0]`,
  `+0xb8` = `STAT_HEALTH` per `disasm/README.md`'s confirmed `playerState_t`
  offset table) compared via `gti -2 0x2936d` — jumps to the next check when
  `health > 0`; fallthrough (`health <= 0`) returns `qfalse` — matches
  `if( target->client->ps.stats[STAT_HEALTH] <= 0 ) return qfalse;`
  (branch direction double-checked per project convention).
- `0x2936d`–`0x2937e`: calls `0x26562(target, attacker)`, result compared to
  `0`; `eq -2 0x2937f` jumps to the `return qtrue` path when the call result
  is `0` (i.e. `OnSameTeam` returned false); fallthrough (result nonzero,
  `OnSameTeam` true) returns `qfalse`. This is the exact `if (
  OnSameTeam(target,attacker) ) return qfalse; return qtrue;` idiom — `0x26562`
  is thus `OnSameTeam` (not independently named elsewhere in this project's
  docs prior to this pass, but the call shape — two-arg boolean predicate used
  exactly like `OnSameTeam` in this exact position — is conclusive).

## Result: confirmed vanilla-identical

Every one of the 5 vanilla `LogAccuracyHit` checks (`takedamage`,
self-hit, `target->client`, `attacker->client`,
`ps.stats[STAT_HEALTH] <= 0`, `OnSameTeam`) is present, in the same order,
with the same short-circuit semantics as `code/game/g_weapon.c`'s
`LogAccuracyHit`. No hidden branch, no additional field access, no changed
constant (there are no float/magic-number constants in this function at all,
consistent with the module README's low-risk assessment). **Confirmed
unmodified.**

**Confirmed callers** (from the `; called from` annotation immediately
preceding this function's `enter` at `0x29334`, i.e. real static call-target
data, not inferred): `0x20773` (`G_RadiusDamage`), `0x2210f`
(`G_MissileImpact`), `0x289ef` (`Bullet_Fire`), `0x28bd0` (`ShotgunPellet`),
`0x28ebf` (`weapon_railgun_fire`), `0x291f4` (Lightning Gun fire function).
The `G_RadiusDamage`/`G_MissileImpact` calls are already independently
documented as confirmed-unmodified-vanilla-behavior in
[../G_COMBAT/G_RadiusDamage.md](../G_COMBAT/G_RadiusDamage.md) and
[../G_MISSILE/G_MissileImpact.md](../G_MISSILE/G_MissileImpact.md) (splash
and direct missile hits both track accuracy in vanilla Q3 too — not a
DeFRaG-specific addition). Both of those sites call `LogAccuracyHit` **before**
`G_Damage`, matching vanilla order — see the note on `ShotgunPellet` below,
which is the **only** call site found (across all 6) that reverses this order.

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x29334` | `LogAccuracyHit` entry |
| `0x29337` | `takedamage` check (`gentity_t+0x2e0`) |
| `0x29340` | `target == attacker` self-hit check |
| `0x29349` | `target->client` null check |
| `0x29354` | `attacker->client` null check |
| `0x2935f` | `target->client->ps.stats[STAT_HEALTH] <= 0` check (`+0xb8`) |
| `0x2936d` | `OnSameTeam(target, attacker)` call (`0x26562`) |
| `0x29381` | Common return trampoline (all early-return paths jump here) |

## oDFs implementation notes

No change needed — confirmed vanilla-identical. `oDFs`'s
`LogAccuracyHit` (`code/game/g_weapon.c`) already matches this exactly.
