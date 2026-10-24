# cgame `0x59b4` — run-validity violation reporter, and the `stats[12]` 0x20/0x40 consumers

Closes the open question from `G_UTILS/func_0x3ccd.md`: **cgame DOES read `ps.stats[12]` bits `0x20` and `0x40`**.

## Consumer (cgame `0x6237`, called from `0x1e4c`; bit tests at `0x627f`/`0x628f`)
`snap+0x114` (= `ps.stats[12]`, snapshot_t ps at +0x2c) is tested directly (not as a change-XOR, unlike bits 1/4/8/0x10):

| qagame source | stats[12] bit | cgame call | message (category) |
|---|---|---|---|
| `0x1320f8` clock drift outside (0.8, 1.2) | `0x20` | `0x59b4(10, 0)` | "timescaling reported" (Server) |
| `0x1320f4` frame length outside 7..16 ms | `0x40` | `0x59b4(11, 0)` | "illegal server frame-rate reported" (Server) |

The same function (same spot, `0x6263`) also calls `0x59b4(9, 0)` with `1000/(a-b)` from two time fields when that delta is < 7 ("high server frame-rate: %i", Settings).

## `0x59b4(code, arg1, arg2)` = violation reporter
Switch over 18 codes (jump table at data `0x139c`), each = (message, category):
0 "invalid player speed %i (expected: %i)" Physics; 1 "invalid player gravity %i (expected: %i)" Physics; 2 "checksum failed" Timer (the `0x5fd7` time-decode mismatch); 3 "abnormal stepping" Timer; 4 "incomplete run" Timer; 5 "checksum failed" Bsp crc; 6 "missing local record" Bsp crc; 7 "invalid version report %i" Defrag; 8 "beta version %s" Defrag; 9 "high server frame-rate: %i" Settings; 10 "timescaling reported" Server; 11 "illegal server frame-rate reported" Server; 12 "%s is missing" Cvar; 13 "%s %i (invalid)" Cvar; 14 "%s %.2f (invalid)" Cvar; 15 "latched cvar %s changed from %f to %f" Cvar; 16 "physics do not match reported physics" Cvar; 17 "illegal client roll detected" Client.
Output line `"%s%s: %s%s"` = `^2`+category `: ` `^3`+message, built in a 128-byte buffer. Global `0x11d73c` is a bitmask of codes already reported; a code seen first time takes the "print" path (`"%s\n"` via `0x23c`, presumably Com_Printf — callee not verified), repeats go into a log array (`0x11d7a0`, 128-byte entries, count `0x11e7a0`, cap 0x20) instead when arg1 != 0. Exact first/repeat split of the `local130/local134` flags was read quickly, not fully verified.
Callers: 25 call sites (`const 0x59b4`). Not enumerated here.

## Implication for oDFs
The DeFRaG client (the one players actually run, per memory note on deployment) flags a run as invalid ("Server: timescaling reported" etc.) **only if qagame sets those bits**. oDFs never sets them, so no false flags; porting `0x3ccd` would ADD the flags (a validity feature: marks servers whose game clock is out of step). Only worth it if oDFs wants the client to display these warnings. Not ported.

---

## Cvar validators (second pass): cgame `0x3a70` / `0x3b5e` -> `0x5b89` / `0x5dd5`
Per-gamestate parse: `0x3d47` calls `0x3b5e(info A)` then `0x3d4d` calls `0x3a70(info B)`. `0x3b5e` first reads `defrag_gametype` (a SERVERINFO cvar) and `0x3a70` first reads `sv_cheats` (SYSTEMINFO in Q3), so A = serverinfo, B = systeminfo (inferred from those keys; the configstring indices at the call sites were not checked). Each calls a validator; helpers `0x5b1b` (int) / `0x5b52` (float) fetch `Info_ValueForKey` and report **code 12 "<name> is missing"** (`0x59b4(12,1,name)`) when the key is absent/empty.

**`0x5b89` (systeminfo)** — value-change driven (cached at `0x11d740..0x11d778`); on first sight/change it prints `^2Cvar: ^3name value` when OK, else reports code 13 (int) / 14 (float):
| cvar | requirement | else |
|---|---|---|
| `sv_cheats` | 0 | code 13 |
| `timescale` | 1.0 | code 14 |
| `g_synchronousClients`, `pmove_fixed`, `pmove_msec` | info only | — |
| `defrag_svfps`, `defrag_clfps` (only when version >= 18100 `0x46b4` and `0x11d2ac==0`) | 60..140 | code 13 named `sv_fps` / `com_maxfps` |
| `g_speed` | 320 | code 13 |
| `g_gravity` | 800 | code 13 |
| `g_knockback` | 1000 | code 13 |
| `defrag_bspcrc` | compared to local map crc (tail not decoded) | code 5/6 family |
**`0x5dd5` (serverinfo)**: `df_promode` info only; `dmflags` must be 0 (else code 13); `sv_maxclients` (only for 18100 <= version < 19114 `0x4aaa`, `0x11d2ac==0`) must be 1 (else code 13) — version window read literally, meaning of `0x11d2ac` unknown; then prints version `%i.%02i.%02i`.

