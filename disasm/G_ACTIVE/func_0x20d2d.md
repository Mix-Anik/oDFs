# func_0x20d2d — Pickup_Weapon (weapon-item pickup handler)

**Address:** `0x20d2d`
**Stack frame:** `enter 0x20`
**Called from:** `0x20fac`, inside `func_0x20f11` (`Pickup_Item`, see below) — specifically
from the `case 1` arm of a switch on an item's `itemType` field (`item->giType`,
values 1–8, read at `0x20f95`, matching vanilla `itType_t`: `IT_WEAPON=1,
IT_AMMO=2, IT_ARMOR=3, IT_HEALTH=4, IT_POWERUP=5, IT_HOLDABLE=6,
IT_PERSISTANT_POWERUP=7, IT_TEAM=8`). `itemType==1` (`IT_WEAPON`) is the only
case that calls this function.
**Args:** `(gentity_t *item, gentity_t *player)` — arg0 = the weapon-pickup
entity (has a `.item` pointer at `+0x324`), arg1 = the entity picking it up
(has a `.client` pointer, dereferenced via `+0x204` throughout).

---

## Why this function was investigated

This function was flagged in [`func_0x4290.md`](func_0x4290.md) and
[`func_0x42b5.md`](func_0x42b5.md) as an unidentified caller of
`SetLGKnockback` (`0x4290`) and `SetGrappleMode` (`0x42b5`) — both previously
noted as "function unknown — possibly a cvar change handler". Its address
(`0x20d2d`) falls inside the previously-unmapped `0x208f2`–`0x22066` gap
between `G_RadiusDamage` (`0x20773`) and `G_ExplodeMissile` (`0x22066`), and
was investigated specifically to check whether it (or anything else in that
gap) is a source of the ~+10-unit additive rocket-jump knockback bonus
documented as unexplained in [`G_COMBAT/G_Damage.md`](../G_COMBAT/G_Damage.md).

**Result: ruled out.** See headline finding below.

---

## Headline finding

**The entire function body (`0x20d2d`–`0x20e24`, 248 instructions) contains
zero floating-point instructions of any kind** — no `addf`, `subf`, `mulf`,
`divf`, `addi` on a float-typed field. It is pure integer/pointer/bitflag
logic: ammo-array bookkeeping, an inventory bitmask update
(`ps->stats[?] |= (1 << weapon)`), and two conditional calls to flag-setter
helpers based on the picked-up weapon's index. **It does not read or write
`ps.velocity`, `dir`, or `knockback` anywhere, and it contains zero references
to the `df_promode` cvar cache (`0x1317c4`)** — confirmed by grepping the full
instruction range for both.

This closes out `0x20d2d` as a candidate for the rocket-jump extra-velocity
investigation: it is a **weapon-pickup handler**, not a damage/knockback or
cvar-change handler.

---

## Pseudocode

