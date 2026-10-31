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

### Menus
- Credits moved from the main menu list to a small dim link at the bottom of the screen
- Smooth menu mouse cursor: one pixel per mouse count (`df_ui_mouseSpeed` to adjust)
- Update check in the main menu (oDFe only): shows a new release, click to download it, installed at the next start (`odfs_update_check 0` disables)
- Engine version and update line in the main menu (oDFe only): click to download the oDFe release for your platform and replace all oDFe files in the game folder (OpenGL, Vulkan and server exe), used after a restart (`odfe_update_check 0` disables)

### Plugin System
Hooks for oDFe native plugins, so features can live outside the engine

