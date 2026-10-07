# RespawnItem — does not exist standalone; merged into the `ent->use` callback

**Vanilla reference:** `RespawnItem`, `0x194ee` in `baseq3-game-functions.hmap`, hash `60a2eb9d`,
size `0x138` (312 instructions). **Confirmed genuinely absent** — a full-file grep for the hash
`60a2eb9d` across the entire `qagame.dis` (all ~290k lines, not just the item-cluster region) returns
**zero matches**. This is not a hash-table lookup gap; the exact instruction shape simply does not
exist anywhere in this QVM.

**Why it's gone:** [`Touch_Item.md`](Touch_Item.md) establishes that DeFRaG's `Touch_Item`
(`0x20f11`) never hides the item entity (no `svFlags`/`eFlags`/`contents`/`nextthink` writes) —
pickup eligibility is instead gated by a new per-client cooldown array (`ent+0x32c`). Since nothing
ever schedules `ent->think = RespawnItem` / `ent->nextthink = <future>` for the automatic-respawn
case any more, vanilla's `RespawnItem` (whose entire job was to be that scheduled `think` callback,
re-showing a globally-hidden item) has no remaining caller for that purpose and was removed as a
standalone function.

**What DeFRaG kept of it:** vanilla's `Use_Item` was always a 1-line wrapper
(`void Use_Item(gentity_t *ent, gentity_t *other, gentity_t *activator) { RespawnItem(ent); }` — hash
`-480139be`, size `0x9` = 9 instructions, confirmed present and unmodified in DeFRaG at several
*other* 9-10-instruction addresses, e.g. `0x1d94c`, `0x21840`, `0x2185b`, `0x219ae`, `0x22877` — these
are presumably other, unrelated trivial 9-instruction functions sharing the same hash collision, not
necessarily item-related; not investigated further here). **DeFRaG's actual `ent->use` callback for
item entities is a different, much larger function that inlines `RespawnItem`'s body directly and
reworks it to be per-activator-scoped**, rather than calling a separate `RespawnItem`.

---

## `0x21249` — the real `ent->use` handler for items (RespawnItem body, inlined and reworked)

**Address:** `0x21249`
**Size:** `0x5f` per hash table (effective body ~`0x5e` instructions, same drift pattern as elsewhere).
**Hash:** `-5ae461a2` — does not match vanilla `Use_Item`'s hash (expected: a 9-instruction wrapper
would never hash-match a 95-instruction function anyway). Not independently hash-matched to anything
in `baseq3-game-functions.hmap`.

**Confirmed identity:** `FinishSpawningItem` (`0x212a7`) writes this exact address into
`gentity_t.use` (`ent+0x2c4`) at instruction `0x212fa` — the same assignment site that, two
instructions earlier (`0x212f4`), writes `Touch_Item`'s address (`0x20f11`) into `gentity_t.touch`
(`ent+0x2c0`). This is vanilla's exact `FinishSpawningItem` pattern:
```c
ent->touch = Touch_Item;
ent->use   = Use_Item;      // "useing an item causes it to respawn"
```
confirming both `0x20f11` and `0x21249` simultaneously (two sibling field-offset assignments 4 bytes
apart, in a function already content-confirmed as `FinishSpawningItem` — see below).

### Decoded body

```c
// arg0 = ent (the item entity), arg2 = activator (the entity that triggered the `use`)
int func_0x21249(gentity_t *ent, gentity_t *other /*unused*/, gentity_t *activator) {

    if (func_0x3c43(ent) != 0)   // gate: ent->item==NULL, or itemFlags(+0x218)&0x1000000 set,
        return 1;                // or a func_0x3c23(ent->item) check fails -> skip entirely

    // --- vanilla RespawnItem's body, confirmed field-for-field: ---
    ent->(0x1cc) = 2.0f;                       // field purpose/name not independently confirmed
    *(ent+0x8)  &= ~0x80;                      // ent->s.eFlags &= ~EF_NODRAW   (confirmed: entityState_t
                                                //   layout number@0/eType@4/eFlags@8, EF_NODRAW=0x80
                                                //   per code/game/bg_public.h)
    *(ent+0x1a8) &= ~0x1;                       // ent->r.svFlags &= ~SVF_NOCLIENT  (confirmed:
                                                //   r.svFlags @ +0x1a8 per disasm/README.md;
                                                //   SVF_NOCLIENT=0x1 per code/game/g_public.h)
    trap(-0x1f, ent);                           // trap_LinkEntity-shaped call (exact trap name not
                                                //   independently re-verified against g_syscalls.asm
                                                //   this pass, but matches vanilla RespawnItem's
                                                //   trap_LinkEntity(ent) call position exactly)
    ent->(0x2b0) = 0;                           // ent->nextthink = 0   (confirmed: nextthink @ +0x2b0
                                                //   per disasm/README.md) — matches vanilla
                                                //   RespawnItem's closing `ent->nextthink = 0;`

    // --- NEW: per-activator-scoped reset, replacing vanilla's global G_AddEvent(ent,EV_ITEM_RESPAWN,0) ---
    ent->perClientPickupTime[activator->client->clientNum] = 0;   // ent+0x32c+clientNum*4 = 0
                                                                     // (clientNum = activator->client+0x8c)
    ClearPerClientBit(ent, activator->client->clientNum);           // call 0x15fa — clears the
                                                                     // companion bitmask bit (ent+0x54/0x58)
                                                                     // set by Touch_Item/FinishSpawningItem
    return 1;
}
```

