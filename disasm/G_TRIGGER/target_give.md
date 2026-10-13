# DeFRaG `target_give` use — `0x252fc` (spawn `0x25378`, spawn-table entry `0x26a4` -> name `0x96b9` = `"target_give"`)

**Address:** `0x252fc`. Identification: the qagame spawn table (data `0x26a4`) pairs name ptr `0x96b9` (`"target_give"`, string pool listing) with spawn fn `0x25378`, which sets `ent->use (+0x2c4) = 0x252fc`. Body (args: `ent`, `other`, `activator`):
```c
if ( !activator->client ) return;              // arg2+0x204
if ( !ent->target ) return;                    // ent+0x288
memset( &trace, 0, 0x38 );                     // -0x65 = memset trap
for ( t = NULL; (t = G_Find( t, targetname, ent->target )) != NULL; ) {   // 0x2809c
    if ( !t->item ) continue;                  // t+0x324
    t->perClientPickupTime[ activator->client->ps.clientNum ] = 0;          // t+0x32c + clientNum*4  (client+0x8c = ps.clientNum)
    Touch_Item( t, activator, &trace );        // 0x20f11
    t->perClientPickupTime[ activator->client->ps.clientNum ] = -1;
    t->nextthink = 0;                          // t+0x2b0
    trap_UnlinkEntity( t );                    // -0x20
}
```
= vanilla `Use_Target_Give` plus the two `perClientPickupTime` stores (DeFRaG's per-client item cooldown array, see `G_CLIENT/func_0x2830.md` respawn sweep and `G_TRIGGER/multi_trigger_perclient.md`): the item is made pickable *for this client* just for the touch and then marked taken (-1) so it can't be picked again by the normal touch path.
oDFs: PORTED (`Use_Target_Give` in g_target.c; syntax-checked, untested). Revert = delete the two stores.
Spawn-table lookup tip: data entries are `name_ptr, fn_ptr` word pairs near `0x2600..0x2740`; names resolve through the `"..."` string-pool lines at the end of `qagame.dis` (`0x0000xxxx  "name"`).
