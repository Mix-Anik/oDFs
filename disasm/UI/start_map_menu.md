# DeFRaG start-map menu (ui 0x3825) — fully decoded (iter 157 picker core, iter 158 panels/run-server/keys)

Not a vanilla function; a full menu built from static state (all addresses below are ui DATA addresses, decimal as printed by qdec).
Decoded with `disasm/tools/qdec.py ui.dis <addr>`. Backend: `UI/map_database.md`.

## Entry 0x3825(type, ?, ?, arg l50)
`memset(624736,0,868)` = the menuframework (+`625264` draw fn 0x36fc, `625268` wrapAround=1, `625272` fullscreen=1, `625328` = game type arg 1..5).
Per type: 1 → `0x30e9` panel, picker `0x279e(menu, 10,10, 1,1, arg, 0)`; 3 → `0x34cd`,`0x2a28`,`0x2db3`, picker `(…,2,2, arg, 64)`; other → `0x330d`,`0x326f`,`0x2a28`,`0x2db3`, picker `(…,3,2,arg,0)`; then `0x3797` (back/go), `0xedc(625284,0)`, `0xc263(menu)` (push). Title string via `trap3(23580 …)` ("…%i" from 24031). Panels NOT decoded yet: 0x30e9, 0x34cd, 0x2a28, 0x2db3, 0x330d, 0x326f, 0x3797, run-server 0x365a, draw 0x36fc.

## State
| addr | meaning |
|---|---|
| 628964[4] / 628980[4] / 629000 | mode ids (bit values 1,2,4,8 taken from the mask arg) / db names (`0x1826(type)` = db+16 name) / count (built by `0x26c0(mask)`) |
| 629004 | current db type (1 defrag, 2 quake3) — set by `0x2651(type)`; type==1 enables the three radio/spin widgets 633604/633668/633732 (flags &= ~0x5000) else disables (|= 0x5000); also sets 633528 = index of type in the list |
| 629008[50] | map entry pointer per button on the current page |
| 629208 / 629212 / 629216 | selected entry / its global index / total entries matching the filter |
| 629220[16] / 629236[..] | title text "%i/%i" (24247 fmt: index+1, total) / info line (second line: vq3+cpm / cpm / vq3 text, then training/level/accuracy/run/…) |
| 629252[] / 629380 / 629384 | page-start entry pointers (every 50th entry) / page count / current page |
| 629388 / 629392 / 629396 | filter maskA (mode: bit0 vq3, bit1 cpm) / maskB (type bits 8 run,4 training,16 level,32 accuracy, 60 = all) / fixed maskA override (arg l40) |
| 629400… | 50 map buttons, 76-byte `menutext_s`: type 9 (PTEXT), id = idx+50*page, flags 2050, callback 0x2751, style 65536, grid pos x = x0 + 13*(i%25)… y = y0 + 17*10*(i/25) (columns of 25) |
| 633200… | widgets: bitmaps (6) prev/next page arrows (633288 id1, 633376 id2; flags 260; hidden via |8192 at first/last page), spinners/radios 633464 (type 3, id 3, spin list 628980, only if >1 db), 633560/633624 (radio "vq3"/"cpm" ids 4, 633688 spin 5 "type" with list at 2940) |

