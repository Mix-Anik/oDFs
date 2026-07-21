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
| `0x131820` | Extra ready flag (set to 1 in mode 1) |
| `0x131824` | Config value (set to 6 in mode 1) |
| `0x131828` | Timer config (set to 126 / 0x7e in mode 1; 0x100 OR'd in mode 3) |
| `0x13182c` | Timer config (same as 0x131828) |
| `0x131830` | Cleared to 0 in mode 1 |
| `0x131834` | Cleared to 0 in mode 1 |
| `0x131838` | Init-complete flag (set to 1 at end of mode 1 init) |
| `0x1317c8` | Set to -1 in mode 1 (unknown purpose) |
| `0x12c6f0` | Shared 256-byte cvar string buffer (reused by ReadCvarBuf) |
| `0x19ecc` | Inactivity timeout cvar cache (referenced in func_0x1a44a) |

---

## oDFs implementation

In oDFs we do not have the DeFRaG mode system (`0x1317bc`). The simplest equivalent is:
set the ready flags directly in the spawn functions (`SP_target_startTimer` sets
`g_startTimerReady = 1`, etc.) rather than requiring a post-init scan. A `G_InitRaceMode`
call in `G_InitGame` could reset all race state at map load.
