# `func_0x1c6be` — DeFRaG-extended `SelectRandomFurthestSpawnPoint`

**Address:** `0x1c6be`
**Stack frame:** `enter 0x254` (596 bytes — large, dominated by a 64-entry
sorted candidate list)
**Called from:** 5 sites total — `0x44bc`
([`O3J_SelectSpawnPoint_UseClosest`](O3J_SelectSpawnPoint_UseClosest.md)'s
own fallback, used when its classname-matching loop finds no winner),
`0x1a63a` (not examined this pass), `func_0x1c84b` (a trivial 3-arg
forwarding stub that always appends a literal `0` as the 4th arg — see
[`func_0x270ee.md`](func_0x270ee.md) — itself called from `func_0x270ee`
and `func_0x1c860`), and twice directly from `ClientSpawn` (`0x1d27b`).
**Args:** `(vec3_t avoidPoint, vec3_t outOrigin, vec3_t outAngles, int tag)`.

## Identity: near-exact structural + literal-string match to vanilla `SelectRandomFurthestSpawnPoint`

This function's shape is essentially identical, instruction-for-instruction
in its core logic, to vanilla Q3's `g_client.c:SelectRandomFurthestSpawnPoint`:

- Maintains a **64-entry** (`0x40`, confirmed by the loop's upper bound
  checks) sorted-by-distance candidate list via **insertion sort,
  descending** (new candidates are inserted at the first position where
  they exceed an existing entry, shifting the rest down — confirmed by
  tracing the exact `lef`/shift sequence).
- On total failure (nothing found across any classname — see below),
  falls back to a single, unfiltered `G_Find(NULL, FOFS(classname),
  "info_player_deathmatch")`, and if **that** also fails, prints the
  literal string **`"Couldn't find a spawn point"`** — vanilla's own
  `G_Error` message text, verbatim, found once in the whole QVM.
- Final selection: `idx = (int)( (rand() & 0x7fff) / 32767.0f * (numSpots / 2) )`
  — i.e. a uniformly-random pick among the **furthest half** of the sorted
  list — exactly vanilla's `rnd = random() * (numSpots/2)` idiom (`0x5dc2`
  = `rand()`, already project-confirmed in
  [`weapon_supershotgun_fire.md`](../G_WEAPON/weapon_supershotgun_fire.md)).
- Winner's origin/angles are copied out via the same `+0x5c`/`+0x74`
  field pair used throughout this session's other spawn-point functions,
  with the same `origin[2] += 9.0f` lift (`0x41100000` = `9.0f`) applied
  on **every** return path (both the sorted-list pick and the last-ditch
  fallback) — matching vanilla's universal "+9 to avoid spawning stuck in
  the floor" convention.

**DeFRaG's additions on top of vanilla:**

1. **Multiple classnames instead of one.** Vanilla's
   `SelectRandomFurthestSpawnPoint` only ever scans
   `"info_player_deathmatch"`. This version has an **outer loop over a
   6-entry classname table at `0x2374`**, confirmed by reading the actual
   string data:

   | index | string |
   |---|---|
   | 0 | `"info_player_deathmatch"` |
   | 1 | `"info_player_start"` |
   | 2 | `"team_CTF_blueplayer"` |
   | 3 | `"team_CTF_redplayer"` |
   | 4 | `"team_CTF_bluespawn"` |
   | 5 | `"team_CTF_redspawn"` |

   (Same 6 classnames [`O3J_SelectSpawnPoint_UseClosest`](O3J_SelectSpawnPoint_UseClosest.md)
   checks inline, just reached via a lookup table here instead of chained
   comparisons, and in a different enumeration order.) The outer loop
   tries classname `0`, and **only moves on to classname `1`, `2`, ... if
   classname `0` yielded zero viable (non-crowded, tag-matching)
   candidates** — it stops at the first classname that produces any
   results at all, it does not merge candidates across classnames.

