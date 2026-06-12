#!/usr/bin/env python3
"""Validate the decoded Waldmeister CP-emission ORDER against a -a 4 trace,
using the exact trie order-mirror (wm_trie_mirror).  See mirror header for
the source map; phases per INF/Unifikation1.c U1_KPsBildenZuRegel (:1481)
/ U1_KPsBildenZuGleichung (:1556):
  rule:    2) per TT(l) subterm (preorder, nonvar): R tops (Ausschluss =
              the new rule's leaf-chain head), then E tops (MitAllen)
           3) l =? eTT: R leaf list, then E leaf list
  eq:      A) per TT(l): R tops, E tops (MitAllenAusser self twins)
           B) l =? eTT: R leaves, E leaves
           F) l =? l         (stereo only from here:)
           C) l(rev) =? l    D) per TT(r): E tops, R tops
           E) r =? eTT: E leaves, R leaves      G) r =? r(rev twin)
Emission per DFS ARRIVAL (duplicates are real, cp1169/1170 evidence).
"""
import re, sys
sys.path.insert(0, '/tmp')
from wm_trie_mirror import WmTrie, TLeaf, FUN, VAR, sub_end

TRACE = sys.argv[1] if len(sys.argv) > 1 else "/tmp/wm_mcii_v2.txt"
# regenerate the trace: wmcli -a 4 <mccune-ioi .pr> > /tmp/wm_mcii_v2.txt
DEBUG_BATCH = sys.argv[2] if len(sys.argv) > 2 else None

# ---------- terms ----------
def expand_pow(s):
    while True:
        m = re.search(r'([a-zA-Z_][a-zA-Z0-9_]*)\^(\d+)\(', s)
        if not m: return s
        name, n = m.group(1), int(m.group(2))
        s = s[:m.start()] + (name + '(') * n + s[m.end():]
        i = m.start() + len((name + '(') * n)
        d = 1
        while d:
            if s[i] == '(': d += 1
            elif s[i] == ')': d -= 1
            i += 1
        s = s[:i] + ')' * (n - 1) + s[i:]

def parse_term(s):
    s = expand_pow(s.strip())
    pos = [0]
    def p():
        m = re.match(r'([a-zA-Z_][a-zA-Z0-9_]*)', s[pos[0]:])
        name = m.group(1)
        pos[0] += len(name)
        if pos[0] < len(s) and s[pos[0]] == '(':
            pos[0] += 1
            args = [p()]
            while s[pos[0]] == ',':
                pos[0] += 1
                args.append(p())
            assert s[pos[0]] == ')'
            pos[0] += 1
            return (name, tuple(args))
        if re.fullmatch(r'x\d+', name):
            return ('var', int(name[1:]))
        return (name, ())
    t = p()
    assert pos[0] == len(s)
    return t

def is_var(t): return t[0] == 'var'

def term_str(t):
    if is_var(t): return 'x%d' % t[1]
    if not t[1]: return t[0]
    return '%s(%s)' % (t[0], ','.join(term_str(a) for a in t[1]))

def depth(t):
    if is_var(t) or not t[1]: return 1
    return 1 + max(depth(a) for a in t[1])

def renumber_pair(l, r):
    mp = {}
    def walk(t):
        if is_var(t):
            if t[1] not in mp: mp[t[1]] = len(mp) + 1
            return ('var', mp[t[1]])
        return (t[0], tuple(walk(a) for a in t[1]))
    return walk(l), walk(r)

def rename_apart(t, off=1000):
    if is_var(t): return ('var', t[1] + off)
    return (t[0], tuple(rename_apart(a, off) for a in t[1]))

def subterms_preorder(t, path=()):
    out = [(t, path)]
    if not is_var(t):
        for i, a in enumerate(t[1]):
            out.extend(subterms_preorder(a, path + (i,)))
    return out

def replace_at(t, path, repl):
    if not path: return repl
    args = list(t[1])
    args[path[0]] = replace_at(args[path[0]], path[1:], repl)
    return (t[0], tuple(args))

