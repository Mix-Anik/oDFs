# df_unit — distance/velocity unit conversion

**Source files:** `D:\Other\Reverse Engineering\q3vm\cgame.dis`, `ui.dis`
**Cvar:** `df_unit` (default `"0"`, flags `0x1` = `CVAR_ARCHIVE`)
**Description (from DeFRaG docs):** `/df_unit [0-2]` — 0 = quake units/units-per-second,
1 = meters/km-per-hour, 2 = miles/miles-per-hour.

---

## Where it lives

`df_unit` is **cgame-only**. Confirmed absent from `qagame.dis` entirely (server
never reads or registers it). It appears in `ui.dis` only as a settings-menu
widget (a combo box wired to `Cvar_SetValue`/`Cvar_VariableValue`-style traps) —
no conversion math exists on the UI side. All actual unit-conversion logic
lives in `cgame.qvm`.

---

## Cvar storage — confirmed `0x11b324` (`.integer` at `0x11b324+0xc = 0x11b330`)

Found via the engine's batch cvar-registration table. `cgame.qvm`/`ui.qvm`
register the large majority of their cvars through one data-driven loop
(see [cgame_cvar_table.md](cgame_cvar_table.md) for the full methodology/table
layout write-up) rather than individual hardcoded `trap_Cvar_Register` calls —
this is why `df_unit`'s name string (`"df_unit"` at data address `0x6720`)
never appears as an immediate operand anywhere in the code segment; it's only
ever read indirectly through the table.

