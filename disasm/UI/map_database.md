# ui map database (start-map menu backend) — ui 0x1378 / 0x13a0 / 0x1444 / 0x1716 / 0x17b8

Decoded with `disasm/tools/qdec.py` + data table at ui data 0x6b4 (2 x 28-byte records).

- Record: `{type, entries*, list*{count, head, loaded}, max=0x1000, name, file, ext}`. Type 1 `"defrag"` ext `.defi` (no fixed file); type 2 `"quake3"` file `scripts/arenas.txt` ext `.arena`.
- `0x1378` find db by type. `0x1716` load: read the fixed file (if any), then every `scripts/*<ext>` (`trap17` FS_GetFileList), print `Found %i %s maps.`, set loaded.
- `0x1444` MapList_ReadMapDef: file must be < 8192 bytes ("file too large", "^1MapList_ReadMapDef(): %s not found."); every record must start with `{` ("%s: Missing opening {"). Keys: `map`(+0,32) `longname`(+32) `author`(+64) `vq3`/`cpm` (value!=0 → flags 1/2) `type`|`style` (run 8, training 4, level 16, accuracy 32, token containing "ctf" 64) `revision` (sets "require maps/<map>.bsp"; value token then skipped as an unknown key); unknown key → skip rest of line. flags&3==0 → |=1. Entry kept only if map non-empty and (no revision key or bsp exists), else zeroed.
- `0x13a0` sorted insert by `Q_stricmp(map)` into the +100 `next` list. Entry = 104 bytes (+96 flags).
- `0x17b8` iterate: `(type, maskA, maskB, prev)` → next entry with `(flags&maskA)&&(flags&maskB)`; loads the db on first use.

Ported: `code/q3_ui/ui_dfmaps.c` (`UI_DF_MapNext`), flags `UIDF_MAP_*` in ui_local.h. Only caller will be the start-map menu (0x3825), not ported yet. QVM build clean, untested. Revert: drop ui_dfmaps.c from CMakeLists.
