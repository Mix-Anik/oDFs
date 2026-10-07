# `BG_PlayerTouchesItem` — DeFRaG address `0x4bc8`

**Vanilla address:** `0x10141`
**Vanilla size:** `0x60` (96 bytes) — `0x101a1-0x10141`
**DeFRaG size:** `0x7a` (122 bytes) — `0x4c42-0x4bc8` (+26 bytes)

**Verdict: MODIFIED** — a promode-gated relaxation of the upper vertical (Z)
pickup-tolerance bound, gated directly on the `pm_flags & 0x8000` promode bit
(not a cvar read).

## Identification

Full instruction-level decode, 3 args `(ps, item, atTime)` matching vanilla's
`(playerState_t*, entityState_t*, int atTime)`:

1. Calls `BG_EvaluateTrajectory(&item->pos, atTime, &origin)` — call target
   `0x4d73`, the already-confirmed `BG_EvaluateTrajectory` address (see
   `disasm/README.md` and `G_COMBAT/RocketJump_ExtraVelocity_Search_Pass2.md`).
2. All 6 bound checks follow (X, Y, Z-lower always; Z-upper branches on
   promode) — the function returns `qfalse` the moment any check fails, and
   `qtrue` only if all pass, matching vanilla's early-return chain shape
   exactly.

## Bound checks decoded

| Axis | Check | Constant | Value | Vanilla | Delta? |
|---|---|---|---|---|---|
| X | `ps.origin[0]-origin[0] > C` → false | `0x42300000` | **44.0** | 44 | none |
| X | `ps.origin[0]-origin[0] < C` → false | `-0x3db80000`→`0xC2480000` | **-50.0** | -50 | none |
| Y | `ps.origin[1]-origin[1] > C` → false | `0x42100000` | **36.0** | 36 | none |
| Y | `ps.origin[1]-origin[1] < C` → false | `-0x3df00000`→`0xC2100000` | **-36.0** | -36 | none |
| Z (non-promode) | `ps.origin[2]-origin[2] > C` → false | `0x42100000` (same local, reused) | **36.0** | 36 | none |
| Z (**promode**) | `ps.origin[2]-origin[2] > C` → false | `0x42840000` | **66.0** | *(n/a, no promode variant in vanilla)* | **DeFRaG-only, +30** |
| Z (both modes) | `ps.origin[2]-origin[2] < C` → false | `-0x3df00000`→`0xC2100000` | **-36.0** | -36 | none |

All negative constants above were computed via **two's-complement
reinterpretation** per this project's documented pitfall (e.g. `-0x3db80000`
is *not* `-(44.0)` by naive sign-flip of the positive hex's float value; its
actual 32-bit two's-complement pattern is `0xC2480000`, which decodes to
`-50.0`) — each was independently verified bit-by-bit, not assumed.

## The promode branch (source of the +26 bytes)

```
0x4c05  if ( ps->pm_flags(+0xc) & 0x8000 ) != 0   ; DeFRaG promode bit, confirmed in disasm/README.md
            goto 0x4c19                            ; SKIP the non-promode Z-upper check
        ; else (non-promode): fall through to the vanilla Z>36.0 check (may return false)

0x4c19: ; reached either way
        if ( (ps->pm_flags & 0x8000) == 0 )        ; re-checked
            goto 0x4c30                             ; non-promode: skip the promode Z-upper check too
        ; promode: Z-upper check against 66.0 instead of 36.0 (may return false)

0x4c30: ; Z-lower check against -36.0, unconditional in both modes
```

Net effect, expressed as C:
```c
if ( !(ps->pm_flags & PMF_PROMODE) ) {
    if ( ps->origin[2] - origin[2] > 36 )  return qfalse;
} else {
    if ( ps->origin[2] - origin[2] > 66 )  return qfalse;
}
if ( ps->origin[2] - origin[2] < -36 )     return qfalse;
```

X, Y, and the Z-lower bound are **completely unaffected by promode** — only
the upper Z tolerance (how far *above* the item the player's origin may be
and still register a touch) is relaxed, and only in promode. This is
consistent with CPM/promode physics allowing higher jump arcs than vanilla
Q3 movement, needing more vertical slack for airborne item pickups
(overbounces, ramp-jumps, etc. passing well above an item's rest height).

## Summary

`BG_PlayerTouchesItem` is otherwise identical to vanilla (all other bounds,
control flow, and the `BG_EvaluateTrajectory` call are unchanged) — the
entire size delta is this one added promode-conditional branch relaxing the
upper Z pickup tolerance from 36 to 66 units.
