# oDFs physics and gameplay tests

End-to-end check that the oDFs game code reproduces what the original DeFRaG did in demos recorded on it: movement,
weapons (rockets, plasma climbs, grenade jumps, rail, lightning, gauntlet ...), pickups, jump pads, damage and the race
timer, including the finish time. It runs in CI on every commit without the game, an engine or a demo.

```
demo (original DeFRaG)  --dfgolden make-->  .dfg golden file  --dfgolden test-->  game QVM built from this commit
```

## What a test does

A golden file holds, for one demo: the map (stripped to what the game and the collision code read), the server settings,
the player's settings, and for every 8 ms step the usercmd (view angles, keys, weapon) and the player state the
recording shows. `dfgolden test` starts the real `qagame.qvm` on that map and settings. For every step it sets the
player to the recorded state of the step before, feeds the step's usercmd to the game's `ClientThink`, runs the server
frame, and compares the player state the game produced with the recorded one. So everything the game does on its own
is tested (weapons and their knockback, damage, triggers, items, the race timer) while the player cannot drift away from
the recording. The race time is compared decoded, as the client reads it, and the finish time is compared with the time
in the demo's file name.

The comparison is not required to be perfect: a few steps of every demo are float noise or things not understood yet.
When a file is made, the steps that do not match are listed in it (`known`). The test fails when a step **outside that
list** stops matching (`NEW`). Steps that start matching are reported as `fixed`; remake the file to record that.

## Running it

```
testing/ci.sh                  # builds qagame.qvm, the helper QVM and dfgolden, replays testing/golden/*.dfg
```

The CI (`.github/workflows/physics.yml`, Windows) runs exactly this. It needs the tools in `bin/` (the project's Q3VM
compiler), cmake, gcc and python. Output per file:

```
dfwc2019-7[df.cpm]00.34.736(WIZ.Wizland).dm_68  steps=5464 exact=5440 (99.56%) known=24 NEW=0 fixed=0 race=4342/4342 finish 0:34.736/0:34.736
```

`race=a/b`: race times that matched / were checked. `finish want/got`: the demo's finish time and the game's.

## Making golden files

Needs the demo and its map (`<map>.bsp`, from the map's pk3), plus the game QVM and two helper QVMs:

```
testing/pmove_qvm/build.sh   <dir>           # <dir>/pmove.qvm      (input search)
testing/offsets_qvm/build.sh <dir>           # <dir>/offsets.qvm
cmake --build ...                            # dfgolden, see ci.sh
dfgolden make --game qagame.qvm --qvm pmove.qvm --offsets offsets.qvm --maps <dir with <map>.bsp> \
              --out-dir testing/golden [--allow-map-mismatch] <demo> ...
dfgolden show testing/golden/<file>.dfg
```

The demo holds the player states the server sent, not the commands the player typed. `make` recovers them: for every
step it searches the usercmd that makes `Pmove()` reproduce the step (key values, then partial values), takes the keys
`Pmove()` does not use from the states (walk from the footstep animation, chat from `EF_TALK`, use-item from the pmove
flags) and the weapon from the weapon changes. Then it replays the new file once and stores which steps do not match.

`--allow-map-mismatch`: some competition demos were recorded on an earlier version of the map than the pk3 that is
available today (different checksum, same collision). Without the flag such demos are refused.

### Which demos

Only clean ones: `dfgolden check -r --csv <demos>` gives `CLEAN` / `DIRTY` / `REJECT`. A clean demo has no sequence gaps,
a constant 8 ms snapshot step with `pmove_fixed 1`, no rate-delayed snapshots, no configuration change and a command
clock that advances one step per snapshot. Multiplayer demos (`mdf`) are not used. Recordings of servers with other game
modifications (for example the dfwc2014 demos, which carry an extra counter in `ammo[11]`) do not replay.

### The committed set

`golden/` holds 24 files (12 CPM, 12 VQ3, about 7.6 MB), chosen from 109 made files: small maps first, and together they
exercise every weapon the demos use (gauntlet, machinegun, shotgun, grenade launcher, rocket launcher, lightning gun,
rail, plasma gun, BFG), jump pads, server knockback and damage. All 109 reproduce their official finish time and their
race time on every step (99.83% of 483,960 steps bit-exact). The rest are kept locally in `out/` (they are about 100 MB).
Add more with `dfgolden make`; every file is 70 KB to 6 MB, mostly the map.

## Layout

```
testing/
  dfgolden/       the tool: demo reader and gate (check, info), input recovery (steps.c), golden file (golden.c, goldmake.c,
                  goldrun.c, goldmain.c), diagnostics (gamesteps.c, gamerun.c)
  host/           headless host: QVM interpreter (pmvm.c), Pmove host (pmhost.c), game host with the syscalls (gamehost.c)
  pmove_qvm/      helper QVM with bg_pmove.c only (input search)
  offsets_qvm/    helper QVM that reports field offsets of the compiled game (health, decay clock)
  third_party/    engine (vendored collision + message code), engine_shim (sv_world.c), miniz
  golden/         committed golden files
  out/            local scratch (ignored by git)
  ci.sh           what the CI runs
```

## Diagnostics

`dfgolden gamesteps` replays a demo directly (input recovery and replay in one go, with a coverage summary per weapon),
`dfgolden steps` checks `Pmove()` alone against a demo (classifies misses: external, ambiguous, unexplained),
`dfgolden gamerun` boots the game on a map. Environment variables: `DFG_DEBUG_SNAP=N` (details of snapshot N),
`DFG_IGNORE_CRC=1` (accept a map of another version).

## What the tests found so far

Bugs in the port that the replay exposed (all in `disasm/PORT_STATUS.md`): client prediction used VQ3 physics on CPM
servers; a single player could end the level by reaching the fraglimit; `SnapVectorTowards` rounded differently from the
original (wrong explosion position, wrong splash knockback); the lightning-gun knockback / grapple mode / kill-OB flags
were refreshed every frame instead of at spawn and at weapon pickup; powerup timers were rounded to seconds and powerup
pickup events were not predicted; ammo could drop below -1; the gauntlet never fired.

## Not done yet

Fuzzing against the original QVM, snapshot goldens (pinning what the port does where it differs from the recording),
Linux CI (the Q3VM tools in `bin/` are Windows executables).
