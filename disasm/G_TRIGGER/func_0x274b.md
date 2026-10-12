# func_0x274b — Spawn-time weapon/ammo/health/armor loadout setup

**Address:** `0x274b`
**Stack frame:** `enter 0x2c`
**Called from:** `0x1d27b` (`0x1d56f`), inside the **spawn block** only — see
`ClientThink_real.md` §7. **Called once per client spawn/respawn, NOT every
frame.** Also self-recurses conceptually via `0x3933` (see below).
**Args:** `(gentity_t *ent)`
**Returns:** void

---

## This corrects a real misidentification in `g_defrag.c`

oDFs's `g_defrag.c` contains `G_UpdateRacePs`, documented in its own header
comment as:

> Mirrors DeFRaG func_0x274b: writes race-state values into ps fields that the
> DeFRaG cgame reads to show/hide the timer HUD... (called every frame, per
> the surrounding "per-frame race HUD update" section header)

**This identification is confirmed wrong on two independent counts:**

1. **Call frequency.** `0x274b` is called exactly once per spawn/respawn (from
   inside `0x1d27b`'s one-time "spawn block", the same block that does
   `memset(client, 0, 0x37c)`, sets `ent->health`, links the entity, etc. —
   see `ClientThink_real.md` line `0x1d56f`). It is never reached from the
   always-executed per-frame path (`ClientThink_real` proper, or the
   `Pmove`/`func_0x37c7` per-frame chain).

   **oDFs's `G_UpdateRacePs`, by contrast, IS live and IS called every single
   frame for every client** — confirmed at `code/game/g_active.c` line ~1072,
   immediately after `G_TimerTick(ent, msec)` inside `ClientThink_real`. This
   makes the call-frequency mismatch a real, currently-active behavioral
   difference (not just a documentation/dead-code issue): oDFs recomputes and
   overwrites `ps.ammo[1..10]`, `ps.stats[2]`, and `ps.stats[3]` on every
   server frame based on `g_startTimerReady`, whereas DeFRaG's actual
   equivalent-in-spirit logic (`0x274b`) runs those writes exactly once, at
   spawn time, based on static per-map mode-config globals. Whether
   overwriting these fields every frame with the same value each frame causes
   any observable problem depends on whether oDFs uses `ps.ammo[]`/`stats[2]`/
   `stats[3]` for anything else during normal (non-spawn) frames — not
   evaluated in this pass, since it would require auditing all of oDFs's own
   `ps.ammo`/`ps.stats[2,3]` usage, which is outside this task's scope.
2. **Content.** The fields it writes are not a race-active toggle. They are
   DeFRaG's version of vanilla Q3's `ClientSpawn` weapon/ammo initialization
   loop (health, armor, starting-weapons bitmask, per-weapon ammo counts),
   driven by DeFRaG mode/cvar-derived globals set once at map-init time
   (`0x131820`, `0x131824`, `0x13182c` — see `func_0x1e60.md`), not by
   whether a race is currently running.

---

## Full decode

```c
void func_0x274b(gentity_t *ent) {
    if (ent->client == NULL) return;              // ent+0x204 == client ptr

    ent->client->field_0xf8 = 0;                   // gclient_t+0xf8, purpose unknown

    Reset_0x3597(ent);                             // unconditional — resets this
                                                    // client's raceTimer_t (see func_0x37c7.md)

    if (*(int*)0x131808 == 3) {
        StartRace(ent);                            // 0x3933 — conditional on a global
                                                    // mode-config value, NOT the
                                                    // "startTimer ready" ==1 check that
                                                    // use_startTimer (0x39c5) uses on the
                                                    // SAME address. See open question below.
    }

    // --- health / armor ---
    int health;
    if (*(int*)0x131820 != 0)
        health = ent->client->field_0x224 + 0x19;  // +25
    else
        health = 200;
    ent->health              = health;             // ent+0x2dc
    ent->client->ps.stats[STAT_HEALTH] = health;   // ps+0xb8 (stats[0])

    int armor;
    if (*(int*)0x131820 != 0)
        armor = 0;
    else
        armor = 200;
    ent->client->ps.stats[3] = armor;              // ps+0xc4 — STAT_ARMOR in vanilla layout

    ent->client->ps.stats[2] = *(int*)0x131824;    // ps+0xc0 — STAT_WEAPONS bitmask in
                                                    // vanilla layout; UNCONDITIONAL global
                                                    // read, not a hardcoded 6

    // --- ammo[] (ps+0x178 base, i.e. vanilla playerState_t.ammo[MAX_WEAPONS]) ---
    ent->client->ps.ammo[1]  = -1;                 // ps+0x17c — unconditional
    ent->client->ps.ammo[10] = -1;                 // ps+0x1a0 — unconditional, NOT part of the loop below

    int ammo2 = ((*(int*)0x13182c & 0x6) != 0) ? 100 : 200;
    ent->client->ps.ammo[2] = ammo2;               // ps+0x180

    for (int i = 3; i <= 10; i++) {
        if ((*(int*)0x13182c & 0x6) == 0) {
            ent->client->ps.ammo[i] = 200;
        } else if ((*(int*)0x131824 & (1 << i)) == 0) {
            ent->client->ps.ammo[i] = 200;
        } else {
            gitem_t *item = BG_FindItemForWeapon(i); // func_0x4b6a — RESOLVED, see below
            ent->client->ps.ammo[i] = item->quantity; // +0x20
        }
    }
}
```

