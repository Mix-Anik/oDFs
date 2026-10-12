# `func_0x38ba` — publish race-timer state into `ps.stats` (+ time obfuscation `func_0x3f0c`)

**Address:** `0x38ba` (`enter 0x24`), arg0 = `gentity_t *ent`. Called from `0x259a`
(per-frame client function reached from `ClientEndFrame`, see
[../G_CLIENT/auth_handshake.md](../G_CLIENT/auth_handshake.md)) as its **first** action; also from
`0x274b`'s neighborhood per the disassembler's caller list (`0x274b 0x39c5 0x3b96`
are listed for the *next* function `0x3933`, not this one — this one's list is
only `0x259a`).

## Decode
```c
void func_0x38ba( gentity_t *ent ) {
    gclient_t   *cl  = ent->client;                 // +0x204
    raceTimer_t *rec = func_0x3588( cl );           // 0x1318dc + clientNum*32
    if ( rec->initialized /* +0x14 */ )  cl->ps.stats[12] |=  2;      // client+0xe8 = stats[12]
    else                                  cl->ps.stats[12] &= ~2;

    int v;
    if ( *(int*)0x1317c0 != 0 /*dedicated/variant flag, see G_UTILS/func_0x21fc.md*/ || func_0x3e0a() != 0 )   // 0x3e0a = cached sv_cheats (see G_COMBAT/G_Damage.md)
        v = rec->ms;                                    // plain elapsed ms
    else
        v = func_0x3f0c( rec->ms, cl );                 // obfuscated/encoded ms
    cl->ps.stats[8] = v & 0xffff;                       // client+0xd8
    cl->ps.stats[7] = v >> 16;                          // client+0xd4 (arithmetic shift)
}
```
Stat offsets: `ps.stats` base `client+0xb8` (stats[12] = `+0xe8` already used in
[../G_ACTIVE/ClientThink_real.md](../G_ACTIVE/ClientThink_real.md) docs; `+0xd4`/`+0xd8` follow by
index arithmetic only). Condition reading: the jump `0x38f7 ne → 0x3900`
(plain value) and `0x38ff eq → 0x3907` (encode path) as transcribed.

So the race time (32-bit `rec->ms`) is networked as two 16-bit halves in
`ps.stats[7]`/`[8]`, **encoded when `sv_cheats == 0` and `0x1317c0 == 0`**; and
`stats[12]` bit `0x2` mirrors `rec->initialized`.

## `func_0x3f0c(ms, client)` — fully decoded (second pass)
Syscall `-0x6f` = `floor` (trap id 110) and `-0x65` = memset (id 100); numbering `-(id+1)` as in `g_public.h`/`bg_lib` trap tables; the same pass checked `-0x67` = strncpy (`0x5dc2` = `rand`, confirmed elsewhere).
```c
int func_0x3f0c( int ms, gclient_t *cl ) {
    if ( ms == 0 ) return 0;
    ms &= 0x3fffff;                                              // 22 bits
    a = 0; for ( i = 0; i < 3; i++ ) a += (ms >> (6*i)) & 0x3f;
    a += (ms >> 18) & 0xf;
    ms |= (a & 0x3f) << 22;                                      // 6-bit checksum in bits 22..27
    r = rand() % 15 + 1;
    r |= (~r << 4) & 0xff;  r |= r << 8;  r |= r << 16;          // replicate byte (hi nibble = ~lo nibble)
    ms ^= r;  ms &= 0xfffffff;  ms |= r << 28;                   // low nibble of r into bits 28..31
    x  = level.time << 2;
    x += ( *(byte*)0x1320f0 + 0x4abb ) << 8;
    x ^= level.time << 24;
    ms ^= x;
    for ( j = 0; j < 3; j++ ) ((byte*)&ms)[j+1] ^= ((byte*)&ms)[j];   // chained byte XOR
    if ( *(int*)0x1317c0 == 0 ) {                                 // else return as-is
        v = (int)floor( cl->ps.origin[0] );   /* cl+0x14 */  ms ^= abs(v) & 0xffff;
        v = (int)floor( cl->ps.velocity[0] ); /* cl+0x20 */  ms ^= abs(v) << 16;
        v = cl->ps.stats[0] /* cl+0xb8, health */;           ms ^= ( v > 0 ) ? (v & 0xff) : 0x96;
        ms ^= ( cl->ps.movementDir /* cl+0x58 */ & 0xf ) << 28;
    }
    return ms;
}
```
(`abs` as the two `lei`-guarded negations at `0x3ff1`/`0x401e`.) So with cheats off the
networked time is a salted, checksummed value that also folds in the player's
current x-origin, x-velocity, health and `movementDir`, the level time and a
byte at `0x1320f0` (`= (Σ chars of lowercased mapname) & 0xff`, set by init `0x406d`, see below) — a tamper-evident
encoding, not a mere hash. The checksum/field layout is transcribed from the
bytecode; the *purpose* (anti-spoof of the displayed race time) is an inference.

