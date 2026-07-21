I'm reverse-engineering the DeFRaG Quake 3 mod's QVM binary to make my oDFs 
re-implementation match DeFRaG's physics and behavior exactly.

## Repositories
- DeFRaG disassembly: D:\Other\Reverse Engineering\q3vm\qagame.dis
- oDFs source: d:\Other\Programming\C\oDFs
- Disassembly notes: d:\Other\Programming\C\oDFs\disasm\ (README.md + per-module subdirs)

## Before you do anything
Read disasm/README.md — it contains struct field offsets, global addresses, 
QVM instruction reference, and the list of analyzed modules with their status.
Read the relevant module README (e.g. disasm/G_ACTIVE/README.md) before working 
on functions in that module.

## Workflow
1. Read the disassembly for a function, decode QVM bytecode into C pseudocode
2. Compare against oDFs source to find differences
3. Document findings in disasm/<MODULE>/<FunctionName>.md + update README files
4. Apply code changes to oDFs only when a difference affects physics/behavior

## CRITICAL RULE — NO SPECULATION
**Never assume, guess, speculate, or imagine anything you are not certain of.**
If a struct field offset is unknown, mark it as unknown. If a function's purpose 
is unclear, say so. If a value could be one of several things, list the 
possibilities and ask — do not pick one and proceed as if it's confirmed.
This applies especially to: struct layouts, field names, function identities, 
and the meaning of magic constants.

The primary goal is: **disassembly → documentation → code modification.**
Do not get stuck trying to fully reverse an entire struct if some fields are 
unknown — document what is confirmed, mark unknowns explicitly, move on.
Always ask before making assumptions.