# ClientBegin — DeFRaG address `0x1d18b`

**Address:** `0x1d18b`
**Vanilla address:** `0x15de5`
**Hash match:** ❌ (previously confirmed modified via debug strings)
**Called from:** the engine itself (`GAME_CLIENT_BEGIN` dispatch — no QVM caller found, consistent with this being a `vmMain`-dispatched entry point like `ClientThink`)
**Status:** Fully decoded this pass — modified (one new call + a team-change stat-reset block)

The README's original pass only noted "small additions found" without
detail; this pass fully decoded the function to pin those additions down.

---

## Full decode

```c
void ClientBegin( int clientNum ) {
    gentity_t *ent    = &g_entities[clientNum];     // base 0x1b2f0, stride 0x43c
    gclient_t *client = &g_clients[clientNum];       // base 0x12a2f0, stride 0x37c

    if ( ent->r.linked )          // +0x1a0, confirmed field
        trap_UnlinkEntity( ent ); // trap -0x20

    G_InitGentity( ent );          // 0x283bc, already confirmed elsewhere in this project
    ent->touch = NULL;              // +0x2c0
    ent->pain  = NULL;               // +0x2c8 — NEW/unconfirmed as "pain" by position only,
                                     // matches vanilla's adjacent touch/pain/die layout pattern
    ent->client = client;            // +0x204

    client->pers.connected = CON_CONNECTED;  // +0x1d4 = 2
    client->pers.enterTime = level.time;     // +0x228, level.time confirmed at 0x12a310
    client->pers.teamState_state = 0;        // +0x22c — plausibly TEAM_BEGIN(0), not independently confirmed

    int oldTeam = client->sess.sessionTeam;  // +0x284, confirmed elsewhere

    // --- DeFRaG addition: team-change stat reset ---
    // skipped entirely if oldTeam==TEAM_SPECTATOR(3) OR the persistant
    // team-tracking field already matches sessionTeam (no real change)
    if ( oldTeam != 3 && *(int*)((char*)client + 0x288) != oldTeam ) {
        *(int*)((char*)client + 0x288) = oldTeam;   // NEW/unconfirmed field, +0x288
        *(int*)((char*)client + 0x2ac) = 0;          // NEW/unconfirmed field
        *(int*)((char*)client + 0x2a8) = 0;          // NEW/unconfirmed field
        *(int*)((char*)client + 0x2a4) = 0;          // NEW/unconfirmed field
    }

    // --- vanilla-shaped eFlags-preserving ps wipe ---
    int savedEFlags = client->ps.eFlags;    // +0x68, confirmed elsewhere
    memset( client, 0, 0x1d4 );              // clears ps (client+0 .. client+0x1d3)
    client->ps.eFlags = savedEFlags;          // restore

    // --- DeFRaG addition ---
    func_0x26e8( ent );    // NOT identified, see below

    ClientSpawn( ent );     // 0x1d27b, confirmed address match

    // --- vanilla-shaped "entered the game" effect + broadcast ---
    if ( client->sess.sessionTeam == 3 ) {         // re-read after ClientSpawn may have changed it
        // (no special effect for spectators)
    } else if ( client->ps.persistant[??] /* +0x288 again? */ == oldTeam ) {
        // no team change -> skip the teleport-effect broadcast
    } else {
        // RESOLVED (later pass, bytecode 0x1d257-0x1d26d): this is vanilla's "entered the game" broadcast.
        // 0x1b1ec = g_gametype.integer (vanilla gates on != GT_TOURNAMENT == 1); the outer gate is
        // client+0x284 (sess.sessionTeam) != 3 (TEAM_SPECTATOR); 0xac4e = va(); trap -0x12 = id 17 =
        // G_SEND_SERVER_COMMAND:
        //   trap_SendServerCommand( -1, va( <str@0x7df0 "print \"%s^7 entered the game.\"...">, client->pers.netname /*+0x200*/ ) );
        // Followed by G_LogPrintf("ClientBegin: %i
") (0xa1a, str 0x7ddf) and func_0x6cc() (zero-arg, same
        // spot where vanilla calls CalculateRanks(); not body-verified). Differs from oDFs only by the
        // string text (DeFRaG's has a '.' after "game"; exact trailing-newline layout of the literal not hex-checked).
        if ( g_gametype != GT_TOURNAMENT ) trap_SendServerCommand( -1, va( ... ) );
    }

    G_Printf( "%s^7 entered the game.\n", ... );     // data string 0x7df0, vanilla-shaped broadcast
    G_LogPrintf( "ClientBegin: %i\n", clientNum );     // 0xa1a = G_LogPrintf, confirmed via this
                                                         // project's own top-of-file hash-table listing
    CalculateRanks();                                   // func_0x6cc — CONFIRMED, see func_0x6cc.md
}
```

**Caveat on the conditional structure above**: the exact branch nesting
around the "entered the game" effect (whether it's gated on team ==
spectator, on the persistant-team match, or on `0x1b1ec`) was reconstructed
from the raw branch targets but not triple-checked against every jump
target this pass — the broad shape (vanilla-style conditional teleport
effect + unconditional log line) is solid, the precise nesting is
lower-confidence.

---

## What's genuinely new here (DeFRaG additions vs. vanilla `ClientBegin`)

1. **Team-change stat reset** (`client+0x288`/`+0x2a4`/`+0x2a8`/`+0x2ac`) —
   when a player's team actually changes (and they weren't coming from
   spectator), three fields get zeroed and a fourth is updated to track the
   new team. **Field identities not confirmed** — do not assume these are
   `ps.persistant[]` slots; this pass did not independently verify
   `persistant[]`'s base address, so no `PERS_*` mapping is asserted.
2. **`func_0x26e8(ent)`** — **now decoded, see [auth_handshake.md](auth_handshake.md)**: it stamps the auth-request delay timer (`+0xc = level.time+2500`) and `func_0x3a47` clears the client's `raceTimer_t` (ported). Earlier partial decode: reads a pointer from `ent+0x328` (a
   "team-adjacent" struct not otherwise identified in this project), checks
   two of its fields (`+0x8`/`+0xc`) against each other, and if they don't
   already hold a matching sentinel, writes `level.time + 2500` into
   `+0xc`. Then unconditionally calls `func_0x3a47(client)`. Shape is
   consistent with some kind of per-team cooldown/notification timestamp,
   but not confirmed. **Caller-list oddity**: this function's own `; called
   from` comment lists only `0x1d05a` (`ClientConnect`) — despite being
   directly observed, by this pass, called from inside `ClientBegin`'s own
   body too. Flagged as an observed gap in this disassembler's caller-list
   generation (not the first such gap found in this project), not
   investigated further.
3. Everything else (`G_InitGentity`, `touch`/`pain` clear, `pers.connected`/
   `enterTime` setup, the eFlags-preserving `ps` memset, the final
   broadcast/log) is vanilla-shaped.

---

## oDFs implementation status

**Partly implemented** (the `func_0x3a47` race-timer clear is ported; the auth timer is not, see auth_handshake.md). Original note: both additions are DeFRaG-internal bookkeeping with
no clear gameplay-visible effect identified (a cooldown timestamp and a
stat-reset on team change); porting either without knowing their exact
purpose risks introducing behavior oDFs doesn't actually need. Flagged for
a future pass if `func_0x26e8`/`func_0x3a47` and the four new `gclient_t`
fields are ever pinned down with more confidence.
