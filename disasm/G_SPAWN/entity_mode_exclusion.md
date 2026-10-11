# DeFRaG's per-entity gametype/mode exclusion system

Resolves the "`func_0x1c9c`"/"`func_0x20e0`" threads left open in
[`../G_TRIGGER/func_0x1e60.md`](../G_TRIGGER/func_0x1e60.md). This is a
genuine, substantial, three-part DeFRaG feature: map entities can carry
boolean/string spawn keys that mark them as belonging only to certain
server configurations (physics mode, dedicated/listen, gametype, "no
movers" restriction), so a single `.bsp` can support several modes/gametypes
by simply hiding the entities that don't apply to whichever one is
currently running.

**Recognized, not guessed**: the key names found
(`notcpm`/`notvq3`/`notteam`/`notfree`/`notsingle`/`gametype`) are the same
convention used by CPMA for exactly this purpose (multi-gametype map
support via "not-applicable-here" entity keys) — a known, public fact about
that community's mapping convention, not an inference from this bytecode
alone. That context supports the *general purpose* read below with real
confidence, even though the *exact* bit-polarity and DeFRaG's own
mode-number meanings are not independently confirmed (see "Open questions").

---

## Part 1 — `G_SpawnEntitiesFromString` (`0x24d71`) dispatches to a new per-entity handler

```c
void G_SpawnEntitiesFromString( void ) {
    *(int*)0x12b488 = 1;     // "currently parsing spawn vars" state flag
    *(int*)0x12b48c = 0;     // spawn-vars key/value pair count, reset per entity

    if ( !G_ParseSpawnVars() )
        G_Printf( "SpawnEntities: no entities\n" );

    func_0x24c8c();          // handles the FIRST entity (worldspawn) — not traced this pass

    while ( G_ParseSpawnVars() ) {
        func_0x24b2a();      // handles EVERY SUBSEQUENT entity
    }

    *(int*)0x12b488 = 0;
}
```
Vanilla-shaped at the top level (parse-then-dispatch loop); the real
insertion is one level down, inside `func_0x24b2a`.

## Part 2 — `func_0x24b2a` = `G_SpawnGEntityFromSpawnVars`, with exclusion checks spliced in

Confirmed via its exact tail: after its own body, it calls the
already-identified, hash-confirmed-unmodified **`G_CallSpawn`** (`0x2493f`)
and frees the entity if that fails — precisely vanilla's
`if (!G_CallSpawn(ent)) G_FreeEntity(ent);` closing pattern. Full structure:

```c
void G_SpawnGEntityFromSpawnVars( void ) {   // func_0x24b2a
    gentity_t *ent = G_Spawn();              // 0x283de, confirmed elsewhere in this project

    // generic key/value field population — walks the spawn-vars table built
    // by G_ParseSpawnVars (base 0x12b490, stride 8, count *(0x12b48c)),
    // calling an ED_ParseField-equivalent helper (func_0x24a42, NOT
    // independently decoded this pass) once per {key, value} pair.
    for ( i = 0; i < *(int*)0x12b48c; i++ ) {
        pair = spawnVarsTable[i];
        func_0x24a42( pair.key, pair.value, ent );
    }

    // --- DeFRaG addition: hard excludes, checked before the entity is
    // allowed to exist at all ---
    if ( *(0x1b1ec) != 2 && G_SpawnInt( "notsingle", "0" ) ) { G_FreeEntity(ent); return; }
    if ( G_SpawnInt( "notq3a", "0" ) )                        { G_FreeEntity(ent); return; }

    // --- DeFRaG addition: soft exclude — tags ent->flags bits for func_0x20e0
    // to evaluate later at level init, see Part 3. Internally this ALSO does
    // its own "notdefrag" hard-exclude check first. ---
    func_0x1c9c( ent );    // see "Part 2b" below for exactly what this sets

    // (two VectorCopy-shaped block_copy calls here, ent+0x5c -> ent+0x18 and
    // ent+0x5c -> ent+0x1e8 — plausibly vanilla's own origin/bounds init,
    // not confirmed as a DeFRaG addition, not decoded further)

    if ( !G_CallSpawn( ent ) )   // 0x2493f, hash-confirmed unmodified
        G_FreeEntity( ent );
}
```

### Part 2b — `func_0x1c9c`: the key-name-to-flag-bit table

One argument (`ent`). Reads a sequence of spawn keys via the already-
identified `G_SpawnInt` (`0x248eb`) and a string-equality helper (`0x5ce7`
for the `"gametype"` checks), ORing bits into `ent->flags` (confirmed
offset `0x218`, cross-checked against this project's established
`gentity_t` layout):

| Spawn key | Bit(s) set in `ent->flags` | Notes |
|---|---|---|
| `notdefrag` | — (hard exclude) | if truthy, the function returns `0` immediately with **no** bit set; its caller is expected to free the entity for this case (not independently confirmed that `func_0x24b2a` actually checks this return value — not traced) |
| `notcpm` | `0x20000` | |
| `notvq3` | `0x10000` | |
| `notmp` | `0x40000` | |
| `notsp` | `0x80000` | |
| `notdf` | `0x100000` | |
| `nottm` | `0x200000` | |
| `notfc` | `0x400000` | |
| `notteam` | `0x400000` (same bit as `notfc`) | |
| `notfree` | `0x300000` (`0x100000`+`0x200000` together) | |
| `"gametype"` key value `"ffa"` | `0x300000` | via `0x5ce7` string-equality, not `G_SpawnInt` |
| `"gametype"` key value `"ctf"` | `0x400000` | same helper |

## Part 3 — `func_0x20e0`: the level-init consumer sweep

Called once at level init (same place as `G_InitRaceMode`). Loops over
every `gentity_t` and frees it based on the bits above, cross-referenced
against already-confirmed mode-config globals:

```c
for each entity:
    if (!ent->inuse) continue;
    if (*(0x1317c4) /* df_promode cache */ != 1) {
        if (ent->flags & 0x20000 /* notcpm */) G_FreeEntity(ent);
        continue;
    }
    if (ent->flags & 0x10000 /* notvq3 */) { G_FreeEntity(ent); continue; }
    if (*(0x1317c0) /* dedicated cache */ == 0) {
        if (ent->flags & 0x40000 /* notmp */) G_FreeEntity(ent);
        continue;
    }
    if (ent->flags & 0x80000 /* notsp */) { G_FreeEntity(ent); continue; }

    mode = *(0x1317bc);  // df_gametype cache
    if (mode == 1)      { if (ent->flags & 0x100000 /* notdf */)   G_FreeEntity(ent); }
    else if (mode == 2) { if (ent->flags & 0x200000 /* nottm */)   G_FreeEntity(ent); }
    else if (mode == 3) {
        if (ent->flags & 0x400000 /* notfc/notteam */) {
            G_FreeEntity(ent);
        } else if (*(0x131838) == 0 /* df_ndm_nomover enabled, see func_0x1e60.md */
                   && (ent->flags & 0x800000)) {
            G_FreeEntity(ent);
        }
    }
    // mode == anything else: no checks, entity always kept
```

Note `0x800000` (checked only under `mode==3`) has **no corresponding key**
in the Part 2b table — none of the ~13 keys traced set that specific bit.
Either a key wasn't found in this pass, or it's set by a different code path
entirely (not traced). Flagged, not guessed at.

---

## Open questions (confirmed to exist, not resolved)

- **Bit-polarity direction reads backwards from the key names' literal
  English.** E.g. `notcpm` (bit `0x20000`) causes removal when
  `df_promode != 1` — i.e. the entity *survives only when `df_promode == 1`*.
  Taken at face value, a mapper setting `"notcpm" "1"` to mean "don't show
  this in CPM mode" would get the opposite of what the key name suggests.
  Either: (a) `df_promode == 1` doesn't mean "CPM is active" the way this
  project has otherwise assumed, (b) the key names mean something subtly
  different from their English reading (e.g. "not[available other than
  in]cpm" collapsed to "notcpm"), or (c) something in this trace is still
  wrong. Not resolved — flagged for a future pass with fresh eyes, ideally
  cross-checked against a real DeFRaG map that actually uses these keys.
- **What DeFRaG's `df_gametype` cache values `1`/`2`/`3` (`0x1317bc`)
  actually correspond to** (human gametype names) is still not established
  anywhere in this project — this sweep is the third place that reads this
  same global without its meaning being pinned down.
- **`0x800000`'s setter** — not located.
- `func_0x24a42` (the per-key field-population helper, ED_ParseField
  equivalent) and `func_0x24c8c` (the first-entity/worldspawn handler) —
  not decoded this pass.
- Whether `func_0x24b2a` actually acts on `func_0x1c9c`'s `notdefrag`
  early-return (`0`) to free the entity, or whether that's handled
  differently — not traced (the call result isn't obviously consumed in
  what was read).

## Not implemented

Nothing from this system is ported to oDFs. The mechanism (three
cooperating functions, ~13 spawn keys, 8 `ent->flags` bits) is now
confirmed and documented in enough detail to implement *something*, but
the bit-polarity oddity above is a real, open, unresolved question — and
getting the polarity backwards would mean oDFs silently shows the *wrong*
set of entities (e.g. CTF flags visible in FFA, or vice versa) on any map
that uses these keys, which is worse than not supporting the feature at
all. Left for a future pass once the polarity question is settled.
