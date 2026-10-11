# DeFRaG `callvote` / `cv` (`func_0x3142`) — the DeFRaG vote system

`ClientCommand` dispatches both `"callvote"` and `"cv"` (alias; string `0x81bd`) to **`func_0x3142(ent)`**. Replaces vanilla's `g_allowVote`-based
`Cmd_CallVote_f` with a per-command table and `df_vote_*` cvars. **Not ported** (oDFs keeps the vanilla vote code); decode recorded below.

## Flow (bytecode `0x3142`-`0x335b`, strings from the pool)
1. `if ( *(int*)0x1317c0 == 0 ) return;` — voting is only active when the dedicated/variant flag is set
   (see [../G_UTILS/func_0x21fc.md](../G_UTILS/func_0x21fc.md)); silent return otherwise.
2. `if ( level.voteTime (0x130d58) ) print "A vote is already in progress."`.
3. `if ( !cvar_int("df_vote_allowVotes") )` → `"Voting not allowed here."` (cvar read via `func_0x1b675(name)`).
4. `max = cvar_int("df_vote_maxVotes"); if ( max && client->+0x25c (voteCount) >= max )` → `"You have used up all of your votes."`.
5. `argv(1)` (vote command) and `argv(2)` (argument) into 64-byte buffers; empty `argv(1)` → `func_0x2da3(ent)` (help/list, not decoded).
6. Clears `0x130858` (0x504 bytes: vote strings) and, for every client, `client->+0x68 (ps.eFlags) &= ~0x4001` (clears `EF_VOTED`) and
   `client->+0x280 = 0` (per-client vote state).
7. Walks the command table (below, stride `0x18`, start `0x94c`) comparing `argv(1)` with entry `+0` using `func_0xaab7` (stricmp-shaped):
   - `cvar_int(entry->+8)` (the per-command allow cvar) == 0 → `"Vote command ^3%s^7 not allowed here."`;
   - `entry->+0x10` (needs-arg) and no `argv(2)` → `"Missing argument.\n%s"` with `entry->+0x14` (usage);
   - `entry->+0xc` (validator, indirect call `fn(ent, argv(2))`) returning 0 → if `0x130958` is still empty, fill it with
     `"Invalid argument.\n%s"` + usage; print `0x130958` and abort;
   - no match → `"Vote command not recognized."`.
8. Start the vote: `level.voteTime = level.time + 1000 * cvar_int("df_vote_voteDelay")` — **correction (see CheckVote below): `voteTime` is the vote's *deadline*, so
   `df_vote_voteDelay` is the vote duration in seconds**; `client->ps.eFlags |= EF_VOTED (0x4000)`; `client->+0x280 = 1` (caller votes yes); `client->+0x25c++`
   (vote counter); if `0x1308d8` is empty: `Com_sprintf(0x1308d8, 0x80, "%s %s", entry name, argv(2))`; if `0x130858`
   is empty: `Com_sprintf(0x130858, 0x80, entry->+4 /*format*/, argv(2))`. **Label correction:** `0x130858` is the *command executed when the vote passes* (CheckVote runs it with
   `trap_SendConsoleCommand`), `0x1308d8` is the *human-readable text* shown in the "called a vote" message (e.g. `kick <name>` vs executed `clientkick <num>`); broadcast
   `"print \"%s ^7called a vote: %s.\n\""` (`netname`, vote string); `trap_SetConfigstring(8 /*CS_VOTE_TIME*/, va("%i", voteTime))`; the
   remainder (`0x3327`-`0x333d`, decoded): `trap_SetConfigstring(CS_VOTE_STRING /*9*/, 0x1308d8)`, `(CS_VOTE_YES /*10*/, "1")`, `(CS_VOTE_NO /*11*/, "0")`, then return. If `argv(1)` matches no table entry the loop (`0x3344`) ends and prints `"Vote command not recognized."`.


