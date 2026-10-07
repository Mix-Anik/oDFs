# G_UseTargets — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00028151`
**Size:** `0x153` = 339 instructions (vanilla: `0x9f` = 159 instructions — **more than double**)
**Hash:** `7be8a2b2` (vanilla: `-3613e102` — **mismatch, confirmed modified**)
**Called from:** cross-referenced via `baseq3-game-functions.hmap` walk; not separately
enumerated this pass (out of scope — this doc focuses on the function body itself).

**Identified as:** `G_UseTargets(gentity_t *ent, gentity_t *activator)` — confirmed via
exact argument signature, the `AddRemap`/`BuildShaderStateConfig` shader-remap
preamble, the `"WARNING: Entity used itself."` / `"entity was removed while using
targets"` literal strings, and the closing `G_Find`-driven `while` loop calling
`t->use(t, ent, activator)` — all byte-for-byte identical to vanilla except for
one large inserted block (see below).

---

## Structure

### 1. Shader-remap preamble (`0x28151`–`0x28197`) — vanilla-identical

```c
if ( !ent ) return;
if ( ent->targetShaderName && ent->targetShaderNewName ) {   // ent+0x294, ent+0x298
    float f = level.time * 0.001;
    AddRemap( ent->targetShaderName, ent->targetShaderNewName, f );   // call 0x27f17
    trap_SetConfigstring( CS_SHADERSTATE, BuildShaderStateConfig() ); // call 0x27f86, trap -0x13
}
```
Confirmed identical control flow, field offsets, constant `0.001` (`0x3a83126f`),
and call targets (`AddRemap`=`0x27f17`, `BuildShaderStateConfig`=`0x27f86`, both
independently hash-confirmed vanilla-identical — see module README).

### 2. `if (!ent->target) return;` (`0x28198`–`0x281a0`) — vanilla-identical

`ent->target` confirmed at offset **`0x288`** (string pointer, null-checked).

### 3. ⚠️ NEW — per-(entity, activator) once-per-frame de-duplication guard (`0x281a1`–`0x28253`)

**This entire block (~180 instructions) does not exist in vanilla.** It sits
between the `!ent->target` check and the vanilla `G_Find`/`while` loop, and
gates whether the loop runs at all.

```c
// once per server frame, reset this ent's per-activator "already fired" bitmask
if ( ent->timestamp_0x42c != level.time ) {      // ent+0x42c, level.time = *(0x12a310)
    ent->usedMaskHi_0x438 = 0;                   // ent+0x438 (only bit 0 used)
    ent->usedMaskB_0x434  = 0;                   // ent+0x434 (clientNum 32-63 bitmask)
    ent->usedMaskA_0x430  = 0;                   // ent+0x430 (clientNum 0-31 bitmask)
    ent->timestamp_0x42c  = level.time;
}

if ( activator->client == NULL ) {               // activator+0x204
    // non-client activator: single sticky "used" bit
    if ( ent->usedMaskHi_0x438 & 1 ) return;
    ent->usedMaskHi_0x438 |= 1;
} else {
    int clientNum = activator->client->field_0x8c;   // clientNum (per G_ACTIVE conventions)
    if ( clientNum < 0x20 ) {
        if ( ent->usedMaskA_0x430 & (1 << clientNum) ) return;
        ent->usedMaskA_0x430 |= (1 << clientNum);
    } else {
        if ( ent->usedMaskB_0x434 & (1 << (clientNum - 0x20)) ) return;
        ent->usedMaskB_0x434 |= (1 << (clientNum - 0x20));
    }
}
```

**Effect:** if the *same* `ent` (the entity whose `target` chain is being fired)
has already had `G_UseTargets` run for the *same activator* (same clientNum, or
the single non-client slot) during the *current server frame* (`level.time`
unchanged), the entire `G_Find`/`while(t->use(...))` loop below is skipped —
the target chain does not fire a second time this frame for that
entity+activator pair. `activator` with `clientNum >= 64` is not representable
(only two 32-bit words are used) — not observed as reachable given normal
`sv_maxclients` limits, noted for completeness.

