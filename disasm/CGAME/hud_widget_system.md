# DeFRaG cgame HUD widget system — `0x2a12` (CG_Draw2D hook), `0x259a` (widget draw)

Started in the full-port loop (iter 15) after the user chose "exact widget-system decode" for the HUD port. WORK IN PROGRESS — this doc records only what the bytecode read so far proves.

## Structure
`0x2a12` (frame `0x88`) is the DeFRaG `CG_Draw2D` replacement. After global gates (`0x160b4` != 0, `ps.persistant[14]` (`snap+0x130`) == 3 -> draw nothing; == 2 with `0x11a890` -> fixed colour) it reads colour/scale cvars through `0x24ac(cvarVmCvar, outFloat)` (a cvar -> float helper, returns 0 on a bad/empty value; e.g. `0x11ccb4` HUD alpha, `0x11ccc4` second alpha) and then, per HUD element, **fills a global widget struct and calls `0x259a(&widget)`**:
- `0x11ccf0` : timer-state widget, populated at `0x2a8c`..`0x2ace`: field `+0x2c(0x11cd1c...)` etc. Text for the timer comes from `0x1024(0x4f8c())` (`0x4f8c` = current stage timer value, `*(0x11d5b0 + stage*4)`), stored in `0x11cd04`; a width/height pair is chosen from the display scale (`0x4f8c() > 0xea5f` (60000 ms) -> `0x100` wide, text size 9 with fonts `0x11d170/0x11d174`; else `0x80` wide, size 6 with `0x11d180/0x11d184`).
- `0x11cd44` : second widget (best-time / elapsed `0x11d284` formatted by `0x1024` into `0x11cd58`), gated by `0x11d284 != 0`.
- `0x11cd98` : third widget, uses `0x41b6` (speed/ups formatter) result `0x11cdac`, gated by cvar `0x11cdcc`.
- later: `0x4f9d` (checkpoint summary, 4 calls), `0x5153` (checkpoint list, 2 calls), `0x668c` (scoreboard), meters via `0x1f53`/`0x2029`/`0x20dd`/`0x216a`/`0x21d2`/`0x225b`/`0x22ea`/`0x235d` (jump/distance/accel meter family; strings "Height", "Distance", "%2i/%2i", "ag").

## `0x259a(w)` — widget draw/animation (decoded structure, field names provisional)
Early-out if `*0x11aab0 == 0` (HUD on). `w` fields (offsets from the loads/stores):
| off | use |
|---|---|
| +0x00 / +0x04 | resolved screen x / y (local `x`,`y` = locals 0x2c/0x30 are written back here in state 1) |
| +0x08 / +0x0c | width / height of the box (passed to the box fill `0x1d28f`) |
| +0x14 | text pointer (strlen via `0x2f24d`; drawn with `0x1d330` = CG_DrawStringExt-like, char size 16, right-aligned by `16*len`) |
| +0x18 | integer value; type 3 formats it with the strings `0x78da`/`0x78d6` through `0x32755` (sprintf) |
| +0x20 | content type (0 none, 3 = integer value) |
| +0x24 | optional label string (drawn first, `0x41800000` = 16.0 char size) |
| +0x2c | colour pointer for the box (`0x1d28f` last arg) |
| +0x34 | animation state: 0 = idle/hidden, 1 = shown/steady, 2 = sliding in (start at `+0x44/+0x48`, end `+0x4c/+0x50`), 3 = sliding out |
| +0x38 | state start time (cg.time `0xea11c` at transition) |
| +0x3c / +0x40 | slide velocities x / y (units per 500 ms: `pos = start + vel * 0x1f4 * (time - t0) / 0x3e8` form) |
| +0x44..+0x50 | start x,y and end x,y of the slide |
Box fill: `SetColor(-0x2e)` + `0x1d28f(x, y, w, h, color)` twice (outline/background), colours from `0x11a44c` / `0x11a33c` vs `0x40c4..0x40d0` / `0x4094..0x40a0` blocks chosen by `0x11ade0 == 1` (theme), alpha/scale from `0x2559(cvar, 0x11ccb4)`.
NOT yet decoded: exact meaning of states 2/3 transitions, the clamps against 640x480, what `0x12d0` is (a sentinel pointer tested against `w+0x44`), and each element's population code.

