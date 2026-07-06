#!/usr/bin/env python3
# Merged-fact-stream firstdiv measurer for the EFFICIENT (-auto Automodus)
# Waldmeister reference.  Sister to autodiff.py, which compares RULE adds
# only -- this one compares the full kept-fact stream (rules + E-set
# equations) so unorientable-equation adds count as facts, matching thvm's
# RULEADD stream (which reports both).  Equations compare orientation-
# insensitively (canonical min over both directions).
#
# usage:  autodiff_facts.py <ref.auto_facts> <thvm_stderr_file> [problem.pr]
#
# <ref.auto_facts> is the tagged full-run extraction from a verbose
# `wmcli -auto -a 4` trace (R\t<lhs -> rhs> / E\t<lhs = rhs> lines), e.g.:
#   wmcli -auto -a 4 x.pr 2>&1 | awk '
#     /added as new rule/ { r=1; next }
#     r && /->/           { print "R\t" $0; r=0; next }
#     /added to E as/     { e=1; next }
#     e && / = / && /opCenterdot/ { print "E\t" $0; e=0; next }'
# (the E line is the line AFTER the "added to E as ..." message -- the
# NORMALIZED added form; the '#'-separated line before it is the raw pop
# form and must NOT be used).
#
# [problem.pr] supplies the SIGNATURE symbol order so thvm's C<label>
# constants map to WM's names (label = 3 + signature index).  Without it,
# ground constants in later facts (skolem-grounded RHS) false-positive.
import re, sys

def parse_auto(s):
    s = s.strip()
    def p(i):
        m = re.match(r'[A-Za-z]\w*', s[i:]); name = m.group(0); i += m.end()
        if i < len(s) and s[i] == '(':
            i += 1; args = []
            while True:
                a, i = p(i); args.append(a)
                if s[i] == ',': i += 1
                elif s[i] == ')': i += 1; break
            return ('o', tuple(args)), i
        return (('v', name) if re.fullmatch(r'x\d+', name) else ('k', name)), i
    return p(0)[0]

SYM = {}

def parse_thvm(s):
    toks = re.findall(r'\(|\)|[^\s()]+', s); pos = [0]
    def p():
        if toks[pos[0]] == '(':
            pos[0] += 2
            args = []
            while toks[pos[0]] != ')': args.append(p())
            pos[0] += 1; return ('o', tuple(args))
        t = toks[pos[0]]; pos[0] += 1
        return ('v', t) if re.fullmatch(r'V\d+', t) else ('k', SYM.get(t, t))
    return p()

def rc(l, r):
    m = {}
    def go(n):
        if n[0] == 'v':
            if n[1] not in m: m[n[1]] = '#' + str(len(m))
            return ('v', m[n[1]])
        if n[0] == 'k': return n
        return ('o', tuple(go(a) for a in n[1]))
    return (go(l), go(r))

def canon(A, B):
    return min(str(rc(A, B)), str(rc(B, A)))

if len(sys.argv) > 3:
    # SIGNATURE section: one "name: argsorts -> sort" line per symbol,
    # in label order starting at PR_LABEL_BASE = 3.
    in_sig = False
    idx = 3
    for ln in open(sys.argv[3], errors='ignore'):
        if ln.startswith('SIGNATURE'):
            in_sig = True
            ln = ln[len('SIGNATURE'):]
        elif in_sig and not ln.startswith(' '):
            break
        if in_sig:
            m = re.match(r'\s*([A-Za-z]\w*)\s*:', ln)
            if m:
                SYM['C' + str(idx)] = m.group(1)
                idx += 1

wm, kinds = [], []
for ln in open(sys.argv[1], errors='ignore'):
    if ln.startswith('R\t'):
        a, b = ln[2:].strip().split('->')
        wm.append(canon(parse_auto(a), parse_auto(b))); kinds.append('R')
    elif ln.startswith('E\t'):
        a, b = ln[2:].strip().split(' = ')
        wm.append(canon(parse_auto(a), parse_auto(b))); kinds.append('E')

thvm = []
for ln in open(sys.argv[2], errors='ignore'):
    mm = re.search(r'RULEADD slot=\d+ (?:uid=\d+ )?lhs=(.*) rhs=(.*)', ln)
    if mm:
        thvm.append(canon(parse_thvm(mm.group(1)), parse_thvm(mm.group(2))))

print(f"wm={len(wm)} facts ({kinds.count('R')}R+{kinds.count('E')}E)  thvm={len(thvm)}")
n = min(len(wm), len(thvm))
fd = next((k + 1 for k in range(n) if wm[k] != thvm[k]), None)
print(f"ordered: matched_prefix={fd-1 if fd else n}  "
      f"firstdiv={'fact '+str(fd)+' (kind '+kinds[fd-1]+')' if fd else 'NONE within '+str(n)}")
if fd:
    import difflib
    sm = difflib.SequenceMatcher(None, wm, thvm, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            print(f"  equal   wm[{i1+1}..{i2}] = thvm[{j1+1}..{j2}]  ({i2-i1})")
        elif tag == 'insert':
            print(f"  EXTRA   thvm[{j1+1}..{j2}]  ({j2-j1})")
        elif tag == 'delete':
            ks = ','.join(kinds[k] for k in range(i1, min(i2, i1 + 6)))
            print(f"  MISSING wm[{i1+1}..{i2}]  ({i2-i1}, kinds {ks}...)")
        else:
            ks = ','.join(kinds[k] for k in range(i1, min(i2, i1 + 6)))
            print(f"  REPLACE wm[{i1+1}..{i2}] ({ks}) vs thvm[{j1+1}..{j2}]")
