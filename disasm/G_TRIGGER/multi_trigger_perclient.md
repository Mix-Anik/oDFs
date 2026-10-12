# DeFRaG `multi_trigger` (`0x273a5`) -- per-client trigger wait; and the respawn timer reset

`0x273a5(ent, other, activator)` = DeFRaG's `multi_trigger` (called by the `trigger_multiple` touch wrapper `0x274db` and by `Use_Multi`). Differences from vanilla (decoded from `0x273a5`-`0x274cd`):
- **Clients**: the wait is per client in the `ent+0x32c` int array (the same `perClientPickupTime[64]` slot items use): `if (t == -1 || level.time < t) return;`. Non-client activators still use `nextthink` (`0x2739c` = `multi_wait`, `nextthink = 0`).
- After `G_UseTargets`: `wait > 0` -> client: `array[clientNum] = level.time + (wait + random*crandom())*1000`; non-client: `think = multi_wait`, `nextthink = ...`. `wait <= 0` (one-shot) -> client: `array[clientNum] = -1` (only this client is locked out); non-client: free the trigger next frame (vanilla).
- Team spawnflag gates (`1` red only / `2` blue only) as vanilla, only checked for client activators.
This is why the respawn sweep (`func_0x2830`, G_CLIENT/func_0x2830.md) resets `trigger_multiple`/`trigger_hurt` `+0x32c[clientNum]` to 0.

## Respawn resets the race timer (`func_0x274b` -> `Reset_0x3597`)
Every `ClientSpawn` (incl. `kill`) zeroes the client's `raceTimer_t` (DeFRaG keeps only the touch-driven `state`/`fracDelta` words; in oDFs `TIMER_RUNNING` lasts the whole run so the full record is cleared). The stats[12] bits are cleared with the ps wipe, so the stock cgame sees the run stop; the timer restarts when the start trigger is touched again.

## oDFs -- PORTED
`g_trigger.c` `multi_trigger` (per-client wait, uses `perClientPickupTime`); `g_client.c` `ClientSpawn`: `memset( G_GetTimerRecord( client ), 0, sizeof(raceTimer_t) )`.
Verified headless (`df_dbg_respawn 4000`, HUD screenshots): after every respawn the timer restarts from 0 and `Race started` is logged again (12 starts for 12 respawns; before the per-client wait the one-shot start trigger never fired again).
**Behaviour change:** `trigger_multiple` waits are now per player (matters only with several clients on one server); respawn stops/resets the run. Revert: restore vanilla `multi_trigger` / delete the memset in `ClientSpawn`.