**Important correction made while deriving this**: the table's 4-word row
layout is `[storage, name, default, flags]`, *storage first* — matching the
real engine signature `trap_Cvar_Register(vmCvar_t *vmCvar, name, defaultValue,
flags)`. An earlier reading pass of this same table (within this session)
initially assumed `[name, default, flags, storage]` (storage *last*), which
produced a plausible-looking but wrong result (`0x119ab4`, which is actually
an unrelated cvar's storage, one row off). The corrected `[storage, name,
default, flags]` ordering was confirmed two ways:
1. It matches the registration loop's own `Arg[0x8]=+0, Arg[0xc]=+4,
   Arg[0x10]=+8, Arg[0x14]=+0xc` → `call trap_Cvar_Register(-4)` argument
   order exactly.
2. Looking up storage `0x11b324` under the corrected layout resolves to
   name=`"df_unit"`, default=`"0"`, flags=`1` — an exact match.

---

## The conversion formula — doubly confirmed

Found in two independent places that must stay consistent with each other:
the live speed-meter HUD widget, and a developer "Stats" debug dump (see
[cgame_stats_dump.md](cgame_stats_dump.md)) whose `printf`-style format
strings make the units unambiguous (`"(%i Km/h)"`, `"(%i miles/h)"`,
`"(%i meters)"`, `"(%i miles)"`).

### Shared helper — `func_0x1002`

```c
float func_0x1002( float raw, int mode ) {
    if ( mode == 1 ) return raw * 0.027f;      // 0x3cdd2f1b
    if ( mode == 2 ) return raw * 0.000016f;   // 0x378637bd
    return raw;
}
```

Confirmed by direct instruction read (`0x1002`-`0x1023`). This is a **generic**
utility — called from 23 distinct sites across `cgame.qvm`, not unit-specific
by name — but the `df_unit`-driven call sites always pass `mode` equal to the
live `df_unit.integer` value, making it the de facto unit-scale table for this
feature: `0.027` = meters per quake unit, `0.000016` = miles per quake unit.

### Distance (no time component)

```c
if ( df_unit.integer == 1 )      meters = raw_units * 0.027f;
else if ( df_unit.integer == 2 ) miles  = raw_units * 0.000016f;
else                              /* unchanged, quake units */
```

Confirmed verbatim in the Stats dump for "Distance Run" (`0x447b`-`0x4492`)
and "Highest Jump" (`0x44b7`-`0x44e5`), both driven by the exact same
`df_unit`-indexed `func_0x1002` call and format strings `"(%i meters)"` /
`"(%i miles)"`.

### Velocity (per-second → per-hour, then the same per-unit scale)

```c
if ( df_unit.integer == 1 )      kmh = (raw_ups * 3.6f)  * 0.027f;    // = raw_ups * 0.0972
else if ( df_unit.integer == 2 ) mph = (raw_ups * 3600.0f) * 0.000016f; // = raw_ups * 0.0576
else                              /* unchanged, quake units/s */
```

Confirmed verbatim in the Stats dump for "Max Speed" (`0x43fa`-`0x442b`,
format strings `"(%i Km/h)"` / `"(%i miles/h)"`) and reproduced identically
for "Average Speed" immediately after it (`0x4439`-`0x4470`). Also confirmed
(same `×3.6`/`×0.027` constants) inside the HUD speed-meter widget itself —
see "Unresolved alternate branch" below for a caveat about that widget.

**The `3.6`/`3600.0` constants are the standard m/s→km/h and m/s→m/h-in-meters
conversions** (seconds-per-hour, applied before the same per-unit scale factor
used for plain distance). They are not themselves unit-specific to Quake.

### Honesty note on `0.027` / `0.000016`

These two constants do **not** agree with each other via the precise
1609.344 m/mile ratio (`0.000016 mi × 1609.344 = 0.02575 m`, vs the `0.027 m`
used directly for the meters case — about 5% apart). They read as two
independently-chosen "close enough" round decimal constants rather than both
being derived from one canonical "1 quake unit = X real-world length" figure.
Reported as found, not reconciled further — there is no bytecode evidence of
a single canonical constant underlying both.

---

## `df_drawSpeed` — confirmed, gates which speed-meter widget style is drawn

Found incidentally while tracing the HUD widget call chain. Storage
`0x11c0f4` (`.integer` at `0x11c100`), default `"1"`, flags `1`.
`disasm/G_WEAPON`-style UI combo-box wiring in `ui.dis` (`0x4ccb`,
`0x4d21`) confirms it's a multi-value selector (not boolean), consistent with
selecting between at least two different speed-meter HUD skins.

### Unresolved alternate branch (flagged, not resolved)

The HUD function at `0x41b6` (called from `CG_Draw2D`=`0x2a12`, plus `0xc320`
and `0x1bafd`) branches on `df_drawSpeed.integer`:
- `df_drawSpeed == 2`: an inline 3-way `df_unit` switch using constants
  `20.0`/`320.0`/`0.625` (→ factors `0.0625` for label `"k"`, `0.0390625` for
  label `"m"`, raw for label `"u"`).
- otherwise (including the default `df_drawSpeed == 1`): the canonical
  `×3.6×0.027` / `×3600×0.000016` formula documented above, labels `"k"`/`"m"`/`"u"`.

**This first branch's `0.0625`/`0.0390625` constants were not reconciled with
the canonical `0.027`/`0.000016` constants, and no literal unit-name string
was found near it to confirm what `"k"`/`"m"` mean there** (unlike the Stats
dump, which spells out `"Km/h"`/`"miles/h"` explicitly). It is plausibly an
alternate/older speed-meter HUD skin that independently approximates the same
real-world units with different rounding, but this is **not confirmed** —
flagged as an open item rather than guessed. Not used for the oDFs
implementation below; only the doubly-confirmed canonical formula was ported.

A separate, much larger function (`0x1a57a`, the "CGAZ" acceleration-zone
meter widget — see `df_hud_cgaz*` cvars) also receives `df_unit.integer` as
an argument, but every float constant found inside it (`2.0`, `90.0`, `180.0`,
π) is angle/geometry-related, not a unit-conversion factor. Its use of
`df_unit` was not resolved to any observed behavioral effect — flagged, not
chased further (out of scope; this widget is unrelated to the plain
distance/speed readouts).

---

## oDFs implementation notes

**Implemented**: `df_unit` cvar registered in `code/cgame/cg_main.c`
(`"0"`, `CVAR_ARCHIVE`), and a conversion helper (`CG_ConvertDistance`/
`CG_ConvertSpeed`) added to `code/cgame/cg_draw.c` implementing the exact
confirmed formula above. **No HUD widget consumes it yet** — oDFs currently
has no speed-meter or distance/jump-meter HUD element at all (confirmed: no
`df_drawSpeed`, `df_drawJumpmeters`, or stats-dump equivalent exists in
`code/cgame`), so this is a ready-to-use utility for whenever that feature is
built, not a wired-up end-to-end feature. Not porting the unresolved
alternate `df_drawSpeed==2` branch or the CGAZ widget, since neither is
confirmed to represent real behavior worth replicating yet.
