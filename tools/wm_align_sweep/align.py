#!/usr/bin/env python3
"""Selection-sequence aligner: wmcli -a 4 trace vs thvm CPSEL trace.

Waldmeister consumes one selection per `ue (..) touched.` event; thvm's
WM preset logs one CPSEL line per popped critical pair (orphan pops are
discarded without consuming a selection -- skipped here, matching WM's
selectNonOrphan).  Both streams reduce to canonical equation keys
(alpha-renumbered variables, orientation-insensitive), and the gate is
position-by-position identity of the two key sequences.

Parser notes proven on the McCune-II 118/118 alignment:
  * WM verbose contracts unary chains as `not^3(u)` -- expand before
    canonicalizing or deep CPs spuriously mismatch.
  * WM stamps initial axioms w1 = -2147483648 (MIN_INT); thvm computes a
    real weight there, so weight parity is only checked on non-MIN_INT
    rows.

usage:
  align.py --wm WM_TRACE --thvm-out THVM_STDOUT --thvm-trace THVM_STDERR
           [--report REPORT_PATH]

stdout: one machine-readable line, e.g.
  ALIGN wm=118 thvm=118 prefix=118 identical=Y firstdiv=- note=weights-exact
"""
import argparse
import re
import sys
from collections import Counter, defaultdict, deque

# ---------- term parsing (shared canonical form: tuples) ----------
# var: ('var', id);  application: (name, (child, ...)).

POW_RE = re.compile(r'([a-zA-Z_][a-zA-Z0-9_]*)\^(\d+)\(')


def expand_pow(s):
    """`not^3(u)` -> `not(not(not(u)))` (WM verbose unary contraction)."""
    while True:
        m = POW_RE.search(s)
        if m is None:
            return s
        name, n = m.group(1), int(m.group(2))
        s = s[:m.start()] + (name + '(') * n + s[m.end():]
        i = m.start() + len((name + '(') * n)
        depth = 1
        while depth:
            if s[i] == '(':
                depth += 1
            elif s[i] == ')':
                depth -= 1
            i += 1
        s = s[:i] + ')' * (n - 1) + s[i:]


WM_VAR_RE = re.compile(r'x(\d+)$')

# Free-variable-instance GROUNDING constants: WM's extra constants
# (SO_Extrakonstante, printed `const<N>` in the verbose trace) and
# thvm's reserved labels 1/2 (ATP_RESERVED_LABEL_MIN_CONST/_CONST2 =
# SO_minimaleKonstante/SO_const2, absent from the PRSYM map) play the
# same role under different names; canonicalize both to one token so
# grounded instances align.
WM_GC_RE = re.compile(r'const\d+$')
GC_TOKEN = '_gc'


def parse_wm_term(s):
    """WM prefix syntax: `and(x1,not(x2))`; variables are x<N>."""
    s = expand_pow(s.strip())
    pos = [0]

    def p():
        m = re.match(r'[a-zA-Z_][a-zA-Z0-9_]*', s[pos[0]:])
        if m is None:
            raise ValueError('bad WM term at %r' % s[pos[0]:pos[0] + 30])
        name = m.group(0)
        pos[0] += len(name)
        if pos[0] < len(s) and s[pos[0]] == '(':
            pos[0] += 1
            args = [p()]
            while s[pos[0]] == ',':
                pos[0] += 1
                args.append(p())
            if s[pos[0]] != ')':
                raise ValueError('bad WM term (missing )) in %r' % s)
            pos[0] += 1
            return (name, tuple(args))
        m = WM_VAR_RE.match(name)
        if m is not None:
            return ('var', int(m.group(1)))
        if WM_GC_RE.match(name) is not None:
            return (GC_TOKEN, ())
        return (name, ())

    t = p()
    if pos[0] != len(s):
        raise ValueError('trailing junk in WM term %r' % s)
    return t


def parse_thvm_sexpr(s, names):
    """thvm CPSEL S-exprs: `(C3 V0 (C4 V1))`, `C5`, `V2`."""
    toks = s.replace('(', ' ( ').replace(')', ' ) ').split()
    pos = [0]

    def atom(tok):
        if tok.startswith('V'):
            return ('var', int(tok[1:]))
        if tok.startswith('C'):
            lab = int(tok[1:])
            if lab in (1, 2):
                return (GC_TOKEN, ())
            return (names.get(lab, tok), ())
        raise ValueError('bad thvm atom %r in %r' % (tok, s))

    def p():
        tok = toks[pos[0]]
        pos[0] += 1
        if tok != '(':
            return atom(tok)
        head = toks[pos[0]]
        pos[0] += 1
        if not head.startswith('C'):
            raise ValueError('bad thvm head %r in %r' % (head, s))
        lab = int(head[1:])
        args = []
        while toks[pos[0]] != ')':
            args.append(p())
        pos[0] += 1
        return (names.get(lab, head), tuple(args))

    t = p()
    if pos[0] != len(toks):
        raise ValueError('trailing junk in thvm term %r' % s)
    return t


