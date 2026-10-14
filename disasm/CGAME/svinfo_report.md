# cgame `svinfo_report` (`0x4795`) — report file writer

Console command `svinfo_report [file]` (string `svinfo_report` in the cgame command table, usage `usage: svinfo_report [file]`). `0x4795(file, timeMs, interactive)`; the `0x4b67` sibling is `svinfo_summary` (different, brace-structured format; NOT decoded/ported).

## Flow (decoded from bytecode this pass)
1. Empty name: print usage when `interactive`, return. Path = `system/reports/` + name (filtered by `0x153d`, undecoded) + `.txt`; `FS_FOpen(path, FS_WRITE)`; failure -> `SVI_WriteReport(): failed to open file for writing.`
2. Header `=
= Report for %s (%s)
=

` with `defrag_servername` and the date `%04i-%02i-%02i` (`0x4775` via `trap_RealTime`, trap `-0x47`).
3. `*** Info` block, lines `"%-24s %s
"`: `validation` (`passed` iff the violation log count `*0x11e7a0` == 0, else `failed`), optional `time` (if the 2nd arg is non-zero: `0x1024(ms)`), `map` (`0x11d2bc`), `gametype` (name table at `0x1260` indexed by `defrag_gametype`: Auto-config mode / Defrag / Tricks Mode / Fast Caps / Reserved / MP Defrag / MP Tricks Mode / MP Fast Caps), `physics` (table `0x12c0`: vq3/cpm/VQ3/CPM), `mode` (only if `!= -1`, `%i`), `overbounces` (only if `!= -1`), `player` (clientinfo name of the snapshot client), `comp` (`df_competition`), `version` (`%i.%02i.%02i` from `defrag_vers`). When `defrag_vers` is 0 a shorter layout is used (validation, map, gametype `%i`, player, `gamename`, `fs_game`).
4. If violations were logged: `
*** Validation Report : %i
` + one `%s
` per log entry (the `0x59b4` log, see cheat_report.md).
5. `
*** Server Info
` / `
*** System Info
`: every key/value of configstring 0 / 1 as `%-24s %s`; `
*** sv_referencedPaks
` and `
*** sv_paks
`: checksum/name pairs (`sv_referencedPaks`+`sv_referencedPakNames`, `sv_paks`+`sv_pakNames`).
6. Close; interactive: `report written to %s%s` (`system/reports/`, file).

## oDFs — PORTED (full-port loop iter 14, `cg_defrag.c`, command table in `cg_consolecmds.c`)
`CG_DF_SvinfoReport_f` implements 1-6. Deviations: the name filter `0x153d` is unknown, so `/ \ : .` in the name become `_`; the `time` line is never written (console invocation: the wrapper that supplies the args is not decoded, assumed 0); `mode`/`overbounces` come from `defrag_mode`/`defrag_obs` serverinfo keys (oDFs publishes `defrag_mode`; `defrag_obs`/`df_competition`/`defrag_servername` are not published -> empty/skipped); the violation log is kept by `CG_DF_Violation` (first occurrences only). Syntax-checked, untested. Revert: remove the command table entry.
