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
    if ( *(0x1b1ec) == 2 /*GT_SINGLE_PLAYER*/ && G_SpawnInt( "notsingle", "0" ) ) { G_FreeEntity(ent); return; }   // corrected: bytecode 0x24b5e `ne -> skip` means the check applies only when == 2, as in vanilla
    if ( G_SpawnInt( "notq3a", "0" ) )                        { G_FreeEntity(ent); return; }

    // --- DeFRaG addition: soft exclude — tags ent->flags bits for func_0x20e0
    // to evaluate later at level init, see Part 3. Internally this ALSO does
    // its own "notdefrag" hard-exclude check first. ---
    if ( !func_0x1c9c( ent ) ) return;   // notdefrag: returns WITHOUT freeing (bytecode 0x24b7e); then G_SpawnInt("notq3a") -> free; see "Part 2b" below for exactly what this sets

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
| `"gametype"` key present, value does **not** contain `"ffa"` | `0x300000` | **Corrected**: `0x5ce7` is `strstr(haystack, needle)` (returns the haystack pointer or 0; body read), and the code sets the bit when the result is **0** (`0x1dbb ne` skips when found). So a `gametype` list lacking "ffa" is excluded from modes 1/2 |
| `"gametype"` key present, value does **not** contain `"ctf"` | `0x400000` | same helper, same polarity |

## Part 3 — `func_0x20e0`: the level-init consumer sweep

Called once at level init (same place as `G_InitRaceMode`). Loops over
every `gentity_t` and frees it based on the bits above, cross-referenced
against already-confirmed mode-config globals:

```c
for each entity i in 64..1023:                       // loop var starts at 0x40, bound 0x400 (0x20e0-0x20ed, 0x21ac)
    if (!ent->inuse) continue;                       // ent+0x208
    // --- physics: df_promode cache (0x1317c4, = trap_Cvar_VariableIntegerValue("df_promode"), set at 0x1b70) ---
    if (promode == 1) { if (ent->flags & 0x20000 /*notcpm*/) { G_FreeEntity(ent); continue; } }
    else              { if (ent->flags & 0x10000 /*notvq3*/) { G_FreeEntity(ent); continue; } }
    // --- dedicated/variant flag (0x1317c0, see G_UTILS/func_0x21fc.md) ---
    if (flag0x1317c0 != 0) { if (ent->flags & 0x40000 /*notmp*/) { G_FreeEntity(ent); continue; } }
    else                   { if (ent->flags & 0x80000 /*notsp*/) { G_FreeEntity(ent); continue; } }
    // --- DeFRaG mode (0x1317bc: 1 = run, 2 = tricks?, 3 = fastcaps; see func_0x21fc.md) ---
    mode = *(0x1317bc);
    if      (mode == 1) { if (ent->flags & 0x100000 /*notdf*/)  G_FreeEntity(ent); }
    else if (mode == 2) { if (ent->flags & 0x200000 /*nottm*/)  G_FreeEntity(ent); }
    else if (mode == 3) {
        if      (ent->flags & 0x400000 /*notfc/notteam*/) G_FreeEntity(ent);
        else if (*(0x131838) == 0 && (ent->flags & 0x800000)) G_FreeEntity(ent);   // see below
    }
```
**Polarity resolved (later pass).** The first version of this pseudo-code (kept in git history of this file) read the
`ne` at `0x20fa` the wrong way round: bytecode `0x20f7 load 0x1317c4; const 1; ne -> 0x210c` jumps to the `notvq3`
(`0x10000`) block when promode **is not** 1, and falls through to the `notcpm` (`0x20000`) block when promode **is** 1;
and neither block `continue`s on "no flag" — both fall to the dedicated check at `0x211d`. With that read, every
key means exactly what its English name says (`notcpm` removes the entity under CPM physics, `notvq3` under VQ3, `notmp` on
the dedicated/variant path, `notsp` otherwise, `notdf`/`nottm`/`notfc` in the matching mode), so the earlier "backwards
polarity" worry no longer applies. Mode names: `df_mode_tricksmode` and `df_mode_fastcaps` are cvars in the string pool and the
auto-detect in `func_0x21fc` picks 1 = has a start timer, 3 = has a team item (CTF flags), 2 = otherwise, matching
`df` (run) / `tm` (tricks) / `fc` (fastcaps). Evidence for 1/2 naming is by that correspondence, not a string.

**`0x800000` RESOLVED (later pass):** it is a *mover* tag, not a spawn key: `SP_func_plat` (`0x23f38`), `SP_func_train` (`0x2437b`) and
`SP_func_bobbing` (`0x244f7`) each end with `ent->flags |= 0x800000` (the three occurrences of that constant besides the sweep).
`0x131838 == 0` means `df_ndm_nomover != 0` (see func_0x1e60.md). So in mode 3 (fastcaps) with `df_ndm_nomover` set, plats, trains and bobbing
movers are removed. (Other mover types — doors, rotating, etc. — are not tagged.) Original note, now obsolete: `0x800000` (checked only under `mode==3`) has **no corresponding key**
in the Part 2b table — none of the ~13 keys traced set that specific bit.
Either a key wasn't found in this pass, or it's set by a different code path
entirely (not traced). Flagged, not guessed at.

---

## Open questions (confirmed to exist, not resolved)

- **(RESOLVED — see Part 3) Bit-polarity direction read backwards from the key names' literal
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
- **(Mostly resolved — see Part 3 / func_0x21fc.md) What DeFRaG's `df_gametype` cache values `1`/`2`/`3` (`0x1317bc`)
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

## oDFs status

**Ported** (syntax-check clean, not run in a game): `FL_NOT_*` bits (`g_local.h`), `G_ParseModeKeys` (replaces vanilla's `notteam`/`notfree`/`gametype`
handling in `G_SpawnGEntityFromSpawnVars`, `g_spawn.c`; `notdefrag` leaves the entity allocated like the bytecode), `G_ApplyModeExclusion` (called
right after `G_SpawnEntitiesFromString` in `G_InitGame`), and a new `df_gametype` cvar (default 1, latched). Mode 1/2/3 comes from `df_gametype`
(values 5..7 = mode-4 + dedicated flag); **not ported**: DeFRaG's mode auto-detect for invalid values (`func_0x21fc`), (the `0x800000` bit is now resolved and ported as `FL_MOVER`: set at the end of `SP_func_plat`/`SP_func_train`/`SP_func_bobbing` in `g_mover.c`, checked in mode 3 against a new `df_ndm_nomover` cvar, default "0"),
and `func_0x24a42`/`func_0x24c8c`. Default behaviour is unchanged for maps without these keys (mode 1 = vanilla FFA-like handling of `notfree`).
