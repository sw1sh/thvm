# IC-native rule dispatch: design memo (stage 8.3a)

> Sibling of `waldmeister_ic_atp.md`, `sup_encoded_cps.md`,
> `kbo_ic_design.md`.  Decision document for stage 8.3's port of
> rewriting from C-side pattern matching to IC-native dispatch
> via LAM-bound rules and APP-SUP fan-out.

## Goal

Stage 8.3 calls for **rules as LAM-binder closed-form terms** that
the IC reducer can fire directly.  Today the rewriter
(`src/rewrite/_.c`) walks each rule's LHS and tries one-way
matching against the input term in C; on match, it applies the
substitution to build the RHS.  The IC-native version puts the
rule itself in the heap as a LAM (or chain of LAMs, one per
pattern variable) and lets the standard APP-LAM interaction do
the binding work.  A SUP of such rules dispatches via APP-SUP
fan-out.

The motivation matches 8.1's: putting rule application in the
same evaluation model as the rest of the IC pipeline opens the
door to SupGen-style search (8.10) over alternative orientations
or rule selections.  It is also a satisfying validation of the
design memo's claim that an entire term-rewriting system can be
expressed as IC-native code.

## The FVR-vs-VAR translation problem

Our codebase has two notions of variable:

- **TAG_FVR** (free first-order variable; `term_new_fvr(id)`):
  an atom carrying a u32 `id` in its `ext` field.  Two FVRs are
  the same variable iff their ids match.  Used in
  `src/rewrite/_.c::thvm_match`, `src/unify/_.c::thvm_unify`,
  the saturation loop, every CP, etc.  This is "first-order
  logic" variable -- pattern matching uses ids to track
  consistency.

- **TAG_VAR** (binder slot; `term_new(0, TAG_VAR, 0, loc)`):
  an atom whose `val` points at a heap cell that is the LAM's
  binder location.  When the LAM fires (`interact_app_lam`),
  the heap cell gets `heap_subst_var(loc, arg)` -- the next
  read of the cell follows the SUB bit and resolves to `arg`.
  Used in interaction nets, IC programs, ICC.  This is "graph
  reduction" variable -- there's no name, just a cell.

Rule-as-LAM needs to translate FVR to VAR.  Two strategies:

### Strategy A: explicit alpha-conversion pass

Walk the rule's LHS and RHS, collect the set of FVR ids, allocate
one binder cell per id, replace each FVR with the corresponding
VAR.  Wrap the result in a chain of LAMs (one per binder).
Argument application (APP) substitutes one LAM at a time
(left-to-right).

**Pros**: clean translation; the resulting term is a proper IC
LAM that fires APP-LAM.

**Cons**: the rule no longer has an explicit pattern -- once
translated, you can't tell which subterms were originally
variables.  Pattern matching becomes "feed args one at a time,
trust the IC reducer to do the binding."  This works only if
the rule's LHS is a CTR with the variable args in known
positions; for nested patterns (e.g., `f(g(x), y) -> ...`),
the rule expects the outer CTR to be peeled before the inner
patterns are fed -- which the IC reducer doesn't naturally do.

### Strategy B: keep FVR; APP-CTR rule

Don't translate FVR; instead define a new interaction or
primitive that directly applies a rule (with FVR-shaped LHS)
to a term.  This is what `prim_unify_apply` (8.1c) already
does for unification; an analogous `prim_rewrite_step` would
take `(rule_lhs, rule_rhs, target)` and return either
`σ(rule_rhs)` (on match) or `ERA` (on miss).

**Pros**: no encoding contortions; reuses the existing
matching code via TAG_PRI; clean parity story.

**Cons**: not "rule as LAM" in the literal sense; the rule
stays as a CTR with FVR leaves.  But the dispatch via SUP +
APP-PRI fan-out is still IC-native.

### Strategy C: hybrid -- LAM on the outermost layer

Use LAM only for the variables that appear at the **top
position** of the LHS as direct CTR args -- these are the
variables that the IC reducer can naturally peel via APP-LAM.
For nested patterns, fall back to FVR + matching primitive.

**Pros**: gets the simple cases (rules like `f(x, e) -> x`)
fully into IC reduction; defers the harder cases to a
primitive.

**Cons**: encoding logic gets fiddly; the win over Strategy B
is unclear.

## Decision

