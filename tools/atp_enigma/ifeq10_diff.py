#!/usr/bin/env python3
"""Compare two TPTP CNF files modulo clause order and renaming.

Usage: ifeq10_diff.py OURS THEIRS [--show N]

Built for FindProof --emit-ifeq10 against TPTP's own -10 files.  Each cnf
record becomes (role, literals) with variables renamed by first occurrence
inside the clause; the files match when these multisets are equal
(equation orientation is kept).  A match then says "(clause names differ)"
when the multisets of (name, clause) pairs differ, so a swapped name is
caught, and "(clause order differs)" when the clauses come in another
order.  Exit 0 on a match, 1 on a mismatch, 2 on unreadable input or a
term nested too deep to compare.

On a mismatch two coarser readings say where the difference lies:
  generated  the generated symbol families (ifeqN, tupleN, sK<n>_..,
             sP<n>_..) collapsed to one name each: a match here means only
             their numbering differs;
  nest       additionally every ifeq tower flattened to its conditions as
             a sorted multiset and every variable read as one placeholder:
             a match here means the conditions are the same but stacked in
             another order (the symbol order behind the atom order).
Text processing only.
"""
import hashlib
import os
import re
import sys
import threading
from collections import Counter

# Parsing and rendering recurse once per nesting level, so they run in a
# thread with a large stack and a raised limit, and a deeper term exits 2
# rather than reading as a mismatch.
DEPTH_LIMIT = 200000
STACK_BYTES = 256 * 1024 * 1024

TOKEN = re.compile(r"""
    (?P<ws>\s+)
  | (?P<comment>%[^\n]*|/\*.*?\*/)
  | (?P<squote>'(?:[^'\\]|\\.)*')
  | (?P<dquote>"(?:[^"\\]|\\.)*")
  | (?P<word>[A-Za-z0-9_$]+)
  | (?P<op><=>|<~>|=>|<=|~\||~&|!=|[()\[\],.:|&~=!?*])
""", re.X | re.S)

# Symbols the symbols and shape levels keep by name.
FIXED = {"true", "IFEQ", "TUPLE", "SK", "SP"}

GENERATED = [
    (re.compile(r"^ifeq\d*$"), "IFEQ"),
    (re.compile(r"^tuple\d*$"), "TUPLE"),
    (re.compile(r"^sK\d+(_.*)?$"), "SK"),
    (re.compile(r"^sP\d+(_.*)?$"), "SP"),
]


# The name slot of an include record: "@" starts no TPTP formula name.
INCLUDE = "@include"


class ParseError(Exception):
    pass


def tokens(text):
    out = []
    pos = 0
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if m is None:
            raise ParseError("bad character %r at offset %d" % (text[pos], pos))
        pos = m.end()
        kind = m.lastgroup
        if kind in ("ws", "comment"):
            continue
        out.append((kind, m.group(kind)))
    return out


