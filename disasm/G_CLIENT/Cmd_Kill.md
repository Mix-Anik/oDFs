# `0x1e02f` = DeFRaG `Cmd_Kill_f(ent)` (called from `ClientCommand` `0x1f4ba`)

**Address:** `0x1e02f` (size `0x6f`). Reads `ent+0x204` = `ent->client` (gentity layout, see `ClientBegin.md`), `client+0x284` = `sess.sessionTeam`, `client+0x290` = `sess.spectatorState` (layout confirmed in `G_CLIENT/session_data.md`), `ent+0x2dc` = `ent->health`, `ent+0x218` = `ent->flags`.

```c
void Cmd_Kill_f( gentity_t *ent ) {
    if ( sessionTeam == TEAM_SPECTATOR ) {           // == 3
        if ( spectatorState == SPECTATOR_FOLLOW )    // == 2
            StopFollowing( ent );                    // 0x1e360 (by use; body not read)
        else
            ClientBegin( ent - g_entities );         // 0x1d18b = ClientBegin; 0x43c = sizeof(gentity_t)
        return;
    }
    if ( ent->health <= 0 ) return;
    ent->flags &= ~FL_GODMODE;                       // & -0x11
    ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
    player_die( ent, ent, ent, 100000, MOD_SUICIDE );
}
```
(the tail past `health = -999` was read only to the point of the stats/health stores; the `player_die(…, 100000, MOD_SUICIDE)` call is as vanilla by structure, not individually verified.)

Difference vs vanilla (which just returns for spectators): `kill` as a spectator **leaves follow mode, or re-begins the spectator** (respawn at a spawn point). Practical use in DeFRaG: a free spectator can press `kill` to snap back to the start.
oDFs: PORTED (`Cmd_Kill_f` in g_cmds.c; syntax-checked, untested). Revert = restore the bare `return`.
