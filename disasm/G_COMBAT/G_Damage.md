# G_Damage — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x0002015b` (vanilla baseq3 equivalent at `0x18b18`, hash `-72712e52`)
**DeFRaG hash:** `29fad3dd` — **MODIFIED** (does not match vanilla; confirmed via
`baseq3-game-functions.hmap` cross-reference)
**Size:** `0x4ee` (1262) instructions in DeFRaG vs `0x3b9` (953) in vanilla — **+309
instructions (~32% larger)**
**Args:** `targ, inflictor, attacker, dir, point, damage, dflags, mod` (identical to
vanilla signature; confirmed by argument slot layout `local 0x7c..0x98`)

---

## Headline finding

DeFRaG's `G_Damage` contains a **`df_promode`-gated branch that is not present in
vanilla Q3**. When `df_promode` (global cvar cache `0x1317c4`) is non-zero **and**
the damage is self-inflicted rocket splash (`targ == attacker && mod ==
MOD_ROCKET_SPLASH(7)`), the horizontal components of the knockback `dir` vector
(`dir[0]`, `dir[1]`) are multiplied by **1.2** before the standard knockback
formula runs. The vertical component (`dir[2]`) is **not** scaled.

This directly confirms the empirically-observed "CPM self-splash rocket jumps are
stronger than vanilla math predicts" — the game applies an explicit, deliberate
1.2x horizontal multiplier in this exact case. It also explains why this
multiplier previously seemed invisible: vanilla's `G_RadiusDamage`/`G_Damage`
math has no such term, so nothing in the un-reverse-engineered oDFs reference
source (which is vanilla id code) hinted at it.

The multiplier is applied to `dir` **after** `VectorNormalize(dir)` has already
run (confirmed — normalize happens earlier in the function, at `0x20250`), so it
is not cancelled by any later re-normalization; it directly scales the magnitude
of the resulting knockback velocity kick on the X/Y plane.

---

## Re-examination pass (searching for an additional ~11.7% knockback-speed term)

A follow-up investigation re-examined this function (plus `G_RadiusDamage`,
`fire_rocket`, and — newly — `G_ExplodeMissile`/`G_MissileImpact`) looking for
a second, previously-missed multiplier that could explain an observed ~11.7%
knockback-*speed* shortfall (~25% displacement shortfall) between the confirmed
1.2x-horizontal formula and a live-server measurement. Summary of what was
re-checked and the result — **no additional multiplier was found**:

- **`scale` (promode path, `0x2027e`-`0x20283`)** — re-read instruction by
  instruction across the full `0x2027a`-`0x20364` range. Confirmed
  unconditionally `1.0` for every `mod` value; no branch, cvar, or stat field
  ever assigns it anything else.
- **`func_0xa2ea`** — fully decoded; it is a generic `clamp(min,max,value)`
  used only for the `pm_time` duration clamp, not velocity. No effect on
  knockback magnitude.
- **Global `0x132660`** — fully traced across all 8 reference sites in the
  module; it is a debug/developer flags bitmask (gates an entity-validation
  loop, an alternate missile trace path, and debug-print logging). No
  arithmetic on damage/knockback/velocity found at any site.
- **`func_0x3ec7` (`g_knockback` getter)** — re-decoded. Correction found (see
  below) regarding *why* it returns 1000.0, but the returned numeric value for
  the specific live-test scenario is unchanged (still 1000).
- **`func_0x1b675`** — fully decoded: plain `atof(Cvar_VariableStringBuffer(name))`,
  no scaling.