class Parser:
    def __init__(self, toks):
        self.toks = toks
        self.i = 0

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else (None, None)

    def take(self, value=None):
        tok = self.peek()
        if tok[0] is None or (value is not None and tok[1] != value):
            raise ParseError("expected %r, found %r" % (value, tok[1]))
        self.i += 1
        return tok

    def term(self):
        kind, text = self.take()
        if kind == "squote":
            name = re.sub(r"\\(.)", r"\1", text[1:-1])
        elif kind == "word":
            name = text
        else:
            raise ParseError("expected a term, found %r" % text)
        if not (kind == "word" and text[0].isupper()) and self.peek()[1] == "(":
            self.take("(")
            args = [self.term()]
            while self.peek()[1] == ",":
                self.take(",")
                args.append(self.term())
            self.take(")")
            return (name, tuple(args))
        if kind == "word" and text[0].isupper():
            return ("?" + name,)
        return (name, ())

    def literal(self):
        if self.peek()[1] == "~":
            self.take("~")
            sign, lhs, rhs = self.literal()
            return (not sign, lhs, rhs)
        if self.peek()[1] == "(":
            self.take("(")
            lit = self.literal()
            self.take(")")
            return lit
        lhs = self.term()
        op = self.peek()[1]
        if op in ("=", "!="):
            self.take(op)
            return (op == "=", lhs, self.term())
        return (True, lhs, ("true", ()))

    def clause(self):
        if self.peek()[1] == "(":
            self.take("(")
            lits = self.clause()
            self.take(")")
            return lits
        lits = [self.literal()]
        while self.peek()[1] == "|":
            self.take("|")
            lits.append(self.literal())
        return lits

    def skip_balanced(self):
        depth = 0
        while True:
            kind, text = self.take()
            if text == "(":
                depth += 1
            elif text == ")":
                if depth == 0:
                    self.i -= 1
                    return
                depth -= 1

    def include(self):
        kind, text = self.take()
        if kind != "squote":
            raise ParseError("expected a quoted file name, found %r" % text)
        path = re.sub(r"\\(.)", r"\1", text[1:-1])
        sel = None
        if self.peek()[1] == ",":
            self.take(",")
            if self.peek()[1] == "*":
                self.take("*")
            else:
                self.take("[")
                sel = []
                while self.peek()[1] != "]":
                    sel.append(self.take()[1])
                    if self.peek()[1] == ",":
                        self.take(",")
                self.take("]")
        self.take(")")
        self.take(".")
        return path, sel

    def records(self, includes=False):
        """The cnf records; with includes, an include(F[, sel]) record
        becomes (INCLUDE, F, sel) in place (sel None when absent)."""
        out = []
        while self.peek()[0] is not None:
            _, kw = self.take()
            self.take("(")
            if kw == "include" and includes:
                path, sel = self.include()
                out.append((INCLUDE, path, sel))
                continue
            if kw != "cnf":
                raise ParseError("not a cnf record: %s" % kw)
            name = self.take()[1]
            self.take(",")
            role = self.take()[1]
            self.take(",")
            lits = self.clause()
            if self.peek()[1] == ",":
                self.take(",")
                self.skip_balanced()
            self.take(")")
            self.take(".")
            out.append((name, role, lits))
        return out


def read_records(path):
    with open(path, encoding="utf-8", errors="replace") as fp:
        return Parser(tokens(fp.read())).records()


def read_problem(path, tptp_root, sel=None, depth=0):
    """The cnf records of a problem file with its include()s resolved
    against dirname(path), then tptp_root, as FindProof resolves them; a
    selection list keeps only the named records, nested selections
    intersect."""
    if depth > 8:
        raise ParseError("includes nested too deep in %s" % path)
    with open(path, encoding="utf-8", errors="replace") as fp:
        recs = Parser(tokens(fp.read())).records(includes=True)
    out = []
    for rec in recs:
        if rec[0] == INCLUDE:
            inc, sub = rec[1], rec[2]
            if sel is None:
                merged = sub
            elif sub is None:
                merged = sel
            else:
                merged = [n for n in sel if n in sub]
            cands = [os.path.join(os.path.dirname(path), inc), os.path.join(tptp_root, inc)]
            hit = next((c for c in cands if os.path.isfile(c)), None)
            if hit is None:
                raise ParseError("cannot read include '%s' of %s" % (inc, path))
            out.extend(read_problem(hit, tptp_root, merged, depth + 1))
        elif sel is None or rec[0] in sel:
            out.append(rec)
    return out


def rename_symbol(name, level):
    if level == "exact" or name.startswith("?"):
        return name
    for pat, family in GENERATED:
        if pat.match(name):
            return family
    return name


def render(term, names, level):
    head = term[0]
    if head.startswith("?"):
        if level in ("nest", "shape"):
            return "V"
        if head not in names:
            names[head] = "V%d" % len(names)
        return names[head]
    head = rename_symbol(head, level)
    if level == "shape" and head not in FIXED:
        head = "F"
    elif level == "symbols" and head not in FIXED:
        key = "!" + head
        if key not in names:
            names[key] = "F%d" % len(names)
        head = names[key]
    if not term[1]:
        return head
    return head + "(" + ",".join(render(a, names, level) for a in term[1]) + ")"


def flatten_nest(lhs, rhs):
    conds = []
    t = lhs
    while (not t[0].startswith("?") and rename_symbol(t[0], "generated") == "IFEQ"
           and len(t[1]) == 4 and t[1][3] == rhs):
        conds.append((t[1][0], t[1][1]))
        t = t[1][2]
    return conds, t


def canonical_literals(lits):
    """The literals with each equation oriented and the clause sorted by a
    renaming-free shape, so that the symbols level is blind to literal
    order and orientation (ties keep their input order)."""
    out = []
    for sign, lhs, rhs in lits:
        a, b = render(lhs, {}, "shape"), render(rhs, {}, "shape")
        if b < a:
            lhs, rhs, a, b = rhs, lhs, b, a
        out.append(((not sign, a, b), (sign, lhs, rhs)))
    out.sort(key=lambda x: x[0])
    return [lit for _, lit in out]


