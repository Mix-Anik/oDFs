# DeFRaG keymaps — `df_keymaps*` (cgame `0xa0be`, iter 45)

Called every frame from the per-frame updater `0x798b` (together with `0x7a4e` demo time-limit/seek, `0xb21d` OB predictor, `0x9d7f` script engine, `0x15c24` accel driver). Decoded with `disasm/tools/qdec.py`.
If `df_keymaps` (TEMP, default 0) == 1: `n = trap_GetCurrentCmdNumber()` (-55), `trap_GetUserCmd(n, &cmd)` (-56); for the six cases (`0x1454` = 6, jump table at data `0x1684`): 0 `forwardmove > 0` -> cvar `df_keymaps_forward`, 1 `forwardmove < 0` -> `df_keymaps_back`, 2 `rightmove > 0` -> `df_keymaps_moveleft`, 3 `rightmove < 0` -> `df_keymaps_moveright`, 4 `upmove > 0` -> `df_keymaps_moveup`, 5 `upmove < 0` -> `df_keymaps_movedown` (note: DeFRaG's literal left/right naming); the cvar's string (`CVAR_VariableStringBuffer`, 1024) is sent with `trap_SendConsoleCommand("%s
")` (-15) when non-empty. All seven cvars are TEMP (not archived), default `0` / empty.
Not decoded: `0x7a4e` (demo time limit/seek with `df_demos_Seek`, writes `trap_Cvar_Set`, strings at `0x8e0b..`), `0x9d7f` (script engine: COM_Parse of a cvar string, per-token conditions; large).
## oDFs
PORTED: `CG_DF_Keymaps` (cg_defrag.c) called from `CG_Draw2D` before the accel driver; cvars registered with the DeFRaG defaults. Behaviour: with `df_keymaps 0` (default) nothing happens. Revert: remove the call in cg_draw.c. Syntax-checked, untested.
