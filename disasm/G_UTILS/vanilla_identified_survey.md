# Vanilla functions identified during the undocumented-function survey (qagame)

Script idea: parse the `0x<addr> 0x<size> <hash>` rows at the top of `qagame.dis`, drop hashes found in `baseq3-game-functions.hmap`, drop addresses mentioned in any `disasm/**/*.md`; ~60 remained after the first round, sorted by size. These were checked by embedded strings / call structure and are **vanilla-shaped (no DeFRaG-specific behaviour found; nothing to port)** — their hashes only differ from the baseq3 hmap (different compiler flags/data layout, e.g. `g_gravity` read through a helper):

| Addr | Identification | Evidence |
|---|---|---|
| `0x25210` | `ConsoleCommand` (vmMain case 9, `GAME_CONSOLE_COMMAND`) | trap `-0xa` Argv(0); `entitylist`, `forceteam`, `game_memory`, `addbot`, `botlist`, `abort_podium`, `addip`, `removeip`, `listip`, then `g_dedicated` -> `say` ("print \"server: %s\""); same list as oDFs `g_svcmds.c` |
| `0x1f041` | `Cmd_CallTeamVote_f` | strings "Voting not allowed here.", "A team vote is already in progress.", "You have called the maximum number of team votes.", "leader", "%s is not a valid player on your team." |
| `0x1f34f` | `Cmd_TeamVote_f` | strings "No team vote in progress.", "Team vote already cast.", "Team vote cast." |
| `0x1cbf7` | `ClientCleanName` | string "UnnamedPlayer" |
| `0x1d880` | `DeathmatchScoreboardMessage` | strings `"scores %i %i %i \"%s\"%s"`, `" %i %i %i %i"` |
| `0x245b7` | `SP_func_pendulum` | `"speed"` default "30", `"dmg"` default "2", `sqrt` trap |
| `0x27930` | `AimAtTarget` (trigger_push) | box centre = (absmin+absmax)*0.5, `G_PickTarget(self->target)`, free self if no target, `height = target.z - origin.z`, `time = sqrt(height / (0.5*gravity))`, free if 0 |
| `0x24d9a` | `StringToFilter` (g_svcmds ban IPs) | string "Bad filter address: %s" |
| `0x208f1` | speaker spawn (`SP_target_speaker`-shaped) | spawn keys "noise", "sound", "angles" |
| `0x16d9` | `SYS_extractPath`-like helper | strings "SYS_extractPath() error: insufficient buffer for file path/name." (DeFRaG/auth file helpers, see auth_handshake.md) |

Still unidentified from the list: `0x6175`, `0x608b` (called from `0x633c`, the vsprintf-style formatter — probably float/int formatting helpers), `0x1f972`, `0x27d3c`, `0x1d880`'s sibling bookkeeping; re-run the survey script to get the current worklist.
