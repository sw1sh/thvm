#!/usr/bin/env python3
"""Synthesize a labelled ProoVer test corpus.

Each scenario FAMILY builds a genuinely-sound first-order refutation proof in
the ProoVer annotation schema (axioms -> negated_conjecture -> skolemize ->
instantiate -> consequence/$false).  For every valid proof we emit buggy
variants by mutating exactly one step, one per applicable bug class:

  neg        wrong quantifier dual in the negated_conjecture   (example1_e)
  sk_absent  declared Skolem symbol absent from the result      (example4_e)
  sk_reuse   a skolemize step reuses a prior Skolem symbol       (example3_e)
  unentail   a consequence/deduction step draws an unentailed conclusion (example2_e)

Gold label is by construction: valid proofs are sound; each mutant injects a
genuine unsoundness.  Output: <out>/{valid,buggy}/*.p + manifest.tsv.
"""
import os, sys, copy

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))

# ---- a proof is an ordered list of Step ----
class Step:
    def __init__(self, name, role, formula, rule=None, status=None,
                 parents=None, new_sym=None, prob=None):
        self.name, self.role, self.formula = name, role, formula
        self.rule, self.status = rule, status
        self.parents = parents or []
        self.new_sym = new_sym         # declared skolem symbol (skolemize)
        self.prob = prob               # source problem name (input steps)

def src(s):
    if s.rule is None:
        return "file('%s',%s)" % (s.prob or "synth", s.name)
    ann = "status(%s)" % s.status
    if s.rule == "skolemize" and s.new_sym:
        ann += ", new_symbols(skolem, [%s])" % s.new_sym
    return "inference(%s, [%s], [%s])" % (s.rule, ann, ", ".join(s.parents))

def render(steps):
    out = []
    for s in steps:
        out.append("fof(%s, %s,\n    %s, %s)." % (s.name, s.role, s.formula, src(s)))
    return "\n".join(out) + "\n"

# ===================================================================
# scenario families -- each returns (list[Step], meta) for the VALID proof.
# meta['conj'] = (quantifier_prefix, matrix) so the neg mutation can build a
# wrong dual; families set it to None when the conjecture is ground.
# ===================================================================

def fam_witness(rel, const, i):
    """A: forall x . exists y . rel(y,x);  conj exists u,v . rel(u,v).
       Two existentials -> two skolemize steps (enables sk_reuse)."""
    pre = "! [X] : ? [Y] : ? [Z] : %s(Y, Z, X)" % rel
    steps = [
        Step("ax", "axiom", pre, prob="synth"),
        Step("c", "conjecture", "? [U] : ? [V] : ? [W] : %s(U, V, W)" % rel, prob="synth"),
        Step("nc", "negated_conjecture",
             "~(? [U] : ? [V] : ? [W] : %s(U, V, W))" % rel,
             rule="negated_conjecture", status="cth", parents=["c"]),
        Step("sk1", "plain", "! [X] : ? [Z] : %s(sK%d0(X), Z, X)" % (rel, i),
             rule="skolemize", status="esa", parents=["ax"], new_sym="sK%d0" % i),
        Step("sk2", "plain", "! [X] : %s(sK%d0(X), sK%d1(X), X)" % (rel, i, i),
             rule="skolemize", status="esa", parents=["sk1"], new_sym="sK%d1" % i),
        Step("inst", "plain", "%s(sK%d0(%s), sK%d1(%s), %s)" % (rel, i, const, i, const, const),
             rule="instantiate", status="thm", parents=["sk2"]),
        Step("false", "plain", "$false",
             rule="consequence", status="thm", parents=["nc", "inst"]),
    ]
    meta = {"conj": ("? [U] : ? [V] : ? [W] :", "%s(U, V, W)" % rel)}
    return steps, meta