def clause_key(role, lits, level):
    """One clause as a string.  Levels: exact; generated (generated symbol
    families collapsed); nest (ifeq towers as condition sets); symbols
    (every non-generated symbol and variable renamed by first occurrence,
    literals in canonical order: equal for two clauses that are the same
    modulo variable and symbol renaming)."""
    names = {}
    parts = []
    if level == "symbols":
        lits = canonical_literals(lits)
    for sign, lhs, rhs in lits:
        if level == "nest":
            conds, then = flatten_nest(lhs, rhs)
            cs = sorted(render(a, names, level) + "=" + render(b, names, level)
                        for a, b in conds)
            parts.append("%s[%s]%s->%s" % ("+" if sign else "-", ";".join(cs),
                                           render(then, names, level),
                                           render(rhs, names, level)))
        else:
            parts.append("%s%s=%s" % ("+" if sign else "-", render(lhs, names, level),
                                      render(rhs, names, level)))
    return role + ":" + "|".join(parts)


def problem_key(records):
    """A key equal for two problems that are the same modulo variable and
    symbol renaming, clause order and clause names: the sorted goal
    (negated_conjecture) clause keys, then the sorted keys of the rest, at
    the symbols level.  Symbols are renamed per clause, so the key is
    coarser than the problem: two problems with one key need not be
    renamings of each other, which makes a duplicate check built on it
    err only towards dropping a problem."""
    goals = sorted(clause_key("c", l, "symbols") for _, r, l in records
                   if r == "negated_conjecture")
    rest = sorted(clause_key("c", l, "symbols") for _, r, l in records
                  if r != "negated_conjecture")
    text = "\n".join(goals) + "\n#\n" + "\n".join(rest)
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def compare(ours, theirs, level):
    a = Counter(clause_key(r, l, level) for _, r, l in ours)
    b = Counter(clause_key(r, l, level) for _, r, l in theirs)
    return a - b, b - a


def main():
    args = []
    show = 5
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        if argv[i] == "--show" and i + 1 < len(argv) and argv[i + 1].isdigit():
            show = int(argv[i + 1])
            i += 2
            continue
        args.append(argv[i])
        i += 1
    if len(args) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    try:
        ours = read_records(args[0])
        theirs = read_records(args[1])
    except (OSError, ParseError) as e:
        print("ifeq10_diff: %s" % e, file=sys.stderr)
        return 2
    only_ours, only_theirs = compare(ours, theirs, "exact")
    if not only_ours and not only_theirs:
        ka = [clause_key(r, l, "exact") for _, r, l in ours]
        kb = [clause_key(r, l, "exact") for _, r, l in theirs]
        same_names = Counter(zip((n for n, _, _ in ours), ka)) == \
            Counter(zip((n for n, _, _ in theirs), kb))
        print("match: %d clauses%s%s" % (len(ours),
                                         "" if same_names else " (clause names differ)",
                                         "" if ka == kb else " (clause order differs)"))
        return 0
    print("MISMATCH: %d clauses only in ours, %d only in theirs (of %d / %d)"
          % (sum(only_ours.values()), sum(only_theirs.values()), len(ours), len(theirs)))
    for level in ("generated", "nest"):
        o, t = compare(ours, theirs, level)
        print("  %-9s %s" % (level, "match" if not o and not t else
                             "differs (%d / %d)" % (sum(o.values()), sum(t.values()))))
    for label, diff in (("ours", only_ours), ("theirs", only_theirs)):
        for key in list(diff)[:show]:
            print("  only in %s: %s" % (label, key))
    return 1


def guarded_main():
    try:
        return main()
    except RecursionError:
        print("ifeq10_diff: a term is nested deeper than %d levels" % DEPTH_LIMIT,
              file=sys.stderr)
        return 2
    except MemoryError:
        print("ifeq10_diff: out of memory", file=sys.stderr)
        return 2


def run_deep(fn):
    sys.setrecursionlimit(DEPTH_LIMIT)
    threading.stack_size(STACK_BYTES)
    rc = [2]

    def body():
        rc[0] = fn()

    t = threading.Thread(target=body)
    t.start()
    t.join()
    return rc[0]


if __name__ == "__main__":
    sys.exit(run_deep(guarded_main))
