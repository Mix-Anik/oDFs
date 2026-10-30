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

### Menus
- Smooth menu mouse cursor: one pixel per mouse count (`df_ui_mouseSpeed` to adjust)
- Update check in the main menu (oDFe only): shows a new release, click to download it, installed at the next start (`odfs_update_check 0` disables)
- Engine version and update line in the main menu (oDFe only): click to download the oDFe build of your own type (same renderer and architecture) and replace the running exe, used after a restart (`odfe_update_check 0` disables)

### Plugin System
Hooks for oDFe native plugins, so features can live outside the engine