### Gate variable `0x11d2ac` and version parsing (third pass) — CORRECTS the above
cgame `0x3c33` (called from `0x3985`) fetches configstring 0 and 1 via `0xd6c(idx)`: **A (serverinfo) = CS 0, B (systeminfo) = CS 1 — now confirmed**. It resolves the server version: `defrag_vers` (int; < 1000 -> x100) -> `out+4` (= global `0x11d29c`); else legacy `defrag_version` (value 16100 `0x3ee4` is rejected via code 7 "invalid version report 16100"); else presence of a `defrag` key only sets the detected flag (`0x11d298`). It also copies `fs_game`/`mapname`/`gamename`/`defrag_servername`.
The serverinfo parser `0x3b5e..` sets: `0x11d2b8` = `defrag_flags` (atoi), `0x11d2a4` = `defrag_gametype`, and **`0x11d2ac = 1` iff version >= 18100 AND defrag_gametype < 5**.
Therefore (literal reading): the `defrag_svfps`/`defrag_clfps` (60..140) and `sv_maxclients` checks run only when `0x11d2ac == 0`, i.e. defrag_gametype >= 5 — **NOT for oDFs (`defrag_gametype` = 1)**. The `g_speed/g_gravity/g_knockback/dmflags/sv_cheats/timescale` checks run for every version >= 18100. `defrag_flags & 8` -> "beta version %i.%02i.%02i" (code 8); oDFs publishes `defrag_flags` = 2 so no beta warning. Meaning of gametype >= 5 / flags bit 4 (`0x11d2a0=1`) not determined.

### oDFs fallout — PORTED
- `g_speed`/`g_gravity`/`g_knockback` were registered with flags 0 in oDFs (DeFRaG: `SYSTEMINFO|CHEAT`, `G_UTILS/cvar_table.md`), so they never reached systeminfo -> the DeFRaG client reads an empty value ("is missing"). Now `CVAR_SYSTEMINFO` (CHEAT deliberately NOT added). One-line revert per cvar (flags back to 0). Syntax-checked, untested at runtime.
- `defrag_svfps`/`defrag_clfps` mirrors: briefly ported then REVERTED, because the checks that read them are gated off for gametype < 5 (above). (Flag 8 = SYSTEMINFO, not ROM — `func_0x21fc.md`/`func_0x23f2.md` labels fixed.)
- Other required values already hold in oDFs (`dmflags` 0, 320/800/1000).

---

