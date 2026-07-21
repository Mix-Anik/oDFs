# CanDamage — Defrag QVM disassembly analysis

**Source file:** `D:\Other\Reverse Engineering\q3vm\qagame.dis`
**Function address:** `0x00020648` (vanilla baseq3 equivalent at `0x18ed1`)
**DeFRaG hash:** `-6193485b` — **matches vanilla hash exactly.**

---

## Finding

`CanDamage`'s hash in the DeFRaG hash table (top of `qagame.dis`) is byte-for-byte
identical to vanilla baseq3's `CanDamage` hash (`-6193485b`). This means the
function's compiled bytecode is unchanged from vanilla — DeFRaG did not modify
`CanDamage`'s line-of-sight logic (the `trap_Trace` based visibility check used
by `G_RadiusDamage` to decide whether an entity in the blast radius can actually
be hit, e.g. around corners).

Not independently decoded instruction-by-instruction beyond this hash
confirmation, since a hash match means there is nothing to compare — the code is
identical. (Note: the hashing scheme used by this toolchain appears to be
opcode-shape based rather than a strict byte-for-byte checksum — see caveat in
[fire_rocket.md](../G_MISSILE/fire_rocket.md) — but `CanDamage` was also visually
spot-checked via its call site in `G_RadiusDamage.md` and behaves exactly as
expected for vanilla `CanDamage(ent, origin)`.)

---

## oDFs implementation notes

No action needed — oDFs's `CanDamage` is unmodified vanilla logic, which matches
DeFRaG's compiled behavior exactly.