(Note: the loop above writes `ps.ammo[10]` a second time on its last
iteration, using the `func_0x4b6a`/200 logic — this can overwrite the
unconditional `-1` set earlier for slot 10, depending on the bit test. This
is exactly what the bytecode does; not simplified.)

---

## Exact instruction-level correspondence (selected)

| Bytecode (offsets from `client` = `*(ent+0x204)`) | Meaning |
|---|---|
| `client+0xf8 = 0` | Unknown gclient_t field, cleared at spawn |
| `client+0xb8 = health` (= `ps.stats[0]`) | `stats[0]` = `STAT_HEALTH` per README struct table |
| `client+0xc4 = armor` (= `ps.stats[3]`) | `stats[3]` = candidate `STAT_ARMOR`; README previously flagged this offset "powerups[PW_HASTE] equivalent?" — this decode suggests **STAT_ARMOR** is the more likely identity, consistent with vanilla Q3's `stats[STAT_ARMOR]` |
| `client+0xc0 = *(0x131824)` (= `ps.stats[2]`) | `stats[2]`, consistent with vanilla `STAT_WEAPONS` (bitmask of owned weapons) — `0x131824` defaults to `6` (binary `110` = bits 1,2 = `WP_GAUNTLET`+`WP_MACHINEGUN` if vanilla weapon enum holds) in DeFRaG mode 1, per `func_0x1e60.md` |
| `client+0x17c = -1` (= `ps.ammo[1]`) | matches vanilla `ps.ammo[WP_GAUNTLET] = -1` pattern |
| `client+0x1a0 = -1` (= `ps.ammo[10]`) | a **second** weapon slot forced to `-1`, unconditionally |
| `client+0x180 = 100/200` (= `ps.ammo[2]`) | gated on `(*(0x13182c) & 0x6) != 0`, **not** on any "race active" flag |
| `client+(0x178 + 4*i)` for `i=3..10` (= `ps.ammo[i]`) | per-slot: `200` if `(*(0x13182c)&0x6)==0`; else `200` if bit `i` of `*(0x131824)` is clear; else `*(func_0x4b6a(i) + 0x20)` |

---

## Comparison against oDFs's `G_UpdateRacePs` (`g_defrag.c`)

