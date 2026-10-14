# DeFRaG client authentication handshake (`auth_request` / `auth_reply`)

Found while chasing `ClientBegin`'s `func_0x26e8`. Evidence is the embedded
strings (strongest form this project uses):

| String addr | Text |
|---|---|
| `0x3e49` | `"auth_request %i %i"` |
| `0x3df9` | `"auth_reply"` |
| `0x3e1c` | `"dropped: Authentication failed\n"` |
| `0x3ef2` | `"dropped: Authentication timed out\n"` |
| `0x424c` | `"sv_mapChecksum"` (cvar read for the request) |
| `0x3e04` | `"system/checksums/%s.crc"` (nearby string; relationship to the handshake NOT verified) |

## Per-client record (`ent+0x328`)
`ent+0x328` is a pointer into a 16-byte-stride table at `0x1313bc`, indexed by
`client->ps.clientNum` (`client+0x8c`). Set in `func_0x271a` (called from
`ClientConnect`, `0x1d05a`: `ent->+0x328 = 0x1313bc + (clientNum<<4)`, then a
`trap_Memset(ptr, 0, 4)` — length read from a `const 0x4`, not re-checked for other
interpretations). Fields seen so far:

| Off | Meaning (from code below) |
|---|---|
| +0x04 | random token sent in the request: `rand() % 0xf3e58 + 0x14d` |
| +0x08 | authentication **deadline** (`level.time + 1500` set when the request is sent) |
| +0x0c | **delayed-send time**: `ClientBegin` (`func_0x26e8`) sets it to `level.time + 2500` if both +0x8 and +0xc are 0 |

## Flow
1. `ClientBegin` → `func_0x26e8(ent)`: if record +0x8 == 0 and +0xc == 0, `rec->+0xc = level.time + 2500`; then `func_0x3a47(client)` (see below).
2. Per frame, `func_0x259a(ent)` (sole caller listed: `0x1adc9`, `ClientEndFrame`; first calls `func_0x38ba(ent)`, not decoded) with `rec = ent->+0x328`, if non-NULL:
   - `t = rec->+0x8; if (t != 0 && t <= level.time)` → `rec->+0x8 = 0` and `trap_DropClient(client->ps.clientNum, "dropped: Authentication timed out")` (syscall `-0x11` = `G_DROP_CLIENT`, id 17, matches the standard game-syscall numbering). Then returns.
   - `t = rec->+0xc; if (t != 0 && t <= level.time)` → `rec->+0xc = 0` and `func_0x2c3f(ent)` (send the request).
   - **Tail of `0x259a` is NOT auth** — it is the `target_fragsFilter` registry loop + unlimited-ammo hook, see [../G_TRIGGER/target_fragsFilter.md](../G_TRIGGER/target_fragsFilter.md). (Original note: loop over a table at `0x1317d8`, count at `0x1317d4`, compares `client+0xf8` with `entry->+0x2f8`, indirect call through `entry->+0x2c4`, then reads `0x13182c & 6` and `client+0x90`) was **seen but not decoded**.
