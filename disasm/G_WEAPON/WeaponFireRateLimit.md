# DeFRaG "LAG CHEAT DETECTED" fire-rate anti-exploit gate

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Affected functions:** `BFG_Fire` (`0x28b70`), `Weapon_RocketLauncher_Fire`
(`0x28e2b`), `weapon_grenadelauncher_fire` (`0x28dbb`),
`Weapon_GrapplingHook_Fire` (`0x290cf`).

All four functions received the **identical** added logic (confirmed via
direct byte-for-byte instruction reads of each), accounting for their
increased size vs. vanilla (`BFG_Fire`/`Weapon_RocketLauncher_Fire`: `0x34`→
`0x61`, +45 instr; `weapon_grenadelauncher_fire`: `0x44`→`0x71`, +45 instr;
`Weapon_GrapplingHook_Fire`: `0x2b`→`0x77`, +76 instr, which also includes an
unrelated second addition, see [Weapon_GrapplingHook_Fire.md](Weapon_GrapplingHook_Fire.md)).

## Confirmed logic (pseudocode, matches all 4 sites)

```c
int lastFireTime = *(int *)( (char*)ent->client + FIELD_OFFSET );  // per-weapon field
if ( lastFireTime <= 0 || level.time > lastFireTime ) {
    // allowed: update the timestamp, then fire normally
    *(int *)( (char*)ent->client + FIELD_OFFSET ) = level.time;
    <call the real fire_* function, exactly as vanilla>
} else {
    // blocked: log and return WITHOUT firing
    G_LogPrintf_or_similar( "LAG CHEAT DETECTED: %s (<weapon>)\n", ent->client->netname /* +0x200 off client */ );
    return;   // fire_* is never called
}
```

Per-weapon client-struct field offsets and log strings (each independently
confirmed via its own literal string in the LIT segment):

| Weapon | Client field offset | Log string (LIT segment) | Confirmed at |
|--------|---------------------|---------------------------|---------------|
| BFG | `+0x358` | `"LAG CHEAT DETECTED: %s (bfg)\n"` (`0xa511`) | `0x28b74`–`0x28b8f` |
| Rocket | `+0x368` | `"LAG CHEAT DETECTED: %s (rocket)\n"` (`0xa4ce`) | `0x28e2f`–`0x28e4a` |
| Grenade | `+0x35c` | `"LAG CHEAT DETECTED: %s (grenade)\n"` (`0xa4ef`) | `0x28dbf`–`0x28dda` |
| Grapple | `+0x364` | `"LAG CHEAT DETECTED: %s (grapple)\n"` (`0xa4ac`) | `0x290d3`–`0x290ee` |

**Branch semantics (verified per the project's branch-direction caution):**
the "allowed" path is `lastFireTime <= 0 OR level.time > lastFireTime`; the
"blocked" (cheat-detected, skip firing) path is the fallthrough (i.e.
`lastFireTime > 0 AND level.time <= lastFireTime`). Since the field is only
ever set to the *current* `level.time` (not `level.time + <cooldown>`),
`level.time <= lastFireTime` can only be true if the field was already set to
the *same or later* `level.time` value than the current frame — i.e. this
does **not** implement a real per-weapon cooldown; it can only trigger if the
same weapon is fired **more than once within the same server frame/tick**
(the same `level.time` value), which under normal play is achieved through a
client attempting to double-fire via duplicated/replayed usercmds — a known
Q3 network-exploit pattern. Legitimate single-shot-per-frame fire commands
always pass (each new `level.time` value is strictly greater than the last
stored one).

The log call (`0xa1a`, all 4 sites) takes a `%s`-format string and
`ent->client + 0x200` as its `%s` argument (a name/netname-shaped field, not
independently confirmed against a struct offset table) inside a very large
(`enter 0x434`) buffer, and separately computes `level.time / 1000` early in
its own body — consistent with `G_LogPrintf`'s known vanilla behavior of
prefixing log lines with a `MM:SS`-style timestamp. **Not independently
confirmed via string content** (the function itself was not fully decoded,
out of scope) — flagged as "very likely `G_LogPrintf`" rather than certain.

## Not present on `Weapon_Plasmagun_Fire` or `weapon_supershotgun_fire`

Confirmed by direct read: neither `Weapon_Plasmagun_Fire` (`0x28e8b`, hash+
size match vanilla almost exactly) nor `weapon_supershotgun_fire` (`0x28d3c`,
hash match) contain this gate — it was selectively added only to
BFG/Rocket/Grenade/Grapple. Machinegun (`Bullet_Fire`), Shotgun, Railgun, and
Lightning were not given this check either. No obvious single reason
(splash-damage weapons? high-value weapons?) accounts for exactly this set —
BFG/Rocket/Grenade are all splash-damage weapons, but Plasmagun also has
splash damage and was *not* gated; Grapple has no damage at all. This pattern
is reported as observed, not explained.

## oDFs implementation notes

**Implemented.** Added `lastBFGFireTime`/`lastRocketFireTime`/
`lastGrenadeFireTime`/`lastGrappleFireTime` fields to `gclient_t`
(`g_local.h`), and a matching same-tick re-fire guard + `G_LogPrintf( "LAG
CHEAT DETECTED: %s (<weapon>)\n", ent->client->pers.netname )` at the top of
`BFG_Fire`, `Weapon_RocketLauncher_Fire`, `weapon_grenadelauncher_fire`, and
`Weapon_GrapplingHook_Fire` in `code/game/g_weapon.c`.
