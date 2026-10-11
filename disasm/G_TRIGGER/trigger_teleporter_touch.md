# `trigger_teleporter_touch` — DeFRaG addition: `df_ndm_noteleport` gate

**Address:** `0x27bf9` (DeFRaG), vanilla `0x1f0da`-ish region (per
`baseq3-game-functions.hmap`'s surrounding entries)
**Size:** `0xe5` (229 bytes), no vanilla hash match (confirmed modified)
**Signature:** `(gentity_t *self, gentity_t *other, trace_t *trace)`

Found immediately after confirming
[`trigger_push_touch`](trigger_push_touch.md)'s `df_ndm_nopad` gate — this is
the direct parallel for teleporters, using the sibling cvar
`df_ndm_noteleport` (`0x131834`).

## Decode

Up through the vanilla "spectator-only teleporter" check
(`if ((self->spawnflags&1) && other->client->sess.sessionTeam != TEAM_SPECTATOR) return;`),
the function is byte-for-byte vanilla-equivalent (confirmed: `pm_type==PM_DEAD`
check, `self->spawnflags&1`, `sessionTeam+0x284`, all matching oDFs's own
existing code exactly). Right after that check, DeFRaG inserts:

```c
if ( *(int*)0x131834 == 1 ) {
    return;                      // teleporter completely inert
}
if ( *(int*)0x131834 != 0 ) {
    G_Damage( other, self, self, NULL, NULL, 9999, DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT );
    return;                      // kill INSTEAD of teleporting — no TeleportPlayer call follows
}
// *(0x131834) == 0: fall through to the normal G_PickTarget/TeleportPlayer logic
```

**Different from `trigger_push_touch` in one respect**: there, the kill
happens *in addition to* the normal jump-pad push (both run). Here, the kill
*replaces* the teleport — confirmed by the bytecode jumping straight to the
function epilogue after the `G_Damage` call, never reaching the
`G_PickTarget`/`TeleportPlayer` code below it. There is also a third state
(`==1`) with no vanilla parallel in `trigger_push_touch`: it returns
immediately, doing neither the kill nor the teleport — the trigger becomes
entirely inert.

`0x131834` = cached integer value of cvar **`df_ndm_noteleport`** —
confirmed the same way as `df_ndm_nopad`: `G_InitRaceMode` reads it via
`trap_Cvar_VariableIntegerValue("df_ndm_noteleport")` (string literal
`0x40d7`, trap `-7`) and stores the result directly into `0x131834`, no
transformation. See [`func_0x1e60.md`](func_0x1e60.md). Default could not be
read directly from the string pool (no adjacent numeric literal); inferred
as `"0"` by the same reasoning as `df_ndm_nopad`/`df_ndm_damage` (teleporters
must work normally on an ordinary map).

## Semantics

- `df_ndm_noteleport == 0` (default): normal teleporter behavior.
- `== 1`: teleporter is present but does nothing when touched (no teleport,
  no damage).
- `>= 2` (or negative): touching the teleporter instantly kills the player
  instead of teleporting them.

Same cvar-name family as `df_ndm_nopad`/`df_ndm_damage`/`df_ndm_weaponsSet`
(all confirmed `df_ndm_*` strings in the registration pool, see
`func_0x1e60.md`) — almost certainly part of the same "no damage mode"
per-map restriction suite (forbid teleporter shortcuts for certain race/trick
categories, same as forbidding jump pads).

## Implemented

`code/game/g_trigger.c`'s `trigger_teleporter_touch`: added the two-state
gate (inert at `1`, instant-kill at `>=2`) exactly as decoded, right after
the existing vanilla spectator-only check. New cvar `df_ndm_noteleport`
registered in `code/game/g_main.c` (default `"0"`, `CVAR_SERVERINFO`).
