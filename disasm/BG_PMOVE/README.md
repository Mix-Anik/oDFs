# BG_PMOVE — Defrag QVM disassembly analysis

Analysis of movement functions from `bg_pmove.c` in the Defrag qagame.qvm.

For general disassembly methodology, struct layouts, and global addresses see [../README.md](../README.md).

---

## Source file notes

All functions in this folder are from `bg_pmove.c` unless noted otherwise:
- **`func_0x2575`** — reads the `df_promode` cvar; address `0x2575` is below the bg_pmove.c range (`0x6000+`), suggesting it is from a DeFRaG-specific source file. Kept here because its sole caller is Pmove.
- **PM_SlideMove / PM_StepSlideMove / func_0x1a0b** — moved to [../BG_SLIDEMOVE/](../BG_SLIDEMOVE/) as they originate from `bg_slidemove.c`.

---

## Match status

Functions verified against disassembly and implemented in oDFs:

| Function | Address | Status |
|----------|---------|--------|
| PM_ClipVelocity | `0x6667` | ✅ Full match |
| PM_Friction | `0x66e0` | ✅ Full match — CPM water friction 0.5f added |
| PM_Accelerate | `0x67f2` | ✅ Full match |
| PM_CmdScale | `0x686e` | ✅ Full match |
| PM_SetMovementDir | `0x6912` | ✅ Full match |
| PM_CheckJump | `0x6a5e` | ✅ Full match — CPM double jump + 270 velocity |
| PM_CheckWaterJump | `0x6b40` | ✅ Full match |
| PM_WaterJumpMove | `0x6c27` | ✅ Full match |
| PM_WaterMove | `0x6c68` | ✅ Full match — CPM water caps 0.585/0.540, accel 5.0 |
| PM_NoclipMove | `0x7544` | ✅ Full match |
| PM_FlyMove | `0x6de6` | ✅ Full match |
| PM_AirMove | `0x6e71` | ✅ Full match — CPM air accel 70.0, wishspeed cap 30 |
| CPM_PM_Aircontrol | `0x1a84` | ✅ Full match — DeFRaG dot ≤ 0 guard added |
| PM_WalkMove | `0x726e` | ✅ Full match — CPM ground accel 15.0 |
| PM_DeadMove | `0x74cb` | ✅ Full match (fixed) — airborne-only decay |
| PM_FootstepForSurface | `0x7698` | ✅ Full match |
| PM_CrashLand | `0x76b1` | ✅ Full match — 0.0001f constant, thresholds 7/40/60 |
| PM_CorrectAllSolid | `0x77ae` | ✅ Full match |
| PM_GroundTraceMissed | `0x7895` | ✅ Full match |
| PM_GroundTrace | `0x792e` | ✅ Full match — OBS kill + landing ClipVelocity added |
| PM_SetWaterLevel | `0x7ae8` | ✅ Full match |
| PM_CheckDuck | `0x7bb5` | ✅ Full match — all box constants confirmed |
| PM_Footsteps | `0x7d06` | ✅ Full match |
| PM_WaterEvents | `0x7e78` | ✅ Full match |
| PM_Weapon | `0x7fb8` | ✅ Full match — WP_GRAPPLING_HOOK guard added; gauntlet check (pm+0x2c) resolved — always 0, see below |
| PM_TorsoAnimation | `0x7f99` | ✅ Full match |
| PM_Animate | `0x81de` | ✅ Full match |
| PM_DropTimers | `0x8203` | ✅ Full match — doublejump countdown inline in PmoveSingle |
| PM_SlideMove | `0x8709` | ✅ Full match — see [BG_SLIDEMOVE](../BG_SLIDEMOVE/) |
| PM_StepSlideMove | `0x8af9` | ✅ Full match — CPM ramp-jump — see [BG_SLIDEMOVE](../BG_SLIDEMOVE/) |
| PM_AddEvent | `0x655d` | ✅ Full match |
| PM_AddTouchEnt | `0x656c` | ✅ Full match |
| PM_StartTorsoAnim | `0x65c1` | ✅ Full match |
| PM_StartLegsAnim | `0x65e7` | ✅ Full match |
| PM_ContinueLegsAnim | `0x6617` | ✅ Full match |
| PM_ContinueTorsoAnim | `0x6637` | ✅ Full match |
| PM_ForceLegsAnim | `0x6657` | ✅ Full match |
| PmoveSingle / Pmove | `0x8331` / `0x1a866` | ✅ Full match — Pmove adds pmove_msec validation (8–33); all PmoveSingle call targets now identified (see below) |
| func_0x4152 (stats[13] input bitfield) | `0x4152` | ✅ Fully decoded — non-physics, see [PmoveSingle.md](PmoveSingle.md) |
| func_0x828d = **PM_UpdateViewAngles** | `0x828d` | ✅ Full match — one DeFRaG pm_type addition, no velocity effect |
| func_0xa186 = **AngleVectors** | `0xa186` | ✅ Full match — generic utility, 23 call sites QVM-wide |

