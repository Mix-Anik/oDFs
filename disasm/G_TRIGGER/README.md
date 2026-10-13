# G_TRIGGER — Defrag QVM disassembly analysis

Analysis of DeFRaG trigger and target entity functions from `g_trigger.c` equivalents.

For general disassembly methodology, struct layouts, and global addresses see [../README.md](../README.md).

---

## Analyzed modules

| Module | File | Status |
|--------|------|--------|
| Race timer entity system | [race_timer_system.md](race_timer_system.md) | ✅ Analyzed |
| G_InitRaceMode | [func_0x1e60.md](func_0x1e60.md) | ✅ Analyzed |
| Per-frame race timer tick | [func_0x37c7.md](func_0x37c7.md) | ✅ Analyzed |
| `SP_trigger_push_velocity` (new DeFRaG classname, spawn-side config for `BG_TouchJumpPad`'s custom modes) | [../BG_MISC/BG_TouchJumpPad.md](../BG_MISC/BG_TouchJumpPad.md#spawn-side--resolved-new-classname-trigger_push_velocity) | ✅ Analyzed — **Implemented** |
| `trigger_push_touch` (`df_ndm_nopad` instant-kill gate, shared by all jump pads) | [trigger_push_touch.md](trigger_push_touch.md) | ✅ Analyzed — **Implemented** |
| `trigger_teleporter_touch` (`df_ndm_noteleport` inert/instant-kill gate) | [trigger_teleporter_touch.md](trigger_teleporter_touch.md) | ✅ Analyzed — **Implemented** |
| `func_0x20e0` (level-init entity-culling sweep, gametype/promode/dedicated/nomover) | [../G_SPAWN/entity_mode_exclusion.md](../G_SPAWN/entity_mode_exclusion.md) | ✅ Decoded (moved to its own module, `G_SPAWN/`, since the full system spans `g_spawn.c`) — **not ported** (bit-polarity direction still unresolved) |
| `func_0x274b` (spawn-time weapon/health/armor/ammo loadout) | [func_0x274b.md](func_0x274b.md) | ✅ Fully analyzed — **Fully implemented**. `func_0x4b6a` resolved as `BG_FindItemForWeapon` (confirmed via exact error string + `gitem_t` field offsets); the `df_ndm_weaponsPickup`-gated ammo logic is now ported |

---

## Key finding: df_ndm_* are cvars, not classnames

`df_ndm_timer_start`, `df_ndm_timer_checkpoint`, `df_ndm_timer_stop` are **cvars**
used internally to detect entity presence. The actual map editor entity classnames are
`target_startTimer`, `target_stopTimer`, `target_checkpoint`.
- [target_multimanager.md](target_multimanager.md) — DeFRaG target_multimanager (ported)
- [target_speed.md](target_speed.md) — DeFRaG target_speed (ported)
- [target_smallprint.md](target_smallprint.md) — target_smallprint + cgame cpsm (ported)
- [target_init.md](target_init.md) — DeFRaG target_init (ported)
- [finish_handler.md](finish_handler.md) — finish handler 0x35ce + time formatter 0x13bf (partly ported)
- [multi_trigger_perclient.md](multi_trigger_perclient.md) — per-client trigger wait + respawn timer reset (ported)
- [target_give.md](target_give.md) — Use_Target_Give with perClientPickupTime stores (ported)
- [target_teleporter.md](target_teleporter.md) — KEEP_SPEED flag (ported)
