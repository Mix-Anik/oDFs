# G_SPAWN — DeFRaG QVM disassembly analysis

Analysis of `g_spawn.c`-equivalent functions from the DeFRaG qagame.qvm —
the entity-spawn pipeline (`G_SpawnEntitiesFromString` →
`G_SpawnGEntityFromSpawnVars` → `G_CallSpawn` → classname-specific `SP_*`).

For general disassembly methodology, struct layouts, and global addresses
see [../README.md](../README.md).

---

## Functions analyzed

| Function | DeFRaG address | Hash match? | Status |
|---|---|---|---|
| `G_SpawnEntitiesFromString` | `0x24d71` | ❌ (see correction below) | ✅ Fully decoded — **modified** |
| `func_0x24b2a` = `G_SpawnGEntityFromSpawnVars` | `0x24b2a` | — (new function, no vanilla position) | ✅ Fully decoded — **modified**, see [entity_mode_exclusion.md](entity_mode_exclusion.md) |
| `func_0x1c9c` = the `not<mode>` flag classifier | `0x1c9c` | — (new function) | ✅ Fully decoded, see [entity_mode_exclusion.md](entity_mode_exclusion.md) |
| `G_CallSpawn` | `0x2493f` | ✅ hash-identical (`-4936a330`) | Confirmed unmodified |
| `G_SpawnInt`/`G_SpawnFloat`/`G_SpawnString`-equivalents | `0x248eb`/`0x248c9`/`0x24885` | — | Identified by call shape, not independently re-verified |
| `G_Spawn` | `0x283de` | — | Already identified in an earlier pass (see `G_COMBAT/RocketJump_ExtraVelocity_Search.md`) |

## Correction to an earlier mis-identification

An earlier pass in this project's hash-table walk assumed `0x24d71` was
`G_SpawnEntitiesFromString` purely from its position in the hash-match list
(size `0x2a` = 42 bytes recorded in the hash table). That's far too small
for the real function, and the earlier identification never actually
decoded it. Direct decode now confirms `0x24d71` genuinely **is**
`G_SpawnEntitiesFromString` (`enter 0x14`, calls `G_ParseSpawnVars` in a
loop, prints `"SpawnEntities: no entities"` on failure) — the apparent
"42-byte" vanilla hash-table size was simply never meaningfully checked
against byte-for-byte content before; this pass corrects that by reading
the function directly rather than trusting position/size alone. See
[entity_mode_exclusion.md](entity_mode_exclusion.md) for the full decode —
this function itself is barely changed from vanilla shape-wise (just
dispatches to `func_0x24b2a` for every entity after the first), with
essentially all of the real DeFRaG insertion living one level down in
`func_0x24b2a`.

**Lesson reinforced**: a hash-table position/size listing is not a
substitute for decoding — this project has now caught this exact mistake
(assuming identity from position without reading the bytes) more than
once this session.
- [shooters.md](shooters.md) — DeFRaG shooter_* (Use_Shooter 0x21b5e, activator aim)