def flat(t):
    if is_var(t): return [(VAR, t[1])]
    out = [(FUN, t[0])]
    for a in t[1]: out.extend(flat(a))
    return out

ARITY = {}
def note_arities(t):
    if not is_var(t):
        ARITY[t[0]] = len(t[1])
        for a in t[1]: note_arities(a)

def cells_to_term(cells, tree_vars=True):
    pos = [0]
    def p():
        c = cells[pos[0]]; pos[0] += 1
        if c[0] == VAR:
            return ('var', -c[1] if tree_vars else c[1])
        return (c[1], tuple(p() for _ in range(ARITY[c[1]])))
    return p()

# ---------- unification ----------
def walk_b(t, b):
    while is_var(t) and t[1] in b: t = b[t[1]]
    return t

def occurs(v, t, b):
    t = walk_b(t, b)
    if is_var(t): return t[1] == v
    return any(occurs(v, a, b) for a in t[1])

def unify(a, c, b):
    a, c = walk_b(a, b), walk_b(c, b)
    if is_var(a) and is_var(c) and a[1] == c[1]: return b
    if is_var(a):
        if occurs(a[1], c, b): return None
        b2 = dict(b); b2[a[1]] = c; return b2
    if is_var(c):
        if occurs(c[1], a, b): return None
        b2 = dict(b); b2[c[1]] = a; return b2
    if a[0] != c[0] or len(a[1]) != len(c[1]): return None
    for x, y in zip(a[1], c[1]):
        b = unify(x, y, b)
        if b is None: return None
    return b

def subst(t, b):
    t = walk_b(t, b)
    if is_var(t): return t
    return (t[0], tuple(subst(a, b) for a in t[1]))

# ---------- tops DFS over the mirror ----------
def tops_arrivals(trie, qterm):
    """Leaves in WM walk order, one yield per successful arrival path."""
    qf = flat(qterm)
    qsubs = [s for s, _ in subterms_preorder(qterm)]
    out = []
    def unify_rest(leaf, qi, b, si):
        key = leaf.key
        while si < len(key):
            if qi >= len(qf): return None
            sc, qc = key[si], qf[qi]
            if sc[0] == FUN:
                if qc[0] == FUN:
                    if sc[1] != qc[1]: return None
                    si += 1; qi += 1
                else:
                    e = sub_end(key, si, ARITY)
                    b = unify(qsubs[qi], cells_to_term(key[si:e]), b)
                    if b is None: return None
                    si = e; qi += 1
            else:
                b = unify(('var', -sc[1]), qsubs[qi], b)
                if b is None: return None
                si += 1; qi = sub_end(qf, qi, ARITY)
        return b if qi == len(qf) else None
    def node_depth(n):
        d = 0
        while n.parent is not None:
            n, d = n.parent, d + 1
        return d
    def walk(n, qi, b, spos=None):
        if isinstance(n, TLeaf):
            si = n.hang + 1 if spos is None else spos
            b2 = unify_rest(n, qi, b, si)
            if b2 is not None: out.append(n)
            return
        if qi >= len(qf): return
        qc = qf[qi]
        if qc[0] == FUN:
            c = n.children.get(qc)
            if c is not None: walk(c, qi + 1, b)
            qend = sub_end(qf, qi, ARITY)
            for sym in sorted((s for s in n.children if s[0] == VAR),
                              key=lambda s: -s[1]):
                b2 = unify(('var', -sym[1]), qsubs[qi], b)
                if b2 is not None: walk(n.children[sym], qend, b2)
        else:
            for sym in sorted((s for s in n.children if s[0] == VAR),
                              key=lambda s: -s[1]):
                b2 = unify(qsubs[qi], ('var', -sym[1]), b)
                if b2 is not None: walk(n.children[sym], qi + 1, b2)
            for e in list(n.exits):
                b2 = unify(qsubs[qi], cells_to_term(e.sub), b)
                if b2 is not None:
                    land = node_depth(e.start) + len(e.sub)
                    walk(e.ziel, qi + 1, b2, land)
    if trie.root is not None:
        walk(trie.root, 0, {})
    return out

