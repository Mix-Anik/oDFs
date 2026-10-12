# DeFRaG `target_speed` (spawn `0x25f14`, use `0x25aa7`)

Spawn table entry `"target_speed"` (data `0x9595`) -> `0x25f14`: `G_SpawnFloat("speed","100",&ent->speed /*+0x2a0*/)`, `ent->use (+0x2c4) = 0x25aa7`. Found by the undocumented-function survey. **Not in oDFs before this port.**
`0x6076` = `fabs` (float), `0x8d63` = `VectorLength`, `-0x6b` = `sqrt`.

## Use (`0x25aa7`, ~1130 instructions) — decoded
Flags (`ent->spawnflags`, `+0x210`): `4`/`8` = +X/-X, `0x10`/`0x20` = +Y/-Y, `0x40`/`0x80` = +Z/-Z (axis "selected" if either of its two bits is set), `2` add-mode, `1` percent-mode, `0x100` special mode. (The axis/direction roles follow from how the bits are combined; the human-facing names are inferred, not from an entity def.)
Activator must have a client (else return).
1. `vel = client->ps.velocity` (`client+0x20`); components of unselected axes are zeroed; `s[i] = vel[i]/|vel[i]|` (0 if `|vel[i]| < 1e-5`, constant `0x3727c5ac`).
2. Per axis `(A[i], B[i])` (defaults 1,1 from data `0x2848`/`0x2854`): both direction bits → `(1, s)`; +only → `(s, 1)`; -only → `(-s, -1)`.
3. `len = |vel|`; `V[0..2] = len`.
4. Per axis: if `spawnflags&2` (add): `if (&0x100) A=1;` percent (`&1`): `V += 0.01*A*V*speed`, else `V += A*speed`. Else (set): `speed = max(speed,0)` (written back to the entity); percent: `V *= 0.01*speed`, else `V = speed`. Then `V *= B`; and `vel[i] = |vel[i]|` (if `> 1e-5`).
5. `if (|len| > 1e-5) vel[i] /= |len|` -> unit-fraction of each axis.
6. Output: if `spawnflags&0x100`: `n` = number of axes with exactly one direction bit; if `n == 0` return; `M = sqrt(max(|V0|,|V1|,|V2|)^2 * n)`; for each axis with exactly one direction bit `client->ps.velocity[i] = (+bit ? +M : -M)`; other axes untouched.
   Otherwise: for each selected axis `client->ps.velocity[i] = vel[i] * V[i]` (so "set" keeps the direction and scales the magnitude; signed by `B`).

## oDFs — PORTED
`g_target.c`: `Use_Target_Speed`, `SP_target_speed`; registered in `g_spawn.c`. Transcribed literally from the bytecode; syntax-checked, untested.
**Behaviour change:** none for existing maps (new entity class). Revert = remove the spawn table entry.
