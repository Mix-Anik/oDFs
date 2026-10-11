# func_0x1a1b2 — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x0001a1b2`
**Size:** 483 instructions (confirmed exact: `0x1a394 - 0x1a1b2 = 0x1e2` = 482, matching the hash-table-recorded size)
**Stack frame:** `enter 0x10a8` (4264 bytes — large; holds entity list buffer + string buffers)
**Called from:** `0x1a866` (Pmove outer / real `ClientThink_real`, section 13), for alive non-noclip players only

**Identified as: this is `G_TouchTriggers`.** Not a race-only sibling of a
still-missing generic function — **this is the genuine article**, confirmed
below.

## Resolution history (read this before anything below)

This function went through two mischaracterizations before landing here:

1. **Originally labeled "race-trigger-only touch handler"** — this led
   oDFs to implement it as a separate `DeFRaG_TouchTriggers`, scoped to
   race triggers, running *alongside* a second, still-called vanilla
   `G_TouchTriggers`.
2. **A later pass (same investigation, see
   [`../G_UTILS/README.md`](../G_UTILS/README.md)'s `G_TouchTriggers`
   search) found the "race-only" label didn't hold up** — this function
   processes `ET_ITEM`/`ET_PUSH_TRIGGER` through the same filters as
   everything else — but at that point still believed the `eType==9`
   (`ET_TELEPORT_TRIGGER`) exclusion was a **universal** exclusion, leaving
   "how does DeFRaG touch teleporters" as an open question.
3. **This pass found the universal exclusion claim was itself a misread.**
   Re-tracing the exact branch targets around the `sessionTeam`/`eType`/
   `0x23ad9` checks shows they form **one single nested conditional**, not
   a flat list of independent exclusions — and it is **byte-for-byte
   identical to vanilla `G_TouchTriggers`'s own spectator-exception
   block**:
   ```c
   // vanilla Q3 G_TouchTriggers:
   if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
       if ( hit->s.eType != ET_TELEPORT_TRIGGER && hit->touch != Touch_DoorTrigger ) {
           continue;
       }
   }
   ```
   The `eType==9` and `touch==0x23ad9` checks are only ever reached when
   `sessionTeam==3` is true — for any normal (non-spectator) player, the
   whole block is skipped via a direct jump, and teleporters are **never**
   excluded. This fully resolves the "how do teleporters get touched"
   question: **by this exact function, like everything else** — no second
   mechanism exists or is needed.
