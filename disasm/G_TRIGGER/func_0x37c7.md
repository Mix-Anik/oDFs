# func_0x37c7 — Per-frame race timer tick

**Address:** `0x37c7`  
**Stack frame:** `enter 0x2c`  
**Called from:** Pmove outer (`0x1a866`) §14 post-effects, once per frame per client  
**Args:** `(gentity_t *ent, int elapsed_ms)`  
**Returns:** void

---

## Purpose

Advances the per-player race timer by `elapsed_ms` each frame and drives state
transitions. This is the only function that modifies `raceTimer_t.ms`.

It also produces HUD feedback by XOR-toggling bits in `ps.stats[STAT_DEFRAG_FLAGS]`
when the timer state changes — this makes the HUD timer blink on start/checkpoint/finish.

---

## State machine

```
              ┌──────────────────────────────────────────────┐
              │                                              │
              ▼                                              │
         [state=0, initialized=0]                           │
              │                                              │
              │ StartRace() sets state=1                     │
              ▼                                              │
 first tick: [state=1, initialized=0]                        │
              │ → initialized=1, call StartNotify(0x3597)    │
              │ → stats[12] ^= 0x4  (start HUD flash)        │
              ▼                                              │
 running:  [state=1, initialized=1]                          │
              │ → ms += elapsed_ms each frame                 │
              │                                              │
        ┌─────┴─────┐                                       │
        │           │                                       │
        │ Checkpoint │ StopRace                             │
        ▼           ▼                                       │
  state=3        state=2                                    │
  ms += dt       ms += dt                                   │
  stats ^=0x10   clear initialized                          │
  state=0        record best time                           │
                 call FinishHandler(0x35ce)                 │
                 state=0  ──────────────────────────────────┘
```

---

## Pseudocode

```c
void TimerTick(gentity_t *ent, int elapsed_ms) {
    raceTimer_t *t = GetTimerRecord(ent->client);
    if (t == NULL) return;

    // If player dead or no client: abort race
    gclient_t *client = ent->client;
    if (client == NULL || client->ps.stats[STAT_HEALTH] < 1) {
        t->state = TIMER_IDLE;
        return;
    }

    if (t->initialized == 0) {
        // Timer not yet active — check if state was set to RUNNING
        goto check_state;
    }

    // Active timer: advance
    t->ms += elapsed_ms;

    if (t->state == TIMER_CHECKPOINT) {          // 3
        client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x10;   // blink
    }

    if (t->state == TIMER_FINISHED) {            // 2
        // Compute rounded best time
        t->bestTime = t->ms + round_half(t->fracAccum);  // +0x04
        t->initialized = 0;                              // reset
        // Some additional float accumulation:
        // t->field_0x04 += t->ms + roundhalf(t->field_0x08)
        client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x8;    // blink different bit
        call FinishHandler(ent);                         // 0x35ce — print time, record
    }

check_state:
    if (t->state == TIMER_RUNNING) {             // 1
        StartNotify(ent);                        // 0x3597 — "race started" notification
        t->initialized = 1;
        t->fracAccum += t->fracDelta;            // accumulate sub-ms fractions
        client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x4;    // blink start
    } else {
        // state != RUNNING and not active: compute accumulated time, clear state
        t->field_0x04 = t->ms + roundhalf(t->fracAccum);
        t->state = TIMER_IDLE;
    }
}
```

`round_half` = add 0.5f then truncate: `cvfi(t->fracAccum + 0x3f000000)`.  
`0x3f000000` as IEEE 754 float = 0.5f.

---

## Complete raceTimer_t layout (revised)

Base `0x1318dc`, stride 32 bytes per client (`clientNum * 32`).  
Cleared per-client by `func_0x3a5f` when called (from `func_0x1b57`, NOT every frame).

| Offset | Type | Field | Notes |
|--------|------|-------|-------|
| +0x00 | int | `ms` | Elapsed timer in milliseconds. Incremented by `elapsed_ms` each frame |
| +0x04 | int | `bestTime` | Best/current result. Computed as `ms + round_half(fracAccum)` |
| +0x08 | float | `fracAccum` | Sub-ms fractional accumulator (for timing accuracy) |
| +0x0c | int | `state` | 0=IDLE, 1=RUNNING, 2=FINISHED, 3=CHECKPOINT |
| +0x10 | float | `fracDelta` | Per-frame fractional delta added to fracAccum when running |
| +0x14 | int | `initialized` | 0 = not yet active; 1 = timer is live (set on first RUNNING tick) |
| +0x18 | int | `checkpointBits` | Bitmask — bit N = checkpoint N was hit |
| +0x1c | int | `checkpointCount` | Number of distinct checkpoints reached |

---

## Stats[12] bits toggled by this function

| Bit | Toggled when |
|-----|-------------|
| `0x4` | State transitions to RUNNING (start flash) |
| `0x8` | State is FINISHED (finish flash) |
| `0x10` | State is CHECKPOINT (checkpoint flash) |

These are XOR-toggled (not set) so the HUD blinks on/off each frame while the state
is active. They use bits in `STAT_DEFRAG_FLAGS` (stats[12]).

---

## Related functions

| Address | Role |
|---------|------|
| `0x35ce` | `FinishHandler(ent)` — prints "%s reached the finish line in %s", records best time, broadcasts result |
| `0x3597` | `StartNotify(ent)` — "race started" notification to player |
| `0x3588` | `GetTimerRecord(client)` — returns `&g_raceTimers[clientNum]` |
