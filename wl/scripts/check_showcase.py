#!/usr/bin/env python3
# Adapted for THVMLink from WolframInstitute/PureMath scripts/check_showcase.py: the default pages are THVMLink's.
"""A symbol page shows what its symbol returns, verbatim, before anything derived from it.

For every reference page given (or every page under docs/en/ReferencePages/Symbols), find
the code cells that apply the page's own symbol and report the page when

  no bare call   no cell shows a bare call of the symbol, that is a cell whose last
                 statement is Sym[...] (optionally bound with x = ... and no trailing
                 semicolon), with nothing applied to the result: no ["property"], no
                 Length, no ===, no //;
  first wrapped  the first cell that applies the symbol is not such a bare call.

Usage: python3 scripts/check_showcase.py [page.md ...]; exits 1 when any page is reported.
"""
import glob, re, sys

CELL = re.compile(r'```wl\n(.*?)\n```', re.S)


def statement(cell):
    """The last top-level statement of a cell: setup lines end in a semicolon, the shown
    expression is what follows the last of them."""
    lines = [l for l in cell.strip().split('\n') if not l.strip().startswith('#|')]
    s = '\n'.join(lines).strip()
    depth = 0
    instr = False
    last = 0
    for i, ch in enumerate(s):
        if ch == '"' and (i == 0 or s[i - 1] != '\\'):
            instr = not instr
        if instr:
            continue
        if ch in '[({':
            depth += 1
        elif ch in '])}':
            depth -= 1
        elif ch == ';' and depth == 0:
            last = i + 1
    return s[last:].strip()


def bare_call(sym, cell):
    s = statement(cell)
    s = re.sub(r'^[A-Za-z][A-Za-z0-9]*\s*=\s*', '', s)
    if not s.startswith(sym + '['):
        return False
    depth = 0
    instr = False
    for i, ch in enumerate(s):
        if ch == '"' and (i == 0 or s[i - 1] != '\\'):
            instr = not instr
        if instr:
            continue
        if ch == '[':
            depth += 1
        elif ch == ']':
            depth -= 1
            if depth == 0:
                return i == len(s) - 1
    return False


def check(path):
    text = open(path).read()
    m = re.search(r'^Name:\s*(\S+)', text, re.M)
    if not m:
        return []
    sym = m.group(1)
    calls = [c.group(1) for c in CELL.finditer(text)
             if re.search(r'(?<![A-Za-z0-9`])' + re.escape(sym) + r'\s*\[', c.group(1))]
    if not calls:
        return []
    issues = []
    if not any(bare_call(sym, c) for c in calls):
        issues.append('no bare call')
    elif not bare_call(sym, calls[0]):
        issues.append('first wrapped: ' + statement(calls[0]).split('\n')[-1][:90])
    return issues


paths = sys.argv[1:] or sorted(glob.glob('wl/THVMLink/docs/Symbols/*.md'))
bad = 0
for p in paths:
    for issue in check(p):
        bad += 1
        print(f'{p}: {issue}')
print(f'check_showcase: {bad} issue(s) across {len(paths)} page(s)')
sys.exit(1 if bad else 0)
