# DeFRaG cgame server-command dispatch (cgame `0x3d5f`..`0x3f..`, region of `CG_ServerCommand`)

String compares found in the DeFRaG cgame dispatcher: `auth_request`, `cpsm`, `stats`, `aswitch`, `ckpt`, `TimerStarted`, `TimerStopped`, `NewTime` (+ `"^3Time performed by ^2%s"`, `"route"`, `"^3 on ^7%s^3 route"`, `"^3 : ^7%s "`, `"^3(%s)"`), `df_options`, `chkpt`, `rec`, `newTime`, `exec`, `bestTime`
(plus the vanilla `cp`, `cs`, `print`, `chat`, ... ). oDFs's cgame handles only the vanilla set plus the ones ported below.

| command | sender (qagame) | cgame behaviour | oDFs |
|---|---|---|---|
| `cpsm "<text>"` | `target_smallprint`, `target_fragsFilter` | `if (!df_noTargetPrint) CG_CenterPrint(Argv(1), 200, 10)` | **ported** |
| `aswitch <w>` | `Touch_Item` on `IT_WEAPON` pickups (`ent->item->giTag`) | `cg_autoswitch`: 0 never; 1 always `weaponSelect = w`; otherwise only if `!(snap->ps.stats[STAT_WEAPONS] & (1<<w))`. No machinegun exclusion | **ported** (server + handler; vanilla `CG_ItemPickup` path kept only for `cg_autoswitch == 1`) |
| `auth_request`/auth reply | `func_0x259a` | client half of the auth handshake (G_CLIENT/auth_handshake.md) | not ported |
| `stats` | finish handler / spawn code (encoded timer values) | (`0x4388` call, gated on cvar `0x11bdd0`) | not ported |
| `ckpt`, `chkpt`, `TimerStarted`, `TimerStopped`, `NewTime`, `newTime`, `rec`, `bestTime`, `route`, `df_options`, `exec` | no qagame sender found yet in the vanilla-ish paths (several look like **client-side** command names fed through `CG_ServerCommand` by cgame itself, e.g. demo playback / local timer) | not decoded | not ported |

**oDFs — PORTED** (`g_items.c` Touch_Item, `cg_servercmds.c`, `cg_event.c`): weapon pickups now send `aswitch`; `cg_autoswitch` values >= 2 become meaningful.
**Behaviour change:** none for `cg_autoswitch` 0 or 1 (same switch, now also driven by the server command); value 2+ no longer switches to already-owned weapons. Revert = delete the `aswitch` branch and the server send. Syntax-checked, untested.

## Legacy/demo-compat commands (decoded structure only)
`ckpt`: consumed silently (`return 1`). `TimerStarted`: calls cgame `0x1ae1()`. `TimerStopped`: `if (global 0x11d29c < 0x46b4 /*18100, a demo/server version gate*/) { v = atoi(Argv(1+..)) ...; 0x11d590 = atoi(Argv(n+6)); 0x11d5a0 = atoi(Argv(n+7)) }` then `0x1aec(atoi(Argv(1)))`.
`NewTime`: `0x6222(atoi(Argv(1)))`, then prints `"^3Time performed by ^2%s"` (`0x23c`, Argv(4)), followed by `route` handling (`"^3 on ^7%s^3 route"`, `"^3 : ^7%s "`, `"^3(%s)"`). These look like the **pre-1.91 text protocol** for race results kept so old demos still play; current DeFRaG servers announce results via `print` (see G_TRIGGER/finish_handler.md). Not ported (low value).

## Server -> client command contract (complete for the current DeFRaG qagame)
Command strings present in the DeFRaG **qagame** literal pool (i.e. the only ones it can send): `auth_request %i %i`, `auth_reply` (client reply, server side verifies), `cpsm "%s"` / `cpsm "%i more frag(s) needed"`, `aswitch %i`, `stats` (no arguments), `print "..."`, `cp`/`chat`-style vanilla ones.
- `cpsm`, `aswitch`, `print`: sent and handled — oDFs now sends all three.
- `stats` (no args): sent by the finish handler `0x35ce` only on its **non-variant** branch (`0x1317c0 == 0`, see G_TRIGGER/finish_handler.md) to make the finisher's cgame print the end-of-run stats; the dedicated/variant branch does not send it (the client prints its stats from its own timer events). oDFs does not send it (no difference for dedicated play).
- `auth_request`: not ported (needs the matching client reply).
- cgame also accepts `df_options <n>` (stores `atoi(Argv(1))` in cgame global `0x11d2b8`), `chkpt "<text>"` (small centre print `CG_CenterPrint(text, 200, 10)` when the checkpoint cvar `0x11bee0` is set) and silently swallows `rec`, `newTime`, `exec`, `bestTime`, `ckpt`;
  **no string for any of these exists in the qagame literal pool**, so the current DeFRaG qagame never sends them (legacy/other-server compatibility). Nothing for oDFs to add.

## Second pass — the whole DeFRaG dispatcher decoded (`cgame 0x3d52`, called from `0x28d70`; prelude of `CG_ServerCommand`)
String compares in order (`0x2f2e0` = `strcmp`-like on `CG_Argv(0)`, `0` = equal; everything returns 1 = consumed, so vanilla handling is skipped for these):
| command | cgame action | does DeFRaG qagame send it? |
|---|---|---|
| `auth_request` | auth handshake (`G_CLIENT/auth_handshake.md`) | yes |
| `cpsm "<text>"` | small centre print (`df_noTargetPrint` gate) | yes (target_print/target_smallprint) |
| `stats` | end-of-run stats dump (`cgame_stats_dump.md`) | yes, finish handler non-variant branch |
| `aswitch <w>` | weapon auto-switch on pickup | yes |
| `ckpt`, `TimerStarted`, `TimerStopped`, `NewTime`, `route` (+ `"^3Time performed by ^2%s"`, `"^3 on ^7%s^3 route"`, `"^3(%s)"`) | legacy text protocol (old servers/demos); prints formatted messages | **no** (no such literals in qagame) |
| `df_options <n>` | `defrag_flags` global (`0x11d2b8`) `= atoi( CG_Argv(1) )` — lets a server change the client's flags word at runtime | **no** |
| `chkpt "<text>"` | if checkpoints display enabled (`*0x11bee0 != 0`): `CG_CenterPrint( CG_Argv(1), 200, 10 )` | **no** |
| `rec`, `newTime`, `exec`, `bestTime` | **swallowed with no action** (`if !strcmp → return 1`) — in particular a server can NOT make the client `exec` a config via `exec` | **no** (qagame's only `exec %s` literal, in `0x23f2`, is a *server* console command for its idle script) |
Anything else falls through to vanilla `CG_ServerCommand`. Contract conclusion: oDFs sends exactly the commands DeFRaG qagame sends (+ `auth_request`/`stats` not implemented, harmless); nothing else is needed. The `df_options` command is the only one with a possible server-side use (changing `defrag_flags` mid-map without a serverinfo update), unused by DeFRaG's qagame.

**Iter 9 (full-port loop): PORTED** in `cg_servercmds.c`: `chkpt` (popup, gated by df_drawCheckpointEvents), `df_options` (stored in `cg_dfOptions`, no consumer yet), and swallowing of `stats` (the end-of-run stats dump itself is NOT ported; needed because oDFs qagame now sends `stats` from the finish handler — before this it would have printed 'Unknown client game command'), `rec`, `newTime`, `exec`, `bestTime`, `ckpt`. Not ported: legacy `TimerStarted`/`TimerStopped`/`NewTime`/`route` printing. Syntax-checked, untested.
