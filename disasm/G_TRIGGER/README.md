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

---

## Key finding: df_ndm_* are cvars, not classnames

`df_ndm_timer_start`, `df_ndm_timer_checkpoint`, `df_ndm_timer_stop` are **cvars**
used internally to detect entity presence. The actual map editor entity classnames are
`target_startTimer`, `target_stopTimer`, `target_checkpoint`.