## Functions
- `0x279e(menu,x,y,?,?,type,fixedMask)` lays out all of the above, then `0x2651(type)`, `0x25c0()`, `0x2551()`.
- `0x25c0` recomputes filters: fixed mask → A=fixed, B=all; else A=B=all; for db 1: A = vq3 radio(633620)?1 | cpm radio(633684)?2; B from type spinner 633752 (1 run 8, 2 training 4, 3 level 16, 4 accuracy 32, else 60).
- `0x2551` rebuilds page index: iterate `0x17b8(db, prev, A, B)`; every 50th entry → 629252[page++]; total count → 629216; shows page 0 (`0x222d(0)`) and selects entry 0 (`0x233c(0)`).
- `0x222d(page)`: show/hide prev/next arrows, fill the 50 buttons from page start (string = map name, centred via `0xc527` width, y offset +13.5, flags &= ~0x4000 (visible) / empty → string 24264 "" and |0x4000), remember entry per button.
- `0x233c(index)`: clamp to [0,total-1], switch page if needed, highlight selected button (colour 10788 vs 10772, flag 0x4000 toggled), set 629208/629212, `0x2223(entry)` stores the entry for the preview (634024; 634004 = 0), title "%i/%i", info line: for db 1 – mode text (both/cpm/vq3) + type text; otherwise 24192.
- `0x246d(char)`: type-ahead — a-z/A-Z/0-9 jumps to the next map whose first letter matches (case-folded with `0x1c178` tolower), wrapping.
- `0x2223(e)`: sets preview entry. `0x26c0(mask)`: fills the mode list.

Strings used (data addrs): 24257,24264 (empty titles), 24247 fmt, 24238/24233/24228 (mode texts), 24219/24213/24204/24200 (type texts), 24192 (db-2 info). Resolve with the ui string table before porting.

## Panels (iter 158)
Panel framework: `0xe97(menu, panel)` registers a panel (panel+0 = menu, +68 = title string, up to 4 per menu; `menu+16+4*i` holds titles), `0xe52(panel, widget)` adds a widget to the panel (<=16) and to the menu (flags |= 0x5000 = hidden/inactive until the panel is shown), `0xedc(panels, idx)` shows panel idx (hides the others). `0x21f0(panel,x,y,w)` adds the 200x150 map-preview bitmap (type 6, flags 16392, ownerdraw/shader id 8406). `0x1df9f(min,max,v)` = clamp. All panel widgets are SPINCONTROLs (type 3, flags 258, ids 13466/12458/13018/… = per-panel callback) whose curvalue comes from a cvar; lists (data addr -> items):
2996 off,on | 3008 on,off | 3020 no,listen,local,public | 3040 VQ3,CPM | 3052 on,off,kill | 3068 off,selective,on | 3084 Quake3,Swinging,Pendulum,Quake2 | 2940 any,run,training,level,accuracy | 3144 0..7.
Tab buttons are created with titles from the panels: "Map Info" (23571), "Gameplay" (23928), "Weapons" (23641), "Timer" (23514).

| fn | game type | panel | widgets (label : cvar / state : list) |
|---|---|---|---|
| 0x30e9 | 1 (VQ3/CPM) | Map Info | preview; Movements : df_promode (VQ3/CPM) ; Overbounces : df_ob_KillOBs (curvalue = value==0) 2996; Server Cheats : state 634032 2996; Run Server : 3020 |
| 0x34cd | 3 (tricks? type==3) | Map Info | Game Mode : df_mode_fastcaps clamp(0,7) list 3144, Movements, Server Cheats, Run Server |
| 0x330d | other (type 2/4/5) | Map Info | preview; Game Mode : df_mode_tricksmode clamp(0,7) list 3144; Movements; Server Cheats; Run Server |
| 0x326f | other | Timer | `0x12e5(panel,x,y)` custom control (labels '0'..'7' = timer item select); not yet decoded |
| 0x2a28 | all | Gameplay | Damage : df_ndm_damage clamp(0,1) 2996; Powerups : df_ndm_powerups 2996; Jump-pads : df_ndm_nopad clamp(0,2) 3052; Teleporters : df_ndm_noteleport clamp(0,2) 3052; Movers : df_ndm_nomover clamp(0,1) 3008; OverBounces : df_ob_KillOBs (==0) 2996; Items spawn : df_ndm_itemsSpawn clamp(0,2) 3068 |
| 0x2db3 | all | Weapons | Weapons pickup : df_ndm_weaponsPickup clamp(0,1) 2996; LG knockback : df_lg_Knockback 2996; Hook type : df_hook_Type clamp(0,3) 3084; 10 checkboxes (type 5) Gauntlet,MG,SG,GL,RL,LG,Rail,Plasma,BFG,Hook = bits 1..10 of df_ndm_weaponsSet; "All"/"None" text buttons (ids from 23591/23595) |

