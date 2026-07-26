# G_BounceItem — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00021561` (vanilla baseq3 equivalent at `0x19c3d`)
**DeFRaG hash:** `de56300` — **identical to vanilla's hash** (`baseq3-game-functions.hmap`
line 502: `0x19c3d G_BounceItem de56300`). Per the project's hashing caveat (opcode-shape
only, not operand/immediate values), a hash match does **not** by itself prove the
constants are unchanged — this pass reads every instruction and every immediate
directly from the bytecode to confirm that independently.
**Args:** `ent, trace` (missile/item entity, `trace_t*`) — confirmed by `local 0x5c
= arg0`, `local 0x60 = arg1` usage throughout.
**Called from:** not confirmed in this pass (no trailing `; called from` annotation
observed within the read range); functionally this is `g_items.c`'s bounce-on-trace
handler, called from `G_RunItem`-equivalent code.

---

## Headline finding

**Fully instruction-by-instruction decoded, entry to `leave`, zero gaps.** Every
float/int literal matches oDFs's current (vanilla-based) `G_BounceItem` in
`code/game/g_items.c` **exactly** — no `df_promode` reference, no altered
constants, no reordered logic, no added/removed fields. This is a genuine,
confirmed-unmodified function (not just a hash-match assumption).

---

## Structure

### 1. `hitTime` computation (`0x21561`–`0x21574`)

```c
hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
```

Confirmed via the dec-comment:
`Q_ftol((float)(*(0x12a314)) + (float)((*(0x12a310) - *(0x12a314)) * (*(int*)((&arg1)+0x8))))`.

- `0x12a310` = `level.time` (already known, README).
- `0x12a314` = `level.previousTime` — **newly confirmed** (first solid identification
  of this global in the disasm project).
- `trace + 0x8` = `trace->fraction` — matches standard `trace_t` layout
  (`allsolid`@0x0, `startsolid`@0x4, `fraction`@0x8).

### 2. `BG_EvaluateTrajectoryDelta` call (`0x21575`–`0x21581`)

```c
BG_EvaluateTrajectoryDelta( &ent->s.pos /* ent+0xc */, hitTime, velocity /* local14 */ );
```

