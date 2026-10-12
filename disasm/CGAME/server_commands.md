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
