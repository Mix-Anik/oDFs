# DeFRaG cgame `CG_EntityEvent` (`0x1f7e0`) — event numbering contract

**Address:** `0x1f7e0` (size `0xaa4`; hash-DIFF from vanilla, called from `0x20283`, `0x26c3d`, `0x26d49`). Its first jump table covers events `1..0x4c` (1..76) and each case body prints a debug line `"ent:%3i event:%3i "` + the `EV_*` name (`"ZEROEVENT"`, `"EV_FOOTSTEP"` …). The case numbers were extracted by script from the `case from …(0xN)` markers and the first `EV_…` string in each body:

| N | event | N | event | N | event |
|---|---|---|---|---|---|
| 1 | FOOTSTEP | 2 | FOOTSTEP_METAL | 3 | FOOTSPLASH |
| 4 | FOOTWADE | 5 | SWIM | 6 (6..9) | STEP (4/8/12/16) |
| 10 | FALL_SHORT | 11 | FALL_MEDIUM | 12 | FALL_FAR |
| 13 | JUMP_PAD | 14 | JUMP | 15–18 | WATER_TOUCH/LEAVE/UNDER/CLEAR |
| 19 | ITEM_PICKUP | 20 | GLOBAL_ITEM_PICKUP | 21 | NOAMMO |
| 22 | CHANGE_WEAPON | 23 | FIRE_WEAPON | 24–38 | USE_ITEM0..14 (39 = USE_ITEM15, no debug line) |
| 40 | ITEM_RESPAWN | 41 | ITEM_POP | 42/43 | PLAYER_TELEPORT_IN/OUT |
| 44 | GRENADE_BOUNCE | 45–47 | GENERAL/GLOBAL/GLOBAL_TEAM_SOUND | 48/49 | BULLET_HIT_FLESH/WALL |
| 50–52 | MISSILE_HIT/MISS/MISS_METAL | 53 | RAILTRAIL | 54 | SHOTGUN |
| 56 | PAIN | 57 | DEATH1 (58/59 DEATH2/3) | 60 | OBITUARY |
| 61–63 | POWERUP_QUAD/BATTLESUIT/REGEN | 64 | GIB_PLAYER | 65 | SCOREPLUM |
| 74 | DEBUG_LINE | 75 | STOPLOOPINGSOUND | 76 | TAUNT |

## Comparison with oDFs (`bg_public.h` `entity_event_t`)
Enumerated by script (names in declaration order, 83 events, no `#if` inside the enum): **every checked event (48 of 48 names, `STEP` = `EV_STEP_4` family) has the same numeric value in oDFs as in the DeFRaG cgame.** The DeFRaG enum equals the ioq3 1.32 one including `EV_USE_ITEM15` (39) and the MISSIONPACK block (`EV_PROXIMITY_MINE_*` … `EV_LIGHTNINGBOLT`, 65..72 shifted by the +1 from item 39 — which is what places `EV_DEBUG_LINE` at 74 here, i.e. DEBUG_LINE=74/STOPLOOPINGSOUND=75/TAUNT=76 in both).
Conclusion: **no event-number mismatch** between oDFs qagame and the original DeFRaG cgame; `G_AddEvent`/`ps.events[]` values are interpreted correctly. Events above `EV_TAUNT` (76, voice-taunt family) are not in the first jump table and were not checked.
