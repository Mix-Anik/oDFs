# Touch_Item — DeFRaG's per-client item pickup mechanism

**Address:** `0x20f11`
**Size:** `0x20a` per hash table (`0x20f11`→`0x2111a`, effective body `0x209` instructions, consistent
with this project's documented systemic "+1 size drift" between consecutive hash-table entries).
**Hash:** `-c015487` — vanilla `Touch_Item` hash is `-79df7020` (`0x19626` in
`baseq3-game-functions.hmap`). **Confirmed absent/mismatched — modified**, as expected.
**Vanilla reference:** `Touch_Item`, `D:\Other\Reverse Engineering\q3vm\baseq3-game-functions.hmap`
line 491 (`0x19626 Touch_Item -79df7020`).

**Identity confirmed by content** (not position/order alone), via three independent lines of evidence:
1. Full switch dispatch on `ent->item->giType` (`ent+0x324`→`+0x24`), values 1–8, calling the exact
   sibling Pickup_\* functions in the exact vanilla order (see table below) — matches vanilla
   `Touch_Item`'s `switch(ent->item->giType)` structurally and by case value.
2. Calls `BG_CanItemBeGrabbed` (`0x4c42`, already confirmed in `BG_MISC/README.md`), `G_UseTargets`
   (`0x28151`, already confirmed in `G_UTILS/G_UseTargets.md`), and the `"Item: %i %s\n"` debug string
   (`0x8ed8`) at the exact vanilla call positions.
3. **This exact address (`0x20f11`) is written into `gentity_t.touch` (`ent+0x2c0`) by two independent
   sibling functions**: `LaunchItem` (`0x2111a`, at instruction `0x21177`) and `FinishSpawningItem`
   (`0x212a7`, at instruction `0x212f4`) — both are vanilla's two call sites that set
   `ent->touch = Touch_Item;` (dropped items and static map items respectively). This is the strongest
   evidence: the constant `0x20f11` is literally assigned as a touch-callback function pointer, in both
   places vanilla does so, with nothing else nearby plausible.

This **corrects the prior pass's doubt** (recorded in `disasm/G_ACTIVE/func_0x20d2d.md` and
`disasm/README.md`'s entry for `0x20f11`) that this function was merely a "Pickup_Item dispatcher,
not independently confirmed", and that it "contains zero svFlags/nextthink manipulation... meaning it
probably is NOT the function that hides/respawns items". That observation about missing
`svFlags`/`nextthink` writes is **correct and important** — see "Headline finding" below — but the
conclusion drawn from it (that this therefore isn't `Touch_Item`) was wrong. It **is** `Touch_Item`;
DeFRaG's `Touch_Item` genuinely no longer touches `svFlags`/`nextthink` at all, because the whole
hide/respawn model changed.

---

## Headline finding: the real per-client pickup mechanism

**DeFRaG's `Touch_Item` never hides the item entity.** There is no `ent->r.svFlags |= SVF_NOCLIENT`,
no `ent->s.eFlags |= EF_NODRAW`, no `ent->r.contents = 0`, no `trap_LinkEntity`/`trap_UnlinkEntity`
call, and no `ent->nextthink`/`ent->think` assignment anywhere in this function — confirmed by reading
every instruction in the function body (`0x20f11`–`0x21119`), not just grepping for a cvar.

Instead, **pickup eligibility is gated per-client** using a brand-new `gentity_t` field:

### New field: `gentity_t + 0x32c` — `int perClientPickupTime[64]`

One 4-byte slot per possible `clientNum` (0–63), indexed as `*(int*)(ent + 0x32c + clientNum*4)`.
Meaning of the stored value:

| Value | Meaning |
|-------|---------|
| `0` (default, zero-initialized by `G_Spawn`/`G_FreeEntity`'s `memset`) | This client may pick up the item right now. |
| a future `level.time`-scale timestamp (ms) | This client specifically may not pick up the item again until `level.time` reaches this value. **Other clients' slots are untouched** and are evaluated independently. |
| `-1` | This client may **never** pick up this specific item entity again (used for non-respawning items — see case below). Still only affects that one client's slot. |

**This confirms the user's reported DeFRaG behavior exactly**: picking up an item sets *only the
picker's own* cooldown slot. Every other client's slot is unaffected, so the item remains fully
visible, solid, and pickupable by them with zero delay. There is no shared/global hide state at all —
not `SVF_NOCLIENT` (hides from everyone), not `SVF_SINGLECLIENT`/`SVF_NOTSINGLECLIENT` (these flags are
defined in `code/game/g_public.h` but **are never referenced anywhere in this function or its sibling
item functions** — confirmed by direct instruction-level reading, not just a cvar grep). The item's
server-side `r.svFlags`/`s.eFlags`/`contents`/`nextthink` never change on pickup at all.

### Field layout cross-check (independent corroboration)

`ent+0x32c` to `ent+0x42c` is exactly `0x100` bytes = 64 × 4-byte slots. `G_UseTargets`'s own new
fields (documented in [`../G_UTILS/G_UseTargets.md`](../G_UTILS/G_UseTargets.md)) begin at **exactly**
`ent+0x42c` and run through `ent+0x438`+4 = `ent+0x43c`, which is **exactly `sizeof(gentity_t)` as
already documented** in `disasm/README.md` (`0x43c`). The two independently-reverse-engineered
DeFRaG field additions tile perfectly end-to-end with no gap and no overlap, strongly corroborating
both offsets.

### Companion field: `gentity_t + 0x54` / `gentity_t + 0x58` — 64-bit per-client bitmask

A second, smaller per-client state: two 32-bit words forming a 64-bit bitmask, one bit per
`clientNum` (`+0x54` holds clients 0–31, `+0x58` holds clients 32–63). Set via helper `0x1629`
(`*(word) |= 1<<bit`) and cleared via helper `0x15fa` (`*(word) &= ~(1<<bit)`), both taking
`(gentity_t *ent, int clientNum)`.

- `Touch_Item` calls the **set** helper (`0x1629`) at `0x21109`, right after writing the cooldown slot.
- `FinishSpawningItem` (`0x212a7`) also calls the **set** helper `0x1629` (confirmed via its own
  caller-list comment in the disassembly: "`; called from 0x2830 0x20f11 0x212a7`" preceding `0x1629`'s
  `enter`).
- The **clear** helper (`0x15fa`) is called only from the `Use_Item`/`RespawnItem`-merged function
  `0x21249` (see [`RespawnItem.md`](RespawnItem.md)).

**Purpose not independently confirmed.** `ent+0x54`/`0x58` fall well inside the `entityState_t` region
of `gentity_t` (long before `r.svFlags` at the documented `+0x1a8`), so this is almost certainly two
otherwise-unused `entityState_t` trajectory/sound ints repurposed by DeFRaG (the same pattern already
seen with `item->item->random` in `func_0x20d2d.md`), not a new field appended at the struct's end.
It behaves as a companion "has this client interacted with this item" bit alongside the cooldown
timestamp, but its exact consumer (if any — it may only be set/cleared and never subsequently read by
anything in this QVM) was not located. Documented because it is unambiguously real, deliberate,
DeFRaG-added logic; not required to explain the core visibility mechanism above.

### No cvar gates any of this

Full grep of the entire disassembly for the `df_promode` cvar cache address (`0x1317c4`) found **zero
references** anywhere inside `Touch_Item`, the `Use_Item`/`RespawnItem`-merged function, or
`FinishSpawningItem`/`LaunchItem`/`Drop_Item`. A full inventory of every `"df_*"` cvar-name string
literal in the whole QVM (there are ~50, including `df_promode`, `df_gametype`, `df_mode_fastcaps`,
`df_mode_tricksmode`, `df_ndm_itemsSpawn`, `df_ndm_weaponsPickup`, etc.) turned up **no cvar literally
named `df_mode`** at all in this build. The per-client mechanism is **unconditional** — it is not a
`df_mode`-gated opt-in, not `df_promode`-gated, and not gated by any other cvar found in this
disassembly. (oDFs's own `df_mode` cvar, `code/game/g_main.c:88`, does not correspond to any cvar name
present in this particular DeFRaG QVM build — noted for the porting decision, not asserted for all
DeFRaG versions/builds.)

---

## Full structure, by address

```c
// arg0 = ent (the item entity), arg1 = other (the toucher)
int Touch_Item(gentity_t *ent, gentity_t *other) {

    // 0x20f11-0x20f1a: vanilla-identical
    if (!other->client)              // other+0x204
        return;

    // 0x20f1b-0x20f23: vanilla-identical in shape ("other->health < 1")
    if (other->health < 1)           // other+0x2dc, offset not independently
        return;                      // confirmed elsewhere in this project but
                                      // position/role match vanilla's other->health check

    // 0x20f24-0x20f37: NEW — per-client cooldown gate, part 1
    if (ent->perClientPickupTime[other->client->clientNum] < 0)   // ent+0x32c+clientNum*4
        return;                                                    // (clientNum = other->client+0x8c)

    // 0x20f38-0x20f4c: NEW — per-client cooldown gate, part 2
    if (level.time < ent->perClientPickupTime[other->client->clientNum])  // level.time @ 0x12a310
        return;

    // 0x20f4d-0x20f70: vanilla-identical
    if (!BG_CanItemBeGrabbed(ent, other->client))   // call 0x4c42; args passed as (ent, client)
        return;                                      // i.e. (&ent->s, &other->client->ps) since both
                                                       // structs' relevant member sits at offset 0

    // 0x20f71-0x20f80: vanilla-identical
    G_LogPrintf("Item: %i %s\n", other->s.number, ent->item->classname);  // literal @ 0x8ed8

    // 0x20f81-0x20f94: vanilla-identical
    predict = other->client->pers.predictItemPickup;   // other->client+0x1f8, offset not
                                                          // independently confirmed elsewhere
    giType  = ent->item->giType;                        // ent+0x324 -> +0x24

    // 0x20f95-0x21012: vanilla-identical switch structure/case values; see dispatch table below
    switch (giType) {
        case 1: respawn = Pickup_Weapon(ent, other);   break;  // 0x20d2d
        case 2: respawn = Pickup_Ammo(ent, other);     break;  // 0x20ced
        case 3: respawn = Pickup_Armor(ent, other);    break;  // 0x20ecd
        case 4: respawn = Pickup_Health(ent, other);   break;  // 0x20e25
        case 5: respawn = Pickup_Powerup(ent, other);  break;  // 0x20afa
        case 6: respawn = Pickup_Holdable(ent, other); break;  // 0x20c57
        case 8: respawn = Pickup_Team(ent, other);     break;  // 0x26e74
        // no case 7 (IT_PERSISTANT_POWERUP) label observed; not confirmed why
        default: return;   // bounds-checked 1..8 via lti/gti before the jump table
    }

    // 0x21013-0x2102c
    func_0x3b96(other, ent);                 // unidentified, result ignored (pop'd)
    if (func_0x3c23(ent->item) == 0)          // unidentified gate on the gitem_t pointer
        respawn = 3;                          // overwrite — purpose/field semantics not confirmed

    // 0x2102d-0x21032: vanilla-identical
    if (respawn == 0)
        return;

    // 0x21033-0x21054: vanilla-identical (predict-vs-non-predict pickup sound event)
    if (predict)
        func_0x28600(other, EV_ITEM_PICKUP /*0x13*/, ent->s.modelindex);   // G_AddPredictableEvent-shaped
    else
        G_AddEvent(other, EV_ITEM_PICKUP, ent->s.modelindex);              // 0x2861b, confirmed address

    // 0x21055-0x2107c: CONFIRMED DeFRaG addition, NOT part of the visibility mechanism —
    // only runs when giType != 1 (not IT_WEAPON). Debug string "aswitch %i" (@ 0x8ecd) plus a call
    // to an unidentified helper (0x ac4e) and a trap call (-0x12). A second nearby debug string
    // "...me changed to %s^7" and associated float math (0x210a2-0x210cc) appear to belong to a
    // related but SEPARATE auto-weapon-switch-on-pickup feature, not the item-visibility mechanism.
    // Neither branch touches ent->perClientPickupTime, svFlags, eFlags, or nextthink. Out of scope,
    // not further decoded.

    // 0x2107d-0x21085: vanilla-identical
    G_UseTargets(ent, other);   // call 0x28151, confirmed address

    // 0x21086-0x21094: confirmed override — NOT vanilla's polarity
    // (vanilla: "if (!ent->wait) ent->wait = g_defaultItemWait / 1000;")
    if (ent->wait != 0.0f)            // ent+0x31c, read as float
        respawn = (int)ent->wait;     // a nonzero per-entity "wait" map key OVERRIDES the Pickup_*
                                       // function's returned default respawn-seconds value

    // 0x21095-0x210a1: confirmed — IT_TEAM-specific override
    if (giType == 8)        // IT_TEAM
        respawn = 1;

    // 0x210a2-0x210cc: confirmed to exist, NOT fully decoded — gated on ent->(0x320) as float != 0.0,
    // involves a call to an unidentified helper (0x5dc2) and float arithmetic against a debug string
    // "...me changed to %s^7" (likely the same auto-weapon-switch feature noted above). Can further
    // modify `respawn`. Left as an open detail — does not change the core per-client mechanism.

    // 0x210cc-0x210db: confirmed write, field purpose not independently confirmed
    if (!(ent->itemFlags /*+0x218*/ & 0x1000))
        ent->(+0x22c) = 1;   // plausibly an FL_RESPAWN-style marker on a dedicated field rather than
                             // an OR'd-in bit; not confirmed; does not gate visibility in any code
                             // path traced from this function

    // 0x210db-0x21108: *** THE MECHANISM ***
    if (respawn > 0) {
        ent->perClientPickupTime[other->client->clientNum] =
            level.time + respawn * 1000;     // this client only: blocked until this future time
    } else {
        ent->perClientPickupTime[other->client->clientNum] = -1;   // this client only: never again
    }

    // 0x21109-0x21117
    SetPerClientBit(ent, other->client->clientNum);   // call 0x1629 — sets the companion bitmask bit

    return 1;   // 0x21118-0x21119
}
```

### Pickup_\* dispatch table (confirmed via the switch structure above)

| `giType` | Vanilla name | DeFRaG address | Status |
|----------|--------------|-----------------|--------|
| 1 (`IT_WEAPON`) | `Pickup_Weapon` | `0x20d2d` | Already fully decoded — [`../G_ACTIVE/func_0x20d2d.md`](../G_ACTIVE/func_0x20d2d.md). Returns `5` — now explained: **5 is the per-client respawn-seconds value** fed directly into the `respawn` variable above (matches vanilla's 5-second default weapon-respawn timing), resolving that doc's "uncertain" open note about the return value's meaning. |
| 2 (`IT_AMMO`) | `Pickup_Ammo` | `0x20ced` | Identified via switch position/case value only; not independently instruction-decoded this pass. |
| 3 (`IT_ARMOR`) | `Pickup_Armor` | `0x20ecd` | **Already confirmed** — exact hash match to vanilla (`7441a372`), the project's existing reliable anchor. |
| 4 (`IT_HEALTH`) | `Pickup_Health` | `0x20e25` | Identified via switch position/case value only; size (`0xa9`=169) differs from vanilla (`0x86`=134) — flagged as likely modified, not further decoded this pass (out of scope: no `svFlags`/`nextthink` touched regardless). |
| 5 (`IT_POWERUP`) | `Pickup_Powerup` | `0x20afa` | Identified via switch position/case value only; not independently instruction-decoded this pass. |
| 6 (`IT_HOLDABLE`) | `Pickup_Holdable` | `0x20c57` | Identified via switch position/case value only; not independently instruction-decoded this pass. |
| 8 (`IT_TEAM`) | `Pickup_Team` | `0x26e74` | Identified via switch position/case value only; not independently instruction-decoded this pass. |

None of these sub-functions were found to reference `svFlags`/`eFlags`/`nextthink` either (their
return value simply feeds `respawn`, consumed only by `Touch_Item`'s per-client cooldown write above).
This was directly confirmed for `Pickup_Weapon` (zero float/svFlags/nextthink instructions, per its own
doc) and is architecturally necessary for the other cases too, since `Touch_Item` itself is the only
place in this call graph that writes to the item entity's visibility-adjacent state, and it doesn't
write `svFlags`/`eFlags`/`nextthink` either.

---

## Key addresses

| Address | Description |
|---------|-------------|
| `0x20f11` | `Touch_Item` entry |
| `0x20f24`, `0x20f38` | Per-client cooldown gate (the two-part check against `ent+0x32c[clientNum]`) |
| `0x20f68` | `BG_CanItemBeGrabbed` call (`0x4c42`) |
| `0x20f95` | Switch on `giType` begins |
| `0x21083` | `G_UseTargets` call (`0x28151`) |
| `0x21086` | `ent->wait` (`+0x31c`) override of `respawn` |
| `0x210df`–`0x21108` | The per-client cooldown write (the core mechanism) |
| `0x21109` | Companion per-client bit set (`0x1629`) |
| `0x21118` | `return 1` |

## Open questions (do not affect the core finding)

- Exact field names/purposes of `ent+0x22c` (write at `0x210d9`) and `ent+0x320`/`ent+0x320`-driven
  block (`0x210a2`–`0x210cc`) — confirmed to exist, not confirmed semantically.
- Identity of helper calls `0x3b96`, `0x3c23` (also used in `func_0x20d2d.md`'s gate helpers'
  family), `0xac4e`, `0x5dc2`, and trap `-0x12` — none were found to touch `perClientPickupTime`,
  `svFlags`, `eFlags`, or `nextthink`, so they don't affect the visibility conclusion.
- `other->health` offset (`+0x2dc`) and `other->client->pers.predictItemPickup` offset (`+0x1f8`) are
  positionally/structurally consistent with vanilla but not independently cross-confirmed elsewhere in
  this project's documented offset tables.
- Why no `case 7` (`IT_PERSISTANT_POWERUP`) appears in the switch — not investigated (Team Arena
  persistent powerups are likely out of scope for a non-missionpack DeFRaG build, consistent with
  `G_WEAPON/README.md`'s existing MISSIONPACK-absence findings elsewhere in this QVM).

## oDFs comparison / porting notes

oDFs's current `code/game/g_items.c` `Touch_Item` (lines ~428–561) is **vanilla-shaped**: it calls
`BG_CanItemBeGrabbed`/the `Pickup_*` family in the same order, then unconditionally does
`ent->r.svFlags |= SVF_NOCLIENT; ent->s.eFlags |= EF_NODRAW; ent->r.contents = 0;` followed by
`ent->nextthink = level.time + respawn*1000; ent->think = RespawnItem;` — i.e. the classic
global-hide-plus-timer model, confirmed as the correct "vanilla baseline" to diff against. oDFs's
existing `df_mode.integer` check at line 493 only gates the global-vs-single-client *broadcast temp
entity sound* for powerup/team pickups — unrelated to the hide mechanism, and (per the cvar-table
search above) not something DeFRaG's own QVM gates by any cvar at all.

**Implemented** (verified independently against the raw bytecode — the gate at `0x20f24`/`0x20f38`
and the write at `0x210d8`-`0x21108` — before porting, not just taken on the agent's word):
1. Added `int perClientPickupTime[MAX_CLIENTS]` to `gentity_t` (`code/game/g_local.h`).
2. In `Touch_Item` (`code/game/g_items.c`): inserted the two-part gate
   (`perClientPickupTime[other->s.number] < 0` / `level.time < perClientPickupTime[...]`) right after
   the existing `other->client`/`health` checks, before `BG_CanItemBeGrabbed`.
3. Replaced the unconditional `SVF_NOCLIENT`/`EF_NODRAW`/`contents=0`/`nextthink`/`think=RespawnItem`
   block with the per-client write (`respawn > 0 ? level.time + respawn*1000 : -1`), using
   `other->s.number` as the clientNum-equivalent index (consistent with this file's existing
   `G_LogPrintf` usage a few lines above, and with `G_UseTargets`'s port using an equivalent index).
   Also removed the vanilla `wait == -1` early-hide branch and the `FL_DROPPED_ITEM`→`freeAfterEvent`
   line — both are incompatible with the per-client model (they'd hide/free the entity for everyone
   the instant one client's pickup event completes), and the confirmed decode's unified
   `respawn>0?future:-1` tail already handles the `wait==-1` case correctly on its own (producing `-1`
   for that one client, not a global hide).
4. `RespawnItem()` itself was left untouched — it has two other, unrelated callers
   (`g_team.c`'s CTF flag-return logic, `g_client.c`'s respawn-time "force all picked-up items to
   respawn" sweep) that are outside this investigation's confirmed scope. Only `Use_Item`'s body was
   changed, per [`RespawnItem.md`](RespawnItem.md)'s findings — see that doc for detail.
