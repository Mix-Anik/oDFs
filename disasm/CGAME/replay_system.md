# DeFRaG replay / recam system (cgame) — structure survey (iter 66, NOT ported)

A demo-camera scripting system ("Recam/Replay"): a `replay` console command edits in-memory tables of cameras, transitions, activators, links and
variables; `.cfg` scripts live in `system/replayscripts/<mapname>.cfg`; cvars `df_demos_ReplayMode` (0), `df_demos_ReplayFilename` (`$demoname`),
`df_demos_ReplayDrawMask` (32767), `df_demos_ReplayCameraMode`, `df_demos_ReplayTransitionSpeed`, `df_demos_ReplayVar_FOV`
(plus the camera-offset cvars `df_demos_CamOffsetPosition{X,Y,Z}`, `CamOffsetAngle{X,Y,Z}`, `CamOffsetRadius{F,R,U}`, `TargetPosition{X,Y,Z}`).
Everything below is read directly from `cgame.dis`; nothing here is ported.

## Strings (data segment)
- Sub-commands of `replay` (string table `0xcb14..0xcb52`): `?`, `HELP`, `ACTIVATE`, `SET`, `GET`, `DELETE`, `CLEAR`, `VALIDATE`, `SAVE`/`WRITE`, `LOAD`/`READ`.
  Usage lines: `SET DataType DataID <...>`, `GET all DataID <format>`, `GET DataType DataID <format>`, `CLEAR all`, `CLEAR DataType %i`, `CLEAR DataType DataID`,
  `ACTIVATE ActID`, `VALIDATE`, `WRITE/SAVE filename`, `READ/LOAD <filename>`. File name is built with `"%s%s.%s"` (path, name, extension).
- DataType names (`0xd97e..0xd986`): `NONE`, `VAR`, `CAM`, `TRAN`, `ACT`, `LINK` (internal type ids used by `0x1676c(type, id)` = "is slot in use": 2 = CAM, 4 = ?, 5 = ACT).
- Activator types (`0xd915..0xd969`): `MANUAL`, `TIMER`, `TRANEND`, `RADIUS`, `VOLUME`, `TIME`, `POSX`, `POSY`, `POSZ`, `POSXY`, `POSXZ`, `POSYX`, `POSYZ`, `POSZX`, `POSZY`.
- Script file table headers (`0xd170..0xd3a4`): LINK `ID DType ID DType ID`; others `ID <type> ID`, VOLUME `ID type X1 Y1 Z1 X2 Y2 Z2`, RADIUS `ID type X Y Z Radius`,
  TIME `ID type Time`, MANUAL/On `ID type On`, ACT `ID ActType <...>`, TRAN `ID TranType DType ID ID Speed Loop`, CAM `ID X Y Z P Y R`, VAR `ID VarName Value`.
- Limits quoted in the script header: cameras, transitions, activators, links, variables ("The maximum number of ... is: %i").
- Messages: `REPLAY: Parsing File`, `Success Parsing File`, `Invalid ActType/Datatype/TranType`, `Checking For Invalid DataID References...`, `Unreferenced`, `Script Is Valid`,
  `Cleared All`, `Disabled Cleanup`, `Spectator Cleanup`, `Spectator Mode: REPLAY`.

## Functions located
| addr | role | evidence |
|---|---|---|
| `0x176e7` | **template/script file writer** (`replay WRITE`): FS_FOpen `system/replayscripts/...`, writes the `// This is a Defrag Recam/Replay script file.` banner (SCRIPT BY = player name sans colours, MAP, DATE via `trap_RealTime`), the per-table headers (callbacks `0x16ce4/0x16d3d`, `0x16d89`, `0x17077`, `0x16fa7`, `0x17357`, `0x171b6`, `0x175bc`, `0x1752a`), `// End Of File` | strings, `trap10/12/70` (FOpen/Write/RealTime) |
| `0x18873` | **edit-mode world overlay**: gated by a global (`*(2823428)`); `l310 = *(1191460)` is the bit mask `df_demos_ReplayDrawMask.integer` (verified: its vmCvar is at `0x122e18`, `.integer` = +12 = `1191460`): bit 1 draws camera markers (`0x9229`), bit 2 calls `0x1f3a0` (not read), bit 4 draws lines between the two cameras of each TRAN (`0x948a`), bit 8 draws lines for the activator/link records (`0x948a`) | decompile of `0x18873` |
| `0x1676c(type, id)` | slot-in-use test | called with 2 (CAM), 4, 5 (ACT) |
| `0x18287`, `0x12a10`, `0x1815f` | per-frame replay runtime / camera apply — NOT yet read | listed in PORT_STATUS |

