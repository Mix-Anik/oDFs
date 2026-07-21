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