def fam_horn(p, q, const, i):
    """B: p(const);  forall x . (p(x) => q(x));  conj exists x . q(x)."""
    steps = [
        Step("ax1", "axiom", "%s(%s)" % (p, const), prob="synth"),
        Step("ax2", "axiom", "! [X] : (%s(X) => %s(X))" % (p, q), prob="synth"),
        Step("c", "conjecture", "? [X] : %s(X)" % q, prob="synth"),
        Step("nc", "negated_conjecture", "~(? [X] : %s(X))" % q,
             rule="negated_conjecture", status="cth", parents=["c"]),
        Step("rule_c", "plain", "%s(%s) => %s(%s)" % (p, const, q, const),
             rule="instantiate", status="thm", parents=["ax2"]),
        Step("false", "plain", "$false",
             rule="consequence", status="thm", parents=["nc", "ax1", "rule_c"]),
    ]
    meta = {"conj": ("? [X] :", "%s(X)" % q)}
    return steps, meta

def fam_trans(f, g, a, b, c, i):
    """C: a=b; b=c;  conj a=c  (equality; deduction step)."""
    steps = [
        Step("ax1", "axiom", "%s = %s" % (a, b), prob="synth"),
        Step("ax2", "axiom", "%s = %s" % (b, c), prob="synth"),
        Step("c", "conjecture", "%s = %s" % (a, c), prob="synth"),
        Step("nc", "negated_conjecture", "%s != %s" % (a, c),
             rule="negated_conjecture", status="cth", parents=["c"]),
        Step("ded", "plain", "%s = %s" % (a, c),
             rule="deduction", status="thm", parents=["ax1", "ax2"]),
        Step("false", "plain", "$false",
             rule="consequence", status="thm", parents=["nc", "ded"]),
    ]
    meta = {"conj": None}   # conjecture is ground -> no neg dual
    return steps, meta

def fam_one_ex(rel, const, i):
    """E: forall x . exists y . rel(x,y);  conj exists y . rel(const,y).
       One existential -> one skolemize step (no sk_reuse)."""
    steps = [
        Step("ax", "axiom", "! [X] : ? [Y] : %s(X, Y)" % rel, prob="synth"),
        Step("c", "conjecture", "? [Y] : %s(%s, Y)" % (rel, const), prob="synth"),
        Step("nc", "negated_conjecture", "~(? [Y] : %s(%s, Y))" % (rel, const),
             rule="negated_conjecture", status="cth", parents=["c"]),
        Step("sk1", "plain", "! [X] : %s(X, sK%d0(X))" % (rel, i),
             rule="skolemize", status="esa", parents=["ax"], new_sym="sK%d0" % i),
        Step("inst", "plain", "%s(%s, sK%d0(%s))" % (rel, const, i, const),
             rule="instantiate", status="thm", parents=["sk1"]),
        Step("false", "plain", "$false",
             rule="consequence", status="thm", parents=["nc", "inst"]),
    ]
    meta = {"conj": ("? [Y] :", "%s(%s, Y)" % (rel, const))}
    return steps, meta

# ---- theme instances (lexical diversity, not just renames of one proof) ----
FAMILIES = []
for k, (rel, cst) in enumerate([("rel", "a"), ("connects", "n0"), ("beats", "p1"),
                                ("adj", "v0"), ("sends", "m0"), ("cites", "d0")]):
    FAMILIES.append(("witness%d" % k, lambda i=k, r=rel, c=cst: fam_witness(r, c, i)))
for k, (p, q, cst) in enumerate([("human", "mortal", "socrates"),
                                 ("prime", "positive", "two"),
                                 ("bird", "flies", "tweety"),
                                 ("metal", "conducts", "iron"),
                                 ("even", "integer", "four"),
                                 ("dog", "animal", "rex")]):
    FAMILIES.append(("horn%d" % k, lambda i=k+10, pp=p, qq=q, c=cst: fam_horn(pp, qq, c, i)))
for k, (a, b, c) in enumerate([("x1", "y1", "z1"), ("p", "q", "r"),
                               ("s0", "s1", "s2"), ("alpha", "beta", "gamma")]):
    FAMILIES.append(("trans%d" % k, lambda i=k+20, aa=a, bb=b, cc=c: fam_trans("f", "g", aa, bb, cc, i)))
for k, (rel, cst) in enumerate([("likes", "alice"), ("owns", "bob"), ("near", "site0"),
                                ("teaches", "carol"), ("visits", "dave"), ("knows", "erin")]):
    FAMILIES.append(("oneex%d" % k, lambda i=k+30, r=rel, c=cst: fam_one_ex(r, c, i)))

