# ClientEndFrame / SpectatorClientEndFrame — resolved (was "SpectatorThink / follow loop")

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Address:** `0x1adc9`, called from `0x118c` (a low-address per-client loop, consistent
with vanilla's `G_RunFrame` calling `ClientEndFrame` once per connected client each
server frame) and from `0x1d27b` (`ClientSpawn`, matching vanilla's `ClientSpawn`
calling `ClientEndFrame(ent)` once at the end of spawn setup).

Previously tracked in [README.md](README.md) as "SpectatorThink / follow loop,
identified only." That label was incorrect — this function is `ClientEndFrame`,
and has nothing to do with spectator input/movement (that's handled elsewhere, in
the cmd-acquisition sections of `ClientSpawn`/`ClientThink_real`).

## Decode

```c
void ClientEndFrame( gentity_t *ent ) {            // 0x1adc9
    gclient_t *client = ent->client;                // ent+0x204

    if ( client->sess.sessionTeam == 3 /* TEAM_SPECTATOR */ ) {
        SpectatorClientEndFrame( ent );              // func_0x1acf4
        return;
    }

    // turn off any expired powerups (MAX_POWERUPS = 16, ps.powerups at client+0x138)
    for ( i = 0; i < 16; i++ ) {
        if ( *(client + 0x138 + i*4) < level.time )
            *(client + 0x138 + i*4) = 0;
    }

    if ( *(int*)0x12c698 == 0 /* race mode NOT active */ )
        func_0x19e41( ent );   // not decoded further — presumably P_WorldEffects/P_DamageFeedback-equivalent, gated off during race mode

    if ( (level.time - client->lastCmdTime /* +0x2c4 */) > 1000 )
        ps->eFlags |= 0x2000;   // EF_CONNECTION
    else
        ps->eFlags &= ~0x2000;
}
```

This matches vanilla `ClientEndFrame` almost exactly: the `sessionTeam ==
TEAM_SPECTATOR` early-dispatch to `SpectatorClientEndFrame`, the `MAX_POWERUPS`
expiry loop, and the `EF_CONNECTION` timeout toggle are all structurally identical.
The one DeFRaG addition is gating `func_0x19e41` (whatever performs the
vanilla-equivalent of `P_WorldEffects`/`P_DamageFeedback`/`G_SetClientSound`) off
during active race mode (`0x12c698`) — consistent with race mode wanting to skip
normal damage/hazard processing. `func_0x19e41` itself was not decoded this pass —
out of scope, no indication it touches velocity/knockback.

## `func_0x1acf4` — `SpectatorClientEndFrame`

```c
void SpectatorClientEndFrame( gentity_t *ent ) {    // 0x1acf4
    gclient_t *client = ent->client;

    if ( client->sess.spectatorState /* +0x290 */ == 2 /* SPECTATOR_FOLLOW */ ) {
        int clientNum = client->sess.spectatorClient;   // +0x294
        if ( clientNum == -1 ) { /* level.follow1 */ }
        // ... (not fully re-transcribed; matches vanilla's follow1/follow2 resolution and ps-copy-from-followed-client shape)
    }
    // ... scoreboard pm_flag handling, matching vanilla's tail end
}
```

Confirmed via the `sess.spectatorState == 2` check at the confirmed `client+0x290`
offset, and the immediately-following `+0x294` (`spectatorClient`) read with `-1`
sentinel comparison — matches vanilla's `SpectatorClientEndFrame`'s
`follow1`/`follow2` team-follow resolution shape exactly. Not decoded past this
point — no plausible velocity/knockback involvement (this function only ever copies
`playerState_t` wholesale from the followed client, matching vanilla).

## oDFs comparison

oDFs's `ClientEndFrame`/`SpectatorClientEndFrame`
([g_active.c:1156-1287](../../code/game/g_active.c#L1156)) already implements this
exact structure (dispatch on `sessionTeam == TEAM_SPECTATOR`, powerup expiry loop,
`EF_CONNECTION` toggle, `P_WorldEffects`/`P_DamageFeedback` calls). **No oDFs code
changes needed** — this was a documentation-only correction (wrong function name),
not a behavior gap. One thing to note: oDFs does not currently gate
`P_WorldEffects`/`P_DamageFeedback` off during any "race mode" state (oDFs has no
race-mode global equivalent to `0x12c698`) — worth keeping in mind if DeFRaG's race
mode is ever more fully ported, but not an issue for current oDFs functionality
since there's no race-mode state for it to interact incorrectly with.
