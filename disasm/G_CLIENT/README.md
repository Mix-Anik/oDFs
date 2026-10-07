# G_CLIENT — Defrag QVM disassembly analysis

Analysis of `g_client.c`-equivalent functions from the Defrag qagame.qvm —
client connect/spawn/disconnect lifecycle and userinfo handling.

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
   called once from `ClientSpawn`. That helper implements a **server-global
   "debug/noclip mode" override**: when a global noclip flag (`0x131820`) is
   set, every spawning player gets a flat **200 health / 200 armor / 200
   ammo (all weapons)** instead of the normal handicap-based health and zero
   armor/default ammo. This is gated by server-side globals, **not** by any
   userinfo key. See [ClientSpawn.md](ClientSpawn.md#func_0x274b).
3. **Two genuinely new userinfo keys were found in `ClientUserinfoChanged`**:
   `"df_cl_respawnMsec"` (per-client configurable respawn delay, 50–1700ms,
   replacing vanilla's hardcoded 1700ms in `player_die`) and `"df_name"` (a
   separate 20-byte "demo name" distinct from the netname, broadcast to
   other clients via a new `\dfn\` configstring token). See
   [ClientUserinfoChanged.md](ClientUserinfoChanged.md).
4. **`ClientConnect` gained a "reject remote clients on an empty server"
   check** — a local/solo-practice-server protection not present in
   vanilla/oDFs. See [ClientConnect.md](ClientConnect.md).
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
8. Resolves the previously-flagged-uncertain global `0x1317bc`
   (`G_ACTIVE/README.md`: "controls weapon selection behavior in
   ClientThink_real spawn path — exact purpose uncertain") — it gates a
   **"keep weapon across respawn"** feature in `ClientSpawn`: when set to
   `1`, and the player still owns the weapon cached in a persistent
   per-client byte (`client+0x1ec`, inside `pers`, survives the respawn
   wipe), spawn with that weapon instead of running the normal
   highest-owned-weapon auto-select loop.

## Not attempted this pass

- `func_0x2830` (called at `ClientSpawn` entry, before spawn-point
  selection) — a loop over all entities checking `eType`/owner, freeing
  matching entities via `G_FreeEntity`. Plausibly cleans up leftover
  hook/missile entities owned by the respawning client, but not fully
  decoded (out of the health/DFLAG scope requested this pass).
- The restructured spawn-point-selection dispatch in `ClientSpawn` (multiple
  candidate helper functions gated on race-mode/spectator globals) — not
  individually decoded; noted structurally in
  [ClientSpawn.md](ClientSpawn.md).
- `func_0x274b`'s two unidentified sub-calls (`func_0x3597`, `func_0x3933`)
  and its ammo-table lookup helper `func_0x4b6a`.
- `func_0x273f` ("ClientEvents", called unconditionally at the top of
  `ClientDisconnect`) — already flagged purpose-unclear in
  `G_ACTIVE/README.md`; not re-examined this pass.
- `ClientBegin`'s new `func_0x26e8` call and the team-change stat-reset
  block (`client+0x2a4`/`0x2a8`/`0x2ac`) — existence confirmed, fields not
  individually identified.