- **`fire_rocket`** — re-read in full; `damage`/`splashDamage`/`splashRadius`
  are set once, unconditionally, *before* the `df_promode` speed branch, and
  are never touched again afterward (verified through the function's `return`).
- **`G_RadiusDamage`'s radius parameter** — traced back to its actual caller,
  `G_ExplodeMissile` (see [G_RadiusDamage.md](G_RadiusDamage.md) and the new
  [G_ExplodeMissile.md](../G_MISSILE/G_ExplodeMissile.md)): the radius argument
  passed in is `ent->splashRadius` read directly from the missile entity
  (confirmed `120`, matching `fire_rocket`'s unconditional set), with **no**
  promode-conditional variant or adjustment anywhere in between.
- **`G_MissileImpact`'s direct-hit `G_Damage` call** — decoded; passes
  `mod = MOD_ROCKET (6)`, not `MOD_ROCKET_SPLASH (7)`, so the 1.2x self-splash
  multiplier structurally cannot apply to it regardless; `damage = 100`
  unconditional, no promode branch found anywhere in `G_MissileImpact`.
- **`G_RunMissile`** — grepped for the `df_promode` cache address (`0x1317c4`)
  across its full instruction range; zero references.

**Conclusion: within `G_Damage`, `G_RadiusDamage`, `fire_rocket`,
`G_ExplodeMissile`, and `G_MissileImpact`, the confirmed 1.2x-horizontal-only
self-splash knockback formula (with `mass=200`, live/cheat-gated
`g_knockback`, and vanilla-identical radius-falloff) is the complete formula.
No second multiplier, additive term, or alternate code path was found that
would explain the reported ~11.7% knockback-speed gap.** If the gap is real,
it is not located in server-side damage/knockback application — it must be
either in client-side prediction/physics consumption of the resulting
velocity (already independently audited per the task brief) or in a
game-side function outside this set entirely (not investigated this pass).

---

## Third re-examination pass (searching for an ADDITIVE ~+10 term to `knockback`)

A further follow-up (4 independent live-server rocket-jump self-splash shots,
damage 44–97, distance 3–67 units, widely varying angles) found the
*percentage* velocity shortfall vs. the confirmed 1.2x-horizontal formula
varies too much (9.7%–20%) to be a missing multiplicative factor (already
exhaustively ruled out above), but the *additive* amount that `knockback`
itself would need to gain, before the final `g_knockback*knockback/mass`
scale, to explain each shot converges tightly: **+10.55, +11.31, +9.44,
+9.81** (average **+10.28**) across all four shots. This strongly suggested a
flat constant close to `10.0` (`0x41200000`) being added to `knockback`
somewhere in the promode self-splash path, not previously decoded at
instruction-level in this range.

**Method:** re-read every single instruction in `qagame.dis` from `0x20250`
(`VectorNormalize(dir)` call) through `0x20361` (`pm_flags |= 0x40`)
line-by-line — i.e. the *entire* `knockback`-computation-through-kvel-loop
range, not just the promode self-splash sub-block — with no reliance on the
pre-existing pseudocode summary above. Confirmed local-variable assignment:
`local24` = `knockback`, `local40` = `mass`, `local44` = `scale`, `local54` =
reused twice (first `1.2` literal for the dir-scale multiply, then
overwritten by `g_knockback_value()`'s return for the kvel formula).

**Result: NO `addf`/`addi` instruction touches `local24` (`knockback`)
anywhere in this range.** Specifically:
- `0x2025a`–`0x20260`: `knockback = min(damage, 200)` — only `lei`/`store4`,
  no add.
- `0x20263`–`0x20276` (`FL_NO_KNOCKBACK`/`DAMAGE_NO_KNOCKBACK` zeroing): only
  conditional `store4 0x0`, no add.
- `0x2027a`–`0x2027d`: `if (knockback == 0) goto merge` — comparison only.
- `0x2027e`–`0x20283`: `mass = 200.0`, `scale = 1.0` — plain `store4`, no add.
- `0x20284`–`0x2028c`: self-splash condition (`targ==attacker && mod==7`) —
  pure comparison/branch, no arithmetic.
- `0x2028d`–`0x2028f`: **only write in the entire self-splash block** —
  `local54 = 0x3f99999a` (exactly `1.2f`), confirmed by direct hex-to-IEEE754
  decode. No other constant (`10.0`=`0x41200000`, `9.0`=`0x41100000`,
  `11.0`=`0x41300000`, `10.5`=`0x41280000`, or any other) appears anywhere in
  this instruction range — grepped for all of these encodings, zero matches.
- `0x20290`–`0x202ab`: `dir[0] *= local54(1.2)`, `dir[1] *= local54(1.2)` —
  confirmed `mulf`, not `addf`; `local24` not referenced at all in this
  sub-block.
- `0x202ac`–`0x202af`: `local54 = g_knockback_value()` (call `0x3ec7`,
  overwriting the `1.2` slot) — plain call+store, no add.
- `0x202b0`–`0x202f1`: the three `kvel[i] = dir[i] * (scale * g_kb *
  (float)knockback / mass)` computations — `local24` is `load4`'d and
  `cvif`'d (int→float) but never has `addf`/`addi` applied to it before or
  after the cast, for any of the 3 axes.
- `0x20302`–`0x2032e`: `velocity[i] += kvel[i]` — the only `addf`
  instructions in the whole range (`0x20309`, `0x2031b`, `0x2032d`), and all
  three operate on `ps.velocity[i]`/`kvel[i]`, never on `local24`.
- `0x2033d`–`0x20342`: `knockback * 2` (via `lsh 0x1`, i.e. `<<1`, not an
  `addf`) for the `pm_time` clamp argument — `local24` read directly, no
  addition applied first.

**`knockback` (`local24`) is read from exactly one stack slot everywhere in
this function** — set once at `0x2025a`, optionally zeroed at `0x2026a`/
`0x20274`, and read as-is at `0x202b9`, `0x202d1`, `0x202e9`, and `0x2033d`.
There is no shadow/copy variable and no point at which the promode path reads
a different, separately-modified slot.

**Fallback check — `G_RadiusDamage`'s `points` computation, searching for a
MOD-conditional additive bonus (as opposed to the multiplicative
distance-falloff formula and promode-radius-source question already answered
above):** re-read `G_RadiusDamage` (`0x20773`) instruction-by-instruction
across its entity loop and `points` computation, `0x207c8`–`0x208e0`. `points`
(`local50`) is computed once, at `0x2086d`–`0x20878`, as `damage * (1.0 -
dist/radius)` — a single `mulf`/`subf`/`divf` sequence with **no `addf`
anywhere near it**. The only other `addf` in the whole function is at
`0x208c1`–`0x20878`'s `dir[2] += 24.0` bias (already documented,
unconditional, unrelated to `points`/`damage`). `mod` (`arg6`) is not even
loaded until `0x208db`, immediately before being passed as `Arg[0x24]` to the
`G_Damage` call at `0x208de` — it is never compared, branched on, or used in
any arithmetic anywhere inside `G_RadiusDamage` itself. **Confirmed: no
MOD-conditional additive term exists in `G_RadiusDamage`.**

**Conclusion of this pass: the ~+10 additive knockback term implied by the
live-shot data is CONFIRMED ABSENT from `G_Damage`'s entire knockback-to-kvel
instruction range (`0x20250`–`0x20361`, both promode and non-promode paths)
and from `G_RadiusDamage`'s `points`/`damage` computation.** No `addf`/`addi`
touching `knockback`/`damage`/`points`, no additional IEEE-754 literal near
`9.0`–`11.0`, and no shadow-copy variable were found anywhere in either
function. If the ~10.28-unit-average gap observed across the 4 live shots is
real, its source is not present in the server-side QVM code covered by this
document and `G_RadiusDamage.md` — it must lie either in a part of the
pipeline not yet decoded (e.g. further upstream of `G_RadiusDamage`/
`G_ExplodeMissile`, or in client-side prediction/physics code consuming the
resulting velocity, both out of scope for this pass) or the empirical
methodology producing the "implied additive bonus" figures warrants
independent re-verification (not performed here — out of scope; this pass was
scoped strictly to the disassembly).

---

## Structure

### 1. Setup / entity-pointer sanity check (`0x2015b`–`0x201d8`)

```
if (targ->something(+0x2e0) == 0): jump to CanDamage tail (0x20646) — early-out
if (0x12c694 == 0): jump to CanDamage tail                          — unknown global, early-out
if (inflictor == NULL): inflictor = 0x129a78                        — default/"world" entity ptr
if (attacker == NULL):  attacker  = 0x129a78

if (0x132660 & 0x2):   // unknown global flag
    local38 = targ
    idx = (local38 - g_entities_base(0x1b2f0)) / sizeof(gentity_t)(0x43c)
    if (idx < 0x40):
        ... entity-list scan/sanity-check loop (0x20183-0x201d8) ...
        if check fails: jump to CanDamage tail (bail out)
```

This is an entity-pointer validation block guarded by global flag `0x132660`
bit `0x2`. **RESOLVED this pass** (previously unknown): `0x132660` was traced
across all 8 of its reference sites in the module
(`0x3686, 0x43c4, 0x464e, 0x1aa55, 0x1c570, 0x1d5bb, 0x20179, 0x2196b`). None of
them is a write — the global is never `store4`'d anywhere in the QVM's code, so
it is populated by the engine directly (consistent with it being a
cheat/developer-style flags bitmask set up once at load, similar in spirit to
`0x1317c4`'s cvar-cache pattern, but never observed being written from
game-side bytecode in this module). Its bits gate:
- bit `0x2` — this entity-pointer validation loop in `G_Damage`, **and** an
  alternate `trap_Trace`-based tracing path inside a helper called from
  `G_RunMissile` (function at `0x43c3`) — i.e. bit `0x2` swaps in a different
  (presumably debug) collision-trace call.
- bit `0x1` — gates (a) a large-stack-frame (`0x1048` bytes) function at
  `0x1c56f` that is a no-op returning `0`/false when the bit is clear (called
  from 4 sites, shape consistent with a debug-print/log formatter that's
  compiled in but inert unless the bit is set), and (b) two similar bit-gated
  blocks inside missile-entity-type checks (`0x1d5bb`, `0x2196b`) whose
  gated bodies were not further decoded (no `store4` to any velocity/damage/
  knockback-related field was found in the immediately following instructions
  at either site).
- `0x3686` and `0x1aa55` pass the raw value as a plain function argument
  (not band-tested at the call site itself).

**No arithmetic operation on damage, knockback, dir, or velocity was found at
any of the 8 sites.** All observed effects are either validation/bail-out
branches, an alternate trace call, or debug-print gating. This global almost
certainly represents a developer/debug flags bitmask (plausibly cheat-gated
like `0x132100`/`sv_cheats`, though this was not independently confirmed) that
is `0` in any normal live game and has **no effect on knockback magnitude**.
Ruled out as a source of the reported knockback discrepancy. Not present in
vanilla `G_Damage`. Not implemented in oDFs (no confirmed gameplay effect).

### 2. Damage-scale-by-attacker-stat block (`0x201e9`–`0x20230`)

```
if (attacker->client != NULL && attacker != targ):
    scale = attacker->client_ptr + 0xd0        ; unidentified per-client field
    damage = damage * scale / 100
```

Another DeFRaG-only addition: scales incoming `damage` by a percentage stored at
`gclient_t + 0xd0` (offset not in the confirmed struct table — **unknown
field**), but **only when `attacker != targ`**, i.e. it is skipped entirely for
self-damage. This means rocket-jump self-splash damage is **not** affected by
this multiplier — noted for completeness, out of scope for the knockback
question. Not implemented in oDFs (unknown field, no confirmed purpose — left
unmodified per CRITICAL RULE).

### 3. `dir` normalization / DAMAGE_NO_KNOCKBACK setup (`0x20230`–`0x20263`)

```
if (dir == NULL):
    dflags |= DAMAGE_NO_KNOCKBACK (4)
else:
    VectorNormalize(dir)          ; call 0x9d12, confirmed VectorNormalize

knockback = min(damage, 200)      ; confirmed: local24 = damage; if (damage > 200) damage = 200
```

Exact match to vanilla.

### 4. Knockback zeroing flags (`0x20263`–`0x20276`)

```
if (targ->flags(+0x218) & 0x800):        knockback = 0   ; FL_NO_KNOCKBACK (0x800) — confirmed value matches oDFs g_local.h
if (dflags & 0x4):                        knockback = 0   ; DAMAGE_NO_KNOCKBACK
```

Exact match to vanilla (`FL_NO_KNOCKBACK` = `0x800` confirmed identical to
oDFs `g_local.h`).

### 5. `df_promode` branch point (`0x20276`–`0x20279`)

```
if (df_promode_cvar_cache(0x1317c4) == 0):
    goto 0x20365   ; ------ NON-PROMODE PATH (section 7) ------
; else fall through to PROMODE PATH (section 6)
```

### 6. PROMODE knockback path (`0x2027a`–`0x20364`)

```
if (knockback == 0): goto 0x2041f          ; skip (merge point after both branches)
mass = 200.0
scale = 1.0                                 ; general-purpose multiplier, always 1.0 here

if (targ == attacker && mod == MOD_ROCKET_SPLASH(7)):
    dir[0] *= 1.2
    dir[1] *= 1.2
    ; dir[2] is NOT scaled

; RE-VERIFIED this pass: `scale` (local44, initialized to 1.0 at 0x20281-0x20283)
; is written exactly ONCE in the entire promode path, by that single `const 1
; 0x3f800000` immediate store. Instructions 0x2027e-0x20364 were re-read one by
; one; there is no branch on `mod`, no cvar read, no stat-field load, and no
; other write to local44 anywhere in this range before it is consumed in the
; kvel loop below. `scale` is unconditionally 1.0 for every value of `mod`,
; not just the instance originally traced. CONFIRMED, not just observed once.

for each axis i in {0,1,2}:
    kvel[i] = dir[i] * ( scale * g_knockback_value() * (float)knockback / mass )
    targ->client->ps.velocity[i] += kvel[i]

if (targ->client->ps.pm_time == 0):
    t = clamp(knockback * 2, 50.0, 200.0)     ; via helper func_0xa2ea(min=50.0, max=200.0, value)
    targ->client->ps.pm_time = (int)t
    targ->client->ps.pm_flags |= 0x40          ; PMF_TIME_KNOCKBACK equivalent bit (DeFRaG bit value; vanilla uses 0x8 — internal enum reordering, not a behavior difference as long as PM_* code checks the same bit consistently)

goto 0x2041f
```

`g_knockback_value()` = call to `func_0x3ec7` (see below) — confirmed to be a
live read of the `"g_knockback"` cvar's current value **only when `sv_cheats` is
non-zero**; otherwise a hardcoded `1000.0` (see corrected `func_0x3ec7` section
below — this is a correction to what was previously assumed). Called fresh once
per axis (3 calls total), each returning the same value.

**`mass = 200.0`** — confirmed hardcoded literal (`0x43480000`), identical to
vanilla.

### 7. NON-PROMODE knockback path (`0x20365`–`0x2041e`) — reached only when `df_promode == 0`

```
if (knockback == 0): goto 0x2041f
if (targ->client == NULL): goto 0x2041f
mass = 200.0
for each axis i in {0,1,2}:
    g_kb = g_knockback_value()                          ; func_0x3ec7, called fresh per axis
    kvel[i] = dir[i] * ( g_kb * (float)knockback / mass )
    targ->client->ps.velocity[i] += kvel[i]

if (targ->client->ps.pm_time == 0):
    t = knockback * 2
    if (t < 50): t = 50
    if (t > 200): t = 200
    targ->client->ps.pm_time = t
    targ->client->ps.pm_flags |= 0x40
```

This is **byte-for-byte equivalent to vanilla's knockback formula**
(`kvel = dir * g_knockback.value * min(damage,200) / 200`), with **no** dir
scaling. Confirms: outside of promode, DeFRaG's self-splash knockback matches
vanilla exactly.

### 8. Merge point / rest of function (`0x2041f` onward)

Not fully decoded — continues into armor/health application, event broadcasting,
etc. (equivalent to the rest of vanilla `G_Damage` past the knockback block).
Out of scope for this analysis (task only required the knockback formula).

---

## `func_0x3ec7` — g_knockback cvar getter (RE-EXAMINED — condition corrected)

```
0x00003ec7  enter 0x2c
if (*(0x132100) == 0):
    return 1000.0              ; fallback default (matches vanilla g_knockback default "1000")
else:
    build string "g_knockback" byte-by-byte into local buffer
    value = <call func_0x1b675>("g_knockback")     ; = atof(Cvar_VariableStringBuffer("g_knockback")) — see below
    <call -101>(&"g_knockback", 0, 0x10)             ; discarded (pop -1), does not affect return value
    return value
```

Confirmed via the literal ASCII bytes `'g'(0x67) '_'(0x5f) 'k'(0x6b) 'n'(0x6e)
'o'(0x6f) 'c'(0x63) 'k'(0x6b) 'b'(0x62) 'a'(0x61) 'c'(0x63) 'k'(0x6b) '\0'`
constructed on the stack and passed as the cvar name.

**CORRECTION (this pass):** the previous doc assumed `*(0x132100) == 0` meant
"cvar not yet cached/initialized." This is **wrong**. `0x132100` was traced to
its origin: it is written exactly once in the whole module, by a small helper
function at `0x3d7e` (confirmed via cross-reference at
`0x3d7e`–`0x3db6`) which builds the literal ASCII string `"sv_cheats"`
(bytes confirmed: `s,v,_,c,h,e,a,t,s`), fetches it via trap `-7`
(`G_CVAR_VARIABLE_INTEGER_VALUE`-class call), and stores the result into
`0x132100`. `0x132100` is also read directly (no re-fetch) by a trivial
one-line wrapper at `0x3e0a` (`enter 0x8; return *(0x132100)`) that is called
from **7 separate sites** (`0x23f2, 0x35ce, 0x38ba, 0x468e, 0x489b, 0x1d955,
0x1f426`) — a call pattern consistent with scattered cheat-protection checks
(`give`, `god`, `noclip`, etc.), which confirms `0x132100` = the cached
**`sv_cheats`** integer, not a "first call" flag.

**Corrected behavior of `func_0x3ec7`:**

```c
if ( !sv_cheats.integer )
    return 1000.0;                       // hardcoded, ignores the live g_knockback cvar entirely
else
    return atof( Cvar_VariableStringBuffer("g_knockback") );  // live cvar value
```

In other words: **on any server with `sv_cheats 0` (i.e. essentially every live
public DeFRaG server, including race servers), DeFRaG's `G_Damage` ignores
whatever `g_knockback` is actually set to and forces `1000.0`.** Only with
`sv_cheats` enabled does it honor a customized `g_knockback` value.

This does **not** explain the reported 11.7% knockback-magnitude gap: the live
test server's `g_knockback` cvar was independently confirmed to be `1000` via
console log, which is numerically identical to the hardcoded fallback either
way — so this correction changes the *semantics* (why the value is 1000) but
not the *value itself* (still 1000) for that specific test. It is nonetheless
an important correction for the general case (any oDFs server admin who sets a
non-default `g_knockback` with `sv_cheats 0` would see DeFRaG silently ignore
it and use 1000 — oDFs currently does not replicate this cheat-gating and
would honor the custom value). Not implemented in oDFs pending a decision on
whether to replicate the cheat-gating (out of scope for this pass — flagged
for follow-up).

### `func_0x1b675` — generic "cvar as float" helper (RESOLVED)

```
0x0001b675  enter 0x98
Cvar_VariableStringBuffer(arg0 /* name */, &local14, 0x80)   ; trap -8
return atof(&local14)                                         ; call func_0x5dd7 (presumed atof, based on
                                                                ; string-buffer-in/float-out signature)
```

Fully decoded: this is a **generic** "fetch cvar `name` as a float" helper —
`Cvar_VariableStringBuffer` into an 0x80-byte stack buffer, then a string→float
conversion call (`func_0x5dd7`, not independently verified as `atof` beyond its
call signature, but behaviorally consistent). **No scaling, clamping, or
per-cvar special-casing found** — it is a plain passthrough of whatever the
named cvar's string value parses to. Confirms `g_knockback`'s live value (when
`sv_cheats` is on) is the raw, unmodified cvar value.

## `func_0xa2ea` — clamp helper (FULLY DECODED this pass)

```c
float func_0xa2ea(float min, float max, float value) {
    if (value >= min) {
        if (value <= max)
            return value;
        return max;
    }
    return min;
}
```

Confirmed by full instruction-level read (`0xa2ea`-`0xa303`): a plain 3-argument
generic `clamp(min, max, value)`, no globals touched, no side effects. Matches
its only call site's already-documented usage
(`func_0xa2ea(50.0, 200.0, knockback*2)` for the promode-path `pm_time` clamp).
Confirmed **not** relevant to knockback magnitude — it only ever clamps
`pm_time`, a duration in milliseconds, never a velocity component.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x2015b` | Function entry (DeFRaG `G_Damage`) |
| `0x20250` | `VectorNormalize(dir)` call (`0x9d12`) |
| `0x20256`–`0x20260` | `knockback = min(damage, 200)` |
| `0x20263`–`0x2026d` | `FL_NO_KNOCKBACK` (targ->flags & 0x800) zeroes knockback |
| `0x2026d`–`0x20276` | `DAMAGE_NO_KNOCKBACK` (dflags & 0x4) zeroes knockback |
| `0x20276`–`0x20279` | **`df_promode` branch point** (`0x1317c4`) |
| `0x2027e`–`0x20283` | `mass = 200.0`, `scale = 1.0` (promode path) |
| `0x20284`–`0x202ab` | **Self-splash check**: `targ==attacker && mod==MOD_ROCKET_SPLASH(7)` → `dir[0]*=1.2`, `dir[1]*=1.2` |
| `0x202ac`–`0x20338` | Promode-path kvel computation + velocity add (3 axes) |
| `0x2033a`–`0x20364` | Promode-path `pm_time`/`pm_flags` set via `func_0xa2ea` clamp helper |
| `0x20365` | Non-promode path entry (vanilla-identical formula) |
| `0x203a3`–`0x2041e` | Non-promode kvel computation + velocity add + pm_time set (inlined clamp) |
| `0x3ec7` | `g_knockback` cvar-value getter — returns hardcoded `1000.0` unless `sv_cheats` is non-zero, in which case it returns the live cvar value (CORRECTED this pass — see `func_0x3ec7` section above) |
| `0xa2ea` | `clamp(min, max, value)` — **fully decoded, confirmed** this pass (see below); generic 3-arg helper, no side effects |
| `0x1317c4` | `df_promode` cvar cache (confirmed, matches README) |
| `0x132660` | Debug/developer flags bitmask — **fully traced (8 sites), resolved** this pass (see section 1 above); no effect on knockback |
| `0x132100` | **NEW this pass** — cached `sv_cheats` integer (confirmed via cvar name string bytes at its writer, `0x3d7e`) |
| `gclient_t + 0xd0` | Unknown per-client field; used as a `damage *= field/100` scale for non-self damage — **unconfirmed purpose** |

---

## oDFs implementation notes

**Confirmed difference — FIXED.** oDFs's `G_Damage` (`code/game/g_combat.c`) was
pure vanilla: no `df_promode` branch, no self-splash multiplier. Added:

```c
if ( df_promode.integer && targ == attacker && mod == MOD_ROCKET_SPLASH ) {
    dir[0] *= 1.2f;
    dir[1] *= 1.2f;
}
```

placed immediately before the existing `VectorScale (dir, g_knockback.value *
(float)knockback / mass, kvel);` line, matching the disassembled order (scale
`dir` in place before it feeds into the shared knockback formula). `mass = 200`
and the `g_knockback.value` cvar read were already correct in oDFs and required
no change.

**Not implemented (deliberately, per CRITICAL RULE):**
- The entity-pointer validation block gated by global `0x132660` (section 1) —
  purpose unconfirmed, likely a defensive/debug check with no gameplay effect.
- The `gclient_t+0xd0` damage-scale-by-attacker-stat block (section 2) — field
  identity unconfirmed; also does not apply to self-damage so it's irrelevant to
  the rocket-jump knockback question regardless.
- The `pm_flags |= 0x40` value: oDFs's `bg_public.h` already defines
  `PMF_TIME_KNOCKBACK` as `64` (`0x40`), which **matches DeFRaG exactly** (not
  vanilla Q3's `0x8`) — no change needed here, this was already correct.