# ===================================================================
# mutations -- each returns a mutated step list, or None if inapplicable.
# ===================================================================
def find(steps, rule):
    return [s for s in steps if s.rule == rule]

def mut_neg(steps, meta):
    if not meta.get("conj"):
        return None
    pre, mat = meta["conj"]
    nc = [s for s in steps if s.rule == "negated_conjecture"]
    if not nc:
        return None
    m = copy.deepcopy(steps)
    for s in m:
        if s.rule == "negated_conjecture":
            # wrong dual: keep the conjecture's quantifier prefix, negate the
            # matrix (should have flipped the quantifiers).
            s.formula = "%s ~(%s)" % (pre, mat)
    return m

def mut_sk_absent(steps, meta):
    if not find(steps, "skolemize"):
        return None
    m = copy.deepcopy(steps)
    for s in m:
        if s.rule == "skolemize":
            s.new_sym = "sKmissing"   # declared, but the formula uses the real one
            break
    return m

def mut_sk_reuse(steps, meta):
    sk = find(steps, "skolemize")
    if len(sk) < 2:
        return None
    first_sym = sk[0].new_sym         # e.g. sKi0
    m = copy.deepcopy(steps)
    seen = 0
    for s in m:
        if s.rule == "skolemize":
            seen += 1
            if seen == 2:
                # reuse the first step's symbol instead of a fresh one, both in
                # the declaration and in the formula.
                s.formula = s.formula.replace(s.new_sym + "(", first_sym + "(")
                s.new_sym = first_sym
    return m

def mut_unentail(steps, meta):
    # Insert a genuinely-unentailed intermediate deduction, from the first
    # input axiom (whose content shares no symbol with the bogus atom), just
    # before the closing $false step.  The refutation still closes, so the
    # bug is precisely one unsound step, not a missing closure.  ex-falso is
    # avoided: the single axiom parent is consistent, so it genuinely does
    # not entail the bogus conclusion.
    axioms = [s for s in steps if s.rule is None and s.role == "axiom"]
    if not axioms:
        return None
    m = copy.deepcopy(steps)
    bogus = Step("bogus_step", "plain", "unrelated_pred(bogus_c)",
                 rule="deduction", status="thm", parents=[axioms[0].name])
    # insert before the last step (the $false closure)
    m.insert(len(m) - 1, bogus)
    return m

MUTS = [("neg", mut_neg), ("sk_absent", mut_sk_absent),
        ("sk_reuse", mut_sk_reuse), ("unentail", mut_unentail)]

# ===================================================================
def main():
    vdir, bdir = os.path.join(OUT, "valid"), os.path.join(OUT, "buggy")
    os.makedirs(vdir, exist_ok=True)
    os.makedirs(bdir, exist_ok=True)
    manifest = []
    nv = nb = 0
    for fname, fn in FAMILIES:
        steps, meta = fn()
        # valid
        vp = "%s_valid.p" % fname
        with open(os.path.join(vdir, vp), "w") as fh:
            fh.write("%% synth ProoVer proof (gold: valid) -- family %s\n" % fname)
            fh.write(render(steps))
        manifest.append((vp, "valid", "-"))
        nv += 1
        # buggy mutants
        for tag, mut in MUTS:
            m = mut(steps, meta)
            if m is None:
                continue
            bp = "%s_bug_%s.p" % (fname, tag)
            with open(os.path.join(bdir, bp), "w") as fh:
                fh.write("%% synth ProoVer proof (gold: buggy, %s) -- family %s\n" % (tag, fname))
                fh.write(render(m))
            manifest.append((bp, "buggy", tag))
            nb += 1
    with open(os.path.join(OUT, "manifest.tsv"), "w") as fh:
        fh.write("file\tgold\tbug_class\n")
        for row in manifest:
            fh.write("\t".join(row) + "\n")
    print("wrote %d valid + %d buggy = %d proofs to %s" % (nv, nb, nv + nb, OUT))

if __name__ == "__main__":
    main()
