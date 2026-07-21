# func_0x426b — SetOBS

**Address:** `0x426b`  
**Stack frame:** `enter 0x10`  
**Called from:** func_0x421b (`0x421b`)  
**Args:** `(playerState_t *ps, int killobs_flag)`

---

## Purpose

Sets or clears bit `0x200` in `ps->stats[12]` (`STAT_DEFRAG_FLAGS`) based on the `df_ob_KillOBs` cvar value. When set, `bg_pmove.c` suppresses overbounce detection for this player.

---

## Pseudocode

```c
void func_0x426b(playerState_t *ps, int killobs_flag) {
    if (killobs_flag != 0)
        ps->stats[12] |=  0x200;   // DFLAG_OBS_DISABLED — set
    else
        ps->stats[12] &= ~0x200;   // clear
}
```

---

## Key addresses

| Address | Description |
|---------|-------------|
| ps + 0xe8 | `stats[12]` = `STAT_DEFRAG_FLAGS` |
| `0x132550` | `df_ob_KillOBs` cvar integer cache (passed in as arg1 by func_0x421b) |
