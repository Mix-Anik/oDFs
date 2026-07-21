# PM_CorrectAllSolid — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x000077ae`  
**Called from:** PM_GroundTrace (when trace.allsolid is set)

---

## Structure

1 arg: `trace` (trace_t pointer — passed by PM_GroundTrace)

Tries to push the player out of solid geometry by testing 27 positions (3×3×3 jitter around origin):

```
for i in −1..1:
    for j in −1..1:
        for k in −1..1:
            point = ps->origin + (i, j, k)
            trace(point, mins, maxs, point, clientNum, tracemask)
            if !trace.allsolid:
                trace down 0.25 units from origin
                pml.groundTrace = trace
                return qtrue
ps->groundEntityNum = ENTITYNUM_NONE
pml.groundPlane = qfalse
pml.walking = qfalse
return qfalse
```

Standard Q3 PM_CorrectAllSolid. No CPM modifications.

---

## Addresses of interest

| Address | Description |
|---------|-------------|
| `0x77ae` | Function entry |
| `0x77af–0x77bd` | Debug print "allsolid" if debugLevel |
| `0x77be` | Initialize loop counters (i=j=k=−1 via const −1) |
| `0x77c4–0x77cd` | Copy origin to test point, add i/j/k offsets |
| `0x7808` | Trace call via pm->trace (pm+0xe4) |
| `0x7812` | Check trace.allsolid == 0 → found non-solid position |
| `0x7814–0x782b` | Non-solid: set point.z = origin.z − 0.25, trace again |
| `0x782c–0x7838` | Store to pml.groundTrace, return qtrue |
| `0x787f–0x7894` | Loop increment / bounds check (i,j,k loop) |
| `0x7895` | After loop: set groundEntityNum=ENTITYNUM_NONE |
| `0x7893` | Set pml.groundPlane=0, walking=0, return qfalse |

---

## oDFs implementation notes

Full match. ✓ Same 3-axis jitter loop with 0.25-unit downward trace on success.
