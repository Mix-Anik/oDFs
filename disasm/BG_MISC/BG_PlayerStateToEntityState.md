# `BG_PlayerStateToEntityState` / `BG_PlayerStateToEntityStateExtraPolate`

- `BG_PlayerStateToEntityState` — DeFRaG `0x5338`, vanilla `0x10647`
- `BG_PlayerStateToEntityStateExtraPolate` — DeFRaG `0x5532`, vanilla `0x10841`

## Sizes (strong indicator of near-total vanilla equivalence)

| Function | DeFRaG size | Vanilla size |
|---|---|---|
| `BG_PlayerStateToEntityState` | `0x1f9` (505 B) — `0x5531-0x5338` | `0x1fa` (506 B) — `0x10841-0x10647` |
| `BG_PlayerStateToEntityStateExtraPolate` | `0x206` (518 B) — `0x5738-0x5532` | `0x207` (519 B) — `0x10a48-0x10841` |

Both are 1 byte smaller than vanilla — consistent with the "size ±1 drift"
pattern already seen for confirmed-unmodified functions elsewhere in this
project (e.g. `Weapon_HookFree`/`Weapon_HookThink` in `G_WEAPON/README.md`),
not a sign of missing logic.

## Identification

Confirmed via full instruction-level decode of `BG_PlayerStateToEntityState`
(not just size/position):

- 3 args (`ps`, `s`, `snap`) — `; 0x10 args` matches vanilla's
  `(playerState_t*, entityState_t*, qboolean)` exactly.
- `if (ps->pm_type==PM_INTERMISSION(5) || ps->pm_type==PM_SPECTATOR(2)) s->eType=ET_INVISIBLE;`
  — matches vanilla's enum values (`pm_type` at confirmed offset `+0x4`).
- `s->number (entityState_t offset 0x0) = ps->clientNum (playerState_t offset 0x8c)`
  — the `playerState_t` offset matches oDFs's own struct exactly: `clientNum`
  sits right after `eventParms[2]` at `0x8c`, as expected.
