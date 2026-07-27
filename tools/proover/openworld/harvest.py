#!/usr/bin/env python3
"""Harvest real TSTP proofs into an open-world ProoVer validation corpus.

  python3 harvest.py OUTDIR PROBLEM.p [PROBLEM.p ...]
      [--findproof BIN] [--vampire BIN] [--wall N] [--max N]

For each problem (SERIAL -- never a parallel sweep), run each available
prover with a short wall; when a prover emits an SZS proof block, rewrite
it into the competition layout:

  OUTDIR/Problems/<base>.p          the problem file (copied verbatim)
  OUTDIR/valid/<base>.<prover>.s    the proof, leaves rewritten to
                                    file('Problems/<base>.p', name)
  OUTDIR/manifest.tsv               file, gold, class, prover

FindProof cites leaves as file('<path>') with NO formula name; the name
is recovered by matching the leaf formula against the problem records
(canonical variable renaming + whitespace stripping).  Vampire cites
file('<path>', name) already; only the path is rewritten.

The 2026 post-mortem's core validation lesson: the old corpus was 80
self-generated proofs in our own rendering with 4 self-invented bug
classes.  This corpus is real prover output; mutate.py adds a bug
battery derived from the TSTP/ProoVer spec, not from what we already
detect.
"""
import os
import re
import subprocess
import sys

def parse_args(argv):
    out, probs = None, []
    fp = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                      "../../../bin/FindProof")
    vamp = "/opt/homebrew/bin/vampire"
    wall, maxn = 20, 40
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--findproof":
            fp = argv[i + 1]; i += 2
        elif a == "--vampire":
            vamp = argv[i + 1]; i += 2
        elif a == "--wall":
            wall = int(argv[i + 1]); i += 2
        elif a == "--max":
            maxn = int(argv[i + 1]); i += 2
        elif out is None:
            out = a; i += 1
        else:
            probs.append(a); i += 1
    return out, probs, os.path.abspath(fp), vamp, wall, maxn

def split_records(text):
    """TPTP records: '.'-terminated at paren/bracket depth 0."""
    recs, depth, inq, start = [], 0, False, 0
    i = 0
    while i < len(text):
        c = text[i]
        if inq:
            if c == "'":
                inq = False
        elif c == "'":
            inq = True
        elif c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif c == "." and depth == 0:
            recs.append(text[start:i])
            start = i + 1
        i += 1
    return [r.strip() for r in recs if r.strip()]

def strip_comments(text):
    out = []
    for ln in text.splitlines():
        j = None
        inq = False
        for k, c in enumerate(ln):
            if inq:
                if c == "'":
                    inq = False
            elif c == "'":
                inq = True
            elif c == "%":
                j = k
                break
        out.append(ln if j is None else ln[:j])
    return "\n".join(out)

def split_top_args(s):
    args, depth, inq, start = [], 0, False, 0
    for i, c in enumerate(s):
        if inq:
            if c == "'":
                inq = False
        elif c == "'":
            inq = True
        elif c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif c == "," and depth == 0:
            args.append(s[start:i])
            start = i + 1
    args.append(s[start:])
    return [a.strip() for a in args]

def parse_record(rec):
    """-> (kind, name, role, formula, source) or None."""
    m = re.match(r"\s*(fof|cnf)\s*\(", rec)
    if not m:
        return None
    inner = rec[m.end():]
    # strip the record-closing ')'
    inner = inner.rstrip()
    if inner.endswith(")"):
        inner = inner[:-1]
    args = split_top_args(inner)
    if len(args) < 3:
        return None
    src = args[3] if len(args) > 3 else ""
    return (m.group(1), args[0], args[1], args[2], src)

def canon_formula(f):
    """Canonical text: variables renamed in first-occurrence order,
    whitespace stripped."""
    toks = re.findall(r"[A-Za-z0-9_$]+|\S", f)
    ren, out = {}, []
    for t in toks:
        if re.match(r"[A-Z]", t):
            if t not in ren:
                ren[t] = "v%d" % len(ren)
            out.append(ren[t])
        else:
            out.append(t)
    return "".join(out)

def swap_eq(f):
    """For a top-level 'A = B' / 'A != B', the side-swapped variant
    (provers freely reorient unit equations when reprinting leaves)."""
    depth = 0
    i = 0
    while i < len(f):
        c = f[i]
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        elif depth == 0:
            if f.startswith("!=", i):
                return "%s != %s" % (f[i + 2:].strip(), f[:i].strip())
            if c == "=" and not f.startswith("=>", i) and \
               (i == 0 or f[i - 1] not in "<>!~="):
                return "%s = %s" % (f[i + 1:].strip(), f[:i].strip())
        i += 1
    return None