## Next steps
1. Finish `0x259a` line by line (the 0x2693-0x27fa slide logic).
2. Decode each element population in `0x2a12` (`0x2a8c`-`0x2fdb`), starting with the timer widget (`0x11ccf0`).
3. Port as `CG_DF_DrawHUD` + `CG_DF_Widget*` in `cg_defrag.c`.

---

## Iter 17 — `0x259a` FULLY DECODED (supersedes the provisional field table above)
Method: the `.dis` carries lcc pseudo-C (`;; dec:` lines); `load4` ops are hidden by naive filters — read them before trusting a field. cvar storage addresses were mapped with the cvar table (rows `[vmCvar, name, default, flags]` incl. rows with vmCvar 0; `.integer` = base+0xc, `.value` = +8, `.string` = +0x10).

### Globals
| addr | meaning |
|---|---|
| `0x11aab0` | `df_hud_drawStatus.integer` (0 -> draw nothing) |
| `0x11ade0` | `df_hud_color.integer` (palette index, default 4) |
| `0x11abc0` | `df_hud_colorwarning.integer` |
| `0x11a780` | `df_hud_fontcolor.integer` (default 7) |
| `0x11a560` | `df_hud_fontshadow.integer` |
| `0x11a44c` / `0x11a33c` | `df_hud_transparency.value` (0.25) / `df_hud_frameTransparency.value` (1.0) |
| `0x11a890` | `df_hud_forceteamcolors.integer` |
| `0x11acd4` / `0x11a674` | `.string` of `df_hud_colorRGB` / `df_hud_fontcolorRGB` -> parsed by `0x24ac` into `0x11ccb4` / `0x11ccc4` |
| `0x11c320` / `0x11c100` | `df_drawBesttime.integer` / `df_drawSpeed.integer` |
| `0x11c644` | `df_checkpoints` vmCvar (`.integer` = `0x11c650`) |
| `0x12d0` | a read-only float `0.0` in the data segment (compiled comparison constant) |

### `0x24ac(str, out[4])` — parse an "R G B" colour string
Skips to digit runs, reads up to 3 decimal numbers (`atoi`-shaped), stores `v / 255.0f` (`0x437f0000`) into `out[0..2]`; `0x2a12` sets `out[0] = -4.0f` (`0xc0800000`) when parsing fails (= "no custom colour").
### `0x2559(vmCvar *c, float *custom)` — HUD colour lookup
`if ( custom[0] != 0 && !(custom[0] < 0) ) return custom;` else `i = c->integer; if (0 <= i < 8) return palette0[i]; if (8 <= i < 13) return palette1[i-8]; return white;` with `palette0` (data `0x4144`): 0 black, 1 red, 2 green, 3 yellow, 4 blue, 5 cyan, 6 magenta, 7 white (all alpha 1); `palette1` (`0x12d4`): 8 grey (.25,.25,.25), 9 orange (1,.5,0), 10 purple (.5,0,.6), 11 dark green (0,.4,0), 12 olive (.6,.6,.1); default white (`0x40f4`).

### Widget struct (offsets verified from loads/stores)
`+0x00 x, +0x04 y, +0x08 w, +0x0c h, +0x14 text (char*), +0x18 value pointer (int*), +0x1c warning threshold (int), +0x20 field width in chars (also text maxChars; 3 selects "%3i", otherwise "%2i"), +0x24 icon shader, +0x28 frame shader, +0x2c fill shader, +0x30 visible, +0x34 mode, +0x38..+0x50 slide-animation scratch`.

### Behaviour
```c
void Widget_Draw( w ) {
    if ( !df_hud_drawStatus ) return;
    if ( !w->visible ) { w->mode = 0; return; }       // non-visible: nothing drawn
    w->mode = 1; x = w->x; y = w->y;                  // the slide-in/out code (0x26ab..0x27e8) is unreachable: guarded by a compare against the constant 0.0 at 0x12d0
    if ( w->value && df_hud_colorwarning && *w->value < w->warn )
        col = ( df_hud_color == 1 ) ? yellow(1,1,0,1) : red(1,0,0,1);
    else col = HudColor( df_hud_color, customRGB );   // palette entry
    col.a = df_hud_transparency;       SetColor(col); DrawPic( x, y, w->w, w->h, w->fill );
    col.a = df_hud_frameTransparency;  SetColor(col); DrawPic( x, y, w->w, w->h, w->frame );
    SetColor( NULL );
    x += 10; y += ( w->h - 16 ) / 2;
    if ( w->icon ) { DrawPic( x, y, 16, 16, w->icon ); x += 26; }
    fontCol = HudColor( df_hud_fontcolor, customFontRGB );
    if ( w->text )       DrawStringExt( x + ( w->width - strlen(text) ) * 16, y, w->text, fontCol, qtrue, df_hud_fontshadow, 16, 16, w->width );
    else if ( w->value ) DrawStringExt( w->x + 36, y, va( w->width == 3 ? "%3i" : "%2i", *w->value ), fontCol, qtrue, df_hud_fontshadow, 16, 16, w->width );
}
```
(`0x1d28f` = `CG_DrawPic`-shaped, `0x1d330` = `CG_DrawStringExt`-shaped with the vanilla argument order x, y, string, color, forceColor, shadow, charW, charH, maxChars.)

