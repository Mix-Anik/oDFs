# Timers on items / respawn / flags and the item rules of the tricks + fastcaps modes

Decoded from qagame (qdec) in the "next things to port" pass. Corrects the older reading of `func_0x1e60.md` ("ready flag 3/4 = unknown"): the three ready-flag
slots at `0x131808` (start), `0x131810` (checkpoint), `0x131818` (stop) hold `{mode, item*}` pairs (`+4` = pointer into bg_itemlist, i.e. `0x13180c`,
`0x131814`, `0x13181c`):

| mode | meaning | set by |
|---|---|---|
| 0 | not used (entities inert in DeFRaG) | |
| 1 | the map's `target_startTimer` / `_checkpoint` / `_stopTimer` entities (value `df` / run mode) | `0x1de9` (cvar "df"), run mode init |
| 2 | fastcaps: start = pickup of the ENEMY team flag (`0x3b22`), stop = touching the OWN flag (`0x3b5c`) | mode 3 init |
| 3 | `resp`: start at every (re)spawn (`0x274b` calls `0x3933` when the start slot == 3); checkpoint / stop value 3 is discarded | `0x1de9` |
| 4 | an item: `df_ndm_timer_*` holds an alias (`0x3adb`: Q_stricmp against the 34 aliases at data 2684, result = `bg_itemlist[i]` of the same index) | `0x1de9` |

Aliases (index = bg_itemlist index): as ya ra h5 h25 h50 mh gt sg mg gl rl lg rg pg bfg hook asg amg agl apg alg arl arg abfg tp med quad bs hast inv reg fli.

## Pickup hook `0x3b96(player, itemEnt)` (called by Touch_Item `0x20f11` after the pickup switch, before the "picked up?" test)
else-if chain: start==2 && enemy flag -> start; stop==2 && own flag -> stop; item == start item -> start only if the timer is not initialized (`0x3a83`);
item == checkpoint item -> checkpoint index 1 (`0x3978(p, 1)`); item == stop item -> stop, but with a checkpoint item configured only after a checkpoint was
reached (`0x3a97` = count). `0x3c23(item)` = item is one of the three chosen items; Touch_Item then (a) skips `BG_CanItemBeGrabbed` for them and (b) forces
the respawn time to 3 s.

## Item rules (the part that changes the game)
* `0x131828` (itemMask) and `0x13182c` (flags) are bit masks over `giType` (bit n = type n; 0x7e = weapon, ammo, armor, health, powerup, holdable; 0x100 = team items).
  Run mode: both 0x7e. Fastcaps: both |= 0x100. Presets 1-6: 0; preset 7: |= 0x7e. Custom (preset 0): flags from df_ndm_damage (0x18) / powerups (0x60) / weaponsPickup (0x6);
  `df_ndm_itemsSpawn` (default 1): 0 = itemMask 0, 2 = |= 0x7e, otherwise |= flags.
* `FinishSpawningItem` (`0x212a7`) frees every item that fails `0x3c43`: flag 0x1000000 (set on dropped items, `0x1db30`) or a timer item or `itemMask & (1 << giType)`.
  The start item also gets `eFlags |= 2`. Consequence: in tricks mode (auto-detected on maps without timer entities) the default preset spawns NO items.
* Pickup_Powerup / Holdable / Ammo / Weapon / Health call `0x3c8b(ent)` first: when false (dropped items and `flags & (1 << giType)` are true; the holdable teleporter
  is always false) the item is consumed for 120 / 60 / 40 / 5 / 35 s without effect. Pickup_Armor (`0x20ecd`) is NOT gated.
* `defrag_route` (tricks mode) = `"%s-%s%s%s"` of the start / checkpoint / stop names; the three cvars are cleared on a map change, not on `map_restart`.

## oDFs
Ported in g_defrag.c (`G_NdmTimerInit`, `G_NdmItemHook`, `G_IsTimerItem`, `G_NdmBuildRoute`), g_items.c (`G_ItemAllowed`, `G_ItemActive`, hooks), g_spawn.c
(`itemMask`, `df_ndm_itemsSpawn`), g_client.c (respawn start), g_trigger.c (entity use callbacks are inert when that timer event is bound to an item).
Deviations: with the cvars empty oDFs keeps the map's timer entities active in tricks mode (DeFRaG would leave them inert unless the value is "df"); the route shows
an empty name instead of "(null)". New cvar `df_ndmItemPolicy` (default 1) switches the item spawn / effect rules off.
