# `g_killWallbug` — the wallbug velocity-kill in `PM_StepSlideMove`

**Resolves** the previously undocumented cvar `g_killWallbug` (default `1`, `CVAR_SERVERINFO | CVAR_ARCHIVE`, see [../G_UTILS/cvar_table.md](../G_UTILS/cvar_table.md)).

## Evidence
- `ClientThink_real` (`0x1a866`, bytecode `0x1aa61`-`0x1aa62`) copies `g_killWallbug.integer` (vmCvar `0x18980`, `.integer` at `+0xc` = `0x1898c`) into **pm struct
  offset `+0x20`** (frame local `0x38`; `pm` starts at local `0x18`, `pm.tracemask` at `0x34` holds `MASK_PLAYERSOLID 0x2010001`). Neighbouring stores identify the layout: the next
  fields are `debugLevel` (`g_debugMove.integer`, `+0x24`) and `noFootsteps` (`dmflags & 0x20`, `+0x28`), and `pmove_fixed` lands at `+0xdc`
  (oDFs's struct has it at `+0xd8`) — so **DeFRaG's `pmove_t` has one extra int, `killWallbug`, between `tracemask` and `debugLevel`.**
- Consumer: `PM_StepSlideMove` (`0x8af9`), bytecode `0x8bd5`-`0x8c01`: in the `trace.allsolid` ("bend can't step") branch, after the `debugLevel` print
  (`pm+0x24`), `if (pm+0x20 /*killWallbug*/) { ps->velocity[0..2] = 0; }` and then the function returns. No promode test.

## oDFs change
oDFs already had `if ( pm->promode ) VectorClear( pm->ps->velocity );` at this spot (likely from openpromode). **Replaced the condition with `pm->killWallbug`**
(`bg_slidemove.c`), added `int killWallbug` to `pmove_t` (`bg_public.h`), registered the `g_killWallbug` cvar (default `"1"`, ARCHIVE|LATCH in DeFRaG, kept SERVERINFO|ARCHIVE in oDFs so cgame can read it from serverinfo, `g_main.c`), set
`pm.killWallbug = g_killWallbug.integer` in `ClientThink_real` (`g_active.c`), and mirrored it for prediction: `cgs.killWallbug` parsed from serverinfo
(`cg_servercmds.c`) and `cg_pmove.killWallbug` in `cg_predict.c`.
**Behaviour change:** with the default `1`, VQ3 (promode 0) servers now also clear velocity in this case, and promode servers can switch it off with `g_killWallbug 0`.
Game-side files syntax-check clean; the cgame files only with the pre-existing `Q_PRINTF_FUNC`/`Q_NO_RETURN` macros defined on the command line (unrelated gap
in this check setup, `cg_main.c` fails identically). Not run in a game.
