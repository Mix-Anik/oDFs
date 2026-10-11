# `O3J_SelectSpawnPoint_UseClosest` — DeFRaG address `0x44bc`

> **UPDATE (later pass):** `client+0x2a4` is the `respawnpoint` command's saved point — see [respawnpoint.md](respawnpoint.md). Two polarity/detail errors in the decode below are corrected there: the early-out returns NULL when the point's length is **0** (not nonzero), the candidate filter skips entities that are **not** in use, and the output origin has **z + 9**. Ported to oDFs as `SelectClosestSpawnPoint`.

**Address:** `0x44bc`
**Stack frame:** `enter 0x60`
**Called from:** `0x1d27b` (`ClientSpawn` — see
[`ClientSpawn.md`](ClientSpawn.md) /
[`../G_ACTIVE/ClientThink_real.md`](../G_ACTIVE/ClientThink_real.md)),
**3 separate call sites** inside it (confirmed by direct line-level check
of each `const 0x44bc` / `call -1` pair, not just the caller-list comment):
`0x1d2ff`, `0x1d34c`, `0x1d37b`. All three pass the identical argument
pattern `(ent, &outOrigin, &outAngles)`.
**Args:** `(gentity_t *ent, vec3_t outOrigin, vec3_t outAngles)` — 3 args
used out of a declared 4-slot (`0x10`-byte) arg area.
**Identity confirmed by an embedded debug string** (the project's
strongest form of evidence — see below), not inferred from shape alone.

## Identity: confirmed via literal error string

At `0x45a8` the function pushes the literal string

```
"O3J_SelectSpawnPoint_UseClosest() couldn't find a spawnpoint"
```
(string pool address `0x46ed`, confirmed by a direct text search of the
full `qagame.dis` LIT dump — this string occurs exactly once in the whole
file) and passes it to an `:unknown function:` at `0x00b1` (1 arg, a
`const 1 -2 -> ...`-style error/print call — not independently identified
this pass, but the call shape, pop of its result, and placement right
after a "count stayed 0" check is exactly a `G_Error`/`Com_Printf`-style
diagnostic, consistent with vanilla's many `"Couldn't find a spawn point"`
-style self-naming error strings).

This single string **names the function itself** — `O3J_` is evidently a
DeFRaG contributor/feature-tag prefix (parallel to this project's other
confirmed `DF_`/`df_`-prefixed additions), and `_UseClosest` describes the
selection strategy below exactly. This is a stronger form of confirmation
than the structural/behavioral matching used for most other functions in
this project, and supersedes the previous (uninspected, guessed) label
`"GetCurrentCmd"` that had been attached to this address in
`ClientThink_real.md` §3 — that label was never checked against this
function's own body and is **wrong**; see the correction noted in that
file.

## Decode

