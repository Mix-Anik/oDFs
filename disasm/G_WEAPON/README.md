# G_WEAPON — Defrag QVM disassembly analysis

Analysis of `g_weapon.c`-equivalent functions from the Defrag qagame.qvm.
Currently limited to the muzzle-point calculation (relevant to rocket-jump /
splash-damage origin geometry investigated alongside
[../G_COMBAT/](../G_COMBAT/) and [../G_MISSILE/](../G_MISSILE/)).

For general disassembly methodology, struct layouts, and global addresses see
[../README.md](../README.md).

---

## Functions analyzed

| Function | DeFRaG address | vanilla address | Hash match? | Status |
|----------|-----------------|------------------|-------------|--------|
| `CalcMuzzlePoint` | `0x29383` | `0x208ce` | ✅ identical | ✅ Confirmed unmodified — see [CalcMuzzlePoint.md](CalcMuzzlePoint.md) |
| `CalcMuzzlePointOrigin` | `0x2940d` | `0x20958` | ✅ identical | ✅ Confirmed unmodified (hash + spot-check only) |

## Headline finding

Both functions are byte-identical to vanilla. Muzzle offset formula confirmed:
`point = origin + (0,0,viewheight) + forward*14`, with `viewheight` read live
from `ps.viewheight` (not a fixed constant) and the `14` forward-offset
constant confirmed present in the bytecode. No difference from vanilla; no
code change needed in oDFs.