---

## Function index

| File | Address | Summary |
|------|---------|---------|
| [PM_AnimationHelpers.md](PM_AnimationHelpers.md) | `0x655d`–`0x6657` | PM_AddEvent, PM_AddTouchEnt, StartTorsoAnim, StartLegsAnim, ContinueLegsAnim, ContinueTorsoAnim, ForceLegsAnim |
| [PM_ClipVelocity.md](PM_ClipVelocity.md) | `0x6667` | Clip velocity against plane |
| [PM_Friction.md](PM_Friction.md) | `0x66e0` | CPM water friction 0.5f |
| [PM_Accelerate.md](PM_Accelerate.md) | `0x67f2` | Standard acceleration |
| [PM_CmdScale.md](PM_CmdScale.md) | `0x686e` | Cmd-to-speed scale factor |
| [PM_SetMovementDir.md](PM_SetMovementDir.md) | `0x6912` | 8-direction movementDir |
| [PM_CheckJump.md](PM_CheckJump.md) | `0x6a5e` | CPM double jump |
| [PM_CheckWaterJump.md](PM_CheckWaterJump.md) | `0x6b40` | Water jump detection |
| [PM_WaterJumpMove.md](PM_WaterJumpMove.md) | `0x6c27` | Water jump movement |
| [PM_WaterMove.md](PM_WaterMove.md) | `0x6c68` | CPM water movement |
| [PM_NoclipMove.md](PM_NoclipMove.md) | `0x7544` | Noclip movement |
| [PM_FlyMove.md](PM_FlyMove.md) | `0x6de6` | Flight powerup movement |
| [PM_AirMove.md](PM_AirMove.md) | `0x6e71` | CPM air movement |
| [CPM_PM_Aircontrol.md](CPM_PM_Aircontrol.md) | `0x1a84` | CPM air control with dot ≤ 0 guard |
| [PM_WalkMove.md](PM_WalkMove.md) | `0x726e` | CPM ground movement |
| [PM_DeadMove.md](PM_DeadMove.md) | `0x74cb` | Dead player velocity decay |
| [PM_FootstepForSurface.md](PM_FootstepForSurface.md) | `0x7698` | Surface footstep event |
| [PM_CrashLand.md](PM_CrashLand.md) | `0x76b1` | Fall damage |
| [PM_CorrectAllSolid.md](PM_CorrectAllSolid.md) | `0x77ae` | Unstuck from geometry |
| [PM_GroundTraceMissed.md](PM_GroundTraceMissed.md) | `0x7895` | Leaving-ground transition |
| [PM_GroundTrace.md](PM_GroundTrace.md) | `0x792e` | Ground detection + OBS kill |
| [PM_SetWaterLevel.md](PM_SetWaterLevel.md) | `0x7ae8` | Water level detection |
| [PM_CheckDuck.md](PM_CheckDuck.md) | `0x7bb5` | Duck/stand-up + bounding box |
| [PM_Footsteps.md](PM_Footsteps.md) | `0x7d06` | Leg anims + bob cycle + sound events |
| [PM_WaterEvents.md](PM_WaterEvents.md) | `0x7e78` | Water enter/leave sound events |
| [PM_Weapon.md](PM_Weapon.md) | `0x7fb8` | CPM weapon switching |
| [PM_TorsoAnimation.md](PM_TorsoAnimation.md) | `0x7f99` | Idle torso animation dispatch |
| [PM_Animate.md](PM_Animate.md) | `0x81de` | Gesture/taunt animation |
| [PM_DropTimers.md](PM_DropTimers.md) | `0x8203` | Timer countdown |
| [PmoveSingle.md](PmoveSingle.md) | `0x8331` / `0x1a866` | Pmove frame dispatch |
| [func_0x2575.md](func_0x2575.md) | `0x2575` | Promode flag setter (source file uncertain) |
| *PM_SlideMove* | `0x8709` | → [BG_SLIDEMOVE/PM_SlideMove.md](../BG_SLIDEMOVE/PM_SlideMove.md) |
| *PM_StepSlideMove* | `0x8af9` | → [BG_SLIDEMOVE/PM_StepSlideMove.md](../BG_SLIDEMOVE/PM_StepSlideMove.md) |