```c
gentity_t *O3J_SelectSpawnPoint_UseClosest( gentity_t *ent, vec3_t outOrigin, vec3_t outAngles ) {
    gentity_t *best = NULL;      // local2c
    float      bestDist = 0.0f;  // local30 (as fallthrough "furthest-so-far" comparator)

    // Early-out: if VectorLength(ent->client->+0x2a4) != 0.0f, bail immediately
    // returning NULL. (+0x2a4 here is the SAME client field read as the
    // caller's own "velocity/reference-point" gate just before calling this
    // function — see ClientThink_real.md §3. Reading it again inside this
    // function's own prologue and bailing if nonzero is a defensive
    // double-check, not independently explained further.)
    if ( VectorLength( &ent->client->+0x2a4 ) != 0.0f )
        return NULL;

    // Walk g_entities starting at index 64 (i.e. &g_entities[64] == 0x2c1f0
    // — see "0x2c1f0 is NOT a separate table" below) through num_entities,
    // skipping all client slots entirely (spawn points are never clients).
    for ( gentity_t *cand = &g_entities[64]; cand_index < level.num_entities; cand++ ) {
        if ( cand->+0x208 != 0 )      // "skip" flag — purpose still unconfirmed
            continue;

        // classname match chain — each call is Q_stricmp-shaped
        // (0x5c8f, confirmed elsewhere this project as Q_stricmp-shaped —
        // see func_0x2830.md), returning 0 on match:
        if ( !strcmp(cand->classname, "info_player_start")      ) goto candidate_found;
        if ( !strcmp(cand->classname, "info_player_deathmatch") ) goto candidate_found;
        if ( !strcmp(cand->classname, "team_CTF_redplayer")     ) goto candidate_found;
        if ( !strcmp(cand->classname, "team_CTF_blueplayer")    ) goto candidate_found;
        if ( !strcmp(cand->classname, "team_CTF_redspawn")      ) goto candidate_found;
        if ( !strcmp(cand->classname, "team_CTF_bluespawn")     ) goto candidate_found;
        continue;  // no classname matched -> next candidate

      candidate_found:
        // distance = VectorLength( (ent->client->+0x2a4/+0x2a8/+0x2ac) - cand->r.currentOrigin )
        //   i.e. distance from the SAME reference point (ent->client+0x2a4
        //   vec3) to this candidate's origin (+0x1e8/+0x1ec/+0x1f0).
        vec3_t delta = {
            ent->client-> +0x2a4 - cand-> +0x1e8,
            ent->client-> +0x2a8 - cand-> +0x1ec,
            ent->client-> +0x2ac - cand-> +0x1f0,
        };
        dist = VectorLength( delta );   // func_0x8d63 — see "0x8d63" below

        if ( best == NULL || dist < bestDist ) {   // "closest" — ge-float skip, not "furthest"
            best = cand;
            bestDist = dist;
        }
    }

    if ( best == NULL ) {
        G_Error_or_Printf( "O3J_SelectSpawnPoint_UseClosest() couldn't find a spawnpoint" );
        return NULL;
    }

    memcpy( outOrigin, &best->+0x5c, 0xc );   // best->s.origin2 / best->movedir-shaped field — see below
    memcpy( outAngles, &best->+0x74, 0xc );   // only when best->+0x74-based vec3 present
    if ( best->+0x40 /* flag? */ != NULL )
        /* extra per-candidate call at 0x45e3, func_0x1c6be, 1 arg
           = ent->client->+0x374 — not decoded this pass */;

    return best;
}
```

### `0x8d63` — a `VectorLength`-shaped helper

Called 3 times in this function, always with a single argument that is the
*address* of a 3-float (12-byte) block (never dereferenced further by the
caller before the call): once on `ent->client+0x2a4` directly (the entry
guard), and once on a locally-computed delta vector (`&local1c`, built from
3 `subf` float subtractions immediately before the call). Shape (1 address
arg in, 1 float out, no other side effects observed) is consistent with
`VectorLength`, but this was **not independently confirmed** against a
known-`VectorLength` call site elsewhere in this project this pass — flagged
as a structural match only, not asserted with the same confidence as the
string-confirmed function identity above.

### `0x5c8f` — reused, already-confirmed `Q_stricmp`-shaped helper

Same function already identified (structurally, via its 0-on-match return
convention) in [`func_0x2830.md`](func_0x2830.md)'s classname-matching
block. Used here identically: `cand->+0x20c` (= `classname`, already a
project-confirmed field) compared against 6 literal classname strings in
sequence.

### `+0x5c`/`+0x74` — candidate output fields (new, not independently confirmed)

The two 12-byte `memcpy`s pulling the winning candidate's output vectors
read from `best+0x5c` and `best+0x74` — **not** the already-known
`+0x1e8`/`r.currentOrigin` field used for the distance computation above.
This means the function copies out a *different* pair of vec3 fields than
the ones it measured distance against. Plausibly `s.origin2`/`s.angles` or
similar alternate position/orientation fields set at spawn-entity parse
time (vanilla's `info_player_start`/`info_player_deathmatch` entities carry
both a origin and an `angles` key, stored separately from the generic
`r.currentOrigin`/`s.angles` used for live entities) — not independently
confirmed, flagged rather than asserted.

## `0x2c1f0` is **NOT** a separate table — it is `&g_entities[64]`

**This corrects an arithmetic error in [`func_0x2830.md`](func_0x2830.md).**
That doc computed `0x2c1f0 - 0x1b2f0 = 0x11100` and concluded the result
wasn't a whole multiple of `0x43c` (`sizeof(gentity_t)`), so `0x2c1f0` must
be a second, separate 1024-slot table. **The subtraction was wrong.**
Redone carefully (hex, digit by digit):

```
  0x2C1F0
- 0x1B2F0
---------
  0x10F00
```