# ---------- CP construction ----------
def make_cp(outer_face, outer_other, path, inner_from, inner_to):
    sub = outer_face
    for i in path: sub = sub[1][i]
    b = unify(sub, inner_from, {})
    if b is None: return None
    return (subst(replace_at(outer_face, path, inner_to), b),
            subst(outer_other, b))

def cp_keys(cp):
    l, r = cp
    a = renumber_pair(l, r)
    bb = renumber_pair(r, l)
    return (term_str(a[0]) + '#' + term_str(a[1]),
            term_str(bb[0]) + '#' + term_str(bb[1]))

# ---------- emission prediction ----------
def predict_batch(new_id, kind, l, r, mono, rtree, etree):
    out = []
    lq, rq = rename_apart(l), rename_apart(r)

    def tops_phase(qface, qother, trees, exclude_self_eq):
        for s, path in subterms_preorder(qface):
            if is_var(s): continue
            for tree in trees:
                for leaf in tops_arrivals(tree, s):
                    if tree is rtree:
                        ents = leaf.chain[:1]
                        if ents and ents[0][0] == new_id and kind == 'rule':
                            continue
                    else:
                        ents = [e for e in leaf.chain
                                if not (exclude_self_eq and e[0][:2] == new_id[:2])]
                    for (fid, fl, fr) in ents:
                        cp = make_cp(qface, qother, path,
                                     rename_apart(fl, 2000),
                                     rename_apart(fr, 2000))
                        if cp is not None:
                            out.append((fid, cp_keys(cp),
                                        'top@%s' % (path,)))

    def ett_phase(qfrom, qto, trees):
        for tree in trees:
            for leaf in tree.leaf_list:
                for (fid, fl, fr) in leaf.chain:
                    for s, path in subterms_preorder(fl):
                        if not path or is_var(s): continue
                        cp = make_cp(fl, fr, path, qfrom, qto)
                        if cp is not None:
                            out.append((fid, cp_keys(cp), 'ett'))

    if kind == 'rule':
        tops_phase(lq, rq, [rtree, etree], False)
        ett_phase(lq, rq, [rtree, etree])
    else:
        tops_phase(lq, rq, [rtree, etree], True)
        ett_phase(lq, rq, [rtree, etree])
        cp = make_cp(l, r, (), lq, rq)             # F
        if cp is not None: out.append((new_id, cp_keys(cp), 'F'))
        if not mono:
            rl, rr = renumber_pair(r, l)
            rlq, rrq = rename_apart(rl), rename_apart(rr)
            cp = make_cp(l, r, (), rlq, rrq)       # C
            if cp is not None: out.append((new_id, cp_keys(cp), 'C'))
            tops_phase(rlq, rrq, [etree, rtree], True)   # D
            ett_phase(rlq, rrq, [etree, rtree])          # E
            cp = make_cp(rl, rr, (), rlq, rrq)     # G
            if cp is not None: out.append((new_id, cp_keys(cp), 'G'))
    return out

# ---------- trace parsing ----------
events = []
lines = open(TRACE).read().splitlines()
i = 0
rule_re = re.compile(r'\.\.\. and added as new rule (\d+):')
eq_re = re.compile(r'\.\.\.and added to E as (such|mono equation) with number (\d+):')
cp_re = re.compile(r'critical pair (\d+) built with parents (-?\d+) and (-?\d+):')
rm_rule = re.compile(r'(?:simplify lhs of rule (\d+), removed from R\.|rule (\d+) removed from R\.)')
rm_eq = re.compile(r'equation no\. (-\d+) removed\.')

