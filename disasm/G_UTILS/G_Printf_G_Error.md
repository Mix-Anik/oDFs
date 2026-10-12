# `0x96` = `G_Printf`, `0xb1` = `G_Error` (qagame)

Both are `enter 0x41c` varargs wrappers with the same body (`0x96`-`0xb0`, `0xb1`-`0xcb`):
`va_start` (`local18 = &arg1`), `vsnprintf(buf=&local1c, 0x400, fmt=arg0, args)` via `func_0x633c`, `local18 = 0` (va_end), then a single syscall on the buffer:

| Fn | Syscall | Trap id (`-(id+1)` rule) | Meaning |
|---|---|---|---|
| `0x96` | `call -1` (`0xad`) | 0 | `G_PRINT` -> `trap_Print` (`G_Printf`) |
| `0xb1` | `call -2` (`0xc8`) | 1 | `G_ERROR` -> `trap_Error` (`G_Error`) |

Matches vanilla `G_Printf`/`G_Error` (g_main.c). `0x96` has ~110 callers (e.g. every `0x274db` debug print of `df_dbg_1mstimer`, the `0x23f2` sv_fps messages, `G_SpawnItem`'s paths);
`0xb1` is called from `0x43d 0x1153 0x44bc 0x1455e 0x1c6be 0x1e4ef 0x2147c ...` (e.g. "Couldn't find item for weapon %i" in `0x4b6a`'s callers).
Nothing DeFRaG-specific; oDFs uses the vanilla functions. Closes the "diag-print at `0x00b1`" leftover.
