# func_0x1e60 — G_InitRaceMode

**Address:** `0x1e60`  
**Stack frame:** `enter 0x7c`  
**Called from:** G_InitGame / game init (`0x21fc`)  
**Args:** none

---

## Purpose

Initializes the race timer system. Reads the current DeFRaG game mode (from `0x1317bc`)
and sets the ready flags (`0x131808`, `0x131810`, `0x131818`) accordingly, along with
several other race-state globals. Three code paths:

- **Mode 1** (`*(0x1317bc) == 1`): Full DeFRaG race mode — sets all ready flags to 1.
- **Mode 3** (`*(0x1317bc) == 3`): Alternate race mode — sets start/stop flags to 2.
- **Else**: Cvar-detection mode — reads `df_ndm_timer_*` cvars to discover which entities
  are present on the map; populates ready flags from those cvar values.

---

## Pseudocode

```c
void G_InitRaceMode(void) {
    *(0x131838) = 0;                     // clear init-complete flag

    int mode = *(0x1317bc);              // DeFRaG game mode (weapon selection style)

    if (mode == 1) {
        // Full DeFRaG race — all entities enabled
        *(0x131808) = 1;                 // startTimer ready
        *(0x131810) = 1;                 // checkpoint ready
        *(0x131818) = 1;                 // stopTimer ready
        *(0x131820) = 1;                 // extra ready flag
        *(0x131824) = 6;                 // config value (checkpoint limit?)
        *(0x131828) = 126;               // 0x7e — timer config value
        *(0x13182c) = 126;
        *(0x131830) = 0;
        *(0x131834) = 0;
        *(0x131838) = 1;                 // mark init complete
        *(0x131840) = 0;                 // df_lg_Knockback = 0
        *(0x13183c) = 0;                 // grapple mode = 0
        *(0x1317c8) = -1;
        trap_CvarSet("defrag_mode", "-1");
        trap_CvarSet("defrag_route", "");

    } else if (mode == 3) {
        // Alternate race mode
        *(0x131808) = 2;                 // startTimer = mode 2
        *(0x131818) = 2;                 // stopTimer = mode 2
        *(0x13182c) |= 0x100;
        *(0x131828) |= 0x100;
        trap_CvarSet("defrag_route", "");

    } else {
        // Default: detect entities via df_ndm_timer_* cvars
        char *buf_start      = ReadCvarBuf("df_ndm_timer_start");
        SetReadyFlag(0x131808, buf_start);        // func_0x1de9

        char *buf_checkpoint = ReadCvarBuf("df_ndm_timer_checkpoint");
        SetReadyFlag(0x131810, buf_checkpoint);

        char *buf_stop       = ReadCvarBuf("df_ndm_timer_stop");
        SetReadyFlag(0x131818, buf_stop);

        // Discard invalid values (3 = some sentinel from SetReadyFlag)
        if (*(0x131810) == 3) *(0x131810) = 0;
        if (*(0x131818) == 3) *(0x131818) = 0;

        // Convert flags to display strings
        const char *s_start      = FlagToString(0x131808);  // func_0x1e35
        const char *s_checkpoint = FlagToString(0x131810);
        const char *s_stop       = FlagToString(0x131818);

        if (s_start != NULL) {
            // Build "defrag_svsc" cvar string from "%s-%s%s%s" and set it
            // (format: "df-dfdf" or similar based on which entities are present)
            ...
        }
    }
}
```

---

## Helper: `ReadCvarBuf` (`func_0x138e`)

Reads a cvar's string value into a shared buffer at `0x12c6f0` (256 bytes) and returns
the buffer address:
```c
char *ReadCvarBuf(const char *cvarName) {
    trap_CvarVariableStringBuffer(cvarName, g_cvarBuf /*0x12c6f0*/, 256);
    return g_cvarBuf;
}
```

---

## Helper: `SetReadyFlag` (`func_0x1de9`)

Writes a ready-flag value derived from a cvar string into a global slot:
```c
void SetReadyFlag(int *flagSlot, const char *cvarStr) {
    memset(flagSlot, 0, 8);                  // zero flag and the int after it
    if (strncmp(cvarStr, "df",   2) == 0) { *flagSlot = 1; return; }
    if (strncmp(cvarStr, "resp", 4) == 0) { /* set to 3? */ return; }
    // other prefixes → other values
}
```

The cvar value string encodes the entity type that was registered; "df" prefix means a
standard DeFRaG entity is present → sets flag to 1.

---

## Helper: `FlagToString` (`func_0x1e35`)

Converts a ready-flag value to a short display string:
```c
const char *FlagToString(int *flagSlot) {
    int v = *flagSlot;
    if (v == 1) return "df";
    if (v == 3) return "resp";
    if (v == 4) return func_0x3acc(*(flagSlot + 1));
    return NULL;
}
```

---

## Ready-flag value meanings

