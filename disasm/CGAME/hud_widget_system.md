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

---

## Iter 22 — local record subsystem (source of best time / checkpoint reference): first decode
**File format** (`0x4e9c` reader, `0x4f46` writer; checksum `0x4e75(buf, n)` = plain byte sum `(unsigned char)`): header 4 bytes `{ version=3, checksum8 = sum(data[0..0x83]), junk, junk }` followed by `0x84` bytes of data (33 ints). Reader accepts files <= 0x200 bytes: v3 (checksum over the 0x84 bytes, error `GetRecord(): %s : v3 file is corrupted.`), v2 (older: 0x24 data bytes = 9 checkpoint ints, checksum over 0x24, upgraded by copying the old field `+0x20` to `+0x80`, error `...v2 file is corrupted.`), anything else `GetRecord(): %s : unknown version.`; a missing file leaves the buffer zeroed.
**Data layout** (33 ints): `rec[0..31]` checkpoint times in ms (index = checkpoint number, the finish time is stored at index `n`), `rec[32]` (offset `0x80`) = `n` = number of checkpoints. `0x4f8c()` = `rec[rec[32]]` = the record's **finish time** (the "best time" shown by widget W1); `0x11d5b0` = best/reference record `A`, `0x11d6b8` = current run `B`, `0x11d738` = `N` (current checkpoint index) — note `0x11d630` (= `A + 0x80`) is `A[32]`.
**Paths**: `system/records/<df_recfiles_session>/<name>.rec` (`"%s/%s"` of `"system/records"` and the session cvar `0x11baa4`, which is `df_recfiles_session.string`) built by the template expander `0x586d(dst, 256, "$map_$-route_$gt_$phys_$-mode", dir, "rec", 0)`; the latest run is also written to `system/records/latest/<name>.rec`. Template variables (`0x5543`): `$map` mapname, `$phys` (`vq3`/`cpm`/`VQ3`/`CPM` by `0x11d2b0`), `$gt` short gametype name by `defrag_gametype` (`ac, df, tm, fc, res, mdf, mtm, mfc`), `$plc` = `df_country`, `$pl` player name, `$route` (`defrag_route`), `$mode` (`%i` of `defrag_mode`, omitted when -1), `$ms` ms time; a `-` after `$` (`$-route`) means "prefix with `-` only when non-empty". Not yet decoded: the cleanup pass in `0x586d` (doubled `_`, illegal characters replaced by `-`, extension handling) and `0x5431`/`0x5504` (copy-with-limit helpers).
**Run logic** (`0x5248(snap, flags)` + events from `0x1c8b`): on start events the current record `B` is zeroed and the reference `A` is (re)loaded (`0x5092`, via the session dir); each checkpoint event stores the decoded server time (`0x5fd7`) into `B[N]` and shows the popup `0x50b6`, `N++`; on finish: `B[N] = time`; if the run is better than `A` the record gets event bits `0x200` (new record) / `0x400` (tie, `df_tieOverwrites`) and `B` is saved to the session record and to `latest`; the checkpoint popup `0x50b6` is called again for the final entry.
Port plan: implement `CG_DF_LoadRecord/SaveRecord` (v3 only + v2 read), the template for the file name, `A`/`B` handling, and feed W1 (best time) and `df_checkpoints 2` delta mode (replacing the current "no reference" fallback in cg_defrag.c).

---

