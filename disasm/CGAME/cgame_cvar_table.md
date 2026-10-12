# cgame/ui batch cvar-registration table — methodology

**Source file:** `D:\Other\Reverse Engineering\q3vm\cgame.dis` (the same
pattern is expected in `ui.dis`, not independently re-verified there).

Most of `cgame.qvm`'s cvars (hundreds — standard `cg_*` plus all the
DeFRaG-specific `df_*` ones) are **not** registered via individually-coded
`trap_Cvar_Register(...)` call sequences. They're registered through a single
data-driven loop reading a big compile-time table. This matters for anyone
reverse-engineering a specific cvar in `cgame.dis`/`ui.dis`: **searching for
the cvar's name string as an immediate operand (`const 1 0x<addr>`) will find
nothing**, because the name is only ever reached indirectly through the
table. You have to go through the table instead.

---

## The registration loop

Entry at `0x000000fb` (called from `0xdc1`):

```c
void RegisterCvarBatch( void *table, int count ) {   // args: table ptr, count
    for ( int i = 0; i < count; i++ ) {
        void   *row      = (char*)table + i * 0x10;
        void   *storage  = *(void**)( row + 0x0 );
        char   *name     = *(char**)( row + 0x4 );
        char   *default_ = *(char**)( row + 0x8 );
        int     flags    = *(int*)  ( row + 0xc );
        trap_Cvar_Register( storage, name, default_, flags );   // trap -4
    }
}
```

Confirmed call site passes `table = 0x30`, `count = *(int*)0x1230 = 0x120`
(288 entries), i.e. the table occupies data-segment bytes `[0x30, 0x1230)`,
with the entry count itself stored immediately after the table ends, at
`0x1230`. Stride is exactly `0x10` (4 words) per entry, with **zero gaps or
exceptions** — every single cvar between table rows 0 and 287 obeys this
layout.

**Row field order is `[storage, name, default, flags]` — storage first.**
This matches the real `trap_Cvar_Register(vmCvar_t*, name, defaultValue,
flags)` signature exactly (the loop's own `Arg[0x8]=+0 .. Arg[0x14]=+0xc`
reads prove the order; it is not a visual/guessed pattern-match).

**Pitfall to avoid**: scanning the raw data dump by eye, a "name" string
address immediately *followed* by a plausible-looking default value and a
small `0`/`1` flag looks exactly like `[name, default, flags]` with the
*storage* value appearing to "belong" to the row *before* it. This is a false
pattern — the storage value you see immediately before a name string is
actually that *same* row's own first field, not the previous row's last
field. Getting this backwards shifts every single row attribution by one
slot and silently produces a wrong-but-plausible-looking cvar storage
address (this happened once during this session's investigation of
`df_unit` — see [df_unit.md](df_unit.md) for the corrected result). **Always
verify a specific cvar's storage by cross-checking against the loop's own
argument order, or against a cvar whose default value is unambiguous (e.g.
an opacity cvar defaulting to exactly `"0.5"`), not by eye alone.**

To resolve "what cvar lives at storage address X" or "what is cvar NAME's
storage address", walk the table programmatically (base `0x30`, stride
`0x10`, 288 entries) rather than hand-counting through a raw dump.

---

## `.integer`/`.value` field offsets

Each `storage` value is the address of a `vmCvar_t`:

```c
typedef struct {
    int   handle;              // +0x0
    int   modificationCount;   // +0x4
    float value;                // +0x8
    int   integer;              // +0xc
    char  string[MAX_CVAR_VALUE_STRING];  // +0x10 (256 bytes, confirmed via
                                            //  0x110-byte stride between
                                            //  several adjacent static
                                            //  vmCvar_t declarations)
} vmCvar_t;
```

Code almost always reads a cvar's live value via `*(int*)(storage+0xc)`
(`.integer`) rather than `.value`; no code site reading `df_unit` via
`.value` (`storage+0x8`) was found.

---

## A handful of cvars are registered individually, outside this table

5 standalone `trap_Cvar_Register` (`call -4`) sites exist elsewhere in
`cgame.dis` (`0x11b`, `0x14e`, `0x159`, `0x164`, `0x16f`), immediately after
the batch loop returns — e.g. `"sv_running"` is registered this way. If a
cvar you're looking for isn't in the batch table, check these individual
call sites next before assuming it doesn't exist.

> **Full decode:** every entry of this table (name, default, flags) is now listed in [cgame_cvar_table_full.md](cgame_cvar_table_full.md).
