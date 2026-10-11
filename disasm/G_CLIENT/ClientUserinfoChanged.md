# ClientUserinfoChanged — DeFRaG address `0x1ccdf`

**Address:** `0x1ccdf`
**Vanilla address:** n/a (not matched in the hmap by this name at time of check)
**Hash match:** — (not evaluated)
**Status:** Fully decoded — modified (two new userinfo keys)

Extracted from [`README.md`](README.md)'s finding 3, which that pass marks
as independently re-confirmed directly against the bytecode (at
`0x1cd50`-`0x1cd92` and `0x1ce0d`-`0x1ce34`) before porting — not
re-verified a third time in this pass, but inheriting that pass's own
re-verification rather than the original, less-careful one.

---

## Two new DeFRaG-added userinfo keys

### `df_cl_respawnMsec` (`0x1cd50`-`0x1cd92`)

Parsed via `Info_ValueForKey` + `atoi`, clamped to `[50, 1700]` (confirmed
literals `0x32`/`0x6a4`). **Default `250` (`0xfa`) when the key is
empty/unset** — notably not vanilla's hardcoded `1700`, and not oDFs's own
prior hardcoded `500` in `player_die` (`g_combat.c`) either.

```c
int respawnMsec = atoi( Info_ValueForKey( userinfo, "df_cl_respawnMsec" ) );
if ( respawnMsec == 0 ) respawnMsec = 250;      // key empty/unset
else if ( respawnMsec < 50 ) respawnMsec = 50;
else if ( respawnMsec > 1700 ) respawnMsec = 1700;
client->respawnMsec = respawnMsec;              // new gclient_t field
```

### `df_name` (`0x1ce0d`-`0x1ce34`)

Parsed, sanitized through the same string-cleaning helper used for `name`
(`func_0x12e3`), and stored in a confirmed **20-byte** field
(`client+0x268`, copy length `0x14` confirmed) — a separate "demo name"
distinct from the player's regular netname.

**The originating pass's claim of a `\dfn\` configstring broadcast could
not be confirmed**: no such token string was found anywhere in the QVM, and
no code consuming `client+0x268` afterward (e.g. building an outgoing
configstring) was located. Treated as an unconfirmed/wrong claim from the
original pass, not ported.

---

## oDFs implementation status

**Implemented**:
- `df_cl_respawnMsec`: added `gclient_t.respawnMsec`, parsed in
  `ClientUserinfoChanged`, used in `player_die` (`g_combat.c`) in place of
  the previous hardcoded literal `500`.
- `df_name`: storage only — added `gclient_t.demoName[21]`, parsed and
  cleaned in `ClientUserinfoChanged`. **No broadcast** — the unconfirmed
  `\dfn\` claim above was not ported.
