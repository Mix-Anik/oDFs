# ui QVM — "exit after demo playback" (`df_demos_exitAfterPlayback`)

**Address:** `ui 0x1a92` (no args; called from `0x1abe`, `0x1aee`, `0x1b11` — three branches of the demo-state handler `0x1ab2`).
Trap numbering for ui (`ui_public.h`: `-(id+1)`): `-5` = `trap_Cvar_VariableValue` (id 4, float result — hence the `cvfi`), `-0xd` = `trap_Cmd_ExecuteText` (id 12).
```c
void UI_ExitAfterDemo( void ) {                         // 0x1a92
    if ( (int)trap_Cvar_VariableValue( "defrag_demoPlayback" ) == 0 ) return;
    if ( (int)trap_Cvar_VariableValue( "df_demos_exitAfterPlayback" ) == 0 ) return;
    trap_Cmd_ExecuteText( EXEC_APPEND /*2*/, "wait; wait; quit\n" );
}
```
`0x1ab2` first reads `defrag_dmq_playback` (the DeFRaG `.dmq` demo-format flag) and calls `0x1a92` only when it is 0 (i.e. for ordinary demos). So: when a DeFRaG demo finishes and `df_demos_exitAfterPlayback 1` is set, the client quits after two frames. Default `0` (registered by the cvar-default block around `0x95a`, see `ui_cvar_table.md`; also `df_demos_validation` `0`).
The ~20 other `df_demos_*` cvars (`CameraMode`, `ReplayMode`, `CamSeq*`, `CamOffset*`, `Seek` …) appear as quoted strings at `ui.dis` ~30828-31300: a table of cvar names bound to demo-camera menu items (UI-only plumbing; the logic lives in the engine/cgame side).
No oDFs impact (players run the original DeFRaG ui/cgame).

**Iter 13 (full-port loop) — port blocked:** `0x1ab2` (frame size 0x7d34) is a demo/`.dmq` loader: reads `defrag_dmq_playback`; if 0 it calls `0x1a92` and returns; otherwise it FS-opens/reads a file (traps `-0xe`/`-0xf`/`-0x11`), clears the cvar, closes, and EXEC_APPENDs a formatted command (`0x538c` fmt) before re-initialising menus (`0x1b67`); the other two `0x1a92` call sites are its error branches. oDFs ui has no such loader, so hooking `UI_ExitAfterDemo` would be dead code. Revisit after decoding the loader's command format (`0x538c`) and finding the oDFs ui entry point that starts demos.
