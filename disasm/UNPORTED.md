# What is still NOT ported from the DeFRaG QVMs (compiled 2026-10-24)

Method: every "not ported / undecoded" note in `disasm/CGAME`, `disasm/UI` and `PORT_STATUS.md` was checked against `code/` (many old notes are
stale: the later iterations ported the flag-carrier / holdable boxes, jump meters, scoreboard draw, replay, miniview, ghost cameras, var* commands,
`df_demos_Seek`, `sv:/cl:` footer, ...). Phase A of the audit (string / cvar / command / syscall / table sweeps) found no other missing feature.

## Not ported, on purpose (config-overriding, opt-in or legacy)
| Item | Where | Why |
|---|---|---|
| Server `df_lockPromode`, `df_forceCvars` (g_speed / g_gravity / g_knockback forced) | qagame `0x23f2`, `0x3e10` ... | override operator config; opt-in cvars, default off |
| Auth handshake (`df_auth`) | qagame `0x259a`, cgame `0x3d52` / `0x4337` | drops clients that do not answer; opt-in, default off (ported precisely) |
| Legacy text protocol `TimerStarted` / `TimerStopped` / `NewTime` / `route` (+ `newTime`, `rec`, `bestTime` pre-1.91 forms) | cgame `0x3d52` | pre-1.91 protocol, our qagame never sends it |
| debug password strings (`nightmonkey`, ...) | cgame | not wanted |
| Cvars `gamedate`, `gamename`, `sv_mapname` | qagame | cosmetic |
| Vanilla `cg_stereoSeparation` | cgame | the engine registers it as a read-only deprecated cvar; stereo goes through `r_stereoSeparation` (ACCEPTED by the user) |

## Not ported, debug / diagnostic only
- Replay verbose messages: `Deactivated by`, `Disabled Cleanup`, `Spectator Cleanup`, `Change [ i ] ( CamID = %i )`.
- `ghostlog.log` debug writer (cgame `0x10952`), `Hud_RegPU(): no stack found.`
- Repeat log of the run-validity reporter (cgame `0x59b4`, array `0x11d7a0`); the first occurrence of each code IS printed.
- Ghost "extra effects" (muzzle / launch effects through a console command, flag at data 8348, off by default) and the global at 73972.

## Known differences from the original (ported, but not identical)
- Best-time scoreboard column / ranking on every server type (original: dedicated / variant only).
- `target_*` / shooter details listed in `OPEN_QUESTIONS.md` (all closed).
- Several qagame cvar flags (`df_ndm_*`, `df_promode`, ...: LATCH-only in the original, SERVERINFO / ARCHIVE here), CHEAT flags left off on `g_speed` / `g_gravity` / `g_knockback` / `dmflags`.
- Everything the cgame / ui re-audit (started 2026-10-24) finds will be added below.
- Scoreboard (re-audit iter 8): the DeFRaG table is also shown during PM_INTERMISSION without the scores key (original: only while `cg.showScores`).
- var* commands (re-audit iter 11): the original writes results through the "safe set" `0x80bf`: names without the `user_` prefix (and cgame-registered protected cvars) are changed by issuing a console `set name value` (applied next console frame, user-created flag), `user_*` names are set immediately; the port always sets immediately with `trap_Cvar_Set`. The `ISSUING SAFE / UNPROTECTED VAR CHANGE` log lines only print in the password-protected debug mode (999) and are not ported. `$pstatNN` / `$bpstatNN` (same debug gate) are not ported. ACCEPTED by the user (kept as is).
- OB predictor debug print (`0xb2ea`, re-audit iter 16): when `df_chs2_Info8` holds 900 the original prints a per-slot `OB ( %5i ) = false/now/true ...` diagnostic line pair to the console on every evaluation; not ported (hidden debugging aid).
- Legacy text-protocol commands (`TimerStarted`, `TimerStopped`, `NewTime`, `route`, `ckpt`) of old servers / demos are not interpreted (`TimerStarted` -> `0x1ae1`, `TimerStopped` -> `0x1aec` + run-settings snapshot for versions < 18100).
- ui server browser (q3_ui `ui_servers2.c`, DeFRaG 1.30-style `0x15b60`): the DeFRaG source spinner is Local / Mplayer / Internet / Favorites (Mplayer = master 1, Internet = master 0); the port keeps the ioq3 list (Local, Internet, Master1..5, Favorites) (the Punkbuster spinner was removed in the follow-up pass). ACCEPTED by the user (kept on purpose). Event handler `0x15a3a` and StartRefresh `0x15871` were compared in the follow-up pass (only the master numbering differs).
- ui old Q3 start-server menu (`ServerOptions_*` `0x18141` / `0x18974`): compared in the follow-up pass; remaining differences are the unregistered `ui_team_friendlt` / `ui_ctf_fraglimit` cvar names the original writes (typos / older names; the port writes the registered names), `UI_Message` (dead code in DeFRaG, no PunkBuster strings); player-slot rows compared and identical. The Load / Save configuration menus do not exist in the DeFRaG ui (no strings, no entry in the setup menu).

## Mixed / unverifiable
- The 41 % of cgame and 60 % of ui functions that are hash-matched are assumed to be unmodified Q3 code (constants not compared).
- Layout of several cgame draw paths (HUD widget positions, scoreboard columns) follows the decode; pixel-exactness is not proven.
