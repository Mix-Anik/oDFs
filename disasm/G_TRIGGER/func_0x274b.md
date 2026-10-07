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
            void *item = func_0x4b6a(i);           // NOT identified — takes the loop
                                                    // index, returns a pointer
            ent->client->ps.ammo[i] = *(int*)((char*)item + 0x20);
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

## Unresolved items (flagged, not speculated on)

- **`func_0x4b6a(i)`** — takes the loop index (`3..10`), returns a pointer;
  offset `+0x20` of that pointer is read into `ps.ammo[i]`. Not identified.
  Plausibly a weapon/item-table lookup (e.g. something returning a
  `gitem_t`-like struct and reading an ammo-count field), but this is
  explicitly **not confirmed** — flagged as unknown per project rules.
- **`ent->client + 0xf8`** — cleared to 0 at the very start of this function.
  Field identity unknown.
- **`ent->client + 0x224`** — read (and `+25` added) when computing spawn
  health if `*(0x131820) != 0`. Field identity unknown; plausibly a
  handicap-derived value, not confirmed.
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
