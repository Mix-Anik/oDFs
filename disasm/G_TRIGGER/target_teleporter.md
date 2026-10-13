# DeFRaG `target_teleporter` use — `0x25881` (spawn `0x25919`)

**Address:** `0x25881`. Vanilla `target_teleporter_use` plus a keep-speed option. Decode (args `self`, `other`, `activator`):
```c
if ( !activator->client ) return;                         // arg2+0x204
dest = G_PickTarget( self->target );                      // 0x280eb, self+0x288
if ( !dest ) { G_Printf( "Couldn't find teleporter destination\n" ); return; }   // 0x96 = G_Printf
if ( self->spawnflags & 1 )                               // self+0x210
    speed = VectorLength( client->ps.velocity );          // 0x8d63 on client+0x20
TeleportPlayer( activator, dest->s.origin /*+0x5c*/, dest->s.angles /*+0x74*/ );   // 0x21864
if ( self->spawnflags & 1 ) {
    VectorNormalize( client->ps.velocity );               // 0x9d12
    client->ps.velocity[0..2] *= speed;                   // mulf on client+0x20/+0x24/+0x28
}
```
(`0x8d63`/`0x9d12` identified by use — one float-returning call on the velocity vector, one in-place call followed by three scalings; their own bodies were not read.) Net effect: **spawnflag 1 = KEEP_SPEED** — the player leaves with the speed they entered with, along the destination's facing, instead of `TeleportPlayer`'s fixed exit speed (400 ups).
oDFs: PORTED (`target_teleporter_use` in g_target.c; syntax-checked, untested). Behaviour change only for `target_teleporter` entities with spawnflag 1 (vanilla ignores it). Revert = delete the `spawnflags & 1` block.
