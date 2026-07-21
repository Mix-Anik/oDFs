# Race Timer Entity System — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

---

## Clarification: df_ndm_* are cvars, not classnames

`df_ndm_timer_start`, `df_ndm_timer_checkpoint`, `df_ndm_timer_stop` are **cvars**
registered during game init and used to detect whether the corresponding entities exist
on the loaded map. They are NOT entity classnames.

The actual entity classnames used in Radiant and visible in `.map` files are:
- `target_startTimer`
- `target_stopTimer`
- `target_checkpoint`

These are point entities. Mappers place a `trigger_multiple` brush and set its `target`
key to point at one or more of these. When a player enters the brush, `trigger_multiple`
calls `G_UseTargets` → each target's `use` callback fires with the player as activator.

---

## Spawn table entries

Located in the data segment spawn table starting near `0x2700`:

| Data addr | Classname string | Spawn function |
|-----------|-----------------|----------------|
| `0x270c` | `0x95f6` = `"target_startTimer"` | `0x3a13` |
| `0x2714` | `0x95e5` = `"target_stopTimer"` | `0x3a1f` |
| `0x271c` | `0x95d3` = `"target_checkpoint"` | `0x3a28` |

---

## Spawn functions

### `SP_target_startTimer` — `0x3a13`

```c
void SP_target_startTimer(gentity_t *ent) {
    ent->use       = use_startTimer;    // 0x39c5  stored at ent+0x2c4
    *(0x1318d0)    = 1;                 // g_hasStartTimer flag
}
```

Sets the use callback and records that at least one start timer entity exists.

### `SP_target_stopTimer` — `0x3a1f`

```c
void SP_target_stopTimer(gentity_t *ent) {
    ent->use = use_stopTimer;           // 0x39dd  stored at ent+0x2c4
}
```

Only sets the use callback. No global flag written at spawn time (the ready flag
`0x131818` is set by the init function `0x1e60` separately).

### `SP_target_checkpoint` — `0x3a28`

```c
void SP_target_checkpoint(gentity_t *ent) {
    ent->use     = use_checkpoint;      // 0x39f5  stored at ent+0x2c4
    int idx      = *(0x1318d8)++;       // auto-increment checkpoint counter
    ent->count   = idx;                 // ent+0x2f8 = ent->count = checkpoint index
}
```

Each checkpoint entity gets a unique 0-based index stored in `ent->count` (byte offset
`0x2f8` from entity base, matching Q3/oDFs `gentity_t::count`).

---

## USE callbacks

Called as `use(self, other, activator)` when the trigger fires.  
`activator` = the player entity that entered the trigger volume.

All three callbacks share the same guard pattern:
1. Check their respective ready flag — if not 1, return early.
2. Check `activator->client != NULL` — spectators / non-clients skip.
3. Call the core timer function.

### `use_startTimer` — `0x39c5`

```c
void use_startTimer(gentity_t *self, gentity_t *other, gentity_t *activator) {
    if (*(0x131808) != 1) return;   // startTimer ready flag
    if (!activator->client)  return;
    StartRace(activator);           // 0x3933
}
```

### `use_stopTimer` — `0x39dd`

```c
void use_stopTimer(gentity_t *self, gentity_t *other, gentity_t *activator) {
    if (*(0x131818) != 1) return;   // stopTimer ready flag
    if (!activator->client)  return;
    StopRace(activator);            // 0x3958
}
```

### `use_checkpoint` — `0x39f5`

```c
void use_checkpoint(gentity_t *self, gentity_t *other, gentity_t *activator) {
    if (*(0x131810) != 1) return;   // checkpoint ready flag
    if (!activator->client)  return;
    Checkpoint(activator, self->count);  // 0x3978, passes checkpoint index
}
```

---

## Core timer functions

### `GetTimerRecord` — `0x3588`