**Implement Strategy B in 8.3b.**  Rule as `CTR` with FVR
leaves stays the storage form; `prim_rewrite_step(lhs, rhs,
target)` is the dispatch primitive.  This faithfully reproduces
the C-side `thvm_rewrite_step` semantics through APP-PRI.  It
sidesteps the FVR-to-VAR translation entirely.

The literal phrasing "rule as LAM-binder" in 8.3's task title
is reinterpreted as **"rule as a callable IC entity"** -- the
LAM-vs-PRI choice is a means to an end (IC dispatch), not an
end in itself.

**8.3c then layers SUP fan-out**: pre-encoded rule set as
`&L{rule_0_pri, rule_1_pri, ...}` where each `rule_k_pri` is
a partial PRI of `prim_rewrite_step` with `(lhs_k, rhs_k)`
already supplied.  `APP(SUP, target)` fan-outs target across
all rules; APP-SUP commutation distributes; each branch fires
APP-PRI to either rewrite or ERA.

Note: this still hits the DUP-CTR problem from 8.1d-i (CTR
args don't fan out cleanly via APP-SUP).  Mitigations carry
over from 8.1d-ii: pre-build the saturated PRI calls, store
them in the SUP, force each child via `wnf` to read the result.

**Strategy A defers to 8.3d / future research.** A real "rule as
LAM-binder" port has to grapple with nested-pattern destructuring
that requires either guards or matching primitives anyway.
The benefit is small relative to Strategy B until SupGen-style
search demands superposition over the rule structure itself.

## Architectural fit

Following 8.1's pattern:

- New primitive `prim_rewrite_step` (arity 3) registered at
  `ATP_PRIM_REWRITE_STEP = 4`.  Body: try `thvm_match(lhs,
  target, &subst)`; on success return `thvm_subst_apply(rhs,
  &subst)`; on failure return ERA.

- New helper `ic_rewrite_step(lhs, rhs, target)` in
  `src/atp/_.c` that builds the APP chain
  `APP(APP(APP(PRI(4), lhs), rhs), target)` and reduces via
  `wnf`.  Mirrors `ic_unify_apply3`.

- 8.3c demo: `&L{ APP(APP(PRI(4), lhs_0), rhs_0),
                 APP(APP(PRI(4), lhs_1), rhs_1) }` partial-PRIs;
  apply via `APP(sup, target)` -> APP-SUP fan-out -> per-branch
  saturated PRI fires.

- 8.3e feature flag: `use_ic_rewrite` on AtpState; when set,
  `thvm_rewrite_step` dispatches through PRI-routed evaluation.

## ICC TAG_BRI / TAG_ANN integration (8.3d)

ICC's type-flow rules (BRI = bridge, ANN = annotation) let us
attach a sort signature to a rule and have the wrong-sort
branches collapse to ERA before APP-LAM (or APP-PRI) fires.
This is genuinely useful **only when sorts arrive in 8.4**:
without sorts, every rule applies to any term and the BRI/ANN
wrapping is just ceremony.

Decision: defer 8.3d until 8.4 lands multi-sort signatures.
At that point, an ANN-wrapped rule can short-circuit on sort
mismatch, and the integration becomes meaningful.

## Verification (for stages 8.3b-e)

- **8.3b**: `tests/test_rewrite_pri.c` -- 4-6 cases.  Direct
  match (e.g., `(f(x, e), x)` applied to `f(a, e)` -> `a`),
  no-match returns ERA, FVR-only LHS, nested CTR.
- **8.3c**: `tests/test_sup_rewrite.c` -- 3-5 cases.  Build a
  SUP of 2-3 rules, apply to a target term, verify the result
  set matches what the C-side `thvm_rewrite_step` would
  produce (modulo SUP wrapping).
- **8.3e**: `tests/test_atp.c` parity case under both flag
  values.  Bench harness (8.1e-iii style) optional but useful.

## Stop conditions

If 8.3b reveals that the existing `thvm_match` is too
intertwined with the saturation loop's RewriteSubst stack to
factor out cleanly, fall back to a simpler primitive that
takes `(lhs_ctr_id, target)` and uses head-symbol matching
only -- enough for a research-grade demo.

If 8.3c parity tests reveal CP-relevant edge cases the
matching primitive misses (variable freshness, nested patterns,
etc.), document them and consider whether 8.3d's BRI/ANN
integration would help.

If 8.3e benchmarks show >2x slowdown, treat 8.3 as research
infrastructure (like 8.1e's IC path is opt-in for SupGen) and
keep the C path as default.