**This is a near-exact transplant of vanilla `RespawnItem`'s body**, confirmed via the two bit-clear
operations resolving to exactly `EF_NODRAW` and `SVF_NOCLIENT` (the two-s'-complement-constant pitfall
was checked explicitly: `-0x81` as a 32-bit AND-mask is `0xFFFFFF7F`, which clears only bit `0x80`;
`-0x2` is `0xFFFFFFFE`, which clears only bit `0x1` — **not** "bits 0x80+0x1" or "bit 0x2" as a naive
read of the hex literals might suggest) — plus `ent->nextthink = 0` in the same relative position as
vanilla. The **only** substantive change is that the vanilla global
`G_AddEvent(ent, EV_ITEM_RESPAWN, 0)` broadcast-to-everyone call is gone, replaced by resetting just
the *activator's* personal cooldown slot and companion bit back to their defaults — i.e. this is no
longer "make the item visible to everyone again", it's "let this one activator try picking it up
again immediately", consistent with the rest of the per-client model in [`Touch_Item.md`](Touch_Item.md).

**Practical effect:** this function only runs when something explicitly `use`s the item entity by
`targetname` (e.g. a `target_give`/trigger chain referencing the item) — the same trigger condition
as vanilla's `Use_Item`. Since `Touch_Item` already does its own per-client cooldown bookkeeping on
every normal touch, this `use` callback is a **map-scripting hook for forcing one specific activator's
cooldown to reset early**, not a general respawn-scheduling mechanism (there is no `nextthink`-driven
automatic path left in this build — confirmed by `Touch_Item`'s own analysis).

**Is `ent->nextthink`/the `EF_NODRAW`/`SVF_NOCLIENT` clearing now dead code?** Likely **mostly
vestigial** in the sense that nothing was found anywhere in the traced call graph
(`Touch_Item`, `LaunchItem`, `Drop_Item`, `FinishSpawningItem`, the `Pickup_*` family) that ever
**sets** `EF_NODRAW`, `SVF_NOCLIENT`, or a non-zero `nextthink` on an item entity in the first place —
so clearing them here is a no-op under normal play. This was **not exhaustively proven** across the
*entire* QVM (only the item-pickup cluster `0x20c57`–`0x2131f` plus `LaunchItem`/`Drop_Item`/
`FinishSpawningItem` was read in full); it remains possible some other, unrelated code path (e.g. a
map's trigger-controlled item, or team/CTF flag logic) still sets these on an item entity, in which
case this clearing would be meaningful for that path. Flagged as **not fully ruled out**, per the
no-speculation rule — but confirmed **not** reachable from ordinary player touch/pickup.

---

## `0x212a7` — `FinishSpawningItem` (supporting identification)

**Address:** `0x212a7`, size `0x160` (352 instructions) vs. vanilla `FinishSpawningItem`
(`0x19995`, size `0x157` = 343 instructions) — close size match, consistent with a lightly modified
function (not independently hash-matched; vanilla's hash `422e7a79` was not found in DeFRaG's hash
table at this address or any other, confirming some modification exists, though the exact delta was
not isolated this pass).

Confirmed via content: sets `ent->s.eType = 2` (`ET_ITEM`, `ent+0x4`), `ent->r.mins`/`ent->r.maxs` to
±`ITEM_RADIUS` (`ent+0x1b4..0x1c8`), `ent->s.modelindex = (ent->item - bg_itemlist) / sizeof(gitem_t)`
(`ent+0xa0`, divisor `0x34` = 52 bytes per `gitem_t`, matching `ent->item` pointer arithmetic),
`ent->s.modelindex2 = 0` (`ent+0xa4`, "not a dropped item" marker), and — critically — the
`ent->touch = Touch_Item (0x20f11)` / `ent->use = <0x21249> ` assignments documented above. All of
this matches vanilla `FinishSpawningItem`'s opening block field-for-field.

---

## Key addresses

| Address | Description |
|---------|-------------|
| `0x21249` | `ent->use` handler (RespawnItem body, inlined + per-activator-scoped) |
| `0x212a7` | `FinishSpawningItem` |
| `0x212f4` | `ent->touch = 0x20f11` (Touch_Item) written here |
| `0x212fa` | `ent->use = 0x21249` written here |
| `0x21177` | `ent->touch = 0x20f11` also written in `LaunchItem` (`0x2111a`) |

## oDFs comparison / porting notes

oDFs's current `RespawnItem` (`code/game/g_items.c:350-420`) is the full vanilla global-respawn
function (team-item random-pick logic, `SVF_NOCLIENT`/`EF_NODRAW` clear, `df_mode`-gated broadcast
sound, `nextthink=0`), still wired as `ent->think = RespawnItem` from `Touch_Item`
(`g_items.c:558`) and as the `Use_Item` target (`g_items.c:647`, `ent->use = Use_Item` at `:672` with
`Use_Item` itself calling `RespawnItem(ent)` per vanilla). This is the correct vanilla baseline.

**Implemented.** `RespawnItem`'s automatic/`think`-driven role is now unnecessary (`Touch_Item` no
longer schedules it — see [`Touch_Item.md`](Touch_Item.md)). `RespawnItem()` itself was left
unmodified, since it has two unrelated callers outside this investigation's scope
(`g_team.c`'s CTF flag-return logic, `g_client.c`'s `ClientRespawn` "force all picked-up items to
respawn" sweep). Only `Use_Item` (`code/game/g_items.c`) was reworked, matching DeFRaG's `ent->use`
handler: it now resets just the activator's own cooldown slot
(`ent->perClientPickupTime[activator->s.number] = 0`) instead of calling `RespawnItem(ent)`. The
vanilla `EF_NODRAW`/`SVF_NOCLIENT`/`nextthink` clears were not ported into `Use_Item` — under the new
model nothing ever sets those on an item touched via the normal pickup path, so clearing them there
would be a no-op (matching this doc's "mostly vestigial" finding above).
