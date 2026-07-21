# PM_ClipVelocity — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00006667`  
**Called from:** PM_GroundTrace, PM_StepSlideMove, PM_SlideMove, PM_WaterMove, and others

---

## Structure

4 args: `in` (vec3_t), `normal` (vec3_t), `out` (vec3_t), `overbounce` (float)

```
backoff = DotProduct(in, normal)
if backoff < 0:  backoff *= overbounce
else:            backoff /= overbounce
for i in 0..2:
    out[i] = in[i] - normal[i] * backoff
```

Standard Q3 PM_ClipVelocity. No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x6667` | Function entry |
| `0x6676–0x669a` | DotProduct computation → local10 |
| `0x669e` | `gef` — if dot ≥ 0, jump to divide path |
| `0x669f–0x66a5` | dot < 0: `dot *= overbounce` |
| `0x66a8–0x66ae` | dot ≥ 0: `dot /= overbounce` (divf) |
| `0x66af–0x66d9` | Loop i=0..2: `out[i] = in[i] - normal[i]*backoff` |
| `0x66d0` | Function return |

---

## oDFs implementation notes

Full match. ✓
