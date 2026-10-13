# `func_0x3ccd` — frame-time / clock-drift sanity check (writer of `0x1320f4` / `0x1320f8`)

Called once per frame from `func_0x23f2` (at `0x2570`, right after DeFRaG `CheckVote` `0x33dc`). No args. Result is two **sticky** (never cleared) globals, published to clients by `func_0x3d37` as `ps.stats[12]` bit `0x40` (`0x1320f4`) and bit `0x20` (`0x1320f8`) (see `G_TRIGGER/target_fragsFilter.md`).

Globals used: `0x12a310` = level.time, `0x12a314` = previous-frame level.time (so `d` = frame length in ms), `0x1320fc` = level.time at init (`0x406d`), `0x1320dc` / `0x1320e0` = accumulated game-ms / real-ms, `0x1320e4` / `0x1320e8` = previous / current `trap_Milliseconds()` (syscall `-3`, G_MILLISECONDS=2).

```c
void CheckClockSanity(void) {
    if (level.time < initTime + 2500) return;        // 0x9c4: 2.5 s grace
    d = level.time - level.previousTime;             // frame length
    if (d < 7 || d > 16) flag_0x1320f4 = 1;          // sticky: frame not in 7..16 ms
    now = trap_Milliseconds();  0x1320e8 = now;
    if (0x1320e4 != 0) {                             // have a previous sample
        gameAcc += d;                                // 0x1320dc
        realAcc += now - prevReal;                   // 0x1320e0
    }
    prevReal = now;
    if (realAcc >= 10000) {                          // every 10 s of wall clock
        r = (float)gameAcc / (float)realAcc;
        if (r <= 0.8f || r >= 1.2f) flag_0x1320f8 = 1;   // sticky: game clock drifts >20% from real
        gameAcc = realAcc = 0;
    }
}
```
Literals: `0x3f4ccccd`=0.8f, `0x3f99999a`=1.2f. The `d` window 7..16 corresponds to sv_fps ~63..142 (matches the 60..140 sv_fps clamp in `func_0x23f2`).

## Interpretation
- `0x1320f8` = "game speed ≠ real time" (timescale / host stall / speed manipulation) — CONFIRMED by the arithmetic above.
- `0x1320f4` = "a frame outside 7..16 ms happened" (sv_fps out of range or hitch) — CONFIRMED arithmetic.
- That these are *anti-cheat / run-validity* markers is an inference from where they go (ps.stats[12] next to the race-timer flags); cgame consumer CONFIRMED in `CGAME/cheat_report.md`: bit 0x20 -> "timescaling reported", bit 0x40 -> "illegal server frame-rate reported" (client-side run-validity warnings).

## oDFs
Not ported: no consumer in the DeFRaG cgame confirmed, and oDFs already doesn't publish bits `0x20/0x40`. If wanted: add the function to the per-frame rules and set `ps.stats[12]` bits in the race ps publish.
