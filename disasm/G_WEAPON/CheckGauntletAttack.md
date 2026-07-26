# Weapon_Gauntlet / CheckGauntletAttack — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

## Weapon_Gauntlet — confirmed unmodified (trivial)

**DeFRaG address:** `0x288ae` (vanilla `0x1ff04`, hash `-64bac70e` both sides).
**Hash + size both exactly identical** (`0x4` instructions both sides) — this
is vanilla's empty `void Weapon_Gauntlet( gentity_t *ent ) { }` body (all
actual gauntlet melee damage happens in `CheckGauntletAttack`, called
separately — see below). No further analysis needed; confirmed byte-identical.

## Cross-check against `disasm/BG_PMOVE/README.md`'s "Gauntlet check resolved"

That note concerns a **different** thing: client-side prediction in
`bg_pmove.c`'s `PM_Weapon`, which reads `pm+0x2c` (a predicted "did the
gauntlet swing land" flag used only for a hit-sound-vs-swing-sound choice) and
found that field is always 0 because nothing in `bg_pmove.c`/`Pmove` ever
calls a `CheckGauntletAttack`-equivalent to populate it. **This is unrelated
to the function documented below.** The `CheckGauntletAttack` analyzed here is
the real **server-side damage-dealing function** in `g_weapon.c`, invoked from
`FireWeapon`'s `WP_GAUNTLET` case (indirectly, via `Weapon_Gauntlet` — though
note `Weapon_Gauntlet` itself is empty in both vanilla and DeFRaG, so
`CheckGauntletAttack` in vanilla Q3 is actually **not called from
`FireWeapon` at all** — it's invoked separately, once per server frame while
the gauntlet is the active weapon and the attack button is held, typically
from `ClientThink`-adjacent per-frame logic, not from the `EV_FIRE_WEAPON`
path). This pass did not re-locate `CheckGauntletAttack`'s caller in the
DeFRaG QVM (out of scope — the task only required decoding the function
itself); the two findings (BG_PMOVE's client-predicted swing flag vs this
function's real server damage) remain independently true and do not
contradict each other.

## CheckGauntletAttack — MODIFIED

**DeFRaG address:** `0x288b1` (vanilla `0x1ff07`, hash `-759640f5`).
**DeFRaG hash:** `185ba7ee` — differs from vanilla.
**Size:** `0xf6` (246) instructions **both** DeFRaG and vanilla — identical
instruction count (notably *not* the usual "DeFRaG = vanilla + 1" drift seen
on every other confirmed-unmodified function in this module, consistent with
a small net content swap rather than a pure addition).

The function was read in full, top to bottom (`0x288b1`–`0x289a4`). Structure
matches vanilla step-for-step:

1. `AngleVectors(ent->client->ps.viewangles, forward, right, up)` — `0xa186`, confirmed.
2. `CalcMuzzlePoint(ent, forward, right, up, muzzle)` — `0x29383`, confirmed identical to vanilla.
3. `VectorMA(muzzle, 32.0, forward, end)` — `32.0` confirmed literal (`0x42000000`).
4. `trap_Trace(&tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT=0x6000001)` — trap `-0x19` (`-25`), confirmed `trap_Trace`.
5. `if (tr.surfaceFlags & 0x10) return qfalse;` — matches vanilla's `SURF_NOIMPACT` check.
6. Blood-impact temp entity (`EV_MISSILE_HIT`, event `0x32`) if `traceEnt->takedamage && traceEnt->client` — matches vanilla.
7. `if (!traceEnt->takedamage) return qfalse;` — matches vanilla.
8. Quad check, damage, `G_Damage` call — see confirmed differences below.

### Confirmed difference #1 — `s_quadFactor` hardcoded, matching `FireWeapon`

```c
if ( ent->client-><+0x13c> != 0 ) {   // same field as FireWeapon's quad check
    G_AddEvent( ent, EV_POWERUP_QUAD /* 0x3d */, 0 );   // 0x2861b, confirmed G_AddEvent
    s_quadFactor = 3.0f;    // 0x40400000 — HARDCODED
} else {
    s_quadFactor = 1.0f;    // 0x3f800000
}
damage = Q_ftol( 50.0f * s_quadFactor );   // 0x42480000 = 50.0, confirmed
G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, 2 /* MOD_GAUNTLET, confirmed via oDFs bg_public.h enum */ );
```

Identical hardcoding pattern to `FireWeapon` (see
[FireWeapon.md](FireWeapon.md#1-quad-damage-scale-factor-0x29497–0x294aa)) —
vanilla uses `g_quadfactor.value` here too (same `s_quadFactor` global,
`0x1a205c`); DeFRaG bakes in the literal `3.0`. Base damage `50.0` is
unchanged from vanilla.

### Confirmed difference #2 — `ent->client->noclip` early-return appears absent

Vanilla `CheckGauntletAttack` has, immediately after the `SURF_NOIMPACT`
check and before computing `traceEnt`:

```c
if ( ent->client->noclip ) {
    return qfalse;
}
```

**This exact check was not found anywhere in DeFRaG's version.** The function
was read exhaustively from entry to `leave` with no gaps skipped; the
instruction sequence goes directly from the `SURF_NOIMPACT` branch
(`0x2890e`, jumping to `0x28913` when clear) straight into `traceEnt`
computation (`local28 = tr.entityNum * sizeof(gentity_t) + g_entities_base`)
with no intervening `noclip`-shaped branch. No other candidate location in
the function (before, inside, or after the quad/damage tail) contains a
matching pattern either.

**Confidence:** high but not absolute — the field this check reads
(`ent->client->noclip`) was not independently located by field offset in
DeFRaG's client struct, so it is possible (though considered unlikely given
the exhaustive read) that the check exists in a form not recognized as such.
Reported as a confirmed absence based on direct, complete instruction-level
reading, not an assumption from the instruction-count delta alone (the
count-delta is corroborating evidence, not the basis for this finding).

**Behavioral implication if confirmed:** in DeFRaG, a player in noclip
(observer/spectator-with-noclip, or a cheat-enabled noclip) could still deal
Gauntlet melee damage via `CheckGauntletAttack`, which vanilla Q3 explicitly
prevents. Whether this is reachable in practice depends on whether DeFRaG
ever puts a `client` with `takedamage`/attack capability into noclip during
normal (non-cheat) play — not investigated further, out of scope.

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x288ae` | `Weapon_Gauntlet` — confirmed identical to vanilla (empty) |
| `0x288b1` | `CheckGauntletAttack` entry |
| `0x2890e` | `SURF_NOIMPACT` branch — falls straight to `traceEnt` calc, no intervening noclip check found |
| `0x2896d` | Quad-factor branch (hardcoded 3.0/1.0) |
| `0x28980` | `damage = Q_ftol(50.0 * s_quadFactor)` |
| `0x2899f` | `G_Damage(..., mod=2)` call (`0x2015b`) |

## oDFs implementation notes

**Implemented, both findings.** In `CheckGauntletAttack`
(`code/game/g_weapon.c`): (1) `s_quadFactor` is now hardcoded to `3.0f`
instead of `g_quadfactor.value`, matching `FireWeapon`'s identical change;
(2) the `ent->client->noclip` early-return guard was removed, matching
DeFRaG's confirmed absence of this check.