| oDFs claim | Disassembly reality | Verdict |
|---|---|---|
| Called every frame to drive race HUD | Called once per spawn/respawn in DeFRaG; oDFs's `G_UpdateRacePs` **is** called every frame (confirmed, `g_active.c` ~line 1072) | **Confirmed wrong identification, and live** — oDFs is actively running per-frame logic that DeFRaG only ever runs once per spawn |
| `ps.ammo[1] = -1` always | `ps.ammo[1] = -1` unconditionally | Value matches, but for unrelated (spawn loadout, not race-HUD) reasons |
| `ps.ammo[2] = raceActive ? 100 : 200` | `ps.ammo[2] = ((*(0x13182c) & 0x6) != 0) ? 100 : 200` | Literals (100/200) match; **gating condition does not** — driven by a static per-map mode-config global, not any "race running" state |
| `ps.ammo[3..10] = raceActive ? i : 200` | Per-slot: `200`, or `200`, or `*(func_0x4b6a(i)+0x20)` — **never simply writes the loop index `i`** | **Confirmed wrong** — real code never writes the bare index; it's either `200` or a value read through an unidentified helper |
| `ps.stats[2] = raceActive ? 6 : 0` | `ps.stats[2] = *(0x131824)` unconditionally (defaults to `6` under DeFRaG mode 1, but is a live global, not a race-gated hardcode of `6`/`0`) | **Confirmed wrong** — no conditional, no `0` fallback branch exists; the "6" literal oDFs assumed is only the *default value* of an entirely different mechanism |
| `ps.stats[3] = raceActive ? 0 : 200` | `ps.stats[3] = (*(0x131820) != 0) ? 0 : 200` | Literals (0/200) match; gating condition uses a different global (`0x131820`, a static mode flag) than `g_startTimerReady` |
| (not modeled in oDFs) | `ps.ammo[10] = -1` unconditionally (separate from the loop) | oDFs has no equivalent write |
| (not modeled in oDFs) | `ent->health` / `ps.stats[0]` (HEALTH) set from `client+0x224` or `200` | oDFs has no equivalent write in this function (health is handled elsewhere in oDFs's own `ClientSpawn`) |
| (not modeled in oDFs) | Unconditional call to `Reset_0x3597` (raceTimer_t wipe) and conditional call to `StartRace` when `*(0x131808)==3` | oDFs has no equivalent |

**Bottom line:** `G_UpdateRacePs` in `g_defrag.c` does not correspond to
`0x274b` in any behaviorally meaningful way beyond two coincidentally-matching
literal values (`-1` for slot 1, and the `100`/`200` pair for slot 2, whose
*condition* is still wrong). The function should not be described as
"mirroring `0x274b`" — a header-comment/identification error, not a bytecode
bug. Because `G_UpdateRacePs` **is** called every frame in current oDFs code
(`g_active.c`), this is not purely a documentation issue — it is live,
per-frame-executing logic built on a misidentified reference function. No
`.c` files were changed in this pass, per task instructions; this is reported
for the project owner to decide how to act on.

---

## RESOLVED — `0x131820` and `0x131824` cvar identities

A later pass (`func_0x1e60.md`'s "`df_ndm_*` cvar cluster" section)
confirmed `0x131820`/`0x131824` are not static mode-config constants but
live cvar caches, re-read every `G_InitRaceMode` (map init):

- `0x131820` = cached integer value of cvar **`df_ndm_damage`** (default
  `"0"`). This resolves the gate above: `df_ndm_damage == 0` (default) →
  flat 200 health, 200 armor; `!= 0` → handicap-based health (`maxHealth +
  25`, matching oDFs's own existing vanilla formula), armor = 0.
- `0x131824` = cached integer value of cvar **`df_ndm_weaponsSet`** (default
  `"1022"`, confirmed from the cvar-registration string pool). This resolves
  the unconditional `ps.stats[STAT_WEAPONS] = *(0x131824)` write: the
  starting weapon bitmask comes directly from this cvar.

`ent->client + 0x224`'s identity (used in the handicap branch) is still not
independently confirmed, but mapping it to oDFs's own
`client->ps.stats[STAT_MAX_HEALTH]` (itself derived from the `"handicap"`
userinfo key, same `+25` formula vanilla/oDFs already uses) is a direct,
low-risk equivalence rather than a guess — the formula and the field's
documented purpose (`STAT_MAX_HEALTH`: "health/armor limit, changeable by
handicap", `code/game/bg_public.h`) already match exactly.

## `func_0x4b6a` — RESOLVED: `BG_FindItemForWeapon`

Full decode confirms this beyond doubt: it walks the item list looking for
`item->giType == 1` (`IT_WEAPON`) and `item->giTag == arg0` (the weapon
number), returning that item — and its error path references the literal
string **`"Couldn't find item for weapon %i"`** (data address `0x4c86`).
This is vanilla's `BG_FindItemForWeapon` byte-for-byte:
```c
gitem_t *BG_FindItemForWeapon( weapon_t weapon ) {
	gitem_t	*it;
	for ( it = bg_itemlist + 1 ; it->classname ; it++) {
		if ( it->giType == IT_WEAPON && it->giTag == weapon ) {
			return it;
		}
	}
	Com_Error( ERR_DROP, "Couldn't find item for weapon %i", weapon);
	return NULL;
}
```
oDFs's own `BG_FindItemForWeapon` (`code/game/bg_misc.c:973`) is this exact
function, unmodified. The field offsets match too: `gitem_t.giType` sits at
`+0x24` and `giTag` at `+0x28` in oDFs's own `gitem_t` layout (computed from
`code/game/bg_public.h`'s field declarations: `classname`(+0)/
`pickup_sound`(+4)/`world_model[4]`(+8..+0x18)/`icon`(+0x18)/
`pickup_name`(+0x1c)/`quantity`(+0x20)/`giType`(+0x24)/`giTag`(+0x28)) —
exactly matching the bytecode's `+0x24`/`+0x28` checks, and `quantity` at
`+0x20` exactly matching what `func_0x274b` reads from the returned
pointer. So `func_0x274b`'s rarer branch is simply:
`ps.ammo[i] = BG_FindItemForWeapon(i)->quantity` — use the weapon's own
default pickup-ammo quantity instead of a flat `200`.

### The `*(0x13182c) & 0x6` gate — RESOLVED

Traced to `func_0x1e60` (`G_InitRaceMode`)'s body past where earlier passes
stopped reading (`0x2097`–`0x20ab`): confirmed to be a plain boolean gate,
not a real bitmask —
```c
if ( trap_Cvar_VariableIntegerValue( "df_ndm_weaponsPickup" ) != 0 )
    *(int*)0x13182c |= 0x6;
```
Nothing else in the QVM sets these two bits. So
`(*(0x13182c) & 0x6) != 0` is exactly `df_ndm_weaponsPickup.integer != 0`.
**Registered cvar default**: no distinct literal found directly adjacent
in the cvar-registration string pool (`df_ndm_nomover`/`noteleport`/`nopad`/
`itemsSpawn`/`weaponsPickup` all sit consecutively with no default string
between them) — same situation as `df_ndm_damage`/`df_ndm_nopad` earlier in
this project, i.e. a dedup'd shared `"0"` literal; same medium-high
confidence as those two, not a directly-read literal.

Also resolves `ps.ammo[2]` (`WP_MACHINEGUN`)'s special case from the table
above: `ammo2 = (df_ndm_weaponsPickup.integer != 0) ? 100 : 200` — the one
slot in this function that does NOT use `BG_FindItemForWeapon` even when
the gate is active, just a flat `100` instead of `200`.

## Implemented

`code/game/g_client.c`'s `ClientSpawn`: replaced the vanilla hardcoded
gauntlet+machinegun loadout and `handicap+25`-always health with the
decoded DeFRaG logic — `STAT_WEAPONS` from `df_ndm_weaponsSet.integer`, the
`df_ndm_damage`-gated health/armor branch above, and now (this pass) the
full `df_ndm_weaponsPickup`-gated ammo logic: `WP_MACHINEGUN` gets
`100`/`200`, and `WP_SHOTGUN..WP_BFG` get `BG_FindItemForWeapon(i)->quantity`
when `df_ndm_weaponsPickup` is set and the weapon is present in
`weaponsSet`, else `200` — exactly matching the decoded loop. New cvars
`df_ndm_damage` (default `"0"`), `df_ndm_weaponsSet` (default `"1022"`),
and `df_ndm_weaponsPickup` (default `"0"`) registered in `code/game/g_main.c`.
Compiled clean.

Still **not** ported faithfully: the bytecode's apparent quirk of
overwriting `ps.ammo[WP_GRAPPLING_HOOK]` from `-1` to `200`/item-quantity in
the same loop — oDFs keeps it at `-1` (infinite) instead, since that's
strictly more sensible and the hook is excluded from the default
`weaponsSet` mask anyway. `ent->client+0xf8` and the `0x131808==3`/
`StartRace` call are likewise not ported (see unresolved items below,
unchanged).

## Unresolved items (flagged, not speculated on)

- **`ent->client + 0xf8`** — cleared to 0 at the very start of this function.
  Field identity unknown.
- **Why `0x131808` is compared `==3` here vs. `==1` in `use_startTimer`
  (`0x39c5`)** for what is nominally the same address — not resolved. Both
  reads target the literal address `0x131808`; per `func_0x1e60.md`,
  `0x131808` is a small-integer mode/ready value (0/1/2/3/4 with distinct
  meanings), so a `==3` check elsewhere on the same address is plausible
  without being a decode error, but the exact intended semantic ("gametype 3
  auto-starts the race on every spawn") is not confirmed against any other
  evidence in this pass.
- **Identity of `ps+0xc4` (`stats[3]`)** — README's struct table previously
  flagged this as "powerups[PW_HASTE] equivalent?"; this decode's values (0 or
  200) and vanilla-shaped layout are more consistent with **STAT_ARMOR**, but
  this is a inference from structure/values, not an independent confirmation
  (e.g. no string literal or unambiguous cross-reference was found in this
  pass) — flagged as a candidate correction to the README table, not asserted
  as fact.

## Update: `client+0xf8` resolved and ported
`client+0xf8` = `ps.persistant[0]` = `PERS_SCORE` (stats[16] ends at 0xf8; consumer `target_fragsFilter`, see [target_fragsFilter.md](target_fragsFilter.md)).
The store at `0x275d` (sole caller `ClientSpawn 0x1d27b`) zeroes the score on **every spawn**. Ported in oDFs `ClientSpawn` (`client->ps.persistant[PERS_SCORE] = 0`).
**Behaviour change:** frag score resets to 0 at each respawn (scoreboard shows per-life score); revert = delete that one line in g_client.c. Syntax-checked, untested.
