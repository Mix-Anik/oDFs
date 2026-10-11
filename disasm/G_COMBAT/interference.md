# `df_mp_InterferenceOff` — player-to-player interference switch

**Cvar:** default `3`, flags `0x28` = SYSTEMINFO|LATCH, vmCvar `0x132654` (`.integer` at `0x132660`). Callvote names (see [../G_CLIENT/callvote.md](../G_CLIENT/callvote.md), validator `0x30b8`, value `0..3`):
`0` "full interaction", `1` "interaction only with missiles", `2` "interaction except for missiles", `3` "no interaction". So **bit 0 = non-missile player interaction OFF** and
**bit 1 = missile interaction OFF** (value 1 leaves only missiles interacting, 2 leaves everything but missiles).

## Consumers found (cvar `.integer` loaded at `0x132660`)
| Site | Function | Use |
|---|---|---|
| `0x1aa55` | `ClientThink_real` (`0x1a866`) | `func_0x4239(&pm, value)` — see below |
| `0x4650` | `func_0x45f0` (sole caller `0x468e` = `placeplayer`) | `if (!(value & 1)) G_KillBox(ent)` (`0x28534`) — `placeplayer` telefrags only when player interaction is on |
| `0x1d5bc` | `ClientSpawn` (`0x1d27b`) | `if (!(value & 1)) G_KillBox(ent)` — spawn telefrag only when interaction is on |
| `0x2196c` | `TeleportPlayer` (`0x21864`) | `if (type != 3 && !(value & 1)) G_KillBox(ent)` (the `type != 3` operand is the spawner/teleporter variant flag; exact meaning not read) |
| `0x1c571` | `func_0x1c56f` (proximity/"crowded spawn" check) | `if (value & 1) return 0` — spawns are never considered crowded when player interaction is off (see [../G_COMBAT/func_0x1c56f.md](func_0x1c56f.md)) |
| `0x43c4` | `func_0x43c3` (missile trace helper, sole caller `0x223cb` in the missile code) | `if (value & 2)` → plain `trap_Trace` (`-0x19`) with the missile's own origin/mins/maxs/`clipmask`; else a longer path (loop over entities, `ent->parent`/client checks) — **not decoded** |
| `0x2017a` | `G_Damage` (`0x2015b`) | `if (value & 2)` and `targ` is a client (`targ - g_entities < 64`) and `attacker != targ` and the inflictor isn't owned by `targ` → a suppression path (remainder not read this pass; presumably damage between players is ignored) |
| `0x3684` | race-timer function `0x35ce` | passes the value as an extra argument to a call (not decoded) |

## `func_0x4239(pm, value)` (called from `ClientThink_real`, args `(&pm, df_mp_InterferenceOff)`)
```c
pm->ps->stats[14] = 0;                    // ps+0xf0 (stats base +0xb8, index 14)
if ( value )         pm->ps->stats[14] = value;     // networked so cgame prediction sees it
if ( value & 1 )   { pm->ps->stats[14] = value;  pm->tracemask &= ~CONTENTS_BODY; }   // -0x2000001 = ~0x2000000
```
So with bit 0 set players no longer collide with each other in `Pmove` (and the networked `stats[14]` lets prediction do the same).
Also in `ClientThink_real`: `pm.tracemask` is `0x10001` (SOLID|`0x10000` PLAYERCLIP) for `ps.pm_type == PM_SPECTATOR`-like value 3, `0x2410001` when `ent->r.svFlags & SVF_BOT (8)`,
else `MASK_PLAYERSOLID 0x2010001`; `func_0x4239` then strips `CONTENTS_BODY` when bit 0 is set. (This explains the `stats[14]` slot, which oDFs currently leaves unused.)

## Decoded this pass
- **`G_Damage`** (after the vanilla "default inflictor/attacker to world" lines, before the shootable-mover handling): with `value & 2`, if `targ` is a client (`index < 64`), `attacker != targ`, and `inflictor->parent != targ`:
  return (no damage) if `attacker` is a client, or if `inflictor->parent` is a client. Self damage and own-missile splash stay allowed.
- **`func_0x43c3(ent, end, trace*, &passEntityNum)` = the missile trace in `G_RunMissile`** (sole caller `0x223cb`): if `!(value & 2)` plain `trap_Trace(ent->r.currentOrigin, mins, maxs, end, *passent, ent->clipmask)`.
  Else: if the owner (`ent->parent`) is missing / not a connected client → trace with contentmask `CONTENTS_SOLID (1)`. Otherwise, for all 64 client slots set `r.contents = 0` on every in-use client other than the owner whose
  contents were exactly `CONTENTS_BODY (0x2000000)`, trace with mask `SOLID|BODY (0x2000001)`, then restore: every in-use client with `contents == 0` gets `CONTENTS_BODY` back (this over-restores spectators and
  other zero-contents slots, a quirk of the original). Net effect: missiles ignore other players' bodies.
- `func_0x1c56f` is DeFRaG's **`SpotWouldTelefrag`** (EntitiesInBox + client check) with an early `return 0` when `value & 1` (earlier docs called it only a "proximity check").

## oDFs status
**Ported** (game files syntax-check clean; `cg_predict.c` clean with the pre-existing `Q_PRINTF_FUNC`/`Q_NO_RETURN` macros defined on the command line; not run in a game):
cvar `df_mp_InterferenceOff` (**default `"3"` as in DeFRaG** — flags SYSTEMINFO|LATCH; set it to `0` in `g_main.c`'s table to restore vanilla full interaction), `STAT_INTERFERENCE = 14`
(`bg_public.h`) networked in `ClientThink_real` and used by `cg_predict.c`; `pm.tracemask &= ~CONTENTS_BODY` when bit 0; no `G_KillBox` in `ClientSpawn`/`TeleportPlayer` and `SpotWouldTelefrag == false` when bit 0;
`G_Damage` block and `G_MissileTrace` (`g_missile.c`, with a deviation: only the clients it changed are restored) when bit 1. Not ported: the `placeplayer` kill-box gate (oDFs's `placeplayer` has no `G_KillBox`), the
race-timer function's extra argument (`0x35ce`, undecoded), the `DeFRaG`-side effect of `stats[14]` in cgame beyond the tracemask.
**Behaviour change:** with the default, players no longer collide, telefrag or damage each other.