- `s->pos.trType = TR_INTERPOLATE(1)`, `VectorCopy(ps->origin, s->pos.trBase)`,
  conditional `SnapVector` (Q_ftol round-trip pattern, `cvfi`/`cvif` — matches
  vanilla's `SnapVector` inlining pattern seen elsewhere), `VectorCopy(ps->velocity, s->pos.trDelta)`.
- `s->apos.trType = TR_INTERPOLATE`, copies `ps->viewangles`.
- `s->angles2[YAW] = ps->movementDir`, `legsAnim`, `torsoAnim`, `clientNum`,
  `eFlags` copies — all field offsets read line up with the computed
  `playerState_t`/`entityState_t` layouts (see below).
- Externally-visible event-sequencing logic
  (`ps->externalEvent`/`entityEventSequence`/`eventSequence`/`events[]`/
  `eventParms[]`, with `MAX_PS_EVENTS` = confirmed **2** matching
  `code/qcommon/q_shared.h`) — **byte-for-byte identical control flow and
  constants to vanilla**, no deltas found.
- `s->weapon = ps->weapon`, `s->groundEntityNum = ps->groundEntityNum`,
  the `powerups` bit-packing loop (`for i in 0..15: if ps->powerups[i]: s->powerups |= 1<<i`,
  loop bound confirmed as `MAX_POWERUPS`=16 via the `0x138`
  `powerups[]`-base offset already cross-confirmed against
  `disasm/README.md`'s `+0x170`=`PW_INVULNERABILITY` note), `s->loopSound`,
  `s->generic1` — all confirmed matching vanilla exactly, both logic and
  field offsets.

`BG_PlayerStateToEntityStateExtraPolate` was spot-checked (not fully
line-by-line decoded) and confirmed to share the same opening
`pm_type`/health gate (see finding below, same constant), the same
`TR_LINEAR_STOP`(=3) trajectory type (vanilla's distinguishing feature vs.
the non-extrapolate version), and the vanilla `trDuration = 50` (`0x32`)
constant — consistent with it being the same vanilla logic pattern, just
targeting `TR_LINEAR_STOP` instead of `TR_INTERPOLATE`.

## Confirmed struct-offset cross-check (both functions)

Every `playerState_t`/`entityState_t` field offset referenced by these two
functions was checked against offsets computed directly from oDFs's own
(vanilla-shaped) struct definitions in `code/qcommon/q_shared.h`, and all
matched exactly: `pm_type`=`0x04`, `stats[STAT_HEALTH]`=`0xb8`,
`eventSequence`=`0x6c`, `events[]`=`0x70`, `eventParms[]`=`0x78`,
`externalEvent`=`0x80`, `externalEventParm`=`0x84`, `externalEventTime`=`0x88`,
`clientNum`=`0x8c`, `weapon`=`0x90`, `weaponstate`=`0x94`,
`entityEventSequence`=`0x1d0`, `powerups[]` base=`0x138`,
`loopSound`=`0x1bc`, `generic1`=`0x1b8`. **This is strong evidence DeFRaG's
`playerState_t` header layout (the entire region touched by these two
functions) is identical to oDFs's current vanilla-shaped struct** — no
struct-layout drift found here (consistent with — and independently
corroborating — the same conclusion reached while decoding
`BG_TouchJumpPad`, see that doc).

## CONFIRMED DELTA — `GIB_HEALTH` gate uses `0`, not `-40`

Vanilla (and oDFs's `code/game/bg_public.h`, which defines
`#define GIB_HEALTH -40`):
```c
} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
    s->eType = ET_INVISIBLE;
}
```
DeFRaG's compiled bytecode (both functions, identical in each):
```
0x5350  arg0->stats[0]   ; STAT_HEALTH, offset 0xb8 confirmed
0x5355  const 1 0x0      ; literal integer 0 — raw bytes "00 00 00 00" confirmed, not a misread
0x5356  gti -2 0x535f    ; branch (to ET_PLAYER path) taken when stats[0] > 0
```
i.e. the effective condition is `ps->stats[STAT_HEALTH] <= 0`, not `<= -40`.
Confirmed identical in `BG_PlayerStateToEntityStateExtraPolate` at the
equivalent site (`0x554e`/`0x554f`/`0x5550`, same `0x0` constant).

**Note this is a different, unrelated check from the `EF_DEAD` flag logic**
(`if (ps->stats[STAT_HEALTH] <= 0) s->eFlags |= EF_DEAD; else clear it;`),
which vanilla *also* compares against `0`, confirmed unchanged at `0x5423`–
`0x5429` (same `0x0` constant, matches vanilla exactly — only the
`GIB_HEALTH` gate for `eType` differs, not the `EF_DEAD` gate).

**Behavioral effect:** in DeFRaG, a client's entity is marked `ET_INVISIBLE`
(stops being transmitted/rendered as a normal player entity — used for the
post-death corpse/gib transition window) as soon as health reaches `0`,
rather than only once health drops to `-40` or below as in vanilla. Since
`EF_DEAD` is already set at health `<=0` in both versions, this narrows (in
DeFRaG) or widens (relative to DeFRaG, if oDFs keeps vanilla's `-40`) the
window during which a freshly-dead player is still sent as a visible corpse
entity before flipping to invisible. **oDFs's `bg_public.h` currently
defines `GIB_HEALTH -40` (vanilla value) — this is a confirmed,
gameplay-relevant divergence from DeFRaG's actual compiled behavior worth
reviewing for a port.** (Whether oDFs's `BG_PlayerStateToEntityState`
implementation itself uses `GIB_HEALTH` or a literal was not re-checked here
since the task is documentation/research only — see `code/game/bg_misc.c`
lines ~1469 and ~1545, both currently `<= GIB_HEALTH`.)

## `BG_EntityStateToPlayerState` — does not exist in this codebase

Searched `baseq3-game-functions.hmap` (vanilla) and oDFs's `bg_misc.c` for
`BG_EntityStateToPlayerState`: **absent from both.** This Q3 SDK revision's
`bg_misc.c` (matching oDFs's current vanilla-based file) simply does not
implement this function — it is not a DeFRaG removal, there is nothing to
compare. (oDFs's own `bg_misc.c` was read in full — no matching function.)
