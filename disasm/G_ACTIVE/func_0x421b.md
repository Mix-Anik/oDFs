# func_0x421b — per-frame playerState flags updater

**Address:** `0x421b`  
**Stack frame:** `enter 0x10`  
**Called from:** ClientThink_real (`0x1d27b`)  
**Args:** `(playerState_t *ps)`

---

## Purpose

Updates `ps->stats[12]` (`STAT_DEFRAG_FLAGS`) each frame by delegating to three setters, one per DeFRaG cvar. Runs once per `ClientThink_real` invocation so the networked playerState always reflects the current server cvar state for client-side prediction.

---

## Pseudocode

```c
void func_0x421b(playerState_t *ps) {
    func_0x426b(ps, df_ob_KillOBs);    // SetOBS    — bit 0x200
    func_0x42b5(ps, grapple_mode);     // SetGrappleMode — bits 0x400/0x800
    func_0x4290(ps, df_lg_Knockback);  // SetLGKnockback — bit 0x1000
}
```

Cvar sources:
- `df_ob_KillOBs` — global `0x132550`
- grapple mode — global `0x13183c` (0–3)
- `df_lg_Knockback` — global `0x131840`

---

## oDFs implementation

Implemented directly in `g_active.c` — `ClientThink_real` clears then selectively sets the DFLAG bits before Pmove. See `d:\Other\Programming\C\oDFs\code\game\g_active.c`.

Sub-functions documented in:
- [func_0x426b.md](func_0x426b.md) — SetOBS
- [func_0x4290.md](func_0x4290.md) — SetLGKnockback
- [func_0x42b5.md](func_0x42b5.md) — SetGrappleMode
