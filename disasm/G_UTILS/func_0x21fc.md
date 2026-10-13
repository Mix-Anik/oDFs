# `func_0x21fc` — DeFRaG game-mode init (called once from `0x2bb`, `G_InitGame` region)

**Address:** `0x21fc` (`enter 0x90`), no args, sole caller `0x2bb`. Decoded from bytecode `0x21fc`-`0x2377`
(plus partial reads further on); string pool and trap numbers per `disasm/README.md`.

## Order of work
1. `mapname` → lowercased with `func_0xaadf` (also used on the map name in `0x406d`/`0x2d02`). If `strcmp(map, "runbeamer")==0`
   (`0x5c8f` strcmp-shaped, string `0x406a`): spawns an extra pickup entity — `BG_FindItemForWeapon(8)` (`0x4b6a`,
   confirmed elsewhere) and `G_Spawn` (`0x283de`), origin set to float constants
   `(-664.0, -72.0, 24.0)` (`0xC4260000`, `0xC2900000`, `0x41C00000`) written to `ent+0x5c/+0x60/+0x64` and
   block-copied to `+0x18`, `+0x1e8`, `+0x20c`, followed by `0x214f5(ent, item)` = **`G_SpawnItem`** (identified this pass: reads spawn keys `"random"`/`"wait"`, `RegisterItem` `0x2147c`, sets `ent->item` (`+0x324`), `nextthink = level.time + 200`; also called from `G_CallSpawn 0x2493f` and `0x1db30`). Weapon id 8 = `WP_PLASMAGUN` in the standard enum.
   Intent = a per-map fix adding a weapon pickup (weapon id 8) — the item identity is **not** looked up by name here.
2. `df_gametype` (`0x405e`) → `0x1317bc` (the DeFRaG mode, 1..3 valid). If `<=0`, `>=8` or `==4`: invalid, so
   `0x1317c0 = cvar("dedicated")` (`0x4054`) and auto-pick the mode from map contents: `0x1318d0 != 0` (a start timer
   entity exists, see `G_TRIGGER/race_timer_system.md`) → 1, else `0x1318d4 != 0` (**a team item exists on the map**: `G_SpawnItem` sets `*(0x1318d4) = 1` when `item->giType (+0x24) == 8 = IT_TEAM`, e.g. CTF flags) → 3, else 2.
   Then if `0x1317bc >= 5`: `0x1317bc -= 4; 0x1317c0 = 1`.
3. `trap_Cvar_Set("g_gametype", va("%i", mode==3 ? 4 : 0))` and `*(int*)0x1b1ec = (mode==3 ? 4 : 0)` — `0x1b1ec` is
   the game's `g_gametype.integer` (used by `ClientBegin`'s "entered the game" gate), i.e. mode 3 runs as GT_CTF(4), others GT_FFA(0).
4. `0x1317c0 |= (cvar("dedicated") != 0)` (set to 1 if the cvar is nonzero).
5. `trap_Cvar_Set("defrag_gametype", va("%i", 0x1317c0 ? mode + 4 : mode))` (`0x4039`).
6. Calls, in order: `func_0x1e60` (= `G_InitRaceMode`, `G_TRIGGER/func_0x1e60.md`); `func_0x21b3`; `func_0x20e0`.
   - **`func_0x20e0`** = the level-init entity-exclusion sweep, fully documented in
     [../G_SPAWN/entity_mode_exclusion.md](../G_SPAWN/entity_mode_exclusion.md) (Part 3): it frees entities whose `ent+0x218` mode-exclusion
     bits (`0x10000..0x400000`) match the current mode (`0x1317c4`/`0x1317c0`/`0x1317bc`) via `G_FreeEntity` (`0x284b7`).
     Its calling place here (right after `G_InitRaceMode`) matches that doc's "called once at level init".
   - **`func_0x21b3`** (decoded this pass): `for (i = 0; i < 64; i++) { ent = &g_entities[i]; if (ent->inuse(+0x208) && ent->client(+0x204)) {
     trap_GetUserinfo(client->ps.clientNum(+0x8c), buf, 0x400); /* syscall -0x15 = id 20 = G_GET_USERINFO */ func_0x24765(client, buf); } }`
     i.e. re-runs `func_0x24765` for every connected client after the mode is decided.
   - **`func_0x24765(client, userinfo)`** (callers: `0x21b3`, `ClientConnect 0x1d05a`) is vanilla **`G_InitSessionData`'s team-determination**:
     writes `client+0x284` (`sess.sessionTeam`). If `g_gametype(0x1b1ec) >= 3` (GT_TEAM+): if `g_teamAutoJoin (0x1920c)` is set →
     `sessionTeam = PickTeam(-1)` (`0x1cbbd`, confirmed `PickTeam` in `G_ACTIVE/PickTeam.md`) + `BroadcastTeamChange(client,-1)` (`0x1e09d`),
     else `sessionTeam = 3 (TEAM_SPECTATOR)`; else `Info_ValueForKey(userinfo,"team")` (`0xac7d`, key str `0x922e`): first char `'s'` → spectator;
     else by gametype: 0/2 → spectator if `g_maxGameClients (0x1adac) > 0 && numNonSpectatorClients (0x12a33c) >= it`, else TEAM_FREE(0);
     1 (TOURNAMENT) → TEAM_FREE if `numNonSpectatorClients < 2`, else spectator. Structure and constants
     match vanilla; the remainder of the function (after `0x247dd`) was not read. Identification is by structure + the `PickTeam`/
     `BroadcastTeamChange` callees, not a hash or embedded string.