| Value | Meaning |
|-------|---------|
| 0 | Entity not present — USE callback is suppressed |
| 1 | Standard DeFRaG entity — USE callbacks active |
| 2 | Alternate race mode |
| 3 | "Resp" mode (suppressed by init) |
| 4 | Custom/extended mode |

Only value **1** enables the USE callbacks (`use_startTimer`, `use_stopTimer`,
`use_checkpoint` all guard on `== 1`).

---

## New globals discovered in this function

| Address | Description |
|---------|-------------|
| `0x131820` | Extra ready flag (set to 1 in mode 1) — **now resolved, see below** |
| `0x131824` | Config value (set to 6 in mode 1) — **now resolved, see below** |
| `0x131828` | Timer config (set to 126 / 0x7e in mode 1; 0x100 OR'd in mode 3) |
| `0x13182c` | Timer config (same as 0x131828) |
| `0x131830` | Cleared to 0 in mode 1 — **now resolved, see below** |
| `0x131834` | Cleared to 0 in mode 1 |
| `0x131838` | Init-complete flag (set to 1 at end of mode 1 init) |
| `0x1317c8` | Set to -1 in mode 1 (unknown purpose) |
| `0x12c6f0` | Shared 256-byte cvar string buffer (reused by ReadCvarBuf) |
| `0x19ecc` | Inactivity timeout cvar cache (referenced in func_0x1a44a) |

## `df_ndm_*` cvar cluster — RESOLVED

A later pass (investigating `trigger_push_touch`'s DeFRaG addition — see
[`trigger_push_touch.md`](trigger_push_touch.md)) traced this function's
tail (after the per-mode switch covered above, at the shared label where
every switch case converges, `0x200a` onward) and found it **overwrites**
`0x131820`/`0x131824`/`0x131830` again, unconditionally, right after the
switch — making the switch's own per-case values for these three addresses
only short-lived defaults, immediately replaced by real cvar reads:

```c
*(int*)0x131820 = trap_Cvar_VariableIntegerValue( "df_ndm_damage" );      // 0x2021-0x202a
*(int*)0x131824 = trap_Cvar_VariableIntegerValue( "df_ndm_weaponsSet" );  // 0x202b-0x2034
*(int*)0x131830 = trap_Cvar_VariableIntegerValue( "df_ndm_nopad" );       // 0x2035-0x203e
```

Confirmed via the string literals ("df_ndm_damage" at `0x4108`,
"df_ndm_weaponsSet" at `0x40f6`, "df_ndm_nopad" at `0x40e9`) immediately
preceding each `trap -7` (`trap_Cvar_VariableIntegerValue`, previously
established in this project) call, each result stored directly into the
named global with no further transformation.

**Registered cvar defaults** — read from the cvar-registration table's
string pool (`0x2f00`–`0x2fb1`), where a numeric literal immediately
*preceding* a cvar's name string in the pool is that cvar's own default
(cross-checked against 3 known-correct vanilla defaults found the same way:
`g_knockback`→`"1000"`, `g_gravity`→`"800"`, `g_speed`→`"320"`, all
confirmed matching real vanilla values, validating the pairing convention):

| Cvar | Global | Default | Confidence |
|------|--------|---------|------------|
| `df_ndm_weaponsSet` | `0x131824` | `"1022"` (`0b1111111110`= all weapons except `WP_GAUNTLET` and `WP_GRAPPLING_HOOK`) | High — literal found directly adjacent in the pool |
| `df_ndm_damage` | `0x131820` | `"0"` | Medium-high — no distinct literal adjacent (likely a dedup'd shared `"0"` string); inferred from semantics (func_0x274b's gating, see below) rather than read directly |
| `df_ndm_nopad` | `0x131830` | `"0"` | Medium-high — same reasoning as `df_ndm_damage` |

The same tail also sets two more addresses from two more `df_ndm_*` cvars,
confirmed the identical way (string literal immediately preceding a
`trap -7` call, result stored with no transformation):

- `0x131834` = cached integer value of cvar **`df_ndm_noteleport`** — see
  [`trigger_teleporter_touch.md`](trigger_teleporter_touch.md).
- `0x131838` — **correction to this doc's earlier entry** ("Init-complete
  flag, set to 1 at end of mode 1 init"): that was only the switch's
  *default* value for mode 1, immediately overwritten right after, same as
  `0x131820`/`0x131824`/`0x131830`. The real final value is
  **`(df_ndm_nomover == 0) ? 1 : 0`** — i.e. it is the *inverse* of
  "nomover is enabled", not an init-complete flag at all. Confirmed consumed
  by a separate, NOT-yet-decoded level-init entity sweep — see
  "Unresolved: entity-culling sweep" below.

This also resolves two previously-blocked findings elsewhere in this
project:

1. **`func_0x274b`'s health/armor override** (see
   [`func_0x274b.md`](func_0x274b.md)) — the gating global `0x131820` is
   **`df_ndm_damage`**. `df_ndm_damage == 0` (default) → flat 200
   health/armor; `!= 0` → handicap-based health, armor = 0. This is the
   "no damage mode" semantics the cvar's name implies: damage doesn't matter
   by default on DeFRaG movement maps, so health/armor are maxed out; a map
   or server can opt back into "damage matters" by setting this cvar.
