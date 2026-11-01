# oDFs
Opensource DeFRaG server runtime

## New features

### Demo browser
- Search autocomplete: dropdown + Tab completion, Up/Down/Enter, click
- Search: case-insensitive "contains", globs (`*`, `?`, `**/`), sub folders
- Blazingly fast searching among tens of thousands of demos
- Persistent index file (`df_demos_index.dat`), new demos merged in, F5 rescans
- Scrollbar instead of arrow buttons
- Redesigned layout, useless settings removed

### Map browser
- One "Maps" main menu entry instead of Defrag VQ3 / CPM / Tricks Mode / Fast Caps: those are the mode filter next to ALL, the default (F1-F5 or Left/Right); ALL starts a map in the mode of the "Play as" spin
- Lists every map of the game folder and its pk3s, also map packs and maps without a .defi/.arena file ("unlisted")
- Search by map name, title or author (several words = all must match), live filtering, Up/Down/PgUp/PgDn while typing, Enter starts the map
- Match counts per mode next to the mode filter, type filter (run / training / level / accuracy)
- Your best time on each map at the end of its row (from your demos, matched by your current player name, old nicknames in `df_ui_maps_playerNames`, comma separated), WATCH / Ctrl+Enter plays that demo and returns to the browser
- Scrollable list with scrollbar, double click starts the map; mode, type and last map are remembered
- Info / Gameplay / Weapons / Timer as tabs (only the ones the mode has), server cheats and run server always visible

### Demo playback (oDFe)
- Pause (`SPACE`, `demo_pause`), speed (`UP`/`DOWN`, `demo_speed`), seek (`LEFT`/`RIGHT` +-5 s, `HOME`, `demo_seek 83.5 | 1:23 | +5 | 50%`) and a timeline with a mouse cursor: click or drag to seek
- Seeking forward and back without reloading the demo: the cgame takes the jump (`CG_ODF_TIME_CHANGE`), the engine keeps keyframes of the demo parser; other cgames fall back to a demo restart
- `+mouseseek`: hold it and move the mouse to scrub the demo, forward and back (`cl_demoMouseSeek` ms per mouse count); `LEFT` / `RIGHT` while held step one snapshot (server frame) back / forward

### Menus
- "Mods" (stock game mod switcher) is hidden from the main menu unless `df_ui_showMods 1`
- Credits moved from the main menu list to a small dim link at the bottom of the screen
- "Plugins" in the main menu: lists the client plugins of the plugins folder with version and state (ON / OFF / FAILED / CRASHED) and switches them on and off, also in a running game; the choice is kept in `plugins/disabled`
- Smooth menu mouse cursor: one pixel per mouse count (`df_ui_mouseSpeed` to adjust)
- Update check in the main menu (oDFe only): shows a new release, click to download it, installed at the next start (`odfs_update_check 0` disables)
- Engine version and update line in the main menu (oDFe only): click to download the oDFe release for your platform and replace all oDFe files in the game folder (OpenGL, Vulkan and server exe), used after a restart (`odfe_update_check 0` disables)

### Plugin System
Hooks for oDFe native plugins, so features can live outside the engine

