# `func_0x270ee` — CTF-spawn-point wrapper, `ClientSpawn`'s fallback-branch selector

**Address:** `0x270ee`
**Stack frame:** `enter 0x20`
**Called from:** `0x1d27b` (`ClientSpawn`) — the `else` branch of the
`VectorLength(client+0x2a4) != 0` gate documented in
[`O3J_SelectSpawnPoint_UseClosest.md`](O3J_SelectSpawnPoint_UseClosest.md)
(when that gate is *false*, `ClientSpawn` calls this function instead of
`O3J_SelectSpawnPoint_UseClosest`).
**Args:** `(int isbot, int team, vec3_t outOrigin, vec3_t outAngles)`.

## Decode

```c
gentity_t *func_0x270ee( int isbot, int team, vec3_t outOrigin, vec3_t outAngles ) {
    gentity_t *spot = func_0x26fbf( team, isbot );   // see func_0x26fbf.md
                                                        // (NOTE: arg order swapped
                                                        // relative to this function's
                                                        // own params — confirmed from
                                                        // the Arg[0x8]/Arg[0xc] push order)

    if ( spot != NULL ) {
        memcpy( outOrigin, &spot->+0x5c, 0xc );   // same output-field pair used by
        outOrigin[2] += 9.0f;                       // O3J_SelectSpawnPoint_UseClosest —
                                                        // identical Z-lift-by-9 idiom
        memcpy( outAngles, &spot->+0x74, 0xc );
        return spot;
    }

    // Fallback: no CTF-classed spot found at all
    return func_0x1c84b( &global_0x1524, outOrigin, outAngles );
}
```

### `func_0x1c84b` — trivial forwarding stub

`enter 0x1c`, called from `0x8ca`, `0x1c860`, and here. Its entire body is:

```c
gentity_t *func_0x1c84b( void *a, vec3_t b, vec3_t c ) {
    return func_0x1c6be( a, b, c, 0 );   // just forwards, appending a literal 0 arg
}
```

### `func_0x1c6be` — shared "last resort" selector — **RESOLVED**

**Now decoded**, see [`func_0x1c6be.md`](func_0x1c6be.md): it's a
DeFRaG-extended `SelectRandomFurthestSpawnPoint` (confirmed via the exact
vanilla `G_Error` string `"Couldn't find a spawn point"` plus an
essentially identical 64-entry descending-distance insertion sort and
`numSpots/2` random pick), extended to try 6 classnames instead of
vanilla's 1, with an added per-entity tag filter (`+0x2f8`, unconfirmed
meaning). Called from 5 sites total — `0x44bc`
(`O3J_SelectSpawnPoint_UseClosest`'s own fallback), `0x1a63a` (not
examined), `func_0x1c84b` (hence transitively from here and
`func_0x1c860`), and twice directly from `ClientSpawn`.

### `func_0x1c860` = `SelectInitialSpawnPoint` (RESOLVED)

Sole caller `0x1d27b` (ClientSpawn). Loop: `G_Find(spot, +0x20c classname, "info_player_deathmatch")` (`0x2809c`), stopping at the first spot with `spot+0x210 & 1`
(`+0x210` = `gentity_t.spawnflags`, between classname `+0x20c` and flags `+0x218`; bit 1 = "initial" spawn). Then `if (!spot || SpotWouldTelefrag(spot)=0x1c56f)`
fall back to `func_0x1c84b(vec3_origin=&0x1524, origin, angles)`; else copy `spot+0x5c` -> origin with `origin[2] += 9.0f` (`0x41100000`) and `spot+0x74` -> angles.
Matches vanilla / oDFs `SelectInitialSpawnPoint`; the only difference is that DeFRaG has no `FL_NO_BOTS`/`FL_NO_HUMANS` skip (not a DeFRaG-specific feature; nothing to port).

## Confirmed building blocks (cited)

- `func_0x26fbf` — see [`func_0x26fbf.md`](func_0x26fbf.md), decoded this
  same pass.
- The `+0x5c`/`+0x74` output-field pair and the `+9.0f` Z-lift — identical
  pattern to `O3J_SelectSpawnPoint_UseClosest`'s own winner-copy code,
  cross-confirming both.

## Open questions (flagged, not guessed)

- (closed) `func_0x1c860` = SelectInitialSpawnPoint, see above.
- `&global_0x1524`'s purpose when passed as `func_0x1c6be`'s `avoidPoint`
  arg (now that `func_0x1c6be` is decoded, this is a zero-initialized
  global being read as a `vec3_t` — plausibly just "no real avoid point
  available, use the origin" / effectively treated as `(0,0,0)`, not
  independently confirmed).
- `0x1a63a` = `ClientEvents` (teleporter), see [../G_ACTIVE/ClientEvents.md](../G_ACTIVE/ClientEvents.md).

## oDFs implementation status

**Not implemented** — depends on the same CTF classname/gametype system
gap already flagged in `func_0x26fbf.md`, plus the undecoded `0x1c6be`.
