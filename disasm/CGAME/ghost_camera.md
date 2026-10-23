# DeFRaG ghost "play camera" — `0x12a10` (miniview / camera view set-up) — decode pass 1 (iter 125)

Status: **structure decoded, NOT ported.** This is the consumer of `df_ghost1_PlayCameraMode` / `df_ghost2_PlayCameraMode` (copied into the slot at `+312216` by `0x10c29`, clamped by `0x10dab`; no other reader) and of the 9 `df_ghosts_Miniview*` cvars (not yet traced). It is called from the ghost playback driver `0x127af` as `0x12a10(slot, ctx, time, idxA, idxB, mode)` when `mode != 0` and nothing else claimed the view this frame (see ghost_system.md Iter 77).

## Arguments / locals
`le0` = slot base (frames at `slot+180`, stride **624**; per frame: `+4` origin, `+16` angles, `+28` weapon/state word, `+136` crouch flag, `+140` teleport flag, `+144..` ten missile records of 32 B: `+24` id, `+28` type), `le4` = 36-byte render context (zeroed by `0x12934`, `+28=-1`), `le8` = time, `lec/lf0` = the two frame indices being interpolated, `lf4` = mode (0..17).
Context fields written here: `+4`, `+8` (two "view claimed / draw" flags), `+12` (third-person), `+16` (missile found), `+20` (scan missiles first→last), `+24` (follow missile angles), `+28` (chosen missile index), `+32` = mode.

## Helpers (all read)
- `0x11e15(slot,time,A,B)`: interpolation fraction `(time - frame[A].time - slot.base[+312184]) / (frame[B].time - frame[A].time)` (with the play-timescale `+312204` and `1221244` special case); 0 when A==B or a frame is empty.
- `0x11fa2(slot,A,B)`: 1 if either frame has the teleport flag (`+140`) set.
- `0x11fd8(slot,time,A,B,&pos,&ang)`: pos/ang = frame[B] origin/angles; if no teleport flag: `pos = lerp(A.origin,B.origin,frac)` (`0x81f9`), `ang = lerpAngles(A.angles,B.angles,frac)` (`0x822c`, per component `0x31219` = LerpAngle).
- `0x7b8c(pos,ang,wantAxis)`: `cg.refdef.vieworg = pos; cg.refdefViewAngles = ang; (AnglesToAxis(viewAngles,viewaxis) if wantAxis)`.
- `0x128fc(slot, vieworg)`: `vieworg[2] += 26` (data 8372) and `-= 14` (data 8376) when the current frame has the crouch flag (`+136`) — i.e. eye height.
- `0x128c5(&saved)`: demo playback only: copies the saved angles to `cg.refdefViewAngles`.
- `0x128d6(mode)`: sets `958772` (third-person state): modes {1,2,5,6,9,10} → 0, {3,4,7,8,11,12} → 1, others keep the old value.
- `0x129fe`: clears `959436, 976508/12/16, 960080` (view state). `0x29e58` → `0x29533()` (third-person view build, vanilla-like). `0x85d8(&ang)`: adds the user's mouse look (trap 54/55 = GetCurrentCmdNumber/GetUserCmd; pitch from `cmd.angles`, yaw delta) to `ang`; `0x8523`: same data clamped ±16000. `0x1296b(1)`: swaps/zeroes the predicted player entity/state copies (`958780` 468 B, `959248` 728 B) around `0x2b81e` (re-derive the player entity) — used at the end when `1221216 == 0` and the mode claimed the view (`l2c != 0`). `0x165e7`: "replay camera active in demo" test using `df_demos_ReplayCameraMode`-like cvar `1192004`/`1193092`. `0x7d59(name, &out…)`: reads a vec3 preset by name (`targetposname`) from a cvar string. `0x7c81`: tracked-client record (ported as DC_Track).