## Next
Decode each element's population in `0x2a12` (which fields/shaders/strings per widget, positions, visibility conditions), starting with the timer widgets `0x11ccf0` / `0x11cd44` / `0x11cd98` (offsets listed above), then port as `CG_DF_DrawWidget` + elements.

---

## Iter 18 — HUD assets and widget initialisation (`0x2fdd`, 0x3ab bytes) and the first three widgets of `0x2a12`
`0x2fdd` is the HUD init: `memset(0x11cca4, 0, 0x58c)` (all widget state), registers the **`df_hud/*` shaders** (they live in the DeFRaG client pk3, not in oDFs) into globals (handles read through pointers by `0x259a`), then writes the static widget fields.

Shader globals (name -> global): `d134 icon_null, d138 icon_health, d13c icon_health_haloRed, d140 icon_health_haloBlue, d144 icon_armor, d148 icon_frags, d150 icon_quad, d154 icon_bs, d158 icon_haste, d15c icon_invis, d160 icon_regen, d164 icon_flight, d168 icon_teleport, d16c icon_medkit, d170 164x32_border, d174 164x32_inner, d178 126x32_border, d17c 126x32_inner, d180 116x32_border, d184 116x32_inner, d188 94x32_border, d18c 94x32_inner, d190 78x32_border, d194 78x32_inner, d198 36x32_border, d19c 36x32_inner`; arrays of 9: `d1a0[i] ckptbg_s_border_%i, d1c4[i] ckptbg_s_inner_%i, d1e8[i] ckptbg_l_border_%i, d20c[i] ckptbg_l_inner_%i` (i = 0..8; the checkpoint list `0x5153` backgrounds).

Widget struct size is **0x54**; field `+0x24` icon, `+0x28` border, `+0x2c` inner are POINTERS to the shader globals above (double-deref in `0x259a`), `+0x18` value is a pointer to an int, `+0x14` text is a plain `char*`, `+0x3c/+0x40` = -1/0 slide velocities (dead).

Static init (x, y, w, h, width(+0x20), icon, border/inner, warn(+0x1c), visible(+0x30)):
| widget (base) | role | x | y | w | h | width | shaders | notes |
|---|---|---|---|---|---|---|---|---|
| `0x11ccf0` W1 | **best time** (`df_drawBesttime`) | 5 | 159 | 128 | 32 | 6 | 78x32 (overridden per frame, see below) | |
| `0x11cd44` W2 | **last run time** (`0x11d284`, set when a run ends) | 5 | 201 | 128 | 32 | 6 | 116x32 | |
| `0x11cd98` W3 | **speed** (`df_drawSpeed`, text from `0x41b6`) | 5 | 243 | 128 | 32 | 6 | 116x32 | |
| `0x11cdec` W4 | (population not yet read) | 5 | 438 | 128 | 32 | 0 | 36x32 | |
| `0x11ce40` W5 | (idem) | 5 | 438 | 128 | 32 | 0 | 36x32 | |
| array `0x11ce94` stride 0x54 | power-up/timer icons | 5 | 438 | 128 | 32 | 6 | 78x32; icon slot `+0x24`... | count in `0x11cca4` |
| `0x11cf90` | **health** | (x set per frame) | 438 | 128 | 32 | 3 | icon_null + 94x32 | warn=25 (`0x19`), visible 1 |
| `0x11cfe0` | **armor** | 199 | 438 | 128 | 32 | 3 | icon_armor + 94x32 | warn=20 (`0x14`) |
| `0x11d034` | (ammo?) | 305 | 438 | 128 | 32 | 3 | 94x32 | |
| `0x11d088` | **frags** | 504 | 438 | 128 | 32 | 5 | icon_frags + 126x32 | warn=20 |
| `0x11d0dc` | checkpoint list line | 498 | 438 | 256 | 32 | — | ckptbg arrays | |
(positions in 640x480 virtual pixels; `y=438` is the bottom status line.)

