# func_0x42b5 — SetGrappleMode

**Address:** `0x42b5`  
**Stack frame:** `enter 0x24`  
**Called from:** func_0x421b (`0x421b`), also `0x20d2d`  
**Args:** `(playerState_t *ps, int grapple_mode)`

---

## Purpose

Encodes the DeFRaG grapple mode (0–3) into bits `0x400` and `0x800` of `ps->stats[12]` (`STAT_DEFRAG_FLAGS`). Both bits are cleared first, then re-set according to a 4-way switch.

---

## Pseudocode

```c
void func_0x42b5(playerState_t *ps, int grapple_mode) {
    ps->stats[12] &= ~0x400;   // clear both grapple bits
    ps->stats[12] &= ~0x800;

    switch (grapple_mode) {
        case 0: break;                                  // no grapple
        case 1: ps->stats[12] |= 0x400; break;
        case 2: ps->stats[12] |= 0x800; break;
        case 3: ps->stats[12] |= 0x400;
                ps->stats[12] |= 0x800; break;
    }
}
```

If `grapple_mode` is out of range (< 0 or > 3), both bits remain cleared (same as case 0).

---

## Bit encoding

| Mode | 0x400 | 0x800 | Meaning |
|------|-------|-------|---------|
| 0 | 0 | 0 | No grapple |
| 1 | 1 | 0 | `DFLAG_GRAPPLE_MODE_1` |
| 2 | 0 | 1 | `DFLAG_GRAPPLE_MODE_2` |
| 3 | 1 | 1 | Both modes |

---

## Key addresses

| Address | Description |
|---------|-------------|
| ps + 0xe8 | `stats[12]` = `STAT_DEFRAG_FLAGS` |
| `0x13183c` | Grapple mode cvar cache (0–3), passed in as arg1 by func_0x421b |

**Note:** Also called from `0x20d2d` — **identified this pass**: `0x20d2d` is
`Pickup_Weapon` (weapon-item pickup handler), which calls this function only
when the picked-up weapon is `WP_GRAPPLING_HOOK` (0xa). Fully decoded,
confirmed to contain zero floating-point instructions and zero references to
`ps.velocity` or the `df_promode` cache — unrelated to knockback/velocity. See
[func_0x20d2d.md](func_0x20d2d.md).

**Consumer fully decoded — see [GrappleMode.md](GrappleMode.md).** The
`DFLAG_GRAPPLE_MODE_1`/`_2` bits this function sets select between four structurally
distinct grapple-swing physics implementations (`func_0x6f7c`/`0x711b`/`0x71a7`/`0x704a`),
dispatched from `func_0x7208` while `PMF_GRAPPLE_PULL` is active.
