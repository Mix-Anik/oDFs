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
