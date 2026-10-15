# Constant-level compare of hash-equal functions: DeFRaG <module>.dis vs a reference .dis (e.g. oDFs or a vanilla ioq3 build disassembled with 'qvmdis -q').
# usage: python -I constcmp.py <defrag.dis> <reference.dis> <baseq3 .hmap> [maxprint]  (ignores struct-offset/address consts; see PORT_STATUS iter 160)
import re, sys
# usage: constcmp.py <defrag.dis> <odfs.dis> <hmap> [maxprint]
def load(path):
    funcs = []   # (addr,size,hash)
    code = {}    # addr -> (op, operand)
    on = False
    for l in open(path, encoding='latin-1'):
        if l.startswith(';; function hashes'):
            on = True
            continue
        if on:
            m = re.match(r'^0x([0-9a-f]{8})\s+0x([0-9a-f]+)\s+(\S+)', l)
            if m:
                funcs.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
                continue
            if not l.strip():
                on = False
            continue
        m = re.match(r'^([0-9a-f]{8})  (\S+)\s+(-?\d+)\s*(-?0x[0-9a-f]+)?', l)
        if m:
            a = int(m.group(1), 16)
            v = m.group(4)
            code[a] = (m.group(2), int(v, 16) if v else None)
    return funcs, code

def consts(funcs_entry, code):
    a, s, h = funcs_entry
    out = []
    addrs = sorted(k for k in code if a <= k < a + s)
    for i, k in enumerate(addrs):
        op, v = code[k]
        if op == 'const' and v is not None:
            nxt = code.get(addrs[i + 1], ('', None))[0] if i + 1 < len(addrs) else ''
            out.append((k, v, nxt))
    return out

fd, cd = load(sys.argv[1])
fo, co = load(sys.argv[2])
names = {}
for l in open(sys.argv[3], encoding='latin-1'):
    p = l.split()
    if len(p) >= 3:
        names.setdefault(p[2], p[1])
odfs_by_hash = {}
for f in fo:
    odfs_by_hash.setdefault(f[2], []).append(f)
maxp = int(sys.argv[4]) if len(sys.argv) > 4 else 60
rows = []
for f in fd:
    cand = odfs_by_hash.get(f[2])
    if not cand or len(cand) != 1:
        continue
    g = cand[0]
    cf = consts(f, cd)
    cg = consts(g, co)
    if len(cf) != len(cg):
        continue
    diffs = []
    for (ka, va, na), (kb, vb, nb) in zip(cf, cg):
        if va == vb:
            continue
        # skip code/data addresses: next op is call/jump or address-like loads
        if na in ('call', 'jump','add','local','arg','store4','sub','block_copy','store1','store2','load4') or abs(va)>200000 or abs(vb)>200000:
            continue
        if na.startswith('load') or na.startswith('store'):
            pass
        diffs.append((ka, va, vb, na))
    if diffs:
        rows.append((f, names.get(f[2], '?'), diffs))
print(len(rows), 'functions with const diffs')
for f, n, d in rows[:maxp]:
    print('0x%x' % f[0], n, 'size', f[1], len(d), 'diffs:', ' '.join('%x:%d>%d(%s)' % (k, a, b, o) for k, a, b, o in d[:8]))
