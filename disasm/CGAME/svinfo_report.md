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


## Iter 59 — `svinfo_summary` (`0x4b67`) DECODED & PORTED; file-name filter `0x153d` decoded
`0x153d` removes `^x` colour codes (same walk as Q_CleanStr; control characters kept apart from the pair handling). File name = `system/reports/` + filtered argument + `.txt` (`0x31e65` default extension). `0x4b67(file, timeMs, interactive)` writes (all lines CRLF): `<defrag_servername> {`; if the server version is known: [`  time "<mm:ss:ms>"` when timeMs != 0], `  map "<mapname>"`, `  gametype "<name>"`, `  physics "vq3|cpm"`, [`  mode "%i"` if != -1], [`  overbounces "%i"` if != -1], `  player "<clean name>"`, [`  comp "<competition>"` if set], `  version "x.yy.zz"`; else `gametype "%i"`, `map`, `player` (raw name), `gamename`, `fs_game`. Then, when violations were logged: blank line, `  validation {` and `    warnNN "<text>"` per entry; `  }` is written unconditionally (so a stray closing line appears without violations); finally `}` + blank line. All lines use the `"  %-12s \"%s\"
"` layout. Messages: `usage: svinfo_summary [file]`, `SVI_WriteSummary(): failed to open "%s" for writing.`, `report written to system/reports/<file>.txt`. oDFs: `CG_DF_SvinfoSummary_f`; the file name filter now strips colour codes in both commands and keeps replacing path characters by `_` (DeFRaG does not). Built with the full QVM build, warning-free. Revert: remove the table / AddCommand entries.

## Iter 103: demo validation `0x4db6` (df_demos_validation)
`0x4795` (report) / `0x4b67` (summary) really take `(name, timeMs, usage)`; the commands pass `(argv1, 0, 1)`. A non-zero `timeMs` adds a `time` line (via `0x1024` = S:mmm formatter) right after `validation` (report) / first inside the summary block. `0x4db6(ms)` is called from the finish handler `0x1aec` (and the legacy text command at `0x3f22`) and runs only when `cg.demoPlayback (0xcfcd0)` and version >= 18100: `df_demos_validation (0x11d330) >= 1` -> summary file `<date>_summary` (+ `<date>_failed.txt` containing `defrag_servername` when a violation was logged, `0x5b02` = `0x11e7a0` count != 0); `>= 2` -> full report `<date>.txt`. `<date>` = `0x4775` = `%04i-%02i-%02i` of the real-time clock. Ported: `CG_DF_DemoValidation`, workers `CG_DF_WriteReport/WriteSummary` (cg_defrag.c), cvar `df_demos_validation` default 0 (DeFRaG default). Legacy `0x3f22` text-command path not ported. Revert: remove the `CG_DF_DemoValidation( ms );` line in `CG_DF_SnapshotTransition`.