2. **A per-entity tag filter.** Each candidate is additionally required
   to satisfy `candidate->+0x2f8 == tag` (the function's 4th arg) before
   being added to the sorted list — not present in vanilla. Observed
   `tag` values at call sites: `ent->client->+0x374` (from
   `O3J_SelectSpawnPoint_UseClosest`'s fallback call) and a literal `0`
   (from every call that routes through `func_0x1c84b`). `+0x2f8`'s real
   meaning is **not independently confirmed** — flagged, not guessed.

3. The usual crowding filter is the already-confirmed
   [`func_0x1c56f`](../G_COMBAT/func_0x1c56f.md) ("is another player near
   this spot"), reused for a 3rd independent confirmation this session
   (also used in `func_0x26fbf`/`func_0x1c860`).

## Decode

```c
gentity_t *func_0x1c6be( vec3_t avoidPoint, vec3_t outOrigin, vec3_t outAngles, int tag ) {
    float dists[0x40];     // local18-array
    gentity_t *spots[0x40]; // local118-array
    int numSpots = 0;      // local21c

    for ( int ci = 0; ci < 6; ci++ ) {
        const char *classname = classnameTable_0x2374[ci];
        gentity_t *spot = NULL;

        while ( (spot = G_Find(spot, FOFS(classname), classname)) != NULL ) {   // 0x2809c
            if ( func_0x1c56f(spot) != 0 )           continue;   // crowded, skip
            if ( spot->+0x2f8 != tag )                 continue;   // tag mismatch, skip

            vec3_t delta = { spot->+0x5c - avoidPoint[0],
                              spot->+0x60 - avoidPoint[1],
                              spot->+0x64 - avoidPoint[2] };
            float dist = VectorLength( delta );          // 0x8d63

            // insertion sort, DESCENDING, into dists[]/spots[], capped at 64 entries
            int i = 0;
            while ( i < numSpots && dist <= dists[i] ) i++;
            if ( i < 0x40 ) {
                // shift [i..numSpots) up by one, then write at i
                memmove(&dists[i+1], &dists[i], (numSpots-i)*4);
                memmove(&spots[i+1], &spots[i], (numSpots-i)*4);
                dists[i] = dist;
                spots[i] = spot;
            }
            numSpots++;   // (capped separately against 0x40 for the shift bound)
        }

        if ( numSpots != 0 )
            break;   // this classname produced results — stop trying further classnames
    }

    if ( numSpots == 0 ) {
        // ultimate fallback: ignore crowding/tag filters, classname table — just grab ANY
        // "info_player_deathmatch" entity
        gentity_t *spot = G_Find( NULL, FOFS(classname), "info_player_deathmatch" );
        if ( spot == NULL )
            G_Error_or_Printf( "Couldn't find a spawn point" );   // 0x00b1, confirmed literal string
        memcpy( outOrigin, &spot->+0x5c, 0xc );
        outOrigin[2] += 9.0f;
        memcpy( outAngles, &spot->+0x74, 0xc );
        return spot;
    }

    int idx = (int)( ( (rand() & 0x7fff) / 32767.0f ) * (numSpots / 2) );   // 0x5dc2 = rand()
    memcpy( outOrigin, &spots[idx]->+0x5c, 0xc );
    outOrigin[2] += 9.0f;
    memcpy( outAngles, &spots[idx]->+0x74, 0xc );
    return spots[idx];
}
```

## Confirmed building blocks (cited, not re-derived)

- `0x2809c` = `G_Find`, `0x1c56f` = "player nearby" check, `0x5dc2` =
  `rand()`, `0x8d63` = `VectorLength`-shaped — all already independently
  confirmed elsewhere in this project (cited above).
- `+0x5c`/`+0x74` output origin/angles field pair — same as
  `O3J_SelectSpawnPoint_UseClosest` and `func_0x270ee`'s winner-copy code.
- The literal string `"Couldn't find a spawn point"` at `0x809b`
  (confirmed via direct text search — occurs exactly once in the QVM) —
  vanilla's own exact `G_Error` text.

## Open questions (flagged, not guessed)

- `spot->+0x2f8` — the DeFRaG-added per-entity tag field this function
  filters candidates by. Not independently confirmed.
- `ent->client->+0x374` — the tag value `O3J_SelectSpawnPoint_UseClosest`
  passes when falling back to this function. Plausibly related to
  gametype/mode (DeFRaG's race mode has no "team", so `0` might mean
  "non-team/race mode" and `client+0x374` might hold an actual team
  number for CTF-aware servers) — not confirmed.
- `0x1a63a` = `ClientEvents` (EV_USE_ITEM1 teleporter), now decoded: [../G_ACTIVE/ClientEvents.md](../G_ACTIVE/ClientEvents.md).
- Exact semantics of "stop at the first classname with ANY result" vs.
  vanilla's single-classname design — not cross-checked against how
  DeFRaG actually uses this in practice (e.g. does normal FFA/race play
  ever reach classname index 1+?).

## oDFs implementation status

**Not implemented.** oDFs already has its own vanilla-derived
`SelectRandomFurthestSpawnPoint`-equivalent in `g_client.c` using only
`"info_player_deathmatch"` and no tag filter; the DeFRaG extensions found
here (multi-classname table, `+0x2f8` tag) would need the tag field's
real meaning pinned down before being safely portable. Documented for
completeness of the `ClientSpawn` spawn-point-dispatch picture — this
closes out that investigation's call graph (every function reached from
`ClientSpawn`'s spawn-point selection is now at least structurally
decoded; the extra caller `0x1a63a` is `ClientEvents`, see ../G_ACTIVE/ClientEvents.md).
