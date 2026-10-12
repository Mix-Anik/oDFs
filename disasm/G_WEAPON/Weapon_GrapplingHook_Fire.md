# Weapon_GrapplingHook_Fire / Weapon_HookFree / Weapon_HookThink — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

Scope note: this document covers only the *launch trigger* and think/free
lifecycle functions from `g_weapon.c`. The grapple **swing physics** consumer
chain and `fire_grapple`'s projectile spawn are already fully documented in
[../G_ACTIVE/GrappleMode.md](../G_ACTIVE/GrappleMode.md) and the
`0x2295f` entry (`0x22b82`/`0x22eb4` are mover code, not grapple — see G_ACTIVE/GrappleHookMover.md) in [../README.md](../README.md) — not
duplicated here.

## Weapon_GrapplingHook_Fire — MODIFIED

**DeFRaG address:** `0x290cf` (vanilla `0x206a2`, hash `363499d8`).
**DeFRaG hash:** `-8235d1f` — differs from vanilla.
**Size:** `0x77` (119) instructions vs vanilla `0x2b` (43) — **+76 instructions**.

Full function read (`0x290cf`–`0x29143`). Confirmed structure:

### 1. Same "LAG CHEAT DETECTED" gate as BFG/Rocket/Grenade

Uses client field `+0x364` and log string `"LAG CHEAT DETECTED: %s
(grapple)\n"` (`0xa4ac`). Identical shape/semantics to the other three sites
— see [WeaponFireRateLimit.md](WeaponFireRateLimit.md) for the full shared
mechanism writeup; not repeated here. If blocked, the function returns
**without** calling `fire_grapple` or setting `fireHeld`.

### 2. Redundant re-aim (confirmed new vs vanilla)

```c
AngleVectors( ent->client->ps.viewangles /* +0x98 off client */, forward, right, up );  // 0xa186, confirmed
CalcMuzzlePoint( ent, forward, right, up, muzzle );  // 0x29383, confirmed CalcMuzzlePoint (NOT CalcMuzzlePointOrigin)
```

Vanilla `Weapon_GrapplingHook_Fire` does **not** call `AngleVectors` or any
`CalcMuzzlePoint*` itself — it just uses the `forward`/`muzzle` globals that
`FireWeapon` already computed once (via `AngleVectors` +
`CalcMuzzlePointOrigin`, see [FireWeapon.md](FireWeapon.md)) before
dispatching. DeFRaG's version **independently recomputes both** right before
firing.

**Confirmed behavioral distinction:** `FireWeapon`'s own muzzle calc uses
`CalcMuzzlePointOrigin(ent, ent->client->oldOrigin, ...)` — basing the muzzle
on the client's *previous-frame* origin. This function's redundant recompute
instead uses plain `CalcMuzzlePoint(ent, ...)`, which internally uses
`ent->s.pos.trBase` (the entity's *current* origin) as its base point (see
[CalcMuzzlePoint.md](CalcMuzzlePoint.md)). This means the grapple hook's
actual launch muzzle point can differ slightly from the muzzle point
`FireWeapon` computed for every other weapon that frame — a genuine, confirmed
divergence in muzzle-origin source, though `AngleVectors` is recomputed from
the same `ps.viewangles` so the *direction* should be identical (viewangles
don't change within the same server frame between the two calls).

### 3. Fire-gate logic — unmodified from vanilla

```c
gclient_t *cl = ent->client;  // +0x204
if ( cl-><fireHeld, +0x348> == 0 && cl-><hook, +0x34c> == 0 ) {
    fire_grapple( ent, muzzle, forward );   // 0x2295f, confirmed fire_grapple
}
cl-><fireHeld, +0x348> = 1;   // always set, matching vanilla's unconditional `ent->client->fireHeld = qtrue;`
```

Matches vanilla's `if (!ent->client->fireHeld && !ent->client->hook)
fire_grapple(ent, muzzle, forward); ent->client->fireHeld = qtrue;` exactly
(inverted-condition `ne`/jump-to-skip form, direction verified).

## Weapon_HookFree — confirmed unmodified

**DeFRaG address:** `0x29145` (vanilla `0x206cd`, hash `68e05e84` both sides).
**Size:** `0x2a` (42) vs vanilla `0x29` (41) — the usual "+1" drift seen on
every other confirmed-unmodified function in this module. Called from 3
sites (`0x4324`, `0x1fc5a`, `0x21864`) — not decoded further, hash-match plus
size-drift-only pattern taken as sufficient confirmation per this module's
established methodology; not independently spot-checked for constants since
the function contains no floats (pure pointer/flag bookkeeping matching
vanilla's `ent->parent->client->hook = NULL; ent->parent->client->ps.pm_flags
&= ~PMF_GRAPPLE_PULL; G_FreeEntity(ent);`).

## Weapon_HookThink — confirmed unmodified

**DeFRaG address:** `0x2916e` (vanilla `0x206f6`, hash `4380702a` both sides).
**Size:** `0x87` (135) vs vanilla `0x86` (134) — same "+1" pattern. Not
decoded in detail (out of scope beyond confirming hash match); calls
`SnapVectorTowards` (`0x289a6`, confirmed via the "called from" list including
`0x2916e`) and `G_SetOrigin`, consistent with vanilla's body. No floats to
spot-check (pure vector/entity-origin bookkeeping).

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x290cf` | `Weapon_GrapplingHook_Fire` entry |
| `0x290e3` | "LAG CHEAT DETECTED: %s (grapple)" string ref (`0xa4ac`) |
| `0x290ed` | `AngleVectors` call (`0xa186`) — new vs vanilla |
| `0x29118` | `CalcMuzzlePoint` call (`0x29383`) — new vs vanilla, uses current origin not `oldOrigin` |
| `0x29137` | `fire_grapple` call (`0x2295f`) |
| `0x29145` | `Weapon_HookFree` entry — confirmed unmodified |
| `0x2916e` | `Weapon_HookThink` entry — confirmed unmodified |

## oDFs implementation notes

**Implemented, both findings.** `Weapon_GrapplingHook_Fire`
(`code/game/g_weapon.c`) now: (1) applies the same "LAG CHEAT DETECTED"
same-tick re-fire guard as the other three gated weapons (see
[WeaponFireRateLimit.md](WeaponFireRateLimit.md)), and (2) independently
recomputes `forward`/`muzzle` via `AngleVectors`+`CalcMuzzlePoint` (current
origin) rather than reusing `FireWeapon`'s `oldOrigin`-based
`CalcMuzzlePointOrigin` result, matching DeFRaG's confirmed redundant re-aim.
