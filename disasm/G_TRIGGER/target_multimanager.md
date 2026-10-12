# DeFRaG `target_multimanager` (`SP` = `0x261f7`, use = `0x26024`, delayed fire = `0x25f29`, random pick = `0x25f76`)

Found via the undocumented-function survey (neither hash-matched nor documented). Identification: classname string `target_multimanager` in the spawn table (data `0x9581`), warning strings
`Warning: target_multimanager "%s" with count 0. No targets will be triggered!`, `Empty target-string for target%02d of multimanager "%s".`, `Too much memory allocated for target_multimanager "%s"`, `Invalid target was specified in multimanager`.
Not in vanilla Q3 and **not in oDFs before this port** (maps using it simply did nothing).

## Mapper interface (decoded)
- Keys `target01` … `target40` (`"target"` + two digits, loop `i < 0x28`); each value is `"[delay:]targetname"` — if it contains no `:` the default delay token `-1` is prepended (`-1` = "use `wait`").
- `count` (default `-1`): how many of the listed targets (chosen at random, distinct) to fire per use; `count == 0` → warning and the entity frees itself after 100 ms. `wait` (default 1) and `random` (default 0) as in `target_delay`.
- spawnflags: `1` only fire for activators on TEAM_RED (client+0x284 == 1; non-client activators pass), `2` only TEAM_BLUE (== 2), `4` add `wait` to explicit delays, `8` stagger: delay += `wait*(index+1)` (for default-delay entries delay = `wait*(index+1)`).
- Total packed text limit `0x400` bytes → "Too much memory…" and self-free.

## Spawn (`0x261f7`)
Builds a buffer of `delay:name:` records, `ent->count = min(count, n) | (n << 8)` (low byte = how many to fire, bits 8+ = number of valid targets `n`), `ent->target` (+0x288) = `G_Alloc`'d (`0x217d0`) copy with every `:` turned into `\0` (copy uses `Q_strncpyz`-style destsize = strlen, which drops the final `:`),
`ent+0x2f8` = packed length, `ent+0x2dc` = running counter (0), `ent->use (+0x2c4) = 0x26024`.

## Use (`0x26024`)
1. Team gates (spawnflags 1/2 above). 2. Return if `ent+0x2dc != 0` (previous use still running). 3. `ent->activator (+0x304) = activator; ent+0x2dc = count & 0xff`; `0x25f76(sel[], count&0xff, count>>8)` marks the chosen records.
4. Walk the records: delay token (`strcmp(tok,"-1")` → default/stagger rules above, else `atof`), then name; skip records not selected. `t = G_PickTarget(name)` (`0x280eb`); if `!t` or `!t->use` → "Invalid target…".
   Delay ≤ 0.001: `+0x2dc--` and `t->use(t, ent, activator)` immediately. Otherwise spawn a helper entity (`G_Spawn`, classname `target_delay`, parent pointer at `+0x258` = the multimanager, `target = name`, `activator`, `think +0x2b4 = 0x25f29`,
   `nextthink = level.time + (int)(1000 * (delay + random*crandom()))` — the `2*(rand&0x7fff)/32767 - 0.5`-style crandom).
5. `0x25f29` (helper think): `G_UseTargets(self, self->activator)` (fires *all* entities with that targetname, unlike the immediate path's `G_PickTarget`), then free itself in 100 ms, `mm->+0x2dc--`, and when it reaches 0 clear `mm->activator`.
6. `0x25f76(arr, k, n)`: `n` clipped to 40, `k` clipped to `n`; `k == n` → all ones; else Fisher–Yates-style: pool of indices, `rand() % remaining` picks, remove from pool, k times.

## oDFs — PORTED
`g_target.c`: `SP_target_multimanager`, `Use_Target_Multimanager`, `MM_DelayedFire`, `MM_SelectRandom`; registered in `g_spawn.c`; `gentity_t` gained `mmRemaining`, `mmTargetLen`, `mmOwner` (stores the helper's parent; DeFRaG uses raw `+0x258`).
Differences: the `0x25f29` guard `self+0x2f8 != -1` (always true for a fresh `G_Spawn` entity) is dropped; non-client activators with spawnflags 1/2 pass as in the bytecode. Syntax-checked, untested.
**Behaviour change:** none for existing maps (new entity class only). Revert = remove the spawn table entry.
