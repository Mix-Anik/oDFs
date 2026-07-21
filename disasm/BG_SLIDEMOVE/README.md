# BG_SLIDEMOVE — Defrag QVM disassembly analysis

Analysis of movement functions from `bg_slidemove.c` in the Defrag qagame.qvm.

For general disassembly methodology, struct layouts, and global addresses see [../README.md](../README.md).

---

## Functions in this file

`bg_slidemove.c` contains exactly two functions in Q3/oDFs:
- `PM_SlideMove` — core iterative plane-clipping movement
- `PM_StepSlideMove` — step-up wrapper around PM_SlideMove

`func_0x1a0b` is an internal ClipVelocity duplicate used exclusively by PM_StepSlideMove's promode path — almost certainly a compiler artifact within bg_slidemove.c, not a separate source file.

---

## Match status

| Function | Address | Status |
|----------|---------|--------|
| PM_SlideMove | `0x8709` | ✅ Full match — no CPM modifications |
| PM_StepSlideMove | `0x8af9` | ✅ Full match — CPM ramp-jump mechanic added |
| func_0x1a0b (internal ClipVelocity) | `0x1a0b` | ✅ Identical to PM_ClipVelocity (0x6667) |

---

## Function index

| File | Address | Summary |
|------|---------|---------|
| [PM_SlideMove.md](PM_SlideMove.md) | `0x8709` | Core slide-move — up to 4 plane clips, optional gravity |
| [PM_StepSlideMove.md](PM_StepSlideMove.md) | `0x8af9` | Step-up wrapper — CPM ramp-jump mechanic |
| [func_0x1a0b.md](func_0x1a0b.md) | `0x1a0b` | Internal ClipVelocity copy used by PM_StepSlideMove promode path |

---

## Key findings

- **PM_SlideMove** has zero CPM modifications — no `pm_flags & 0x8000` checks in its entire range (`0x8709–0x8af8`). Pure Q3.
- **PM_StepSlideMove** adds a CPM ramp-jump block (`0x8b79–0x8cf1`) gated on promode AND `velocity[2] <= 370` AND `stats[10] > 0` (doublejump window open). This allows mid-air step-up onto ramp faces, which Q3 would refuse.
- **func_0x1a0b** is a duplicate of PM_ClipVelocity called by the promode push-down clip path in PM_StepSlideMove. The non-promode path calls the main PM_ClipVelocity (0x6667) instead. Functionally identical.
- `0x43b90000` = **370.0f** — the velocity[2] cap for the ramp-jump gate
- `0x41d00000` = **26.0f** — step height used in PM_StepSlideMove
- The 400ms doublejump window (`stats[10]`) serves double duty: reset timer for the +100 bonus AND gate for ramp step-up mechanics
