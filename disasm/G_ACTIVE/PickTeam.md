# PickTeam / TeamCount — confirmed (supersedes wrong "G_RunClient" identification)

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`

---

## Background — how these were found

The README previously guessed `0x1cbbd` was **`G_RunClient`** (a synchronous-client
per-frame runner). Investigating that guess to close it out found instead that
`0x1cbbd` is **`PickTeam`**, and its callee `0x1cb3b` is **`TeamCount`** — both
matching vanilla Q3's `g_utils.c` almost line-for-line. The `G_RunClient` guess was
wrong; the real `G_RunClient` has not been located (see
[README.md](README.md#clientthink--g_runclient-investigation)).

---

## TeamCount — `0x1cb3b`

**Called from:** `0x1be2b`, `0x1cbbd` (×2, see PickTeam below), `0x1e0fb` (×2)
**Args:** `arg0` = clientNum to ignore (int), `arg1` = team to count (int)
**Returns:** count (int)

```c
int TeamCount( int ignoreClientNum, int team ) {
    int i, count = 0;
    for ( i = 0; i < *(0x12a308) /* level.maxclients */; i++ ) {
        if ( i == ignoreClientNum ) continue;
        client = (gclient_t*)(0x12a2f0 + i * 0x37c);   // g_clients[i]
        if ( *(int*)(client + 0x1d4) == 0 ) continue;  // pers.connected == CON_DISCONNECTED
        if ( *(int*)(client + 0x284) != team ) continue; // sess.sessionTeam
        count++;
    }
    return count;
}
```

Byte-for-byte structural match to Q3's `g_utils.c` `TeamCount( ignoreClientNum, team )`.
Confirms/cross-validates:
- `0x12a2f0` = base of `g_clients` array (already known)
- `0x12a308` = `level.maxclients` (**new**)
- `0x37c` = `sizeof(gclient_t)` (already independently confirmed in `ClientThink_real.md`,
  `race_timer_system.md`, `func_0x1a44a.md` — this is a fourth independent confirmation)
- `client+0x1d4` = `pers.connected` (**new** — see [PickTeam.md](#pickteam--0x1cbbd) and
  correction in [Pmove_outer.md](Pmove_outer.md))
- `client+0x284` = `sess.sessionTeam` (already known)

---

## PickTeam — `0x1cbbd`

**Called from:** `0x1be2b`, `0x1ccdf`, `0x1e0fb`, `0x24765`
**Args:** `arg0` = clientNum to ignore (int)
**Returns:** `1` or `2`

```c
int PickTeam( int ignoreClientNum ) {
    int blue = TeamCount( ignoreClientNum, 2 );  // TEAM_BLUE
    int red  = TeamCount( ignoreClientNum, 1 );  // TEAM_RED
    if ( blue > red ) return 1;  // TEAM_RED
    if ( red > blue ) return 2;  // TEAM_BLUE
    // tie: pick team with lower score
    if ( *(0x12a324) > *(0x12a320) ) return 1;  // TEAM_RED
    return 2;                                    // TEAM_BLUE
}
```

Matches vanilla Q3's `PickTeam` exactly (counts, then tie-break on
`level.teamScores[TEAM_BLUE]` vs `level.teamScores[TEAM_RED]`).
`0x12a320`/`0x12a324` are very likely `level.teamScores[TEAM_RED]`/`[TEAM_BLUE]`
(order not independently confirmed beyond the comparison direction shown above) — team
balance is not part of movement physics, so this wasn't traced further.

Multiple callers (`0x1be2b`, `0x1ccdf`, `0x24765`) are consistent with a shared
autojoin/team-balance helper called from several places (join-team command, respawn,
possibly a vote/autobalance path) — none of these callers were decoded, as they're
outside the movement-logic scope of this investigation.
