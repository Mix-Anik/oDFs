# ClientConnect — DeFRaG address `0x1d05a`

**Address:** `0x1d05a`
**Vanilla address:** `0x15cbb`
**Hash match:** ❌ (no hash match found — genuinely modified)
**Status:** Fully decoded — modified (listen-server lockout addition)

Extracted and spot-verified from [`README.md`](README.md)'s original
findings (that pass never wrote individual files — see the README's
reliability caveat). The core addition described below (`func_0x3db9`) was
independently re-decoded end-to-end this pass and matches exactly.

---

## The addition: a listen-server connection lockout, `func_0x3db9`

Called right after the existing ban check, guarding against a string
literal `"Remote clients are not allowed in this game mode."` (data address
`0x7e6b`). Full decode of `func_0x3db9` (`enter 0x24`, called from 7 sites
including `ClientConnect`):

```c
qboolean CanConnect( int clientNum ) {          // func_0x3db9
    if ( *(int*)0x1317c0 /* dedicated cache, confirmed elsewhere in this
                             project via ../G_SPAWN/entity_mode_exclusion.md */
         != 0 )
        return qtrue;                            // dedicated servers: unaffected

    char *ip = Info_ValueForKey( userinfo, "ip" );   // 0xac7d = Info_ValueForKey,
                                                       // confirmed elsewhere (ClientSpawn's
                                                       // handicap lookup uses the same address)
    if ( ip[0] != '\0' ) {
        if ( Q_stricmp( ip, "localhost" ) != 0 )  // 0x5c8f = Q_stricmp-shaped helper
            return qfalse;                         // non-empty, non-"localhost" IP -> reject
    }

    for ( i = 0; i < *(int*)0x12a308 /* sv_maxclients cache */; i++ ) {
        gclient_t *cl = (gclient_t*)( 0x12a2f0 + i * 0x37c );  // g_clients base+stride, confirmed
        if ( cl->pers.connected /* +0x1d4, confirmed elsewhere */ != 0 /* CON_DISCONNECTED */ )
            return qfalse;                         // someone (even local) already connected -> reject
    }
    return qtrue;                                  // server completely empty -> allow
}
```

Every field/trap/helper referenced here was already independently
confirmed elsewhere in this project before this pass (`0x1317c0`=dedicated
cache, `0xac7d`=`Info_ValueForKey`, `0x12a2f0`/`0x37c`=`g_clients`
base/stride, `+0x1d4`=`pers.connected`) — this decode is a clean,
non-speculative re-derivation, not a guess.

**Net effect**: on a non-dedicated listen server, only the very first
connection is ever permitted, and only if it's local (empty `ip` key or
exactly `"localhost"`) — every connection after that (local or remote) is
rejected. Dedicated servers are completely unaffected.

**Not independently re-confirmed this pass** (carried over from the
original README pass, not re-derived): the claim that `df_gametype`
remapping raw values `5`/`6`/`7` to `1`/`2`/`3` "also forces this same
bypass" — this nuance would live in `ClientConnect`'s own caller-side logic
around where it decides whether to call `func_0x3db9` at all, not inside
`func_0x3db9` itself (confirmed: no `df_gametype`/`0x1317bc` reference
exists anywhere in `func_0x3db9`'s body). Not re-traced this pass.

---

## oDFs implementation status

**Implemented** in `ClientConnect` (`code/game/g_client.c`), using oDFs's
existing `g_dedicated` cvar as the dedicated-server check. The `df_gametype`
remapping nuance above was **not** ported (values `5`/`6`/`7`'s meaning
still unconfirmed).