# ---------- canonical pair keys ----------

def render(t, mp):
    if t[0] == 'var':
        if t[1] not in mp:
            mp[t[1]] = len(mp) + 1
        return 'v%d' % mp[t[1]]
    if not t[1]:
        return t[0]
    return '%s(%s)' % (t[0], ','.join(render(a, mp) for a in t[1]))


def canon_key(l, r):
    """Alpha-renumbered, orientation-insensitive pair key."""
    m1 = {}
    s1 = '%s#%s' % (render(l, m1), render(r, m1))
    m2 = {}
    s2 = '%s#%s' % (render(r, m2), render(l, m2))
    return min(s1, s2)


# ---------- WM trace ----------

WM_TOUCH_RE = re.compile(r'^ue \((-?\d+), (-?\d+)\) touched\.')
WM_SUE_RE = re.compile(r'\.\.\. added to SUE: (-?\d+), (\d+)\.')


def parse_wm(path):
    """Selections: [(key, w1_or_None, raw_pair_str, lineno)]."""
    lines = open(path, errors='replace').read().splitlines()
    sels = []
    weights = defaultdict(deque)  # canon key -> queued w1s, FIFO
    last_pair = None              # most recent `lhs # rhs` line seen
    expect_pair = 0               # >0: next pair line is a touched selection
    pending = None                # (lineno,) of the touch event
    for i, ln in enumerate(lines):
        if expect_pair:
            if ' # ' in ln:
                raw = ln.strip()
                l, r = raw.split(' # ', 1)
                key = canon_key(parse_wm_term(l), parse_wm_term(r))
                w = weights[key].popleft() if weights[key] else None
                sels.append((key, w, raw, pending))
                expect_pair = 0
                last_pair = raw
                continue
            # blank/divider lines between "touched." and the pair
            if ln.strip():
                raise ValueError('%s:%d: expected pair after touched, got %r'
                                 % (path, i + 1, ln))
            continue
        m = WM_TOUCH_RE.match(ln)
        if m is not None:
            expect_pair = 1
            pending = i + 1
            continue
        if ' # ' in ln:
            last_pair = ln.strip()
            continue
        m = WM_SUE_RE.search(ln)
        if m is not None and last_pair is not None:
            l, r = last_pair.split(' # ', 1)
            key = canon_key(parse_wm_term(l), parse_wm_term(r))
            weights[key].append(int(m.group(1)))
    return sels


# ---------- thvm trace ----------

PRSYM_RE = re.compile(r'^PRSYM (\d+) (\S+) (\d+)')
CPSEL_RE = re.compile(r'^CPSEL pick=(\d+) j=\d+ seq=(\d+) pri=(\d+) '
                      r'rules=\d+ lhs=(.*) rhs=(.*)$')


def parse_thvm(out_path, trace_path):
    """Picks: [(key, pri, raw_line, lineno)] -- orphan pops skipped."""
    names = {}
    for ln in open(out_path, errors='replace'):
        m = PRSYM_RE.match(ln)
        if m is not None:
            names[int(m.group(1))] = m.group(2)
    picks = []
    for i, ln in enumerate(open(trace_path, errors='replace')):
        if not ln.startswith('CPSEL'):
            continue
        ln = ln.rstrip('\n')
        if ' ORPHAN' in ln or ' WOLFDEEPJOIN' in ln:
            continue
        m = CPSEL_RE.match(ln)
        if m is None:
            raise ValueError('%s:%d: unparsable CPSEL line %r'
                             % (trace_path, i + 1, ln))
        lt = parse_thvm_sexpr(m.group(4), names)
        rt = parse_thvm_sexpr(m.group(5), names)
        picks.append((canon_key(lt, rt), int(m.group(3)), ln, i + 1))
    return picks, names


# ---------- comparison ----------

