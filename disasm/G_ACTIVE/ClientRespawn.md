# ClientRespawn / ClientBegin / CopyToBodyQue — confirmed

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

This supersedes the prior wrong guesses that `0x1cb0b` was "ClientThink" and
`0x1c937` was its "cmd fetch" helper (see
[README.md#clientthink--g_runclient-investigation](README.md#clientthink--g_runclient-investigation)).

---

## ClientRespawn — `0x1cb0b`

**Called from:** two sites inside **Pmove outer** (`0x1a866`, at `0x1ac2a` and `0x1ac4e`),
both gated on `ps.stats[STAT_HEALTH] <= 0` (qagame.dis:185046, `gti -2 0x1ac53` skips
both call sites when health > 0).

```c
void ClientRespawn( gentity_t *ent ) {   // 0x1cb0b
    CopyToBodyQue( ent );                 // func_0x1c937
    ClientSpawn/ClientThink_real( ent );  // 0x1d27b — see open question below
}
```

The two call sites in Pmove outer exactly match oDFs's `ClientThink_real`
dead-player block ([g_active.c:1096-1109](../../code/game/g_active.c#L1096)):

**Site 1 (`0x1ac0e`-`0x1ac2a`) — forced-respawn timer:**
```
if ( g_forcerespawn.integer > 0 &&
     ( level.time - client->respawnTime ) > g_forcerespawn.integer * 1000 )
    ClientRespawn( ent );
```
- `0x19fdc` = `g_forcerespawn` cvar cache (int, confirmed by the `> 0` guard and
  `* 1000` scaling matching the source exactly)
- `client+0x330` = `client->respawnTime` (int) — corrects the earlier vaguer note
  "receives level.time each frame" in `ClientThink_real.md`
- `0x12a310` = `level.time` (confirmed independently multiple times now — also used
  as a `level.time`-typed value in `func_0x1c937`/CopyToBodyQue, see below)

**Site 2 (`0x1ac2f`-`0x1ac4e`) — attack/use button respawn:**
```
if ( ucmd->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) )  // mask 0x5 = 1|4
    ClientRespawn( ent );
```
`cmd->buttons` read from `cmd+0x10` (`cmd` = `client+0x1d8`, already confirmed).
Mask `0x5` = `BUTTON_ATTACK(1) | BUTTON_USE_HOLDABLE(4)`, matching exactly.

**Architectural note:** in oDFs/vanilla this whole block lives inside
`ClientThink_real`. In DeFRaG it has been relocated into **Pmove outer**
(`0x1a866`), which already absorbs `ClientEndFrame`-equivalent logic (see
[Pmove_outer.md](Pmove_outer.md)) — this is now a second confirmed instance of
DeFRaG merging per-frame client bookkeeping into the Pmove outer function rather
than keeping it in a `ClientThink_real`-shaped function.

---

## CopyToBodyQue — `func_0x1c937`

**Called from:** `0x1cb0b` (ClientRespawn) only, and `0x1e0fb` (not yet examined).
**Args:** `arg0` = ent (gentity_t*)

Structural match to vanilla Q3 `g_client.c` `CopyToBodyQue`:

```c
void CopyToBodyQue( gentity_t *ent ) {
    trap_UnlinkEntity( ent );                        // trap -0x20 on ent, confirmed
    contents = trap_PointContents( ent->???, -1 );    // trap -0x1a on ent+0x5c, -1
    // ... nodrop check not confirmed decoded ...

    body = bodyQueSlots[ ringIndex ];                 // 8-slot pool at 0x12c6cc,
    ringIndex = (ringIndex + 1) % 8;                  // index counter at 0x12c6c8
    trap_UnlinkEntity( body );                        // trap -0x20 on body

    block_copy( body, ent, 0xd0 );                    // bulk copy of entity fields
    body->flags = 1;                                  // ent+0xbc = 1 (unconfirmed field)
    body->? = 0;                                       // ent+0x9c = 0 (unconfirmed field)
    body->clientNum = (body - g_entities) / 0x43c;    // body->s.number-style self-index
    body->tracemask = MASK_PLAYERSOLID;               // 0x23c = 0x10001
    body->svFlags |= 0x4000000;                        // 0x1cc — SVF_USE_CURRENT_ORIGIN-like
    body->ownerNum = *(int*)ent;                        // 0x200 = *ent (first 4 bytes)
    body->nextthink = level.time + 5000;                // 0x2b0 = *(0x12a310) + 0x1388
    body->think = 0x1c905;                               // 0x2b4 — function pointer constant
    body->die  = 0x1faca;                                // 0x2cc — function pointer constant
    body->takedamage = (ent->health > 0) ? 0 : 1;        // 0x2e0, ent->health at ent+0x2dc
    body->r.currentOrigin = body-><0x18-field>;           // 0x1e8 = block_copy from body+0x18
    trap_LinkEntity( body );                              // trap -0x1f
}
```

Matches `BODY_QUEUE_SIZE = 8` (vanilla's exact body-queue size), the
`nextthink = level.time + 5000` / `think = BodySink`-style callback assignment, and
the `takedamage` decision based on `ent->health` — all textbook `CopyToBodyQue`.
`ent+0x9c`, `ent+0xbc`, and the exact semantics of the `0x1c905`/`0x1faca` function
pointers were **not decoded** (out of scope — not movement-relevant); marking this
identification as **strong but not fully exhaustive**, consistent with project rules
against asserting unconfirmed field purposes.

---

## ClientBegin — `0x1d18b`

**Called from:** vmMain dispatch (`0x62`, near file start — the low-address dispatch
table pattern already seen for other `GAME_*` entry points), plus several other
internal callers.

Confirmed via two debug strings decoded directly from the bytecode:
- `0x7ddf` = `"ClientBegin: %i"` (passed to an unidentified logging call at `0x1d273`)
- `0x7df0` = `"%s^7 entered the game."` (passed to a server-command broadcast at `0x1d263`)

Also confirms the classic Q3 `ClientBegin` eFlags-preserving memset idiom exactly:
```c
flags = client->ps.eFlags;                 // save *(client+0x68)
memset( &client->ps, 0, sizeof(ps) );      // trap -0x65(client, 0, 0x1d4)
client->ps.eFlags = flags;                 // restore *(client+0x68)
ClientSpawn( ent );                        // call 0x1d27b — see open question below
```
`0x1d4` (468) as the memset size is consistent with `playerState_t`'s size.
`client+0x1d4 = pers.connected` is set to `2` (`CON_CONNECTED`) at `0x1d1c8`,
matching vanilla's `client->pers.connected = CON_CONNECTED;` in `ClientBegin` —
independent confirmation of the `pers.connected` offset found via
[PickTeam.md](PickTeam.md).

---

## Resolved: `0x1d27b` is `ClientSpawn`

**Explanation 2 below was correct.** The real per-command dispatcher was found —
see [ClientThink.md](ClientThink.md) — at `0x1ac8f`, called directly by the engine
(`vmMain`'s `GAME_CLIENT_THINK`) and structurally an exact match for vanilla's
`ClientThink(clientNum)` (`trap_GetUsercmd`, `lastCmdTime`, bot/synchronous-clients
gate). It calls `0x1a866` — the function documented at length in this project as
"Pmove outer" — which is therefore DeFRaG's actual `ClientThink_real`.

`0x1d27b`'s cmd-acquisition/Pmove/`ClientImpacts` content is explained by its call
to `0x1ac8f` at the end of its own body (already shown in the call chain above as
`ClientSpawn/ClientThink_real( ent )`): DeFRaG's `ClientSpawn` finishes by
immediately running one full `ClientThink` cycle post-spawn, rather than waiting
for the next network command — it isn't a merge of the two functions, it's
`ClientSpawn` *calling* the real `ClientThink`.

<details>
<summary>Original open question (kept for history)</summary>

`0x1d27b`'s only two callers in the entire QVM are `0x1cb0b` (ClientRespawn) and
`0x1d18b` (ClientBegin) — never a plain per-command engine dispatcher. Two
explanations were considered: (1) a DeFRaG-specific merge of `ClientSpawn` +
`ClientThink_real`, or (2) `0x1d27b` really is `ClientSpawn`, DeFRaG-extended to
also perform an initial physics settle, with the actual per-command
`ClientThink_real` being a different, not-yet-located function. (2) is confirmed.
</details>