## Modes (jump table at data 8592; second table at 8644)
| mode | action |
|---|---|
| 0 / >17 | not handled (`l2c = 0`) |
| 1 | `0x11fd8` pos/ang → `0x7b8c` + eye height; weapon/state of the ghost frame (`frame+28`) copied to `959440` and `958924`; ctx flags `+4=1,+8=1` |
| 2 | vieworg = pos; angles from `0x128c5` (demo) else own; eye height; `959440=958924=93012=0`; flags `+4=+8=1` |
| 3 | `0x7b8c` then `0x29e58` (third person) |
| 4 | like 2 then `0x29e58` |
| 5–12 | missile cameras: ctx `+12 = ((m-5)>>1)&1`, `+20 = (m<9)`, `+24 = ((m-5)&1)==0`; scan the ten missile records (forward if `+20` else backward) for one that `0x12149` matches by id between frames A and B (type 9 stops early); chosen index → `+28`, `+16 = 1`, clear `959440/958924/93012`; position/angles interpolated between the two missile records (`0x81f9`/`0x822c`), `+24` set: angles = vectoangles(missileB - missileA) (clamping yaw sign trick `82944`); `+12` set: `vieworg[2] -= 35` (`0x29e58` third person); no missile found: falls back to the ghost view (same as mode 1 with `+4=1,+8=1`) |
| 13 | free-look at the ghost position: clear `93012, 90292, 85668`, add mouse look (`0x85d8`), `0x7b8c`, eye height; flags `+4=1,+8=0` |
| 14 | like 13 but aims at preset `targetposname` (`0x7d59`) via vectoangles + mouse look; flags `+4=1,+8=0` |
| 15 | `0x7b8c`, eye height, weapon copy; flags `+4=1,+8=0` |
| 16 | flags only (`+4=1,+8=0`) |
| 17 | if cvar `df_demos_ReplayCameraMode` (atoi) nonzero and `0x165e7()`: flags `+4=0,+8=1`, done; else third-person (`958772`) → `0x7b8c`+`0x29e58`, otherwise `0x7b8c`, eye height, weapon copy, flags `+4=1,+8=1` |
Tail (all modes): `if (df_ghosts_CheckPVS) 0x8f57(predicted origin, vieworg)` (ported, ghost_system.md Iter 124); `if (1221216 == 0 && l2c) 0x1296b(1)`.

