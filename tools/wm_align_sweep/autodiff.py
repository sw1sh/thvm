#!/usr/bin/env python3
# Firstdiv measurer for the EFFICIENT (-auto Automodus) Waldmeister reference.
# Usage:  <thvm RULEADD stream> | autodiff.py <ref.auto_rules>
# Sister to run_one.sh (which targets the HEAVY no-auto reference). FEQ uses
# -auto -- see memory project_atp_wm_efficient_automodus.
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
def parse_thvm(s):
    toks = re.findall(r'\(|\)|[^\s()]+', s); pos = [0]
    def p():
        if toks[pos[0]] == '(':
            pos[0] += 2
            args = []
            while toks[pos[0]] != ')': args.append(p())
            pos[0] += 1; return ('o', tuple(args))
        t = toks[pos[0]]; pos[0] += 1
        return ('v', t) if re.fullmatch(r'V\d+', t) else ('k', t)
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
auto = []
for ln in open(sys.argv[1], errors='ignore'):
    if '->' in ln:
        a, b = ln.strip().split('->'); auto.append(rc(parse_auto(a), parse_auto(b)))
thvm = []
for ln in sys.stdin:
    mm = re.search(r'RULEADD slot=\d+ lhs=(.*) rhs=(.*)', ln)
    if mm: thvm.append(rc(parse_thvm(mm.group(1)), parse_thvm(mm.group(2))))
n = min(len(auto), len(thvm))
fd = next((k + 1 for k in range(n) if auto[k] != thvm[k]), -1)
print(f"auto={len(auto)} thvm={len(thvm)} matched_prefix={(fd - 1) if fd > 0 else n} "
      f"firstdiv={'rule ' + str(fd) if fd > 0 else 'none/' + str(n)}")
