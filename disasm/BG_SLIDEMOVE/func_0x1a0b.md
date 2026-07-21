# func_0x1a0b — Promode ClipVelocity copy

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`  
**Function address:** `0x00001a0b`  
**Called from:** `0x8af9` (PM_StepSlideMove, promode push-down clip path)

---

## Purpose

An internal copy of `PM_ClipVelocity` used exclusively by the promode path in `PM_StepSlideMove`
during the push-down clip. The function arguments and computation are identical to `PM_ClipVelocity (0x6667)`.

---

## Signature

```c
void func_0x1a0b(vec3_t in, vec3_t normal, vec3_t out, float overbounce);
```

Same signature as standard `PM_ClipVelocity`.

---

## Logic

```c
// dot product of in[0..2] and normal[0..2]
float backoff = in[0]*normal[0] + in[1]*normal[1] + in[2]*normal[2];

if (backoff >= 0.0f)
    backoff /= overbounce;    // going away from surface
else
    backoff *= overbounce;    // going into surface

for (int i = 0; i < 3; i++)
    out[i] = in[i] - normal[i] * backoff;
```

This is **identical to stock Q3's `PM_ClipVelocity`**.

---

## Why a separate copy?

Unclear from disassembly alone. Possible reasons:
- The Defrag developers inlined or duplicated ClipVelocity for a specific optimization
- The QVM compiler/linker created a separate copy due to call-graph or linking constraints
- It was originally a modified version that was later reverted

Functionally, `0x1a0b` and `0x6667` produce the same result. The only observable
difference is that the promode path in PM_StepSlideMove that calls `0x1a0b` returns
immediately after (no step sounds), while the non-promode path that calls `0x6667`
continues to the step-sound and debug-print block.

---

## Disassembly reference

```
00001a0b  enter 0x28
; load arg pointers
; compute backoff = dot(in, normal)
; if backoff >= 0: backoff /= overbounce
; else: backoff *= overbounce
; for i=0..2: out[i] = in[i] - normal[i] * backoff
00001a82  leave 0x28
```

See [[PM_StepSlideMove]] for the calling context.