## Not yet read
`0x12149` body is read (id match of missile records); `0x2b81e`, `0x29533` (third-person view), `0x12934`; the miniview *drawing* (`df_ghosts_Miniview*`: Draw, DrawGun, CameraMode, Size, OffsetX/Y, BorderSize, FOV) is NOT in `0x12a10` — find its readers via the cvar storages 0x124e04/0x124cf4/0x124be4/0x124ad4/0x1249c4/0x1248b4/0x1247a4/0x124694 (probably `0x12a10`'s callers or a second-pass render of the scene). Port plan: first the modes 1–4/13–17 (simple view set-up from the interpolated ghost frame), then missile cameras, then the miniview render.

## Iter 126 — port notes (modes 1-4, 15-17)
- Context: `ctx+0` near flag (set by 0x13125 when the ghost is within `df_ghosts_NoDrawRadius` of the player AND `ctx+32 == 0`; also sets `ctx+4`), `ctx+4` hide body+label, `ctx+8` = draw only the player shadow (`0x26a50 -> 0x2627f` = vanilla CG_PlayerShadow, cvar `95188` = cg_shadows). DrawPlayer table `0x13125` (jump table 8716): 1 draw if !ctx4; 2 draw invisible (flag 16) if !ctx4; 3 near? invisible : (draw if !ctx4); 0/4 nothing (4 = marker) - shadow afterwards if ctx8.
- Globals: `958780` = cg.predictedPlayerState (468 B), `958924` = ps.weapon, `959248` = cg.predictedPlayerEntity (728 B), `959440` = its currentState.weapon; `93012`/`90292`/`85668` = cg_drawCrosshair/cg_draw2D/cg_drawGun `.integer` (modes 2, 5-14 write 0 directly); `1221076` = persistent int[3] angle delta of `0x8523` (view angles = SHORT2ANGLE(usercmd.angles[i] + delta[i]), pitch clamped to +-16000 with the overflow written back into the delta). `0x29533` = vanilla CG_OffsetThirdPersonView (wrapper `0x29e58`).
- `0x1296b(1)`: copies ps+predictedPlayerEntity to the stack, zeroes both, restores only the two weapon fields, calls `0x2b81e` (= CG_AddViewWeapon) and restores everything - an immediate view-weapon draw from a blank ps. PORTED iter 164 (`DG_BlankGun`, called at the end of `DG_Camera` when not in the miniview; the ordinary gun is skipped by `CG_DF_SkipViewWeapon` = 0x109d6/0x146fa).
- Still open: `0x85d8` (modes 13/14 mouse-look: reads the usercmd, buttons&16 (walking) test, writes data 5688 / roll), missile cameras 5-12.

## Iter 127 - Miniview (hook `0x7a04` after `CG_DrawActive` `0x1ce20`)
Globals: `1221216` in-miniview-pass flag, `1221268..1221296` = copies of df_ghosts_MiniviewDraw/DrawGun/CameraMode/Size/OffsetX/OffsetY/BorderSize/FOV (made by `0x109e9`, cvar copy also of FileExtension etc.), `965976` = cg.showScores, `958772` = cg.renderingThirdPerson, `958776` = cg.hyperspace, `83488/83492` = cg_fov value/integer, `1130620` = cgs.media.backTileShader, `960188..200` = cg.refdef x,y,width,height.
`0x7a04`: skip if showScores or ps.pm_type==PM_DEAD; set `1221216=1`; (ReplayMode 3/4 and (not a demo or CameraMode==0): replay edit overlay `0x18873`, NOT ported); if gate `0x13488` then `0x134dc(cg.time)`; `1221216=0`.
Gate `0x13488` (D = MiniviewDraw): D==0 no; no ghost playing && D<=1 no; !demo && !ghost && D==2 no; !demo && D==3 no; D==8 needs the tracked record (`0x7c81`) client == df_mp_TrackPlayerNum; else yes.
`0x134dc`: save third-person flag, fov, refdef rect. D 1..4: third = !third (forced 0 when a ghost with its own camera mode plays); D 5,7,8: 0; D 6: 1 (only when no ghost or MiniviewCameraMode==0; with a ghost + CameraMode the ghost camera is used and the player body is drawn: third=1 during the re-add). `0x29e5e` = OffsetThirdPerson(0x29533) or OffsetFirstPerson(0x296d6) then CG_CalcFov (0x29939). Rect: w,h = int(size/100 * w,h); x = W/2 - w/2 + OffsetX*W/640, y = H/2 - h/2 + OffsetY*W/640. Border (>0): backtile quad (x-b, y-b, w+2b, h+2b) with s,t = pos/64. Re-add lists unless hyperspace: AddPacketEntities `0x1f2f2`, AddMarks `0x21218`, AddParticles `0x22d02`, AddLocalEntities `0x20dab`. View: ghost -> `0x127af(time,0,1)` (no cursor advance, camera mode = MiniviewCameraMode, same `0x12a10`); no ghost: D 7/8 -> view = tracked record (`0x7d32`: vieworg/angles/axis from the record, ps.viewangles, weapons); then if the tracked miniview is active (`0x10912`) gun with blank state, else if first person: vieworg = ps.origin+viewheight, angles = ps.viewangles; gun. Gun = `0x129e6(blank)` = cg_drawGun forced 1, `0x1296b(blank)`. `trap_R_RenderScene(&refdef)` (trap 44), restore rect/third/fov.

## Iter 129 - modes 13/14 and 0x85d8 decoded
`0x85d8(&ang)`: cmd = current usercmd; `if (!(cmd.buttons & 16 /*BUTTON_WALKING*/)) base(data 5688) = -(float)cmd.angles[1]; else ang[2] -= 360/65536 * (short)(int)((float)cmd.angles[1] + base)` (roll control by mouse-X while walking). `0x7d59(name-cvar, out, ...)`: value of the cvar named by cvar `targetposname` = `placeplayer x y z pitch yaw roll vx vy vz` (tokens via COM_ParseExt, atoi; optional +26 z flag; outputs pos / angles / velocity). Mode 13: crosshair/2D/gun `.integer=0`, roll look on the interpolated angles, `0x7b8c`, eye height, ctx hideBody. Mode 14: same but angles = vectoangles(normalize(targetpos - ghostpos)) before the roll.
