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

## `trigger_hurt` touch `0x27d3c` — per-client interval (decoded + PORTED)
Spawn table: `trigger_hurt` (spawn `0x27df3`) sets `touch (+0x2c0) = 0x27d3c` (hash-DIFF from vanilla `hurt_touch`) and `use = 0x27d24` (vanilla `hurt_use`). Body: `if (!other->takedamage) return;` then **`if other->client: slot = self->perClientPickupTime[other->client->ps.clientNum] (self+0x32c, client+0x8c); if (slot > level.time) return; slot = level.time + interval; else: if (self->timestamp (+0x280) > level.time) return; timestamp = level.time + interval`** with `interval = (spawnflags & 16) ? 1000 : 100`; then `if (!(spawnflags & 4)) G_Sound(other, CHAN_AUTO, self->noise_index)` (`0x28694`, `self+0x318`); `dflags = (spawnflags & 8) ? DAMAGE_NO_PROTECTION : 0`; `G_Damage(other, self, self, NULL, NULL, self->damage (+0x2e4), dflags, MOD_TRIGGER_HURT (0x16))` (`0x2015b`). Everything but the per-client interval equals vanilla. The respawn sweep (`func_0x2830`) resets this slot to 0 for each hurt trigger.
oDFs: PORTED (`hurt_touch` in g_trigger.c; syntax-checked, untested). Effect: with several players in one hurt brush, each takes damage on their own 100 ms/1 s clock (vanilla: the first toucher each frame consumes the shared timestamp, so other players are damaged less often). Revert = restore the `self->timestamp` test.

### Touch/think/use/blocked sweep result (same method as the use-fn sweep)
Non-vanilla (hash-DIFF) entity callbacks in the DeFRaG spawn table: `trigger_multiple` touch `0x274db` (done: Touch_Multi_1mstimer.md), `trigger_push`/`push_velocity` touch `0x278ef` + think `0x27930` (done: trigger_push_touch.md), `trigger_teleport` touch `0x27bf9` (done: trigger_teleporter_touch.md), **`trigger_hurt` touch `0x27d3c` (done above)**, plus the target_* use fns already documented. All other callbacks hash-match vanilla (`Touch_Plat`, `Blocked_Door`, `Touch_Button`, `Reached_Train`, `Think_SetupTrainTargets`, `func_timer_*`, `trigger_always_think`, `Use_Multi`, `hurt_use`, `Use_target_remove_powerups`, `Use_Target_Delay`, `Use_Target_Speaker`, `target_laser_start`, `target_relay_use`, `target_kill_use`, `target_location_linkup`, `Use_target_push`, `locateCamera`).