### First three widgets (population in `0x2a12`, bytes `0x2a8c`-`0x2b36`)
1. `visible(+0x30) = df_drawBesttime`; `t = 0x4f8c()` (the current stage's stored time, `*(0x11d5b0 + stage*4)`); if `t == 0` the widget is hidden, else `text = FormatTime(t)` (`0x1024`) and: if `t > 0xea5f` (60 s) -> `w = 256`, `width = 9`, shaders 164x32; else `w = 128`, `width = 6`, shaders 116x32.
2. W2: `visible = (0x11d284 != 0)`; `+0x10 = 0x11d284` (ms), `text = FormatTime(that)`, same wide/narrow choice by the value.
3. W3: only when the widget's `mode(+0x34)` is non-zero (it was drawn last frame) `text = 0x41b6(speed)`; visible from `df_drawSpeed` (`0x11c100`).
Then `0x259a` is called for each. Open: where `0x11d5b0[stage]` (the "stored time") comes from, the exact `0x41b6` format, and the remaining widgets (health/armor/ammo/frags/powerups/checkpoint backgrounds) — the rest of `0x2a12` (`0x2b36`-`0x2fdb`).

---

## Iter 19 — `0x2a12` status-bar section `0x2b36`-`0x2cf8` decoded
Reading convention for the raw tokens: `loc:X` without a following `load4` pushes the ADDRESS of the local (that is how `+0x18` "pointer to int" fields get a stack local that is only valid inside the same frame); `local28` = `ps` (= `snap + 0x2c`), so `ps+0x138` = `ps.powerups[]`, `ps+0x178` = `ps.ammo[]`, `ps+0xb8` = `stats[]`, `ps+0x90` = `weapon`. Widget array: base `0x11ce94`, stride `0x54`, field `+0x10` = **powerup id**, `+0x18` = pointer to the seconds-left int, `+0x1c` warn = 6 s, `+0x20` width 2 (`"%2i"`), `+0x30` visible; list `0x11cca8[]` = widget indices in display order, `0x11cca4` = entries in use (max 3 slots are filled by the first loop, ids 0..6 scanned).

```c
// 1. add newly active powerups (ids 0..6) while fewer than 3 are shown
for ( id = 0; id <= 6 && nShown < 3; id++ ) {
    if ( ps->powerups[id] == 0 ) continue;
    if ( already in list (widget[list[k]].id == id) ) continue;
    AddPowerupWidget( id );                      // 0x225b
}
// 2. remove expired ones
for ( k = 0; k < nShown; k++ ) if ( ps->powerups[ widget[list[k]].id ] == 0 ) RemovePowerupWidget( k );   // 0x22ea (index shifts, loop continues)
// 3. update + draw each
for ( k = 0; k < nShown; k++ ) {
    secs = ( ps->powerups[ id ] - cg.time ) / 1000 + 1;  if ( secs > 99 ) secs = 99;
    w->value = &secs; w->visible = 1;  Widget_Draw( w );
}
```
Then (`0x2c16`): **flag carrier box W4** (`0x11cdec`): shown when `ps.powerups[7]` or `ps.powerups[8]` (the CTF red/blue flag slots) is non-zero; `kind = powerups[7] ? 2 : 1`; `W4.y = 438 - 42 * nShown` (`0x1b6 - 0x2a*n`), `visible = 1`, `Widget_Draw(W4)`, then a pic of the carried flag: if `0x11aef0` (cvar `.integer`, name not yet mapped) is 0 -> `DrawPic( W4.x + 10, W4.y + 8, 16, 16, flagShader[kind] )` else a 32x32 pic at `W4.x, W4.y`; `0x11ce40.x = 5`... Then **holdable-item box W5** (`0x11ce40`): shown when `ps.stats[1]` (holdable) != 0: `y = 438 - 42 * nShown`, `visible = 1`, `icon = (stats[1] << 2) + 0x11d100` (a pointer into a table at `0x11d100`; NOTE the init code also uses `0x11d100..0x11d10c` as the checkpoint-line widget's `+0x20..+0x2c` fields (that widget's base is `0x11d0e0`), so the literal table is the widget memory itself — looks like a DeFRaG bug/quirk; unresolved which table was intended: the holdable icons `icon_teleport` (`0x11d168`) / `icon_medkit` (`0x11d16c`) are the only matching shader globals), `Widget_Draw(W5)`.
Then **health widget** (`0x11cf90`): `value (+0x18) = &ps.stats[0]` (`ps+0xb8`, health), `visible = 1`, `Widget_Draw`; then `0x235d(...)` (health-halo / red-blue health glow, args `0x11cfd4+10`, `0x11cfd8+8`).  **Armor widget** (`0x11cfe0`): `icon (+0x24) = &icon_armor`-ish pointer chosen by `powerups[?]`... (`ps+0xf0` != 0 -> `visible(0x11d014)=1; icon = ps+0xc4`), `Widget_Draw`.
Next: ammo `0x11d034` (value = `ps.ammo[weapon]`, `ps+0x90` weapon index, struct `0x88`-stride table `0x19490`...), frags `0x11d088` (`PERS_SCORE`, `ps+0xf8`), checkpoint line `0x11d0e0` + `0x5153`, `0x2029` etc. (`0x2d3a`-`0x2fcc`) still to be written up.

