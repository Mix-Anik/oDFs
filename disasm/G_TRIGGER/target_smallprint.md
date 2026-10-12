# DeFRaG `target_smallprint` (spawn `0x2c36`, use `0x2bfb`) + cgame `cpsm`

- Spawn table entry (`data 0x2734`): `ent->use (+0x2c4) = 0x2bfb`, nothing else.
- Use: `if (ent->spawnflags (+0x210) & 8) SendServerCommand(-1, va("cpsm \"%s\"", ent->message /*+0x27c*/)) else SendServerCommand(activator - g_entities, same)` (traps `-0x12`, `va` = `0xac4e`, string `0x3e5c`).
- **cgame** (`cgame 0x3d8a`, server-command dispatch): `strcmp(cmd, "cpsm") == 0` -> `if (df_noTargetPrint.integer == 0) CG_CenterPrint(CG_Argv(1), 200, 10)` (`0x1c707`; vanilla `"cp"` uses y = 0.30*480, charWidth 16); `df_noTargetPrint` = cgame cvar (default "0", table entry at data `0x720`, storage `0x11bcb4`); returns "handled" either way.

## oDFs — PORTED
`g_target.c` `Use_Target_SmallPrint`/`SP_target_smallprint` (+ spawn table), cgame `cg_servercmds.c` `cpsm` handler, `df_noTargetPrint` cvar (ARCHIVE, default 0) in `cg_main.c`/`cg_local.h`. Syntax-checked (game; cgame needs the `Q_PRINTF_FUNC` defines), untested.
**Behaviour change:** none for existing maps (new entity + new server command). Revert = remove the spawn entry and the `cpsm` branch.
