# Server info keys read by the DeFRaG cgame (serverinfo / systeminfo / userinfo) vs what oDFs publishes

Method: every call of cgame `Info_ValueForKey` (`0x32784`) whose key string literal is visible just before it (script over `cgame.dis`) → **50 distinct keys**; checked against the oDFs qagame source (`code/game`). (Info keys read from *userinfo/configstring of players* like `n t model hmodel c1 c2 hc w l skill tt tl dfn` are the vanilla `CG_NewClientInfo` set.)

| key | read by | oDFs publishes? | consequence if absent |
|---|---|---|---|
| `defrag_vers` | `0x3c33` (version parse) | yes (`19131`, SERVERINFO\|ROM) | — (required; see `cgame_timer_hud.md`) |
| `defrag_flags`, `defrag_gametype`, `defrag_mode` | `0x3b5e` (serverinfo parse) | yes | — |
| `df_promode` | `0x3b5e` | yes | — |
| `dmflags` | `0x5dd5` validator | yes | — |
| `g_synchronousClients`, `pmove_fixed`, `pmove_msec` | `0x5b89` systeminfo validator | yes (SYSTEMINFO) | info lines only |
| `g_speed`, `g_gravity`, `g_knockback` | same | yes (SYSTEMINFO — fixed earlier this session) | "is missing" violation |
| `sv_cheats`, `timescale` | same | engine | — |
| `defrag_route` | `0x34f5`/timer text | yes (`g_defrag.c`) | — |
| `defrag_svfps`, `defrag_clfps`, `defrag_bspcrc` | `0x5b89` | **no** | checks gated off for `defrag_gametype < 5` (`cheat_report.md`) — no effect |
| `defrag_obs` | `0x3a70` (version ≥ 18100 only; stored `0x11d2b4`, else -1) | **no** | value reads as 0: only the info/report text "overbounces" shows off. Display-only consumers: `0xc320`-region info panel (`"overbounces"` / `"on"`), `svinfo_report` (`0x4795`), summary (`0x4b67`). DeFRaG sets `defrag_obs = (df_ob_KillOBs != 0)` (`G_UTILS/func_0x21fc.md`). |
| `df_competition` | `0x3b5e` → 256-byte buffer `0x11d30c` | **no** | empty string; used in reports/demo naming (`"comp"` line in `svinfo_report`), not traced further |
| `o3j_version` | `0x7f2a` → `0x120bcc` | **no** | empty; version/report text |
| `defrag_servername` | `0x3c33`, `0x34f5`, `svinfo_report` | **no** | empty server name in reports/HUD title — cosmetic |
| `teamflags`, `g_redTeam/g_blueTeam` (+ lowercase), `capturelimit`, `fraglimit`, `timelimit`, `g_gametype`, `mapname`, `gamename`, `fs_game`, `sv_hostname`, `sv_maxclients`, `sv_pure`, `sv_pakNames`, `sv_paks`, `sv_referencedPak(Name)s`, `skill`, `hc` … | vanilla CG_ParseServerinfo / CG_NewClientInfo / report | engine or vanilla qagame | — |

## Conclusion
No missing key breaks gameplay or the HUD. The only unpublished keys that the DeFRaG cgame consumes are **cosmetic / report fields** (`defrag_obs`, `df_competition`, `o3j_version`, `defrag_servername`) plus the version-gated fps/crc checks that are off for oDFs. If the server-info panel or `svinfo_report` should show a server name / "comp", register `defrag_servername` (SERVERINFO) and `df_competition` (SERVERINFO\|LATCH, DeFRaG default empty) — optional; not done. `svinfo_report` / `svinfo_summary` (cgame `0x4795`/`0x4b67`) are client console commands writing validation reports to `system/reports/*.txt` (fields: validation passed/failed, time, map, gametype, physics, mode, overbounces, player, comp, version, pak lists).