3. `func_0x2c3f(ent)` (the request sender), gated on: global `0x1317c0 == 0` (**corrected**: earlier text had this inverted; `0x2c43 eq → continue`, otherwise return; `0x1317c0` = "dedicated/variant" flag, see ../G_UTILS/func_0x21fc.md — so requests go only to non-dedicated (listen) servers with df_gametype < 5; reading of intent is an inference); `ent->+0x328 != NULL`; `(ent->+0x1a8 & 8) == 0` — `ent+0x1a8` is `r.svFlags` (`0x1b498 = &g_entities[0]+0x1a8`, tested with `& 8 = SVF_BOT` in `CalculateRanks`, see ../G_CLIENT/func_0x6cc.md), so **bots are never sent a request**; `rec->+0x8 == 0` (no request outstanding). Then: `rec->+0x4 = rand()%0xf3e58 + 0x14d`; `rec->+0x8 = level.time + 1500`; `v = trap_Cvar_VariableIntegerValue("sv_mapChecksum")` (syscall `-0x7`); `msg = func_0xac4e("auth_request %i %i", rec->+0x4, v)` (`0xac4e` returns the formatted string and is used the same way as vanilla `va()` — shape-based, not hash-checked); `trap_SendServerCommand(*(int*)rec /* first word, see caveat */, buf)` (syscall `-0x12`).
4. **Reply side — decoded (second pass).**
   - `func_0x2d45(clientNum, cmd)` is a DeFRaG pre-dispatch in the client-command path (sole call `0x1f7f5`; if it returns nonzero the caller exits early). `func_0xaab7(cmd, "auth_reply")==0` (string compare, shape-identified as `Q_stricmp`) → `trap_Argv(1, buf, 255)` (syscall `-0xa`) → `func_0x2cab(buf, clientNum)` → returns 1. (It also handles a second command `"notify"`: `trap_Argv(1, buf, 0x400)`, then sends `va("print \"%s\"", buf)` back to the same client.)
   - `func_0x2cab(replyStr, clientNum)`: `rec = 0x1313bc + clientNum*16`. If `rec->+0x8 (deadline) == 0` → ignore. Else `ver = va("%i.%02i.%02i", 1, 0x5b, 0x1f)` (= **"1.91.31"**, literal constants), `expected = func_0x1594(rec->+0x4 /*token*/, ver)`, and if `expected != 0`, `got = func_0x5f67(replyStr)` (called with a string, returns int — atoi-shaped). `got == expected` → `rec->+0x8 = 0` (authenticated). Otherwise (or `expected == 0`) → `trap_DropClient(clientNum, "dropped: Authentication failed")` and `rec->+0x8 = 0`.
   - `func_0x1594(token, str)` (called only from `0x2cab`): returns 0 if `*str == 0` or `token < 0x14d` or `token > 0x18405`. Else, with `d = (token - (token % 100) / 10) % 10` and `sum = Σ over chars c of str ( (c - 3) - d )`, returns `token - 3 - 3*sum`. (Transcribed from the bytecode; `d` is held in a single-byte local `store1`/`load1`, read as such.)
   - **Discrepancy flagged:** the request token is `rand() % 0xf3e58 + 0x14d` (up to ~999 333) but `0x1594` rejects `token > 0x18405` (99 333) — for those tokens `expected == 0` and the check fails. Either the constants are read wrongly here or most random tokens would fail; not resolved.
   - `func_0x2d02` (no args, sole call `0x21fc`, an init-time path): `trap_Cvar_VariableStringBuffer("mapname", buf38, 32)`, a string helper `func_0xaadf(buf38)`, `trap_Cvar_VariableStringBuffer("sv_mapChecksum", buf18, 32)`, `path = va("system/checksums/%s.crc", buf38)`, `trap_FS_FOpenFile(path, &f, 1 /*FS_WRITE*/)` (syscall `-0xb`), `n = func_0x5bfc(buf18)` (strlen-shaped), `trap_FS_Write(buf18, n, f)` (`-0xd` = id 12 = FS_WRITE; **corrected**: an earlier version of this doc wrongly said FS_READ), `trap_FS_FCloseFile(f)` (`-0xe`). So it **writes the current `sv_mapChecksum` value into `system/checksums/<mapname>.crc`**; the cvar at `0x420b` is `"mapname"` (string-pool check). Purpose of the file (a checksum cache consumed by something else) not located.

## Caveats / unknowns
- The request's target client is `*(int*)(ent->+0x328)` = record word +0x0, which `func_0x271a` zeroes at connect; who writes it before use is unresolved (looks like it should be the clientNum, possibly stored elsewhere). Replies, by contrast, index the table by clientNum directly (`0x1313bc + clientNum*16`), so `+0x328` and that indexing agree on the record.
- Global `0x1317c0` is decoded in ../G_UTILS/func_0x21fc.md; the delay/timeout semantic beyond the code is unconfirmed.

