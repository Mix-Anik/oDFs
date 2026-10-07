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

**Superseded — see the "Correction" section below and the pseudocode above.**
The diagram below (from the earlier pass) is kept for history but is
**inaccurate**: it implies a distinct "first tick" path that does not exist in
the bytecode (the RUNNING branch runs identically every frame `state==1`), and
it omits that `0x3597` zeroes the whole `raceTimer_t` (not just a
"StartNotify" call), and that the terminal `state=0` branch also zeroes
`fracDelta` and never touches `ps.stats[12]`.

```
              ┌──────────────────────────────────────────────┐
              │                                              │
              ▼                                              │
         [state=0, initialized=0]                           │
              │                                              │
              │ StartRace() sets state=1                     │
              ▼                                              │
 every RUNNING tick: [state=1]                                │
              │ → call 0x3597 (zeroes ms/bestTime/fracAccum/  │
              │   initialized/checkpointBits/checkpointCount, │
              │   preserves state+fracDelta)                  │
              │ → initialized=1, fracAccum += fracDelta       │
              │ → stats[12] ^= 0x4  (start HUD flash)         │
              ▼                                              │
        ┌─────┴─────┐                                       │
        │           │                                       │
        │ Checkpoint │ StopRace                             │
        ▼           ▼                                       │
  state=3        state=2                                    │
  ms += dt       ms += dt                                   │
  stats ^=0x10   stats ^=0x8, fracAccum -= fracDelta,        │
  (state stays   clear initialized, record bestTime,         │
   3 until the   call FinishHandler(0x35ce)                  │
   RUNNING check │
   below resets  │
   it to 0)      │
                 ▼
        [falls into "not RUNNING" branch next: state=0,
         fracDelta=0, bestTime recomputed, stats[12] untouched]
```

---

## Pseudocode

**This section was re-derived instruction-by-instruction from the raw
disassembly in a 2026 verification pass and corrects several details of the
previous version (kept below only for the diff — trust this version).**

```c
void TimerTick(gentity_t *ent, int elapsed_ms) {
    raceTimer_t *t = GetTimerRecord(ent->client);
    if (t == NULL) return;

    gclient_t *client = ent->client;
    // NOTE: raw bytecode checks `client == 0` OR `stats[HEALTH] >= 1` to decide
    // the "alive" path; net effect is identical to "if client==NULL or health<1: abort".
    if (client == NULL || client->ps.stats[STAT_HEALTH] < 1) {
        t->state = TIMER_IDLE;      // t+0x0c
        return;
    }

    if (t->initialized != 0) {                    // +0x14
        t->ms += elapsed_ms;                       // +0x00, unconditional while initialized

        if (t->state == TIMER_CHECKPOINT) {        // 3
            client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x10;   // ps+0xe8, confirmed
        }

        if (t->state == TIMER_FINISHED) {          // 2
            client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x8;    // confirmed — toggled BEFORE the bestTime calc below
            t->fracAccum -= t->fracDelta;           // +0x08 -= +0x10 — CONFIRMED, was missing from prior pseudocode entirely
            t->initialized = 0;
            t->bestTime = t->ms + round_half(t->fracAccum);  // uses fracAccum AFTER the subtraction above
            FinishHandler(ent);                      // 0x35ce
        }
    }
    // (if t->initialized==0, falls straight through to the RUNNING check below —
    //  there is no separate "goto check_state" label in the bytecode, it's simply
    //  the natural fallthrough of the `if (initialized != 0) { ... }` block)

    if (t->state == TIMER_RUNNING) {               // 1 — evaluated every frame state==RUNNING, not just "first tick"
        Reset_0x3597(ent);                          // 0x3597 — see correction below; NOT "StartNotify"
        t->initialized = 1;
        t->fracAccum += t->fracDelta;               // fracAccum was just zeroed by Reset_0x3597, so this sets fracAccum = fracDelta
        client->ps.stats[STAT_DEFRAG_FLAGS] ^= 0x4;      // confirmed
    } else {
        t->bestTime = t->ms + round_half(t->fracAccum);  // confirmed, no fracAccum adjustment here (unlike FINISHED branch)
        t->state    = TIMER_IDLE;                    // +0x0c = 0
        t->fracDelta = 0;                            // +0x10 = 0 — CONFIRMED, missing from prior pseudocode
        // NOTE: this branch does NOT touch ps.stats[STAT_DEFRAG_FLAGS] at all —
        // bit 0x4 is left however the last XOR toggle in the RUNNING branch left it.
    }
}
```

`round_half` = add 0.5f then truncate: `cvfi(t->fracAccum + 0x3f000000)`.  
`0x3f000000` as IEEE 754 float = 0.5f. Confirmed unchanged.

---

## Correction: `0x3597` is NOT "StartNotify"

The prior version of this document (and `race_timer_system.md`) labeled `0x3597`
as `StartNotify(ent)` — "race started notification". Re-reading `0x3597`'s own
disassembly (enter `0x24`) shows it does the following, confirmed:

```c
void func_0x3597(gentity_t *ent) {           // called from 0x274b AND from 0x37c7
    raceTimer_t *t = GetTimerRecord(ent->client);
    int   savedState     = t->state;          // +0x0c
    float savedFracDelta = t->fracDelta;       // +0x10
    trap_MemSet(t, 0, 0x20);                   // trap -0x65 — same trap id confirmed
                                                // elsewhere as memset (ClientThink_real
                                                // spawn block: memset(client,0,0x37c))
    t->state     = savedState;
    t->fracDelta = savedFracDelta;
}
```

I.e. it **zeroes the entire 32-byte `raceTimer_t`** (`ms`, `bestTime`,
`fracAccum`, `initialized`, `checkpointBits`, `checkpointCount`), preserving
only `state` and `fracDelta` across the wipe. There is no string, no
`trap_SendServerCommand`/print call, nothing resembling a "notification" — the
"StartNotify" label was an unverified guess in the earlier pass. Renamed here
to `Reset_0x3597` pending a better name.

**This is called from `func_0x37c7`'s RUNNING branch on *every* frame that
`state == RUNNING`is true — not only "the first tick"**, since nothing in the
control flow distinguishes first-frame-of-running from any later frame (the
prior doc's `check_state:`-with-a-first-tick-special-case framing does not
exist as a distinct code path in the bytecode; it's simply what "state==RUNNING
happens to still be true and initialized was just reset to 0 by the same
function moments earlier" looks like from the outside).

**Open question, explicitly flagged as unresolved (not speculated on further):**
taken literally, this means `t->ms`/`t->checkpointBits`/`t->checkpointCount`
are wiped to 0 by this function immediately after being read/used earlier in
the very same `TimerTick` call, and every frame thereafter — so these fields
never appear to accumulate across multiple frames in the way a millisecond
race-timer counter normally would. Whether this is intentional (e.g. the
*actual* authoritative elapsed time is tracked entirely client-side from the
bit-toggle events, and `ms`/`bestTime` here are a vestigial/unused
accumulator), a per-subframe reset that's compensated for elsewhere not yet
found, or something misread in this pass, is **not determined**. This does not
change the verdict on oDFs's `G_TimerTick`, since oDFs does not replicate
`ms`/`fracAccum`/`fracDelta`/`checkpointBits`-reset semantics at all — it uses
its own `level.time`-based elapsed computation (an already-documented,
intentional design substitution) and never calls anything analogous to
`0x3597`.

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
| `0x4` | Every frame `state == RUNNING` (confirmed — not just on transition) |
| `0x8` | `state == FINISHED`, toggled once (state is cleared to IDLE right after) |
| `0x10` | `state == CHECKPOINT`, toggled once per tick it's seen |

These are XOR-toggled (not set) so the HUD blinks on/off each frame while the state
is active. They use bits in `STAT_DEFRAG_FLAGS` (stats[12], confirmed offset
`ps+0xe8`). **Confirmed:** neither the FINISHED branch nor the final
"not-RUNNING" (`state=0`) branch ever clears bit `0x4` explicitly — once the
race stops, bit `0x4` is simply left at whatever the last XOR toggle in the
RUNNING branch set it to (it is a per-frame blink, not a level signal). oDFs's
`G_TimerTick` instead uses `|=` in the RUNNING branch (a deliberate,
self-documented deviation to get a stable "on" signal) and additionally does
`&= ~0x4` explicitly in both the FINISHED branch and the final `else` branch —
**this explicit clearing is not present in the DeFRaG bytecode and is an
oDFs-only addition**, confirmed via this pass.

**Do not treat this as a bug to fix.** A separate, earlier reverse-engineering
pass over the actual `cgame.qvm` disassembly (`disasm/CGAME/cgame_timer_hud.md`)
independently found that the real DeFRaG cgame detects *state changes* on this
bit, not its raw per-frame value, and explicitly notes: "XOR-toggling every
frame was wrong — it caused startTime to reset each snapshot (~every 8ms)."
oDFs's set-once/clear-once approach is the empirically-verified-correct
behavior for driving the actual client HUD, even though it diverges from the
literal qagame bytecode documented above. The qagame-side blink behavior is
confirmed accurately transcribed here; it is DeFRaG's server that appears to
rely on the client tolerating/compensating for the blink in a way not fully
understood, not an error in this document.

---

## Related functions

| Address | Role |
|---------|------|
| `0x35ce` | `FinishHandler(ent)` — not re-verified in this pass (out of scope: oDFs's `G_RaceFinished` was not part of this verification's 5 target functions); prior identification as "prints finish line, records/broadcasts result" not re-checked against raw bytecode here |
| `0x3597` | **Corrected** — NOT `StartNotify`. Confirmed to be `memset(&raceTimer_t, 0, 0x20)` preserving `state`+`fracDelta`; also called unconditionally from `0x274b` at spawn. See correction above. |
| `0x3588` | `GetTimerRecord(client)` — returns `&g_raceTimers[clientNum]`; re-confirmed via identical call pattern in `0x3933`/`0x3958`/`0x3978`/`0x3597`/`0x37c7`/`0x274b` |