WM_MIN_INT = -2147483648


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--wm', required=True)
    ap.add_argument('--thvm-out', required=True)
    ap.add_argument('--thvm-trace', required=True)
    ap.add_argument('--report')
    args = ap.parse_args()

    wm = parse_wm(args.wm)
    th, names = parse_thvm(args.thvm_out, args.thvm_trace)

    n = min(len(wm), len(th))
    prefix = 0
    while prefix < n and wm[prefix][0] == th[prefix][0]:
        prefix += 1
    identical = (len(wm) == len(th) and prefix == len(wm))

    # Secondary signal: WM canonically sorts the input axioms at load
    # (SpezNormierung GleichungenSortieren) and stamps them w1=MIN_INT,
    # so multi-axiom problems diverge inside the intake segment on
    # ORDER alone.  Align the two streams again with the axiom keys
    # removed (first occurrence each side) to expose whether the
    # DERIVED selection stream agrees past the intake permutation.
    ax_keys = Counter(k for k, w, _, _ in wm if w == WM_MIN_INT)
    nonax_wm, nonax_th = [], []
    for seq, out in ((wm, nonax_wm), (th, nonax_th)):
        left = Counter(ax_keys)
        for rec in seq:
            if left[rec[0]] > 0:
                left[rec[0]] -= 1
            else:
                out.append(rec)
    m = min(len(nonax_wm), len(nonax_th))
    nonax_prefix = 0
    while (nonax_prefix < m and
           nonax_wm[nonax_prefix][0] == nonax_th[nonax_prefix][0]):
        nonax_prefix += 1
    nonax_identical = (len(nonax_wm) == len(nonax_th) and
                       nonax_prefix == len(nonax_wm))

    # weight parity over the aligned prefix (WM MIN_INT rows excluded)
    wdiff = [(k + 1, wm[k][1], th[k][1]) for k in range(prefix)
             if wm[k][1] is not None and wm[k][1] != WM_MIN_INT and
             wm[k][1] != th[k][1]]

    # content-set comparison (order-insensitive)
    cw, ct = Counter(k for k, _, _, _ in wm), Counter(k for k, _, _, _ in th)
    only_wm = sum((cw - ct).values())
    only_th = sum((ct - cw).values())

    notes = []
    if not identical:
        notes.append('content-delta wm-only=%d thvm-only=%d' % (only_wm, only_th))
        notes.append('post-axiom nonax-wm=%d nonax-thvm=%d nonax-prefix=%d%s'
                     % (len(nonax_wm), len(nonax_th), nonax_prefix,
                        ' IDENTICAL' if nonax_identical else ''))
    notes.append('weights-exact' if not wdiff else
                 'weight-mismatches=%d (first @%d: wm=%s thvm=%s)' % (
                     len(wdiff), wdiff[0][0], wdiff[0][1], wdiff[0][2]))
    firstdiv = '-' if identical else str(prefix + 1)

    if args.report:
        with open(args.report, 'w') as fp:
            fp.write('wm=%d thvm=%d prefix=%d identical=%s firstdiv=%s\n'
                     % (len(wm), len(th), prefix, 'Y' if identical else 'N',
                        firstdiv))
            fp.write('note: %s\n\n' % '; '.join(notes))
            if not identical:
                lo = max(0, prefix - 3)
                hi = min(max(len(wm), len(th)), prefix + 10)
                fp.write('--- positions %d..%d (1-based) ---\n' % (lo + 1, hi))
                for k in range(lo, hi):
                    wk = wm[k] if k < len(wm) else None
                    tk = th[k] if k < len(th) else None
                    mark = ' ' if (wk and tk and wk[0] == tk[0]) else '*'
                    fp.write('%s %4d WM   w1=%-12s %s\n'
                             % (mark, k + 1,
                                'None' if wk is None else wk[1],
                                '-' if wk is None else wk[2]))
                    fp.write('%s %4d thvm pri=%-11s %s\n'
                             % (mark, k + 1,
                                'None' if tk is None else tk[1],
                                '-' if tk is None else tk[2]))
                fp.write('\n--- multiset deltas (first 50 each side) ---\n')
                for key, cnt in list((cw - ct).items())[:50]:
                    fp.write('WM-only   x%d  %s\n' % (cnt, key))
                for key, cnt in list((ct - cw).items())[:50]:
                    fp.write('thvm-only x%d  %s\n' % (cnt, key))
            if wdiff:
                fp.write('\n--- weight mismatches on aligned prefix ---\n')
                for pos, w, p in wdiff[:50]:
                    fp.write('@%d wm=%s thvm=%s\n' % (pos, w, p))

    print('ALIGN wm=%d thvm=%d prefix=%d identical=%s firstdiv=%s note=%s'
          % (len(wm), len(th), prefix, 'Y' if identical else 'N', firstdiv,
             ';'.join(notes).replace(' ', '_')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