4. **`0x23ad9` is now identified: it is `Touch_DoorTrigger`**, confirmed by
   decoding it in full and matching it instruction-for-instruction against
   oDFs's own (vanilla-derived, unmodified) `Touch_DoorTrigger` in
   [`../../code/game/g_mover.c:857`](../../code/game/g_mover.c#L857) — see
   "`0x23ad9` = `Touch_DoorTrigger`" below.
5. **The function's tail (previously unread) contains the jumppad-frame
   reset**, matching vanilla's end-of-`G_TouchTriggers` housekeeping
   exactly — see "Tail: jumppad-frame reset" below. This means
   `func_0x1a1b2` is a **complete**, self-contained `G_TouchTriggers`, not
   a partial/restricted one missing some vanilla behavior.

**Net result: `func_0x1a1b2` is DeFRaG's `G_TouchTriggers`, full stop.**
Only one genuinely new/unresolved detail remains (the `0x1cc & 0x40000000`
exclusion bit, see below) — everything else matches vanilla's structure
exactly, just restructured to run once per frame from inside
`ClientThink_real`/"Pmove outer" instead of being a separately-named call.

---

## Guards

```c
if ( !ent->client ) return;
if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) return;   // dead players skip
```

Exactly vanilla's own entry guards.

---

## Structure

### 1. Compute broadphase AABB (0x1a1ce–0x1a21d)

Reference vec3 at `0x232c–0x2334` = **{40.0, 40.0, 52.0}** — vanilla's exact
`static vec3_t range = { 40, 40, 52 };`.

```c
mins[0] = ps.origin[0] - 40.0;  maxs[0] = ps.origin[0] + 40.0;
mins[1] = ps.origin[1] - 40.0;  maxs[1] = ps.origin[1] + 40.0;
mins[2] = ps.origin[2] - 52.0;  maxs[2] = ps.origin[2] + 52.0;
```

### 2. EntitiesInBox query (0x1a21e–0x1a229)

```c
count = trap_EntitiesInBox( mins, maxs, entity_list, 0x400 );   // trap -0x21
```

### 3. Compute extended AABB for the per-entity exact-overlap test (0x1a22a–0x1a29d)

Uses `ent+0x1b4..0x1cc` ("player movement bounds", written back by Pmove
outer after physics each frame) instead of vanilla's
`VectorAdd(ps.origin, ent->r.mins/maxs, ...)` — same result, computed from
a pre-combined snapshot rather than on the fly. Not a behavioral
difference.

### 4. Entity loop (0x1a29f–0x1a369)

```c
for ( i = 0; i < count; i++ ) {
    trigger = &g_entities[ entity_list[i] ];

    // combined early-out: skip only if BOTH touch pointers are null
    // (local1098 = 0x2c0, local109c = 0 — i.e. this is just
    // "if (!trigger->touch && !ent->touch) continue;", not a mysterious
    // ownership/self-touch check as an earlier pass's "not translated"
    // note speculated)
    if ( *(int*)((char*)trigger + 0x2c0) == 0 && *(int*)((char*)ent + 0x2c0) == 0 )
        continue;

    // genuinely new, unconfirmed DeFRaG-added exclusion — see below
    if ( trigger->flags /* +0x1cc */ & 0x40000000 )
        continue;

    // byte-for-byte vanilla spectator exception (confirmed this pass —
    // see "Resolution history" above); unreachable in practice at this
    // call site because the caller (Pmove outer) already returns early
    // for sessionTeam==TEAM_SPECTATOR before ever reaching this function
    if ( ent->client->sess.sessionTeam == 3 /* TEAM_SPECTATOR */ ) {
        if ( trigger->s.eType != 9 /* ET_TELEPORT_TRIGGER */
          && trigger->touch != (void*)0x23ad9 /* Touch_DoorTrigger */ ) {
            continue;
        }
    }

    // exact overlap test
    qboolean touching;
    if ( trigger->s.eType == 2 /* ET_ITEM */ ) {
        touching = func_0x4bc8( ent->client, trigger, level.time );  // plausibly BG_PlayerTouchesItem-equivalent
    } else {
        touching = trap_EntityContact( mins, maxs, trigger );        // trap -0x22
    }
    if ( !touching ) continue;

    trace_t trace;
    memset( &trace, 0, 0x38 );

    if ( trigger->touch )
        trigger->touch( trigger, ent, &trace );

    if ( (ent->flags /* +0x1a8 */ & 0x8) && ent->touch )
        ent->touch( ent, trigger, &trace );
}
```

### 5. Tail: jumppad-frame reset (0x1a370–0x1a393)

Previously unread by any prior pass (earlier docs only covered the loop,
not the function's true tail — the function's actual end is `0x1a393`,
confirmed via its matching `leave 0x10a8` immediately preceding the next
function, `func_0x1a394`, the already-documented "pm struct builder").
This block matches vanilla's end-of-`G_TouchTriggers` exactly:

```c
// vanilla:
if ( ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount ) {
    ent->client->ps.jumppad_frame = 0;
    ent->client->ps.jumppad_ent = 0;
}
```
```c
// decoded (base pointer confirmed as ps/client, offsets +0x1cc/+0x1c8
// cross-validated against ../BG_MISC/BG_TouchJumpPad.md's own independently
// confirmed "ps->jumppad_frame(+0x1cc) = ps->pmove_framecount(+0x1c8)"):
if ( ps->jumppad_frame /* +0x1cc */ != ps->pmove_framecount /* +0x1c8 */ ) {
    client->jumppad_frame /* +0x1cc */ = 0;
    client->jumppad_ent   /* +0x1c0 */ = 0;
}
```

This confirms `func_0x1a1b2` is a **complete** `G_TouchTriggers`
(entry guards → broadphase query → per-entity filter/dispatch loop →
jumppad-frame housekeeping), not a partial one missing the tail-end
vanilla behavior.

---

## `0x23ad9` = `Touch_DoorTrigger`

Full decode confirms a 3-arg `touch(self, other, trace)` shape whose logic
matches oDFs's own (vanilla-derived) `Touch_DoorTrigger` instruction for
instruction:

```c
// decoded 0x23ad9:
void func_0x23ad9( gentity_t *self, gentity_t *other, trace_t *trace ) {
    gclient_t *client = other->client;             // +0x204
    if ( client != NULL && client->sess.sessionTeam /* +0x284 */ == 3 ) {
        int moverState = *(int*)((char*)self->parent /* +0x258 */ + 0x240);
        if ( moverState == 2 || moverState == 1 )   // MOVER_POS2 / MOVER_1TO2
            return;
        func_0x239f9( self, other, trace );          // = Touch_DoorTriggerSpectator
        return;
    }
    int moverState = *(int*)((char*)self->parent /* +0x258 */ + 0x240);
    if ( moverState == 2 /* MOVER_1TO2 */ )
        return;
    func_0x236b5( self->parent, self, other );        // = Use_BinaryMover(ent->parent, ent, other)
}
```

```c
// oDFs's existing Touch_DoorTrigger (g_mover.c:857), vanilla-derived, unmodified:
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace ) {
	if ( other->client && other->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( ent->parent->moverState != MOVER_1TO2 &&
			ent->parent->moverState != MOVER_POS2) {
			Touch_DoorTriggerSpectator( ent, other, trace );
		}
	}
	else if ( ent->parent->moverState != MOVER_1TO2 ) {
		Use_BinaryMover( ent->parent, ent, other );
	}
}
```

Exact structural match (modulo the decode's `moverState==1||==2` early-return
framing vs. the source's `!=1 && !=2` guard — logically identical).
`func_0x239f9` = `Touch_DoorTriggerSpectator`, `func_0x236b5` =
`Use_BinaryMover`, both by the same positional/argument-shape match, not
independently re-derived byte-for-byte this pass. `self+0x258` = `parent`
(a `gentity_t*`), matching this project's established "entity pointer field
reused as argument and as a base for further dereferences" pattern.

---

## The one remaining genuinely unresolved detail: `trigger->flags & 0x40000000`

No spawn-time writer of this bit was found anywhere in the QVM (a plain
text search for `0x40000000` is mostly useless on its own — the same bit
pattern is also the IEEE-754 float literal `2.0`, used constantly elsewhere
in unrelated float math). Its purpose is **not confirmed**. Earlier passes'
"excludes non-race entities" label was an unverified assumption — do not
trust it. Whatever this bit means, it is a narrow, additional DeFRaG filter
layered on top of an otherwise-complete vanilla `G_TouchTriggers` — flagged
for a future pass, not guessed at further.

---

## Key constants

| Value | Meaning |
|-------|---------|
| `{40.0, 40.0, 52.0}` | AABB broadphase half-extents (data at 0x232c–0x2334) — vanilla-identical |
| `0x40000000` | `trigger->flags` bit — purpose **not confirmed** (see above) |
| `0x23ad9` | `Touch_DoorTrigger` (confirmed this pass) |
| `0x239f9` | `Touch_DoorTriggerSpectator` (by positional match) |
| `0x236b5` | `Use_BinaryMover` (by positional match) |
| `0x400` | Max entities returned by EntitiesInBox |
| `-0x21` | trap_EntitiesInBox |
| `-0x22` | trap_EntityContact (mins, maxs, entity) → bool |
| `0x4bc8` | DeFRaG's `ET_ITEM` overlap check (client, trigger, level.time) — plausibly the source of oDFs's own `BG_PlayerTouchesItem`, not independently re-derived this pass |

---

## gentity_t / gclient_t field offsets confirmed or cross-validated this pass

| Offset | Field |
|--------|-------|
| `ent+0x4` | `s.eType` |
| `ent+0x1a8` | flags (bit `0x8` = player has active touch) |
| `ent+0x1cc` | `flags`; bit `0x40000000` causes exclusion here — purpose otherwise unconfirmed |
| `ent+0x2c0` | `touch` function pointer, `touch(self, other, trace*)` |
| `ent+0x258` | `parent` (`gentity_t*`) — confirmed via the `Touch_DoorTrigger` match above |
| `client+0x284` | `sess.sessionTeam` (3 = `TEAM_SPECTATOR`) |
| `client/ps+0x1c0` | `jumppad_ent` — cross-validated against `../BG_MISC/BG_TouchJumpPad.md` |
| `client/ps+0x1c8` | `pmove_framecount` — ditto |
| `client/ps+0x1cc` | `jumppad_frame` — ditto |
| `0x240` (relative to a mover's `parent`) | `moverState` (`MOVER_1TO2`=1, `MOVER_POS2`=2, by match against `Touch_DoorTrigger`'s known enum usage) |

---

## oDFs implementation — fixed this pass

Previously, oDFs ran **two** separate functions every non-noclip frame:
`DeFRaG_TouchTriggers` (modeled on the old, wrong "race-only" reading —
unconditionally excluded `ET_TELEPORT_TRIGGER` and all spectators, and had
no jumppad-frame reset of its own) followed later by the full vanilla
`G_TouchTriggers`. Since `func_0x1a1b2` is now confirmed to be a complete,
self-contained `G_TouchTriggers` — including the spectator exception and
the jumppad-frame reset — running both every frame double-touches every
non-exempt trigger (items, push triggers, doors) that both functions agree
to process.

**Fixed:** `DeFRaG_TouchTriggers` (`g_active.c`) now implements the correct
nested spectator/`Touch_DoorTrigger` exception (instead of a blanket
spectator/teleporter skip) and includes the jumppad-frame reset tail,
making it a faithful, complete port of `func_0x1a1b2`. The redundant
second `G_TouchTriggers(ent)` call inside `ClientThink_real`'s non-spectator
tail has been removed — `G_TouchTriggers` as a function is kept (still
legitimately used by `SpectatorThink`'s separate per-spectator-physics
call, which matches a genuinely different call site in real DeFRaG, inside
the spectator-only early-return branch of "Pmove outer" that `func_0x1a1b2`
is never reached from at all).