```c
raceTimer_t *GetTimerRecord(gclient_t *client) {
    int clientNum = (client - g_clients) / sizeof(gclient_t);  // stride 0x37c
    return &g_raceTimers[clientNum];  // base 0x1318dc, 32 bytes per slot
}
```

Per-player timer records are stored in a global array at `0x1318dc`, 32 bytes per client,
indexed by clientNum. This array is separate from gclient_t.

### `StartRace` — `0x3933`

```c
void StartRace(gentity_t *ent) {
    raceTimer_t *t = GetTimerRecord(ent->client);
    // Allow (re)start only if never used (initialized==0) OR timer already ≥ 750ms.
    // The 750ms gate prevents re-entering the start zone too quickly.
    if (t->initialized == 0 || t->ms >= 750) {
        t->state = TIMER_RUNNING;   // 1
    }
}
```

### `StopRace` — `0x3958`

```c
void StopRace(gentity_t *ent) {
    raceTimer_t *t = GetTimerRecord(ent->client);
    if (t->initialized != 0) {
        t->state = TIMER_FINISHED;  // 2
    }
}
```

### `Checkpoint` — `0x3978`

```c
void Checkpoint(gentity_t *ent, int idx) {
    raceTimer_t *t = GetTimerRecord(ent->client);
    if (t->initialized == 0) return;
    if (t->checkpointBits & (1 << idx)) return;  // already hit
    t->checkpointBits |= (1 << idx);
    t->checkpointCount++;
    t->state = TIMER_CHECKPOINT;    // 3
}
```

---

## Per-player timer record — `raceTimer_t`

Base address `0x1318dc`, stride 32 bytes (= `clientNum * 32`).  
`func_0x3a5f` clears one client's record (called from `func_0x1b57`, not every frame).  
Per-frame advancement by `func_0x37c7` — see [func_0x37c7.md](func_0x37c7.md).

| Offset | Type | Field | Notes |
|--------|------|-------|-------|
| +0x00 | int | `ms` | Elapsed ms. `StartRace` checks `ms >= 750` to allow re-entry into start zone |
| +0x04 | int | `bestTime` | Best/current result — `ms` + rounded `fracAccum` |
| +0x08 | float | `fracAccum` | Sub-ms fractional accumulator (timing precision) |
| +0x0c | int | `state` | 0=IDLE, 1=RUNNING, 2=FINISHED, 3=CHECKPOINT |
| +0x10 | float | `fracDelta` | Per-frame fraction added to `fracAccum` |
| +0x14 | int | `initialized` | 0=not yet active; 1=live (set on first RUNNING tick by func_0x37c7) |
| +0x18 | int | `checkpointBits` | Bitmask — bit N = checkpoint N reached |
| +0x1c | int | `checkpointCount` | Number of distinct checkpoints reached |

---

## Race state globals

| Address | Description |
|---------|-------------|
| `0x1318d0` | `g_hasStartTimer` — set to 1 in `SP_target_startTimer`; presence indicator |
| `0x1318d8` | `g_checkpointCount` — next available checkpoint index (auto-incremented) |
| `0x1318dc` | Base of `g_raceTimers[]` — 32 bytes × MAX\_CLIENTS |
| `0x131808` | startTimer **ready flag** — must be 1 for `use_startTimer` to call `StartRace` |
| `0x131810` | checkpoint **ready flag** — must be 1 for `use_checkpoint` to call `Checkpoint` |
| `0x131818` | stopTimer **ready flag** — must be 1 for `use_stopTimer` to call `StopRace` |
| `0x12c698` | Race mode flag — non-zero while race is active; read in ClientThink_real / func_0x85a |
| `0x12c6a8` | Race start position (vec3\_t) |
| `0x12c6b4` | Race start angles (vec3\_t) |
| `0x131800` | stopTimer entity tracking value (see func_0x2919) |
| `0x131804` | stopTimer last-fire timestamp (= level.time + 200) |

---

## How the ready flags get set — `func_0x1e60`