---

## Iter 20 — speed text, jump meters, unit formatters, tail of `0x2a12`
- `0x1002(value, unit)`: unit 1 -> `value * 0.027f` (`0x3cdd2f1b`, metres), unit 2 -> `value * 0.000016f` (`0x378637bd`, miles), else value — identical to oDFs `CG_ConvertDistance` (speed is pre-multiplied by `3.6f` (`0x40666666`) / `3600.0f` (`0x45610000`) first, as in `CG_ConvertSpeed`).
- `0x41b6()` — **speed text** for widget W3. `speed = (float)*(0xec1f0)` (the horizontal speed computed each frame by the tracker `0x3a4a`: length of the predicted `ps.velocity` with z zeroed). `df_drawSpeed` (`0x11c100`) == 2 is the "scaled" mode: unit 1 -> `"%3i k"` of `speed * 20/320`; unit 2 -> `"%3i m"` of `speed * 20/320 * 0.625`; unit 0 -> `"%3i u"` of `speed`. Otherwise (`df_drawSpeed` != 2): unit 1 -> `"%3i k"` of `0x1002(3.6*speed, 1)`, unit 2 -> `"%3i m"` of `0x1002(3600*speed, 2)`, unit 0 -> `"%3i u"` of `speed`. (`unit` = `df_unit.integer` at `0x11b330`; strings `0x7ff5 "%3i k"`, `0x7fef "%3i m"`, `0x7fe9 "%3i u"`.)
- `0x4247(value, unit)` — **distance/height text**: unit 1 -> `"%im"` of `0x1002(value,1)`; unit 2 -> `0x1002(value,2)` miles printed as `"%i.%i%imil"` (int part, tenths, hundredths; `"-%i.%i%imil"` for negative); else `"%iu"` of `(int)value`. (Strings at `0x7fe5/0x7fda/0x7fce/0x7fca`.)
- **Jump meters** (`0x2e7c`-`0x2fcc`, flag word `df_drawJumpmeters` at `0x11c210`: bit 0 = height row, bit 1 = distance row, bit 2 = "auto-hide after a jump" mode): the current jump's max height `*0x11d248`→`0x11ccdc` and distance `*0x11d244`→`0x11cce0` are latched with a 1 s hold (`0x11cce4` = latch expiry `cg.time + 1000`, `0x11cce8` = "show" flag) when the jump ended with `height` in (-100, 100) ... and distance < 400 conditions on `0x11d254` (airborne flag). Rows are drawn with `0x1d461(x, y, text, alpha)` (a float-coordinate `DrawString`-shaped helper): label `"Height"` at x=`0x21b` (539) and `"Distance"` at x=`0x1fb` (507), y = `local30 + 2`, then the value string right-aligned at `x = 0x27b (635) - 16*len`; `local30` advances by 24 (`0x41800000`) / 20 per row (`0x41a00000` = 20.0 after the last).
- Tail calls (`0x2fcd`-`0x2fd9`), still undecoded: `0x1f53` (centered message, uses `0xee0fc/0xee500` = screen metrics, `0x17b40` seconds scale), `0x21d2`, `0x1c9f7`, `0x1c966`, `0x1c4eb`. Before the meters: `0x2029` (world-to-screen overlay using `0x16604/0x164f4` and `refdef` `0xea6bc..0xea6c8`), `0x1a57a`, `0x216a`, `0x20dd`.