## Memory tables (cgame data addresses, decimal as printed by qdec)
| base | stride | contents (observed) | count |
|---|---|---|---|
| `2806496` | 24 | CAM: origin[3] then angles[3] (floats, `memcpy 0xc` at +0 and +12) | 100 (`l2fc < 100`) |
| `2808900` | 44 | TRAN: `+0` type (2 seen = camera-to-camera), `+4`/`+8` source/target ids, further fields (speed, loop, interpolants) | 100 |
| `2813296` | 36 | LINK-ish record: `+0` type (4 = fixed point, 5 = point derived from two vectors by `0x8343`), `+4..` vec3, `+16` vec3 | indexed by activator `+0` |
| `2816900` | 16 | ACT: `+0` link id, `+4` target type (2 = CAM, 3 = TRAN), `+8` target id | 400 (`l2fc < 400`) |

The remaining structure (SET/GET field layouts, activator evaluation, transition interpolation, the cameramode integration with `df_demos_CameraMode`) is not decoded.

## Next steps (if continuing)
1. Decode the `replay` command dispatcher (find the function that compares against the `0xcb14..` string table; callers of the `0xca36..0xca48` formatters).
2. Decode `0x18287` / `0x12a10` (runtime), then SET/GET field layouts, then port as `cg_dfreplay.c`.
Value for oDFs is low (demo-only camera scripting that needs DeFRaG's demo tooling), so this is the largest remaining payload-for-effort item.

## Iter 67 — dispatcher and data model decoded (verified in qdec)
**`0x18006` = the `replay <sub> ...` dispatcher** (token via `0x31f8b` = Com_Parse-style on the cursor `&l1c`; compares with `0x325be` = stricmp-like; strings at `0xcb14..0xcb52`):
| sub-command | handler |
|---|---|
| none / `READ` / `LOAD` | `0x17e8c` (load script) — NB the empty-token test falls to it |
| `WRITE` / `SAVE` | `0x17ec1` (writes `system/replayscripts/<name>.cfg` via the writer `0x176e7`) |
| `VALIDATE` | sets `*(2823424)=1`, `0x16afb()` -> result stored in `*(2823428)` = the **"script valid / enabled" flag that gates the edit overlay `0x18873`** and `0x16b76`-style runtime |
| `CLEAR` / `DELETE` | `0x17e0f`: optional DataType name (table lookup `0x8646(9276, *(9304), name)`), `all`, then `0x8b96` int parse of DataID, `0x16b76(type,id,1)` + `0x18287(type,id,0,0)` |
| `GET` | `0x17d95`: `all` or DataType, DataID, format; loops `0x18287(type, id, format, 1)` (= print/refresh one record, `0x18287` is the GET/print function, not the runtime) |
| `SET` | `0x175ca(dataTypeName, &cursor, 0)`: by DataType calls `0x16c75` (VAR) / `0x16d4b` (CAM) / `0x16e53` (TRAN) / `0x170c6` (ACT) / `0x173e2` (LINK), then (if `*(2823436)`) `0x18287(type, id, flags, 1)` |
| `ACTIVATE` | `0x17f21` |
| `HELP` / `?` / (anything else: unknown command message) | usage text via `0x17fc3` / `0x23c` (Com_Printf) |
DataType ids (`0x8646` lookup in the name table at data `9276`, count `*(9304)` = 6): 0 NONE, 1 VAR, 2 CAM, 3 TRAN, 4 ACT, 5 LINK.

**Slot-in-use test `0x1676c(type, id)`** (range check `0x16740`->`0x166ef`/`0x16711`; jump table at data `9536`), and storage:
| type | base | stride | in use when |
|---|---|---|---|
| 1 VAR | `2791296` | 76 | name string (first bytes) non-empty (`strlen != 0`) |
| 2 CAM | `2806496` | 24 | `|origin| != 0` or `|angles| != 0` |
| 3 TRAN | `2808896` | 44 | first int (`+0`) != 0 (TranType) |
| 4 ACT | `2813296` | 36 | first int (`+0`) != 0 (ActType) |
| 5 LINK | `2816896` | 16 | `+0` != 0 and `+8` != 0 |
Previous iteration's table is corrected: TRAN fields start at `2808896`(+0 type, +4 ids…), ACT records are the 36-byte ones (`+0` ActType: 4 = fixed position, 5 = two-vector derived (`0x8343`); `+4` vec3, `+16` vec3), LINK records (16 bytes) are `{ActId, targetDType, targetId, ?}` — in the overlay `fn_1676c(5,i)` then `act = link[+0]`, `link[+8]==2` CAM / `==3` TRAN `link[+12]`=target id (offsets as printed by qdec: `+0` act id at `2816900`, `+4` dtype `2816904`... i.e. fields at +4/+8/+12 of a base `2816896`).
Still undecoded: `0x18287` (GET print), per-type SET parsers `0x16c75..0x173e2`, validate `0x16934`, script loader `0x17e8c` / parser, activate `0x17f21`, runtime that applies cameras during demo playback.

## Iter 68 — SET parsers and tables decoded (qdec of 0x16c75 / 0x16d4b / 0x16e53 / 0x170c6 / 0x173e2, data dump `0x2420..0x25b0`)
**Name/limit tables (cgame data):** per-DataType slot counts at `0x2424..0x2434` = {NONE 0, VAR 200, CAM 100, TRAN 100, ACT 100, LINK 400}; DataType names `0x243c..0x2450` (NONE VAR CAM TRAN ACT LINK), count word `0x2458` = 6. ActType names `0x245c..0x2470` (NONE MANUAL TIMER TRANEND RADIUS VOLUME), count `0x2478` = 6. TranType names `0x247c..0x24a4` (NONE TIME POSX POSY POSZ POSXY POSXZ POSYX POSYZ POSZX POSZY), count `0x24ac` = 11. Constants after it: `0x24b0` = 100.0f, `0x24b4` = 0.001f, `0x24b8` = 15 (number of built-in VAR cvar names), `0x24bc` = 8, `0x24c0` = 500, then 15 cvar-name pointers `0x24c4..0x24fc` (df_demos_ReplayCameraMode, ReplayTransitionSpeed, CamOffsetPositionX/Y/Z, CamOffsetAngleX/Y/Z, CamOffsetRadiusF/R/U, TargetPositionX/Y/Z — the VAR slots bound to cvars). Lookup helper `0x8646(nameTable, count, str)` = index or -1; `0x8a85` = "is the token non-numeric"; `0x8b96/0x8b60/0x8b7b/0x8bb1` = parse int / string(64) / float / vec3 from the command cursor; `0x2f428` = atof; `0x16740(type,id,warn)` = range+in-use check (id < count[type]); `0x166ef` = DataType index range check.

**Record layouts (SET parsers):**
- VAR (76 B @ `2791296`): `+0` name[64] (string token), `+64` float Value, `+68` float CurrentValue, `+72` float OriginalValue — after parsing, `+68/+72` are both set to `atof(cvar named by the record)` (`trap_Cvar_VariableStringBuffer(name)`); i.e. a VAR mirrors a cvar.
- CAM (24 B @ `2806496`): `+0` origin vec3, `+12` angles vec3 (pitch yaw roll) — two successive vec3 parses.
- TRAN (44 B @ `2808896`): `+0` TranType (name or number; error `Invalid TranType`), `+4` DataType of the endpoints (name lookup in DataType table), `+8` DataID1, `+12` DataID2 (both validated with `0x16740(dtype,id)`), `+16` Speed (float), `+20` Loop (float; `+24` is initialised = `+20` = CurrentLoop), `+28..+40` Interpolant0/1/2 (runtime, printed by GET as `Interpolant0..2`).
- ACT (36 B @ `2813296`): `+0` ActType (name/number, error `Invalid ActType`; switch at data `0x2580`): MANUAL(1) `float@+28` (On), TIMER(2) `float@+28` (Time), TRANEND(3) `float@+28`, RADIUS(4) `vec3@+4` + `float@+28` (X Y Z Radius), VOLUME(5) `vec3@+4` + `vec3@+16` (X1 Y1 Z1 X2 Y2 Z2). (Earlier "ActType 4/5" reading in the Iter 67 table was the same field.)
- LINK (16 B @ `2816896`): `+0` must be 4 (ACT; error `Invalid Datatype` otherwise), `+4` ActID (validated against ACT), `+8` target DataType (name or number: CAM or TRAN), `+12` target DataID (validated).
SET then (when `*(2823436)` is set — the replay edit/enabled flag) calls the refresh/print function `0x18287(type, id, a, 1)`.

**Port status:** the data model, `SET`, `CLEAR/DELETE`, `GET` formatting (`0x18287`), `VALIDATE` (`0x16934`), script LOAD (`0x17e8c`/parser), SAVE (`0x176e7` + table writers `0x16ce4..0x175bc`), ACTIVATE (`0x17f21`) and the runtime are all candidates for `cg_dfreplay.c`; of these only the SET parsers, tables and writer skeleton are decoded so far.

## Iter 69 — ACTIVATE, VALIDATE and the GET printer `0x18287` decoded
- **ACTIVATE `0x17f21`**: token = ActID (non-numeric -> `atoi`-style `0x2f5b8`; else `0x8ae2` int parse), `0x16740(4,id)` must be in use; only **ACT type 1 (MANUAL)** is touched: `+28` (On) = optional second token as float (`0x2f428`), or `1.0` when no token is given. Other act types: command does nothing (returns 1).
- **VALIDATE `0x16afb` -> `0x16934(verbose)`**: `0x168db(type, verbose)` for VAR/CAM/TRAN/ACT = "every in-use slot is referenced" (`0x16816(type,id)` = referenced by some TRAN `+4/+8`... precisely: a TRAN with `+4 == type` and `+8 == id` or `+12 == id`, or a LINK whose `{+0,+4}` or `{+8,+12}` equals `{type,id}`); then every TRAN endpoint pair `(dtype,id1)`/`(dtype,id2)` and every LINK `{dtype,id}` pair must be in use (`0x16893(dtype,id)`); messages "Checking For Invalid DataID References...", "DataID References OK", "Checking for Unreferenced Data Types...", "Script Is Valid/Contains Errors". The wrapper `0x16afb` **discards the result and returns 1**, so after any `replay validate` the global `*(2823428)` (overlay enabled) is 1 and `*(2823424)` = 1; `verbose = *(2823436)`.
- **GET printer `0x18287(dtype, id, format, verbose)`** (jump table `9680`): `id == -1` = all slots of the type (`count = 9248[type]`), else one; skips unused slots; `0x16740` range check. Formats: `0` full record, `1` CHAIN line (`^2<TypeName>^7 ^3[ id ]^7 -->  CHAIN`, then per-endpoint pieces via `0x1816e(buf, 3/5, dtype, id)` = trace of TRAN/LINK users), `2` table row (header printed once for the all-slots call via the per-type header writer `0x16d3d`/`0x16e45`/`0x17069`/`0x17357`/`0x175bc`; row writer `0x16ce4`/`0x16d89`/`0x16fa7`/`0x171b6`/`0x1752a`), `3` header only (then `l18 = count`, ends loop).
  Full-record (format 0) text, in print order, all prefixed by `^2<TypeName>^7 ^3[ id ]^7` and ` ^4-->^7 `:
  - VAR: `Name = %s`, `Value = %.3f` (+64), `( CurrentValue = %.3f )` (+68), `( OriginalValue = %.3f )` (+72)
  - CAM: `Position = ( x y z )` (+0), `Angles = ( p y r )` (+12)
  - TRAN: `Type = <TranType name>`, `DataType = <name>` (+4), `StartID = %i` (+8), `StopID = %i` (+12), `Speed = %.3f` (+16), `Loop = %.3f` (+20), `( Interpolant0..2 = %.3f )` (+28/+32/+36), `( CurrentLoop = %.3f )` (+24), `( Activated = %i )` (+40)
  - ACT: `Type = <ActType name>`, `< Position1 = ( ... ) >` (+4), `< Position2 = ( ... ) >` (+16), `< Value = %.3f >` (+28), `( Activated = %i )` (+32)
  - LINK: `DataType1 = <name>` (+0), `DataID1 = %i` (+4), `DataType2 = <name>` (+8), `DataID2 = %i` (+12)
  Format strings: `0xc724..0xca30`. After SET/ACTIVATE changes it re-runs this printer when verbose (`*(2823436)`).
Remaining undecoded: the table row/header writers (shared with the file writer), `0x1816e` chain tracer, the script loader/parser `0x17e8c`, and the **runtime** (what drives `Interpolant*`, `Activated`, and applies camera position/angles/FOV to the view during demo playback — callers of the camera cvars `df_demos_CamOffset*` / `TargetPosition*`).
**Next:** start `code/cgame/cg_dfreplay.c` with the data model + `replay` dispatcher + SET/CLEAR/DELETE/ACTIVATE/VALIDATE + GET formats 0/1 (everything above is decoded); tables, loader, saver, runtime follow.

## Iter 71 — script LOAD decoded and PORTED
`replay LOAD [name]` = `0x17e8c`: stores the optional name in global `2823296`, clears the loaded/enabled flags (`2823424/2823428`), calls `0x1a237` ("ensure loaded"): `0x190e1` resets 14 cvars to `0` (`df_demos_ReplayCameraMode`, `ReplayTransitionSpeed`, `CamOffsetPosition{X,Y,Z}`, `CamOffsetAngle{X,Y,Z}`, `CamOffsetRadius{F,R,U}`, `TargetPosition{X,Y,Z}`) and sets `df_demos_ReplayVar_FOV` = `cg_fov`; `0x1901c` picks the file name (name given, else the cvar `df_demos_ReplayFilename` — default `$demoname` — else the map name), tries `system/replayscripts/<name>.cfg` through `0x17659`, and if that fails falls back to `<mapname>.cfg` unless a demo is playing and the expanded `$demoname` equals the name just tried; success sets "enabled" (the validator result is discarded). `0x17659(path)`: clears all tables (`0x16b76(-1,-1)`), reads the file (`0x8da0`, max 32000 bytes), then tokenises with Com_Parse (`0x31f8b`) and feeds every token — a DataType name — to the SET dispatcher `0x175ca(token, &cursor, 2)`; any failure -> "While Parsing File". **So a script file is just a sequence of SET records** (`VAR id name value`, `CAM id x y z p y r`, `TRAN id type dtype id id speed loop`, `ACT id type ...`, `LINK id ACT actid dtype id`), `//` comments handled by the tokenizer. The per-record echo of `0x175ca` in load mode is GET format 2 (table row): row writers decoded in the same pass (`0x16ce4` VAR `"%s " "%s%3i " "%s%30s " "%s%7.3f"`, `0x16d89` CAM ids + 6 ints `%6i %6i %6i %4i %4i %4i`, `0x16fa7` TRAN, `0x171b6` ACT per type, `0x1752a` LINK) — not ported.
oDFs: `DR_LoadPath` / `DR_LoadByName` / `DR_EnsureLoaded` / `DR_CmdLoad` in cg_dfreplay.c; cvars `df_demos_ReplayMode`, `df_demos_ReplayFilename`, `df_demos_ReplayDrawMask` registered with DeFRaG defaults. SAVE (`0x17ec1` -> writer `0x176e7`) next; also fixed lcc "overflow in converting constant" warnings in cg_dfchs.c (iter 61 arrow glyphs).

## Iter 72 — script SAVE decoded and PORTED
`replay SAVE|WRITE <name>` (`0x17ec1`): name token (`$var` expanded; empty -> "Missing Filename"), path `system/replayscripts/<name>.cfg`, writer `0x176e7` (FS_WRITE; open failure -> "Could not create file"). File layout (all CRLF): slash bar, `// DO NOT EXECUTE IN QUAKE3 !!!`, banner, `// The file must be placed in the "system/replayscripts" folder.`, filename note, bar, blank, bar, `SCRIPT BY` (player name, colours stripped) / `MAP` (cvar mapname) / `DATE` (`trap_RealTime`), bar, blank, bar; then per table: max-count comment (VAR 200, CAM 100, TRAN 100, ACT 100, LINK 400), the extra help lines (VAR: default value -9999, `df_demos_ReplayCameraMode`, `df_demos_ReplayVar_FOV`; TRAN: `// TranTypes:` list; ACT: six per-ActType header lines), `// <header>` + blank, then one row per in-use record (3 spaces + the row text), blank, bar; finally `// End Of File`. Row formats (decoded from `0x16ce4/16d89/16fa7/171b6/1752a`): the row text is the SET syntax with fixed widths, e.g. `VAR   0 name... 1.000`, `CAM   0   100   200   300   0   90    0`, so **a saved file is loadable by LOAD** (round trip). Ported as `DR_WriteFile` / `DR_Row*` / `DR_ActHeader` in cg_dfreplay.c. The row writers are also the GET table formats 2 / 3 (not yet wired to GET).