This is a **confirmed, deliberate DeFRaG addition**, not a decompilation
artifact — it reads/writes four newly-identified `gentity_t` fields
(`+0x42c`, `+0x430`, `+0x434`, `+0x438`) that have no vanilla counterpart, and
the reset-then-check-then-set sequence is coherent, purposeful logic (not dead
code). Plausible motivation: prevents a `target`-holding entity from
re-firing its full target chain multiple times within one server frame when
touched by the same client more than once in that frame (e.g. multiple
overlapping trigger volumes, or DeFRaG's per-physics-substep touch-trigger
scanner in `g_active.c` (`func_0x1a1b2`) re-touching the same trigger across
Pmove subdivisions before `level.time` advances). Not confirmed against a
specific bug report — purpose inferred from mechanism only, not speculated
beyond what the bytecode shows.

### 4. `G_Find`/`while` loop (`0x28254`–`0x282a2`) — vanilla-identical

```c
t = NULL;
while ( (t = G_Find(t, FOFS(targetname)/*0x28c*/, ent->target)) != NULL ) {
    if ( t == ent ) {
        G_Printf("WARNING: Entity used itself.\n");
    } else if ( t->use ) {            // ent+0x2c4 == "use" function pointer
        t->use(t, ent, activator);
    }
    if ( !ent->inuse ) {              // ent+0x208
        G_Printf("entity was removed while using targets\n");
        return;
    }
}
```
`G_Find` call target = `0x2809c`, matches the independently hash-confirmed
`G_Find` address in this same module. `t->use` invoked with `(t, ent,
activator)` — same argument order as vanilla. `G_Printf` trap/const = `0x96`,
matching vanilla's `G_Printf` hmap entry exactly.

---

## New `gentity_t` field offsets confirmed (DeFRaG-only, no vanilla equivalent)

| Offset | Field | Notes |
|--------|-------|-------|
| `ent+0x42c` | last-fire `level.time` stamp | reset trigger for the three fields below |
| `ent+0x430` | "used this frame" bitmask, clientNum 0–31 |
| `ent+0x434` | "used this frame" bitmask, clientNum 32–63 |
| `ent+0x438` | bit 0 = "used this frame" flag for non-client activators |

## Other `gentity_t` field offsets confirmed/reconfirmed via this function

| Offset | Field |
|--------|-------|
| `ent+0x204` | `client` (gclient_t*) — matches `G_ACTIVE/README.md` |
| `ent+0x208` | `inuse` (qboolean) |
| `ent+0x288` | `target` (string pointer) |
| `ent+0x28c` | `targetname` (string pointer, `FOFS(targetname)` constant used as `G_Find` arg) |
| `ent+0x294` | `targetShaderName` |
| `ent+0x298` | `targetShaderNewName` |
| `ent+0x2c4` | `use` (function pointer, `(self, other, activator)`) — 4 bytes after `touch` (`ent+0x2c0`, per `G_ACTIVE/README.md`), matching vanilla's adjacent `touch`/`use` struct layout |

## oDFs comparison

**Implemented.** oDFs's `G_UseTargets` (`code/game/g_utils.c`) was the
unmodified vanilla version with no de-duplication guard. Added four new
`gentity_t` fields (`useTargetsTime`/`useTargetsMaskA`/`useTargetsMaskB`/
`useTargetsNonClient`, `g_local.h`) and the same reset-then-check-then-set
guard logic, using `activator - g_entities` as the clientNum-equivalent index
(oDFs doesn't need `activator->client`'s own clientNum field since player
entities' index in `g_entities[]` already equals their clientNum). This is
directly relevant to oDFs: `DeFRaG_TouchTriggers` (`func_0x1a1b2`'s
reimplementation) runs once per `Pmove` subdivision inside `ClientThink_real`,
so without this guard a race start/stop/checkpoint trigger touched across
multiple Pmove substeps within one server frame could fire its `use` chain
more than once before `level.time` advances.
