# func_0x1a1b2 — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x0001a1b2`  
**Size:** 483 instructions  
**Stack frame:** `enter 0x10a8` (4264 bytes — large; holds entity list buffer + string buffers)  
**Called from:** `0x1a866` (Pmove outer section 13), for alive non-noclip players only

**Identified as:** DeFRaG per-frame **touch trigger handler** — detects when a player enters start/finish/checkpoint zone triggers and calls their touch functions.

---

## Guards

```
if !ent->client: return
if ps.stats[STAT_HEALTH] (client+0xb8) <= 0: return   ; dead players skip
```

Only alive players with a valid client trigger races.

---

## Structure

### 1. Compute broadphase AABB (0x1a1ce–0x1a21d)

Reference vec3 at `0x232c–0x2334` = **{40.0, 40.0, 52.0}** (pre-initialized data segment constant).

```c
mins[0] = ps.origin[0] - 40.0
mins[1] = ps.origin[1] - 40.0
mins[2] = ps.origin[2] - 52.0
maxs[0] = ps.origin[0] + 40.0
maxs[1] = ps.origin[1] + 40.0
maxs[2] = ps.origin[2] + 52.0
```

These are broadphase half-extents — larger than the player's actual AABB to avoid missing edge cases. Exact per-entity overlap is confirmed by a separate test.

### 2. EntitiesInBox query (0x1a21e–0x1a229)

```c
count = trap_EntitiesInBox(mins, maxs, entity_list[0x400], 0x400)
// trap call: -0x21
```

Returns all entities within the broadphase box into `entity_list` (up to 0x400 = 1024 entries).

### 3. Compute extended AABB for per-entity tests (0x1a22a–0x1a29d)

Computes expanded AABB using ent+0x1b4/0x1b8/0x1bc (player movement bounds) and ent+0x1c0/0x1c4/0x1c8/0x1cc:
```c
// expanded mins/maxs incorporating player bounds from last Pmove
extended_mins = ps.origin + ent->field_0x1b4
extended_maxs = ps.origin + ent->field_0x1c0
```

These fields (0x1b4–0x1cc) are written back by Pmove outer after physics.

### 4. Entity loop (0x1a29f–0x1a369)

Loop: `for (i = 0; i < count; i++)`

For each entity in the list:

```c
trigger = entity_list[i] * sizeof(gentity_t) + g_entities;
```

**Skip conditions:**
```c
if trigger->field_0x2c0 == 0: continue          ; touch function null
if ent->field_0x2c0 == 0: continue              ; player touch null (but checked separately)
if trigger+0x1cc & 0x40000000: continue         ; entity type flag — excludes non-race entities
if client->sess.sessionTeam (client+0x284) == 3: continue  ; spectators skip
if trigger->s.eType (trigger+0x4) == 9: continue ; exclude type 9 entities
if trigger+0x2c0 == 0x23ad9: continue           ; exclude specific function pointer
```

**Exact overlap test:**
```c
if trigger->s.eType == 2:
    touching = call 0x4bc8(client, trigger, level.time)   ; custom DeFRaG overlap check
else:
    touching = trap_EntityContact(mins, maxs, trigger)    // trap -0x22
if !touching: continue
```

**Touch dispatch:**
```c
// zero-initialize a trace_t (56 bytes) on the stack
memset(&trace, 0, 0x38);

// call the trigger's touch function
if trigger->touch (trigger+0x2c0) != NULL:
    trigger->touch(trigger, ent, &trace)

// call the player's touch function (if player entity has one AND has active flag)
if ent->field_0x1a8 & 0x8 && ent->touch (ent+0x2c0) != NULL:
    ent->touch(ent, trigger, &trace)
```

---

## Key constants

| Value | Meaning |
|-------|---------|
| `{40.0, 40.0, 52.0}` | AABB broadphase half-extents (data at 0x232c–0x2334) |
| `0x40000000` | Entity type flag bit — if set, entity is excluded from race trigger check |
| `0x23ad9` | Specific touch function address excluded from race triggers |
| `0x400` | Max entities returned by EntitiesInBox |
| `-0x21` | trap_EntitiesInBox |
| `-0x22` | trap_EntityContact (mins, maxs, entity) → bool |
| `0x4bc8` | DeFRaG custom overlap check (client, trigger, level.time) |

---

## gentity_t field offsets confirmed

| Offset | Field |
|--------|-------|
| `ent+0x4` | s.eType (entity state type) |
| `ent+0x1a8` | some flags field (bit 0x8 = player has active touch) |
| `ent+0x1cc` | entity flags (bit 0x40000000 = excluded from race trigger check) |
| `ent+0x2c0` | `touch` function pointer (called as `touch(self, other, trace*)`) |
| `client+0x284` | sess.sessionTeam (3 = TEAM_SPECTATOR) |

---

## oDFs comparison

oDFs has no race trigger system. DeFRaG's race timing (start/finish/checkpoint) is handled entirely through this function's touch dispatch chain — when a player enters a trigger zone, `trigger->touch()` handles the timer logic.

`G_TouchTriggers` in oDFs (called from `ClientThink_real`) handles standard Q3 triggers (items, teleporters). DeFRaG's 0x1a1b2 is a separate, per-physics-frame handler specifically for race triggers — this is why it runs inside `0x1a866` (Pmove outer) rather than in `ClientThink_real`.

**No code changes required** — this is DeFRaG-specific game content not related to physics.