---

## Iter 21 — status bar widgets (`0x2cb3`-`0x2e4a`) and the discovery of the local record subsystem
`ps` = `local28`; `defrag_flags` = `0x11d2b8` (oDFs publishes 2: bit 1 set, bit 0 clear); `cgs.fraglimit` = `0xf6248`.
- **Health** (base `0x11cf90`): `+0x18 = &ps.stats[0]`, `visible = 1`, `Widget_Draw`, then `0x235d( w.x+10, w.y+8 )` (health icon + red/blue halo glow, uses the shaders `icon_health*`; `+0x44/+0x48` hold the x/y that `0x259a` resolved).
- **Armor** (base `0x11cfe4`, x=199): `visible = ( ps.stats[3] != 0 )`, `+0x18 = &ps.stats[3]` when visible, `Widget_Draw`.
- **Ammo** (base `0x11d038`, x=305): `if ( health <= 0 ) visible = 0; else { visible = 1; w+0x10 = ps.weapon; }`; `+0x18 = &ps.ammo[ w+0x10 ]` (`ps+0x178`); `+0x24 icon = 0x19490 + 0x88*weapon` (a pointer to the weaponInfo_t element's icon handle: `cg_weapons[]` has stride 0x88); low-ammo warn threshold `+0x1c` by weapon via jump table `0x1334..0x1350` for weapons 2..9: MG(2)=25, SG(3)=4, GL(4)=4, RL(5)=4, LG(6)=25, RG(7)=4, PG(8)=25, BFG(9)=6; other weapons leave the field unchanged.
- **Frags** (base `0x11d08c`, x=504, width 5): shown when `cgs.fraglimit != 0 && !(defrag_flags & 1)`: `visible = 1`, checkpoint line `y := 396 (0x18c)` (and `+0x48 := 396` if its mode is 0), text `va("%2i/%2i", ps.persistant[0] /*score*/, cgs.fraglimit)` into `+0x14`; else `visible = 0` and the checkpoint line `y := 438`.
- **Checkpoint line** (base `0x11d0e0`; `y0 = 0x11d0e4` after the above): `n = 0x4f97()` (= `*0x11d738`, number of checkpoints); shown only if `df_hud_drawStatus && (defrag_flags & 2) && lastRunTime(0x11d284) != 0 && df_drawCheckpointStats && *0x11d40c == 0` (the last global's meaning is unknown): `v = min(n, 7)`; `visible = 1`; `y -= 16*v`; `h = 16*v + 0x20`; if `|0x4f9d()| >= 60000` use the "large" background pair `ckptbg_l_*[v]` at `x = 450 (0x1c2)` else the small pair `ckptbg_s_*[v]` at `x = 498 (0x1f2)`; `Widget_Draw`; then if the widget was drawn (`mode != 0`) call `0x5153( w.x + 170 (large) | + 120 (small), y0 + 24 - 16 )` = the checkpoint text list (CGAME/cgame_cvar_consumers.md).
- Then `0x2029()` (world overlay), `0x1a57a(*0x119ac0, *0x1199ac, *0x1198a0)`, `0x216a`/`0x20dd` (conditional), the jump meters, and the tail calls `0x1f53`, `0x21d2`, `0x1c9f7`, `0x1c966`, `0x1c4eb`.

### DeFRaG local record subsystem (new finding, NOT yet analysed)
`0x4e9c(path, buf)` reads a **record file** (FS_FOPEN read, 0x84-byte block = 33 ints) and `0x4f46` writes one (`-0xd` FS_WRITE); strings `system/records`, `system/records/latest`, `system/records/demos` (`0x844d..0x847c`), callers around `0x508d`/`0x50b1` (checkpoint code) and the timer-state update `0x5248`. This is where the **stored best-time array** `0x11d5b0[stage]` (used by the best-time widget W1 and the checkpoint delta mode `df_checkpoints 2`, array `A` in the notes) comes from: DeFRaG keeps per-map record files on the client. Porting W1 / delta mode needs this record load/save logic decoded first.
