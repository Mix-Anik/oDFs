# `func_0x26fbf` — CTF-flag/player/spawn-classname spawn-point picker

**Address:** `0x26fbf`
**Stack frame:** `enter 0xdc`
**Called from:** `0x270ee` (`func_0x270ee`, the only caller — see
[`func_0x270ee.md`](func_0x270ee.md)).
**Args:** `(int isbot, int team)` — `team`: `1` = red, `2` = blue (any
other value → return `NULL` immediately).

Structurally matches vanilla Q3's `SelectCTFSpawnPoint` closely enough to
be confident of the overall role, but **no embedded debug string confirms
this exact name** (unlike
[`O3J_SelectSpawnPoint_UseClosest`](O3J_SelectSpawnPoint_UseClosest.md)),
so this doc keeps the address-based name per the NO SPECULATION rule — the
behavioral identification below is solid, the *name* is not independently
proven.

## Decode

```c
gentity_t *func_0x26fbf( int isbot, int team ) {
    gentity_t *flag = NULL;        // local18

    // Step 1: locate this team's flag (used only as a distance reference below)
    if ( team == 1 )
        flag = G_Find( NULL, FOFS(classname), "team_CTF_redflag" );   // 0x2809c, project-confirmed G_Find
    else if ( team == 2 )
        flag = G_Find( NULL, FOFS(classname), "team_CTF_blueflag" );

    // Step 2: choose the classname to scan for
    const char *classname;
    if ( isbot ) {
        if      ( team == 1 ) classname = "team_CTF_redplayer";
        else if ( team == 2 ) classname = "team_CTF_blueplayer";
        else return NULL;
    } else {
        if      ( team == 1 ) classname = "team_CTF_redspawn";
        else if ( team == 2 ) classname = "team_CTF_bluespawn";
        else return NULL;
    }

    // Step 3: scan up to 32 matching entities, filtering out "crowded" spots
    gentity_t *candidates[0x20];   // local30, 32 slots
    int   count    = 0;            // localc0
    int   bestIdx  = -1;           // localb4
    float bestDist;                // local1c (re-set only when bestIdx is assigned)
    gentity_t *spot = NULL;        // local14

    do {
        spot = G_Find( spot, FOFS(classname), classname );
        if ( spot == NULL )
            break;

        if ( func_0x1c56f( spot ) == 0 ) {   // "is another player near this spot" —
                                                // already project-confirmed in
                                                // ../G_COMBAT/func_0x1c56f.md
            candidates[count] = spot;
            if ( flag != NULL ) {
                vec3_t delta = { flag->+0x1e8 - spot->+0x1e8,
                                  flag->+0x1ec - spot->+0x1ec,
                                  flag->+0x1f0 - spot->+0x1f0 };
                float dist = VectorLength( delta );     // 0x8d63, structural match only
                if ( bestIdx < 0 || dist < bestDist ) {
                    bestDist = dist;
                    bestIdx  = count;
                }
            }
            count++;
        }
    } while ( count != 0x20 );

    // Step 4: if literally nothing of this classname exists at all, bail
    if ( count == 0 && /* G_Find never returned non-NULL even once */ )
        return NULL;

    // Step 5: pick a winner
    if ( bestIdx >= 0 )
        return candidates[ bestIdx ];           // closest to this team's flag
    else
        return candidates[ rand() % count ];    // 0x5dc2, project-confirmed `rand()`
                                                   // (see ../G_WEAPON/weapon_supershotgun_fire.md)
}
```

### Confirmed building blocks (cited, not re-derived)

- `0x2809c` = `G_Find`, confirmed unmodified elsewhere in this project
  ([`../G_UTILS/README.md`](../G_UTILS/README.md)).
- `0x1c56f` = "is another player near this position" boolean check,
  already fully decoded in
  [`../G_COMBAT/func_0x1c56f.md`](../G_COMBAT/func_0x1c56f.md). This call
  site is a second, independent confirmation of that function's role
  (a telefrag/crowding-avoidance filter during spawn-point scans).
- `0x5dc2` = `rand()`, confirmed in
  [`../G_WEAPON/weapon_supershotgun_fire.md`](../G_WEAPON/weapon_supershotgun_fire.md).
- `+0x1e8`/`+0x1ec`/`+0x1f0` = `r.currentOrigin`, established throughout
  this project.

### Not independently confirmed this pass (flagged)

- `0x8d63`'s exact identity as `VectorLength` — structural match only
  (single address-of-vec3 argument in, float out), consistent with every
  other use of `0x8d63` found this session
  ([`O3J_SelectSpawnPoint_UseClosest.md`](O3J_SelectSpawnPoint_UseClosest.md)),
  but never cross-checked against an independently-named `VectorLength`
  call site.
- The exact "did G_Find ever return non-NULL" bail condition at step 4 —
  read from the disassembly's control flow shape, not re-verified
  instruction-by-instruction against every edge case (e.g. the
  "all candidates crowded, but classname did exist" case).

## oDFs implementation status

**Not implemented.** oDFs has no CTF gametype/flag-classname system to
hang this off of; porting would also require resolving the `isbot`/`team`
argument identities (plausible from context, not independently confirmed)
first. Documented for completeness of the `ClientSpawn` spawn-point-dispatch
picture; see [`func_0x270ee.md`](func_0x270ee.md) for how it's reached.