The ready flags at 0x131808/0x131810/0x131818 are **not** set in the spawn functions.
They are set by an init function at `0x1e60` called from game init (`0x21fc`).
`0x1e60` reads the `df_ndm_timer_*` cvars (whose values reflect entity presence via
cvar registration during spawn) and sets the flags accordingly.

See [func_0x1e60.md](func_0x1e60.md) for full analysis.

---

## ent->count = checkpoint index

In DeFRaG's QVM, `SP_target_checkpoint` writes the checkpoint index to `ent+0x2f8`.
In oDFs's `gentity_t`, offset `0x2f8` corresponds to the standard Q3 field `count`
(confirmed by the field layout: health=0x2dc, takedamage=0x2e0, damage=0x2e4,
splashDamage=0x2e8, splashRadius=0x2ec, methodOfDeath=0x2f0, splashMethodOfDeath=0x2f4,
count=0x2f8). No new struct field is needed.

---

## oDFs implementation plan

### Files to modify

| File | Change |
|------|--------|
| `game/g_spawn.c` | Forward-declare three new SP\_ functions; add three entries to `spawns[]` |
| `game/g_trigger.c` | Add spawn functions, USE callbacks, `StartRace`, `StopRace`, `Checkpoint`, `GetTimerRecord` |
| `game/g_local.h` | Declare `raceTimer_t`, `g_raceTimers[]`, ready flags, `g_checkpointCount` |
| `game/g_main.c` | Define the globals (initial values 0) |

### New type

```c
typedef enum {
    TIMER_IDLE        = 0,
    TIMER_RUNNING     = 1,
    TIMER_FINISHED    = 2,
    TIMER_CHECKPOINT  = 3,
} timerState_t;

// oDFs simplification: DeFRaG accumulates ms via per-subdivision elapsed_ms
// (accurate but complex). oDFs instead stores level.time at race start and
// computes elapsed as (level.time - startTime) on demand, which is accurate
// since level.time advances at real game speed regardless of pmove_msec.
typedef struct {
    int          startTime;       // level.time when race began
    int          bestTime;        // best/last result (level.time delta)
    float        fracAccum;       // unused (DeFRaG sub-ms field)
    timerState_t state;
    float        fracDelta;       // unused (DeFRaG sub-ms field)
    int          initialized;     // 1 = timer is live
    int          checkpointBits;  // bitmask of reached checkpoints
    int          checkpointCount;
} raceTimer_t;
```

### Globals

```c
raceTimer_t g_raceTimers[MAX_CLIENTS];   // per-player timer records
int         g_hasStartTimer;             // 1 if target_startTimer present
int         g_checkpointCount;           // next checkpoint index
qboolean    g_startTimerReady;           // guards use_startTimer
qboolean    g_stopTimerReady;            // guards use_stopTimer
qboolean    g_checkpointReady;           // guards use_checkpoint
```

---

## Still unknown / needs analysis

| Item | Notes |
|------|-------|
| `func_0x1e60` — race init, sets ready flags | ✅ Analyzed — see [func_0x1e60.md](func_0x1e60.md) |
| `func_0x1a44a` — previously misidentified as race timer | ✅ Corrected — it is the inactivity kick handler; see [../G_ACTIVE/func_0x1a44a.md](../G_ACTIVE/func_0x1a44a.md) |
| Per-frame timer tick | ✅ Found — `func_0x37c7` called from Pmove outer §14; see [func_0x37c7.md](func_0x37c7.md) |
| `func_0x85a` — freeze player at race start | Partially analyzed in ClientThink_real.md |
| `func_0x2919` — stopTimer entity lookup | Partially analyzed |
| `raceTimer_t` fields at +0x04, +0x08, +0x10 | Purpose unknown |
| What `0x3a5f` (called from ClientEvents) clears | Unknown — partial or full clear of raceTimer_t? |
| How `0x12c698` gets set during gameplay | Unknown — set at 0x946 on init, cleared at reset |
