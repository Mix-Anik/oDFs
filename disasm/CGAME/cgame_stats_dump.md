# cgame "Stats" debug dump — `func_0x4388`

**Source file:** `D:\Other\Reverse Engineering\q3vm\cgame.dis`
**Address:** `0x00004388` (`enter 0x44`)

Found incidentally while tracing `df_unit` consumers (see [df_unit.md](df_unit.md)).
A console/print-only debug dump — not a HUD overlay — printing three labeled
sections via a `Printf`-style call (`func_0x23c`, not independently
identified beyond its call shape; presumed `trap_Print`/`CG_Printf`-adjacent).
**What triggers this dump (a console command? a keybind?) was not traced —
flagged as unknown, out of scope for this pass.**

---

## Full confirmed output shape

```
----- Stats -----
Physics
 type           : <string, physics-type name>
 pmove_fixed    : <0 or 1>
 pmove_msec     : <int>
 sv_fps         : <int>
 g_synch        : <int>
 com_maxFps     : <int>

Performance
 Max Speed      : <int> ups (<int> Km/h) / (<int> miles/h)      [per df_unit]
 Average Speed  : <int> ups (<int> Km/h) / (<int> miles/h)      [per df_unit]
 Distance Run   : <int> units (<int> meters) / (<int> miles)    [per df_unit]
 Highest Jump   : <int> units (<int> meters) / (<int> miles)    [per df_unit]
```

(The `df_unit`-gated parenthetical only appears for `df_unit.integer == 1` or
`== 2`; at `df_unit.integer == 0` only the bare `units`/`ups` value prints,
with no parenthetical.)

---

## Confirmed global addresses

| Address | Meaning | Confirmed via |
|---------|---------|----------------|
| `0x11d230` | "Max Speed" accumulator, quake units/s (float) | Direct format-string pairing, `" Max Speed : %i ups"` |
| `0x11d234` | "Distance Run" accumulator, quake units (float) | Direct format-string pairing, `" Distance Run : %i units"`; also the numerator for the average-speed calc below |
| `0x11d250` | "Highest Jump" value, quake units (float) | Direct format-string pairing, `" Highest Jump : %i units"` |
| `0x11d284` | Elapsed-time denominator used for the average-speed calc | `avgSpeed = (0x11d234 * 1000.0) / (float)0x11d284` when `0x11d284 != 0`, else `0`. Units not independently confirmed (plausibly accumulated milliseconds of movement, given the `*1000.0` scaling pattern matches ms→units/s) |
| `0x11d2b0` | Physics-type index | Used as `*(int*)( (0x11d2b0_value << 2) + 0x12c0 )` to index a string-pointer array at `0x12c0`, printed as `" type : %s"` — the physics-mode name (e.g. vq3/cpm-style promode label; individual table entries at `0x12c0` not enumerated this pass) |
| `0x11d590` | `sv_fps` snapshot | Direct format-string pairing, `" sv_fps : %i"` |
| `0x11d594` | `g_synch` snapshot | Direct format-string pairing, `" g_synch : %i"` |
| `0x11d598` | `pmove_msec`-related | See caveat below — read twice, once booleanized (`pmove_fixed` line) and once raw (`pmove_msec` line) |
| `0x11d5a0` | `com_maxFps` snapshot | Direct format-string pairing, `" com_maxFps : %i"` |

### Caveat: `pmove_fixed` and `pmove_msec` both read `0x11d598`

The bytecode reads `*(int*)0x11d598`, booleanizes it (`!= 0` → `1` else `0`)
for the `pmove_fixed` line, then **separately reads the same address again,
unmodified**, for the `pmove_msec` line immediately after. This was read
directly, not inferred — but whether this reflects two real distinct
client-side concepts collapsing onto one cached value (plausible: vanilla Q3
engine semantics already tie `pmove_fixed`'s meaning to whether
`pmove_msec` is nonzero), or a mistaken address reuse by the original
implementer, was not resolved. Reported as observed, not explained further.

---

## Relationship to `df_unit`

Both speed lines ("Max Speed", "Average Speed") and both distance lines
("Distance Run", "Highest Jump") independently re-implement the exact same
`df_unit`-driven conversion (see [df_unit.md](df_unit.md) for the formula) —
this repetition across four separate call sites, each with its own explicit
`"Km/h"`/`"miles/h"`/`"meters"`/`"miles"` format string, is what makes the
`df_unit` formula doubly/triply/quadruply confirmed rather than a single
unverified reading.

## oDFs implementation notes

