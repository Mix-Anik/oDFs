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

### Server browser
- Redesigned in the colours of the map and demo browsers: search (name, map, address; several words = all must match), source tabs (Internet / Local / Favorites / extra masters), scrollable list with scrollbar
- Mode column (run / tricks / fast caps) and a filter for it; asked from the servers' status in the background
- Click a column heading to sort, again to reverse; Enter or double click connects, F5 refreshes, SPACE stops
- Panel with the selected server: levelshot, address, map, mode, players, ping, version, password lock, player list
- Favorites can be added and removed from any list (FAVORITE button, `*` mark in the list); "Specify server" page replaced by an address box (JOIN, Ctrl+Enter saves it as a favorite)
- Gone: Game Type spinner (it never filtered), Punkbuster logo, net type column, old bitmap buttons

### Demo playback (oDFe)
- Pause (`SPACE`, `demo_pause`), speed (`UP`/`DOWN`, `demo_speed`), seek (`LEFT`/`RIGHT` +-5 s, `HOME`, `demo_seek 83.5 | 1:23 | +5 | 50%`) and a timeline with a mouse cursor: click or drag to seek
- Seeking forward and back without reloading the demo: the cgame takes the jump (`CG_ODF_TIME_CHANGE`), the engine keeps keyframes of the demo parser; other cgames fall back to a demo restart
- `+mouseseek`: hold it and move the mouse to scrub the demo, forward and back (`cl_demoMouseSeek` ms per mouse count); `LEFT` / `RIGHT` while held step one snapshot (server frame) back / forward

### Menus
- "Mods" (stock game mod switcher) is hidden from the main menu unless `df_ui_showMods 1`
- Credits moved from the main menu list to a small dim link at the bottom of the screen
- One "Settings" entry (main and in-game menu) instead of Setup, Defrag Setup and Plugins: every settings page has the same category rail on the left (General: Player, Controls, Video, Audio, Network, Game; DeFRag: HUD, Crosshair, Replays, Gameplay, Advanced; Plugins), a page title, one column layout; reopens on the page you used last; the CD Key page is gone
- Settings look: orange setting names, sliders you can drag with the value shown, Driver Info page redone
- Player page redone: coloured name, handicap slider, bigger 3D preview you can rotate, login/records section
- Player model picker redone: portrait grid with search and live preview on hover
- In-game (Escape) menu redone: small see-through panel, Join game / Spectate in one click, Settings, Disconnect
- Settings pages merged: Video = Graphics + Display (brightness, screen size), Replays = Demos + Ghost, Gameplay = Misc + Multiplayer, Advanced = Quake 3 Extras; "Automatic Downloading" moved to Network
- New menu colours (the dark red of the map and demo browsers): the `df_ui_*` colours that still have an old default are moved to them once (`df_ui_theme`), colours you changed stay
- "Plugins" page in Settings: lists the client plugins of the plugins folder with version and state (ON / OFF / FAILED / CRASHED) and switches them on and off, also in a running game; the choice is kept in `plugins/disabled`
- Smooth menu mouse cursor: one pixel per mouse count (`df_ui_mouseSpeed` to adjust)
- Update check in the main menu (oDFe only): shows a new release, click to download it, installed at the next start (`odfs_update_check 0` disables)
- Engine version and update line in the main menu (oDFe only): click to download the oDFe release for your platform and replace all oDFe files in the game folder (OpenGL, Vulkan and server exe), used after a restart (`odfe_update_check 0` disables)

### Plugin System
Hooks for oDFe native plugins, so features can live outside the engine

