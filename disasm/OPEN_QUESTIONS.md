# Open questions (audit loop, Phase C)

Each item: the question, the evidence for each reading, and the test that settles it in the ORIGINAL DeFRaG. Added by the audit loop; nothing here is guessed into code.

## Q1. Does the original stop an autorecord demo when the map changes?
- Evidence: cgame `CG_Shutdown` (`0xf36` -> `0x3a44` -> `0x19d4`) sends `stoprecord` when the autorecord state (`0x1189b0`/1149360) is non-zero. Ported in audit iter 1 (`CG_DF_AutorecShutdown`). The decode is solid; the open part is only whether the engine ever tears the cgame down on a `map` change while recording (ioq3 keeps one demo across level changes on a local server).
- Test (original DeFRaG): `df_autorecord 1`, start a run (leave the start zone), type `map <other map>`. Expected: console prints the engine's "stopped demo" line right at the map change if the original behaves like the port now; no such line if the demo keeps recording.

## Q2. `com_maxfps` clamp notices (`0x758f` / `0x75c7`)
- Evidence: cgame per-frame driver `0x1bd3` clamps `com_maxfps` to 60..140 and prints `^1com_maxfps lowest value restricted to 60.^7` / `highest value restricted to 140`. Overrides client config, so NOT ported (loop rule).
- Decision needed from the user: port behind a cvar (default off)? Test in the original: `com_maxfps 30` then `com_maxfps` (expected: value 60 + notice) and `com_maxfps 250`.

## Q3. Armor Used in the end-of-run Stats
- Evidence: the original accumulator adds (current - current) = 0 (`0x4714..0x472e`), so the line always shows 0; the port reproduces that. If a DeFRaG build printed non-zero values there, the decode of that build differs.
- Test (original): pick up/lose armor during a run (`Stats` appear at the finish or via `stats`): `Armor Used` should be 0.