2. **`trigger_push_touch`'s instant-kill gate** (see
   [`trigger_push_touch.md`](trigger_push_touch.md)) — the gating global
   `0x131830` is **`df_ndm_nopad`**.

`0x131824` (`df_ndm_weaponsSet`) is also the source of `func_0x274b`'s
`ps.stats[STAT_WEAPONS]` assignment — see that doc.

Also confirmed in this same function's tail: `df_hook_Type` → `0x13183c`,
`df_lg_Knockback` → `0x131840` (both already existing oDFs cvars,
`df_hook_type`/`df_lg_knockback` — this just confirms their qagame-side cache
addresses, no new behavior to port), and `df_ndm_powerups` /
`df_ndm_weaponsPickup` / `df_ndm_itemsSpawn` all feed bits into a combined
bitmask at `0x13182c` (which turns out to be the same global `func_0x274b`'s
ammo-loop tests via `& 0x6` — the `0x6` bits specifically come from
`df_ndm_weaponsPickup`, resolving what that gate actually means, though
`func_0x4b6a` itself is still unidentified). `df_ndm_itemsSpawn` additionally
drives `0x131828` (a related bitmask, OR'd with either a constant `0x7e` or
the `0x13182c` bitmask depending on its value) — not traced further, no
known consumer located in this pass.

## Level-init entity-culling sweep (`func_0x20e0`) — RESOLVED, see dedicated doc

`0x131838`'s consumer turned out to be part of a much larger, now mostly
decoded feature: a per-entity gametype/mode exclusion system spanning three
functions (`func_0x1c9c`, `func_0x24b2a`/`G_SpawnGEntityFromSpawnVars`, and
this sweep, `func_0x20e0`). Moved to its own module writeup —
see [`../G_SPAWN/entity_mode_exclusion.md`](../G_SPAWN/entity_mode_exclusion.md)
for the full decode, the ~13 `not<mode>` spawn-key table, and what's still
genuinely open (the bit-polarity oddity, and the unidentified `df_gametype`
mode-number meanings). **Not ported** — see that doc for why.

---

## oDFs implementation

In oDFs we do not have the DeFRaG mode system (`0x1317bc`). The simplest equivalent is:
set the ready flags directly in the spawn functions (`SP_target_startTimer` sets
`g_startTimerReady = 1`, etc.) rather than requiring a post-init scan. A `G_InitRaceMode`
call in `G_InitGame` could reset all race state at map load.

---

## Preset / mode table decoded and PORTED (`G_ResolveNdm`)
`G_InitRaceMode` (`0x1e60`) is where the "no damage mode" globals get their values:
- **mode 1 (run)**: `0x131820 = 1` (damage), `0x131824 = 6` (weapons = gauntlet + machinegun), `0x131828 = 0x13182c = 0x7e`, nopad/noteleport 0, `0x131838 = 1` (movers kept), `defrag_mode = -1`. So in run mode the spawn loadout is **gauntlet + machinegun (100 ammo), health maxHealth+25, armor 0** regardless of the `df_ndm_*` cvars.
- **other modes**: `defrag_mode` = `df_mode_tricksmode` (mode 2) / `df_mode_fastcaps` (mode 3) (both default 2). Values 1..7 select a preset (jump table data `0x930`, code `0x1f86`-`0x2006`): 1: weapons 6, nopad 2, noteleport 2, nomover; 2: weapons `0x1fe` (gauntlet..plasma, no BFG/hook); 3: weapons 6; 4: `0x1fe`, nopad 2, noteleport 2, nomover; 5/6: weapons `0x404` (MG + hook; sets `0x13183c` hook 1 / 0); 7: damage 1, weapons 6, flags `0x7e`. Any other value (0) reads the `df_ndm_*` cvars (`damage`, `weaponsSet`, `nopad`, `noteleport`, `nomover`; flags |= `0x18` if damage, `0x60` if powerups, `0x6` if weaponsPickup).
**oDFs:** `g_spawn.c` `G_ResolveNdm()` (run from `G_ResolveDfMode`, i.e. after entity spawn) fills `g_ndm`; `ClientSpawn`, `target_init`, nomover exclusion, jump-pad/teleporter gates now read `g_ndm` instead of the raw cvars; new LATCH cvars `df_mode_tricksmode`/`df_mode_fastcaps` (default 2). The preset hook setting (`0x13183c`) is not applied.
**Behaviour change (visible):** on maps with a start timer (mode 1) you spawn with gauntlet + machinegun only, MG ammo 100, 125 health, 0 armor. On maps without a timer (tricks, preset 2) you get weapons 1..8 with 200 ammo/200 hp/200 armor. Revert: make `G_ResolveNdm` copy the cvars (the `default:` branch) for every mode. Verified headless (`df_dbg_speed 1` prints `weapons 0x6 hp 125 armor 0 mg 100 mode 1`).
