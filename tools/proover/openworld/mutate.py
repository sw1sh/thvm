#!/usr/bin/env python3
"""Mutation battery for the open-world ProoVer corpus.

  python3 mutate.py CORPUSDIR [--per-proof N]

Reads CORPUSDIR/valid/*.s (harvest.py output), applies a battery of
bug classes derived from the TSTP / ProoVer specification -- NOT the 4
classes the 2026 corpus was built from -- and writes:

  CORPUSDIR/buggy/<stem>.<class>.s     gold = buggy
  CORPUSDIR/valid/<stem>.shuffle.s     gold = valid (order-independence
                                       control; the rules impose no
                                       record order)

Classes:
  axiom_lie      leaf formula differs from the problem formula it cites
  conj_lie       the conjecture / negated-conjecture import lies
  ghost_ref      leaf cites a nonexistent name with a perturbed formula
  drop_literal   a derived disjunction loses a literal (stronger claim)
  flip_polarity  a derived equation/atom flips polarity
  var_capture    two distinct variables of a derived step collapse
  circular       a step cites its own descendant (citation cycle)
  sk_scope       Skolem applications lose their argument dependencies
  sk_reuse       a second Skolem introduction reuses the first symbol
  esa_abuse      a derived step turns esa AND mutates its conclusion
  false_jump     the $false root cites a single unrelated leaf
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harvest import split_records, strip_comments, parse_record

def fnv(s):
    h = 1469598103934665603
    for c in s.encode():
        h = ((h ^ c) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h

def rebuild(recs):
    out = []
    for kind, name, role, formula, src in recs:
        if src:
            out.append("%s(%s, %s, %s, %s)." % (kind, name, role, formula, src))
        else:
            out.append("%s(%s, %s, %s)." % (kind, name, role, formula))
    return "\n".join(out) + "\n"

def is_leaf(src):
    return src.lstrip().startswith("file")

def is_derived(src):
    return "inference" in src

def flip_eq(f):
    if "!=" in f:
        return f.replace("!=", "=", 1)
    m = re.search(r"(?<![<=>~!])=(?!>)", f)
    if m:
        return f[:m.start()] + "!=" + f[m.end():]
    return "~(" + f + ")"

def swap_syms(f):
    syms = []
    for s in re.findall(r"\b([a-z][A-Za-z0-9_]*)\s*\(", f):
        if s not in syms and s not in ("file", "inference", "status"):
            syms.append(s)
    if len(syms) >= 2:
        a, b = syms[0], syms[1]
        return (f.replace(a + "(", "\x01(").replace(b + "(", a + "(")
                 .replace("\x01(", b + "("))
    return None

def mutate_formula(f):
    s = swap_syms(f)
    return s if s is not None else flip_eq(f)

def top_split(f, sep="|"):
    parts, depth, start = [], 0, 0
    for i, c in enumerate(f):
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif c == sep and depth == 0:
            parts.append(f[start:i])
            start = i + 1
    parts.append(f[start:])
    return [p.strip() for p in parts]

def variables(f):
    seen = []
    for v in re.findall(r"\b[A-Z][A-Za-z0-9_]*\b", f):
        if v not in seen:
            seen.append(v)
    return seen

def parents_of(src):
    m = re.findall(r"\[([^\[\]]*)\]", src)
    if not m:
        return []
    return [p.strip() for p in m[-1].split(",") if p.strip()]

def set_parents(src, plist):
    i = src.rfind("[")
    j = src.find("]", i)
    if i < 0 or j < 0:
        return src
    return src[:i + 1] + ", ".join(plist) + src[j:]

# --- classes: each takes recs (list of tuples), returns recs' or None --

def m_axiom_lie(recs, h):
    idx = [i for i, r in enumerate(recs)
           if is_leaf(r[4]) and r[2] in ("axiom", "hypothesis", "plain")]
    if not idx:
        return None
    i = idx[h % len(idx)]
    k, n, ro, f, s = recs[i]
    nf = mutate_formula(f)
    if nf == f:
        return None
    recs = list(recs)
    recs[i] = (k, n, ro, nf, s)
    return recs

def m_conj_lie(recs, h):
    idx = [i for i, r in enumerate(recs)
           if is_leaf(r[4]) and r[2] in ("conjecture", "negated_conjecture")]
    if not idx:
        return None
    i = idx[h % len(idx)]
    k, n, ro, f, s = recs[i]
    nf = mutate_formula(f)
    if nf == f:
        return None
    recs = list(recs)
    recs[i] = (k, n, ro, nf, s)
    return recs

def m_ghost_ref(recs, h):
    idx = [i for i, r in enumerate(recs) if is_leaf(r[4]) and "," in r[4]]
    if not idx:
        return None
    i = idx[h % len(idx)]
    k, n, ro, f, s = recs[i]
    s2 = re.sub(r",\s*([A-Za-z0-9_]+)\s*\)", r", \1_ghost)", s, count=1)
    nf = mutate_formula(f)
    if s2 == s or nf == f:
        return None
    recs = list(recs)
    recs[i] = (k, n, ro, nf, s2)
    return recs

def m_drop_literal(recs, h):
    idx = [i for i, r in enumerate(recs)
           if is_derived(r[4]) and len(top_split(r[3])) >= 2]
    if not idx:
        return None
    i = idx[h % len(idx)]
    k, n, ro, f, s = recs[i]
    parts = top_split(f)
    recs = list(recs)
    recs[i] = (k, n, ro, " | ".join(parts[:-1]), s)
    return recs

def m_flip_polarity(recs, h):
    idx = [i for i, r in enumerate(recs)
           if is_derived(r[4]) and r[3].strip() != "$false"]
    if not idx:
        return None
    i = idx[h % len(idx)]
    k, n, ro, f, s = recs[i]
    nf = flip_eq(f)
    if nf == f:
        return None
    recs = list(recs)
    recs[i] = (k, n, ro, nf, s)
    return recs

def m_var_capture(recs, h):
    idx = [i for i, r in enumerate(recs)
           if is_derived(r[4]) and len(variables(r[3])) >= 2]
    if not idx:
        return None
    i = idx[h % len(idx)]
    k, n, ro, f, s = recs[i]
    vs = variables(f)
    nf = re.sub(r"\b%s\b" % vs[1], vs[0], f)
    recs = list(recs)
    recs[i] = (k, n, ro, nf, s)
    return recs

def m_circular(recs, h):
    names = {r[1]: i for i, r in enumerate(recs)}
    cand = []
    for j, r in enumerate(recs):
        if not is_derived(r[4]):
            continue
        for p in parents_of(r[4]):
            i = names.get(p)
            if i is not None and is_derived(recs[i][4]):
                cand.append((i, j))       # j cites i; add j to i's parents
    if not cand:
        return None
    i, j = cand[h % len(cand)]
    k, n, ro, f, s = recs[i]
    ps = parents_of(s)
    recs = list(recs)
    recs[i] = (k, n, ro, f, set_parents(s, ps + [recs[j][1]]))
    return recs

def sk_symbols(recs):
    syms = []
    for r in recs:
        for m in re.finditer(r"new_symbols\s*\(\s*skolem\s*,\s*\[([^\]]*)\]", r[4]):
            for s in m.group(1).split(","):
                s = s.strip()
                if s and s not in syms:
                    syms.append(s)
    return syms

def strip_app(text, sym):
    """Replace every 'sym(...)' application by the bare 'sym'."""
    out = []
    i = 0
    pat = re.compile(r"\b%s\s*\(" % re.escape(sym))
    while True:
        m = pat.search(text, i)
        if not m:
            out.append(text[i:])
            break
        out.append(text[i:m.start()] + sym)
        depth = 1
        j = m.end()
        while j < len(text) and depth:
            if text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
            j += 1
        i = j
    return "".join(out)

def m_sk_scope(recs, h):
    syms = [s for s in sk_symbols(recs)
            if any(re.search(r"\b%s\s*\(" % re.escape(s), r[3]) for r in recs)]
    if not syms:
        return None
    sym = syms[h % len(syms)]
    return [(k, n, ro, strip_app(f, sym), strip_app(s, sym))
            for k, n, ro, f, s in recs]

def m_sk_reuse(recs, h):
    syms = sk_symbols(recs)
    if len(syms) < 2:
        return None
    a, b = syms[0], syms[1 + h % (len(syms) - 1)]
    if a == b:
        return None
    return [(k, n, ro, re.sub(r"\b%s\b" % re.escape(b), a, f),
             re.sub(r"\b%s\b" % re.escape(b), a, s))
            for k, n, ro, f, s in recs]

def m_esa_abuse(recs, h):
    idx = [i for i, r in enumerate(recs)
           if is_derived(r[4]) and "status(thm)" in r[4]
           and r[3].strip() != "$false"]
    if not idx:
        return None
    i = idx[h % len(idx)]
    k, n, ro, f, s = recs[i]
    nf = flip_eq(f)
    if nf == f:
        return None
    recs = list(recs)
    recs[i] = (k, n, ro, nf, s.replace("status(thm)", "status(esa)", 1))
    return recs

def m_false_jump(recs, h):
    fi = [i for i, r in enumerate(recs) if r[3].strip() == "$false"]
    li = [i for i, r in enumerate(recs) if is_leaf(r[4])]
    if not fi or not li:
        return None
    i = fi[0]
    k, n, ro, f, s = recs[i]
    ns = set_parents(s, [recs[li[h % len(li)]][1]])
    if ns == s:
        return None
    recs = list(recs)
    recs[i] = (k, n, ro, f, ns)
    return recs

def m_shuffle(recs, h):
    order = sorted(range(len(recs)), key=lambda i: fnv("%d:%d" % (h, i)))
    return [recs[i] for i in order]

CLASSES = [
    ("axiom_lie", m_axiom_lie), ("conj_lie", m_conj_lie),
    ("ghost_ref", m_ghost_ref), ("drop_literal", m_drop_literal),
    ("flip_polarity", m_flip_polarity), ("var_capture", m_var_capture),
    ("circular", m_circular), ("sk_scope", m_sk_scope),
    ("sk_reuse", m_sk_reuse), ("esa_abuse", m_esa_abuse),
    ("false_jump", m_false_jump),
]

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    corpus = sys.argv[1]
    per_proof = 4
    if "--per-proof" in sys.argv:
        per_proof = int(sys.argv[sys.argv.index("--per-proof") + 1])
    os.makedirs(os.path.join(corpus, "buggy"), exist_ok=True)
    man_path = os.path.join(corpus, "manifest.tsv")
    have = set()
    for ln in open(man_path).read().splitlines()[1:]:
        have.add(ln.split("\t")[0])
    man = open(man_path, "a")
    made = 0
    valids = sorted(f for f in os.listdir(os.path.join(corpus, "valid"))
                    if f.endswith(".s") and ".shuffle." not in f)
    for vf in valids:
        path = os.path.join(corpus, "valid", vf)
        raw = open(path).read()
        hdr = "\n".join(ln for ln in raw.splitlines() if ln.startswith("%"))
        recs = []
        ok = True
        for rec in split_records(strip_comments(raw)):
            p = parse_record(rec)
            if p is None:
                ok = False
                break
            recs.append(p)
        if not ok or not recs:
            print("skip (unparsed): %s" % vf)
            continue
        h = fnv(vf)
        stem = vf[:-2].rstrip(".")
        # rotate a deterministic window of classes over the battery
        chosen = [CLASSES[(h + k) % len(CLASSES)] for k in range(len(CLASSES))]
        n_here = 0
        for cname, fn in chosen:
            if n_here >= per_proof:
                break
            out_fn = "%s.%s.s" % (stem, cname)
            if out_fn in have:
                n_here += 1
                continue
            mut = fn(recs, h)
            if mut is None:
                continue
            open(os.path.join(corpus, "buggy", out_fn), "w").write(
                hdr + "\n" + rebuild(mut))
            man.write("%s\tbuggy\t%s\tmutate\n" % (out_fn, cname))
            made += 1
            n_here += 1
        # order-independence control (gold VALID)
        out_fn = "%s.shuffle.s" % stem
        if out_fn not in have:
            open(os.path.join(corpus, "valid", out_fn), "w").write(
                hdr + "\n" + rebuild(m_shuffle(recs, h)))
            man.write("%s\tvalid\tshuffle\tmutate\n" % out_fn)
            made += 1
    man.close()
    print("mutants written: %d" % made)
    return 0

if __name__ == "__main__":
    sys.exit(main())
