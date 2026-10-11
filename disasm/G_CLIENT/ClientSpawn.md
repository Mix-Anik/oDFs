# ClientSpawn — DeFRaG address `0x1d27b`

**Address:** `0x1d27b`
**Status:** Fully decoded — modified. **Full instruction-level decode
lives in [`../G_ACTIVE/ClientThink_real.md`](../G_ACTIVE/ClientThink_real.md)**
(filed there under its original, later-corrected name — see that doc's
top-of-file identity note, and [`../G_ACTIVE/ClientThink.md`](../G_ACTIVE/ClientThink.md)
for the resolution establishing this address is `ClientSpawn`, not
`ClientThink_real`). This file exists so `g_client.c`'s module index has
its own entry for this address, without duplicating ~400 lines of existing
decode.

---

## Summary (see the linked doc for full detail)

`ClientSpawn` initializes a (re)spawning client's entity/client state:
saves/restores `pers`/`sess` across a `gclient_t` wipe, reads the
`"handicap"` userinfo key, sets up entity flags/contents/bounds, calls
`func_0x274b` (spawn-time weapon/health/armor/ammo loadout — see
[`../G_TRIGGER/func_0x274b.md`](../G_TRIGGER/func_0x274b.md), now fully
resolved and implemented), links the entity, and — DeFRaG-specifically —
immediately runs one full `ClientThink` cycle (fetch a usercmd, run the
real `ClientThink_real`/"Pmove outer") rather than waiting for the next
network command, plus a race-mode freeze branch and an "OBS" state-scan
loop whose exact semantics are still not fully pinned down.

## Already-implemented pieces

- `func_0x274b`'s full loadout (health/armor/weapons/ammo) — see that doc.
- `0x1317bc`'s race-mode handling has no oDFs equivalent and is not ported
  (oDFs has no "DeFRaG game mode" system) — see
  `../G_ACTIVE/ClientThink_real.md`'s §9 for the decode.

## Still open (see the linked doc's "Status" section for the full list)

- `func_0x2830` (client state manager, called at entry)
- The restructured spawn-point-selection dispatch
- The `0x1317bc==1` "keep weapon across respawn" gate (identified
  structurally — `0x1317bc` is the same DeFRaG game-mode selector
  `func_0x1e60`/`G_InitRaceMode` already established, mode `1` = full
  DeFRaG race mode — but deliberately not ported since oDFs has no
  equivalent mode toggle to gate it on, and porting it unconditionally
  would be a design decision beyond what's been reverse-engineered)
- `0x19ecc` cvar multiplier identity