```c
// arg0 = item (weapon pickup entity), arg1 = player (entity picking it up)
int func_0x20d2d(gentity_t *item, gentity_t *player) {
    if (!func_0x3c8b(item))            // gate: item->client-ish field null-check
        return 5;

    int ammoWanted;
    if (item->item->quantity < 0)              // item+0x324 = ->item (gitem_t*); ->item+0x28 = quantity
        ammoWanted = -1;                        // infinite ammo marker
    else if (item->item->quantity == 0)
        ammoWanted = item->item->quantity;      // 0 → 0 (redundant branch, same as below)
    else
        ammoWanted = ammoTable[item->item->weapon]; // base ammo count per weapon, from a table indexed by item->item->weapon

    if (!(player->client->pers.flags(+0x218) & 0x1000)) {   // not "no ammo cheat"/unlimited flag
        if (level.gametype(0x1b1ec) != 3) {                 // not GT_?  (uncertain gametype constant)
            int weaponIdx = item->item->weapon;             // item+0x324 -> +0x28
            int curAmmo = player->client->ps.ammo[weaponIdx]; // via ammo array at client+0x204+0x204(+0x178 offset chain)
            if (curAmmo < ammoWanted)
                curAmmo = ammoWanted - curAmmo;    // top up to ammoWanted
            else
                curAmmo = 0;                       // already enough / less
            // (stored back into player's ammo array for this weapon)
        } else {
            player->ps.ammo[weaponIdx] = -1;        // infinite ammo (some gametype)
        }
    }

    // Mark weapon as owned in the inventory bitmask:
    player->client->ps.stats[STAT_WEAPONS] |= (1 << item->item->weapon);

    // Special-case flag updates when the specific weapon is LG or Grapple Hook:
    if (item->item->weapon == WP_LIGHTNING /* 6 */)
        SetLGKnockback(player->client->ps, (int)(float)item->random /* item+0x2a0, Q_ftol */);   // func_0x4290
    else if (item->item->weapon == WP_GRAPPLING_HOOK /* 0xa */)
        SetGrappleMode(player->client->ps, (int)(float)item->random);                             // func_0x42b5

    return 5; // or item respawn value (uncertain — see open questions)
}
```

---

## Structure (by address)

### 1. Gate check (`0x20d2d`–`0x20d3c`)
```
local18 = func_0x3c8b(arg0)     ; gate helper (see func_0x3c8b below)
if (local18 == 0): return 5     ; early out
```

### 2. Compute `ammoWanted` from item quantity (`0x20d3d`–`0x20d63`)
```
q = *(item->item + 0x28)                 ; item+0x324 -> item pointer; +0x28 = quantity/weapon-index shared slot? (see note)
if (q >= 0):
    if (q == 0):
        ammoWanted = q                    ; == 0
    else:
        ammoWanted = q                    ; (redundant path, same value both branches)
else:
    ammoWanted = *(ammoBaseTable + weapon*4)   ; table lookup, indexed by weapon (item+0x324->+0x28)
```
**Note:** this section's exact field semantics (is `+0x28` "quantity" or "weapon index" or both, aliased?) are **not fully disambiguated** — the two branches at `0x20d49`/`0x20d59` both ultimately read the same `item+0x324+0x28` field either as a direct value or as a table index. Marked uncertain; does not affect the velocity-related conclusion.

### 3. Ammo top-up, gated by unlimited-ammo flag and gametype (`0x20d63`–`0x20da3`)
```
if (!(player->client_flags(+0x218) & 0x1000)) {         ; not "unlimited ammo" cheat/mode bit
    if (level.gametype(0x1b1ec) == 3):                    ; specific gametype
        continue to ammo-array top-up logic
    else:
        ammo = -1                                          ; infinite ammo
}
; ammo-array top-up: reads/writes player->client + 0x204 (ps) + 0x8c (ammo table base) + weapon*4
```
No `addf`/`subf` anywhere in this block — all integer ammo arithmetic (`sub`,
comparisons), confirmed at instruction level.

### 4. Inventory bitmask update (`0x20da3`–`0x20dbd`)
```
*(local1c) |= (1 << weapon)      ; local1c = &player->client->ps + 0x204 + 0xc0 (STAT_WEAPONS-class field)
```
Integer `bor`/`lsh` only.

### 5. Generic item-give call (`0x20dbd`–`0x20dce`)
```
func_0x20c97(player->client, item->item->weapon, ammoWanted)   ; unidentified helper — not decoded further, no float ops observed at call site
```

