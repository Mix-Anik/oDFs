# QVM bytecode -> pseudo-C lister (stack simulation). usage: python qdec.py <module.dis> <hexaddr>. Constants print in DECIMAL; frame args are l(frame+8+4k) (frame = operand of "enter").
import sys, re, struct
# usage: qdec.py <dis> <hexaddr> [maxfuncs]
dis, addr = sys.argv[1], int(sys.argv[2], 16)
T = open(dis, encoding='latin-1').read().split('\n')
start = None
for i, l in enumerate(T):
    if l.startswith('%08x  enter' % addr):
        start = i
        break
ins = []
for x in T[start:start + 20000]:
    m = re.match(r'^([0-9a-f]{8})  (\w+)[ ]*(?:(-?\d+)[ ]+)?(-?0x[0-9a-f]+|-?\d+)?', x)
    if not m:
        continue
    a, op, sz, val = m.groups()
    if op == 'enter' and ins:
        break
    ins.append((int(a, 16), op, val))
BIN = {'add': '+', 'sub': '-', 'muli': '*', 'divi': '/', 'modi': '%', 'mulu': '*', 'divu': '/', 'modu': '%',
       'band': '&', 'bor': '|', 'bxor': '^', 'lsh': '<<', 'rshi': '>>', 'rshu': '>>',
       'addf': '+f', 'subf': '-f', 'mulf': '*f', 'divf': '/f'}
CMP = {'eq': '==', 'ne': '!=', 'lti': '<', 'lei': '<=', 'gti': '>', 'gei': '>=', 'ltu': '<u', 'leu': '<=u', 'gtu': '>u', 'geu': '>=u',
       'eqf': '==f', 'nef': '!=f', 'ltf': '<f', 'lef': '<=f', 'gtf': '>f', 'gef': '>=f'}
def num(v):
    if v is None: return '?'
    n = int(v, 16) if v.startswith(('0x', '-0x')) else int(v)
    return n
def fl(n):
    try:
        return repr(struct.unpack('<f', struct.pack('<I', n & 0xffffffff))[0])
    except Exception:
        return str(n)
st = []
out = []
targets = set()
for a, op, v in ins:
    if op in CMP or op == 'jump':
        n = num(v)
for a, op, v in ins:
    if op in CMP:
        targets.add(num(v))
# jump targets from const+jump
for k in range(len(ins) - 1):
    if ins[k + 1][1] == 'jump' and ins[k][1] == 'const':
        targets.add(num(ins[k][2]))
tmp = 0
args = []
for a, op, v in ins:
    if a in targets:
        out.append('L%x:' % a)
    if op == 'enter':
        out.append('enter %s' % v); continue
    if op == 'const':
        n = num(v)
        st.append(('%d' % n) if abs(n) < 0x100000 or True else hex(n)); st[-1] = (n, st[-1]); continue
    if op == 'local':
        n = num(v); st.append((None, 'L%x' % n if n < 0x40 or True else hex(n)))
        st[-1] = (None, '&l%x' % n); continue
    if op == 'load4':
        e = st.pop()[1]
        if e.startswith('&l'): e = 'l' + e[2:]
        else: e = '*(%s)' % e
        st.append((None, e)); continue
    if op == 'load1':
        e = st.pop()[1]; st.append((None, '*(byte*)(%s)' % e)); continue
    if op == 'load2':
        e = st.pop()[1]; st.append((None, '*(short*)(%s)' % e)); continue
    if op in ('sex8', 'sex16'):
        continue
    if op in BIN:
        b = st.pop()[1]; a_ = st.pop()[1]; st.append((None, '(%s %s %s)' % (a_, BIN[op], b))); continue
    if op in ('negi', 'negf'):
        e = st.pop()[1]; st.append((None, '(-%s)' % e)); continue
    if op == 'bcom':
        e = st.pop()[1]; st.append((None, '(~%s)' % e)); continue
    if op == 'cvif':
        e = st.pop()[1]; st.append((None, 'f(%s)' % e)); continue
    if op == 'cvfi':
        e = st.pop()[1]; st.append((None, 'i(%s)' % e)); continue
    if op == 'store4':
        val = st.pop()[1]; ad = st.pop()[1]
        if ad.startswith('&l'): ad = 'l' + ad[2:]
        else: ad = '*(%s)' % ad
        out.append('%04x  %s = %s' % (a, ad, val)); continue
    if op == 'store1':
        val = st.pop()[1]; ad = st.pop()[1]
        out.append('%04x  *(byte*)(%s) = %s' % (a, ad, val)); continue
    if op == 'arg':
        e = st.pop()[1]; args.append((num(v), e)); continue
    if op == 'call':
        tgt = st.pop()
        al = ', '.join('%s' % e for _, e in sorted(args))
        args = []
        t = tgt[0]
        name = ('fn_%x' % t) if t is not None and t >= 0 else ('trap%d' % (-t - 1) if t is not None else tgt[1])
        tmp += 1
        out.append('%04x  t%d = %s(%s)' % (a, tmp, name, al))
        st.append((None, 't%d' % tmp)); continue
    if op in CMP:
        b = st.pop()[1]; a_ = st.pop()[1]
        out.append('%04x  if (%s %s %s) goto L%x' % (a, a_, CMP[op], b, num(v))); continue
    if op == 'jump':
        tg = st.pop()
        out.append('%04x  goto %s' % (a, ('L%x' % tg[0]) if tg[0] is not None else tg[1])); continue
    if op == 'pop':
        e = st.pop()[1]; continue
    if op == 'push':
        st.append((None, '0')); continue
    if op == 'leave':
        e = st.pop()[1] if st else ''
        out.append('%04x  return %s' % (a, e)); continue
    if op == 'block_copy':
        s = st.pop()[1]; d = st.pop()[1]
        out.append('%04x  memcpy(%s, %s, %s)' % (a, d, s, v)); continue
    out.append('%04x  ?? %s %s' % (a, op, v))
# post-process: floats in consts are shown as ints; annotate
txt = '\n'.join(out)
def repl(m):
    n = int(m.group(0))
    if abs(n) > 0x1000000:
        return m.group(0) + '[' + fl(n)[:10] + ']'
    return m.group(0)
txt = re.sub(r'(?<![\w.])-?\d{8,}(?![\w.])', repl, txt)
print(txt)
