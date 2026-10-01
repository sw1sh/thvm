#!/usr/bin/env python3
"""Static replica of FindProof's exit-fast lane gates.

It reads TPTP CNF problem text only, runs no prover, and says for each
SCHEDULE lane (tools/atp_cli/main.c) whether its gate lets it search (live)
or makes it exit at once (dead).  It mirrors the CLI's string-level gates:
  equals, narrowq, equals-twee   the goal is existential (an uppercase token
                                 in a negative literal)
  horn-cd, horn-back             an ifeq dispatcher and a ground goal
  fof-*                          never on cnf input
  unfold, unfold-tgt, -awr       a pure function definition f(X..) = t
                                 exists (the CLI's unfold dry run is not
                                 replicated)
  tgt-ax                         a non-conjecture positive unit with a
                                 ground compound subterm
  tgt-sel                        the conjecture symbols hit some but not
                                 all symbols
  ifeq                           an ifeq dispatcher ifeqN(X,X,Y[,Z]) = Y
  otter-*                        not static (--otter-exit orients the
                                 distributivity laws under the lane's LPO
                                 precedence): unknown (None)
Every other lane is always live.

Usage: static_gates.py <tptp_root> <problem.p>...
  prints: problem, the gate bits, the live lanes (otter-* lanes as '?'), in
  the SCHEDULE order of tools/atp_cli/main.c.
"""
import os
import re
import sys

IDENT = re.compile(r"[A-Za-z0-9_]+")


def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    return "\n".join(
        "" if l.lstrip().startswith("%") else l.split("%")[0]
        for l in t.split("\n"))