## Codes 5/6 (Bsp crc) — DECODED (fourth pass)
- cgame `0x61c6` (called from `0x63f5`, no args): **returns immediately when `0x11d2ac != 0`** (i.e. unless defrag_gametype >= 5, see above). Otherwise opens `system/checksums/<mapname>.crc` (`0x11d2bc` = mapname) with `FS_FOPEN` (`-0xb`); if the file is missing/ empty or longer than 32 bytes -> `0x59b4(6,0)` "missing local record" (Bsp crc) and `0x11d794 = 0`; else reads it, closes, `atoi` -> `0x11d798` = local crc, `0x11d794 = 1`.
- cgame `0x6222(arg0)` (called from `0x5b89`, i.e. with the server's `defrag_bspcrc` value): if a local record exists (`0x11d794 != 0`) and `arg0 != 0x11d798` -> `0x59b4(5,0)` "checksum failed" (Bsp crc).
- The file's writer is qagame `func_0x2d02` (`G_CLIENT/auth_handshake.md`: writes `sv_mapChecksum` to `system/checksums/<map>.crc`; qagame forces `defrag_bspcrc = sv_mapChecksum` in `G_InitGame`, `G_UTILS/G_InitGame_forced_cvars.md`). So on a client that shares a game dir with the server (listen/local), the record always matches; elsewhere it is "missing". Verification therefore only matters for gametype >= 5 servers.
- oDFs: `defrag_gametype` 1 -> check inactive; nothing to port. (If a gametype >= 5 mode is ever emulated, qagame must publish `defrag_bspcrc` and write the `.crc` file.)

---

## Codes 0/1/4/16/17 — the per-snapshot checker (cgame `0x6237`, fifth pass) — DECODED
`0x6237(snap, arg1)` runs once per snapshot (called from `0x1e4c`; the `ps` is `snap+0x2c`). In order:
1. (`0x11d2ac==0` only) bsp-crc compare `0x6222` (above); then the timer-state events: `0x59b4(9,..)` high server frame-rate (`1000/(t_a - t_b)` where < 7 -> i.e. snapshot interval > 142 Hz; two time fields not identified), bit `0x20` -> code 10, bit `0x40` -> code 11 (above).
2. **Code 4 "incomplete run"**: if event `0x40` (stats[12] bit 4 changed) -> `0x11d790 = 1` ("start seen", set-only flag, never cleared) and the finish test is skipped; else if event `0x100` (finish) fires while `0x11d790 == 0` -> code 4. So a finish with no start ever seen by this client (joined mid-run) is flagged. Not a problem for oDFs (it raises 0x40 at start; and a same-snapshot 0x40+0x100 takes the 0x40 branch).
3. Event `1` -> clears `0x11d788` (haste-expiry memo).
4. **Code 17 "illegal client roll detected"**: `snap+0xe4` = `ps.stats[0]` (health) > 0 **and** `snap+0xcc` = `ps.viewangles[2]` (roll) != 0.0.
5. Skipped for spectators (`clientinfo.team == 3`; `ps.persistant[3]`=PERS_TEAM == 3) and dead players (health <= 0), and when `ps.gravity==0 && ps.speed==0 && |velocity|==0` (frozen ps; the 3rd test is a float call at `0x3086a` on `ps+0x20..` — reading of that one is the least certain).
6. **Code 1 "invalid player gravity %i (expected: %i)"**: `ps.gravity` (`snap+0x5c`) must be 800 (`0x320`) or equal to the last-seen value `0x11d784` (first seen value initialises to 800); otherwise `0x59b4(1,0,gravity,800)` once per change.
7. `ps.powerups[3]` (PW_HASTE timer, `snap+0x170`) is memoised in `0x11d788` when non-zero.
8. **Code 0 "invalid player speed %i (expected: %i)"**: `ps.speed` (`snap+0x60`) must be 320 (`0x140`) — always OK; else, while `snap.serverTime < haste_memo + 250` the expected value is 416 (`0x1a0`, haste x1.3) and otherwise 320; a mismatch (and != last-seen `0x11d780`) reports `0x59b4(0,0,speed,expected)`. `0x11d780` = last speed.
9. **Code 16 "physics do not match reported physics"**: `ps.pm_flags & 0x8000` (PMF_PROMODE) must equal `0x11d2b0`, which is `(serverinfo df_promode != 0)`.

### oDFs status
Everything in 4-9 is satisfied by the existing oDFs qagame: `ps.speed` = `g_speed`(320)/haste, `ps.gravity` = `g_gravity`(800), roll comes from usercmd as in Q3, and oDFs already networks `PMF_PROMODE` (0x8000) = `df_promode` in `g_active.c` (`ClientThink_real`). Requires `df_promode` in serverinfo (cvar `df_promode` present in `g_main.c`). Nothing to port; the only operator-visible effect is that changing `g_speed`/`g_gravity` makes DeFRaG clients print the violation (as in DeFRaG).

## Iter 11 (full-port loop): PORTED into oDFs cgame (`cg_defrag.c`, cvar `df_validityChecks` default 1)
Reporter `CG_DF_Violation` (first occurrence per code printed as `^2<category>: ^3<message>`, repeat log not ported) and per-snapshot checker `CG_DF_Validate` (called from `CG_DF_SnapshotTransition`): codes 10/11 (stats[12] 0x20/0x40), 4 (finish without seen start), 17 (roll), 1 (gravity), 0 (speed, haste memo 250 ms), 16 (promode flag vs `df_promode`), and the cvar validators 12/13/14 for sv_cheats 0, timescale 1, g_speed 320, g_gravity 800, g_knockback 1000 (systeminfo) and dmflags 0 (serverinfo). Not ported: 2 (time checksum; needs `0x5fd7`), 3, 5/6 (bsp crc, inactive for gametype<5), 7/8 (version), 9, 15 (latched cvar change), and DeFRaG's 'cvar OK' info prints. Frozen-ps test is the least certain decode (see item 5). Client-only, prints to the console. Syntax-checked, untested. Revert: `df_validityChecks 0`.


## CORRECTION (re-audit iter 14)
The "Gate variable 0x11d2ac" paragraph above has the polarity inverted: `0x3b5e` sets `0x11d2ac = 1` when version >= 18100 AND `defrag_gametype >= 5` (`3be6: if (gt < 5) -> 0`). The sv_fps / com_maxfps (60..140) and sv_maxclients checks, the bsp-crc compare, the 0x20 / 0x40 / code-4 block and the code-9 frame-rate test therefore run on the NON-multiplayer gametypes (gt < 5, i.e. oDFs). See PORT_STATUS "Re-audit cgame/ui iter 14".