`0x10F00 / 0x43C = 64` exactly (`0x43C * 64 = 0x10F00`, verified by
multiplication, not just division). So `0x2c1f0 == &g_entities[64]` —
**the 65th slot of the exact same `g_entities` array**, not a separate
table at all.

This is directly confirmed by this function's own loop-bound check at
`0x45a1`–`0x45a3`:

```c
if ( (local18 - 0x1b2f0) / 0x43c < *(int*)0x12a2fc )   // level.num_entities (plausible identity,
    goto next_candidate;                                 // not independently re-derived this pass)
```

i.e. the loop computes the **entity index** of the current pointer
(relative to `g_entities`' own base, `0x1b2f0`) and continues while that
index is below `num_entities` — the textbook vanilla idiom for
`for (i = start; i < globals.num_entities; i++)`, just starting at a
hardcoded `64` instead of `0` (skipping all `MAX_CLIENTS` client slots,
since spawn-point entities are never clients). There is no second table:
`func_0x2830`'s "shadow table" is simply `g_entities[64..1023]`, the exact
same array this project already knows well, entered at a different start
index. **`func_0x2830.md`'s "What's confirmed" section, "Not confirmed"
section, and "oDFs implementation status" section have been updated
accordingly** — the shadow-table framing there is retracted; the field
offsets/dispatch logic documented there are otherwise unaffected (they
still apply to whichever `g_entities[64..]` slot is being inspected).

## Call-site context inside `ClientSpawn` (`0x1d27b`)

All 3 call sites share the same immediate guard:

```c
if ( VectorLength( client->+0x2a4 ) != 0.0f ) {
    call O3J_SelectSpawnPoint_UseClosest( ent, &origin, &angles );
} else {
    call func_0x270ee( pmtype, 0, &origin, &angles );   // fallback selector, not decoded this pass
}
```

(`client->+0x284` is read into a local and remapped `1→2`/`2→1` just
before this, matching `ClientThink_real.md` §3's existing — and still
accurate — description of that remapping; only the identity/args of the
`0x44bc` call itself were wrong there, not the surrounding condition.)

This means: if the client has a nonzero reference vector at `client+0x2a4`
(role/meaning of that field itself still not independently confirmed —
see `ClientBegin.md`'s team-reset block, which touches the same offset),
`ClientSpawn` picks the closest-matching spawn-type entity to that
reference point; otherwise it falls through to a different, undecoded
selector (`func_0x270ee`).

## Open questions (flagged, not guessed)

- `ent->client+0x2a4`/`+0x2a8`/`+0x2ac`'s real identity (used both as the
  "is there a reference point" gate and as the distance-computation origin
  here, and as part of the team-change reset block in `ClientBegin.md`).
- `best->+0x5c`/`+0x74` (the two output vec3 fields) vs. the already-known
  `+0x1e8`/`r.currentOrigin` used for the distance metric — why two
  different position-shaped fields on the same entity.
- `cand->+0x208` ("skip" flag) — same unconfirmed field already flagged in
  `func_0x2830.md`.
- `func_0x270ee` (the `else`-branch fallback selector) — **now decoded**,
  see [`func_0x270ee.md`](func_0x270ee.md) and
  [`func_0x26fbf.md`](func_0x26fbf.md) (a CTF flag/player/spawn-classname
  picker it wraps). `func_0x1c6be` (the small extra per-winner call at
  `0x45e3`, run as *this* function's own fallback when its own
  classname-matching loop finds nothing) is **also now decoded** — see
  [`func_0x1c6be.md`](func_0x1c6be.md): a DeFRaG-extended
  `SelectRandomFurthestSpawnPoint` (confirmed via vanilla's exact
  `"Couldn't find a spawn point"` error string). This closes out the
  entire `ClientSpawn` spawn-point call graph found this session (every
  reached function is at least structurally decoded; `0x1a63a`, a
  separate caller of `func_0x1c6be`, is `ClientEvents` — see ../G_ACTIVE/ClientEvents.md).
- The exact diagnostic-print function at `0x00b1`.

## oDFs implementation status

**Not implemented.** oDFs already has its own `SelectSpawnPoint`-family
logic (vanilla-derived, in `g_client.c`) with no "closest-to-reference"
selection mode; porting this would require first identifying what
`client+0x2a4` actually represents (the gating field this entire call
chain hinges on), which is still open. Documented for a future pass.
