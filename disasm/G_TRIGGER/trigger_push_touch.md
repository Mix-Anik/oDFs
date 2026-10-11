# `trigger_push_touch` — DeFRaG addition: `df_ndm_nopad` instant-kill gate

**Address:** `0x278ef` (DeFRaG), vanilla `0x1f161`
**Size:** `0x42` (66 bytes) vs. vanilla's much smaller original (no hash
match — confirmed modified)
**Signature:** `(gentity_t *self, gentity_t *other, trace_t *trace)` — only
`self`/`other` referenced (`local 0x38`="arg0"=`self`, `local 0x3c`="arg1"=
`other`; `trace` unused, matching vanilla)

Found while tracing `trigger_push_velocity`'s spawn side (see
[`../BG_MISC/BG_TouchJumpPad.md`](../BG_MISC/BG_TouchJumpPad.md)) — this
function is shared by **both** `SP_trigger_push` and
`SP_trigger_push_velocity` (both set `self->touch = trigger_push_touch`), so
the addition below applies to every jump pad, not just the custom-physics
ones.

## Decode

```c
void trigger_push_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
    if ( !other->client )                 // other+0x204, confirmed client ptr offset
        return;

    if ( *(int*)0x131830 != 1 && *(int*)0x131830 != 0 ) {
        G_Damage( other, self, self, NULL, NULL, 9999, 8/*DAMAGE_NO_PROTECTION*/, 22/*MOD_TRIGGER_HURT*/ );
    }

    BG_TouchJumpPad( &other->client->ps, &self->s );
}
```

Confirmed via:
- `func_0x2015b` is **`G_Damage`** — already identified and hash-confirmed
  in [`../G_COMBAT/G_Damage.md`](../G_COMBAT/G_Damage.md) (`0x2015b` in
  DeFRaG, vanilla `0x18b18`). The 8-argument call shape
  (`other, self, self, 0, 0, 9999, 8, 22`) matches `G_Damage`'s
  `(targ, inflictor, attacker, dir, point, damage, dflags, mod)` signature
  exactly.
- `8` = `DAMAGE_NO_PROTECTION` (confirmed, `code/game/g_local.h:545`).
- `22` = `MOD_TRIGGER_HURT` (confirmed by enum position — `MOD_UNKNOWN`=0
  through `MOD_TARGET_LASER`=21, `MOD_TRIGGER_HURT`=22 — exact match,
  `code/game/bg_public.h`).
- `0x131830` = the cached integer value of cvar **`df_ndm_nopad`** —
  confirmed directly: `G_InitRaceMode` (`func_0x1e60`, called once from
  `G_InitGame`) reads it via `trap_Cvar_VariableIntegerValue("df_ndm_nopad")`
  (string literal at `0x40e9`, trap `-7`) and stores the result straight into
  `0x131830` — see the updated
  [`func_0x1e60.md`](func_0x1e60.md#df_ndm_-cvar-cluster-resolved). The
  cvar's own registered default could not be read directly from the string
  pool (no distinct numeric literal sits adjacent to `df_ndm_nopad`'s own
  name entry, meaning it most likely reuses a dedup'd `"0"` literal stored
  elsewhere) — `0` is also the only value consistent with jump pads working
  normally by default on every ordinary map, so it is used as the
  registered default with high confidence.

## Semantics

- `df_ndm_nopad == 0` (default) or `== 1`: no change — jump pad behaves
  normally (goes straight to `BG_TouchJumpPad`).
- `df_ndm_nopad >= 2` (or negative): touching **any** jump pad (plain
  `trigger_push` or `trigger_push_velocity`) instantly deals 9999 damage,
  `DAMAGE_NO_PROTECTION`, `MOD_TRIGGER_HURT` to the touching client — before
  still running `BG_TouchJumpPad` (so if the 9999 damage didn't kill them,
  e.g. a mod intercepting damage, they'd still get pushed). In practice 9999
  damage is always lethal, so this is effectively "replace the jump pad with
  an instant-death trigger."
- Why `1` is excluded from the kill condition (rather than just gating on
  `!= 0`) is not established — flagged, not guessed. Plausibly `1` is
  reserved for a different "disabled" semantic elsewhere, or is just
  defensive coding; the cvar's real-world range of used values is unknown
  beyond this project's reach into the bytecode.

Given the cvar name (`df_ndm_*` = the same "no damage mode" restriction
family as `df_ndm_damage`/`df_ndm_weaponsSet`/`df_ndm_timer_*`/
`df_ndm_nomover`/`df_ndm_noteleport`/`df_ndm_itemsSpawn`/
`df_ndm_weaponsPickup`/`df_ndm_powerups`, all confirmed present in the
string table at `0x2f00`–`0x2fb1`), this is almost certainly a per-map
restriction flag intended for "no jump pad" race/trick categories, forcing
players who rely on movement tech instead of pads. Not confirmed beyond the
name and the mechanism itself.

## Implemented

`code/game/g_trigger.c`'s `trigger_push_touch`: added the
`df_ndm_nopad`-gated `G_Damage` call exactly as decoded above. New cvar
`df_ndm_nopad` registered in `code/game/g_main.c` (default `"0"`,
`CVAR_SERVERINFO`, matching the `df_ndm_timer_*` sibling family's flags).
