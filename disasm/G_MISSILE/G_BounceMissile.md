# G_BounceMissile — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00021f4f` (vanilla baseq3 equivalent at `0x1a3b8`)
**DeFRaG hash:** `5fcd4e2b` — **identical to vanilla's hash** (`baseq3-game-functions.hmap`
line 523: `0x1a3b8 G_BounceMissile 5fcd4e2b`). Per the project's hashing caveat, a
hash match only proves identical instruction *shape*, not identical
constants/behavior — this pass reads every instruction to verify independently,
and **found a genuine discrepancy** (see headline finding).
**Args:** `ent, trace` (missile entity, `trace_t*`) — confirmed by `local 0x44 =
arg0`, `local 0x48 = arg1` usage throughout.
**Called from:** `0x20998` (per the disassembly's trailing `; called from
0x20998` annotation) and `G_MissileImpact` (`0x22139`, confirmed elsewhere).

---

## Headline finding — CONFIRMED DIFFERENCE

**DeFRaG's compiled `G_BounceMissile` is missing the `ent->s.time = level.time /
4;` assignment** that vanilla's `EF_BOUNCE_HALF` early-stop path contains. This
was found by reading every instruction from `enter` to `leave` with zero gaps —
not inferred from a hash mismatch (the hash is identical) or from missing
context.

Vanilla / oDFs's current `code/game/g_missile.c` (lines 44-52):

```c
if ( ent->s.eFlags & EF_BOUNCE_HALF ) {
    VectorScale( ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta );
    // check for stop
    if ( trace->plane.normal[2] > 0.2 && VectorLength( ent->s.pos.trDelta ) < 40 ) {
        G_SetOrigin( ent, trace->endpos );
        ent->s.time = level.time / 4;      // <-- THIS LINE
        return;
    }
}
```

DeFRaG's decoded bytecode for the exact same branch (`0x2200d`–`0x22019`):

```
Arg[0x8] = ent
Arg[0xc] = &trace->endpos
call 0x286ad          ; G_SetOrigin(ent, trace->endpos)  -- confirmed present
const 0x22064
jump                   ; unconditional jump straight to the function epilogue (return)
```

There is **no instruction anywhere between the `G_SetOrigin` call and the
`return` jump** that writes to `ent->s.time` (`ent+0x54`, per the confirmed
`entityState_t` layout — see [G_BounceItem.md](G_BounceItem.md)). The whole
function (every instruction from `0x21f4f` to `0x22065`) was read; no
`level.time / 4` (or equivalent shift/divide-by-4) computation exists
anywhere else in the function either. This is a confirmed bytecode fact, not
an inference.

**Effect:** any bouncing missile that hits a near-flat-enough surface
(`normal[2] > 0.2`) slow enough (`trDelta` length `< 40`) to trigger the
"stop" path (this is the `EF_BOUNCE_HALF` grenade-launcher case — vanilla
rockets/plasma/BFG don't set `EF_BOUNCE_HALF` so they never reach this branch)
never gets its `entityState_t.time` field set to `level.time/4` in DeFRaG. In
vanilla Q3, `entityState_t.time` is a general-purpose per-entity-type field
(its interpretation depends on `eType`); for a stopped grenade this looks like
a vanilla effect-timing/state field that DeFRaG's compiled game code simply
never sets. No downstream consumer of this specific field was investigated in
this pass (out of scope — the instruction-level omission itself is the
confirmed finding).

---

## Structure (everything else — confirmed identical to vanilla)

### 1. `hitTime` + `BG_EvaluateTrajectoryDelta` + dot/`VectorMA` reflection (`0x21f4f`–`0x21fc5`)

Byte-for-byte identical in shape and constants to `G_BounceItem`'s
corresponding section (same `0x12a310`/`0x12a314` globals, same `call 0x4f43`
= `BG_EvaluateTrajectoryDelta`, same `-2.0` reflection constant
`-0x40000000`→`0xC0000000`). See [G_BounceItem.md](G_BounceItem.md) for the
full breakdown; not re-derived here since it's the same code shape.

```c
hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
dot = DotProduct( velocity, trace->plane.normal );
VectorMA( velocity, -2*dot, trace->plane.normal, ent->s.pos.trDelta );
```

### 2. `EF_BOUNCE_HALF` gate (`0x21fc6`–`0x21fce`)

```c
if ( ent->s.eFlags & EF_BOUNCE_HALF ) {   // ent+0x8 & 0x20
```

`eq -2 0x2201a` jumps to the else-path (`0x2201a`, section 4 below) when
`(eFlags & 0x20) == 0` is true; fallthrough executes the `EF_BOUNCE_HALF`
block. `0x20` = `EF_BOUNCE_HALF`, confirmed against oDFs's
`code/game/bg_public.h:286` (`#define EF_BOUNCE_HALF 0x00000020`).

### 3. `VectorScale(0.65)` + stop-check (`0x21fcf`–`0x22019`)

```c
ent->s.pos.trDelta[0] *= 0.65;    // literal 0x3f266666, confirmed = 0.65
ent->s.pos.trDelta[1] *= 0.65;
ent->s.pos.trDelta[2] *= 0.65;

if ( trace->plane.normal[2] > 0.2 &&              // literal 0x3e4ccccd, confirmed = 0.2
     VectorLength( ent->s.pos.trDelta ) < 40.0 ) { // literal 0x42200000, confirmed = 40.0; call 0x8d63 = VectorLength (already known)
    G_SetOrigin( ent, trace->endpos );             // call 0x286ad, confirmed
    return;                                         // *** ent->s.time = level.time/4 is MISSING here ***
}
```

Both `lef -2 0x2201a` (normal[2]<=0.2 → skip) and `gef -2 0x2201a`
(length>=40.0 → skip) branch to the else-path (section 4) when false,
matching vanilla's `&&` short-circuit structure exactly. All three constants
(`0.65`, `0.2`, `40.0`) confirmed unconditionally vanilla-identical — the only
delta in this entire function is the missing `s.time` write documented above.

### 4. Else path — normal move (`0x2201a`–`0x22063`)

```c
VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin );
VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
ent->s.pos.trTime = level.time;
return;
```

Identical in shape and offsets to `G_BounceItem`'s else-path (same
`ent+0x1e8/0x1ec/0x1f0` = `r.currentOrigin`, `ent+0x18` = `s.pos.trBase`,
`ent+0x10` = `s.pos.trTime`). Confirmed exact match, no delta.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x21f4f` | Function entry |
| `0x21f6d` | `BG_EvaluateTrajectoryDelta(&ent->s.pos, hitTime, velocity)` call (`0x4f43`) |
| `0x21fc6`–`0x21fce` | `if (ent->s.eFlags & EF_BOUNCE_HALF)` gate (`0x20`) |
| `0x21fd5`–`0x21ff8` | `VectorScale(trDelta, 0.65, trDelta)` (`0x3f266666`) |
| `0x21ffe`–`0x2200c` | `if (normal[2] > 0.2 && VectorLength(trDelta) < 40.0)` (`0x3e4ccccd`, `0x42200000`) |
| `0x22015` | `G_SetOrigin(ent, trace->endpos)` call (`0x286ad`) |
| `0x22018`–`0x22019` | **Unconditional jump straight to `return` — no `s.time` write in between (CONFIRMED MISSING vs vanilla)** |
| `0x2201a`–`0x22063` | Else path: `VectorAdd`/`VectorCopy`/`trTime = level.time` (identical to `G_BounceItem`'s else path) |
| `0x22064` | Function epilogue (single shared return point) |

**Confirmed absence:** no reference to the `df_promode` cache (`0x1317c4`) or
any other cvar-cache address anywhere in this function's instruction range.

---

## oDFs implementation notes

**Do not change oDFs's `G_BounceMissile` based on this finding without a
decision from the reviewer** (per task scope, this is documentation only). For
the record: oDFs's current `code/game/g_missile.c:33-57` implements the full
vanilla behavior *including* `ent->s.time = level.time / 4;` — meaning oDFs
currently does **not** match DeFRaG's actual compiled behavior in this one
spot. Whether to "fix" oDFs to match DeFRaG (by removing the line) or leave it
vanilla-correct is a product decision, not a bytecode-accuracy question — this
doc only records what DeFRaG's binary actually does.

All other constants and logic (0.65 scale, 0.2/40.0 stop-check thresholds,
`-2.0` reflection factor, `G_SetOrigin`/`VectorAdd`/`VectorCopy`/`trTime`
else-path) are confirmed **unconditionally vanilla-identical** — no
`df_promode` or other DeFRaG-specific gating exists anywhere in this function.
