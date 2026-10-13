# DeFRaG `target_smallprint` (spawn `0x2c36`, use `0x2bfb`) + cgame `cpsm`

- Spawn table entry (`data 0x2734`): `ent->use (+0x2c4) = 0x2bfb`, nothing else.
- Use: `if (ent->spawnflags (+0x210) & 8) SendServerCommand(-1, va("cpsm \"%s\"", ent->message /*+0x27c*/)) else SendServerCommand(activator - g_entities, same)` (traps `-0x12`, `va` = `0xac4e`, string `0x3e5c`).
- **cgame** (`cgame 0x3d8a`, server-command dispatch): `strcmp(cmd, "cpsm") == 0` -> `if (df_noTargetPrint.integer == 0) CG_CenterPrint(CG_Argv(1), 200, 10)` (`0x1c707`; vanilla `"cp"` uses y = 0.30*480, charWidth 16); `df_noTargetPrint` = cgame cvar (default "0", table entry at data `0x720`, storage `0x11bcb4`); returns "handled" either way.

## oDFs — PORTED
`g_target.c` `Use_Target_SmallPrint`/`SP_target_smallprint` (+ spawn table), cgame `cg_servercmds.c` `cpsm` handler, `df_noTargetPrint` cvar (ARCHIVE, default 0) in `cg_main.c`/`cg_local.h`. Syntax-checked (game; cgame needs the `Q_PRINTF_FUNC` defines), untested.
**Behaviour change:** none for existing maps (new entity + new server command). Revert = remove the spawn entry and the `cpsm` branch.

## Correction (spawn-table sweep): DeFRaG `target_print` ITSELF uses `cpsm`
A script over the spawn table (name ptr, fn ptr pairs at data `0x2604..0x27d4`) resolving each spawn fn's `ent->use` store and comparing hashes with the vanilla table shows `target_print` (spawn `0x254c0`) sets `use = 0x25485` — a DIFFERENT, non-vanilla function with exactly the `target_smallprint` body: `if (spawnflags & 8) SendServerCommand(-1, "cpsm \"%s\"") else SendServerCommand(activator - g_entities, same)` (message = `ent+0x27c`). So in DeFRaG every `target_print` is a small `cpsm` print (this is what the cgame cvar `df_noTargetPrint` suppresses); the vanilla team (1/2) and private (4) spawnflags are not implemented. `target_smallprint` is a second class with the same behaviour.
**PORTED** (this pass): oDFs `Use_Target_Print` now sends `cpsm` exactly like `Use_Target_SmallPrint`. BEHAVIOUR CHANGE for maps that use `target_print`: small text instead of centre-print `cp`, and spawnflags 1/2/4 ignored. Revert = restore the vanilla body.
Other spawn-table use fns that differ from vanilla and are still UNREAD in this sweep: `target_score` use `0x25457`, `target_teleporter` use `0x25881` (see `G_UTILS/vanilla_identified_survey.md`). The class-name set of the DeFRaG spawn table (74 names) equals oDFs's (no missing classes).