while i < len(lines):
    ln = lines[i]
    m = rule_re.search(ln)
    if m:
        lhs, rhs = lines[i + 1].split('->')
        events.append(('add_rule', (int(m.group(1)), parse_term(lhs), parse_term(rhs))))
        i += 2; continue
    m = eq_re.search(ln)
    if m:
        lhs, rhs = lines[i + 1].split('=')
        events.append(('add_eq', (int(m.group(2)), parse_term(lhs), parse_term(rhs),
                                  m.group(1) != 'such')))
        i += 2; continue
    m = cp_re.search(ln)
    if m:
        raw = lines[i + 1].strip()
        lraw, rraw = raw.split('#')
        events.append(('cp', (int(m.group(1)), int(m.group(2)), int(m.group(3)),
                              parse_term(lraw), parse_term(rraw))))
        i += 2; continue
    m = rm_rule.search(ln)
    if m:
        events.append(('rm_rule', int(m.group(1) or m.group(2)))); i += 1; continue
    m = rm_eq.search(ln)
    if m:
        events.append(('rm_eq', int(m.group(1)))); i += 1; continue
    i += 1

for t, p in events:
    if t in ('add_rule', 'add_eq'):
        note_arities(p[1]); note_arities(p[2])

# ---------- replay + validate ----------
rtree, etree = WmTrie(ARITY), WmTrie(ARITY)
rules, eqs = {}, {}
pending = None
results = []

def flush():
    global pending
    if pending is None: return
    desc, pred, obs = pending
    if DEBUG_BATCH and desc.startswith(DEBUG_BATCH):
        print('=== DEBUG', desc, '===')
        for k, (fid, pk, tag) in enumerate(pred):
            print('  P%-3d %-16s %-12s %s' % (k, str(fid), tag, pk[0][:70]))
        for (cpid, pa, pb, lraw, rraw) in obs:
            keys = cp_keys((lraw, rraw))
            hits = [k for k, (fid, pk, tag) in enumerate(pred)
                    if keys[0] in pk or keys[1] in pk]
            print('  cp%-5d (%s,%s) slots=%s' % (cpid, pa, pb, hits))
    pi = 0
    bad = []
    for (cpid, pa, pb, lraw, rraw) in obs:
        keys = cp_keys((lraw, rraw))
        found = None
        for k in range(pi, len(pred)):
            if keys[0] in pred[k][1] or keys[1] in pred[k][1]:
                found = k; break
        if found is None:
            anywhere = any(keys[0] in pk or keys[1] in pk for _, pk, _ in pred)
            bad.append((cpid, pa, pb, 'ooo' if anywhere else 'missing'))
        else:
            pi = found + 1
    results.append((desc, 'FAIL' if bad else 'OK', len(obs), len(pred), bad))
    pending = None

for (typ, p) in events:
    if typ == 'add_rule':
        flush()
        rid, l, r = p
        rules[rid] = (l, r)
        rtree.insert(flat(l), (('R', rid), l, r), depth(l))
        pred = predict_batch(('R', rid), 'rule', l, r, False, rtree, etree)
        pending = ('rule %d' % rid, pred, [])
    elif typ == 'add_eq':
        flush()
        eid, l, r, mono = p
        eqs[eid] = (l, r, mono)
        etree.insert(flat(l), (('E', eid, 0), l, r), depth(l))
        if not mono:
            rl, rr = renumber_pair(r, l)
            etree.insert(flat(rl), (('E', eid, 1), rl, rr), depth(rl))
        pred = predict_batch(('E', eid), 'eq', l, r, mono, rtree, etree)
        pending = ('eq -%d%s' % (eid, ' (mono)' if mono else ''), pred, [])
    elif typ == 'rm_rule':
        rid = p
        if rid in rules:
            l, r = rules.pop(rid)
            rtree.remove(flat(l), (('R', rid), l, r))
    elif typ == 'rm_eq':
        eid = -p
        if eid in eqs:
            l, r, mono = eqs.pop(eid)
            etree.remove(flat(l), (('E', eid, 0), l, r))
            if not mono:
                rl, rr = renumber_pair(r, l)
                etree.remove(flat(rl), (('E', eid, 1), rl, rr))
    elif typ == 'cp':
        if pending is not None:
            pending[2].append(p)
flush()

nok = sum(1 for r in results if r[1] == 'OK')
print('batches OK: %d / %d' % (nok, len(results)))
for (desc, st, no, np, bad) in results:
    if st != 'OK':
        print('  FAIL %-18s obs=%-4d pred=%-4d bad=%d  %s' %
              (desc, no, np, len(bad), bad[:4]))
