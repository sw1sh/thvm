# Changelog

Human-readable log of meaningful changes. Newest first. Group entries
under `## Unreleased` until we cut a tagged version, then roll into a
dated section.

## Unreleased

### Added: thvm_atp_orient_and_add (stage 5.2b)

KBO-orient an equation and push the resulting rule(s) onto `R`.
Returns `AtpAddedRange { first, count }` so the next saturation
phase (5.2d generate-CPs) can target only the new rules.

- `KBO_GT`: push `lhs -> rhs`                                 (count = 1)
- `KBO_LT`: push the swap `rhs -> lhs`                         (count = 1)
- `KBO_UN`: unfailing fallback -- push both orientations,
  atomic on capacity (skip both if there's room for only one)   (count = 2)
- `KBO_EQ` or `R` full: no-op                                  (count = 0)

The unfailing variant of Knuth-Bendix completion (Bachmair-
Dershowitz-Plaisted) keeps unorientable equations as 2-way rules
so rewriting can try either direction.  Mirrors Waldmeister's
behavior; see `waldmeister/sources/INF/Hauptkomponenten.c`
(*Hauptkomponenten* = "main components").

### Added: thvm_atp_select_cp FIFO pop (stage 5.2a)

`thvm_atp_select_cp(s, &lhs_out, &rhs_out)` lands in
`src/atp/_.c` -- pops the front CP, shifts the tail down to
keep the array dense.  Returns 1 on success / 0 on empty.
Stage 5.3 will replace the FIFO with priority-collapse over
INC-wrapped CPs (the `--add` heuristic from
`waldmeister/sources/CLAS/ClasHeuristics.c`).  Tests cover
empty queue, FIFO order across three pushes, and tail
densification after a pop.

### Added: AtpState struct + init/free helpers (stage 5.1)

`src/atp/_.c` lands the saturation-loop state container plus
`thvm_atp_init` (heap-allocates, stores cfg + step_cap),
`thvm_atp_free` (NULL-safe), `thvm_atp_add_equation` (push CP),
`thvm_atp_set_goal`.  Public types in `src/thvm.h`: AtpStatus
enum, ATP_MAX_RULES (256), ATP_MAX_CPS (4096), AtpState struct
(rules R, CP queue, goal, KboConfig, step counter).  Tests in
`tests/test_atp.c` cover init/free symmetry, NULL-free safety,
queue-full rejection, goal set/clear.  Step + run drivers are
5.2.

### Changed: f1d helper accepts REDUCE-as-tail-op (CPU)

`materialize_kernel_inlined` (CPU-only via the existing backend
gate) now accepts `root_op == UOP_REDUCE` when the source is a
fully-inlinable elementwise chain.  Tinygrad's "local reduction"
pattern: one kernel runs N-1 elementwise ops into a register and
the final REDUCE writes the output buffer.

Linear-train forward+loss with toggle ON: 16 -> 8 kernels.  Both
the Softmax-normalization REDUCE_SUM(EXP(x)) and the CE-loss
REDUCE_SUM(MUL(target, LOG(p))) chains now collapse into one
kernel each instead of one kernel per UOp.

### Added: saturation-loop design sketch (stage 5.0)

[docs/plans/saturation_loop.md](docs/plans/saturation_loop.md)
designs the AtpState struct, the 10-step saturation algorithm,
fairness mitigations (step_cap + round-robin escape), termination
conditions, and the mapping from existing C-side primitives
(`thvm_match`, `thvm_unify`, `thvm_critical_pairs`,
`thvm_rewrite_normalize`, `thvm_kbo`, `thvm_collapse_ordered`)
into the loop body.  The implementation lands in 5.1-5.4; demo
(prove `f(a, i(a)) = e` from group axioms) is 5.5.

### Verified: ATP arc baseline green (stage 0 sanity)

`make test` (48 C executables, 166 sub-checks) and `make wl-test`
(295 WL VerificationTests) both green at HEAD `f49f267`.  First
firing of cron `757c483c` driving
[docs/plans/waldmeister_ic_atp_tasks.md](docs/plans/waldmeister_ic_atp_tasks.md)
through stages 5-8+.

### Added: ICC type-flow primitives (TAG_BRI + TAG_ANN, real ICC rules)

`TAG_BRI = 23` (Bridge / Val: θx.body) and `TAG_ANN = 24`
(Annotation: {val : typ}) land with the actual ICC reduction rules
from `TinyHVM/resources/gists/icc_spec.md`, not the LAM-alias that
TinyHVM shipped with:

  APP (θx.body) arg = θx (APP body[x ← λ$k.x]  (ANN $k arg))
  ANN val (λx.body) = λx (ANN (APP val $k) body[x ← θ$k.x])
  ANN val (θx.body) = body[x ← val]                          (type erasure)

Plus DUP-BRI commutation (mirror of DUP-LAM) so bridges duplicate
correctly under SUP search.

Files: `src/term/new_bri.c`, `src/term/new_ann.c`,
`src/interact/{app_bri,ann_lam,ann_bri,dup_bri}.c`.  TAG_ANN
reduction is inline in `src/wnf/_.c` (mirrors TAG_OP2's strict-
on-typ-then-dispatch pattern).

Tests (`tests/test_icc.c`, 11 sub-checks):
- ANN-BRI type erasure on θx.x consumes the bridge, leaves the val
- ANN-BRI on a bridge whose body is its own bound y returns y[y ← val]
- APP-BRI on θx.x with NUM(7) fires and the head is again BRI
  (the inner structure changes; ICC is type-flow, not value-flow)
- ANN-LAM fires and the head wraps in a new λ
- DUP-BRI commutes !&7{F0,F1} = θx.x into two bridges
- ANN with a non-LAM/BRI typ stays stuck

These are the ICC primitives the IC-native ATP plan can fall back
on for closed-form encodings of equations + dependent-type proofs.
FVR-based open-form remains the active path for stages 2-4 of the
plan; BRI/ANN are now ready when stages 5-7 motivate them.

### Added: stage 4 -- unification + critical-pair enumeration

`src/unify/_.c` lands the Robinson MGU on TAG_CTR + TAG_FVR with
the standard occurs check.  Result lives in the same RewriteSubst
struct used by stage 3's matcher; `unify_walk` follows FVR -> FVR
chains, and `thvm_unify_apply` realizes a chained substitution
into a fully-instantiated term.  `thvm_rename_vars(t, offset)`
shifts every FVR id by `offset` so two rules can be unified
without variable-name collisions.

`src/cp/_.c` enumerates critical pairs.  Walks every non-variable
position of `rule_i.lhs`, tries unifying with `rule_j.lhs`
(renamed apart by `REWRITE_MAX_VAR/2`), and emits
`(σ(l_i[p ← r_j]), σ(r_i))` on success.

Demo (`tests/test_cp.c`): rule set `{ f(e, x) = x ; f(f(x, y), z)
= f(x, f(y, z)) }` produces the expected CP `(f(y, z), f(e, f(y,
z)))` from the [0]-overlap of left-id into assoc.

C-side only for now; SUP-encoded CP enumeration via a `TAG_PRI`
unify primitive (stage 4.5) is optional and deferred.

### Added: TMatStatsLabel for per-realize THVM_MAT_STATS attribution

`TMatStatsLabel["fwd_conv1"]` tags the next `thvm_realize` call's
`THVM_MAT_STATS=<path>` log line with the given string; the buffer
clears after one realize.  Bridges:
`thvm_wl_mat_stats_label(UTF8String)` in `wl/THVMLink/CSource/thvmlink.c`,
backed by a 64-byte `MAT_STATS_LABEL` global in `materialize_memo.c`.

Lets probes attribute kernel counts to specific layers / grad
chains.  Sample LeNet breakdown: forward+loss=231 kernels;
grad_b1..b3=20 each; grad_w4=40; grad_b4=36 -- forward Conv2D-
lowered chain dominates and is the next fusion target.

### Changed: lenet bench + verify use TGradMany; materialize descends into TAG_CTR

`lenetStep` (baseline.wls) and `stepGrads` (verify.wls) now build a
single multi-target `UOP_GRAD` via `TGradMany[loss, weights]`.
`materialize_expr` gained a `TAG_CTR` case that recursively
materializes each child within the same realize, so all n backward
kernels emit in ONE materialize pass with shared memo.

Bench result is NEGATIVE for the kernel-count metric (427 -> 426
on lenet) and 0% for peak.  Cause: each per-target chain rule
allocates fresh cotangent UOp cells with new heap locs, so the
memo can only dedup the forward leaf references.  Detail +
follow-up options in `docs/bench-results.md` "k0e" section.

### Added: equational rewriter -- stage 3 (one-shot, top-position only)

`src/rewrite/_.c` exports a small C-side equational rewriter for
TAG_CTR + TAG_FVR terms:

- `thvm_match(pattern, term, subst)` -- one-way matching with
  linearity check (a variable seen multiple times must bind to
  the same sub-term, verified via `kbo_eq`).
- `thvm_subst_apply(t, subst)` -- substitution-with-rebuild: TAG_FVR
  becomes its bound sub-term; TAG_CTR is rebuilt with substituted
  children; everything else passes through.
- `thvm_rewrite_step(t, lhs, rhs, n_rules)` -- try each rule in
  order at the *top* position; first match wins, RHS returned with
  substitution applied.
- `thvm_rewrite_normalize(t, lhs, rhs, n_rules, step_cap)` -- iterate
  rewrite_step to a fixpoint or until step_cap exhausts.

Recursive descent into sub-terms is not yet wired -- that's part of
the saturation loop in stage 5.

The headline demo from `docs/plans/waldmeister_ic_atp.md` sec.5
runs in `tests/test_rewrite.c`: under the full group axioms

  f(x, e)        = x
  f(x, i(x))     = e
  f(f(x,y), z)   = f(x, f(y, z))

`f(a, e)` normalizes to `a` (one rewrite_step fires; the second
step is a fixpoint).  Plus 8 supporting cases for matching,
non-linear consistency, substitution, no-applicable-rule, and the
inverse rule firing.

### Added: TAG_FVR + thvm_kbo -- stage 2 (term encoding + KBO ordering)

`TAG_FVR = 22` is an atomic first-order variable: `EXT = var_id`,
no heap cells.  Distinct from `TAG_VAR` (the IC's bound variable
tied to a binder).  Used by the IC-as-ATP layer to encode the
universally / existentially quantified variables of equational
logic.

`thvm_kbo(s, t, cfg)` (`src/kbo/_.c`) implements the Knuth-Bendix
ordering on TAG_CTR + TAG_FVR terms.  KboConfig holds per-symbol
weights, total precedence, and the scalar variable weight w0.
Returns KBO_EQ / KBO_GT / KBO_LT / KBO_UN.  Algorithm: Baader-
Nipkow (variable-domination check, weight comparison, top-symbol
precedence tiebreak, lexicographic on args).

The headline demo from `docs/plans/waldmeister_ic_atp.md` sec.5
runs in `tests/test_kbo.c`: under Waldmeister's default group-
axiom KBO (weights `i=0, f=1, e=1, a=1`; precedence `i > f > e > a`;
`w0 = 1`), `f(x, e) > x` orients correctly.

C-side only for now -- the IC-as-pure-program port (stage 2.4) is
optional and deferred.

### Added: TGradMany WL bridge

`TGradMany[y, {x_1, ..., x_n}]` in `wl/THVMLink/Kernel/Tensor.wl`
builds a single `UOP_GRAD` and realizes once; the resulting
`TAG_CTR` of n cotangents is unpacked into a List of TTerm
wrappers via `thvm_wl_term_ctr_at`.  3 new tests in `grad.wlt`
assert equality with the per-target `TGrad` results.

### Added: multi-target chain rule for UOP_GRAD

`interact_grad` now handles `n>1` by lowering to a `TAG_CTR` of `n`
unary `uop_grad(y, gy, x_i)` terms.  Each unary grad walks the
chain rule independently; the forward DAG (y and its descendants)
lives at shared heap locs so materialize's per-realize memo dedups
every kernel emitted from those forward UOps across all `n`
targets.  `n=1` keeps the scalar return for backward compat.

### Changed: UOP_GRAD heap layout is now multi-target (k0b)

`uop_grad` heap is now `[y, gy, NUM(n), x_1, ..., x_n]` (was
`[y, gy, target]`).  New `uop_grad_multi(y, gy, targets, n)` is
the primary constructor; the legacy unary `uop_grad(y, gy, x)`
is a thin wrapper with `n=1`.  `interact_grad` bails on `n>1`
for now -- the multi-target chain rule lands in k0c.

The change cascades through every site that knows GRAD's heap
arity: `wnf/redex.c` `term_arity` reads `NUM(n)` to compute
`3+n`; `alo/realize.c` `alo_node_arity` and
`book/from_dynamic.c` `dyn_arity` take a `val` argument so they
can `book_read` / `heap_read` the count when cloning UOP_GRAD
templates (TOptim's recursive lambdas embed it).  Accessors
`uop_grad_n` / `uop_grad_target` provide read-side parity.

### Added: TAG_WHEN boolean filter -- closes the stage-1 e2e demo

`TAG_WHEN = 21` is the IC-side primitive for "collapse to the
matching one":

  WHEN(NUM(0), _)        -> ERA               (failed branch erases)
  WHEN(NUM(n != 0), b)   -> wnf(b)
  WHEN(ERA, _)           -> ERA
  WHEN(&L{c0,c1}, b)     -> &L{WHEN(c0, B0), WHEN(c1, B1)}, !&L{B0,B1}=b

The end-to-end demo from `docs/plans/waldmeister_ic_atp.md` now
runs in one IC reduction + one collapse:

```
cands = &L{NUM(2), NUM(3)}
t     = WHEN(EQL(cands, NUM(3)), cands_dup)
collapse(t) -> [NUM(3)]    -- only the matching candidate
```

Failed candidates collapse to ERA via WHEN-NUM-zero, and
`thvm_collapse` drops ERA branches.  This is stage 1.7 revised:
constructors+MAT deferred to stage 2 (term encoding) where they
are motivated by encoding equations.

Constructor: `term_new_when(cond, body)`.  Tests:
`tests/test_when.c` covers all rules + the e2e demo.

### Added: TAG_INC priority wrapper + thvm_collapse_ordered

`TAG_INC = 19` is a one-cell priority wrapper.  The reducer treats
it as a WNF atom (default fall-through; no interactions), so the
INC layer survives reduction and becomes visible to collapse.

`thvm_collapse_ordered(t, out, cap)` performs the same shallow
SUP-tree walk as `thvm_collapse`, but counts INC wrappers along
the path to each leaf and emits the leaves sorted by INC-depth
ascending (ties broken by DFS order).  Lower INC count = higher
priority = enumerated first.  Implementation collects (Term, pri,
idx) into a heap-allocated buffer, qsorts, writes Terms back.

This is the IC encoding of Waldmeister's `--mix` CP-selection
heuristic: wrap each candidate with INC^k where k is its weighted
cost, and `thvm_collapse_ordered` enumerates cheapest first.

Constructor: `term_new_inc(body)`.  Tests: `tests/test_inc.c`.

### Added: TAG_ANY wildcard

`TAG_ANY = 18` is an atomic wildcard.  Two interactions:

  EQL(ANY, x) -> NUM(1)        (matches anything, on either port)
  ! &L{x0,x1} = ANY  ->  x0 <- ANY, x1 <- ANY

Constructor: `term_new_any()`.  Used as the IC encoding of
existential / Skolem variables in the ATP plan.  Tests:
[tests/test_any.c](tests/test_any.c).

### Added: TAG_AND, TAG_OR with short-circuit + SUP commutation

`TAG_AND = 16` and `TAG_OR = 17` land as short-circuit boolean
nodes; both are strict on the left operand and lazy on the right:

  AND(NUM(0), _)        -> NUM(0)        (right stays unreduced)
  AND(NUM(n != 0), b)   -> wnf(b)
  AND(ERA, _)           -> ERA
  AND(&L{a0,a1}, b)     -> &L{AND(a0,B0), AND(a1,B1)}, !&L{B0,B1}=b

  OR(NUM(0), b)         -> wnf(b)
  OR(NUM(n != 0), _)    -> NUM(1)        (right stays unreduced)
  OR(ERA, _)            -> ERA
  OR(&L{a0,a1}, b)      -> &L{OR(a0,B0), OR(a1,B1)}, !&L{B0,B1}=b

The SUP commutation routes a superposed left operand through both
branches with the right operand DUPed, mirroring EQL-SUP.  This
enables the SupGen-style filter pattern `AND(EQL(cand, expected),
cand)`: the matching candidate survives, the rest become NUM(0).
Full ERA-propagating filter (collapse to *only* the matching
candidate) needs the MAT/constructor work in stage 1.7.

Constructors: `term_new_and(a, b)`, `term_new_or(a, b)` in
`src/term/`.  Tests: `tests/test_and_or.c`.

### Added: EQL-SUP commutation + DUP-NUM annihilation

The `EQL` reducer now commutes through `SUP` on either port:

  EQL(&L{a0,a1}, b)  ->  &L{EQL(a0, B0), EQL(a1, B1)}, !&L{B0,B1}=b
  EQL(a, &L{b0,b1})  ->  &L{EQL(A0, b0), EQL(A1, b1)}, !&L{A0,A1}=a

The DUPed b (resp. a) propagates correctly because `DUP-NUM`
annihilates atomically, copying the Term value into both
projections.  New file `src/interact/dup_num.c`.

End-to-end: `EQL(&L{NUM(2), NUM(3)}, NUM(3))` now reduces to
`&L{NUM(0), NUM(1)}`, and `thvm_collapse` enumerates `[NUM(0),
NUM(1)]` -- the SupGen-style search-as-superposition pattern is
working for the first time on thvm.

### Added: TAG_EQL (structural equality) -- minimal cut

`TAG_EQL = 15` lands as a strict equality node with heap layout
`[a, b]`.  The wnf reducer walks both ports to WNF and dispatches:

- `EQL(NUM(x), NUM(y))` -> `NUM(1)` if x==y else `NUM(0)`
- `EQL(ERA, _)` / `EQL(_, ERA)` -> `ERA` (failed branches collapse out)
- otherwise stuck

SUP commutation (the rule that pushes a SUP at either port up to
the head) lands separately in stage 1.3b alongside DUP-NUM.

Constructor: `term_new_eql(a, b)` ([src/term/new_eql.c](src/term/new_eql.c)).
Tests: [tests/test_eql.c](tests/test_eql.c).

### Added: glossary section on equational reasoning and the IC-as-ATP layer

[docs/glossary.md](docs/glossary.md) gains an *Equational reasoning
and the IC-as-ATP layer* table, explicitly distinguishing **HVM-SUP**
(the runtime data primitive `&L{a, b}`) from **ATP-superposition**
(the logical inference rule, refined paramodulation), plus
companion entries: collapse, label, substitution, **cosubstitution
and bisubstitution** (Wolfram's framing -- bisubstitution = paramodulation),
unification, matching, paramodulation, critical pair, Knuth-Bendix
completion, unfailing completion, reduction ordering, joinability,
saturation, subsumption, PCL.  The plan memo
[docs/plans/waldmeister_ic_atp.md](docs/plans/waldmeister_ic_atp.md)
gets a terminology warning at the top cross-referencing the new
section, and [docs/README.md](docs/README.md) lists the plan in its
plans-and-references index.

### Added: thvm_collapse -- shallow SUP-tree enumeration

`src/collapse/_.c` exposes `thvm_collapse(t, out, cap)` which walks
the head of `t` via WNF and recurses on TAG_SUP, dropping TAG_ERA
branches.  Caller-supplied buffer + cap; returns count.  This is
the "shallow" version: deeper enumeration through APP / OP2 / EQL /
... lands as those tags get SUP-commutation interactions.
Tests in `tests/test_collapse.c` cover single-leaf, single-SUP,
nested SUP, ERA-pruned branch, and cap-truncation.

### Added: docs/plans/waldmeister_ic_atp.md -- IC-native ATP design memo

Research-and-design memo summarizing Waldmeister's unfailing
Knuth-Bendix completion algorithm, surveying prior art on
interaction-net + ATP work (April 2026), and sketching how the same
proof procedure could be expressed as IC graph rewrites in thvm
using SupGen / NeoGen-style superposition over rule sets and
overlap spaces.  Includes a 7-stage build trajectory.

### Added: DUP-SUP cross-label commutation

`interact_dup_sup` now handles the commuting case
`!&L{x0,x1} = &R{a,b}` (L != R) by allocating a 6-cell block of
two new dup bodies (for `a` and `b`) plus four DP0/DP1 leaves, and
returning two fresh `&R`-labeled SUPs.  Previously the cross-label
case was stuck.  This unblocks any future tag whose interactions
need SUPs to flow through DUPs (EQL, AND/OR, MAT, INC, ...).
Tests in `tests/test_dup_sup.c` exercise head shape, inner
structure, and both-projection consistency.

### Changed: lazy GRAD + lazy materialize via shared term_resolve

`interact_grad` and `materialize_expr` no longer call `wnf` to
expose their inputs.  Both now route through a new shared
`term_resolve` (in `src/term/resolve.c`) that does the minimum
work needed to surface the outermost layer:

- TAG_VAR: take the SUB-bit cell (single-step deref); chase the
  chain if it cascades.
- TAG_ALO: force one realisation layer via `alo_force` (which is
  itself memoised, so repeated walks are cheap).
- everything else: return unchanged.

That's the entire resolver -- it does NOT fire materialize, kernel,
or grad reductions.  Anything `interact_grad` can't structurally
pattern-match (e.g., a free VAR that hasn't been bound yet) is
returned unchanged; `wnf`'s UOP_GRAD case got a fixed-point check
so the term sits as WHNF rather than re-fires.  `materialize_expr`
follows the same pattern, with a single `wnf` step retained for
the LAM/APP/REF case (where actual beta / unfolding is required
before any UOp shape is visible).

The SGD demo in `wl/THVMLink/Tests/sgd.wlt` was rewritten to drop
the per-iteration `TUOpMaterialize` wrapper from the loop body.
The recursive call now passes the symbolic `step(w)` UOp graph as
the new w; `TRealize` at the end fires one materialize over the
deeply-nested expression.  That's both cleaner and side-steps the
"shared TUOpMaterialize wrapper produces distinct fresh TENs per
fire so grad's leaf check breaks" issue we papered over with the
materialize cache: now every reference to `w` in the body is the
same UOp Term value, and the leaf check just works.

### Added: phase 3 -- SGD optimizer as a recursive lambda term

Tying phases 1 + 2 together to demonstrate the original use case
the user laid out: a lambda term that takes a "net with loss at
root" plus a parameter and adds GRAD nodes inside a recursive
training loop expressed as `TDef`/`TRef`.

Three runtime fixes were needed to make `materialize(step(w))`
compose multiple times without breaking grad's leaf check:

1. **Lazy GRAD chain rule.**  `interact_grad` used to recursively
   compute the entire chain rule expansion in one fire (eager).
   Now each fire does a single structural step on `y`'s outermost
   UOp, deferring sub-positions as fresh `UOP_GRAD` nodes that
   wnf re-enters on demand.  `wnf` is called ONCE on `y` and
   `target` to expose the outermost layer (so a `GRAD[APP(loss_fn,
   w), w]` body can beta-reduce before pattern-matching).  Existing
   numerics preserved (9/9 grad + 17/17 nn end-to-end tests still
   pass); test_grad.c structural assertions updated to expect the
   one-layer form.

2. **Materialize follows VAR substitutions.**  After APP-LAM beta,
   a UOP body's cells hold VARs pointing at the binder's
   substituted heap slot.  `materialize_expr` now wnfs each input
   first so VAR (and ALO and the active-path UOPs the wnf reducer
   knows about) resolve to a concrete TEN/UOP_KERNEL.

3. **Materialize result memoization.**  The same `MATERIALIZE`
   wrapper inside a single graph is often referenced from N
   slots (e.g. a recursive `step(w)` body uses `w` in both the
   loss and the weight update).  Each fire used to allocate a
   fresh kernel + TenDesc, so the resulting TAG_TEN ids differed
   per use; `interact_grad`'s `y == target` pointer-equality
   leaf check then failed and the gradient collapsed to zero.
   `thvm_materialize` now caches the realised result back into the
   wrapper's heap cell so subsequent fires return the SAME
   TAG_TEN id.

4. **ALO_force memoization.**  The companion fix for #3.  Each
   ALO fire used to re-realize from the book template, allocating
   fresh dyn cells.  In a recursive REF body that references the
   bound `w` multiple times, the multiple references then mapped
   to distinct fresh wrappers and #3 didn't help.  `alo_force`
   now writes the realised term back into the ALO cell and marks
   the second slot non-NUM as a "cached" sentinel.  Subsequent
   fires hit the cache.

WL example, in `wl/THVMLink/Tests/sgd.wlt`:

```
sgd_loop = TLam[w |->
  TLam[n |->
    TIfZero[n, w,
      TApp[
        TApp[TRef["sgd_loop"],
             TUOpMaterialize[
                w + (-lr) * grad(L2(w - target), w)]],
        TOp2["-", n, TNum[1]]
      ]
    ]
  ]
]
TDef["sgd_loop", sgd_loop]
TWnf @ TApp[TApp[TRef["sgd_loop"], w0], TNum[2]]
  -> {0.36, 0.72, 1.08}    (* w_2 = 0.8 w_1 + 0.2 target *)
```

4/4 SGD cases pass (one-step lambda + 0/1/2 recursive iters).
Compute scales steeply (kernels ~3-4x per iteration without DUP
sharing for tensors); training-scale runs need that next.

### Added: phase 2 -- MAT (numeric switch) + OP2 (binary ops on NUMs)

Two more term tags so a recursive REF body can hit a base case and
manipulate its iteration counter (precondition for the SGD-as-lambda
optimizer in phase 3):

- `TAG_OP2` (val = heap loc -> [x, y], ext = OP_*) -- strict on x
  then y; both must reduce to TAG_NUM for the op to fire.  Opcodes
  `OP_ADD` / `OP_SUB` / `OP_MUL` / `OP_EQ` / `OP_LT`.  Stuck if
  either operand stays non-NUM.
- `TAG_MAT` (val = heap loc -> [handler, fallback], ext = match)
  -- numeric-switch atom.  In wnf's `apply` phase, when an APP frame
  pops with a MAT head, the arg is forced via a recursive `wnf()`
  call: NUM matching `ext` reduces to `handler`, otherwise the
  result is `APP(fallback, arg)`.  Mirrors HVM4's APP-MAT-NUM.

`book/from_dynamic.c` and `alo/realize.c` learned the two new
fixed-arity-2 nodes so REF unfolding handles them.  `wnf/_.c` got
`case TAG_OP2` in enter and a `case TAG_MAT` branch inside the APP
apply switch.

WL surface in `wl/THVMLink/Kernel/Switch.wl`:
- `TNum[i]` / `TNum[i, dtype]` -- a TAG_NUM atom (defaults to i32).
- `TOp2["+"|"-"|"*"|"=="|"<", x, y]` -- a TAG_OP2 term.
- `TMatNum[matchVal, handler, fallback]` -- a TAG_MAT atom.
- `TIfZero[counter, then, else]` -- sugar that wraps the else branch
  in a discarding lambda so MAT's miss-path looks like a plain
  conditional.

Tests:
- `tests/test_mat_op2.c` (9 cases) -- OP2 arithmetic + MAT match /
  miss + an end-to-end **recursive countdown** built from
  REF + ALO + LAM/APP + MAT + OP2 (`@count 0 5 -> NUM(5)`).
- `wl/THVMLink/Tests/switch.wlt` (9 cases) -- the WL surface plus
  recursive countdown + sumto via `TDef`/`TRef`.  All pass.

All 331 C cases + 39 WL cases pass.

### Added: phase 1 of REF / ALO -- lazy named definitions

Two new term tags layered on top of the IC + UOP graph so users can
register named definitions and unfold them lazily during reduction
(precondition for the recursive SGD-as-lambda optimizer described in
PLAN.md):

- `TAG_REF` (val = name slot) -- a one-cell pointer into a fresh
  `DEFS[]` table holding the registered definition's *static
  template*.  Reducing a REF wraps the template in an empty-state
  ALO and re-enters; the body itself isn't expanded.
- `TAG_ALO` (val = dyn loc -> [book_term, NUM(state_id)]) -- the
  HVM4-style allocator.  Each fire walks one layer of the static
  template into a fresh dynamic heap region, threading an
  `AloState` chain that rebinds binders (LAM -> VAR) through the
  new dyn locs so multiple unfoldings of the same def don't alias
  each other's bound variables.

New runtime infrastructure:
- `BOOK_HEAP[]` (256K cells, parallel to `HEAP`) -- immutable
  per-def template cells.
- `DEFS[256]` -- root book term per registered name.
- `ALO_STATES[]` -- linked substitution chain for ALO descents.
- `book/{alloc,read,set,from_dynamic}.c` -- allocator + the
  recursive snapshot that lifts a dynamic term tree into the book
  heap (handles LAM / APP / VAR / fixed-arity UOP families; SUP /
  DUP / variable-arity movement ops are a follow-up).
- `alo/{state,realize,force}.c` -- the substitution chain plus
  `alo_realize` (one book-layer -> dyn) and `alo_force` (force a
  TAG_ALO term into its dyn shape).
- `term/{new_ref,new_alo}.c` -- term constructors.

`wnf/_.c` gained `case TAG_REF` / `case TAG_ALO` cases that fire
the unfolding; both bump `ITRS`.

WL surface in `wl/THVMLink/Kernel/Ref.wl`:
- `TDef[name, body]` -- snapshots `body` into the book heap and
  registers it under an integer slot (`name` may be a string -- it
  gets interned to a stable slot via `$defNames`).
- `TRef[name]` -- returns a TTerm wrapping a TAG_REF cell.
- `TDefName[name]` -- expose the slot mapping for tests.

Tests: `tests/test_ref.c` (5 cases) covers identity-via-REF + fresh
allocation per call + lazy self-reference; `wl/THVMLink/Tests/ref.wlt`
(4 cases) covers the WL surface end-to-end.  All 322 C cases + 30+
WL cases still pass.

Known scope: REF unfolds forever for self-referential defs without a
termination construct.  Phase 2 adds `MAT` (pattern match / numeric
switch) + `OP2` (SUB on NUMs for counter decrement) so a recursive
`train_step` lambda can hit a base case at iteration 0.

### Added: NN training-step numerics + per-render TimeConstrained budget

`nn.wlt` grew five training-flavoured cases on top of the layer
helpers:
- two-head square loss `(w.x + v.x)^2`, gradient sums across both
  paths to the same target;
- MSE through a dot product checked w.r.t. both `w` and `x`;
- one SGD step on `(w.x - t)^2` confirms the gradient direction
  reduces the loss;
- three-step gradient descent verifies loss is monotonically
  non-increasing;
- polynomial-regression-ish `(a x^2 + b x - t)^2` checks both
  partials.

`wl/Examples/run.wls` wraps each render in `TimeConstrained` (30 s
budget per heap-graph / IC-diagram render).  Dense tensor graphs
(NN-style compositions) sometimes blow the IC layout up by 100x and
hung the whole batch; now the over-budget render is skipped and
logged with `[skip] ... (over 30s)` so the rest of the examples
keep going.

### Added: NN.wl -- Wolfram NeuralNetworks layer -> TUOp graph converter

`wl/THVMLink/Kernel/NN.wl` lets users build the UOp graph by feeding
in built-in layers (`LinearLayer`, `ElementwiseLayer`, `NetChain`,
...) instead of inventing parallel layer constructors.  Tinygrad's
"Tensor + thin layer wrappers" model: a layer is a snapshot of
weights, the converter lifts them to TTensors and emits the same
TUOp* combinators users would write by hand.

Public surface (all in the THVMLink` context):
- `TFromNet[net, x]` / `TFromLayer[layer, x]` -- entry points,
  dispatch on `Head[layer]`.
- `TLayerWeights[layer]` / `TLayerToTensors[layer]` -- read a
  layer's NumericArrays / wrap them as TTensors.
- Tensor-method helpers: `TSum`, `TSquare`, `TDot`, `TMatVec`,
  `TL2Loss`, `TMSELoss`.

Currently supported layers:
- `LinearLayer[out, "Input" -> in]` -- forward via TMatVec
  (W @ x + b through EXPAND-broadcast + REDUCE_SUM).  Backward
  through W is a TODO until interact_grad gains an EXPAND rule.
- `ElementwiseLayer[#*# &]` -- maps to TSquare.  Adding more
  functions is a one-line entry in `$elementwiseDispatch`.
- `NetChain[{...}]` -- folds layers in declaration order.

Stubbed / out-of-scope:
- `ConvolutionLayer` -- raises a Message; needs movement-op
  support in materialize/interpret + the matching grad rules
  (step 14).

`wl/THVMLink/Tests/nn.wlt` covers the helpers (12 cases): forward
of LinearLayer / ElementwiseLayer / NetChain, plus end-to-end
gradient chains (TDot, TL2Loss, TMSELoss, polynomial, square of
dot product) -- all 12 pass.

### Fixed: shared-wire spiders for non-CONST UOP multi-reference

`wireFor` used to give every cell its own `w<loc>` wire name, so
when a non-CONST UOP fed N consumer slots only the principal cell
matched up; the other N-1 consumers dangled (visible in
`MUL[x, x]` where x is itself a UOP -- the second src wire had a
fresh name and stayed unconnected).

Fix: TAG_UOP cells (excluding CONST, which we render per-reference
as leaves) now key the wire on the producer's base
(`uop<val>` instead of `w<loc>`), so all consumer slots and the
producer's output share one wire.  DC then draws a spider where
the producer fans out to all the consumers -- same idiom we
already use for VAR / DP0 / DP1.

`plainUopDiagram` and `gradDiagram` updated their synthetic-fallback
pWire (used when the seed is heapless) to match the new naming
(`uop<base>` instead of `p<base>`).

### Fixed: grad chain rule allocates fresh EXPAND per branch + single-line node headers

`grad_rec` previously lifted `gy` to target.shape ONCE in
`interact_grad`, which was correct numerically but produced a heap
where multiple chain-rule consumers all referenced the same EXPAND
node.  In any visualisation that doesn't fan out via DUP, all but
one of those consumer wires dangled (visible in `grad-x-times-x`:
the second branch's MUL had a missing CONST input).

Fix: each branching chain-rule node (`UOP_MUL`, `UOP_ADD`,
`UOP_NEG`, `UOP_REDUCE`) now allocates a *fresh* EXPAND of `gy`
per branch.  A new `gy_lifted` flag threaded through `grad_rec`
prevents redundant outer EXPANDs at deeper leaf positions when
the cotangent is already target-shaped.  Test structure update
in `tests/test_grad.c`; numerics unchanged (9/9 WL grad cases
still pass).

### Changed: single-line node headers carry heap loc + handle id

Diagram + heap-graph labels now use `OPCODE@<heap-loc>(#<id>)` on
one line instead of stacking opcode and base across two lines.
The `#<id>` suffix only appears when the opcode carries an extra
handle: `KERNEL@10#2` (kernel id from the `NUM(kid)` cell),
`GRAD@3#1` (target tensor id from cell base+2), `TEN@10#1` (cell
loc + tensor id).  Plain compute UOPs stay terse: `MUL@8`,
`ADD@14`.  Shape (when known) and CONST scalar value remain on
follow-up lines.

### Changed: WL kernel split into per-concern files; shape inference centralised

`wl/THVMLink/Kernel/` now uses one BeginPackage["THVMLink`"] +
Begin["`Private`"] block per file, all sharing the same private
context.  Cross-file references resolve directly without
THVMLink`Private`-qualified calls.

Two new files separate concerns that used to be inlined in the
renderers:
- `Shape.wl` -- shape arithmetic (`broadcastShape`, `dropAxis`,
  `shapeText`), tensor-id shape lookup (`tenShapeOf`), and the
  manual IEEE 754 single-precision decoder (`bitsToReal32`,
  `bitsToInt32`, `scalarTextFromCell`).
- `Uop.wl` -- per-opcode metadata in one place: `uopArity`,
  `uopName`, plus an inferred-output-shape walker (`uopShapeOf`,
  `cellShape`, `uopSrcShape`) that mirrors the rules in
  `src/schedule/materialize.c`.

`Visualization.wl` and `Diagram.wl` now read these helpers
directly, dropping their duplicated tables.  UOP labels gained the
inferred output shape (e.g. "MUL\n@8\n{3}") and CONST keeps both
its heap base and its scalar value.

`THVMLink.wl` no longer hard-codes the load order -- after its
own EndPackage it Get's every other `*.wl` in the Kernel directory
in alphabetical order.  Adding a new sibling file means dropping
it in; no edits to the loader.

### Changed: shape-aware grad_rec drops the MUL(target, CONST(0)) wrapper

`interact_grad` no longer post-wraps the chain-rule output in
`ADD[raw, MUL(target, CONST(0))]` to coax materialize into producing
target-shaped gradients.  Instead, every leaf-level emission inside
`grad_rec` is wrapped in `EXPAND(_, target.shape)`:

- leaf match (`y === target`)        -> `EXPAND(gy, target.shape)`
- independent leaf / NUM             -> `EXPAND(CONST(0), target.shape)`
- `UOP_CONST`                        -> `EXPAND(CONST(0), target.shape)`
- `default`                          -> `EXPAND(CONST(0), target.shape)`

This required minimal materialize + interpret support for `UOP_EXPAND`
(previously a step-14 placeholder): `op_output_shape` now reads the
heap NUM cells for EXPAND's target dims (using the source view's rank
to know how many cells to read -- tinygrad EXPAND preserves rank), and
a new `cpu_op_expand` fans the source buffer out to the larger numel.
Sufficient for the autograd path (scalar -> 1-D); per-axis broadcast
in higher ranks lands with view tracking in step 14.

`tests/test_grad.c` was rewritten to expect the EXPAND wrapping
(replacing the old `unwrap` helper that stripped the dead `MUL` wrapper).
WL `grad.wlt` end-to-end numerics still pass (9/9).

### Added: shape on TEN labels, scalar value on CONST labels

`THeapDiagram`'s leaf labels now carry the data the user actually wants
to see:
- `TEN#<id>` -> reads `TENS[id].view.shape` and shows e.g. `{3}` on a
  third line.
- `CONST` -> decodes the NUM cell's bits via manual IEEE 754 (so a
  CONST(1.0) renders as `CONST\n1.` instead of a mystery `CONST\n@2`).

### Added: tensor-aware THeapDiagram (IC string-diagram path)

Diagram.wl now renders TAG_UOP / TAG_TEN terms via Wolfram`Diagrammatic`-
`Computation`, so `THeapDiagram[term]` produces a proper IC string
diagram for tensor compute graphs (it previously returned an empty
network for anything that wasn't pure IC).

UOP rendering uses an opcode-driven shape/style:
- Plain compute UOPs (ADD/MUL/...) -- apex-down blue triangle, N
  inputs at top (one per `uopArity[opcode]`), 1 output at bottom.
- GRAD -- DUP-shaped (apex-up orange triangle), 1 input at top
  apex (the y branch), 2 outputs at the flat bottom (forward
  passthrough + backward gradient).  `gy` and `target` cells are
  hidden; the target tensor id is surfaced as `#<tid>` in the
  GRAD label.

TEN handles render as cyan apex-down triangles, one leaf per
referencing slot (no DUP needed for multi-reference -- each ref
gets its own `TEN#<id>` triangle).

CONST UOPs (zero-arity) are rendered the same way: per-reference
leaves labeled `CONST@<base>`, so a constant referenced from N
slots draws N triangles instead of forcing a shared agent (which
would require DUPs to fan out).

Reachability filter walks UOPs/TENs forward from the seed term
so post-`TWnf` heaps don't surface their pre-rewrite cells.
`principalCellOf` was tightened to consider only cells inside
reachable agents' slot ranges, so dead heap can't grab a UOP's
output wire.

`run.wls` no longer skips IC diagrams for `grad-` examples; both
pre-reduce (`diagram.png`) and post-WNF (`diagram-wnf.png`) IC
diagrams are now rendered alongside the heap graphs.

New plain-UOP examples (no grad rewrite):
- `wl/Examples/uop-add` -- `TUOpAdd[a, b]`
- `wl/Examples/uop-mul` -- `TUOpMul[a, b]`
- `wl/Examples/uop-mul-add` -- `(a*b)+c`

Existing grad-`*` examples now use lazy `TTensor[{3}]` allocations
instead of `TTensorCreate @ NumericArray[...]`; the visualization
doesn't need real numerics and the lazy form is shorter.

### Added: tensor-aware heap graph + grad- visualization examples

`Visualization.wl` got a major extension to render tensor compute
graphs (it previously only knew about IC tags LAM/APP/SUP/DUP/ERA,
so any `TAG_UOP` / `TAG_TEN` term came out blank).

New vertex-id convention prefixes the kind:
- `a<base>` -- IC compound at args base `<base>`
- `e<loc>`  -- ERA cell at heap loc
- `u<loc>`  -- TAG_UOP at heap loc
- `t<id>`   -- TAG_TEN at tensor id

Per-tag rendering:
- `TAG_TEN` -- cyan square labeled `TEN\n#<id>`
- `TAG_UOP` -- blue rectangle labeled `<OPCODE>\n@<loc>`
- Edge labels follow `src<N>` using a per-opcode `uopComputeArity`
  table (NUM-only cells stay implicit).

Single-vertex default size bumped (0.18 -> 0.45) so identity-only
terms don't render as a pinhead.

Three new `wl/Examples/grad-*` folders, each with `term.wl` plus
pre-reduce (`term.png`) and post-`TWnf` (`term-wnf.png`) heap
renderings:
- `grad-add` -- gradient of `a + b` w.r.t. `a` -> ones_like(a)
- `grad-mul` -- product rule `d(ab)/da` -> b
- `grad-x-times-x` -- `d(a*a)/da` -> 2a

`run.wls` detects `grad-`-prefixed folders, skips the IC string
diagram (tensor graphs aren't IC nets), and renders both the
pre-reduce graph and the post-`TWnf` rewritten graph using the
`TWnf` result as the discovery seed.

### Added: PLAN.md step 13 (partial) -- UOP_GRAD reverse-mode autograd

`UOP_GRAD` is the 18th UOp opcode and a pure rewrite rule (not a
graph node that survives reduction).  Reducing
`UOP_GRAD[y, gy_seed, target]` under `TWnf` recursively applies the
chain rule until no `UOP_GRAD` nodes remain, then wraps the result
in a `target * 0` summand so the broadcast machinery in materialize
projects it onto target's shape.

Step-13 chain-rule coverage: leaf cases (target match, other tensor,
NUM, CONST), `UOP_ADD`, `UOP_MUL` (product rule), `UOP_NEG`, and
`UOP_REDUCE` (SUM only -- MAX needs an indicator one-hot, deferred
to step 14).  Anything else returns `CONST(0)` and warns.

WL surface:
- `TUOpGrad[y, gy, target]` -- explicit cotangent.
- `TGrad[y, target]` -- top-level VJP shortcut with `gy = CONST(1)`.

`materialize_expr` recognises `UOP_GRAD` and reduces it inline before
kernelizing, so `TMaterialize[TGrad[...]]` works without a separate
TWnf pass.

Tests:
- `tests/test_grad.c` (16 checks): structural pin-downs of the
  rewrite output for each handled opcode.
- `wl/THVMLink/Tests/grad.wlt` (9 checks): end-to-end f32 numerics
  including identity, independent leaf, ADD, MUL product rule,
  NEG, REDUCE_SUM broadcast-back, `x*x = 2x`, and `2x + 3 = 2`.

### Removed: `Function[t_TTerm]` UpValue

The `(f_Function)[t_TTerm] -> TApp[TLam[f], t]` IC sugar was a
footgun -- it silently rewrote any pure-function map over TTerms
into a beta-redex (which surfaced as a crash when our numeric
Plus/Times UpValues used `& /@`).  Removed alongside the
`$inTLamBinder` guard that only existed to break the resulting
recursion.  `TTerm[id_Integer][arg]` sugar (forming
TApp[TTerm[id], arg]) stays.

### Added: PLAN.md step 12 -- TTensor + TUOp + materialize + dispatch

End-to-end tensor pipeline.  WL-built UOp graphs reduce naturally
through schedule + kernelize + linearize + interpreter dispatch to
concrete `TAG_TEN` results, all under one `TWnf` call.  See
`docs/tensors.md` and `docs/glossary.md`.

Six commits across the step:

- **tensor foundation** (139af93)
  - Three new term tags: `TAG_TEN` (8) atom for tensor handles,
    `TAG_UOP` (9) heap-backed for graph nodes, `TAG_NUM` (10) atom
    for inline scalars.
  - `TenDesc` side table (`TENS[]`) with refcount, View
    (shape/strides/offset), buffer id, and Backend pointer.
  - CPU `Backend` vtable: alloc/free/incref/decref + buf_read/write,
    parallel `CPU_BUFS[]` table with its own refcount for view
    aliasing.
  - View aliasing (`tensor_view_of`) bumps the buffer refcount so
    reshape/permute can share storage zero-copy in step 14.

- **UOp vocabulary + WL surface** (719ac4a)
  - 18 opcodes covering CONST, six movement ops
    (RESHAPE/PERMUTE/EXPAND/PAD/SHRINK/FLIP), eight elementwise ops
    (ADD/MUL/NEG/RECIP/EXP2/LOG2/SQRT/CMPLT), REDUCE, plus the
    rewrite triggers MATERIALIZE and KERNEL.
  - One `src/uop/<op>.c` per opcode emitting the documented heap
    layout.
  - WL surface in the new `Tensor.wl` sibling: `TTensor`,
    `TUOpAdd/Mul/.../Reduce`, `TUOpMaterialize`, plus inspection
    helpers.

- **TRealize + TTensorCreate + zero-copy NumericArray I/O** (862887e)
  - `TRealize[expr] := TWnf[TUOpMaterialize[expr]]`.
  - `TTensorCreate[data]` shares a `NumericArray`'s buffer on the
    CPU backend (Shared passing mode + per-buffer cleanup
    callback).  PackedArrays / nested Lists lift to NumericArray
    first.
  - `TTensorData` returns a `NumericArray` whose type matches the
    dtype (single memcpy in the f32 fast path; no f32 -> f64
    conversion).
  - CpuBuf gains `owns_data` + `on_release` callback so the same
    slot can hold either malloc'd or borrowed bytes.

- **materialize pipeline** (8ffd333)
  - New `KERNELS[]` side table with linearized `KProgOp` programs;
    the same SSA-over-indices shape tinygrad's PYTHON device
    consumes.
  - `src/schedule/materialize.c` rewrites a UOp graph into a tree
    of `UOP_KERNEL[output_buf, NUM(kid)]` terms; recursively
    materializes children, dedups identical inputs.
  - `TMaterialize` WL helper for inspecting the scheduled DAG
    *before* kernel firing; `TKernelInfo[kid]` returns the
    linearized program as an Association.

- **CPU interpreter + interact_kernel** (3e071bd)
  - Per-op CPU files under `src/backend/cpu/op/` (one per opcode,
    matching the project's file = function name convention).
  - `cpu_interpret` walks `KernelEntry.program[]`, allocates one
    scratch per intermediate, dispatches via switch on opcode.
  - `interact_kernel` recursively fires producer kernels first
    (via the new `TenDesc.producer_kid` field), then invokes
    `Backend.dispatch_kernel` for the current kernel.  Increments
    `ITRS` once per firing, the same way HVM4 counts an OP2-NUM-NUM
    collapse.
  - `wnf` extension: `TAG_UOP/UOP_MATERIALIZE` -> direct rewrite,
    `TAG_UOP/UOP_KERNEL` -> fire, anything else -> WNF.

- **PLAN.md** (9b5a4db)
  - Step 12 marked done.

Numerical UpValues on `TTerm` (Plus / Times / Minus / Power[1/2] /
Less) rewrite ordinary WL arithmetic against tensor-shaped TTerms
into UOp graphs.  Scalars lift to UOP_CONST with the seed tensor's
dtype.

Removed: the `Function[t_TTerm]` UpValue that converted `f[t]` to
`TApp[TLam[f], t]`.  It was dumb, surprised the Plus/Times rewrite
that maps over tensors, and the matching `TLam[$inTLamBinder] guard`
went with it.

End-to-end:
```mathematica
a   = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
b   = TTensor[{4}, {10.0, 20.0, 30.0, 40.0}];
out = TRealize[2.0 * (a + b) + 1.0];
Normal @ TTensorData[out]
(* {23.0, 45.0, 67.0, 89.0} *)
```

### Added: THeapDiagram (Wolfram`DiagrammaticComputation` backend)

- New `wl/THVMLink/Kernel/Diagram.wl` subpackage at context
  `THVMLink`Diagram` exporting `THeapDiagram[term]`, which builds a
  `DiagramNetwork` from the current heap using the
  `Wolfram/DiagrammaticComputation` paclet (assumed installed).
- The subpackage lives in its own context so its `BeginPackage`
  imports can pull in `Wolfram`DiagrammaticComputation` and its
  `Diagram` subcontext without shadowing names in the main
  `THVMLink` context.
- Wire-name strings are unique per heap location: `w<loc>` for
  cells, with `VAR` cells collapsed to their binder's wire and
  `DP0`/`DP1` cells expanded to `dup<base>_dp{0,1}_lab<ext>`.
- `wl/Examples/run.wls` now writes `diagram.png` next to `term.png`
  for each example's input `term.wl` (skipped for reference
  variants like `term-reduced.wl`).

### Added: TTermExpr / TTermTree, TReduce, .hvm refs, restructured examples

- `wl/Examples/` folders no longer carry numeric prefixes; the
  reduced variants merge into their parent (`02-id-app-era` +
  `03-id-app-era-reduced` -> `id-app-era/` with both `term.wl` and
  `term-reduced.wl`).  `13-church-2-applied` becomes its own
  top-level `church-2-applied/` (the lambda lives in `church-2/`).
- `term.wl` is the input term construction.
- `term-reduced.wl` (optional) constructs the *expected* WHNF
  directly -- no `TWnf` / `TReduce` inside it.  The reduction test
  runner compares `TTermExpr[TWnf[term.wl]]` against
  `TTermExpr[term-reduced.wl]`.
- `term.hvm` (optional) carries the HVM4 surface-syntax reference
  with the expected output as a `//` comment line.  Documentation
  only; we have no parser yet.
- `wl/Examples/run.wls` now scans `term*.wl` per folder but only
  renders the input `term.wl` (skips `term-reduced.wl`, which is
  reference data).
- `wl/Examples/test_reductions.wls` is the reduction-comparison test
  driver, wired up as `make wl-examples-test`.
- New WL helpers in the paclet:
  - `TReduce[t]` = `(TWnf[t]; t)` -- reduces in place and returns the
    original root.  Useful as a `THeapGraph` seed when you want to
    visualise the post-reduction state.
  - `TTermExpr[t]` walks the heap from `t` and returns a nested
    expression with tag-name string heads (`"LAM"`, `"APP"`, `"SUP"`,
    `"DUP"`, `"DP0"`, `"DP1"`, `"VAR"`, `"ERA"`).  Cycles produce
    `"Cycle"[loc]` leaves.
  - `TTermTree[t]` = `ExpressionTree[TTermExpr[t]]` for visual
    rendering as a Wolfram `Tree`.
- README catalogue rewritten with the new folder names + an
  "Expected WHNF" column pointing at `term-reduced.wl`.

### Added: dark export + auto-fit labels + sugar

- WL `THeapGraph` accepts trailing `Graph` options via
  `OptionsPattern[]` (per the GUIDE) so callers can override
  `GraphLayout`, `VertexSize`, `PlotRange`, `Background`, etc.
- Vertex labels now render INSIDE each shape via `Inset[Pane[label,
  {pixelW, pixelH}, ImageSizeAction -> "ShrinkToFit"]]`.  Labels
  auto-shrink so the same `LAM @0` text fits cleanly in any vertex
  size.
- `VertexShapeFunction` honours the `size` argument throughout,
  including the ERA stroked Circle, so `VertexSize -> Tiny | Small |
  Large | Scaled[...]` all behave.  Removed the manual
  `singleVertexLoopFn` hack -- the default Wolfram self-loop renderer
  works once the shape sizes are scaled correctly and the plot range
  has room for the loop (single-vertex case explicitly widens
  `PlotRange` and shrinks the vertex).
- Examples export onto a dark `GrayLevel[0.12]` background with
  `Style[..., "DarkScheme"]` so `LightDarkSwitched` picks the
  dark-mode arm (white labels, darker fills, white outlines).
  Generated PNGs now read cleanly on dark READMEs and notebooks.

### Added: TTerm sugar (call as function, lambda literal)

- `TTerm[id_Integer][arg_]` desugars to `TApp[TTerm[id], arg]` so
  users can write `id[era]` instead of `TApp[id, era]`.
- `(var |-> body)[t_TTerm]` desugars to `TApp[TLam[var |-> body], t]`
  via a tagged UpValue on `TTerm`.  Lets you write a literal
  beta-redex without spelling out `TLam` / `TApp`.
- The `Function` UpValue is guarded by `$inTLamBinder` so `TLam`'s
  own internal call `builder[TVarFor[loc]]` does not trigger it
  (which would recurse infinitely).
- Two new VerificationTests cover both forms.

### Added: DUP-LAM + church-numeral examples

- `src/interact/dup_lam.c`: real DUP-LAM rule.  Allocates one
  five-cell block holding the new pair of bound vars (as a SUP
  inside the original binder) and the new pair of body projections
  (as a fresh DUP over the original body).  No body cloning happens
  eagerly -- only when a future projection inspects part of the
  body does it descend lazily.  This is the rule that gives Church
  numerals (and similarly Lamping / optimal-reduction style
  workloads) their non-exponential cloning behaviour.
- `tests/test_dup_lam.c`: two C tests; clone an identity lambda and
  confirm DUP-LAM fires once, then end-to-end apply one of the
  cloned copies to ERA.
- `wl/Examples/10-k-combinator/`, `11-church-1/`, `12-church-2/`,
  `13-church-2-applied/`: four new runnable examples.  The Church 2
  family exercises the DUP machinery; the applied form reduces
  end-to-end and the resulting graph (in `13-...-applied/graph.png`)
  shows the post-firing heap including the cloned lambdas and the
  substituted DUP cell.
- Two new VerificationTests in `wl/THVMLink/Tests/core.wlt`: a
  direct DUP-LAM clone, and Church-2-applied reducing to the
  identity-applied result.
- `docs/interact/dup_lam.md` documents the rule, the C, the cost,
  and why the lazy-cloning shape matters.

### Added: visualization renderer split + theme-aware colors

- `wl/THVMLink/Kernel/Visualization.wl`: extracts the heap-graph
  renderer into a dedicated kernel sibling.  THVMLink.wl now `Get`s
  it after declaring public symbols.
- Theme-aware colors throughout: `LightDarkSwitched[Black, White]`
  for foreground; `Lighter[StandardX, 0.55]` / `Darker[StandardX,
  0.45]` per-tag agent fills (green LAM, blue APP, orange SUP,
  purple DUP); ERA stays as a plain foreground-stroked Circle.
- Vertex labels now render in column form: `TAG\n@<base>` for
  arity-1 agents, `TAG\n@<base>..<base+1>` for arity-2.
- Triangles are real triangles via `Triangle[]` (not trapezoids)
  with apex orientation matching IC convention: LAM/DUP point down,
  APP/SUP point up.
- VertexShapeFunction now respects the size argument so
  `VertexSize -> Tiny | Small | Large | Scaled[...]` actually take
  effect.
- Single-vertex self-loop is drawn explicitly via
  EdgeShapeFunction; the identity lambda's loop is now visible.
- Pink "background" mystery solved: `Dashing[{Small, Small}]` was
  invalid (Small is not a numeric Dashing arg) which silently put
  Wolfram into an error-overlay state.  Replaced with the proper
  `Dashed` directive.
- Context-shadowing fix: switched `wl/Examples/run.wls` and
  `wl/THVMLink/Tests/run.wls` from `Needs["THVMLink`"]` to
  `Get["THVMLink`"]` so user code resolves to package symbols
  rather than auto-created `Global`*` placeholders.
- `wl/GUIDE.md` gains a Dark-mode + Standard colors section and an
  OptionsPattern[] section.

### Added: TTerm atomic wrapper + ensureInit

- TTerm[id_Integer] is the canonical wrapper around a packed Term;
  TLam / TApp / TSup / TDup / TEra / TVarFor return TTerm-wrapped
  values; TTermTag/Ext/Val/Sub accept either a TTerm or a raw
  Integer.  Old TTermInfo is gone (folded into TTerm[id]["info"]);
  TTermNew is no longer in the public API (private packTerm helper).
- TTerm[id]["tag" | "ext" | "val" | "sub" | "tagName" | "raw" |
  "info"] forwards to the bridge.  Format.wl gives TTerm a summary
  box keyed off the structural pattern (QuantumFramework style).
- ensureInit[]: heap-touching ops auto-call TInit if the runtime is
  not initialised yet.  TFree clears the flag.

### Added: wl/Examples/ runnable example database

- New `wl/Examples/` directory: one folder per example term, each
  holding a minimal `term.wl` (no `Needs`, no `TInit`, just the
  expression to construct the term) plus the rendered
  `graph.png` produced by the runner.
- 9 examples covering every interaction we currently fire: identity
  lambda, (id ERA) before / after `TWnf`, (ERA lam) before / after
  `TWnf`, bare `TSup[ERA, ERA]`, DUP-SUP same-label annihilation
  before / after, and nested APPs.
- `wl/Examples/run.wls`: single CLI for both bulk and per-example
  runs.  Loads the paclet, calls `TInit` per example, evaluates the
  `term.wl`, exports the resulting `THeapGraph[term]` as a PNG
  alongside the source.  Supports a positional example id and a
  `--eval` flag to skip the PNG export.
- `wl/Examples/README.md` catalogues every example and documents how
  to add new ones.
- `make wl-examples` (regenerate every PNG) and
  `make wl-examples EXAMPLE=<id>` (just one).
- `docs/heap_graph.md` now embeds two of those PNGs directly from
  `wl/Examples/<id>/graph.png` so the doc and the runnable example
  stay in sync.  The previous one-off `docs/images/` directory is
  removed.
- `wl/GUIDE.md` gains a rule for multi-line `If`: leading space after
  the bracket so the test argument lines up with the branches
  (`If[ cond, then, else]`).

### Added: heap graph rendering (PLAN.md step 10)

- `THeapGraph[]` and `THeapGraph[term]` (or `THeapGraph[{t1, t2,
  ...}]`) render the runtime as an IC string-diagram Wolfram
  `Graph[]`: compound terms (LAM/APP/SUP/DUP) are agent vertices
  keyed by their args base, VAR cells collapse into wires labelled
  `var`, and ERA cells render as small black dots.  Optional seed
  terms add agents that are heapless (held only as WL return values).
- `THeap[]` now returns an atomic `THeap[<|nextLoc, cells, Graph|>]`
  with the rendered graph at the `"Graph"` key (capitalized).
- `TTermInfo[t]` now returns an atomic `TTermInfo[<|...|>]` with the
  same payload shape.
- Both atomic objects expose Association-style indexing via DownValues
  and forward `KeyExistsQ`/`Keys`/`Values`/`Normal` via UpValues so
  callers see the same access shape as before.
- `wl/THVMLink/Kernel/Format.wl` defines the `MakeBoxes` UpValues
  (QuantumFramework-style: structural Q-test guarded by `Unevaluated`,
  `BoxForm`ArrangeSummaryBox` for the visual).  Loaded from
  `THVMLink.wl` after the public symbols are declared.
- `TFreshLabel[]` returns a fresh integer from a monotonic counter
  (reset by `TReset[]`).  `TSup[a, b]` and `TDup[body, k]` now
  auto-label via `TFreshLabel[]`; the existing 3-arg
  `TSup[label, a, b]` / `TDup[label, body, k]` forms remain for
  tests that need explicit label matching.
- `wl/THVMLink/Tests/core.wlt` gains six new VerificationTests:
  fresh-label monotonicity + TReset rewind, auto-label distinctness
  for both SUP and DUP, identity-lambda `THeapGraph` shape, seeded
  vs unseeded graph for `TApp[id, ERA]` and `TDup[TSup[ERA, ERA], k]`.
- `docs/heap_graph.md` is now the permanent reference for the model
  (agent-as-vertex, VAR-as-wire, ERA-as-dot) with six worked
  snapshots, mermaid diagrams, and live `THeapGraph` PNGs for
  examples 2 and 4 (regenerated by `docs/images/generate.wls`).
- `docs/term.md` gains a glossary table pinning down term / cell /
  loc / slot / agent / args base / port / node / wire and links
  forward to `docs/heap_graph.md`.
- `docs/wl.md` documents the `Format.wl` summary-box layer and the
  layout convention.
- `docs/images/generate.wls` produces the PNGs embedded in the doc;
  `docs/images/` is the canonical location for generated diagrams.

`make test`    -> 91 C checks pass.
`make wl-test` -> 23 WL VerificationTests pass.

### Added: architecture docs (PLAN.md steps 8-9)

- `docs/` with a self-contained markdown per piece, indexed by
  `docs/README.md`:
  - `docs/term.md`: bit layout + tag table + worked examples.
  - `docs/heap.md`: bump allocator + the substitution model
    (`heap_subst_var`, `heap_subst_cop`).
  - `docs/wnf.md`: enter/apply state machine, frame protocol, and
    the dispatch table for current interactions.
  - `docs/interact/_.md`: index of active-pair rules + tracking of
    which active pairs are stuck (deferred).
  - `docs/interact/{app_lam,app_era,dup_sup,dup_era}.md`: one page
    per interaction with the sequent rule, the C, a worked example,
    and a cost summary.
  - `docs/wl.md`: WL paclet design (scalar bridge + WL-side
    constructors) and usage.
- `README.md`: top of the file points at `docs/README.md` and the
  layout block now includes `docs/`.
- `AGENTS.md`: workflow step 4 clarifies that the `docs/interact/`
  page is the source of truth when it disagrees with the C file's
  header comment.

### Added: minimal reducer + interactions (PLAN.md steps 5-6)

- `src/wnf/_.c`: real two-phase stack-machine reducer (enter/apply)
  modeled on HVM4's clang/wnf/_.c.  Pushes APP / DP0 / DP1 frames at
  enter, dispatches active-pair interactions at apply, rebuilds stuck
  nodes by writing the reduced head back into the heap cell.
- `src/interact/app_lam.c`: real APP-LAM beta (`(lam x.body) arg`
  substitutes `arg` at the binder loc and continues into `body`).
- `src/interact/app_era.c`: APP-ERA (erased function yields ERA).
- `src/interact/dup_sup.c`: DUP-SUP same-label annihilation.  The
  commuting (different-label) case is left stuck for now; a test will
  drive the implementation when needed.
- `src/interact/dup_era.c`: DUP-ERA (both projections receive ERA via
  `heap_subst_cop`).
- `src/heap/subst_cop.c`: pair-substitution helper used by both
  DUP-style interactions; substitutes one side and returns the other.
- `src/thvm.h`: declares the new `interact_*` and `heap_subst_cop`
  signatures.
- `tests/test_app_lam.c`, `tests/test_era.c`, `tests/test_dup_sup.c`:
  removed the `PENDING(...)` gates and added ITRS-counter assertions
  so each test verifies the specific interaction fires (and only
  fires once).
- `wl/THVMLink/Tests/core.wlt`: three new VerificationTests covering
  APP-LAM, APP-ERA, and same-label DUP-SUP through the LibraryLink
  bridge.
- `Makefile`: moved `SRC :=` definition above the `$(WL_LIB)` rule so
  `make wl` correctly retriggers when any C runtime file changes.

`make test` -> 91 C checks pass.  `make wl-test` -> 14 WL tests pass.

### Added: WL paclet (PLAN.md step 4)

- `wl/THVMLink/` paclet that exposes the C runtime to Wolfram
  Language, with the LibraryLink bridge in `wl/THVMLink/CSource/`,
  the package in `wl/THVMLink/Kernel/THVMLink.wl`, and tests in
  `wl/THVMLink/Tests/`.
- `wl/THVMLink/CSource/thvmlink.c` exports 14 scalar `EXTERN_C
  DLLEXPORT` functions covering lifecycle (init/free/reset), term
  packing/unpacking (`thvm_wl_term_*`), heap access
  (`thvm_wl_heap_pos/alloc/read/set`), the WNF entry point, and the
  interaction counter. Every function is scalar-in / scalar-out (no
  arrays, no opaque handles).
- `wl/THVMLink/Kernel/THVMLink.wl` synthesizes higher-level term
  constructors (`TLam`, `TApp`, `TSup`, `TDup`) from the scalar
  primitives via shared `heapWith` / `heapTerm` helpers, plus the
  inspector `TTermInfo` and the heap snapshot `THeap[]`.
- `wl/THVMLink/Tests/core.wlt` defines 11 `VerificationTest` specs
  covering term packing roundtrip, heap primitives, the four
  high-level constructors, the heap snapshot, and the WNF stub
  passthrough.
- `wl/THVMLink/Tests/run.wls` is the test runner. It loads the
  paclet, invokes `TestReport` on every `*.wlt` file, prints
  `wl tests: N passed, M failed` to stdout, lists each failed test,
  and exits non-zero on any failure.
- `wl/GUIDE.md` records WL style rules: no `Print` (use a local
  `debugPrint` wrapping `WriteString`), no em dashes, no Unicode
  box-drawing characters, no decorative arrows in source.
- `Makefile` gains `make wl` (build the dylib at
  `wl/THVMLink/LibraryResources/$(WL_PLATFORM)/THVMLink.dylib`) and
  `make wl-test` (run `run.wls`). Auto-detects the newest
  `/Applications/Wolfram*.app`; override with
  `WOLFRAM_APP=/Applications/Wolfram\ X.Y.app`.

### Added: scaffold (PLAN.md steps 0-3)

- `AGENTS.md` with conventions (path-is-the-function-name, single TU,
  one-interaction-per-file), build/test instructions, and a code map.
- `.gitignore` covering `bin/`, `*.o`, `*.dylib`, macOS `.DS_Store`,
  the local-only `.claude/` settings dir, and the `TinyHVM` reference
  symlink.
- `Makefile` with `make` (build all), `make test` (build + run tests),
  `make clean`. Tests are independent C programs that include
  `src/thvm.c`.
- `src/thvm.h` declaring the term bit layout (SUB:1 / TAG:7 / EXT:18 /
  VAL:38), the minimal tag set (APP, LAM, VAR, ERA, DP0, DP1, SUP,
  DUP), heap globals, and function signatures for the
  term/heap/wnf/interact modules.
- `src/thvm.c` single-TU hub that `#include`s all `.c` files in build
  order.
- `src/term/{new,tag,ext,val}.c` and `src/term/sub/{get,set}.c`:
  full implementations of term packing/unpacking. Trivial
  bit-twiddling.
- `src/heap/{alloc,read,set,take,subst_var}.c`: flat single-threaded
  bump-allocated heap with substitution helper.
- `src/wnf/_.c`: WNF stack machine **stub** that returns its input
  unchanged. Step 6 will replace this with the real reducer.
- `src/interact/app_lam.c`: APP-LAM beta reduction **stub**. Step 6
  fills it in.
- `tests/test_term.c`: round-trip test for `term_new` and
  `term_tag/ext/val/sub_get`. 73 checks pass.
- `tests/test_heap.c`: alloc-then-read-back, set-then-read-back. 9
  checks pass.
- `tests/test_app_lam.c`, `tests/test_era.c`, `tests/test_dup_sup.c`:
  carry the spec for APP-LAM beta, ERA propagation, and DUP-SUP
  collapse/commute. Bodies are gated by `PENDING(...)` until step 6
  lands `wnf` and the interactions, so they exit 0 today and report
  `pend` in `make test` output.
- `README.md` describing what works today and what is stubbed.
