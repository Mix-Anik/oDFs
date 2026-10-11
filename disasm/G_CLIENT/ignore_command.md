# `ignore` command and the `client+0x2b4/0x2b8` bitmask

**Resolves** the "per-client bitmask pair, purpose unidentified" from [ClientDisconnect.md](ClientDisconnect.md): it is the
per-player **ignore list** (64 bits: bit n of `client+0x2b4` for n < 32, bit n-32 of `client+0x2b8` for n >= 32).

## Evidence
- `func_0x1de5e(ent)` — `ClientCommand` handler for `"ignore"` (call at `0x1f7ea`). Strings: `"print \"Now ignoring %s.\n\""` (`0x8a4a`),
  `"print \"Now unignoring %s.\n\""` (`0x8a64`).
- `argv(1)` → `func_0x1da64(ent, arg)` = a `ClientNumberFromString`-shaped lookup (returns -1 if not found → command ends); then
  `func_0x1de19(ent, target)` = `IsIgnoring` (bit test, below): returns 1 → print "unignoring", else "ignoring" (message uses
  `g_clients[target].pers.netname` = `client+0x200`); finally toggles the bit: `client->+0x2b4 ^= 1 << target` (target 0..31) or
  `client->+0x2b8 ^= 1 << (target-32)` (32..63).
- `func_0x1de19(ent, n)`: `if 0 <= n <= 31: return (ent->client->+0x2b4 >> n) & 1` (and the matching `+0x2b8` branch for 32..63; the
  upper half was not re-read this pass but mirrors the toggle code).
- Consumer: `func_0x1e614` (= `G_Say`, args `(ent, target, mode, text)`): in both the targeted branch and the per-listener loop it calls
  `IsIgnoring(listener, speaker->ps.clientNum)` (bytecode `0x1e764`, second site `~0x1e8xx`) and skips delivery when it returns 1, before the
  `func_0x1e5ac` (= `G_SayTo`) call.
- `ClientDisconnect` clears the disconnecting client's bit in every client's mask (decoded earlier; now explained).

## oDFs
**Ported** (syntax-check clean, not run): `clientPersistant_t.ignoreMask[2]`, `G_IsIgnoring`, `Cmd_Ignore_f` (`g_cmds.c`, dispatched with the other
commands), the listener check at the top of `G_SayTo` (slightly broader than DeFRaG: it also covers other callers of `G_SayTo`, e.g. `tell`),
and the disconnect-time bit clear in `ClientDisconnect` (`g_client.c`).

**Follow-up check (call-graph):** `IsIgnoring` (`0x1de19`) has exactly three call sites — the `ignore` command itself and the two inside `G_Say`
(`0x1e614`). So DeFRaG's **voice chat (`vsay`/`vtell`/`vosay`…) does NOT honour the ignore list**. Also `G_SayTo` (`0x1e5ac`) is called only from `G_Say`,
so in DeFRaG `tell` is routed through `G_Say` and *is* covered; oDFs's `tell` calls `G_SayTo` directly, so the `G_SayTo`-level check matches.
