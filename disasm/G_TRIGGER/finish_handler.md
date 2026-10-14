# DeFRaG finish handler `0x35ce` and time formatter `0x13bf` / `0x1463`

`0x35ce` (`enter 0x1cc`, arg0 = client `gentity_t*`; called from `func_0x37c7` on the FINISHED transition, see func_0x37c7.md) is the "client finished the race" handler.
`0x13bf(ms)` (6 calls from `0x35ce`) = **time formatter**, returns one of two 32-byte ring slots (`0x12c7f0 + (idx<<5)`, `idx = (idx+1)&1`, `0x12c830` = idx): negative -> leading `-`;
`ms < 60000` -> `"%s%i:%03i"` (seconds:millis, e.g. `12:345`), else `"%s%i:%02i:%03i"` (minutes:seconds:millis). `0x1463` is a second, more configurable formatter (`+`/`-` sign, colour `^2`, `:`-separator; strings `"%s%s%i:%03i"`) — not decoded further.

## `0x35ce` flow (decoded from strings + call structure)
1. `cheats = Cvar_Integer("sv_cheats")` (`0x3e0a`); if non-zero: `print "^1Cheats are ENABLED^7"` to the client and **return** (no time is announced/logged with cheats on).
2. `rec = raceTimer(client)` (`0x3588`); if `0x1317c0 != 0` (dedicated/variant flag, see G_UTILS/func_0x21fc.md): broadcast `print "%s^7 reached the finish line in ^2%s^7 (est. %s)"` with `netname` (`client+0x200`), `FormatTime(rec[0])` (whole ms) and `FormatTime(rec[1])` (the sub-ms-corrected "est." time); then log/bookkeeping calls (`0x18d8`, `0xa1a`=`G_LogPrintf`-like), and the log line
   `"ClientTimerStop: %i %i \"%s\" \"%s\" %i %i %i %i %i %i %s\n"` (clientNum, ms, mapname, name, mode `0x1317bc`, promode `0x1317c4`, `0x1317c8`, `0x132660`, `(0x132550==0)`, ..., date `"%04i-%02d-%02d"`) — arguments after the name are only partially identified (not ported).
   Then a best-time/rank update: `client+0x2b0`-based record compare and `CalculateRanks (0x6cc)` + a ClientUserinfo-ish refresh (`0x1ccdf`).
3. Else (`0x1317c0 == 0`): send `stats` server command; `print "Date: %02i-%02i-%02i %02i:%02i"`; `print "Competition: %s"` (global `0x1312bc`, `df_competition`); route = `defrag_route` cvar; version string `"%i.%02i.%02i"` = (1, 91, 31); `print "Time performed by ^2%s^7 on route ^2%s^7 : ^3%s^7 (v%s%s) (est. %s)"` with `" beta"` suffix.

## oDFs — PORTED (partially)
`g_defrag.c`: `G_FormatTime` now uses the DeFRaG layout (`S:mmm` / `M:SS:mmm`, sign), `G_RaceFinished` prints the DeFRaG wording with both the whole-ms time and the `(est. …)` value
(`g_dfVariant` branch: broadcast "reached the finish line…"; otherwise "Time performed by … on route … : … (est. …)" to the finisher only — without DeFRaG's `(vX.Y.Z[ beta])` tag and Date/Competition lines).
Not ported: the `sv_cheats` suppression, the `ClientTimerStop` log line, the `stats` command and the best-time/rank bookkeeping.
**Behaviour change:** finish-time text changes from `m:ss.mmm` to DeFRaG's `S:mmm` / `M:SS:mmm`; add "(est. …)". Revert = restore the old `G_FormatTime`/`G_RaceFinished` in `g_defrag.c`. Syntax-checked, untested.

**Iter 2 (full-port loop): PORTED** the `sv_cheats` suppression (prints `^1Cheats are ENABLED^7`, returns before ranking) and the `stats` server command (non-variant branch) in `G_RaceFinished`. **Behaviour change:** with `sv_cheats 1` finishes are no longer announced/ranked. Revert: delete those two blocks in `g_defrag.c`. Still unported: `ClientTimerStop` log line (args partly unidentified).