7. `defrag_flags` cvar (`0x402c`) = bitmask: `|2` if `0x131810 != 0` (checkpoint ready flag), `|0x10` if `0x131830 != 0` (= cached `df_ndm_nopad`, `G_TRIGGER/func_0x1e60.md`),
   `|0x20` if `0x131834 != 0` (= cached `df_ndm_noteleport`, same doc), accumulated in `0x1317cc`.
8. `defrag_obs` cvar (`0x4021`) = `df_ob_KillOBs (0x4013) != 0 ? 1 : 0`.
9. **Tail (decoded this pass, `0x237c`-`0x23f1`)** — `v = trap_Cvar_VariableIntegerValue("sv_fps")` (`0x400c`), then:
   - if `0x1317c0 != 0` (dedicated/variant): if `cvar("defrag_svfpsInitialized") == 0` → set it `"1"`, `sv_fps = 125` (`va("%i",0x7d)`), `g_synchronousClients = "0"` — a one-time defaults push.
   - else (flag 0): if `v != 0 && (v < 60 || v > 140)` → `sv_fps = 125`; then **always** `timelimit = "0"`, and
     `trap_Cvar_Register(NULL, "defrag_svfps", "0", flags 8)` and `("defrag_clfps","0", flags 8)` (`-4` = id 3 `G_CVAR_REGISTER`; flags value 8 = `CVAR_SYSTEMINFO` (earlier "ROM" label was wrong; see CGAME/cheat_report.md)).
   - finally `func_0x2d02()` (writes `system/checksums/<map>.crc`, see `G_CLIENT/auth_handshake.md`).
   Reading: DeFRaG clamps `sv_fps` to 60..140 (else forces 125) and zeroes `timelimit` on the non-dedicated path, and publishes read-only
   `defrag_svfps`/`defrag_clfps` cvars (zero here; written elsewhere — writers not searched).

## What `0x1317c0` is
A flag that is 1 when the `dedicated` cvar is nonzero OR `df_gametype >= 5`; reported to clients as `defrag_gametype = mode + 4`.
Consumers found so far: the auth request is sent only when it is **0** (`G_CLIENT/auth_handshake.md`), and the race-time encoding
folds in player state only when it is **0** (`G_TRIGGER/func_0x38ba.md`); `func_0x38ba` sends the plain time when it is non-zero.
Reading it as "dedicated server" is based on the cvar name; the `>= 5` path shows it is also a mode-variant flag, so the name
`isDedicatedOrVariant` is descriptive only.

## oDFs
oDFs registers `defrag_flags`/`defrag_gametype`/`defrag_mode` as static serverinfo cvars (`g_main.c:203-205`) and has no mode
auto-detection; **not ported** (the `sv_fps` clamp / `timelimit=0` behaviour is decoded but deliberately not forced onto oDFs servers).

## oDFs port status (mode auto-detect)
Steps 2 and 5 are **ported**: `G_ResolveDfMode()` (g_spawn.c, run at the start of `G_ApplyModeExclusion` after entity spawn) resolves `df_gametype`
(<=0, >=8 or ==4 -> auto: `g_startTimerReady` -> 1, `g_hasTeamItem` (set in `G_SpawnItem` for `IT_TEAM`) -> 3, else 2; >=5 -> mode-4 + variant),
sets `g_dfMode`/`g_dfVariant` and publishes `defrag_gametype`. `df_gametype` default changed "1" -> "0" (auto, as DeFRaG).
**Behaviour change:** maps without a start timer now run as mode 2 (tricks) for the `nottm`/`notdf` exclusion sweep; revert = set `df_gametype 1`
(or default "1" in g_main.c). Step 3 (forcing `g_gametype` 4 for mode 3) deliberately NOT ported. Syntax-checked, untested.
