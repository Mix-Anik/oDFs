# ClientThink — the real per-command dispatcher (resolved)

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x1ac8f`
**Called from:** `0x0` (i.e. **the engine itself**, via `vmMain`'s `GAME_CLIENT_THINK`
dispatch — not from any other QVM function; this is why the disassembler's
caller-tracking shows a placeholder `0x0` instead of a QVM address) and, separately,
directly from `0x1d27b` (see [ClientRespawn.md](ClientRespawn.md) — that direct call
is `ClientSpawn`'s own inline first physics tick, not the per-command path).

---

## Why this closes the long-standing open question

Every prior pass in this project ([G_ACTIVE/README.md](README.md)'s "ClientThink /
G_RunClient investigation" section, [ClientRespawn.md](ClientRespawn.md)'s
"open question") had established that `0x1d27b` (previously mislabeled
`ClientThink_real`) has only two callers — both spawn-related — and could not be the
genuine per-command dispatcher. The real one was never located. It has now been found
by following `0x1ac8f`'s **own** (not its callees') identity, which was previously
only known as "the Pmove wrapper" referenced from inside `0x1d27b`.

## Full decode

```c
void ClientThink( int clientNum ) {          // 0x1ac8f
    gentity_t *ent = &g_entities[clientNum]; // (0x43c * clientNum) + 0x1b2f0
    gclient_t *client = ent->client;         // ent+0x204

    trap_GetUsercmd( clientNum, &client->pers.cmd );   // trap -0x25, confirmed; pers.cmd at client+0x1d8
    client->lastCmdTime = level.time;                   // client+0x2c4, level.time from 0x12a310

    if ( !(ent->r.svFlags & SVF_BOT /*0x8*/) && !(*(int*)0x1986c) ) {
        ClientThink_real( clientNum );   // 0x1a866 — see note below
    }
}
```

This is **structurally an exact match** for vanilla Q3's `ClientThink`:

```c
void ClientThink( int clientNum ) {
	gentity_t *ent = g_entities + clientNum;
	trap_GetUsercmd( clientNum, &ent->client->pers.cmd );
	ent->client->lastCmdTime = level.time;
	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		ClientThink_real( ent );
	}
}
```

Same trap call, same field, same guard shape, same conditional call. `0x1986c` is
presumed to be the `g_synchronousClients` cvar cache by this structural match — not
independently confirmed via a string literal this pass, flagged rather than asserted
as certain.

## The real headline: `0x1a866` ("Pmove outer") is DeFRaG's `ClientThink_real`

Since `ClientThink` (`0x1ac8f`) calls `0x1a866` in exactly the position vanilla calls
`ClientThink_real(ent)`, **`0x1a866` — documented at length across this project as
"Pmove outer" — is actually the full DeFRaG equivalent of `ClientThink_real`**, not
merely a `Pmove()` wrapper. This reframes (but does not invalidate) everything
already documented about it in [Pmove_outer.md](Pmove_outer.md): every section
described there (the `pers.connected` gate, `df_promode` flag update, `pmove_msec`
enforcement, the two inlined `ClientRespawn` call sites, race-mode handling, and the
eventual call down into `func_0x1a394` → the `Pmove()` subdivision loop →
`PmoveSingle`) is DeFRaG's `ClientThink_real` body — DeFRaG has simply restructured
vanilla's single `ClientThink_real` into a shape where the *actual* `bg_pmove.c`
`Pmove()` call is buried several layers deeper, with far more DeFRaG-specific
per-frame logic (respawn timers, race mode, promode flag refresh) folded directly in
rather than kept as separate calls the way vanilla's `ClientThink_real` keeps them.

## `0x1d27b` is `ClientSpawn`, not `ClientThink_real`

`ClientThink_real.md`'s existing call-chain diagram (section §10) already showed
`0x1d27b` ending with `0x1d691: call 0x1ac8f(clientNum)` — i.e. `0x1d27b` finishes by
calling `0x1ac8f` itself, which this doc identifies as the genuine `ClientThink`.
So `0x1d27b` doesn't call `Pmove`/`0x1a866` directly; it calls the full per-command
`ClientThink` path (re-fetch a usercmd via `trap_GetUsercmd`, then run
`ClientThink_real`/`0x1a866`) right after finishing its own spawn setup — consistent
with "spawn the player, then immediately run one full normal per-command think cycle
so their state isn't stale for a frame."

**Conclusion: `0x1d27b` = `ClientSpawn`** (matching vanilla's `ClientSpawn`'s role:
initialize a (re)spawning client's entity/client state), extended by DeFRaG to
immediately invoke a full `ClientThink` cycle (fetch cmd, run `ClientThink_real`)
right after spawn setup, rather than waiting for the next network command. This
resolves the identity question left open in
[ClientRespawn.md](ClientRespawn.md#open-question-what-is-0x1d27b).

---

## Summary of the corrected call graph

```
Engine (vmMain GAME_CLIENT_THINK) ──> ClientThink (0x1ac8f)
                                          │
                                          ├─ trap_GetUsercmd
                                          └─ if (!bot && !g_synchronousClients)
                                                  └─> ClientThink_real (0x1a866, "Pmove outer")
                                                          ├─ pers.connected gate
                                                          ├─ df_promode flag refresh (func_0x2575)
                                                          ├─ pmove_msec enforcement
                                                          ├─ ClientRespawn (0x1cb0b) [inlined, health<=0 gated]
                                                          ├─ race-mode handling
                                                          └─> Pmove() subdivision loop (0x1a394 → 0x8653 → PmoveSingle 0x8331)

ClientBegin (0x1d18b) ──┐
                        ├─> ClientSpawn (0x1d27b)
ClientRespawn (0x1cb0b) ┘        ├─ entity/client struct init, memset+restore, handicap, PMF_RESPAWNED
                                  ├─ race-mode freeze / OBS setup
                                  └─> ClientThink (0x1ac8f)   [immediate first think cycle post-spawn]
```

---

## oDFs comparison

oDFs already has this exact architecture — `ClientThink(clientNum)` calls
`trap_GetUsercmd` then conditionally `ClientThink_real(ent)`
([g_active.c:1125-1138](../../code/game/g_active.c#L1125)), and `ClientSpawn`
(`g_client.c`) does entity setup separately from the per-command path — no
oDFs code changes are needed as a result of this finding. Its value is purely in
correcting the disassembly documentation's identification of DeFRaG's functions
to match, so future passes don't re-open this question.
