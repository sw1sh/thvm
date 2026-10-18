#!/usr/bin/env python3
# Adapted for THVMLink from WolframInstitute/PureMath scripts/check_enumerations.py: the default pages are THVMLink's.
"""One example cell shows one value: never a list that bundles several outputs, and
never a semicolon-separated compound whose earlier statements hide theirs.

Reports every code cell whose last statement is a list literal of two or more elements
that are each a call or a property lookup, such as

    {gr["Order"], gr["Characteristic"], gr["ModulusPolynomial"]}
    {ExtensionDegree[cubic], ExtensionBasis[cubic]}

and every cell of two or more top-level statements, such as

    alg = GeometricAlgebra[2]; GeometricProduct[alg[1], alg[2]]

The first hides three outputs behind one list and belongs in three cells; the second hides
the algebra the reader never sees and belongs in two, the binding first as its own
captioned cell whose output shows the object. Both stay inside one example group. A list
of plain data (an input) is not reported.

Usage: python3 scripts/check_enumerations.py [page.md ...]; without arguments every page
under docs/en; exits 1 when any cell is reported.
"""
import glob, re, sys

CELL = re.compile(r'```wl\n(.*?)\n```', re.S)


def last_statement(s):
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


def top_elements(s):
    inner = s[1:-1]
    depth = 0
    instr = False
    parts = []
    cur = ''
    for i, ch in enumerate(inner):
        if ch == '"' and (i == 0 or inner[i - 1] != '\\'):
            instr = not instr
        if not instr:
            if ch in '[({':
                depth += 1
            elif ch in '])}':
                depth -= 1
            elif ch == ',' and depth == 0:
                parts.append(cur.strip())
                cur = ''
                continue
        cur += ch
    parts.append(cur.strip())
    return parts


def closes_at_end(s):
    """the leading brace is the one closing at the end: `{a, b}` is a list, `{a, b} & /@ c` is not"""
    depth = 0
    instr = False
    for i, ch in enumerate(s):
        if ch == '"' and (i == 0 or s[i - 1] != '\\'):
            instr = not instr
        if instr:
            continue
        if ch in '[({':
            depth += 1
        elif ch in '])}':
            depth -= 1
            if depth == 0:
                return i == len(s) - 1
    return False


def output_like(e):
    return (re.match(r'^[A-Za-z][A-Za-z0-9]*(\[.*\])?\["', e) is not None
            or re.match(r'^[A-Z][A-Za-z0-9]*\[', e) is not None)


def statement_count(s):
    depth = 0
    instr = False
    count = 0
    seen = False
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
            if seen:
                count += 1
            seen = False
        elif not ch.isspace():
            seen = True
    return count + (1 if seen else 0)


def body_of(cell):
    return '\n'.join(l for l in cell.strip().split('\n') if not l.strip().startswith('#|'))


def compound(cell):
    return statement_count(body_of(cell)) >= 2


def enumeration(cell):
    body = body_of(cell)
    s = re.sub(r'^[A-Za-z][A-Za-z0-9]*\s*=\s*', '', last_statement(body))
    if not (s.startswith('{') and s.endswith('}') and closes_at_end(s)):
        return None
    els = top_elements(s)
    return els if len(els) >= 2 and all(output_like(e) for e in els) else None


paths = sys.argv[1:] or sorted(glob.glob('wl/THVMLink/docs/*/*.md'))
bad = 0
for p in paths:
    for m in CELL.finditer(open(p).read()):
        els = enumeration(m.group(1))
        if els:
            bad += 1
            print(f'{p}: {len(els)} outputs in one cell: {m.group(1).strip().splitlines()[-1][:100]}')
        if compound(m.group(1)):
            bad += 1
            first = m.group(1).strip().splitlines()[0]
            print(f'{p}: {statement_count(body_of(m.group(1)))} statements in one cell: {first[:100]}')
print(f'check_enumerations: {bad} cell(s) across {len(paths)} page(s)')
sys.exit(1 if bad else 0)
