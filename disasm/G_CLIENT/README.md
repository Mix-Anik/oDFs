# G_CLIENT — Defrag QVM disassembly analysis

Analysis of `g_client.c`-equivalent functions from the Defrag qagame.qvm —
client connect/spawn/disconnect lifecycle and userinfo handling.

**Update (later pass): the five per-function docs now actually exist** —
[`ClientUserinfoChanged.md`](ClientUserinfoChanged.md),
[`ClientConnect.md`](ClientConnect.md), [`ClientBegin.md`](ClientBegin.md),
[`ClientSpawn.md`](ClientSpawn.md) (a pointer to the full decode, which
lives in `../G_ACTIVE/ClientThink_real.md` under this address's corrected
identity), and [`ClientDisconnect.md`](ClientDisconnect.md) (fully
re-decoded fresh this pass, independently of this README, then
cross-referenced against it). `ClientConnect.md` and `ClientBegin.md` were
independently re-verified/re-decoded against the raw bytecode this pass;
`ClientUserinfoChanged.md` carries over an earlier pass's own
re-verification (see that doc). Findings below not covered by one of those
five docs have **not** been independently re-checked and should be treated
with appropriate caution. Two branch-polarity errors from the original
pass (headline findings 2 and 4) were already corrected in place below in
an earlier revision of this README.

For general disassembly methodology, struct layouts, and global addresses see
[../README.md](../README.md). `ClientBegin`/`ClientSpawn`/`ClientRespawn`/
`CopyToBodyQue` were already located by a prior pass — see
[../G_ACTIVE/ClientRespawn.md](../G_ACTIVE/ClientRespawn.md) and
[../G_ACTIVE/README.md](../G_ACTIVE/README.md). `ClientConnect` and
`ClientDisconnect` were newly located this pass via direct call-target
tracing from a hash-confirmed anchor (`G_FilterPacket`, hash-identical to
vanilla) rather than the position-based hash/order walk — DeFRaG's file
layout in this region does not preserve vanilla's function order closely
enough for a pure position walk (see headline finding 5 below).

---

## Functions analyzed

