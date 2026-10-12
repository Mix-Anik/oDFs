# DeFRaG cgame.qvm — Race Timer HUD Architecture

Analyzed from `cgame.dis` (version approx 1.91.29/30 based on `pps_offset = 0xEA11C`).

---

## Entry point: `CG_Draw2D` at `0x2A12`

The DeFRaG-specific draw function starts at instruction `0x2A12` (enter 0x88).
The mdd cgame-proxy intercepts at:
- `cg_draw2d_defrag  = 0x2A36` (1.91.31) / `0x2A20` (1.91.29)
- `cg_draw2d_vanilla = 0x1CDC1` (1.91.31) / `0x1CDAB` (1.91.29)

---

## What `0xcfcec` is

A pointer stored in cgame VM data at `0xcfcec`. Its VALUE is a pointer to
`snapshot_t`. Adding `0x2C` gives `&snapshot->ps` (since `ps` starts at
`snapshot_t + 0x2C` in Q3's layout). Most ps reads in the draw function go
through `*(*(0xcfcec) + 0x2C + field_offset)`.

---

## Draw flow (simplified)

```
CG_Draw2D(0x2A12):
  1. local28 = *(0xcfcec) + 0x2c  ← pointer to snapshot ps
  2. if ps.persistant[14] (ps+0x130) == 3: skip all → 0x2fdb (no draw)
  3. if ps.persistant[14] == 2: mode-2 path
  4. Read cvars for display scale/alpha (via func_0x24ac)
  5. Call func_0x4f8c → check timer state array
  6. If timer state == 0: jump to 0x2ACB (no timer draw)
  7. ... timer and checkpoint draw ...
```

---

## Timer state gate: `func_0x4f8c`

```c
int func_0x4f8c(void) {
    return *(int*)(0x11d5b0 + *(0x11d630) * 4);
}
```

- `0x11d5b0` = base of timer state int array (indexed by stage/route index)
- `*(0x11d630)` = current stage index
- Returns 0 → no timer draw; non-zero → timer active

---

## Timer state array population: `func_0x4e9c`

Called as `func_0x4e9c(cvar_name_addr, &out_val)`:

```c
void func_0x4e9c(int vm_cvar_name_ptr, int* out) {
    char buf[0x84];
    trap_CvarVariableStringBuffer(vm_cvar_name_ptr, buf, 0x84);
    // also calls trap_GetConfigstring(arg1, ...) for configstring data
    // parses buf → fills timer state globals at arg0 and nearby addresses
}
```

Called multiple times with different first arguments (spaced 0x84 bytes apart):
- `0x11d5b0` — start timer data / buffer
- `0x11d634` — stop timer data / buffer (= 0x11d5b0 + 0x84)
- `0x11d6b8` — checkpoint data / buffer (= 0x11d634 + 0x84)

**These three buffers correspond to `df_ndm_timer_start`, `df_ndm_timer_stop`,
`df_ndm_timer_checkpoint` cvars** read from SERVERINFO. If the value starts with
`"df"`, the timer state at that slot is set to 1 (active).

---

## Draw guard: `*(0x11b000)`

In the draw function `0x1B123` (timer widget draw), early exit if `*(0x11b000) == 0`.

`0x11b000` is set to 1 by `func_0x3c33` which:
1. Reads `defrag_vers` cvar string from SERVERINFO
2. If non-empty: `*(arg0) = 1` → where arg0 = `0x11d298` (passed from caller)
3. Separately, `0x11b000` must also be set → exactly what sets it is unclear but
   it appears set from SERVERINFO cvar detection (same mechanism as `0x11d298`)

**In oDFs**: `defrag_vers = ODFS_VERSION` is registered as CVAR_SERVERINFO,
so `*(0x11d298) = 1`. This should be sufficient for that guard.

---

## Timer ms — client-side computation in `func_0x259a`

The cgame computes elapsed time LOCALLY using:

```c
// Each frame while timer state == RUNNING:
elapsed = base + rate * (500 * (ps.commandTime - startTime) / 1000)
```

Where:
- `ps.commandTime` = `*(0xea11c)` (the first field of predicted playerState at pps_offset)
- `startTime` = stored in timer struct at `0x11cdec + 0x38` (set when state transitions to RUNNING)
- `base` = previous accumulated elapsed (in timer struct at `+0x4c`)
- `rate` = initialized to -1 (no timer); set to some value when timer starts

The timer VALUE is computed entirely client-side from `ps.commandTime`.
The server communicates STATE (running/idle/checkpoint/finish) through `ps.stats[STAT_DEFRAG_FLAGS]` bits.

---

## ps.stats[STAT_DEFRAG_FLAGS] (stats[12]) bit usage by cgame

| Bit | What cgame does when it sees the bit change |
|-----|---------------------------------------------|
| `0x4` | Detects transition → records `startTime = ps.commandTime`; timer starts |
| `0x8` | Race finished → record final time |
| `0x10` | Checkpoint hit → record split time |

**Critical**: the cgame detects STATE CHANGES (transition), not the raw bit value.
Setting the bit ONCE when the timer starts is correct. XOR-toggling every frame
was wrong — it caused startTime to reset each snapshot (~every 8ms).

---

## What oDFs must set in server ps for the HUD to work

> **CORRECTION (later pass):** this section's `ps+0x..` offsets were taken straight from cgame addresses that are *snapshot-relative* (`snap = &snapshot_t`, `ps` starts at `snap+0x2c`, see `*(0xcfcec) + 0x2c` above). Re-based: `snap+0x17c/0x180/0x184` = `ps+0x150/0x154/0x158` = `ps.powerups[6/7/8]` (flight / **red flag / blue flag**), not `ammo[1..3]`; the `0x130` read (`persistant[14]`) is `ps+0x104`, etc. DeFRaG's own qagame never writes `ammo[1..10]`/`stats[2]`/`stats[3]` as signals (they are real ammo / weapons mask / armor there, see G_TRIGGER/func_0x274b.md, target_init.md). The real server inputs are the SERVERINFO cvars below, `stats[12]` event bits and (new) the race time in `stats[7]/[8]` (G_TRIGGER/func_0x38ba.md). oDFs `G_UpdateRacePs` now only publishes those; the old ammo/stats writes are behind `df_racePsSignal` (default 0).
> The table below is kept for history.

| Field | Value | Purpose |
|-------|-------|---------|
| `ps.ammo[1]` = ps+0x17c | `-1` | Sentinel (always) |
| `ps.ammo[2]` = ps+0x180 | `100` (race on), `200` (no race) | HUD visibility mode switch |
| `ps.ammo[3..10]` = ps+0x184..0x1a0 | weapon index or `200` | Weapon availability display |
| `ps.stats[2]` | `6` | Race mode config |
| `ps.stats[3]` | `0` (race on), `200` (no race) | Race active indicator |
| `ps.stats[STAT_DEFRAG_FLAGS]` bit `0x4` | SET while running, CLEAR when stopped | Timer running signal |
| `ps.stats[STAT_DEFRAG_FLAGS]` bit `0x8` | toggle once on finish | Finish event |
| `ps.stats[STAT_DEFRAG_FLAGS]` bit `0x10` | toggle once on checkpoint | Checkpoint event |

## What oDFs must set as SERVERINFO cvars

| Cvar | Value when race entities present | Purpose |
|------|----------------------------------|---------|
| `df_ndm_timer_start` | `"df"` | cgame reads → sets timer state array[start] = 1 |
| `df_ndm_timer_stop` | `"df"` | cgame reads → sets timer state array[stop] = 1 |
| `df_ndm_timer_checkpoint` | `"df"` | cgame reads → sets checkpoint slot = 1 |
| `defrag_route` | `"df-df-df"` (or similar) | Race layout config |
| `defrag_vers` | non-empty string | Sets `*(0x11d298)=1` guard flag |

---

## Open questions

1. Exactly what does `func_0x4e9c` read (trap -0x65 vs standard cvars) and how
   does it map cvar values to the timer state array values?
2. What sets `*(0x11b000)` non-zero? (currently unclear — possibly same as `0x11d298`)
3. What value does `rate` (timer struct `+0x3c`) need to be set to for interpolation?
4. Does the cgame also need a specific value in `ps.persistant[14]` (ps+0x130)?

---

## Authoritative event derivation: cgame `0x1c8b(out, snapOld, snapNew)` (decoded later; all offsets are `snapshot_t`-relative, `ps` = `snap+0x2c`)
Called from `0x29216` once per snapshot transition; fills the event word at `out+4` (global `0x11d280`, tested through `0x1ac7(mask)`) and passes `stats[12] & 2` (`snap+0x114`) as `out+0xc` to the timer-state update `0x5248`.
Events derived by comparing the two snapshots:

| Condition (old vs new) | Event bit |
|---|---|
| no old snapshot | `0x11` (first frame) |
| `ps.eFlags` (`snap+0x94`) bit `4` (teleport toggle) differs | `0x13`, and the `out+0x18` counter is cleared |
| `ps.persistant[14]` (`snap+0x130`) differs | `0x14` (interference/respawn-type state change) |
| `ps.persistant[15]` (`snap+0x134`) differs | `0x10` |
| `ps.stats[12]` (`snap+0x114`) bit `0x1` differs (**target_init toggle**) | `0x20` |
| `ps.stats[12]` bit `0x4` differs | `0x40` ("timer start / running changed") |
| `ps.stats[12]` bit `0x8` differs | `0x100` (finish) |
| `ps.stats[12]` bit `0x10` differs | `0x80` (checkpoint) |
| `snap+0xb8`-range stats differ (first stats compare at `0x1ce1`) | `0x800` |
All "differs" tests are XORs between the old and new snapshot, i.e. **any change of the bit (set or clear) raises the event** — which is why DeFRaG's server toggles `0x8`/`0x10`/`0x1` once per event, and why oDFs may SET bit `0x4` once on start but CLEARing it on stop raises a second `0x40` event (kept as is: the earlier note says it was verified against the cgame).
The result of the server's `stats[7]/[8]` time decode (`0x5fd7`) is compared against the best-time table in `0x5248` to add the record bits `0x200`/`0x400` (see G_TRIGGER/func_0x38ba.md).

### How the server side raises those events in DeFRaG (cross-reading with G_TRIGGER/func_0x37c7.md)
`TimerTick` XORs `stats[12]` bit `0x4` only inside its `state == TIMER_RUNNING` branch, and `TIMER_RUNNING` is a *command* set by the start trigger's touch (every frame the player overlaps it), not a persistent "running" state — after the tick the else-branch demotes it to IDLE once the touch stops while `initialized` stays 1 and `ms` keeps accumulating.
So in DeFRaG bit `0x4` toggles on **each frame the player is inside the start zone** (cgame event `0x40` = `TimerStarted`: reset client accumulators, play the start sound), and the run's clock effectively starts from the last touched frame (with the sub-ms correction of G_TRIGGER/Touch_Multi_1mstimer.md). This is a hypothesis from the two decodes, not runtime-verified; oDFs's "set once / clear at stop" scheme (see g_defrag.c comment) produces `0x40` only on start and on stop, and in the stop snapshot the cgame would see `0x40` and `0x100` together (`0x1bd3` handles `0x40` first -> `0x1ae1` resets the stat accumulators, then `0x100` -> `0x1aec` prints/finalises). If end-of-run stats or the stop sound look wrong with the stock DeFRaG cgame, this ordering is the first thing to check.