### 6. LG / Grapple special-case dispatch (`0x20dcf`–`0x20e21`)
```
weapon = item->item->weapon      ; item+0x324 -> +0x28
if (weapon == 0xa):                                    ; WP_GRAPPLING_HOOK
    *(item->item + 0x28 + (weapon<<2) + 0x178 [via player->client+0x204]) = -1   ; (grapple ammo slot set to -1, integer only)
if (weapon == 6):                                       ; WP_LIGHTNING
    SetLGKnockback(player->client->ps(+0x204), Q_ftol(item->item->random(+0x2a0)))   ; call 0x4290
if (weapon == 0xa):                                      ; WP_GRAPPLING_HOOK
    SetGrappleMode(player->client->ps(+0x204), Q_ftol(item->item->random(+0x2a0)))   ; call 0x42b5
return 5
```
The `Q_ftol(item->item->random)` argument (`item+0x2a0` cast to int) becomes
the boolean/mode flag passed into `SetLGKnockback`/`SetGrappleMode` — **not**
a cvar read at this call site (the cvars `df_lg_Knockback`/grapple-mode cvar
are read elsewhere, e.g. by `func_0x421b`, and cached in `item->item->random`
or a similar field at pickup-table build time; this was **not** further
traced — out of scope once velocity involvement was ruled out).

---

## `func_0x3c8b` — gate helper (fully decoded, called by this + 4 siblings)

```c
int func_0x3c8b(gentity_t *ent) {
    if (ent->item /* +0x324 */ == NULL)
        return 0;
    if (ent->itemFlags /* +0x218 */ & 0x1000000)
        return 1;
    return (ent->item /* +0x324 */ != NULL);   // effectively always 1 past the first check
}
```
Called from `0x20afa`, `0x20c57`, `0x20ced`, `0x20d2d`, `0x20e25` — a sibling
family of item-pickup-related functions (all in the same `0x208f2`-`0x22066`
gap). No float operations. Not velocity-related.

---

## Confirmed: no floating-point instructions anywhere in this function

Full grep of the instruction range `0x20d2d`–`0x20e24` for `addf`, `subf`,
`mulf`, `divf`: **zero matches.** Full grep for `1317c4` (df_promode cache):
**zero matches.** Full grep for the global playerState pointer `0x132900`:
**zero matches.**

---

## Key addresses

| Address | Description |
|---------|-------------|
| `0x20d2d` | Function entry |
| `0x20d38` | Gate check via `func_0x3c8b` |
| `0x20d63` | Unlimited-ammo flag check (`+0x218 & 0x1000`) |
| `0x20d6d` | `level.gametype (0x1b1ec) == 3` check |
| `0x20dbb` | Inventory bitmask `\|= (1 << weapon)` |
| `0x20dfc` | `weapon == 6` (WP_LIGHTNING) → call `SetLGKnockback` (`0x4290`) |
| `0x20e15` | `weapon == 0xa` (WP_GRAPPLING_HOOK) → call `SetGrappleMode` (`0x42b5`) |
| `0x20e21` | `leave 0x20` (return 5) |

---

## Open questions (not resolved, do not affect the velocity conclusion)

- Exact semantics of `item->item+0x28` (quantity vs. weapon index vs. both aliased).
- Identity/purpose of `func_0x20c97` (called at `0x20dcc`, generic "give weapon" helper, not decoded).
- Whether `item->item->random` (`+0x2a0`) truly caches `df_lg_Knockback`/grapple-mode cvar values at item-table build time, or something else — not traced (out of scope).
- Exact caller identity of `func_0x20f11` — strongly evidenced to be **Pickup_Item** (item-pickup dispatcher, switches on `itemType` 1–8, references a `"Item: %i %s"` debug-log string literal at `0x8ed8`), but not independently confirmed against a vanilla source diff. Not documented as its own file since it is orthogonal to the velocity investigation and was only decoded far enough to establish the switch/dispatch context.

---

## oDFs implementation notes

**No change needed.** This function has no confirmed effect on movement
physics, damage, or knockback — it only sets two DeFRaG-specific HUD/behavior
flags (`stats[12]` bits `0x1000` and `0x400`/`0x800`) on weapon pickup, and
performs integer ammo bookkeeping. Per CRITICAL RULE, not ported.