| Function | DeFRaG address | vanilla address | Hash match? | Status |
|----------|-----------------|------------------|-------------|--------|
| `ClientUserinfoChanged` | `0x1ccdf` | n/a (not in hmap by this name at time of check) | — | ✅ Fully decoded — **modified**, see [ClientUserinfoChanged.md](ClientUserinfoChanged.md) |
| `ClientConnect` | `0x1d05a` | `0x15cbb` | ❌ (no hash match found) | ✅ Fully decoded — **modified**, see [ClientConnect.md](ClientConnect.md) |
| `ClientBegin` | `0x1d18b` | `0x15de5` | ❌ (previously confirmed via debug strings) | ✅ Spot-checked in full this pass — **modified** (small additions found), see [ClientBegin.md](ClientBegin.md) |
| `ClientSpawn` | `0x1d27b` | n/a | — | ✅ Fully decoded — **modified**, see [ClientSpawn.md](ClientSpawn.md) |
| `func_0x274b` (health/armor/ammo spawn-stat setter, called from `ClientSpawn`) | `0x274b` | n/a (not a standalone vanilla function) | — | ✅ Fully decoded — DeFRaG-specific "debug/noclip mode" 200/200/200 override, see [ClientSpawn.md](ClientSpawn.md#func_0x274b) |
| `ClientDisconnect` | `0x1d6dc` | `0x16242` | ❌ (no hash match found) | ✅ Fully decoded — **modified**, see [ClientDisconnect.md](ClientDisconnect.md) |
| `G_FilterPacket` | `0x24ea5` | `0x1d1da` | ✅ hash-identical (`7c023160`) | ✅ Confirmed unmodified (used only as a location anchor, not independently re-decoded) |

## Headline findings

1. **No per-client userinfo `"health"` override exists anywhere in
   `ClientUserinfoChanged` or `ClientSpawn`.** The task-handoff note citing
   `0x1dc0e`/`0x1dc18` as a "health override inside ClientSpawn" was a
   mislocation from an earlier pass — that address range is actually inside
   **`Cmd_Give_f`** (the `give` cheat-command handler, `g_cmds.c` territory,
   function at `0x1db30`, confirmed via its `"Usage: give item <count>"`
   string and `trap_Argc`/`ConcatArgs`-shaped calls), not `g_client.c` at
   all. See [ClientSpawn.md](ClientSpawn.md#the-health-override-red-herring)
   for the full trace that resolved this.
2. **The real `ent->health = maxHealth + 25` assignment does not live in
   `ClientSpawn`'s own body.** It's inside a separate helper, `func_0x274b`,
   called once from `ClientSpawn` — already fully decoded in an earlier,
   independently-careful pass this session, see
   [`../G_TRIGGER/func_0x274b.md`](../G_TRIGGER/func_0x274b.md).
   **Correction**: the original summary here had the polarity backwards.
   The confirmed behavior is `if (*(int*)0x131820 != 0) health =
   handicap+25, armor=0; else health=200, armor=200` — i.e. the flat
   **200/200 override is the DEFAULT (flag clear)** state, and the
   handicap-based calculation is what happens when the flag is **set**, not
   the other way around. **`0x131820`'s identity is now resolved**: a later
   pass (investigating `trigger_push_touch`, see
   [`../G_TRIGGER/trigger_push_touch.md`](../G_TRIGGER/trigger_push_touch.md))
   traced `func_0x1e60`/`G_InitRaceMode` and found `0x131820` is the cached
   integer value of cvar **`df_ndm_damage`** (default `"0"`), re-read once
   per map init via `trap_Cvar_VariableIntegerValue`. **Implemented**: see
   [`../G_TRIGGER/func_0x274b.md`](../G_TRIGGER/func_0x274b.md)'s
   "Implemented" section — `ClientSpawn` now gates health/armor on
   `df_ndm_damage.integer`, and also now sets the starting weapon bitmask
   from the sibling cvar `df_ndm_weaponsSet` (same pass, same resolution).
3. **Two genuinely new userinfo keys were found in `ClientUserinfoChanged`**:
   `"df_cl_respawnMsec"` (per-client configurable respawn delay) and
   `"df_name"` (a separate "demo name" distinct from the netname).
   **Re-verified independently** (this doc's individual detail files were
   never actually written by the originating pass — only this README
   existed — so these two were re-confirmed directly against the bytecode
   before porting, at `0x1cd50`-`0x1cd92` and `0x1ce0d`-`0x1ce34`):
   - `df_cl_respawnMsec`: parsed via `Info_ValueForKey`+`atoi`, clamped to
     `[50, 1700]` (confirmed literals `0x32`/`0x6a4`), **default `250`
     (`0xfa`) when the key is empty/unset** — not vanilla's hardcoded 1700,
     and not `oDFs`'s own prior hardcoded `500` in `player_die`
     (`g_combat.c`) either. **Implemented**: added `gclient_t.respawnMsec`,
     parsed in `ClientUserinfoChanged`, used in `player_die` in place of the
     literal `500`.
   - `df_name`: parsed, sanitized through the same cleaning helper used for
     `name` (`func_0x12e3`), and stored in a confirmed **20-byte** field
     (`client+0x268`, copy length `0x14` confirmed). **The claimed `\dfn\`
     configstring broadcast could not be confirmed** — no such token string
     was found anywhere in the QVM, and no code consuming `client+0x268`
     afterward (e.g. building an outgoing configstring) was located in this
     pass. **Implemented storage only** (`gclient_t.demoName[21]`,
     parsed/cleaned in `ClientUserinfoChanged`), with no broadcast — the
     original README's broadcast claim is corrected here as unconfirmed,
     not ported.
4. **`ClientConnect` gained a listen-server lockout check** —
   **re-decoded in full this pass** (the referenced `ClientConnect.md` was
   never actually written; the mechanism required correcting the original
   summary's framing, see below). Confirmed via `func_0x3db9`
   (`0x3db9`-`0x3e09`), called right after the ban check, string literal
   `"Remote clients are not allowed in this game mode."` at `0x7e6b`:
   ```c
   qboolean CanConnect( int clientNum ) {
       if ( Cvar_VariableIntegerValue("dedicated") != 0 )
           return qtrue;   // dedicated servers: unaffected
       // (df_gametype remapping of raw values 5/6/7 -> 1/2/3 also forces
       // this same bypass — exact semantic meaning of those values not
       // identified, not ported)
       ip = Info_ValueForKey(userinfo, "ip");
       if ( ip[0] && Q_stricmp(ip, "localhost") != 0 )
           return qfalse;  // genuinely remote IP on a non-dedicated (listen) server -> reject
       for ( i = 0; i < sv_maxclients; i++ )
           if ( g_clients[i].pers.connected != CON_DISCONNECTED )
               return qfalse;  // someone (even local) is already connected -> reject
       return qtrue;  // server is completely empty -> allow (the very first/host connection)
   }
   ```
   Net effect: on a non-dedicated listen server, only the very first
   connection is ever permitted, and only if local — every connection
   after that (local or remote) is rejected. Dedicated servers are
   unaffected. **Implemented** in `ClientConnect` (`g_client.c`), using
   oDFs's existing `g_dedicated` cvar; the `df_gametype` remapping nuance
   was not ported (values 5/6/7's meaning unconfirmed).
5. **`ClientDisconnect` does *not* reset any `g_raceTimers[MAX_CLIENTS]`-style
   per-client race-timing state.** The only race/speedrun-adjacent cleanup
   found is resetting a *global* (not per-client) "max speed achieved this
   session" record if the disconnecting client happens to be its current
   holder. This directly answers (in the negative) the task's original
   question about race-timer cleanup on disconnect. See
   [ClientDisconnect.md](ClientDisconnect.md).
6. **Corrects a stale entry in `G_ACTIVE/README.md`**: `0x1bda5` was
   previously described as "a tiny 16-entry table-clear loop with no
   `EntitiesInBox`/trigger-touch content at all" (used to rule out a
   `G_TouchTriggers` identity). This pass found its actual identity:
   **`G_RemoveQueuedBotBegin`**, called from `ClientDisconnect` exactly where
   vanilla calls it. The "16-entry queue clear" shape was correct — it's a
   small pending-bot-begin queue, not a trigger table.
7. **Corrects a stale entry in `G_ACTIVE/README.md`**: `0x13184c` was
   previously described as "Max speed reset flag (1 byte)". `ClientDisconnect`
   shows it is actually the destination of a 20-byte `Q_strncpyz` copy (a
   name-display buffer for the max-speed record holder), not a 1-byte flag.
8. Re-confirmed directly (`0x1d627`-`0x1d679`): global `0x1317bc` gates a
   **"keep weapon across respawn"** feature in `ClientSpawn` — when
   `*(0x1317bc) == 1` and the player still owns the weapon cached in a
   persistent per-client byte (`client+0x1ec`, byte-tested against the
   `STAT_WEAPONS` bitmask), spawn with that weapon instead of running the
   normal highest-owned-weapon auto-select loop (which starts at `0x1d651`
   on the `else`/fallthrough path). **Not ported**: what `0x1317bc` actually
   represents (a gametype value? a `df_mode`-style setting?) was not
   identified in this or the prior pass — porting a gate without knowing
   its real trigger condition risks enabling DeFRaG-specific behavior for
   the wrong oDFs gametypes/settings, so this was deliberately left
   un-implemented pending that identification.

## Not attempted this pass

- `func_0x2830` — **now fully decoded**, see [`func_0x2830.md`](func_0x2830.md).
  Loops over `g_entities[64..1023]` (every non-client slot — an earlier
  pass mistakenly called this a separate "shadow table" due to an
  arithmetic slip, now corrected) with a three-way type dispatch that
  frees owned entities (`G_FreeEntity`) and clears classname-matched
  entries' bookkeeping fields. Also connects to and confirms the
  `+0x54`/`+0x58` per-client bitmask fields independently found in
  [`../G_MISSILE/func_0x20998.md`](../G_MISSILE/func_0x20998.md) earlier
  this session. Not implemented (the `type` values and several field
  offsets are still unconfirmed).
- The restructured spawn-point-selection dispatch in `ClientSpawn` —
  **fully resolved, whole call graph decoded**: `0x44bc` =
  `O3J_SelectSpawnPoint_UseClosest` (confirmed via an embedded debug
  string), see
  [`O3J_SelectSpawnPoint_UseClosest.md`](O3J_SelectSpawnPoint_UseClosest.md).
  Its fallback (`func_0x270ee` → `func_0x26fbf`, a CTF spawn-point
  picker) and the shared "last resort" helper both paths converge on
  (`func_0x1c6be`, a DeFRaG-extended `SelectRandomFurthestSpawnPoint`,
  confirmed via vanilla's exact error string) are now also decoded — see
  [`func_0x270ee.md`](func_0x270ee.md),
  [`func_0x26fbf.md`](func_0x26fbf.md),
  [`func_0x1c6be.md`](func_0x1c6be.md). Confirmed reusing this project's
  own `G_Find`/`func_0x1c56f`/`rand()`. `client+0x2a4`'s identity (the
  reference-point field the whole dispatch hinges on) and the
  DeFRaG-added `+0x2f8` entity tag field are the only pieces still not
  decoded.
- `func_0x274b`'s two unidentified sub-calls (`func_0x3597`, `func_0x3933`)
  and its ammo-table lookup helper `func_0x4b6a`.
- `func_0x273f` — **resolved**: race-timer record clear (not ClientEvents),
  see [func_0x273f.md](func_0x273f.md).
- `ClientBegin`'s new `func_0x26e8` call and the team-change stat-reset
  block (`client+0x288`/`0x2a4`/`0x2a8`/`0x2ac`) — **now fully decoded**,
  see [`ClientBegin.md`](ClientBegin.md). The gating condition (skip unless
  team actually changed and wasn't from spectator) is now confirmed; the
  individual fields' identities and `func_0x26e8`/`func_0x3a47`'s exact
  purpose are still not identified.

- Auth handshake (client authentication, `auth_request`/`auth_reply`): see [auth_handshake.md](auth_handshake.md).
- [Cmd_VoiceTaunt.md](Cmd_VoiceTaunt.md) — `0x1ea7b` vanilla vtaunt
- [session_data.md](session_data.md) — DeFRaG session cvar format (respawnpoint + ignore mask persisted); ported
- [Cmd_Kill.md](Cmd_Kill.md) — spectator kill = stop follow / re-begin (ported)
