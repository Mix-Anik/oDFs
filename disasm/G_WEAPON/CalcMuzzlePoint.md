# CalcMuzzlePoint / CalcMuzzlePointOrigin — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**CalcMuzzlePoint address:** `0x00029383` (vanilla baseq3 equivalent at `0x208ce`, hash `-bc1c503`) — **hash matches vanilla exactly**
**CalcMuzzlePointOrigin address:** `0x0002940d` (vanilla baseq3 equivalent at `0x20958`, hash `2011efa0`) — **hash matches vanilla exactly**

Both functions are **byte-identical to vanilla** (confirmed via hash
cross-reference against `baseq3-game-functions.hmap`, and independently
spot-read below).

---

## Finding

The muzzle-offset formula is **unchanged from vanilla**:

```
point = ent->s.pos.trBase                      ; block_copy from ent+0x18, 0xc bytes
point[2] += (float)ent->client->ps.viewheight   ; confirmed: client_ptr + 0xa4 = ps.viewheight (matches README)
point[0] += 14.0 * forward[0]                   ; confirmed literal 0x41600000 = 14.0
point[1] += 14.0 * forward[1]
point[2] += 14.0 * forward[2]
; each component then rounded via Q_ftol (SnapVector-equivalent)
```

This exactly matches vanilla `CalcMuzzlePoint`:
```c
VectorCopy( ent->s.pos.trBase, point );
point[2] += ent->client->ps.viewheight;
VectorMA( point, 14, forward, point );
SnapVector( point );
```

- The **`14`** forward-offset constant is confirmed present (`0x41600000`).
- **`viewheight`** (a per-frame, per-player value from `ps.viewheight`, not a
  fixed constant) is confirmed used — not a hardcoded height.

`CalcMuzzlePointOrigin` (a variant used when the caller already has an origin
rather than an entity pointer — inferred from the naming convention and its
address immediately following `CalcMuzzlePoint`) was not independently
line-read; its hash also matches vanilla exactly, so it is presumed
byte-identical.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x29383` | `CalcMuzzlePoint` entry |
| `0x293aa` | `point[0] += 14.0 * forward[0]` (`0x41600000`) |
| `0x293c0` | `point[1] += 14.0 * forward[1]` |
| `0x293d9` | `point[2] += 14.0 * forward[2]` |
| `0x2939b` | `point[2] += (float)ps.viewheight` (`client_ptr + 0xa4`) |
| `0x2940d` | `CalcMuzzlePointOrigin` entry (hash-confirmed unmodified, not independently decoded) |

---

## oDFs implementation notes

No action needed — oDFs's muzzle-point calculation already matches this
formula exactly (14-unit forward offset, dynamic `viewheight`, not a fixed
height). This is the one of the four investigated areas where DeFRaG made
**no changes at all** relative to vanilla.