## Command table at `0x94c` (name, display/vote format, allow-cvar, validator fn, needs-arg, usage string ptr)
| name | format | allow cvar | validator | arg |
|---|---|---|---|---|
| `map` | `map %s` | `df_vote_allow_map` | `0x2e3f` | 1 |
| `devmap` | `devmap %s` | `df_vote_allow_devmap` | `0x2e83` | 1 |
| `map_restart` | `map_restart 3` | `df_vote_allow_map_restart` | — | 0 |
| `nextmap` | `vstr nextmap` | `df_vote_allow_nextmap` | `0x2ec7` | 0 |
| `gametype` | `df_gametype %s; map_restart 3` | `df_vote_allow_gametype` | `0x2ee7` | 1 |
| `promode` | `df_promode %s; map_restart 3` | `df_vote_allow_promode` | `0x2fa4` | 1 |
| `mode` | `%s; map_restart 3` | `df_vote_allow_mode` | `0x2f39` | 1 |
| `kick` | `` (empty) | `df_vote_allow_kick` | `0x2fdf` | 1 |
| `clientkick` | `clientkick` | `df_vote_allow_clientkick` | `0x303a` | 1 |
| `interference` | `df_mp_InterferenceOff %s; map_restart 3` | `df_vote_allow_interference` | `0x30b8` | 1 |
| `kill_OBs` | `df_ob_KillOBs %s; map_restart 3` | `df_vote_allow_killobs` | `0x3107` | 1 |

(Table decoded by script from the data segment; strings are pool literals.)

## Validators (second pass; signature `fn(ent, argv2)` → nonzero = valid, they fill the vote string `0x1308d8` and display string `0x130858`)
Argument parsing helper pattern: first char must be `'0'..'9'` (`lti 0x30 / lei 0x39`), then `atoi`-shaped conversion, then a range check; failure
returns 0 and the caller prints `"Invalid argument.
<usage>"`.
| cmd | validator | rule | strings produced |
|---|---|---|---|
| `map` | `0x2e3f` | `va("%s.bsp")` must exist in `"maps"` (file-list lookup) else `"Unknown map."`; | vote string `map %s; set nextmap "%s"` (second `%s` = current `nextmap` cvar) |
| `devmap` | `0x2e83` | same file check | `devmap %s; set nextmap "%s"` |
| `nextmap` | `0x2ec7` | `nextmap` cvar must be non-empty else `"Nextmap not set."` | — |
| `gametype` | `0x2ee7` | digit, value 1..3 | display `gametype %s` with a name looked up from a table at `0x8c0` (index = value); vote value `"%i"` |
| `mode` | `0x2f39` | digit, 1..7; only allowed when current DeFRaG mode `0x1317bc` is 2 or 3, else `"Game modes don't apply to the %s gametype."` | vote string `"%s %i"` with cvar `df_mode_fastcaps` (mode 3) or `df_mode_tricksmode` (mode 2) |
| `promode` | `0x2fa4` | digit, value 0..1 | `"%i"` |
| `kick` | `0x2fdf` | player lookup by name (`func_0x...` with `ClientNumberFromString` shape) → `"Unknown player."`; `pers.connected == 2` else `"Player not connected."` | display `clientkick %i` (`(ent - g_entities)/0x43c`), vote string `kick %s` (netname `client+0x200`) |
| `clientkick` | `0x303a` | digit, value 0..63 → else `"Client num out of range."`; `connected == 2` else `"Not a connected client."` | display `clientkick %i`, vote string `kick %s` |
| `interference` | `0x30b8` | digit `'0'..'3'` | display one of `"full interaction"` / `"interaction only with missiles"` / `"interaction except for missiles"` / `"no interaction"` (table at `0x9ac` region); value `"%i"` |
| `kill_OBs` | `0x3107` | digit, value 0..1 | `"%i"` |