def records(text, root):
    out = []
    text = strip_comments(text)
    for m in re.finditer(
            r"include\(\s*'([^']+)'\s*(?:,\s*\[([^\]]*)\])?\s*\)\s*\.", text):
        cand = os.path.join(root, m.group(1))
        if os.path.exists(cand):
            with open(cand, errors="replace") as fp:
                sub = records(fp.read(), root)
            if m.group(2):
                keep = set(s.strip() for s in m.group(2).split(","))
                sub = [r for r in sub if r[0] in keep]
            out += sub
    i = 0
    while True:
        j = text.find("cnf(", i)
        if j < 0:
            break
        if j > 0 and (text[j - 1].isalnum() or text[j - 1] == "_"):
            i = j + 4
            continue
        depth = 0
        k = j + 3
        while k < len(text):
            if text[k] == "(":
                depth += 1
            elif text[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        body = text[j + 4:k]
        parts = []
        d = 0
        s = 0
        for q, ch in enumerate(body):
            if ch == "(":
                d += 1
            elif ch == ")":
                d -= 1
            elif ch == "," and d == 0 and len(parts) < 2:
                parts.append(body[s:q])
                s = q + 1
        parts.append(body[s:])
        if len(parts) >= 3:
            out.append((parts[0].strip(), parts[1].strip(), parts[2].strip()))
        i = k + 1
    return out


def split_lit(lit):
    lit = lit.strip()
    while lit.startswith("(") and lit.endswith(")"):
        d = 0
        whole = True
        for q, ch in enumerate(lit):
            if ch == "(":
                d += 1
            elif ch == ")":
                d -= 1
                if d == 0 and q != len(lit) - 1:
                    whole = False
                    break
        if not whole:
            break
        lit = lit[1:-1].strip()
    neg = False
    if lit.startswith("~"):
        neg = True
        lit = lit[1:].strip()
        if lit.startswith("(") and lit.endswith(")"):
            lit = lit[1:-1]
    d = 0
    for q, ch in enumerate(lit):
        if ch == "(":
            d += 1
        elif ch == ")":
            d -= 1
        elif d == 0 and lit[q:q + 2] == "!=":
            return lit[:q].strip(), lit[q + 2:].strip(), True
        elif d == 0 and ch == "=" and lit[q - 1:q] != "!":
            return lit[:q].strip(), lit[q + 1:].strip(), neg
    return lit, "$true", neg


def is_var_tok(tok):
    return tok[0].isupper()


def has_upper_token(s):
    return any(m.group(0)[0].isupper() for m in IDENT.finditer(s))


def side_has_ground_compound(s):
    for p, ch in enumerate(s):
        if ch != "(":
            continue
        depth = 1
        ground = True
        q = p + 1
        while q < len(s) and depth > 0:
            c = s[q]
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            elif c.isupper() and not (s[q - 1].isalnum() or s[q - 1] == "_"):
                ground = False
            q += 1
        if ground and depth == 0:
            return True
    return False


def parse_term(s):
    s = s.strip()
    m = IDENT.match(s)
    if not m:
        return None
    rest = s[m.end():].strip()
    if not rest:
        return (m.group(0), [])
    if not rest.startswith("("):
        return None
    inner = rest[1:-1]
    args = []
    d = 0
    st = 0
    for q, ch in enumerate(inner):
        if ch == "(":
            d += 1
        elif ch == ")":
            d -= 1
        elif ch == "," and d == 0:
            args.append(inner[st:q])
            st = q + 1
    args.append(inner[st:])
    return (m.group(0), [a.strip() for a in args])


def is_dispatcher(side, other):
    t = parse_term(side)
    o = other.strip()
    if t is None or len(t[1]) not in (3, 4):
        return False
    if not all(IDENT.fullmatch(a) and is_var_tok(a) for a in t[1]):
        return False
    if not (IDENT.fullmatch(o) and is_var_tok(o)):
        return False
    a0, a1, a2 = t[1][0], t[1][1], t[1][2]
    return a0 == a1 and a2 == o and a2 != a0


def definition_head(side):
    t = parse_term(side)
    if t is None or not t[1] or t[0][0].isupper():
        return None
    args = t[1]
    if len(set(args)) != len(args):
        return None
    if not all(IDENT.fullmatch(a) and is_var_tok(a) for a in args):
        return None
    return t[0], args


def definition_body_ok(body, fname, args):
    for m in IDENT.finditer(body):
        tok = m.group(0)
        if tok == fname:
            return False
        if is_var_tok(tok):
            q = m.end()
            while q < len(body) and body[q].isspace():
                q += 1
            if q < len(body) and body[q] == "(":
                continue
            if tok not in args:
                return False
    return True


def gates(path, root):
    """The gate bits of one problem file (includes resolve under root)."""
    with open(path, errors="replace") as fp:
        recs = records(fp.read(), root)
    cls = []
    for _name, role, lit in recs:
        lhs, rhs, neg = split_lit(lit)
        cls.append((lhs, rhs, neg, "negated_conjecture" in role))
    syms = set()
    for lhs, rhs, _neg, _conj in cls:
        for side in (lhs, rhs):
            syms.update(m.group(0) for m in IDENT.finditer(side)
                        if not is_var_tok(m.group(0)))
    existential = any(neg and (has_upper_token(l) or has_upper_token(r))
                      for l, r, neg, _c in cls)
    dispatcher = any((not neg) and (is_dispatcher(l, r) or is_dispatcher(r, l))
                     for l, r, neg, _c in cls)
    axiom_ground_compound = any(
        (not neg) and (not c)
        and (side_has_ground_compound(l) or side_has_ground_compound(r))
        for l, r, neg, c in cls)
    definition = False
    for l, r, neg, c in cls:
        if neg or c:
            continue
        for head, body in ((l, r), (r, l)):
            hd = definition_head(head)
            if hd and definition_body_ok(body, hd[0], hd[1]):
                definition = True
    conj_toks = set()
    for l, r, _neg, c in cls:
        if c:
            conj_toks.update(m.group(0) for m in IDENT.finditer(l))
            conj_toks.update(m.group(0) for m in IDENT.finditer(r))
    hit = syms & conj_toks
    return {
        "cnf_records": len(cls),
        "existential": existential,
        "dispatcher": dispatcher,
        "axiom_ground_compound": axiom_ground_compound,
        "definition": definition,
        "tgt_sel": 0 < len(hit) < len(syms),
    }


def lane_live(lane, g):
    """True (searches), False (exits at once) or None (not static)."""
    if lane.startswith("fof-"):
        return False
    if lane in ("equals", "narrowq", "equals-twee"):
        return g["existential"]
    if lane in ("horn-cd", "horn-back"):
        return g["dispatcher"] and not g["existential"]
    if lane in ("unfold", "unfold-tgt", "unfold-tgt-awr"):
        return g["definition"]
    if lane == "tgt-ax":
        return g["axiom_ground_compound"]
    if lane == "tgt-sel":
        return g["tgt_sel"]
    if lane == "ifeq":
        return g["dispatcher"]
    if lane.startswith("otter"):
        return None
    return True


def schedule_lanes():
    """The SCHEDULE[] lane names of tools/atp_cli/main.c, in order."""
    main_c = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "..", "atp_cli", "main.c")
    pat = re.compile(r'^  \{"([^"]*)", *"([^"]*)", *[0-9.]*\},')
    with open(main_c) as fp:
        return [m.group(1) for m in map(pat.match, fp) if m]


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    root = sys.argv[1]
    lanes = schedule_lanes()
    for path in sys.argv[2:]:
        g = gates(path, root)
        live = []
        for lane in lanes:
            v = lane_live(lane, g)
            if v is None:
                live.append(lane + "?")
            elif v:
                live.append(lane)
        bits = " ".join(f"{k}={int(v) if isinstance(v, bool) else v}"
                        for k, v in g.items())
        name = os.path.basename(path)[:-2]
        print(f"{name}\t{bits}\t{len(live)}\t{','.join(live)}")


if __name__ == "__main__":
    main()