**cgame consumer — `cgame 0x5fd7(arg0, arg1)` (called from cgame `0x5248`), read in a second pass:**
1. Cache: if `arg1 != 0` and `*(arg0+8) == *(arg1+8)` returns the cached decoded value `*(int*)0x11d77c`.
2. If a global counter `0x11d29c < 0x46b4`, it first defers to `cgame 0x5eb7(arg0)` and returns that result (`0x11d77c` updated) — an early/startup path.
3. Otherwise it rebuilds the 32-bit encoded value `x = (arg0+0x100) << 16 | ((arg0+0x104) & 0xffff)` (the two 16-bit halves published in `ps.stats[7]/[8]`; if both are 0 the result is 0),
   then recomputes the same salt the server folded in: for the mask pieces it uses `floor(float at arg0+0x40)` (|v| & 0xffff), `floor(float at arg0+0x4c)` (|v| << 16),
   `*(arg0+0xe4)` (`& 0xff`, or `0x96` when `<= 0`, i.e. health) and `(*(arg0+0x84) & 0xf) << 28`, XORing them into `x`; then undoes the chained byte-XOR (`b[i] ^= b[i-1]`, loop at `0x60f1`),
   the replicated random byte and the time salt, extracts `chk = (x >> 22) & 0x3f`, masks `x &= 0x3fffff`, recomputes the 6-bit-field checksum (same `6*i` loop plus `>>18 & 0xf`) and compares:
   mismatch → `cgame 0x59b4(2, 0)` (an error/cheat report, not decoded) and return 0; match → store to `0x11d77c` and return the decoded milliseconds.
**Resolved:** `arg0` is the cgame `snapshot_t*` (ps starts at `snap+0x2c`): `snap+0x100/0x104` = `ps.stats[7]/[8]`, `+0x40` = `ps.origin[0]`, `+0x4c` = `ps.velocity[0]`, `+0xe4` = `ps.stats[0]` (health), `+0x84` = `ps.movementDir` — exactly the fields the server encoder folds in. The decoded value is stored by `cgame 0x5248` into the per-checkpoint/stop time list `0x11d6b8[0x11d738]` and compared with the best-time table (`0x11d634`) to raise the new-record event bits `0x200`/`0x400`; i.e. DeFRaG's cgame takes checkpoint/finish times from these stats. Two debug-label artefacts in the `.dis` ("Item not found", "String: bad index") are
mis-attached constants (`0xffff`, `0x46b4`), not strings used here.

## Init `func_0x406d` (called once from `0x1c74`, init path) — salts for the encoder
```c
*(int*)0x1320fc = level.time;                                        // init stamp (readers not searched)
trap_Cvar_Set( "defrag_vers", va("%i", 0x4abb /*19131*/) );          // name built byte-by-byte on stack; value 19131 = "1.91.31"
name = Cvar_VariableString("mapname"); strncpy(buf, name, 0x80); buf[0x7f]=0;
func_0xaadf( buf );                                                  // string helper, same one 0x2d02 applies to the map name (lowercasing shape; not hash-checked)
*(byte*)0x1320f0 = 0; for ( p = buf; *p; p++ ) *(byte*)0x1320f0 += *p;   // byte-sum of the map name
```
(`func_0x138e(name)` = `Cvar_VariableStringBuffer(name, 0x12c6f0, 0x100); return 0x12c6f0;`.) The
`19131` literal independently matches the `"1.91.31"` version string used by the
auth hash in [../G_CLIENT/auth_handshake.md](../G_CLIENT/auth_handshake.md). Note
oDFs registers `defrag_vers` itself in `g_main.c` as a ROM serverinfo cvar
(value `ODFS_VERSION`) — this init overwrites it at runtime with `19131` in DeFRaG.

## oDFs — PORTED (2nd pass)
`g_defrag.c`: `G_PublishRaceTime` (called from `G_UpdateRacePs`) writes `ps.stats[7]/[8]` (new `STAT_RACE_TIME_HI/LO`) = `raceTimer.ms` (new field: whole-ms elapsed, frozen after the finish) and sets/clears `stats[12]` bit 2 from `initialized`; plain when `g_dfVariant` (dedicated) or `sv_cheats`, otherwise `G_EncodeRaceTime` (transcribed from `0x3f0c` above, salted with mapname byte-sum + 19131). **Behaviour change:** cgame now receives times it previously saw as 0; revert = remove the `G_PublishRaceTime( ent )` call. Syntax-checked, untested; the encoder has not been cross-checked against the cgame decoder.

## (old note) oDFs
Not ported. oDFs's cgame path derives timer start from `STAT_DEFRAG_FLAGS`
state-change bits (see `g_defrag.c` comments / `CGAME/cgame_timer_hud.md`), and
nothing in oDFs writes `stats[7]`/`stats[8]` or flag bit `0x2`; porting the
encoded value without the matching cgame decoder would break nothing but add
nothing either. Bit `0x2` ("timer initialized") is a possible small port once
cgame's reader of `stats[12] & 2` is confirmed — not checked.
