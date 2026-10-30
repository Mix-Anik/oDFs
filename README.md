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

### Plugin System
Hooks for oDFe native plugins, so features can live outside the engine

