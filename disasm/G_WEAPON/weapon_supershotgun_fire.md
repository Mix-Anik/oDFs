# weapon_supershotgun_fire — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

**DeFRaG address:** `0x28d3c` (vanilla `0x20367`, hash `-40f8d4a2` **both
sides** — exact hash match).
**Size:** `0x80` (128) instructions vs. instruction range `0x28d3c`–`0x28dba`
(0x7e = 126, +1-ish drift, consistent with confirmed-unmodified functions in
this module). Single caller: `0x29497` (`FireWeapon`).

Previously only spot-checked (4096.0 blast-scale constant). Read in full,
top to bottom, this pass.

## Full decode

```c
void weapon_supershotgun_fire (gentity_t *ent) {
    tent = G_TempEntity( muzzle, EV_SHOTGUN /*0x36*/ );          // 0x28d3d-0x28d44, call 0x284e6
    VectorScale( forward, 4096, tent->s.origin2 );                // 0x28d49-0x28d63, tent+0x68/0x6c/0x70, 0x45800000=4096.0
    SnapVector( tent->s.origin2 );                                // 0x28d64-0x28d8d, inlined as 3x (float)Q_ftol(x) round-trips
    tent->s.eventParm = rand() & 255;                              // 0x28d8e-0x28d9a, call 0x5dc2 (rand), tent+0xb8
    tent->s.otherEntityNum = ent->s.number;                        // 0x28d9b-0x28da2, tent+0x8c, *(ent)
    ShotgunPattern( tent->s.pos.trBase, tent->s.origin2,
                     tent->s.eventParm, ent );                     // 0x28da3-0x28db8, call 0x28c57
}
```

All argument-passing to `ShotgunPattern` confirmed exactly matching vanilla
call order: `Arg[0x8]=tent+0x18` (`s.pos.trBase`), `Arg[0xc]=tent+0x68`
(`s.origin2`), `Arg[0x10]=*(tent+0xb8)` (the *value* of `s.eventParm`, not
its address), `Arg[0x14]=ent`.

`SnapVector` here is inlined per-component as `(float)Q_ftol(x)` (truncating
round-trip through int), applied separately to `tent+0x68`, `+0x6c`, `+0x70`
— consistent with how `SnapVector`-shaped code appears elsewhere in this
codebase (not a distinct function call at this site; whether the real
`SnapVector` trap/function is itself inlined this way, or this is a separate
inlined-by-compiler idiom, was not further investigated — out of scope).

## Result: confirmed vanilla-identical

No `LAG CHEAT` gate (confirmed absent — this function was fully read and
contains no comparison against any per-client last-fire-time field, no log
string reference). No other deviation found; every constant (`4096.0`
blast scale, `EV_SHOTGUN=0x36`, `rand()&255`) and every struct-field offset
used matches the expected vanilla behavior exactly.

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x28d3c` | `weapon_supershotgun_fire` entry |
| `0x28d42` | `G_TempEntity(muzzle, EV_SHOTGUN)` call (`0x284e6`) |
| `0x28d4d` | `VectorScale(forward, 4096.0, tent->s.origin2)` — `0x45800000` confirmed |
| `0x28d8f` | `rand()` call (`0x5dc2`) |
| `0x28db6` | `ShotgunPattern(...)` call (`0x28c57`) |

## oDFs implementation notes

No change needed — confirmed vanilla-identical. `oDFs`'s
`weapon_supershotgun_fire` (`code/game/g_weapon.c`) already matches this
exactly.