---

## Key findings

- `pm_flags & 0x8000` = **promode enabled** — set every frame by func_0x2575, not a per-jump flag
- Double jump has **no one-time guard** in PM_CheckJump — only `PMF_JUMP_HELD` prevents spamming
- `ps->stats[10]` = doublejump window (400ms); serves double duty as timer AND ramp-jump gate
- `ps->stats[11]` = bonus-used marker for HUD/scoring, does not gate any physics
- `EV_JUMP = 0xe` (14) in this Defrag build
- `JUMP_VELOCITY = 270.0f` (`0x43870000`)
- **CPM ramp jump**: PM_StepSlideMove allows step-up onto ramp face mid-air when `stats[10] > 0` AND `velocity[2] <= 370`
- **OBS kill**: in PM_GroundTrace — triggered by `SURF_NOOB (0x80000)` OR `ps->stats[12] & 0x200`; only on exactly flat surface (`normal[2] == 1.0f`)
- **SURF_NOOB (0x80000)**: DeFRaG-specific per-face OBS disable flag
- **CPM_PM_Aircontrol dot guard**: DeFRaG adds `if (dot <= 0) skip` (0x1ae7); reference source lacks this
- **CPM water friction**: 0.5f in promode, 1.0f stock
- **CPM instant weapon switch**: no weaponTime delay in promode (+200ms drop / +250ms raise skipped)
- **WP_GRAPPLING_HOOK blocked**: BeginWeaponChange (0x7eef) returns early for weapon == 10
- **Gauntlet check resolved**: PM_Weapon reads `pm + 0x2c` for gauntlet check; pm struct is memset to 0 every frame in `func_0x1a394` and no code path ever writes `pm+0x2c` (no `CheckGauntletAttack` call exists) — the read is always 0 regardless of which field oDFs's layout puts there, so gauntlet always fires in continuous/animation-only mode. oDFs already matches (never sets `pm.gauntletHit`). See [../G_ACTIVE/func_0x1a394.md](../G_ACTIVE/func_0x1a394.md#resolved-gauntlethit-always-0) and [PM_Weapon.md](PM_Weapon.md#gauntlet-check-0x80ec--resolved)
- **Movement dispatch order**: FlyMove → PMF_GRAPPLE_PULL(grapple mode) → PMF_TIME_WATERJUMP(WaterJumpMove) → waterlevel>1(WaterMove) → walking(WalkMove) → AirMove — matches oDFs for cases 3–6
- **0x74cb misidentification corrected**: is PM_DeadMove (not PM_DropTimers); 0x7544 is PM_NoclipMove (not PM_DropTimers)
- **All physics globals confirmed**: stopspeed=100, duckScale=0.25, swimScale=0.5, accelerate=10, airaccelerate=1, wateraccelerate=4, flyaccelerate=8, friction=6, waterfriction=1, flightfriction=3, spectatorfriction=5
- **Exhaustive promode audit**: all 11 `pm_flags & 0x8000` checks in movement range verified; no hidden CPM modifications
- **Pmove outer (0x1a866)**: adds pmove_msec bounds check (8–33) and calls func_0x2575 before subdivision loop
- **PM_DeadMove fix**: DeFRaG decays airborne dead-player speed; oDFs had inverted walking guard — fixed

