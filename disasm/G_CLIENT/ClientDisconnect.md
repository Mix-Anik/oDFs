# ClientDisconnect — DeFRaG address `0x1d6dc`

**Address:** `0x1d6dc`
**Vanilla address:** `0x16242`
**Hash match:** ❌ (no hash match found — genuinely modified)
**Stack frame:** `enter 0x2c`
**Status:** Fully decoded — modified (two DeFRaG-specific cleanup additions)

This address has a tangled history in this project: it was independently
mislabeled in an earlier pass as "`G_TouchTriggers` + `ClientEvents`
wrapper" (see [`../G_ACTIVE/ClientThink_real.md`](../G_ACTIVE/ClientThink_real.md)'s
correction note), then decoded fresh (without realizing the identity) while
chasing the `G_TouchTriggers` search, then finally connected to *this*
module's own, earlier, correct identification once both passes were
cross-referenced. The decode below merges both passes' findings.

---

## Full decode

```c
void ClientDisconnect( int clientNum ) {
    gentity_t *ent = &g_entities[clientNum];   // base 0x1b2f0, stride 0x43c

    G_RemoveQueuedBotBegin( clientNum );   // 0x1bda5 — confirmed this pass: a 16-entry
                                             // pending-bot-begin queue clear, matching
                                             // vanilla's own call at this exact position

    if ( ent->client != NULL /* +0x204 */ )
        ClearRaceTimer( ent );               // 0x273f — NOT ClientEvents (vanilla's
                                               // ClientDisconnect never called it); zeroes this
                                               // client's 32-byte raceTimer_t, see func_0x273f.md

    // --- DeFRaG addition #1: clear this clientNum out of every other
    // client's per-client bitmask pair ---
    for ( i = 0; i < MAX_CLIENTS; i++ ) {       // loop bound not independently re-derived
        gclient_t *other = &g_clients[i];       // base 0x12a2f0, stride 0x37c

        if ( clientNum >= 0 && clientNum < 0x20 ) {       // bits 0-31
            if ( (*(int*)((char*)other + 0x2b4) >> clientNum) & 1 )
                *(int*)((char*)other + 0x2b4) ^= (1 << clientNum);
        }
        if ( clientNum >= 0x20 && clientNum < 0x40 ) {    // bits 32-63
            if ( (*(int*)((char*)other + 0x2b8) >> (clientNum - 0x20)) & 1 )
                *(int*)((char*)other + 0x2b8) = 0;   // not independently re-verified: this pass's
                                                        // earlier read of this exact block (same
                                                        // offsets, same loop, during the
                                                        // G_TouchTriggers search) is condensed here
                                                        // from that read, not re-derived a second time
    }

    // --- vanilla-shaped: G_LogPrintf / possibly a "left the game" broadcast ---
    trap_SetConfigstring( CS_PLAYERS + clientNum, "" );   // CORRECTED: -0x13 = id 18 = G_SET_CONFIGSTRING (not G_LogPrintf); args include the empty-string literal 0x7db4 — this is vanilla's own call at the end of ClientDisconnect
    CalculateRanks();            // func_0x6cc — CONFIRMED (body decoded), see func_0x6cc.md

    // --- DeFRaG addition #2: invalidate the global max-speed record if
    // this client was its holder ---
    if ( ent->flags /* +0x1a8 */ & 0x8 )
        func_0x15af5( clientNum, 0 );    // unidentified, 2 args

    if ( clientNum == *(int*)0x131848 ) {   // 0x131848 = max-speed record holder's clientNum
                                              // (cross-confirmed against
                                              // ../G_ACTIVE/Pmove_outer.md's own independently
                                              // confirmed 0x131848 = "who achieved it" write)
        *(int*)0x131848 = -1;                 // invalidate: no current holder
        Q_strncpyz( (char*)0x13184c,           // 20-byte name-display buffer, confirmed this pass
                    (char*)((char*)ent->client + 0x200),  // client+0x200 — NOT independently
                                                            // confirmed as "netname" this pass
                    0x14 );
    }
}
```

---

## What's genuinely new here (DeFRaG additions vs. vanilla `ClientDisconnect`)

1. **Per-client bitmask cleanup** (`other+0x2b4`/`+0x2b8`, looped over
   every client) — clears any bit corresponding to the disconnecting
   `clientNum` out of every other client's bitmask pair. Plausibly related
   to some per-client "who has interacted with me" tracking (structurally
   similar in *spirit* to `G_UseTargets`'s own new per-frame dedup bitmask
   fields documented elsewhere in this project, though at different
   offsets and on a different base struct — not confirmed to be the same
   mechanism). Not independently identified.
2. **Global max-speed-record invalidation** (`0x131848`/`0x13184c`) —
   confirmed, matches `G_CLIENT/README.md`'s finding 5 exactly: if the
   disconnecting client was the current "highest speed observed" record
   holder (the same global `Pmove_outer.md` documents being written to
   during normal gameplay), the holder slot is invalidated and a 20-byte
   name buffer is overwritten via `Q_strncpyz` from `client+0x200` — this
   project has not independently confirmed `client+0x200`'s exact identity
   (plausibly `pers.netname` by position, but not verified this pass).
   **`ClientDisconnect` does NOT reset any per-client race-timer state** —
   this is the only race/speedrun-adjacent cleanup found, confirming the
   README's original negative finding.
3. `func_0x15af5` (2 args), gated on an entity flag bit (`+0x1a8 & 0x8`) —
   not identified.

Everything else (`G_RemoveQueuedBotBegin`, `ClientEvents`, `G_LogPrintf`)
is vanilla-shaped or already-confirmed elsewhere.

---

## oDFs implementation status

**Not implemented.** Neither addition has an identified gameplay-visible
purpose precise enough to port confidently (the bitmask cleanup's exact
meaning is unknown; the max-speed invalidation would need oDFs to already
track a global max-speed record the way DeFRaG does, which it currently
does not). Flagged for a future pass.
