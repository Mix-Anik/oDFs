# entityState fields the DeFRaG cgame reads that the server must supply (audit part 4)

## Per-client item availability — `IsItemEntityAvailableToClient` (cgame `0x4e14`) — VERIFIED
**Address:** `0x4e14` (debug strings `"IsItemEntityAvailableToClient(): (Warning) entity is not an item."` and `"... clientNum [%i] out of range."`). Called from `CG_Item` (`0x1ea0c`, 4 sites; clientNum argument = the viewed player's `ps.clientNum`, `snap+0xb8`) and from `0x270e6` (the client-side pickup prediction path). Body:
```c
qboolean IsItemEntityAvailableToClient( centity_t *cent, int clientNum ) {
    if ( cent->currentState.eType != ET_ITEM ) { warn; return 0; }
    if ( clientNum < 0 || clientNum >= 64 )     { warn; return 0; }
    if ( clientNum < 32 )  return ( cent->currentState.time  & ( 1 << clientNum        ) ) == 0;
    else                   return ( cent->currentState.time2 & ( 1 << (clientNum - 32) ) ) == 0;
}
```
So **a SET bit in `entityState_t.time` (clients 0–31) / `.time2` (32–63) means "taken / not available to that client"** (the item is drawn dimmed/hidden or not predicted for them; with `df_cl_alwaysDrawItems 1` the draw still happens, see `cgame_cvar_consumers.md`).
oDFs qagame (`g_items.c`, lines ≈430-445; `G_ClientSpawnSweep` in `ClientSpawn`): `Touch_Item` sets the bit (`time |= 1 << clientNum`, `time2 |= 1 << (clientNum-32)`), respawn/`Use_Item` clears it — **same polarity and bit layout**. ✓ No change needed.

## Other entityState fields
The remaining offset scan hits for `s.time/time2/generic1/otherEntityNum*/modelindex2` in cgame are vanilla uses (missile `time2`, score plum `time`, particle effects, `CG_Mover`, team-item display…) except:
- `s.time` in `EV_SCOREPLUM` (score value) — vanilla, and oDFs `ScorePlum` is gated by `df_mode`/`AddScore` (no plums in df mode).
- `s.generic1` is read once, in the validity checker `0x6237` (`ps` side, not an entity field) — covered in `playerstate_contract.md`.
No DeFRaG-specific entity field other than the item taken-bits was found by the scan; entity classes themselves (`trigger_*`, `target_*`) are server-only (not networked as new `eType`s).

## Conclusion (contract audit complete for configstrings, ps, entity items, server commands, info keys)
All DeFRaG-specific server→client inputs identified in the cgame are now matched by the oDFs qagame, except: the scoreboard protocol (fixed this session, `G_CLIENT/scoreboard.md`), cosmetic info keys (`info_keys_contract.md`), and the `stats[12]` clock-drift bits (intentionally unset).
