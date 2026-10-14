# playerState fields the DeFRaG cgame reads (server → client audit, part 3)

Method: byte-offset scan of `cgame.dis` for `snap+0x…` reads (ps = `snap+0x2c`; `stats[n]` = `snap+0xe4+4n`, `powerups[n]` = `snap+0x164+4n`) plus the already-decoded readers. Only fields that carry DeFRaG *meaning* are listed; vanilla ps reads (origin, velocity, weapon, ammo, persistant[], events…) are unchanged.

| ps field | snap offset | cgame readers | meaning | oDFs |
|---|---|---|---|---|
| `stats[7]`, `stats[8]` | `+0x100/+0x104` | `0x5fd7` (decoder), `0x5eb7` (legacy decoder), `0x5248` | race time ms (hi/lo), plain on dedicated, encoded otherwise | set (`G_UpdateRacePs`, `G_EncodeRaceTime`) |
| `stats[12]` | `+0x114` | `0x1c8b` (event derivation), `0x6237` (validity: bits `0x20`/`0x40`) | DeFRaG flags: 1 target_init, 2 initialized, 4 running, 8 finish, 0x10 checkpoint, 0x20 clock-drift, 0x40 bad frame time | set (bits 1,2,4,8,0x10); `0x20/0x40` deliberately not set (`cheat_report.md`) |
| `stats[13]` | `+0x118` | `0x85c0` — a masked bit test called ≈40× from the input/key HUD (`0xc320`, `0xfde7`, `0x155d2`); when the demo/local flag `*0x129864 != 0` it tests `*0xea228` instead | **input bitmask**; masks used: `0x1 0x2 0x8 0x10 0x20 0x40 0x100 0x200` | identical to oDFs `DINPUT_*` (FORWARD 1, BACKWARD 2, LEFT 8, RIGHT 0x10, UP 0x20, DOWN 0x40, ATTACK 0x100, DIAGONAL 0x200) — set in `PM_UpdateInputState` ✓ |
| `stats[14]` | `+0x11c` | `0x275db` (tracemask) | `df_mp_InterferenceOff` value | set (`STAT_INTERFERENCE`) |
| `pm_flags & 0x8000` | `+0x38` | `0x6237` (code 16) | promode | set from `df_promode` ✓ |
| `speed`, `gravity` | `+0x60`, `+0x5c` | `0x6237` (codes 0/1) | must be 320 (416 during haste) / 800 | from `g_speed`/`g_gravity` ✓ |
| `stats[0]`, `viewangles[2]` | `+0xe4`, `+0xcc` | `0x6237` (code 17) | health > 0 with non-zero roll is flagged | vanilla ✓ |
| `powerups[7]`, `powerups[8]` | `+0x180/+0x184` (HUD `0x2a12`) | | `PW_REDFLAG`/`PW_BLUEFLAG` — vanilla CTF flag carrying (qagame writes them only in CTF code: `0x2aa8` target_init, pickup/die, `0x1a63a`) | not needed |
| `powerups[3]` | `+0x170` | `0x6237` | haste expiry memo for the speed check | vanilla ✓ |
| `stats[9]` | `+0x108` | `0x5eb7` (legacy decoder only, version < 18100), `0x11d58` is NOT a ps read | unused for version 19131 | — |

## Conclusion
Every DeFRaG-specific ps field the cgame consumes is either already supplied by the oDFs qagame/Pmove or intentionally left unset (`stats[12]` bits `0x20/0x40`, harmless). The input mask contract (`stats[13]`) matches bit-for-bit. Remaining part of the audit: **entityState** fields read by DeFRaG cgame for items/triggers (`s.time2`, `s.generic1`, `s.modelindex2`, … — e.g. the respawn sweep's "taken" bits, `func_0x2830`).