## `func_0x3a47`
Identical in body to `func_0x3a5f`: `trap_Memset(&raceTimers[client - g_clients], 0, 0x20)` — i.e. `ClientBegin` also clears the client's `raceTimer_t` (same record as in [func_0x273f.md](func_0x273f.md)). Compiler did not merge the two copies.

## oDFs
Auth is a DeFRaG client/server protocol (needs a matching client reply); **not ported** (dropping unauthenticated clients would kick every client lacking the reply). The race-timer clear in `ClientBegin` is ported separately (ported in `ClientBegin`, `g_client.c`, right after the `ps` wipe and before `ClientSpawn`; syntax-check clean).

**Iter 7 (full-port loop) — port BLOCKED, not done:** re-read `0x2c3f` bytecode: `const 0xf3e58; modi; +0x14d` at `0x2c73-0x2c76` confirms the token range, so the discrepancy with `0x1594`'s `token > 0x18405` rejection is REAL in the binary (most tokens would fail verification as written); request target client (record word +0) also has no writer. A port would either kick everyone or require guessing. Needs a user decision (and ideally a runtime trace of a real DeFRaG listen server) before any code; requests are only sent when `0x1317c0 == 0` (non-dedicated), so dedicated oDFs servers are unaffected either way.

## Iter 16 (full-port loop) — ALL OPEN POINTS RESOLVED, PORTED (user: "port the auth system as precisely as possible")
Retractions of the "BLOCKED" note from iter 7:
- **Token range**: not a discrepancy. The QVM `rand()` (bg_lib) returns 15 bits (`& 0x7fff`), so `rand() % 0xf3e58 + 0x14d` is in `[0x14d, 0x829c]`, always inside `0x1594`'s accepted `[0x14d, 0x18405]`.
- **Request target**: record word `+0x0` is cleared by `0x271a` (`memset(rec, 0, 4)`) and never written, so requests always go to **client 0**. Even worse (verified): `0x271a` computes the record from `ps.clientNum` (`client+0x8c`) in `ClientConnect` *right after* the `memset(client, 0, 0x37c)` (`0x1d0e8`..`0x1d0f4`), i.e. ps.clientNum is 0 -> **every client shares record 0**. Consequence: in practice only the first request/deadline is live; the client whose `ClientEndFrame` runs first after an expired deadline is dropped. Ported literally (quirk included).
- **Reply format**: the DeFRaG cgame (`0x4337`) sends `auth_reply %i%i` = `<hash><rand()%10>` (a random decoy digit appended); the server pre-dispatch `0x2d45` copies `Argv(1)` (max 255) and **strips the last character** (`buf[strlen-1] = 0`) before `0x2cab`, which does `atoi(buf) == hash(token, "1.91.31")` (`0x5f67` is a plain atoi). `notify <text>` echoes `print "<text>"` back to the sender.
- cgame `0x3d52` handler: `auth_request <token> <crc>` -> `0x4337(atoi(argv1))`, then `0x6222(atoi(argv2))` (bsp-crc comparison, inactive for defrag_gametype < 5 -> not ported). Hash functions `cgame 0x11f9` and `qagame 0x1594` are the same code (constants verified identical).

oDFs: qagame `g_defrag.c` (`G_AuthConnect/Begin/Frame/Command`, `gentity_t.authRec`, hooks in ClientConnect, ClientBegin, ClientEndFrame (a dropped client skips the rest of that frame, as in `0x259a`), ClientCommand), cvar **`df_auth` default 0** (off: dropping clients that cannot answer is operator-visible). Active only when `df_auth 1` AND the server is non-dedicated (`g_dfVariant` false), as in DeFRaG (`0x1317c0 == 0`). cgame `cg_defrag.c` `CG_DF_AuthRequest` + `auth_request` handler: the oDFs cgame answers, so oDFs <-> oDFs works. Syntax-checked (qagame + cgame), untested at runtime. Revert: `df_auth 0` (default).
