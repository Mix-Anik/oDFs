# PM_Weapon — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00007fb8`  
**Called from:** `0x8331` (PmoveSingle)

Subfunctions also documented here:
- `0x7ecf` — PM_BeginWeaponChange
- `0x7f38` — PM_FinishWeaponChange

---

## Weapon state enum (confirmed from disassembly)

| Value | Name |
|-------|------|
| `0` | `WEAPON_READY` |
| `1` | `WEAPON_RAISING` |
| `2` | `WEAPON_DROPPING` |
| `3` | `WEAPON_FIRING` |

---

## PM_BeginWeaponChange (0x7ecf)

**Called from:** `0x7d06`, `0x7e78`, `0x7fb8`  
**Args:** `arg0` = weapon number (int)

**Guards (in order):**
1. `weapon <= 0 || weapon >= 11` → return (range check)
2. `!(ps->stats[STAT_WEAPONS] & (1 << weapon))` → return (doesn't have weapon)
3. **`weapon == 10` (WP_GRAPPLING_HOOK) → return** ← DeFRaG addition, absent from Q3/oDFs
4. `ps->weaponstate == WEAPON_DROPPING (2)` → return (already dropping)

**Then:**
1. `PM_AddEvent(0x16)` — EV_CHANGE_WEAPON
2. `ps->weaponstate = 2` (WEAPON_DROPPING)
3. **Promode** (`pm_flags & 0x8000`): `weaponTime` unchanged (no-op)  
   **Non-promode**: `weaponTime += 200` (0xc8)
4. `PM_StartTorsoAnim(9)` — TORSO_DROP

**Verified match with oDFs except:** guard 3 (WP_GRAPPLING_HOOK). Added to oDFs `bg_pmove.c` PM_BeginWeaponChange.

---

## PM_FinishWeaponChange (0x7f38)

**Called from:** `0x7fb8`  
**Args:** none (reads cmd.weapon directly from pm)

1. `weapon = *(byte*)(pm + 0x18)` = `cmd.weapon`
2. If `weapon < 0 || weapon >= 11`: `weapon = 0` (WP_NONE)
3. If `!(ps->stats[STAT_WEAPONS] & (1 << weapon))`: `weapon = 0`
4. `ps->weapon = weapon`
5. `ps->weaponstate = 1` (WEAPON_RAISING)
6. **Promode**: `weaponTime` unchanged  
   **Non-promode**: `weaponTime += 250` (0xfa)
7. `PM_StartTorsoAnim(10)` — TORSO_RAISE

**Verified match with oDFs — full match.** ✓

---

## CPM promode modification: instant weapon switching

**Stock Q3:** weapon drop 200ms + raise 250ms = 450ms total.

**Defrag CPM promode:** both delays skipped (`weaponTime` unchanged = 0). State still steps
DROPPING → RAISING → READY, but since `weaponTime` is 0, each transition fires the next frame
(~8ms at 125fps). Effectively instant.

---

## PM_Weapon main (0x7fb8)

**Called from:** `0x8331` (PmoveSingle)

Structure matches stock Q3 PM_Weapon closely:

1. `ps->pm_flags & 0x200` (PMF_RESPAWNED) → return
2. `ps->persistant[PERS_TEAM] == 3` (TEAM_SPECTATOR) → return
3. `ps->stats[STAT_HEALTH] <= 0` → `ps->weapon = WP_NONE`, return
4. BUTTON_USE_HOLDABLE logic (matches Q3 holdable item handling) ✓
5. If `weaponTime > 0`: `weaponTime -= pml.msec`
6. If `weaponTime <= 0 || weaponstate != WEAPON_FIRING (3)`: check weapon change  
   → if `ps->weapon != cmd.weapon`: call PM_BeginWeaponChange(cmd.weapon)
7. If `weaponTime > 0`: return
8. If `weaponstate == WEAPON_DROPPING (2)`: call PM_FinishWeaponChange, return
9. If `weaponstate == WEAPON_RAISING (1)`:  
   → `weaponstate = 0` (WEAPON_READY)  
   → `ps->weapon == WP_GAUNTLET`: PM_StartTorsoAnim(12=TORSO_STAND2) else PM_StartTorsoAnim(11=TORSO_STAND)  
   → return
10. If `!(cmd.buttons & 0x1)` (no BUTTON_ATTACK): `weaponTime = 0`, `weaponstate = 0`, return
11. Gauntlet handling (see below)
12. `weaponstate = 3` (WEAPON_FIRING)
13. Ammo check → EV_NOAMMO + 500ms, return
14. Decrement ammo (if not -1 = infinite)
15. PM_AddEvent(0x17) — EV_FIRE_WEAPON
16. Fire delay switch (see below)
17. Haste powerup: `addTime /= 1.3f`
18. `weaponTime += addTime`

No promode modification to fire delay. ✓

---

## Gauntlet check (0x80ec) — resolved

> **CORRECTION (demo replay, PORT_STATUS entry 85):** the "always 0" conclusion below is wrong. `ClientThink_real` (`0x1a9d9..0x1aa05`) calls `CheckGauntletAttack` (`0x288b1`) when the weapon is the gauntlet, attack is down, the talk button is up and `weaponTime <= 0`, and stores the result in `pm+0x2c`; a recorded gauntlet hit fires the weapon (event, `weaponTime` 400). The port now does the same.

At 0x80ec: loads `*(pm + 0x2c)` and checks `!= 0` (branch to TORSO_ATTACK2 vs WEAPON_READY+return).

**Resolved via [../G_ACTIVE/func_0x1a394.md](../G_ACTIVE/func_0x1a394.md#resolved-gauntlethit-always-0):**
the pm struct is `memset(pm, 0, 0xec)` at the top of every frame in `func_0x1a394` (the pm-struct
builder called from Pmove outer), and no DeFRaG code path ever writes to `pm+0x2c` afterward — there
is no `CheckGauntletAttack` call anywhere in the QVM. So regardless of which field actually lives at
`pm+0x2c` (whichever of oDFs's `gauntletHit`/`framecount` DeFRaG's layout puts there), the read is
**always 0**. Gauntlet therefore always drops into the `weaponTime=0, weaponstate=WEAPON_READY, return`
branch instead of firing — i.e. it fires in continuous/animation-only mode with no per-frame hit
detection, matching Q3's "gauntlet only fires when it actually hits something" being permanently false.

oDFs already matches this: `g_active.c` never calls `CheckGauntletAttack`/sets `pm.gauntletHit`
(memset default `qfalse`), so `bg_pmove.c`'s `if (!pm->gauntletHit)` guard at PM_Weapon always takes
the same early-return path DeFRaG does. No code change needed — this was fixed in a prior pass.

---

## Fire delays (switch at 0x817c, weapons 1–10)

| Weapon | Value | addTime |
|--------|-------|---------|
| 1 WP_GAUNTLET | case 1 | 400ms (0x190) |
| 2 WP_MACHINEGUN | case 2 | 100ms (0x64) |
| 3 WP_SHOTGUN | case 3 | 1000ms (0x3e8) |
| 4 WP_GRENADE_LAUNCHER | case 4 | 800ms (0x320) |
| 5 WP_ROCKET_LAUNCHER | case 5 | 800ms (0x320) |
| 6 WP_LIGHTNING | case 6 | 50ms (0x32) |
| 7 WP_RAILGUN | case 7 | 1500ms (0x5dc) |
| 8 WP_PLASMAGUN | case 8 | 100ms (0x64) |
| 9 WP_BFG | case 9 | 200ms (0xc8) |
| 10 WP_GRAPPLING_HOOK | case 10 | 400ms (0x190) |

All match oDFs exactly. ✓ No MISSIONPACK cases present. ✓

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x7ecf` | PM_BeginWeaponChange entry |
| `0x7eef` | WP_GRAPPLING_HOOK guard (weapon == 10 → return) |
| `0x7f0f` | Promode check in PM_BeginWeaponChange |
| `0x7f38` | PM_FinishWeaponChange entry |
| `0x7f70` | Promode check in PM_FinishWeaponChange |
| `0x7fb8` | PM_Weapon entry |
| `0x7fc2` | PMF_RESPAWNED guard |
| `0x7fcf` | TEAM_SPECTATOR guard |
| `0x80ac` | weaponstate == WEAPON_RAISING (1) → WEAPON_READY |
| `0x80cb` | BUTTON_ATTACK check |
| `0x80e4` | Gauntlet weapon check |
| `0x80ec` | Gauntlet hit check (pm+0x2c — uncertain, see above) |
| `0x810f` | weaponstate = WEAPON_FIRING (3) |
| `0x816f` | EV_FIRE_WEAPON (0x17) |
| `0x817c` | Fire delay switch start |
| `0x81bc` | Haste powerup check + weaponTime += addTime |
| `0x81dc` | Function return |
