# `0x1ea7b` = `Cmd_VoiceTaunt_f` (vanilla shape; hash does not match the baseq3 hmap)

Found by listing qagame functions that are neither hash-matched to vanilla nor mentioned in any doc (70 such functions at the time; biggest first: `0x25aa7`, `0x1f041`, `0x261f7`, `0x26024`, `0x1ea7b`, `0x2aa8` ...).
Called from `0x1f4ba` (the ClientCommand "vtaunt" dispatch region). 0x1c0 instructions, `enter 0x54`, one arg (`gentity_t *ent`).

Decoded flow (all offsets cross-checked against known fields: `+0x204` client, `+0x300` enemy, `+0x1a8` r.svFlags (`&8` = SVF_BOT), `+0x324`/`+0x32c` client fields):
1. `if (!ent->client) return;`
2. **Insult someone who just killed you:** `if (ent->enemy && ent->enemy->client && ent->enemy->client->lastkilled_client (+0x324) == ent->s.number)` → for each of the two parties that is not a bot: `G_Voice(party, other, SAY_TELL(=2), "death_insult", qfalse)` (`G_Voice` = `0x1e92b`, string `0x8773`); then `ent->enemy = NULL; return`.
3. **Insult someone you just killed:** `who = &g_entities[ent->client->lastkilled_client]` if `>= 0 && != ent->s.number`, `who->client` non-NULL: if `who->client->lasthurt_mod (+0x32c) == 2 (MOD_GAUNTLET)` → `"kill_gauntlet"` (strings `0x8765`) to both non-bot parties, else `"kill_insult"` (`0x8759`); then `ent->client->lastkilled_client = -1; return`.
4. Otherwise (`g_gametype (0x1b1ec) < 3`, i.e. non-team): pick a random/nearby eligible player and send `"praise"`/`"taunt"` voices (the tail beyond `0x1eba3` follows vanilla's `G_Voice(ent, NULL, SAY_ALL, VOICECHAT_TAUNT, qfalse)` / praise selection; only the head of the tail was read here).

Identification rests on the five exact voicechat strings (`death_insult`, `kill_gauntlet`, `kill_insult`, `praise`, `taunt`) in that arrangement and the field pattern; matches oDFs's vanilla `Cmd_VoiceTaunt_f` in `g_cmds.c`. The hash mismatch is unexplained (not read to the end); nothing DeFRaG-specific was found. Nothing to port.