def canon_keys(f):
    ks = {canon_formula(f)}
    s = swap_eq(f)
    if s is not None:
        ks.add(canon_formula(s))
    return ks

def problem_table(prob_path):
    text = strip_comments(open(prob_path).read())
    tab = []
    for rec in split_records(text):
        p = parse_record(rec)
        if p:
            tab.append(p)
    return tab

def extract_proof_block(out_text):
    m = re.search(r"% SZS output start [^\n]*\n(.*?)% SZS output end",
                  out_text, re.S)
    return m.group(1) if m else None

def rewrite_proof(block, prob_tab, prob_rel):
    """Rewrite file() directives to file('<prob_rel>', name)."""
    canon_map = {}
    for kind, name, role, formula, src in prob_tab:
        for k in canon_keys(formula):
            canon_map.setdefault(k, name)
    recs = split_records(strip_comments(block))
    out = []
    unmatched = 0
    for rec in recs:
        p = parse_record(rec)
        if p is None:
            out.append(rec + ".")
            continue
        kind, name, role, formula, src = p
        m = re.match(r"\s*file\s*\(\s*'([^']*)'\s*(?:,\s*([A-Za-z0-9_]+))?\s*\)", src)
        if m:
            fname = m.group(2)
            if fname == "unknown":      # Vampire's placeholder, not a name
                fname = None
            if fname is None:
                for k in canon_keys(formula):
                    fname = canon_map.get(k)
                    if fname is not None:
                        break
            if fname is None:
                unmatched += 1
                newsrc = "file('%s')" % prob_rel
            else:
                newsrc = "file('%s', %s)" % (prob_rel, fname)
            out.append("%s(%s, %s, %s, %s)." % (kind, name, role, formula, newsrc))
        else:
            out.append("%s(%s, %s, %s, %s)." % (kind, name, role, formula, src)
                       if src else "%s(%s, %s, %s)." % (kind, name, role, formula))
    return "\n".join(out) + "\n", unmatched

def run(cmd, wall, env=None):
    try:
        e = dict(os.environ)
        if env:
            e.update(env)
        r = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=wall + 15, env=e)
        return r.stdout
    except subprocess.TimeoutExpired:
        return ""
    except FileNotFoundError:
        return ""

def main():
    out, probs, fp, vamp, wall, maxn = parse_args(sys.argv)
    if not out or not probs:
        print(__doc__)
        return 2
    os.makedirs(os.path.join(out, "Problems"), exist_ok=True)
    os.makedirs(os.path.join(out, "valid"), exist_ok=True)
    man_path = os.path.join(out, "manifest.tsv")
    have = set()
    if os.path.exists(man_path):
        for ln in open(man_path).read().splitlines()[1:]:
            have.add(ln.split("\t")[0])
    man = open(man_path, "a")
    if os.path.getsize(man_path) == 0:
        man.write("file\tgold\tclass\tprover\n")
    n = 0
    for prob in probs:
        if n >= maxn:
            break
        base = os.path.basename(prob)
        stem = base[:-2] if base.endswith(".p") else base
        provers = []
        if os.path.exists(fp):
            provers.append(("findproof",
                            [fp, "--szs", "--proof", "--wall=%d" % wall, prob],
                            {"THVM_ATP_RSS_ABORT_MB": "5300"}))
        if os.path.exists(vamp):
            provers.append(("vampire",
                            [vamp, "--mode", "casc", "-t", str(wall),
                             "--proof", "tptp", prob], {}))
        for pname, cmd, env in provers:
            fn = "%s.%s.s" % (stem, pname)
            if fn in have:
                continue
            txt = run(cmd, wall, env)
            if not re.search(r"SZS status (Unsatisfiable|Theorem)", txt):
                continue
            block = extract_proof_block(txt)
            if block is None:
                continue
            tab = problem_table(prob)
            prob_rel = "Problems/%s" % base
            proof, unmatched = rewrite_proof(block, tab, prob_rel)
            if unmatched:
                print("  %s: %d unmatched leaves (kept nameless)" % (fn, unmatched))
            dst_prob = os.path.join(out, "Problems", base)
            if not os.path.exists(dst_prob):
                open(dst_prob, "w").write(open(prob).read())
            hdr = "%% Proof    : %s\n" % prob_rel
            open(os.path.join(out, "valid", fn), "w").write(hdr + proof)
            man.write("%s\tvalid\t-\t%s\n" % (fn, pname))
            man.flush()
            print("harvested %s" % fn)
            n += 1
    man.close()
    print("total harvested this run: %d" % n)
    return 0

if __name__ == "__main__":
    sys.exit(main())