`mode`'s semantics tie it to the earlier finding that DeFRaG modes are 1 = run, 2 = tricks (`df_mode_tricksmode`), 3 = fastcaps (`df_mode_fastcaps`) — this is the
strongest evidence yet for the "2 = tricks" naming (both cvar names are literal in the validator). `func_0x2da3` (help, called when `argv(1)` is empty) is decoded: prints `"print \"Available vote commands:^3\"` and then, for each table entry whose allow-cvar (`entry+8`) is non-zero, appends the command name formatted `"%-15.15s"` (three names per row), ending with `"^7\"
"`, sent to the caller. Not decoded: the exact range for `mode` beyond the observed `1..7` literals and the `gametype` name table contents (`0x8c0`).

## `CheckVote` (`func_0x33dc`, called from `G_RunFrame` region `0x1b57`) — DECODED
```c
void CheckVote( void ) {
    voters = level.numVotingClients;                       // 0x12ac64
    if ( *(int*)0x1317c0 == 0 ) return;                    // same dedicated/variant gate
    if ( level.voteTime == 0 ) return;                     // 0x130d58
    if ( voters <= 0 ) { SendServerCommand(-1, "print \"Vote failed.
\""); goto endVote; }
    yes = no = 0;
    for ( i = 0; i < level.maxclients; i++ )
        if ( client[i].pers.connected == CON_CONNECTED ) { v = client[i].+0x280; if ( v == 1 ) yes++; else if ( v == 2 ) no++; }
    if ( level.time >= level.voteTime ) {                  // deadline reached: non-voters leave the electorate
        for ( i = 0; i < level.maxclients; i++ )
            if ( client[i].pers.connected == CON_CONNECTED && client[i].+0x280 == 0 ) voters--;
        if ( voters <= 0 ) { level.voteTime = 0; trap_SetConfigstring( CS_VOTE_TIME /*8*/, "" ); return; }   // silent cancel
    }
    yesRatio = (float)yes / voters;  noRatio = (float)no / voters;
    pass = func_0x1b675( "df_vote_passVoteOnRatio" );      // float cvar value (wrapper inferred: same helper that reads df_vote_allowVotes)
    if ( !( yesRatio < pass ) ) {                          // early pass: no need to wait for the deadline
        SendServerCommand( -1, "print \"Vote passed.
\"" );
        trap_SendConsoleCommand( EXEC_APPEND /*2*/, va( "%s
", voteCommand /*0x130858*/ ) );
        goto endVote;
    }
    if ( !( noRatio < 1.0f - pass ) ) { SendServerCommand( -1, "print \"Vote failed.
\"" ); goto endVote; }   // early fail
    trap_SetConfigstring( CS_VOTE_YES /*10*/, va("%i", yes) );  trap_SetConfigstring( CS_VOTE_NO /*11*/, va("%i", no) );
    return;
  endVote:
    level.voteTime = 0;  trap_SetConfigstring( CS_VOTE_TIME, "" );
}
```
Differences from vanilla `CheckVote`: the vote can **pass or fail early** by ratio (`df_vote_passVoteOnRatio`, a fraction like 0.5) instead of
waiting `VOTE_TIME` (30 s) for a simple majority; at the deadline (`df_vote_voteDelay` seconds after the call) non-voters are excluded from the
denominator; `numVotingClients` excludes bots (`func_0x6cc`); no 3 s `voteExecuteTime` delay — the command is appended immediately; and the whole system is inactive
when `0x1317c0 == 0`. `0x130d58` (voteTime) and the yes/no counting use per-client `+0x280` (`1` = yes, `2` = no, `0` = hasn't voted).

## oDFs
Not ported: oDFs's `Cmd_CallVote_f` is vanilla (`g_allowVote`, vanilla vote list). A faithful port needs the 11 validators, `df_vote_*` cvar
registration, the unread configstring tail and `CheckVote`'s `df_vote_passVoteOnRatio`. Registering the `cv` alias for the existing command is
trivial but not done (it would deviate from the table).
