# CGAME — DeFRaG cgame.qvm disassembly analysis

Analysis of the DeFRaG client-side game code from `cgame.dis` (version ~1.91.29/30).

For general methodology and background, see [../README.md](../README.md).

---

## Analyzed modules

| Module | File | Status |
|--------|------|--------|
| Race timer HUD system | [cgame_timer_hud.md](cgame_timer_hud.md) | ✅ Analyzed |
| `df_unit` distance/velocity conversion | [df_unit.md](df_unit.md) | ✅ Analyzed — formula doubly-confirmed; implemented in oDFs (cvar + helper, no HUD widget consumes it yet) |
| Batch cvar-registration table (cgame/ui) | [cgame_cvar_table.md](cgame_cvar_table.md) | ✅ Methodology documented — needed to find ANY cvar's storage address in cgame.dis/ui.dis |
| "Stats" debug dump (`func_0x4388`) | [cgame_stats_dump.md](cgame_stats_dump.md) | ✅ Analyzed — trigger mechanism unresolved, not gameplay-relevant |

---

## Key findings

### Timer HUD is entirely client-side computed

The cgame **does not** read a pre-computed elapsed time from the server ps. Instead:

1. **State signals** come via `ps.stats[STAT_DEFRAG_FLAGS]` bit transitions (0x4 = start, 0x8 = finish, 0x10 = checkpoint)
2. **Time computation** is done locally: `elapsed = base + rate * (ps.commandTime - startTime) / 1000`
3. **Entity presence detection** happens via SERVERINFO cvars (`df_ndm_timer_start`, `df_ndm_timer_stop`, `df_ndm_timer_checkpoint`)
4. **Draw enabling** requires `defrag_vers` cvar to be non-empty in SERVERINFO

### Critical architectural difference: QVM shared memory vs DLL module separation

In **DeFRaG (QVM mode)**: qagame and cgame share the same VM address space. The server writes to VM addresses (e.g., `0x11b000 = 1`) and cgame reads from identical addresses. No explicit communication layer needed.

In **oDFs (DLL mode)**: qagame and cgame are separate modules with separate data segments. Communication must happen through networked fields in `playerState_t` or SERVERINFO cvars. The `0x11b000` address written by qagame is NOT visible to cgame.

**Solution**: Use SERVERINFO cvars as the bridge. Set them from qagame during map spawn; cgame reads them via `trap_CvarVariableStringBuffer` and populates its own timer state array.

### What cgame needs from qagame

**SERVERINFO cvars** (read by cgame via `trap_CvarVariableStringBuffer`):
- `df_ndm_timer_start` — set to `"df"` if start timer entity exists
- `df_ndm_timer_stop` — set to `"df"` if stop timer entity exists
- `df_ndm_timer_checkpoint` — set to `"df"` if checkpoint entity exists
- `defrag_route` — set to `"df-df-df"` (or similar) to configure display layout
- `defrag_vers` — set to any non-empty string to enable timer HUD draw guard

**playerState_t fields**:
- `ps.stats[STAT_DEFRAG_FLAGS]` bit 0x4 — SET (not toggled) while timer running
- `ps.stats[STAT_DEFRAG_FLAGS]` bit 0x8 — toggle once on race finish
- `ps.stats[STAT_DEFRAG_FLAGS]` bit 0x10 — toggle once on checkpoint
- `ps.ammo[1]` = -1 (sentinel)
- `ps.ammo[2]` = 100 (race active) or 200 (no race)
- `ps.stats[2]` = 6 (race config)
- `ps.stats[3]` = 0 (race active) or 200 (inactive)

---

## VM data segment usage by cgame

Cgame uses static addresses within its own VM data to cache timer state:

| Address | Size | Purpose |
|---------|------|---------|
| `0x11b000` | 4 | Draw guard (must be non-zero for timer HUD to draw) |
| `0x11c320` | 4 | Timer mode enable flag (checked at draw time) |
| `0x11d298` | 4 | Defrag version detection flag (set by `defrag_vers` cvar read) |
| `0x11d5a4` | 4 | Shared state (unclear exact purpose) |
| `0x11d5b0` | 0x84 | Start timer state data buffer (from `df_ndm_timer_start` cvar) |
| `0x11d630` | 4 | Current stage index |
| `0x11d634` | 0x84 | Stop timer state data buffer (from `df_ndm_timer_stop` cvar) |
| `0x11d6b8` | 0x84 | Checkpoint state data buffer (from `df_ndm_timer_checkpoint` cvar) |
| `0x11cdec` | 0x50+ | Timer struct (elapsed ms, start time, rate, state) |
| `0x11ccc4` | 4 | Display scale/alpha cvar cache |

These are filled by `func_0x4e9c` which reads from SERVERINFO cvars and parses their values.

---

## Functions of interest

| Address | Name | Purpose |
|---------|------|---------|
| `0x2A12` | `CG_Draw2D` | Main DeFRaG draw entry point; intercepted by mdd proxy at 0x2A36 |
| `0x259a` | (interpolation) | Computes `elapsed = base + rate * (commandTime - startTime) / 1000` |
| `0x4f8c` | (timer state getter) | Returns `*(0x11d5b0 + *(0x11d630)*4)` |
| `0x4e9c` | (timer state loader) | Reads SERVERINFO cvars and populates timer state array |
| `0x3c33` | (cvar detector) | Reads `defrag_vers` cvar; sets `*(0x11d298) = 1` if non-empty |

---

## Next steps

- [ ] Find exactly what `*(0x11b000)` should be set to (currently only verified that non-zero = draw enabled)
- [ ] Trace `func_0x4e9c` completely to understand cvar-to-state parsing
- [ ] Verify `ps.persistant[14]` (ps+0x130) value requirements
- [ ] Test timer HUD display with current SERVERINFO cvar setup
