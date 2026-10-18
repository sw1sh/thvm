#!/usr/bin/env python3
# Adapted for THVMLink from WolframInstitute/PureMath scripts/check_example_groups.py: the default pages are THVMLink's.
"""Flag reset boundaries that split one example on a reference page: a cell that
binds a symbol (name = ..., or {a, b, ...} = ...) followed, after a '---' or a
heading, by a cell that uses that symbol. Usage: check_example_groups.py file.md ...

Tutorials are skipped. A path with a 'Tutorials' component gets one line naming
the reason and does not affect the exit status: the build (scripts/docs.wls)
converts tutorials with "EvaluateSeparator" -> None, so a tutorial threads state
from its first cell to its last and neither a '---' nor a heading clears anything
there. scripts/run_doc_examples.wls applies the same mode, and it is the gate for
tutorials."""
import re, sys
from pathlib import Path
ASSIGN = re.compile(r'^\s*([A-Za-z][A-Za-z0-9]*)\s*=(?!=)')
# a destructuring binding binds each name in its list
DESTRUCTURE = re.compile(r'^\s*\{\s*([A-Za-z][A-Za-z0-9]*(?:\s*,\s*[A-Za-z][A-Za-z0-9]*)*)\s*\}\s*=(?!=)')
def bound_names(line):
    m = ASSIGN.match(line)
    if m: return {m.group(1)}
    m = DESTRUCTURE.match(line)
    if m: return {name.strip() for name in m.group(1).split(',')}
    return set()
def cells(lines):
    """yield (kind, payload, lineno): kind in heading|break|cell"""
    i=0; n=len(lines); inb=False; buf=[]; start=0
    # skip frontmatter
    if lines and lines[0].strip()=='---':
        j=1
        while j<n and lines[j].strip()!='---': j+=1
        i=j+1
    while i<n:
        l=lines[i]
        if inb:
            if l.strip()=='```': inb=False; yield ('cell','\n'.join(buf),start)
            else: buf.append(l)
        elif l.startswith('```wl') or l.startswith('```mathematica') or l.strip()=='```wl':
            inb=True; buf=[]; start=i+1
        elif re.match(r'^#{1,6}\s', l): yield ('heading',l.strip(),i+1)
        elif re.match(r'^(-{3,}|_{3,}|\*{3,})\s*$', l): yield ('break',l.strip(),i+1)
        i+=1
def check(path):
    lines=open(path,encoding='utf-8').read().split('\n')
    bound={}   # symbol -> line of binding, within the current section, before the last break
    pending={} # symbols bound since the last break (not yet separated)
    issues=[]; breaks=0; total_breaks=0
    for kind,payload,ln in cells(lines):
        # A heading is a reset boundary exactly like a ---: the converter (MarkdownToNotebook
        # 486a861, 2026-09-02) clears every page binding at both, and a house-style symbol page
        # has no setup section that would protect anything. This gate used to reset its
        # bookkeeping at a heading - cross-heading carry-over from Basic Examples was once
        # sanctioned - so it never saw the class of split that shipped GeodesicBall.md's whole
        # Properties and Relations as unevaluated echoes.
        if kind=='heading' or kind=='break':
            total_breaks+=1
            bound.update(pending); pending={}
        else:
            # a cell that re-binds a name before using it starts a fresh example,
            # so the earlier binding is not what it reads - and so does a name
            # re-bound by its OWN cell since the last break (pending): that is the
            # house pattern, the definition as its own captioned cell, then the use
            rebound = set().union(*(bound_names(cl) for cl in payload.split('\n')))
            for sym,bl in bound.items():
                if sym in rebound or sym in pending: continue
                # a name inside a string literal - LieAlgebra["su3"], "not a signature" - is
                # not a read of the binding, so a quote on either side is not a word boundary
                if re.search(r'(?<![A-Za-z0-9`"])'+re.escape(sym)+r'(?![A-Za-z0-9`"])', payload):
                    issues.append((ln, sym, bl))
            for cl in payload.split('\n'):
                for name in bound_names(cl): pending[name]=ln
    return total_breaks, issues
bad=0
for path in sys.argv[1:] or sorted(__import__('glob').glob('wl/THVMLink/docs/*/*.md')):
    if 'Tutorials' in Path(path).parts:
        print(f"  skipped           {path}  (a tutorial threads state end to end, EvaluateSeparator -> None; run_doc_examples.wls is its gate)")
        continue
    tb, issues = check(path)
    if issues: bad+=1
    print(f"{len(issues):3d} split-uses  {tb:3d} breaks  {path}")
    for ln,sym,bl in issues[:6]: print(f"      line {ln} uses {sym!r} bound at line {bl} across a ---")
sys.exit(1 if bad else 0)