## Iter 23 — record subsystem PORTED (`code/cgame/cg_dfrecords.c`)
Completed decodes: file-name expander `0x586d` (dir + `/`, colour codes `^N` dropped, `$var` expansion with `$-var`/`$+var` meaning "erase the preceding / skip the following separator when the variable is empty", illegal characters `<=0x20 >=0x7e : * ? " < > | \ /` -> `-`, extension appended with a `.`), `0x5431` (format + sanitised copy with max length), `0x5504` (empty-variable separator handling), template variables `map phys gt plc pl route mode` (+ time/date ones, not needed here), the two reference loaders (`0x506c` personal record R2 `0x11d634` from `system/records` (or `/demos` while playing a demo), `0x5092` session reference R1 `0x11d5b0` from `system/records/<df_recfiles_session>/`), and the run state machine `0x5248`: **start** (stats[12] bit 4 set): if a record was written last run, R2 := B and R1 reloaded, else (keepLatest) R1 reloaded; B cleared; R1 also reloaded when `df_recfiles_session` changed; **checkpoint**: `B[N] = t; popup(N); N++; B[N] = t`; **finish**: `B[N] = t`; if the player is not a spectator: `best = R2[R2.count]`; `best == 0 || t < best` -> new record, `t == best` -> tie; `df_recfiles_keepLatest` -> also write `system/records/latest/<name>.rec`; a new record (or a tie with `df_tieOverwrites`) writes `system/records/<name>.rec` and marks R2 stale; then popup(N). `0x11d738` (N) is `B[32]`. The checkpoint text (`0x50b6`/`0x5153`) uses R1 for `df_checkpoints 2` (delta, via the plain `0x10c8` formatter = signed short time) and, for the split at index 0, reads `B[-1]` = R2's count word exactly like DeFRaG.
Behaviour change: oDFs cgame now creates/reads `system/records/*.rec` (same format as DeFRaG's, so the files are interchangeable) on servers publishing the plain race time (`defrag_gametype >= 5`). Files are only written on finish. New cvars `df_recfiles_session` (""), `df_recfiles_keepLatest` (0), `df_tieOverwrites` (1). Syntax-checked, untested. Revert: remove `cg_dfrecords.c` + hooks (git).
Not ported: `0x11d40c` sticky state (unknown role), `0x4f9d` summary, per-gametype `defrag_flags` gating, the demo (`.dmq`) paths, `$ms/$date/...` template variables.

---

## Iter 24 — HUD PORTED (first pass): `code/cgame/cg_dfhud.c`
`CG_DF_HudInit` (shader registration + static widget table), `CG_DF_DrawWidget` (exact `0x259a`), `CG_DF_DrawHUD` (frame population from `0x2a12`): best time (reference record R1 finish time, wide 164x32 box above 60 s), last run time (set by the record finish), speed text (`0x41b6` formulas; the unit helper `0x1002` is `v*0.027` / `v*0.000016`, i.e. `CG_ConvertDistance` applied to `3.6*speed` / `3600*speed`), power-up timer list (3 slots, ids 1..6, add/remove/stack by 42 px exactly as `0x225b/0x22ea`), health (with the smoothed health value and the red/blue glow of `0x235d`, literal formulas incl. DeFRaG's negative-alpha red halo below 100), armor, ammo (low-ammo thresholds MG/LG/PG 25, SG/GL/RL/RG 4, BFG 6; icon = `cg_weapons[w].weaponIcon`), frags (`%2i/%2i`, hidden when fraglimit 0 or `defrag_flags & 1`), checkpoint line + list (`0x4f9d` summary chooses the large/small background, list drawn by `0x5153` at `x+170/120`, `y0+8`). Hooks: `CG_Draw2D` draws `CG_DF_DrawHUD()` and skips `CG_DrawStatusBar` / `CG_DrawAmmoWarning` / the stand-alone checkpoint list when `df_hud` is non-zero.
cvars (DeFRaG names/defaults): `df_hud` (**default 0 in oDFs**, DeFRaG default 1 — the images live in the DeFRaG pk3), `df_hud_drawStatus, df_hud_color 4, df_hud_colorRGB, df_hud_fontcolor 7, df_hud_fontcolorRGB, df_hud_fontshadow 1, df_hud_transparency 0.25, df_hud_frameTransparency 1.0, df_hud_colorwarning 1, df_hud_forceteamcolors 1, df_drawBesttime 1, df_drawSpeed 1`.
**Behaviour change:** none unless `df_hud 1`; with it the vanilla status bar is replaced. Requires the `df_hud/*` images (DeFRaG pk3) — without them the boxes draw as the default shader. Revert: `df_hud 0`.
Not ported yet: flag-carrier box (CTF), holdable-item box (its icon table is the unresolved quirk above — the vanilla holdable draw is kept), jump meters (tracker `0x458e` undecoded), spectator-follow name (`0x21d2`), the other helpers `0x2029`, `0x1a57a`, `0x1f53` (= a copy of `CG_DrawCenterString`, vanilla kept), `0x216a` (= `CG_DrawTimer`), `0x20dd` (= `CG_DrawFPS`), `0x1c966` (= `CG_DrawCrosshairNames`), `0x1c9f7`, `0x1c4eb`; the sticky state `0x11d40c`; the accel-meter entry (`0x79ac`). Syntax-checked, untested.

## Iter 32: spectator-follow name `0x21d2` DECODED & PORTED
Guards: cvar `df_hud_drawSpectFollow` (`0x11a9a0`, default 1) != 0 and `snap.ps.pm_flags & PMF_FOLLOW` (snap+0x38 & 0x1000). Colour = white; if the cvar is > 1 a timestamp `0x11ccec` (0 -> `cg.time+3200`; `cg.time > stamp` -> hidden) drives a 3.2 s display with alpha `(stamp-time)/200` over the last 200 ms; the stamp is zeroed in CG_TransitionPlayerState when `ps.clientNum` changes (`0x27046`). Text `va("^4>>^7 %s", clientinfo[ps.clientNum].name)` via CG_DrawStringExt at x = 320 - 10*len/2, y = 400, 10x16 chars, shadow, no force-colour. oDFs: `CG_DF_DrawFollowName` at the end of `CG_DF_DrawHUD` (cg_dfhud.c; only with `df_hud 1`); stamp reset in `CG_DF_SnapshotTransition`. Note: `len` is the raw string length incl. colour codes (`0x2f24d` verified in bytecode = plain `strlen`, so colour codes count towards the width; ported as strlen). Revert: remove the call/function. Syntax-checked, untested.

## Iter 33: jump meters `0x2e7c..0x2fcc` DECODED exactly & PORTED (also corrects the iter-20 sketch)
Cvar `df_drawJumpmeters` (`0x11c210`, default 0; bit1 height row, bit2 distance row, bit4 auto-hide). `T` = `0x11d254` (**1 = on ground, 0 = airborne**), `H` = `0x11d248` (max rise), `D` = `0x11d244` (horizontal distance from take-off), `big` = `H>100 || H<-100 || D>400`. Latches: `0x11ccdc` (H), `0x11cce0` (D), expiry `0x11cce4`, show `0x11cce8`. Without bit4: latches = (int)H/(int)D every frame. With bit4: show==0 -> show=1 if `!T && big`; show && expiry -> (`!T && big` -> expiry=0), then if expiry < cg.time: expiry=0, show=0; show && !expiry -> if `T` or not big: expiry = cg.time+1000, else latch H/D. Rows drawn when `flag-bit && (!bit4 || show)`: 16x16 chars via `0x1d461(x,y,str,alpha)` = `CG_DrawStringExt(x,y,str,{1,1,1,a},0,1,16,16,0)`; label (`"Height"` x=539 / `"Distance"` x=507) at `y+2`, then y += **16**, value right-aligned at `x = 635 - 16*strlen` at the new `y+2` (i.e. below the label), then y += **20**. y starts at 4.0, first advanced by `0x216a`/`0x20dd` when `*0x171b4`/`*0x170a4` are set (two UNDECODED/unported elements above the meters; oDFs starts at 4). Value text `0x4247(v, unit==1)`: `%im` of metres if `df_unit==1`, else `%iu` (miles never used for the meters). oDFs: `CG_DF_DrawJumpMeters` (cg_defrag.c) called from `CG_DF_DrawHUD`.
**Bug fix in the iter-31 tracker port:** `0x45d5` returns when `0x11d254 != 0` (distance/rise are computed only while AIRBORNE); the port had the test inverted. Fixed; Highest Jump now follows the original. Syntax-checked, untested.

## Iter 34: `0x216a` / `0x20dd` = timer and FPS, restacked in the DeFRaG HUD — PORTED
`0x171b4` = `cg_drawTimer` and `0x170a4` = `cg_drawFPS` (cvar-table rows at data `0xe0`/`0xf0`: vmCvar ptr `0x171a8`/`0x17098` +12). `0x216a(y)`: `msec = cg.time(0xea11c) - cgs.levelStartTime(0xf6f48)`, text `"%i:%i%i"` (min, tens, ones), right-aligned at `635 - 16*CG_DrawStrlen` (`0x1d514`, skips `^x`), `y+2`, returns `y+20` — identical to vanilla `CG_DrawTimer`. `0x20dd(y)` = vanilla `CG_DrawFPS` (4-frame average, `"%ifps"`, drawn once > 4 samples). Difference from vanilla: inside the DeFRaG HUD frame (`0x2a12`) they are called from y = 4 in the order timer, FPS, then the jump meters continue at the returned y. oDFs: with `df_hud 1` (not spectator, no scoreboard) `CG_DrawUpperRight` skips the vanilla FPS/timer (`CG_DF_HudDrawsUpperRight`) and `CG_DF_DrawHUD` draws them from y=4 before `CG_DF_DrawJumpMeters( y )`. Behaviour change only with `df_hud 1`: timer/FPS move to y=4 and swap order (vanilla FPS-then-timer, and below snapshot/team overlay). Revert: restore the vanilla block in CG_DrawUpperRight / delete the stacking block. Syntax-checked, untested.

## Iter 35: `0x1c9f7` = DeFRaG CG_DrawVote, `0x1c966` = vanilla CG_DrawCrosshairNames — vote timing PORTED
`0x1c966`: cg_drawCrosshair/cg_drawCrosshairNames gates (`0x16b54`/`0x16a44`), scan (`0x1c8f7`), fade (`0x1d63a(crosshairClientTime,1000)`), name centred at `320 - 16*strlen/2`, y=170 (0xaa), alpha*0.5 — vanilla, nothing to port (the DeFRaG HUD merely calls it from its tail). `0x1c9f7`: if `cgs.voteTime(0xf6318)`: talk beep when modified (`0xf6324`, sound `0x114248`), `sec = (cgs.voteTime - cg.time)/1000` clamped at 0 (**voteTime is the DEADLINE**, matching the DeFRaG qagame CheckVote), text `"VOTE(%i):^3%s^7 yes:%i no:%i"` (vote string `0xf6328`, yes `0xf631c`, no `0xf6320`) via small string at (0,58) — no "or press ESC" line. oDFs: `CG_DrawVote` now uses the deadline formula when `CG_DF_VoteIsDeadline()` (serverinfo `defrag_gametype>=5` && `df_vote_system`), else the vanilla `VOTE_TIME - (cg.time - voteTime)`; colour codes ^3/^7 added always. qagame: `df_vote_system` now also CVAR_SERVERINFO so the cgame can tell (BEHAVIOUR: one more serverinfo key). Without this fix the oDFs cgame would have shown ~60 s for a DeFRaG-style vote. Revert: restore the vanilla `sec` line and drop the SERVERINFO flag. Syntax-checked, untested.

## Iter 36: remaining HUD tail helpers classified (no code change)
- `0x2029` = **vanilla `CG_DrawCrosshair`** relocated into the DeFRaG HUD tail: gates `cg_drawCrosshair` (`0x16b54`), `ps.persistant[PERS_TEAM]==TEAM_SPECTATOR`, `cg.renderingThirdPerson` (`0xea134`), `cg_crosshairHealth` (`0x162d4`, health colour via `0x1d744` else white `0x8fe1`), size `cg_crosshairSize` x refdef, `CG_AdjustFrom640` (`0x1d12a`), shader `cgs.media.crosshairShader[ch % 10]` (`0x114050`), `trap_R_DrawStretchPic` (-0x2f). Same structure as vanilla; nothing to port.
- `0x1c966` = vanilla `CG_DrawCrosshairNames` (iter 35).
- `0x1c4eb` = `CG_DrawLagometer` (string `"snc"`, helpers `0x1d12a` AdjustFrom640, `0x1d28f` FillRect, `0x1d461`, `0x1c475`): structurally the vanilla function; a per-instruction constant comparison against ioq3 was NOT done (hash differs because of relocated callees), so a small DeFRaG tweak cannot be ruled out.
- `0x235d` = health icon + halo, already ported in iter 24 (`cg_dfhud.c`).
So the DeFRaG HUD frame (`0x2a12`) is now fully accounted for except the flag-carrier box, the holdable-item box (icon table quirk) and CGAZ `0x1a57a`.