Widget y spacing 20, panel top y = frame y + 230 (Map Info) / +130 (Gameplay) / +50 (Weapons); x = frame.x + frame.w/2 + 25.

## Buttons 0x3797
back (art `menu/art/back_0/1`, id 2, x=0 y=360 96x48, flags 260), go (`fight_0/1`, id 1, x=630 y=412?? (625344=630 -> x 630? actually left-aligned bitmap at 630,412 96x48; back at 360?? fields printed: back x=360? see qdec) and a "> options" (23124) text button id 100 at (495,392) flags 258 opening panel switch via `0xedc(menu panels, id)` — exact coordinates to be copied from qdec when porting.

## Run server (event 0x365a, event type 3 = activate)
id 1 GO: needs a selected map (629208); `df_gametype <625328>` (+4 when "Run Server" spinner 634028 != 0... exact: if 634028 != 0: type += 5-1); `sv_pure 0`; if spinner >= 1: `dedicated <spinner-1>`; then `wait ; wait ; devmap <map>` (cheats spinner 634032 != 0) else `wait ; wait ; map <map>`. id 2 BACK: pop menu (0xc2fe). id 100: panel switch `0xedc(625284, item+64)`.

## Draw/key handler 0x36fc (menu key fn)
Key table (jump 2704, keys: K_UPARROW 132 → sel-1, K_DOWNARROW 133 → +1, K_LEFTARROW 134 → -25, K_RIGHTARROW 135 → +25, K_PGDN 141 → +50, K_PGUP 142 → -50, K_HOME 143 → 0, K_END 144 → last; all via `0x233c`, returning menu_move_sound); `key & K_CHAR_FLAG(1024)` → type-ahead `0x246d(key & ~1024)`; anything else → Menu_DefaultKey (0x14a42).
Map button callback 0x2751: event 3 (activate) → select button's entry (`0x233c(id)`); 101/102 hover swap colours (10772 normal <-> 15048 hover).

## Timer panel (0x326f -> 0x12e5 -> 0x1147, event 0xf1a, init 0x1036) — decoded iter 159
Three groups (start / checkpoint / stop) at y = frame y + 90 + 80*g, x = panel centre + 25 - 10, each three spinners: type (`none`,`Defrag Targets`,`Item`[,`Respawn` for start only]), `Item Type:` (Weapons, Ammo, Powerups, Armor, Health, Holdables) and `Item:` (list depends on the type). Backed by cvars `df_ndm_timer_start|checkpoint|stop` holding a TOKEN string: "" none, "df", "resp", or an item token from the 60-entry table at ui data 1460 (group*10+item: mg sg gl rl lg rg pg bfg hook / amg asg agl arl alg arg apg abfg / quad bs hast inv reg fli / as ya ra / h5 h25 h50 mh / tp med). Group/item spinners are greyed unless type == Item; changing the group reloads the item list.

## Port (iter 159)
`code/q3_ui/ui_startmap.c` `UI_DF_StartMapMenu(type, variant)` (types: 1 Defrag VQ3/CPM, 2 Tricks, 3 Fast Caps; main-menu calls (1,0) (1,1) (2,0) (3,0)). Deviations from DeFRaG (all cosmetic/robustness): map text uses UI_SMALLFONT (DeFRaG style 0x10000 font scale unknown); selected map is marked only by being inactive (both DeFRaG colours 10772/10788 are the same orange); Run Server / Server Cheats spinners always start at their defaults (DeFRaG keeps stale globals); the preview bitmap uses QMF_OWNERDRAW. GO sets `df_gametype` (+4 when Run Server != no), `sv_pure 0`, optional `dedicated n`, then `wait ; wait ; [dev]map <name>` (`
` appended as in the other ports). Panels write their cvars on every activation (all values at once), exactly like the DeFRaG callbacks.
BEHAVIOUR CHANGE: main-menu Defrag VQ3/CPM/Tricks/Fast Caps buttons now open this menu (previously the stock Start Server stop-gap). Revert: restore the four `UI_StartServerMenu( qtrue )` calls in ui_menu.c `Main_MenuEvent`.
Untested at runtime (QVM build clean). Needs `scripts/*.defi` / `scripts/arenas.txt` from the DeFRaG pk3 and the `menu/art/gs_arrows_*` shaders (stock Q3 has them).

## oDFs rework (map browser)
`UI_DF_StartMapMenu(type, variant)` is replaced by `UI_DF_MapsMenu()`; the four main-menu buttons (Defrag VQ3 / CPM / Tricks Mode / Fast Caps) became one "Maps" button (`ui_menu.c`), the game types are the mode chips VQ3 / CPM / TRICKS / FAST CAPS of `ui_startmap.c`. The paged 50-button grid, the db spinner, the vq3/cpm radios and the key table above are gone: one scroll list over a merged, name sorted record list of both databases (a quake3 map that a defrag file lists too is shown once), a search box (every word has to occur in map + longname + author, name prefix first), mode chips with match counts, type chips, tabs for the panels and an always visible Server Cheats / Run Server. The panels, cvar writes and the run-server handler (0x365a) are unchanged; the panels now exist in every mode but are shown per mode (Map Info: VQ3/CPM = Overbounces, Tricks/Fast Caps = Game Mode + Movements; Gameplay + Weapons: Tricks/Fast Caps; Timer: Tricks), as in the original.
BEHAVIOUR CHANGES: (1) "Any" type no longer drops DeFRaG maps without a type key (the original mask 60 did). (2) START always writes the mode's cvars (df_promode, df_mode_*) first, the original only wrote them when a panel widget was touched. (3) New cvars df_ui_mapsMode / df_ui_mapsType / df_ui_mapsLast (archived). Revert: restore the previous ui_startmap.c / ui_menu.c from git history.
Follow-up: mode chip ALL (default, `df_ui_mapsMode` 0..4 = All/VQ3/CPM/Tricks/Fast Caps; `df_ui_mapsPlay` = mode ALL starts a map in, a "Play as" spin shows it). Maps that no map file lists (`maps/*.bsp` of the game folder and pk3s via `trap_FS_GetFileList`, e.g. map packs without .defi) are added as db 3 "unlisted" (VQ3/CPM/Tricks). `ui_dfmaps.c` limits raised (DeFRaG): file list buffer 16 KB -> 256 KB (it held ~1000 of ~2000 `scripts/*.defi` names, the rest was never read), map file 8 KB -> 64 KB, entries per db 4096 -> 8192.
Best times: `ui_dfbest.c` reads the demo names (`<map>[<game>.<physics>]<mm.ss.mmm>(<name>.<country>).dm_68`; the list comes from `df_demos_index.dat` whatever its age, else the recursive listing; `demos/` and `df_ar_targetfolder` are read again at every open) and keeps the lowest time per map for run (df/mdf), fast caps (fc/mfc) and tricks (tm) x vq3/cpm; demos of other kinds ([cpm.seg], [..cpm.tr]) are skipped. "Yours" = the player name in the file name equals the client `name` (colours removed, letters and digits only, no case). Other names that count as you: `df_ui_maps_playerNames` (comma separated, same comparison); without it only the current name counts. The row shows the time of the mode the row starts in (ALL: the Play as mode); WATCH / Ctrl+Enter plays it with the demo browser's commands (`df_ghosts_Command`) and `df_ui_mapsReturn` / `df_ui_mapsSearch` bring the map browser back afterwards (`UI_DF_MapsReturnAfterPlayback`). Note: DeFRaG's autorecord names the per-map folder without the map's leading 'd' / 'dd' / 'm' (`demos/aemond-run2/daemond-run2[...]`), the map is therefore taken from the file name.