Not ported. This is a pure debug/diagnostic feature with no gameplay or HUD
effect, and its trigger mechanism was never identified. Documented for
completeness only, per the instruction to record everything found rather
than silently discard incidental findings. If oDFs ever wants an equivalent
`/stats`-style debug dump, the four `df_unit`-converted fields can reuse the
`CG_ConvertDistance`/`CG_ConvertSpeed` helpers added for `df_unit` (see
[df_unit.md](df_unit.md#odfs-implementation-notes)).

---

## Accumulators and writers (mapped in a later pass; NOT ported)
The "Stats" dump is the **end-of-run statistics** feature: the `stats` server command (and the timer-stop handler `0x1aec` / `0x1b10`) call `0x4388` unless `df_noEndOfRunStats` (cvar storage `0x11bdd0`) is set.
Writers of the accumulators (all in cgame, called per frame from `0x1bd3`, the per-snapshot timer/HUD update):

| Global | Meaning | Writers |
|---|---|---|
| `0x11d230` | max speed | `0x3985`, `0x3a4a` |
| `0x11d234` | distance run (sum of per-frame `|Δorigin|`, `VectorLength` = `0x3086a`) | `0x4677` |
| `0x11d238`/`0x11d23c`/`0x11d240` | per-axis ± distance / jump-pad-count style counters (`0x4677` also bumps `0x11d240` when an axis delta equals exactly 1.0, i.e. "unit move" detection) | `0x4677` |
| `0x11d244`/`0x11d248`/`0x11d24c`/`0x11d250` | current jump height / max-in-air-height-this-jump / max-so-far / highest jump | `0x458e` (jump tracking from ground/air transitions: `pm_flags`-style bit `4` of `ps+0x94`, origin `ps+0x40..0x48`, snapshot time `ps+0xe4`) |
| `0x11d254`..`0x11d264` | last-ground-position bookkeeping (`0x11d258` = origin copy at takeoff) | `0x458e` |
| `0x11d284` | elapsed time denominator | timer code |
| `0x11d288` | "run active" flag | timer code; `0x1b10` clears it when the stats are printed |
| `0x11d290`/`0x11d294` | pointers to the current and previous **snapshot playerState** (offsets `ps+0x40/44/48` origin, `ps+0x94` flags, `ps+0xe4` time, `ps+0xbc/0xe4/0xf0/0x1a4` misc) | snapshot code |
| `0x4370` | resets all accumulators (called by `TimerStarted` handler `0x1ae1`) | |

**Correction:** the offsets are the vanilla `snapshot_t` layout, not a different `playerState_t`: `0x11d290`/`0x11d294` point to snapshots (`ps` starts at `snap+0x2c`), so `snap+0x40` = `ps.origin[0]`, `+0x4c` = `ps.velocity[0]`, `+0x94` = `ps.eFlags` (bit 4 = `EF_TELEPORT_BIT`), `+0xe4` = `ps.stats[0]` (health; <= 0 skips tracking).
The per-frame driver is `0x1bd3` (from `0x1e4c`): it polls event bits in global `0x11d280` via `0x1ac7(mask)` (`0x40` -> TimerStarted `0x1ae1`, `0x100` -> TimerStopped `0x1aec`, `0x08` -> `0x1b10`, `0x10` -> checkpoint reset `0x1b2e`), clamps `com_maxfps` to 60..140 (messages "^1com_maxfps lowest value restricted to 60.^7"), then runs the trackers `0x4677` (distance) and `0x458e` (jump).
The event-bit word `0x11d280` is built in `0x5248` (the timer-state update, 4 writers) from the server-provided ps bits, i.e. this whole subsystem lives in **DeFRaG's own cgame.qvm**.
**Important context:** oDFs deployments run players on the original DeFRaG `cgame.qvm` (via the mdd cgame-proxymod) with only `qagame` re-implemented, so a cgame port of this feature has little practical value; what matters is the *server -> client contract* (ps fields, configstrings, server commands) that DeFRaG's cgame consumes. See server_commands.md.

## Iter 12 (full-port loop): PORTED (cg_defrag.c)
Decoded this pass: `0x3a4a` = max horizontal speed (`pps` velocity, z zeroed, only while run-active flag `0x11d288`); `0x4677` = distance: sum of horizontal |delta origin| between consecutive snapshots while active, skipped when event `0x20` (stats[12] bit 1 changed) or health <= 0; `0x3985` is the init sequence (not a tracker); `0x1b10` (finish event `0x8`) prints the dump only if `0x11d2ac == 0` (defrag_gametype >= 5) and `df_noEndOfRunStats == 0`; the `stats` server command also prints it. oDFs: trackers run per snapshot from `CG_DF_SnapshotTransition`; print on finish (gametype >= 5) or on the `stats` command (listen servers); same text/format incl. `df_unit` suffixes; average speed = distance*1000/race ms. Output goes to the console. `type` line: cpm/vq3 from `PMF_PROMODE`; sv_fps/g_synch/pmove_msec read from systeminfo (`defrag_svfps` not published by oDFs -> may print 0). **Not ported: 'Highest Jump' line (tracker `0x458e`, semantics unclear — it keys on EF_TELEPORT_BIT toggles).** Syntax-checked, untested. Revert: `df_noEndOfRunStats 1`.