Call target `0x4f43`. **Newly confirmed identification**: `0x4f43` =
`BG_EvaluateTrajectoryDelta(trajectory_t*, int atTime, vec3_t result)`. (The
README previously only listed this address as "shares helper 0x4f43 with
G_MissileImpact", unidentified.)

### 3. Dot product + `VectorMA` reflection (`0x21582`–`0x215d7`)

```c
dot = velocity[0]*trace->plane.normal[0] + velocity[1]*trace->plane.normal[1]
    + velocity[2]*trace->plane.normal[2];
ent->s.pos.trDelta[0] = velocity[0] + trace->plane.normal[0] * (-2.0 * dot);
ent->s.pos.trDelta[1] = velocity[1] + trace->plane.normal[1] * (-2.0 * dot);
ent->s.pos.trDelta[2] = velocity[2] + trace->plane.normal[2] * (-2.0 * dot);
```

Confirms `trace->plane.normal` at `trace+0x18/0x1c/0x20` (matches standard
`cplane_t` layout after `endpos[3]`@0xc). Confirms `ent->s.pos.trDelta` at
`ent+0x24/0x28/0x2c` (consistent with `entityState_t.pos` — a `trajectory_t`,
9 ints/floats — starting at `ent+0xc`: `trType`@0xc, `trTime`@0x10,
`trDuration`@0x14, `trBase[3]`@0x18/0x1c/0x20, `trDelta[3]`@0x24/0x28/0x2c).

**Constant confirmed:** `-0x40000000` → two's-complement bit pattern `0xC0000000`
→ **`-2.0`**, matching vanilla's `VectorMA( velocity, -2*dot, trace->plane.normal,
ent->s.pos.trDelta )` exactly.

### 4. `physicsBounce` scaling (`0x215d8`–`0x2161a`)

```c
ent->s.pos.trDelta[0] *= ent->physicsBounce;   // ent+0x238
ent->s.pos.trDelta[1] *= ent->physicsBounce;
ent->s.pos.trDelta[2] *= ent->physicsBounce;
```

**Newly confirmed struct offset:** `gentity_t.physicsBounce` = `ent+0x238` (a
`float`). Matches vanilla/oDFs's `VectorScale( ent->s.pos.trDelta,
ent->physicsBounce, ent->s.pos.trDelta );` exactly — read directly from the
entity field, no hardcoded/altered constant.

### 5. "Check for stop" branch (`0x2161a`–`0x21676`)

```c
if ( trace->plane.normal[2] > 0.0 && ent->s.pos.trDelta[2] < 40.0 ) {
    trace->endpos[2] += 1.0;
    endpos[0] = (float)Q_ftol(endpos[0]);   // SnapVector, per-component
    endpos[1] = (float)Q_ftol(endpos[1]);
    endpos[2] = (float)Q_ftol(endpos[2]);
    G_SetOrigin( ent, trace->endpos );      // call 0x286ad
    ent->s.groundEntityNum = trace->entityNum;   // ent+0x94 = trace->entityNum (trace+0x34)
    return;
}
```

Branch semantics verified directly: `lef -2 0x21677` jumps to the **else**-path
(`0x21677`, below) when `normal[2] <= 0.0` is true; `gef -2 0x21677` jumps to
the else-path when `trDelta[2] >= 40.0` is true. Fallthrough (both conditions
false, i.e. `normal[2] > 0.0 && trDelta[2] < 40.0`) executes the stop block —
matching vanilla's `if ( trace->plane.normal[2] > 0 && ent->s.pos.trDelta[2] <
40 )` exactly, including the **`> 0.0`** (not `>= `) and **`< 40.0`**
comparisons and the literal `+1.0` nudge and `40.0` threshold
(`0x42200000` confirmed = 40.0).

**Newly confirmed identification:** call target `0x286ad` = `G_SetOrigin(gentity_t*,
vec3_t)`. **Newly confirmed struct offset:** `entityState_t.groundEntityNum` =
`ent+0x94` (consistent with standard `entityState_t` layout: `number`@0x0,
`eType`@0x4, `eFlags`@0x8, `pos`@0xc..0x30, `apos`@0x30..0x54, `time`@0x54,
`time2`@0x58, `origin[3]`@0x5c..0x68, `origin2[3]`@0x68..0x74, `angles[3]`@0x74..0x80,
`angles2[3]`@0x80..0x8c, `otherEntityNum`@0x8c, `otherEntityNum2`@0x90,
`groundEntityNum`@0x94 — and this chain independently cross-checks against the
already-confirmed `clientNum`@0xa8 from the README's `fire_*` `s.clientNum`
finding: `constantLight`@0x98, `loopSound`@0x9c, `modelindex`@0xa0,
`modelindex2`@0xa4, `clientNum`@0xa8 — all consistent).

### 6. Else path — normal move (`0x21677`–`0x216c2`)

```c
VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin );
VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
ent->s.pos.trTime = level.time;
return;
```

Confirms `ent->r.currentOrigin` at `ent+0x1e8/0x1ec/0x1f0` (already known from
prior docs) and `ent->s.pos.trBase` at `ent+0x18` (block_copy dest, 0xc bytes,
consistent with the `trajectory_t` layout established in section 3). Exact
match to vanilla.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x21561` | Function entry |
| `0x2157f` | `BG_EvaluateTrajectoryDelta(&ent->s.pos, hitTime, velocity)` call (`0x4f43`, newly confirmed) |
| `0x21620`, `0x21627` | `if (normal[2] > 0.0 && trDelta[2] < 40.0)` branch point (both `lef`/`gef` jump to else-path `0x21677` when false) |
| `0x21633` | `trace->endpos[2] += 1.0` |
| `0x21668` | `G_SetOrigin(ent, trace->endpos)` call (`0x286ad`, newly confirmed) |
| `0x21674` | `ent->s.groundEntityNum = trace->entityNum` (`ent+0x94 = trace+0x34`) |
| `0x216b9` | `VectorCopy(currentOrigin, trBase)` (`block_copy`, 0xc bytes) |
| `0x216c0` | `ent->s.pos.trTime = level.time` |
| `0x216c2` | `leave` (function end) |

**Confirmed absence:** no reference to the `df_promode` cache (`0x1317c4`) or
any other cvar-cache address anywhere in this function's instruction range.

---

## oDFs implementation notes

**No difference found — no action needed.** oDFs's `G_BounceItem`
(`code/game/g_items.c:912`) already matches every constant and every line of
logic confirmed above: the `-2*dot` reflection, `ent->physicsBounce` scaling,
the `> 0` / `< 40` stop-check with `+1.0` endpos nudge and
`G_SetOrigin`/`groundEntityNum` set, and the else-path `VectorAdd`/`VectorCopy`/
`trTime` update. This function is **byte-for-byte behaviorally identical**
between DeFRaG and vanilla Q3, confirmed independently of the hash match.
