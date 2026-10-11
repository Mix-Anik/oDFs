> **Identity resolved — this function is actually `ClientSpawn`, not `ClientThink_real`.**
> See [ClientThink.md](ClientThink.md) for the full resolution: the genuine
> per-command `ClientThink` is `0x1ac8f`, which calls `0x1a866` ("Pmove outer",
> documented in [Pmove_outer.md](Pmove_outer.md)) as the real `ClientThink_real`.
> This function's instruction-level decoding below is still accurate; only the name
> in the title and headers is wrong. Kept as-is (not renamed/moved) to preserve the
> address-indexed history — treat every "ClientThink_real" below as "ClientSpawn".

# ClientThink_real — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x0001d27b`  
**Stack frame:** `enter 0x5b0` (1456 bytes — very large)  
**Called from:** **corrected** — `0x1cb0b` is `ClientRespawn` (not "ClientThink"), and it
calls `func_0x1c937` = `CopyToBodyQue` first (not "cmd fetch"), then this function. This
function is also called from `0x1d18b` = `ClientBegin` (confirmed via debug strings).
**These are `0x1d27b`'s only two callers in the entire QVM** — see the open identity
question in [ClientRespawn.md](ClientRespawn.md#open-question-what-is-0x1d27b) before
trusting the "ClientThink_real" name below; it may actually be `ClientSpawn`
(DeFRaG-extended to run an initial physics tick), or a genuine merge of the two.  
**Ends at:** `0x1d6db` (`leave 0x5b0`)

---

## Important: what is NOT in this function

The `G_TouchTriggers` and `ClientEvents` calls documented in the old analysis are **not** inside ClientThink_real. ClientThink_real ends cleanly at `0x1d6db`.

**Correction (later pass, see [`../G_UTILS/README.md`](../G_UTILS/README.md)):**
the claim that the next function (`0x1d6dc`) is "a thin wrapper that calls
`G_TouchTriggers(0x1bda5)`" is **wrong and has been directly disproven**.
`0x1d6dc` does call `0x1bda5` unconditionally first (confirmed, matches the
original claim about the call site itself), but `0x1bda5` itself was
independently decoded this pass and is **not** `G_TouchTriggers`.

**Identity resolved (same session, cross-referenced from
[`../G_CLIENT/README.md`](../G_CLIENT/README.md)): `0x1d6dc` is
`ClientDisconnect`.** That module's own README had already identified it
(address match confirmed exactly) and, independently, `0x1bda5` is
`G_RemoveQueuedBotBegin` (per `G_CLIENT/README.md`'s own finding 6 — the
"16-entry queue clear" shape was correct, it's a pending-bot-begin queue,
not a trigger table), called from `ClientDisconnect` exactly where vanilla
calls it. The `0x273f` call (gated on `ent->client != NULL` in this pass's
own decode) is `ClientEvents`. The per-client bit manipulation against an
array at `0x12a2f0` (stride `0x37c`) found in this pass is plausibly the
"global max-speed-holder cleanup" and/or `0x13184c` 20-byte name-copy
behavior described in `G_CLIENT/README.md`'s findings 5 and 7 — not
individually re-matched instruction-for-instruction against that README's
claims in this pass. So `0x1d6dc` is **not** the
"`G_TouchTriggers`+`ClientEvents` wrapper" this doc previously assumed, but
its real purpose is now identified (`ClientDisconnect`), just not via this
doc. It has been removed from the `G_TouchTriggers` search. The real
generic `G_TouchTriggers` turned out to be `func_0x1a1b2` after all — see
[`../G_ACTIVE/func_0x1a1b2.md`](func_0x1a1b2.md) for the final resolution.

---

## Call chain

```
ClientBegin (0x1d18b) / ClientRespawn (0x1cb0b, after CopyToBodyQue)
    → this function (0x1d27b) — identity uncertain, see header
            → 0x2830 (client state manager)
            → [cmd acquisition: 0x44bc / 0x1c6be / 0x1c8c5 / 0x1c860 / 0x270ee]
            → [spawn block if needed]
                → 0x274b (entity setup)
                → 0x421b (per-frame ps flags)
                → 0x286ad (origin get)
                → trap -0x25 (unknown DeFRaG trap, spawning)
                → 0x1cab3 (unknown)
            → 0x28534 (conditional entity setup, non-spectator)
            → trap -0x1f (probably LinkEntity, non-spectator spawn)
            → [animation/time setup]
            → race mode branch:
                → if race active (0x12c698 != 0): 0x85a(ent) [race freeze] → skip to Pmove
                → else: 0x28151(client, ent) + [OBS check]
            → 0x1ac8f (Pmove wrapper → Pmove outer → PmoveSingle)
            → 0x5338 (ClientImpacts, non-spectator)
            → [save ent->oldOrigin, non-spectator]
            → trap -0x1f (LinkEntity, non-spectator)
            → 0x1adc9 (SpectatorThink, if sess.spectatorClient != 2)
            → 0x5338 (second ClientImpacts call)
```

---

## Section-by-section analysis

### §1 — Entry (0x1d27b–0x1d290)

```c
clientNum = (ent - g_entities) / sizeof(gentity_t);   // 0x1b2f0, 0x43c
client    = ent->client;                               // ent+0x204

call 0x2830(clientNum);   // client state manager — iterates internal client list
                          // at 0x2c1f0, checks session states, syncs spectator
```

`0x2830` is a DeFRaG-specific client state management function. Takes clientNum, walks a
linked structure at `0x2c1f0`, checking `pm_type` and session states. Purpose not fully
decoded but it doesn't affect movement physics.

---

### §2 — Spectator command path (0x1d291–0x1d2c5)

```c
if (client->sess.sessionTeam == 3) {         // TEAM_SPECTATOR
    // check global 0x12c6ec (spectator flags)
    if (*(0x12c6ec) != 0) {
        call 0x1c6be(client+0x14, &cmd, &oldcmd, pers.cmd.serverTime);  // alt cmd fetch
    } else {
        call 0x1c8c5(&cmd, &oldcmd);         // simpler cmd fetch for spectator
    }
    // store resulting cmd; jump to 0x1d3d0
}
```

---

### §3 — pm_type setup (0x1d2c6–0x1d31a)

```c
if (*(0x1b1ec) < 4) goto cmd_check;          // unknown level global gate

local_pmtype = client->sess.sessionTeam;
if (local_pmtype == 1) local_pmtype = 2;    // team 1 → PM_SPECTATOR(2)?
if (local_pmtype == 2) local_pmtype = 1;    // team 2 → PM_NOCLIP(1)?

// Command acquisition (several paths based on entity velocity):
speed = VectorLength(client->ps + 0x2a4);   // some velocity field
if (speed > 0.0f) {
    call 0x44bc(ent, &cmd, &oldcmd);         // GetCurrentCmd normal path
} else {
    call 0x8d63(client + 0x2a4);             // VectorLength again?
    if (result > 0.0f) {
        call 0x44bc(ent, &cmd, &oldcmd);     // GetCurrentCmd
    } else {
        // client+0x1f4 / client+0x1f0 flag checks → select further paths
        call 0x270ee(local_pmtype, 0, &cmd, &oldcmd);   // spectator cmd
        // or fall through to: ...
    }
}
```

The exact pm_type values for teams 1 and 2 are uncertain — DeFRaG may use a different
team numbering than stock Q3. Team 3 = TEAM_SPECTATOR is confirmed.

---

### §4 — Command merge / eFlags setup (0x1d3d0–0x1d3f5)

```c
client->field_0x22c = 1;                     // unknown client flag, set each frame

// Clear entity bits:
ent->s.eFlags &= ~0x201;                     // clear EF_NODRAW(0x200) and bit 0x1

// Save eFlags bits for later restore:
local80 = (client->ps.field_0x68 & 0x84004) ^ 0x4;
```

`client->ps+0x68` is accessed in oDFs as `ps->eFlags` (eFlags is at ps+0x68 in standard Q3).
The XOR with 0x4 toggles a specific eFlag bit for animation/model cycling.

---

### §5 — Spawn block: save state (0x1d3f6–0x1d4a2)

This block runs when the client needs (re)spawning, determined by flags at
`client+0x218` (bits 0x2000, 0x4000) and `ent+0x1a8` (bit 0x8).

```c
// --- Save fields that survive the memset ---
block_copy(local484, client + 0x1d4, 0xb0);  // save pers (0xb0 bytes)
block_copy(local534, client + 0x284, 0x38);  // save sess (0x38 bytes)
local56c = client->field_0x1c4;
local570 = client->field_0x320;
local574 = client->field_0x31c;

// First loop: save extended ps.stats (16 ints from ps+0xf8 = stats[16+])
for (i = 0; i < 16; i++)
    local20[i] = *(client + 0xf8 + i*4);     // save ps stats[16..31]

// --- Wipe the client struct ---
memset(client, 0, 0x37c);                     // zero entire gclient_t

// --- Restore preserved fields ---
block_copy(client + 0x1d4, local484, 0xb0);  // restore pers
block_copy(client + 0x284, local534, 0x38);  // restore sess
client->field_0x1c4 = local56c;
client->field_0x320 = local570;
client->field_0x31c = local574;
client->field_0x324 = -1;

// Second loop: restore extended stats
for (i = 0; i < 16; i++)
    *(client + 0xf8 + i*4) = local20[i];

// Restore field saved before memset:
client->field_0x6c = local578;

// Increment counter at client+0x108:
*(client + 0x108) += 1;

// Sync:
*(client + 0x104) = client->sess.sessionTeam; // copy sess to ps field
```

---

### §6 — Spawn block: entity/client init (0x1d4c2–0x1d56b)

```c
// Expire timer (12 seconds):
client->field_0x340 = level.time + 12000;   // 0x2ee0 ms

// Handicap from userinfo:
trap_GetUserInfo(clientNum, buf, 0x400);
handicap = atoi(Info_ValueForKey(buf, "handicap"));  // 0xac7d = InfoValueForKey, 0x5f67 = atoi
if (handicap < 1 || handicap > 100) handicap = 100;
client->field_0x224 = handicap;             // store clamped handicap
client->ps.field_0xd0  = handicap;          // ps+0xd0 = handicap (possibly STAT_MAX_HEALTH)

// Restore eFlags bits:
client->ps.field_0x68 = local80;            // ps.eFlags

// Entity setup:
ent->s.event      = 0x3ff;                  // clear event (ENTITYNUM_NONE sentinel?)
ent->client       = g_clients + clientNum * 0x37c;  // re-establish client ptr
ent->field_0x2e0  = 1;                      // entity in-use or similar flag
ent->inuse        = 1;                      // ent+0x208
ent->classname    = "player";               // ent+0x20c → data string 0x7dd8
ent->field_0x1cc  = 0x2000000;              // entity-side flags
ent->r.contents   = 0x2010001;             // CONTENTS_BODY|CONTENTS_SOLID (ent+0x23c)
ent->die          = 0x1fc5a;               // die function pointer (ent+0x2cc)
ent->watertype    = 0;                      // ent+0x314
ent->waterlevel   = 0;                      // ent+0x310
ent->field_0x218  = 0;                      // clear flags used in spawn check
ent->mins  = data[0x235c];                  // copy 12 bytes: player mins
ent->maxs  = data[0x2368];                  // copy 12 bytes: player maxs
client->ps.field_0x8c = clientNum;          // store clientNum in ps

call 0x274b(ent);                           // entity positioning/setup
call 0x421b(client);                        // per-frame DeFRaG flags (initial set)
call 0x286ad(ent, &local64);               // get origin (stores 12 bytes at local64)
block_copy(client->ps.origin, local64, 0xc); // ps.origin = spawn origin
```

---

### §7 — Spawn block: PMF_RESPAWNED + link (0x1d586–0x1d5cc)

```c
client->ps.pm_flags |= 0x200;               // PMF_RESPAWNED (ps+0x0c, client+0x0c)

// DeFRaG custom trap:
call trap_-0x25(clientNum, &client->pers.cmd);   // purpose unclear; spawn notification?

call 0x1cab3(ent, &oldcmd);                 // unknown; post-spawn cmd processing

if (client->sess.sessionTeam != 3) {        // non-spectator spawn path:
    if (*(0x132660) & 0x1 == 0)
        call 0x28534(ent);                  // conditional entity setup (unknown)
    call trap_-0x1f(ent);                   // LinkEntity (confirm position in BSP)
    client->ps.weapon = 2;                  // force initial weapon on spawn (ps+0x90)
}

client->ps.weaponstate = 0;                 // WEAPON_READY
```

`0x132660` is an unknown global flag. When bit 0x1 is clear, `0x28534` is called (likely
some additional entity initialization). After this, the entity is linked (trap -0x1f = probably
`trap_LinkEntity`).

---

### §8 — Animation / pm_time setup (0x1d5d9–0x1d611)

Runs for both spawn and non-spawn paths (most fields already set above, this fills in more):

```c
// ps.pm_flags |= PMF_DUCKED (0x40): forces crouch position on entry/respawn
client->ps.pm_flags |= 0x40;

client->ps.pm_time = 100;                    // pm_time = 0x64 (ps+0x10)
client->field_0x330 = level.time;            // client+0x330 = level.time
client->field_0x334 = level.time + 1000 * *(0x19ecc);  // expire time based on cvar

client->ps.eFlags &= ~0x2d0;               // clear eFlags bits 0x2d0 (via ps+0x2d0... actually ps.field)
client->ps.torsoAnim = 0xb;                 // TORSO_STAND (ps+0x54)
client->ps.legsAnim  = 0x16;               // LEGS_IDLE   (ps+0x4c)
```

**Note:** 0x19ecc is an unknown global multiplier; it appears to scale a time interval.

---

### §9 — Race mode branch (0x1d612–0x1d678)

```c
if (*(0x12c698) != 0) {                     // race mode active
    call 0x85a(ent);                         // race freeze handler:
    //   if spectatorClient == 2: call 0x1e360
    //   copy race start position from 0x12c6a8 → ent+0x5c and ps.origin
    //   copy race angles from 0x12c6b4 → ps+0x98
    //   ps.pm_type = 5 (PM_INTERMISSION/freeze)
    //   memset(ps+0x138, 0, 0x40)           // clear powerups area
    //   ps.eFlags = 0 (ps+0x68)
    //   ent->s.eFlags = 0 (ent+0x8)
    goto pre_pmove;                          // skip normal path → 0x1d679
}

// Non-race-mode path:
call 0x28151(client, ent);                   // spectator state check/update
client->ps.field_0x90 = 1;                  // possibly ps.weapon = WP_GAUNTLET (uncertain)

// OBS state logic (0x1d62d–0x1d678):
local5a4 = 1;                               // initial OBS bit value
if (*(0x1317bc) == local5a4) {
    goto pre_pmove;                          // 0x1317bc == 1: skip OBS check
}
// Scan bits in client->ps.field_0xc0 (= ps.pm_flags?) shifted by local5a4:
for (local18 = 10; local18 >= 0; local18--) {
    bit = 1 << local18;
    if (!(client->ps.field_0xc0 & bit)) continue;
    if (local18 != 10) {
        client->ps.field_0x90 = local18;    // store found bit index
        goto pre_pmove;
    }
}
// Decrement local18 until < 0, searching...
```

**Note:** `0x1317bc` is still uncertain — previous analysis suggested weapon selection
behavior rather than OBS. The exact meaning of the bit-scanning loop here needs further
tracing.  
**Note:** `0x12c698` = race mode global. `0x12c6a8` = race start position (vec3_t).
`0x12c6b4` = race start angles (vec3_t).

---

### §10 — Pre-Pmove (0x1d679–0x1d693)

```c
// label pre_pmove:
*client = level.time - 100;                 // ps.commandTime = level.time - 100
                                            // (client+0x00 = ps+0x00 = commandTime)
client->pers.cmd.serverTime = level.time;   // client+0x1d8 = pers.cmd.serverTime

clientNum = (ent - g_entities) / sizeof(gentity_t);
call 0x1ac8f(clientNum);                    // Pmove wrapper → Pmove outer → PmoveSingle
```

---

### §11 — Post-Pmove (0x1d694–0x1d6db)

```c
if (client->sess.sessionTeam != 3) {        // non-spectator
    call 0x5338(client, ent, 1);            // ClientImpacts equivalent
    // save origin: ent+0x1e8..0x1f4 = client->ps.origin (block_copy 0xc)
    call trap_-0x1f(ent);                   // LinkEntity (re-link after physics)
}

// Spectator/follow check:
if (client->sess.field_0x290 != 2) {        // field +0x290 from sess base:
    call 0x1adc9(ent);                      // SpectatorThink / spectator follow
}

// Final:
call 0x5338(client, ent, 1);               // second ClientImpacts call
leave                                       // 0x1d6db
```

`client->sess + 0x290 - 0x284 = +0xc` past sessionTeam; in Q3 layout this is
`sess.spectatorClient` (+0x290 from base of gclient). Value 2 = SPECTATOR_FOLLOW —
when following a player, SpectatorThink is skipped.

---

## Key address table

| Address | Description |
|---------|-------------|
| `0x1d27b` | Function entry (`enter 0x5b0`) |
| `0x1d28e` | Call 0x2830(clientNum) |
| `0x1d296` | sessionTeam == 3 → spectator cmd path |
| `0x1d2c9` | `*(0x1b1ec) < 4` gate |
| `0x1d2d1` | pm_type from sessionTeam |
| `0x1d2f8` | VectorLength check / cmd acquisition |
| `0x1d3d0` | Post-cmd merge: set client flag 0x22c, clear eFlags bits |
| `0x1d3f6` | Spawn block start: save pers/sess |
| `0x1d44f` | memset(client, 0, 0x37c) |
| `0x1d45c` | Restore pers |
| `0x1d4cb` | trap_GetUserInfo → handicap |
| `0x1d517` | ent->s.event = 0x3ff |
| `0x1d56f` | Call 0x274b(ent) |
| `0x1d575` | Call 0x421b(client) — DeFRaG flags init |
| `0x1d57d` | Call 0x286ad(ent, &local64) |
| `0x1d586` | ps.pm_flags \|= PMF_RESPAWNED (0x200) |
| `0x1d5a4` | Call trap -0x25 |
| `0x1d5ad` | Call 0x1cab3(ent, &cmd) |
| `0x1d5c7` | Call trap -0x1f (LinkEntity, spawn path) |
| `0x1d5d9` | pm_flags \|= PMF_DUCKED; pm_time = 100 |
| `0x1d612` | Race mode check (0x12c698) |
| `0x1d619` | Race active: call 0x85a(ent), jump to pre_pmove |
| `0x1d624` | Call 0x28151(client, ent) (non-race path) |
| `0x1d630` | OBS state logic (0x1317bc check) |
| `0x1d679` | Pre-Pmove: commandTime = level.time - 100 |
| `0x1d691` | Call 0x1ac8f(clientNum) → Pmove |
| `0x1d6a7` | Call 0x5338 (ClientImpacts) |
| `0x1d6b8` | block_copy 0xc: save ent->oldOrigin |
| `0x1d6bc` | Call trap -0x1f (LinkEntity, post-Pmove) |
| `0x1d6cc` | Call 0x1adc9 (SpectatorThink) |
| `0x1d6d7` | Call 0x5338 (second impacts call) |
| `0x1d6db` | `leave 0x5b0` — function end |

---

## Known globals referenced

| Address | Description |
|---------|-------------|
| `0x1b1ec` | Unknown level global — gated with `< 4` at entry |
| `0x12a310` | `level.time` |
| `0x12c698` | Race mode flag (non-zero = race timer active) |
| `0x12c6a8` | Race start position (vec3_t) |
| `0x12c6b4` | Race start angles (vec3_t) |
| `0x12c6ec` | Spectator flags (checked in spectator cmd path) |
| `0x1317bc` | Controls OBS scan path — exact purpose uncertain |
| `0x132660` | Unknown global; bit 0x1 gates call to 0x28534 on spawn |
| `0x19ecc` | Unknown cvar/time multiplier |

---

## Known gclient offsets (new from this analysis)

| Offset | Field |
|--------|-------|
| client+0x0c | ps.pm_flags |
| client+0x6c | Unknown field (saved/restored across memset) |
| client+0x68 | ps.eFlags (ps+0x68 in standard Q3) |
| client+0x8c | ps.clientNum |
| client+0x90 | ps.weapon |
| client+0x94 | ps.weaponstate |
| client+0xd0 | ps stats field (handicap stored here) |
| client+0xf8 | ps.stats[16] start (DeFRaG extended stats, 16 entries) |
| client+0x104 | Unknown ps stats field — receives sess.sessionTeam copy |
| client+0x108 | Spawn counter (incremented each spawn) |
| client+0x1d4 | pers start; **pers.connected confirmed** (pers+0x0, int: CON_DISCONNECTED=0/CONNECTING=1/CONNECTED=2) — via [PickTeam.md](PickTeam.md)'s `TeamCount` helper, which checks `*(client+0x1d4) != 0` to skip disconnected clients; struct-layout-consistent since pers.cmd is confirmed at client+0x1d8 (=pers+0x4), and `connected` is `clientPersistant_t`'s first field in Q3 |
| client+0x1ec | Field within `pers.cmd` (pers+0x18, i.e. cmd+0x14) — **not** pers.connected (superseded, see above); exact cmd sub-field not identified |
| client+0x1f0 | Unknown pers flag (cmd acquisition gate) |
| client+0x1f4 | Unknown pers flag (cmd acquisition gate) |
| client+0x218 | Spawn condition flags (bits 0x2000, 0x4000) |
| client+0x22c | Unknown per-frame flag (set to 1 each frame) |
| client+0x224 | Handicap value (clamped 1–100) |
| client+0x290 | sess.spectatorClient (value 2 skips SpectatorThink) |
| client+0x320 | Unknown field (saved/restored across memset) |
| client+0x31c | Unknown field (saved/restored across memset) |
| client+0x324 | Unknown field (set to -1 after memset) |
| client+0x330 | Receives level.time each frame |
| client+0x334 | Expire timestamp (level.time + 1000 * cvar) |
| client+0x340 | 12-second expire timer (level.time + 12000) on spawn |

---

## oDFs comparison and differences

| Area | DeFRaG | oDFs |
|------|--------|------|
| Per-frame DeFRaG flags (stats[12]) | func_0x421b called during spawn | Implemented in g_active.c |
| Race mode freeze (0x85a) | Player teleported/frozen at start | No race mode — not implemented |
| trap -0x1f (LinkEntity) | Called post-spawn and post-Pmove | oDFs calls trap_LinkEntity at similar points |
| PMF_RESPAWNED set | `ps.pm_flags \|= 0x200` on spawn | Same in Q3/oDFs |
| ClientImpacts (0x5338) | Called twice: non-spectator + all | oDFs equivalent in ClientThink_real |
| SpectatorThink | Skipped if spectatorClient == 2 | Standard Q3 behaviour |
| memset gclient on spawn | Clears 0x37c bytes, preserves pers+sess | Similar in oDFs ClientSpawn |

---

## Status

**Substantially analyzed.** All major sections are now documented. Remaining uncertainties:

- Exact purpose of `0x2830` (client state manager) — DeFRaG-specific, no movement impact
- Exact pm_type mapping for sessionTeam 1/2 — unlikely to affect movement
- `0x1317bc` OBS bit-scan logic — needs further trace; currently marked uncertain
- Trap -0x1f identity (probably `trap_LinkEntity` but not 100% confirmed)
- Trap -0x25 purpose (DeFRaG custom trap)
- `0x274b` — ✅ Analyzed, see [../G_TRIGGER/func_0x274b.md](../G_TRIGGER/func_0x274b.md). It is DeFRaG's spawn-time weapon/ammo/health/armor loadout setup (analogue of vanilla `ClientSpawn`'s ammo-init loop) — **not** a race-HUD function, correcting oDFs's `g_defrag.c` header comment which claims `G_UpdateRacePs` "mirrors" it (oDFs's `G_UpdateRacePs` is called every frame; `0x274b` is called once per spawn).
- Functions 0x286ad, 0x28534, 0x1cab3 — spawn helpers, not yet analyzed
- `0x19ecc` cvar multiplier identity