---

## Unresolved

| Issue | Details |
|-------|---------|
| stats[12] & 0x200 origin | ✅ Resolved (see [../G_ACTIVE/README.md](../G_ACTIVE/README.md)) — set by func_0x426b called from func_0x421b, called from ClientThink_real |
| func_0x2575 source file | Address 0x2575 is outside bg_pmove.c range; arg0 confirmed as `client` pointer (not `pm`) but this doesn't pin down the compilation unit; likely DeFRaG-specific file — no further evidence available, not behavior-relevant |
| `0x828d` identity (called from PmoveSingle, step 5) | ✅ Resolved — **is `PM_UpdateViewAngles`** (prior "likely PM_SetWaterLevel" label was wrong). Full decode: [PmoveSingle.md#func_0x828d--pm_updateviewangles](PmoveSingle.md#func_0x828d--pm_updateviewangles). One confirmed DeFRaG addition: early-return gate checks pm_type-like field against two values (5 **and** 6) vs vanilla's one (5=PM_INTERMISSION); the extra type-6 case isn't named. No velocity/knockback effect. |
| `0xa186` identity (called from PmoveSingle, step 6) | ✅ Resolved — **is `AngleVectors`** (prior "likely PM_CheckDuck" label was wrong). Full decode: [PmoveSingle.md#func_0xa186--anglevectors](PmoveSingle.md#func_0xa186--anglevectors). 23 call sites across the whole QVM. No CPM/DeFRaG modification found. |
| `0x4152` identity (called from PmoveSingle, step 2) | ✅ Resolved — **not `PM_UpdateViewAngles`** (prior "likely" label was wrong, by position only). Builds `ps->stats[13]`, the already-documented "DeFRaG input state bitfield (non-physics)", from the signs/magnitude of `cmd.forwardmove`/`rightmove`/`upmove`. Full decode, including its two sub-calls (`func_0x4122`, `func_0x6061`=generic `abs()`): [PmoveSingle.md#func_0x4152--stats13-input-bitfield-builder](PmoveSingle.md#func_0x4152--stats13-input-bitfield-builder). One field (`pm+0x14`) and one magic constant (`64` in `func_0x4122`) remain unnamed — flagged, not guessed. Write-only, never read back in `bg_pmove.c`; no velocity/knockback effect. |
| Step-14 call order in PmoveSingle | `PM_Weapon (0x7fb8)`, `PM_TorsoAnimation (0x7f99)`, `PM_Footsteps (0x7d06)`, `PM_WaterEvents (0x7e78)` are each individually confirmed correct (own table rows above), but their **relative call order** within PmoveSingle was never pinned down — cosmetic/non-behavioral unless call order itself matters (not expected to, each is self-contained). |

Gauntlet check, stats[12]&0x200 origin, and all three previously-mislabeled `PmoveSingle` call targets (`0x828d`, `0xa186`, `0x4152`) are now resolved. Every function called from anywhere in `bg_pmove.c`'s reachable graph has been identified. Remaining open items in this table are either genuinely inert (func_0x2575's source file) or cosmetic (step-14 ordering, two unnamed minor sub-details inside `func_0x4152`/`func_0x828d` noted above) — none affect movement/velocity/knockback behavior.
