# IC-native ATP -- design sketch

This memo redirects the equational ATP off the C-side completion
loop in [src/atp/_.c](../../src/atp/_.c) and onto the existing
IC reducer + AOT-Metal pipeline.  The C-side stays as a fallback
until the IC encoding handles the same cases.

## Architecture

- **WL-side prep**: encode `(axioms, conjecture, depth)` into a
  Term DAG.  The Term graph IS the search problem; reducing it
  produces the proof.
- **IC reduction**: existing reducer (CPU `wnf`/`cnf`, or AOT
  Metal) collapses the Term to a SUP-tree of `NUM` leaves.  Each
  leaf = one candidate proof path's outcome (1 = closed at
  tautology, 0 = stuck or out of depth).
- **WL-side decode**: collapse the SUP-tree, look for any `NUM(1)`
  leaf, optionally trace back the SUP-label path that yielded 1
  to reconstruct the proof chain.
- **Ordering** (later iter): INC labels on CTR cells encode
  precedence; the same labels drive both reduction direction and
  saturator branch priority.  No `KboConfig`.

## Toy case: transitivity, atoms only

Conjecture `a == c` from `{a == b, b == c}`.

Encode atoms as `NUM`s: `a -> 1, b -> 2, c -> 3`.

A "rewrite" of one atom by axiom `(old, new)` direction `dir`:
```
rewriteAtom[atom, old, new] = TIfZero[ TOp2["==", atom, old],
                                       atom,         (* not equal: passthrough *)
                                       new ]         (* equal: rewrite *)
```

A term is a pair `(lhs, rhs)`.  One step of the search picks
which of (axiom1Fwd, axiom1Bwd, axiom2Fwd, axiom2Bwd) to apply,
and which side (LHS or RHS) to apply it to:

```
choice[lhs, rhs, axRew] = TSup[label_choice, axRew[lhs, rhs] (* L *),
                                              axRew[lhs, rhs] (* R *)]
```

where `axRew[lhs, rhs]` returns the resulting pair after applying
the rewrite to one side.

Stack `D` choices, get a SUP-tree of `4 * 2 = 8` leaves per step,
`8^D` leaves total.

At each leaf, evaluate `TOp2["==", lhs', rhs']` to produce
NUM(1) iff the two atoms are now equal (proof closed) else
NUM(0).

`TCollapse` enumerates the leaves; `AnyTrue[#, # === 1 &]`
answers "is the conjecture provable in <= D steps?"

## What this gets us at depth 2

For transitivity `a == c` from `{a == b, b == c}`:

- D=2, 8^2 = 64 leaves.
- One winning path: step 1 applies axiom1Fwd to LHS (`a -> b`),
  giving `(b, c)`; step 2 applies axiom2Fwd to LHS (`b -> c`),
  giving `(c, c)` -> NUM(1).
- 63 non-winning paths -> NUM(0).
- `AnyTrue[leaves, # === 1 &]` -> True.

Cost: dominated by the SUP-tree fan-out.  At V variables and D
depth, leaves = `(2 * n_axioms * 2)^D`.  Toy is fine; AOT-Metal
makes V=10/D=4 tractable per the iter Z+1 plan.

## Bigger case: substitutivity (positions)

`f[a] == f[b]` from `{a == b}`: now the rewrite has to descend
into a structured term, not just compare/replace at the root.

Encoding: each "term" is a CTR cell `Eq[lhs, rhs]` where lhs/rhs
are themselves CTR cells.  A rewrite walks the cell tree, fires
at every position whose head matches the rule's LHS.  Each match
position becomes a SUP child (so the search enumerates positions
the way it enumerates axiom choice).

Position-enumeration is the harder primitive.  Two options:

1. **Compile-time unfold**: WL pre-computes the set of positions
   where each axiom *could* match (purely structural -- LHS shape
   matching), bakes them as separate per-position rewrite TDefs,
   the IC just enumerates over a fixed set.  Cheaper, but limits
   to fixed term shapes.

2. **Runtime traversal**: encode a generic "rewrite-anywhere"
   traversal as an IC term that walks any structure and fires.
   More general but heavier reduction.

Start with (1) since the toy + most ATP benchmarks have known
term shapes at encode time.

## Pattern axioms (commutativity etc.)

`add[x_, y_] == add[y, x]` is harder: the rewrite is a pattern
with capture variables.  Two options:

1. **Pre-instantiate**: WL enumerates the substitutions
   `{x -> ?, y -> ?}` against all subterms of the conjecture and
   bakes the resulting ground rewrites into the SUP-tree.  Same
   trick as (1) above but for pattern variables.

2. **Native pattern matching in IC**: encode the pattern variable
   as a SUP-of-all-atoms; reduction picks which atom each var
   resolves to.  Closer to "true" IC-native, but the SUP fan-out
   is V^k for k pattern vars.

Defer.  Most equational benchmarks are ground after a small
number of pre-instantiations.

## AOT-Metal integration

The search Term reduces to a SUP-tree of `NUM`s.  This is the
exact shape that iter Z+1 (in the misty-jumping-popcorn plan)
already handles:

- Stage 1: AOT-Metal kernel-1 sequentially reduces the TDef body
  to a SUP-tree-rooted Term in the shared book heap.
- Stage 2: parallel collapse kernel walks the SUP-tree, one
  thread per leaf, writes the leaf NUM into a result buffer.
- Host: read result buffer, `AnyTrue[#, # === 1 &]`.

So the ATP encoding rides the existing pipeline -- no new
shaders.  The work is on the WL encode side.

## Required primitives

What we need (mostly already exist):

- [x] `TNum`, `TOp2["=="]`, `TIfZero` -- atoms, equality, branching.
- [x] `TSup[label, a, b]` -- search-branch SUP.
- [x] `TLam`, `TApp`, `TRef`, `TDef` -- function abstraction.
- [x] `TCollapse` -- SUP-tree leaf enumeration on CPU.
- [x] AOT-Metal collapse (iter Z + Z+1, in flight).
- [ ] **A clean way to encode "Eq[lhs, rhs]" pairs** -- probably
      just `TCtr["Eq", lhs, rhs]` or stash as a 2-tuple via
      `TLam[k, TApp[TApp[k, lhs], rhs]]` Church-pair.
- [ ] **Compile-time position unfold** for substitutivity --
      WL helper that enumerates match positions per (axiom, term)
      and emits per-position rewrite TDefs.

Nothing in C changes for v1.

## Decoder

After running, we have either:
- A SUP-tree on the heap (CPU path, post-`TWnf`).
- A `[N]` Term result buffer (Metal path, post-iter-Z+1).

Either way, decode = walk leaves, look for `NUM(1)`, return
True/False.

For `TFindEquationalProof`'s ProofObject path, we want more than
a yes/no.  The winning leaf came from a specific SUP-label path;
trace that path back to recover the (axiom, direction, side)
sequence used.  Each SUP gets a distinct label at construction
time; the post-collapse leaf's `path` (sequence of which side of
each SUP it came from) maps back to the choice sequence.  Encode
the choice index in the SUP label so the decode is mechanical.

## Findings: TEql is the right primitive, not TOp2["=="]

The user pointed out that HVM4 has a dedicated **EQL** cell explicitly
designed for theorem proving:
[`HVM4/clang/wnf/eql_*.c`](../../HVM4/clang/wnf/) covers EQL-CTR
(structural recursion + AND-chain), EQL-LAM (fresh-name HOAS subst
into both binders), EQL-SUP-{L,R}, EQL-NUM-NUM, EQL-ERA/ANY.  thvm
already has `TAG_EQL` and most of the rules in
[src/wnf/_.c](../../src/wnf/_.c) (NUM-NUM, ERA/ANY, SUP-L, SUP-R).

This is the right primitive for our equational ATP encoding -- the
current `TOp2["=="] + TIfZero + Church-AND` recipe is a hack.

**Added in this iter**:
- `term_new_eql` C FFI binding (`thvm_wl_term_new_eql`).
- `TEql[a, b]` WL surface in [Switch.wl](../../wl/THVMLink/Kernel/Switch.wl).
- Direct `TEql[NUM, NUM]` and `TEql[SUP, *]` work (5 smoke probes pass
  via `TWnf + TCollapse`).

**Discovered gap** (not fixed yet): `TEql` inside a `TLam` body is
broken under `alo_realize` because [src/alo/realize.c](../../src/alo/realize.c)'s
`alo_node_arity` doesn't list TAG_EQL (only APP/SUP/DUP/OP2/MAT/UOP).
For unrecognized tags `default_node` returns the BOOK_HEAP cell
verbatim -- no fresh DYN copy.  The TVar inside EQL still points to
the BOOK_HEAP binder, never sees the per-call APP-LAM substitution,
and the EQL frame's `heap_set(loc+0, whnf)` mutates the BOOK_HEAP
cell across calls.  Same gap likely for AND/OR/WHEN.

**Resolution** (commit a355cb1d): the alo_arity fix needed BOTH
sides -- `alo_node_arity` AND
[src/book/from_dynamic.c](../../src/book/from_dynamic.c)'s
`dyn_arity`.  When `dyn_arity` returned 0, TDef snapshots left
the EQL cell pointing into DYN_HEAP; subsequent TRef accesses
dereferenced freed memory -> segfault.  Fixed by listing
TAG_EQL/AND/OR/WHEN in both arity tables.  TEql in LAM bodies
now works correctly: `TLam[v, TIfZero[TEql[v, 1], v, 2]]`
applied to NUM(3) returns NUM(3) (passthrough), to NUM(1)
returns NUM(2) (rewrite fired).

**Metal-side EQL port** (commit 98bf5c1a): mirrors OP2's wnf
state machine -- TAG_EQL ENTER pushes frame and descends left;
APPLY EQL frame fires NUM-NUM directly, eql_sup_l on SUP-left,
or pushes F_EQL_R (with reduced left at heap[loc+0]) and
descends right; APPLY F_EQL_R fires NUM-NUM compare, eql_sup_r
on SUP-right.  Two new MSL inlines (aot_eql_sup_l / _r) at
7 cells each.

**Toys still on TOp2** for now: Metal regression on the
TEql-converted toys (1/6 down to 2/6) traces to the same
DUP-of-shared-cell pattern we hit before with OP2 -- EQL's
`heap_set(loc + 0, whnf)` mutation through the F_EQL_R frame
interacts differently with the search-Term sharing than
OP2's frame does.  EQL emit infrastructure stays in tree for
when the encoder learns to insert TDups at sharing points.

**Implications for milestones**:
- Milestone 4 (rebuild TFindEquationalProof on IC search) should use
  TEql rather than TOp2["=="] once the alo gap closes.
- Milestone 5 (pattern axioms) gets EQL-LAM HOAS naturally for free
  if we port the HVM4 rule.
- The existing OP2-based encoding's residual off-by-1 on Metal
  (B4 in [bisect_aot_metal.wls](../../wl/Examples/atp_ic/debug/bisect_aot_metal.wls))
  may dissolve once the encoding switches to EQL -- the
  DUP-of-shared-rSup-REF trouble in B4 is downstream of the TIfZero
  + TApp[rSup, ...] pattern that EQL handles directly.

## Findings from milestone-3 (AOT-Metal port)

Iter Z's GPU-side wnf state machine + emit was extended with
OP2 + MAT + the new HVM4-port interactions (op2_sup, op2_num_sup,
app_mat_sup) so the IC-native ATP search Term compiles+runs on
Metal.  Battery [wl/Examples/atp_ic/metal.wls](../../wl/Examples/atp_ic/metal.wls):
**6/6 cases match CPU** (was 5/6 before the off-by-1 fix below).

### The off-by-1 (fixed)

Minimal repro (`B4` in
[wl/Examples/atp_ic/debug/bisect_aot_metal.wls](../../wl/Examples/atp_ic/debug/bisect_aot_metal.wls)):

```wolfram
sideSup = TSup[100, TNum[0], TNum[1]];
rSup = TSup[200, TRef["fwd"], TRef["bwd"]];
TOp2["==",
     TIfZero[TOp2["==", sideSup, TNum[0]], TNum[5], TApp[rSup, TNum[5]]],
     TIfZero[TOp2["==", sideSup, TNum[1]], TNum[6], TApp[rSup, TNum[6]]]]
```

CPU collapsed to `{0,0,0,0}`; Metal gave `{0,0,1,1}` -- two
spurious `NUM(1)` leaves.

Root cause was NOT the suspected `app_mat_sup` / `alo_realize`
freshening, and NOT encoder-side DAG sharing.  It was the AOT
emit's DUP memo.  `rSup` tree-expands into two `TSup[200,...]`
copies; each copy inlines `TRef["fwd"]` / `TRef["bwd"]`.  `fwd`'s
body uses its binder twice, so WL's auto-dup rewrote the body
into `DP0/DP1` of a DUP cell.  `metal_emit.c`'s dup memos
(`g_msl_dup_memo`, `g_msl_dup_term_memo`) key on the *book*
`dup_loc` -- but a def inlined at N `TRef` sites re-walks the
SAME book template, so the auto-dup DP0/DP1 carry identical
`dup_loc`s every inlining.  The memo then aliased inlining #2's
`DP0/DP1` onto inlining #1's GPU dup cell (holding inlining #1's
VAR binder), so the second copy of `fwd` ignored its argument
and read the first copy's variable.

Fix: scope both dup memos per `TAG_REF` inline -- save/restore
`g_msl_dup_n` / `g_msl_dup_term_n` around the ref-body emit, so
each inlining gets a fresh memo window.  DP0+DP1 of one dup
still share within the window; distinct inlinings stay
independent.  (`metal_emit.c`, `TAG_REF` case.)  `B4` and the
full atp_ic Metal battery now match CPU; `bisect_aot_metal.wls`
is kept as a regression fixture.

## Findings from milestone-1 prototype

[wl/Examples/atp_ic/transitivity.wls](../../wl/Examples/atp_ic/transitivity.wls)
implements the toy.  Atomic-axiom-only encoding is straightforward
(rewriteFn = TLam[v, TIfZero[TOp2["==", v, old], v, new]]), and
APP-SUP commute correctly fans `TApp[rSup, val]`.  Wnf+collapse
reaches the search leaves.

**Blocker found**: runtime is missing an OP2-SUP interaction for
the *right* argument.  `TOp2[op, NUM, SUP]` heap-exhausts (vs.
`TOp2[op, SUP, NUM]` which works).  Reproduced via `TOp2["+",
TNum[10], TSup[100, TNum[1], TNum[2]]]`.  See
[src/wnf/_.c:618-723](../../src/wnf/_.c) and
[src/interact/dup_op2.c](../../src/interact/dup_op2.c) -- there is
a `dup_op2.c` (DUP-OP2) but no `op2_sup.c` (OP2-SUP).  When
F_OP2_NUM has a SUP `whnf`, the runtime rebuilds the OP2 and
spins.

**Resolution**: ported HVM4's missing interactions directly --
turned out to be two gaps, not one:

1. `src/interact/op2_sup.c` (OP2 with left-arg SUP) and
   `src/interact/op2_num_sup.c` (OP2 with NUM left, SUP right --
   the elegant case: no DUP needed because NUM is atomic).  Wired
   into wnf's TAG_OP2 + TAG_F_OP2_NUM frame handlers.
2. `src/interact/app_mat_sup.c` (APP-MAT with SUP scrutinee).
   thvm's wnf TAG_MAT case only handled NUM and CTR scrutinees;
   SUP fell through to the fallback branch, silently collapsing
   the SUP enumeration.  Without this, sideUpdate's TIfZero on a
   SUP-derived condition always took the fallback path.

With both landed, the toy now runs both-sides rewriting (5/5
including a previously-false-positive unprovable case and a
RHS-only-path test).  See commits 5ba50b25 + 051a9ef7 +
b63464e3.

## Milestones

1. **Toy in WL/CPU** (DONE): WL helper `buildSearchTerm[
   conjecture, axioms, depth]` for the atomic-axiom case (LHS-
   only at first; both sides after the runtime fixes).  Reduce
   via `TWnf` + `TCollapse`, verify `a==c` from `{a==b, b==c}`
   returns True.  ~120 LOC in
   [wl/Examples/atp_ic/transitivity.wls](../../wl/Examples/atp_ic/transitivity.wls).
   5/5 pass after the runtime fixes (transitivity-3,
   reflexivity, symmetry, unprovable, rhs-only-path).

2. **Substitutivity via compile-time position unfold**: extend
   the encoder to walk both lhs/rhs of the conjecture, find
   matching positions per axiom, emit per-position rewrite
   functions.  Verify `f[a]==f[b]` from `{a==b}`.  ~150 LOC.

3. **AOT-Metal pass**: register the toy TDef, dispatch via
   `Method->"Metal"`, validate result matches CPU.  Mostly piggy-
   backs on iter Z+1.  ~50 LOC + whatever iter Z+1 still needs.

4. **`TFindEquationalProof` rebuild** (PARTIAL): the C-ATP
   secondary signal is gone -- the BFS chain synth is now the
   sole provability check.  `synthesizeChain` returns
   `<|Expr, Hist, Closed|>` and `buildProofDataset` returns
   `$Failed` when the chain doesn't close into a tautology
   (instead of emitting a fake Conclusion); `atpEncodeProblem`
   exposes `AxPairs` (held `{lhs, rhs}` pairs) so trivial
   tautology axioms `a == a` survive parameter passing without
   collapsing to `True`.  Verified: trivial-refl, transitivity,
   subst, head-mismatch, sym, backward-needed all give the right
   ProofObject vs `$Failed` answer.

   IC-search provability oracle landed (ATP.wl `=== IC-search
   provability oracle ===` section): `icSearchProvable[conjPair,
   axPairs, depth]` builds a depth-D SUP-fanout search Term and
   decides provability from a NUM(1) leaf in the collapse --
   IC reduction, no WL-side BFS.  Atomic-equational only;
   structured/pattern problems return `$Failed` and fall back to
   the BFS.  Cross-checked in atp.wlt
   (`ATP/IC/oracle-agrees-with-bfs-on-atomic-battery`): the
   oracle's verdict matches the BFS-driven `TFindEquationalProof`
   on every atomic case.  The atp_ic toys' duplicated
   `buildSearchTerm` will be rewired onto this shared builder.

   SUP-path decoder landed (ATP.wl `=== IC-search provability
   oracle ===` + `=== IC-search proof decoder ===`).  It uses a
   FUSED single-Term encoding: one packed-Int state
   `trace*(m*m) + rhs*m + lhs` threads through D depth steps; each
   step fans over `nAct = 4*|axioms|` action lambdas via one SUP,
   and an action `(s, r)` rewrites side `s` with rewrite `r` AND
   folds the choice code into `trace`.  Because the trace rides
   the SAME packed state through the SAME SUP fan-out as the
   atoms, every collapse leaf intrinsically carries both the
   proven bit and its exact choice code -- one collapse, no second
   Term, no skeleton-matching, exact at every depth.  The
   `finalize` lambda maps the leaf state to `proven*big + trace`
   (`big = nAct^depth`); `IntegerDigits` base-`nAct` splits the
   code into per-step `(side, rewriteIdx)`, `icReplayChain`
   replays them into step records, `assembleDataset` (shared with
   the BFS path) builds the ProofObject.

   `TFindEquationalProof` routes atomic problems through this IC
   path.  `icChainClosedQ` replay-verifies the decoded chain as a
   defensive check; non-atomic problems and any non-closing decode
   fall back to the BFS, so no wrong proof is ever emitted.
   atp.wlt `ATP/ICdec/...` covers the decoder (incl. a depth-4
   chain decoded entirely through IC); `ATP/TFEP/...` cover the
   wired `TFindEquationalProof` end to end (69/69).  Action /
   finalize defs use fixed names so re-registration overwrites
   stable slots instead of leaking the 256-slot def table.

   Lazy pruner: the SUP fan-out still encodes every depth-D
   rewrite combination (`nAct^depth` leaves), but `icFindProvenLeaf`
   no longer collapses it eagerly.  It walks the SUP-tree
   depth-first WL-side, `TCnf`-forcing only the head of each
   visited node and short-circuiting at the first leaf >= big (a
   proven leaf).  Descending one child leaves the sibling subtree
   an unforced redex -- so every branch past the first proof is
   never `TCnf`'d, never reduced, never enumerated.  Pruning falls
   straight out of IC's demand-driven reduction (TLazyTake-style);
   no eager `TCollapse`, no runtime-side walker.  A visit cap
   (200k nodes) bounds pathological searches -- hitting it throws
   "icCap" and the caller falls back to the BFS.

   With the pruner, provable conjectures stop at their proof
   instead of materializing the whole tree: depth-6 chains now
   produce a verified ProofObject through the IC path (were BFS
   fallback).  Search cost is "tree position of the first proven
   leaf in DFS order", not `nAct^depth` -- so it depends on rewrite
   ordering, which is what KBO is for (next).

   Outstanding:
   - KBO-order the action list so DFS-left branches are the
     KBO-decreasing rewrites -- puts the proof near the front of
     the DFS, turning the pruner from "bounded" into "fast" at
     depth 5-6 (currently ~4 s, cap-bounded, BFS-backed).
   - Route the IC path through Metal (the fused Term is
     OP2/SUP/LAM/APP only -- all covered by the AOT-Metal emit +
     state machine), so the depth-D enumeration runs on the GPU.

5. **Pattern axioms** (DONE -- not via pre-instantiation, via WL
   Rule semantics).  The original plan was to enumerate
   substitutions over a constant pool, but the BFS already
   pattern-matches via `Position` + `ReplaceAt` once the rules
   are constructed correctly.  Two encoder fixes:
     - `forAllToPattern[axHC]`: strips `ForAll[v, body]` and
       `ForAll[{v, ...}, body]` wrappers, rewriting bound bare
       symbols as `Pattern[v, Blank[]]` so the BFS treats them
       as pattern variables.  Tautologies like `ForAll[x, f[x]
       == f[x]]` are protected by wrapping the body in
       `HoldComplete` before the substitution fires.
     - `oneAxiomRules`: builds `Rule[lhs, stripPatterns[rhs]]`
       so the rule substitutes BARE bound values into the rhs
       (otherwise WL leaks `Pattern[x, Blank[]]` into the
       output, breaking ReplaceAt).
   Doc-form coverage in atp.wlt (`ATP/TFEP/...`):
   pattern-rightId-1use/2uses/verifies, forall-fg-gf-from-fx-gx,
   forall-multi-axiom-instantiated, assoc-rewrite-doc-example,
   forall-tautology-no-axioms, forall-single-1step,
   forall-multi-symmetric, doc-scope-3step-chain,
   doc-scope-insufficient-axioms, plus
   forall-multi-axiom-verifies / assoc-rewrite-verifies for
   ProofFunction round-trip.  53/53 atp.wlt cases pass.

6. **Retire `src/atp/_.c`** (PARTIAL): `TFindEquationalProof`
   no longer calls into the C-side ATP saturator (`$atpRunFn`
   removed from this code path).  `TATP[]` still uses it for
   the lower-level Status/Steps/Rules contract, and
   `thvm_wl_atp_run_file` (Waldmeister .pr ingestion) stays.
   Full removal of `src/atp/_.c` waits on the IC SUP-path
   decoder + an IC-side replacement for the `TATP` Status/Steps
   surface.

Stages 4-6 only after 1-3 land cleanly.

## Milestone 7: completion engine investment

The IC-native path (milestones 1-5) proves ground + shallow-pattern
equational logic.  Hard single-axiom proofs (e.g.
`FindEquationalProof[DoubleNegation, WolframAxioms]` -- a 54-step
proof) need real Knuth-Bendix completion.

`src/atp/_.c` already IS an unfailing-completion engine, and a
more capable one than first assumed: best-first CP selection (the
`--add` / `--mix` heuristics, via INC^k-wrapping + collapse_ordered),
KBO orientation with unfailing fallback, interreduction, trivial-CP
discard, PCL-shaped trace.  The blockers measured on the Wolfram
axiom are *operational*, not algorithmic:

- **Fixed caps** (`thvm.h`): `ATP_MAX_RULES = 256`,
  `ATP_MAX_CPS = 4096`.  Completion of the Wolfram axiom overruns
  both -- CPs past 4096 are silently dropped (`n_cps >= ATP_MAX_CPS
  -> break`), so the engine cannot be complete on hard problems.
- **No GC in the saturation loop**: `thvm_atp_step` only reclaims
  heap on trivially-joined CPs (checkpoint/reset); a long run
  exhausts the 128M-cell GC space (`heap_alloc: from-space
  exhausted` at ~4096 steps).
- **`select_cp` rebuilds** an O(n_cps) SUP-tree + collapse_ordered
  every step -- O(n^2) allocation over a run.

Measured: depth 64 -> 64 rules / CP queue 4096 (capped, no proof);
depth 4096 -> heap exhausted.

### Workstreams (sequenced by enabling-order)

- **7a -- memory** (DONE, commit c500f833): growable rule / CP
  arrays (dropped the 256 / 4096 caps) + GC inside `thvm_atp_step`.
  Verified: the Wolfram-axiom CP queue now grows past 4096 and a
  60s run does not exhaust the heap.

- **7c' -- kill the O(n^2) per-step cost** (NEW, now the blocker):
  measured after 7a -- `thvm_atp_step` runs MaxSteps=24 in ~0s but
  MaxSteps=64 in ~60s.  The per-step cost explodes super-linearly,
  so the engine cannot run deep enough to prove anything hard
  (24 steps -> 24 rules, 613 CPs queued, no proof).  Cause:
  `thvm_atp_select_cp` rebuilds an O(n_cps) `wrapped[]` SUP-tree +
  runs `collapse_ordered` *every step* -> O(steps * n_cps) overall;
  and `generate_cps` is O(n_rules) per step.  Fix: an incremental
  CP priority structure (binary heap keyed on `atp_cp_priority`, or
  a kept-sorted array) so selection is O(log n) per step, not a
  full rebuild.  This must land before 7b/7c -- without it no long
  run is observable.

- **7b -- convergence assessment** (DONE -- verdict: does NOT
  converge).  Measured with the standalone bench
  `tests/test_atp_wolfram_bench.c` (a plain C process with an
  in-loop wall-clock guard -- it cannot orphan a kernel the way
  `timeout`-wrapped `wolframscript` probes did).  Wolfram-axiom
  DoubleNegation completion: at **step 250 the CP queue is
  64,275** and growing ~257/step while the rule set grows ~1/step.
  The queue runs away -- the engine generates critical pairs far
  faster than its redundancy criteria (trivial-join, interreduce,
  subsumption) eliminate them.

  And the per-step cost re-explodes: step 64 was 0.47s (post-7c'),
  step 250 took 87s.  7c' fixed `select_cp`; the new dominant cost
  is the O(n_cps) linear scan in `atp_cp_queue_subsumed` -- every
  newly generated CP is subsumption-checked against the entire
  64k-entry queue, so a step costs O(new_cps * n_cps) ~ millions
  of match attempts.

  Conclusion: completion of a 54-step single-axiom proof is
  blocked on BOTH (a) the CP queue ballooning -- needs stronger
  redundancy (7c) -- and (b) O(n) queue/rule scans -- needs term
  indexing (7d).  Neither alone suffices: 7c keeps the queue
  small, 7d makes whatever scans remain cheap.

  Debug ladder (kept for once the engine converges): WL's own
  `FindEquationalProof[DoubleNegation, WolframAxioms]` ProofDataset
  is a DAG of 34 CriticalPairLemma + 17 SubstitutionLemma + the
  Conclusion; `CriticalPairLemma 1/2` derive from `{Axiom 1}`
  alone.  Prove each lemma at increasing distance -- a failure
  near the axiom is an inference bug, a failure far out a search
  limit.  `SubstitutionLemma 17` is the theorem itself.

### 7b profiler diagnosis (the concrete bottleneck)

`sample` on the bench mid-run (CP queue ~64k) -- top of stack:

    thvm_match            15158   (~91%)
    atp_push_cps_traced    1322
    thvm_rewrite_step       292

Call tree: the `thvm_match` samples are all under
`atp_push_cps_traced`, which runs three per-CP redundancy checks
-- `atp_cp_source_disjoint_connected`, `atp_cp_rule_subsumed`,
`atp_cp_queue_subsumed`.  `atp_cp_queue_subsumed` scans the ENTIRE
CP queue calling `thvm_match` per entry: with n_cps = 64k and
~257 new CPs/step that is ~16M recursive `thvm_match` calls per
step, on deeply-nested Wolfram-axiom terms.  That single O(n_cps)
scan is the wall.

So the inference is correct (group-theory completion proves
`f(a,i(a))==e` in <=20 steps, `test_atp` 8544/8544) -- the
Wolfram axiom is purely a redundancy/indexing scaling wall.

- **7d -- term indexing** (DONE -- `-DATP_FV_INDEX`, default OFF):
  a real subsumption index over the CP queue so
  `atp_cp_queue_subsumed` finds candidates in ~O(1) instead of the
  O(n_cps) `thvm_match` scan.  Resumed as the genuine fix after
  Milestone 8's "structural sharing makes the CP set a free
  discrimination tree" thesis was REFUTED by 8e (thvm has a bump
  allocator, not hash-consing -- a fresh query shares no cells with
  stored CPs; the flat-CpSet shared traversal IS the per-CP scan).

  *Design path.*  Built first as the plan-recommended feature-vector
  (FV) index -- sound monotone integer features (symbol count,
  per-depth CTR profile, term depth) where a generalization is
  componentwise <=, stored in an FV-trie.  MEASURED: on the single-
  symbol Wolfram nand axiom it plateaued at ~47% false-positive
  survival (18.8k of 40.2k queued CPs surviving the filter per
  query) -- a CP whose one side is a bare variable has the size
  profile of its other side alone, so its FV dominates almost every
  larger CP's FV; a size-based FV cannot exclude a small term that
  "could generalize" a large one by shape.  Adding depth-profile
  features did not move it.

  *What shipped: a PERFECT discrimination tree.*  Excluding by SHAPE
  needs a position-keyed symbol test -- a discrimination tree.  The
  plan permitted the deviation "with a strong reason, justified
  against the GC-stability point": the reason is the measured FV
  plateau, and the GC point still holds -- the tree is keyed
  entirely on integer label ids (a CTR's label; a numbered wildcard
  for a variable), not heap addresses, so it needs no fixup under
  the Cheney collector (the only Term-valued storage is each leaf
  record's lhs/rhs mirror, rooted in `thvm_atp_gc_collect`).  The
  tree spans the preorder of the synthetic term `Cp(lhs,rhs)`.  It
  is the *perfect* variant: pattern variables are numbered by first-
  appearance order, so `nand(x,x)` -> `nand *0 *0` while
  `nand(x,y)` -> `nand *0 *1`.  Retrieval walks the flattened
  subject in lockstep with the tree, carrying a per-path binding
  array: a STAR(k) edge with k unbound binds it to the current
  subject subterm, k bound applies only if the subterm `kbo_eq` the
  binding.  This folds full one-way matching -- structure AND
  variable consistency -- into the descent: a stored CP reaches a
  leaf IFF it matches.  Sound (never misses a subsumer): a
  subsuming CP must have all var ids < `REWRITE_MAX_VAR` (else
  `thvm_match` rejects it), and for that matchable subset the
  numbering is exact, so the descent reaches its leaf and never
  prunes it; the leaf still runs the same two-sided `thvm_match`
  as the byte-identity guard.

  *Measured result (cpl1, the rung the 7b profiler ran).*
  Behavior-identical -- `cps=10113` at 100 steps, `cps=40213` at
  200 steps, ON == OFF exactly; `test_atp` 8544/8544 with the flag
  on; `cpgen` still 5 CPs.  Wall time: 200 steps **39.2s -> 4.4s**
  (~9x); at the 120s wall the engine reaches **step 608** vs the
  array scan's **step 258** (2.4x further up the ladder).  The
  index hands `thvm_match` **0.0 candidates/query** -- only 2
  `thvm_match` calls in a 372k-query run -- even at n_cps=371k, so
  retrieval is genuinely O(1), not just a constant-factor cut.
  `sample` mid-run: `atp_cp_queue_subsumed` no longer appears in
  the hot path; the 91% `thvm_match` wall is GONE.  The new
  dominant cost is `thvm_rewrite_step` (the trivial-joinability /
  connectedness normalization) -- a different wall, 7e/8b
  territory.  cpl2 proves in 2 steps either way; subl2/thm are
  normalization-bound from step 1, so 7d is wall-neutral on them
  (index-maintenance overhead is within run-to-run noise --
  `cps=90313` at 300 steps, ~18s ON and OFF).  Gated behind
  `-DATP_FV_INDEX`, independent of `-DATP_CP_GRAPH`; OFF is the
  byte-for-byte milestone-7 array scan.
- **7e (normalization wall) -- the `thvm_rewrite_step` fix**
  (DONE -- two levers; all work assumes `-DATP_FV_INDEX` ON, i.e. 7d
  active).  7d pinned the next wall: with the CP-queue scan gone,
  `sample` shows the hot path is `thvm_rewrite_step`.  A re-diagnosis
  pinned it precisely to `atp_push_cps_traced`'s per-CP filter block,
  which ran **four full `atp_rewrite_normalize` calls per candidate**.
  Each normalize is `O(term_size x n_rules)`: `rewrite_try_top`
  (`src/rewrite/_.c`) tries every rule LHS at the top of a term,
  `thvm_rewrite_step` recurses into every CTR child so that runs at
  every position, and `thvm_rewrite_normalize` restarts from the root
  up to `NORM_CAP=64` times.

  *Lever 1 -- drop the dead counter-only filters* (`-DATP_CP_DIAG`,
  default OFF).  Of the per-CP filters only 7.1 trivial-joinability
  and 7.3b queue-subsumption are real drop filters.  7.2b
  source-disjoint connectedness (`atp_cp_source_disjoint_connected`,
  two normalizes + a malloc) and 7.3a rule-subsumption
  (`atp_cp_rule_subsumed`, an O(n_rules) two-sided match scan) are
  COUNTER-ONLY -- their verdicts only ever tick
  `n_cps_dropped_connected` / `n_cps_dropped_rule_subsumed` and never
  drop a CP.  Both calls now run only under `-DATP_CP_DIAG`; the
  default hot loop skips them.  Behavior-identical (same CPs queued,
  same proof); the functions stay defined for the `test_atp` unit
  tests.  Measured ~2.1x on cpl1 (step 500: 66.7s -> 30.7s, with 7d
  ON, rule index OFF).

  *Lever 2 -- a rule-LHS redex index* (`-DATP_RULE_INDEX`, default
  OFF, independent of every other ATP flag).  `rewrite_try_top`'s
  O(n_rules) linear LHS scan becomes an index lookup.  The index is
  the DUAL of 7d's discrimination tree: 7d indexes `Cp(lhs,rhs)` pairs
  and retrieves a stored pattern subsuming a subject CP; this indexes
  single rule-LHS terms and, at each subject position, retrieves which
  rule LHS one-way matches the subterm there -- the SAME matching
  direction (stored pattern has variables, subject is concrete), so
  7d's perfect-discrimination-tree descent (CTR-exact / first-var-bind
  STAR / repeat-var-`kbo_eq` STAR, the STAR/CTR flat alphabet, the
  preorder flatten with subtree spans) is reused verbatim.  Only the
  insert key (one term, no `Cp` wrapper) and the leaf action (collect
  a rule index) are rewritten.  ATP-side, mirroring the
  `atp_ic_rewrite_step` / `atp_rewrite_normalize_ic` precedent:
  `src/rewrite/_.c` (the shared runtime rewriter) is UNTOUCHED; the
  indexed normalizer (`atp_rewrite_normalize_indexed`) is reached
  through the existing `atp_rewrite_normalize` shim, taken only when
  the call targets the full current rule set
  (`lhs == s->lhs && n_rules == s->n_rules` -- the hot
  trivial-joinability / saturation-step / goal-check path).  The
  index is built lazily over `s->lhs[0..n_rules)` and rebuilt when a
  `rule_index_dirty` flag (set on every `atp_push_rule` append and
  every `interreduce` drop) or an `n_rules` mismatch is seen.

  Behavior-identity subtlety: the mid-completion rule set is NOT
  confluent, so WHICH rule fires changes the normal form.
  `rewrite_try_top` picks the FIRST (lowest-index) matching rule; the
  tree returns leaves in tree order.  So the descent does not stop on
  first hit -- it visits every reachable leaf, runs the SAME one-way
  `thvm_match` as the authoritative guard, and tracks the MINIMUM
  confirmed rule index.  The redex-selection order is the first
  preorder position with a redex -- exactly `thvm_rewrite_step`'s
  top-then-children-left-to-right.  A single flatten per step feeds
  every per-position descent (no O(S^2) re-flatten).

  *Measured result.*  `test_atp` 8544/8544 in default,
  `ATP_RULE_INDEX=1`, and `ATP_FV_INDEX=1 ATP_RULE_INDEX=1` builds;
  `cpgen` still 5 CPs.  Behavior-identical on cpl1 (with both
  indexes): step 250 rules=251 cps=63265, step 500 rules=501
  cps=251515 -- byte-identical to the pre-fix baseline.  Wall: cpl1
  step 250 **8.6s -> 0.2s** (~43x), step 500 **66.7s -> 0.9s**
  (~74x); 1000 steps in 2.8s.  Headline (`ATP_FV_INDEX=1
  ATP_RULE_INDEX=1`, both vs 7d-only): subl2 reaches **1309 steps in
  60s** (vs 482, ~2.7x; ~8.0 -> ~21.8 steps/s); thm **1308 steps in
  60s** (vs 484, ~2.7x); at 120s subl2 ~1469 and thm ~1468.  A
  `sample` mid-run confirms `thvm_rewrite_step` is GONE from the hot
  path -- time is now spread across the 7d FV index, CP enumeration
  (`thvm_unify`), and `gc_evacuate`.

  *NOT a proof -- the engine diverges (convergence is a separate
  follow-up).*  This fix is a pure speedup; it does NOT make subl2 or
  thm prove.  The re-diagnosis found the completion engine is
  *divergent* on these goals: it generates ~753 CPs per step and the
  queue runs away past 250k+ (over 2.1M at 120s).  Each step is now
  2-10x faster, so the engine climbs the ladder further per wall-
  second, but a divergent search never terminates regardless of
  per-step speed.  Making subl2/thm actually prove needs a
  convergence change -- stronger redundancy (7c: full forward/backward
  subsumption, simplify-reflect, blocked-CP deletion to keep the queue
  bounded), a better CP-selection heuristic, or a goal-directed
  strategy -- not a faster inner loop.  7c is the right next lever:
  7d/7e made the per-CP checks cheap, 7c makes them strong enough to
  bound the queue.
- **7c -- canonical variable normalization** (DONE -- `-DATP_VAR_NORM`,
  default OFF).  The convergence re-diagnosis found the real wall was
  not "redundancy too weak" -- it was **the rewriter going DEAD on
  out-of-range variables**, which made redundancy a structural no-op.

  Root cause: `thvm_match` / `thvm_subst_apply` (`src/rewrite/_.c`)
  silently treat any FVR with id `>= REWRITE_MAX_VAR` (=64) as an
  unmatchable constant.  The CP enumerator renames rule j by
  `CP_RENAME_OFFSET` (=32) before unification and BAKES that offset
  into the stored CP -- there was no variable-normalization pass
  anywhere.  Each completion round a stored rule's variables creep up
  by +32, so after ~2 rounds rule/CP variable ids cross 64.  Past that
  cliff the rewriter cannot fire on the rule -- joinability is a no-op,
  subsumption returns 0, interreduction never drops a rule.  ALL
  redundancy dies at once.  Measured (var_norm OFF, 60 steps): rules
  reach `max_var=66` (57 of 60 cross 64), CPs reach `max_var=98`
  (3670 of 3673 cross 64); the first 2000 queued CPs collapse to **16
  distinct** modulo renaming -- a 99.2%-duplicate queue the dead
  matcher cannot detect.  Over a 1308-step run only **3** CPs were
  ever dropped joinable and **1** queue-subsumed; `rules` tracked
  `steps` 1:1 and the queue ran away past 1.7M.

  The fix: `thvm_normalize_vars` (`src/unify/_.c`) canonically
  renumbers a stored (lhs, rhs) pair -- a deterministic preorder walk
  (lhs fully, then rhs) assigns each DISTINCT variable a dense id
  `0,1,2,...` by first occurrence, with the two sides SHARING the
  numbering (a variable in both is one variable).  It is
  alpha-renaming: meaning preserved, and alpha-equivalent rules/CPs
  become BYTE-IDENTICAL.  Applied at every storage point:
  `atp_push_cps_traced` (a CP before it lands in the queue),
  `atp_push_rule` (a rule before it enters R, axiom path included via
  `thvm_atp_add_equation`).  Every stored rule/CP then carries dense
  ids `[0, k)` -- nothing crosses 64, the matcher never goes dead.  A
  cheap **duplicate-rule guard** in `atp_push_rule` rejects a rule
  already in R (both sides `kbo_eq`) -- belt-and-suspenders against
  the "add the same rule 300x" pathology now that alpha-equivalent
  rules are byte-identical.

  Result (`ATP_FV_INDEX=1 ATP_RULE_INDEX=1 ATP_VAR_NORM=1`): the
  redundancy criteria fire.  At step 250 of completion `joinable`
  drops jump from 3 (whole prior run) to **5720**, `queue-subsumed`
  from 1 to **1636**; `rules` grows ~0.7/step (192 rules at step 250)
  instead of 1:1; **0 duplicate rules** in R.  The inference ladder:
  `cpl1` PROVED in **13 steps** (was divergent, never proved),
  `cpl2` PROVED in 2, `subl2` PROVED in **14 steps** (was divergent).
  The queue stays tiny (cps ~300-360) on the lemmas instead of
  running away to 1.7M.

  *The milestone target `thm` (54-step DoubleNegation) still does NOT
  prove.*  var_norm makes redundancy fire and slows the divergence,
  but `thm`'s queue still grows -- step 250 cps=60,882, step 500
  cps=200,651 (~560/step net) -- and the run exhausts even the
  256M-cell heap (with or without the Cheney GC, since the live set
  itself is unbounded).  So 7c is necessary and unblocks the shallow
  ladder, but not sufficient for the deep target: `thm` needs a
  further convergence lever -- a stronger CP-selection heuristic, a
  goal-directed strategy, or simplify-reflect / blocked-CP deletion
  -- on top of the now-working redundancy criteria.

  Gated behind `-DATP_VAR_NORM` (default OFF -- the all-flags-off
  build stays the milestone-7 engine for A/B).  CHANGES BEHAVIOR by
  design: the search trajectory differs because the redundancy
  criteria now actually fire.  test_atp stays 8544/8544 on the flag;
  `atp/orient-and-add-kbo-un-pushes-both` was updated -- under
  var_norm the two orientations of an unorientable equation
  `x = y` both renumber to `(v0, v1)` and the duplicate guard
  collapses them to one rule (behavior-neutral for the rewriter:
  byte-identical rules are indistinguishable to it), so that test now
  asserts one stored rule plus that a re-add stores nothing.
- **7e -- wire-through**: route `TFindEquationalProof`'s structured
  problems through completion; decode the PCL trace into a
  verifier-passing `ProofObject`.  (Also fixes the bound-symbol
  encoder bug: a caller-bound `wax = ForAll[...]` reaches the
  HoldAll surface as a held symbol -- resolve via `OwnValues`
  before `forAllToPattern`; or callers use `With[{l = ax}, ...]`
  to inject the value past HoldAll.)

This supersedes milestone 6: `src/atp/_.c` is NOT retired -- it is
the hard-proof engine.  The IC path stays for the easy/shallow
class it already handles well (and is GPU-targetable).

## Scope cuts (v1)

- No INC-labels-as-precedence yet.  Branch order is left-first
  in the SUP-tree (matches `TCollapse` enumeration).
- No KBO.  Depth-bounded BFS-equivalent via SUP fan-out.
- No critical-pair generation.  We don't need it: the SUP
  enumerates rewrite *applications*, not derived rules.
- No infinite-rule support.  Depth bound caps the search.
- No witness extraction (existential goals).  Defer to v2.

## Milestone 8: IC-native completion -- the CP set as a live SUP-graph

Milestone 7 located the wall precisely.  The engine's inference is
correct -- group-theory completion proves `f(a,i(a))==e` in <=20
steps, `cpgen` derives WL's distance-1 `CriticalPairLemma 1/2`
exactly, `test_atp` is 8544/8544.  What drowns the Wolfram axiom is
an un-indexed redundancy scan: `atp_cp_queue_subsumed` calls
`thvm_match` against every one of ~64k queued critical pairs (CPs)
each step -- 7b measured ~16M match calls/step, 91% of runtime.
7d's planned answer was a bolt-on discrimination tree.

Milestone 8 takes the IC-native answer instead, because a
maximally-shared SUP-graph IS a discrimination tree.  Both exploit
structural sharing for fast candidate retrieval -- a discrimination
tree shares term *prefixes*; a hash-consed term DAG shares term
*subterms*.  thvm already hash-conses every cell.  So if the whole
CP set is ONE shared Term rather than ~64k disjoint array entries:

- normalizing the CP set under a newly oriented rule is ONE
  reduction whose optimal sharing rewrites each distinct redex
  once, not once per CP containing it;
- a subsumption query is ONE match traversal that shares
  per-subterm match work across every CP, not n disjoint scans;
- exact-duplicate CPs annihilate for free via same-label DUP-SUP;
- trivially-joined CPs collapse to ERA and vanish from the graph
  during normalization -- no external discard pass.

This is the milestone the whole arc was built toward.  The toy IC
mechanisms prototyped in `ATP.wl` -- SUP fan-out search, the
INC-priority `collapse_ordered`, the `icFindProvenLeaf` lazy
pruner -- become the engine's actual data structures in
`src/atp/_.c`.

### Representation

Today `AtpState` (`src/thvm.h`) holds the CP set as five parallel
growable arrays: the *term payload* `cp_lhs` / `cp_rhs` (`Term`)
and the *metadata* `cp_trace` / `cp_pri` / `cp_seq` (`u32`), with
`n_cps`.  `cp_trace` is an index into the `trace[]` derivation
log, not a Term; `cp_pri` / `cp_seq` are the 7c' binary-heap key.
Milestone 8 replaces only the term payload:

    Term cp_graph;   // SUP-tree; each leaf a Cp[lhs,rhs] CTR node

The `u32` metadata arrays are cheap and not the bottleneck -- they
stay as side arrays indexed by leaf order.  The sharing wins
8b/8e need operate on the lhs/rhs *terms*, which is exactly what
`cp_graph` carries.

Each leaf is a 2-child CTR `Cp[lhs,rhs]`.  Because thvm
hash-conses every cell, two CPs sharing a subterm share its heap
cells with no extra bookkeeping -- `cp_graph` is already a
maximally-shared DAG.  There is no reusable "fused decoder":
`ATP.wl`'s IC proof decoder packs *integers*
(`trace*(m*m)+rhs*m+lhs`), not CP terms; the C side gets a fresh
`Cp[lhs,rhs]` codec.

Priority migrates into the graph structurally only at 8d -- a CP
of priority k wrapped in INC^k, the combinator `collapse_ordered`
already reads.  Until 8d, selection stays the 7c' binary heap
over the unchanged `cp_pri` / `cp_seq` side arrays.

### Workstreams (sequenced; each independently bench-validatable)

- **8a -- introduce `cp_graph` behind `-DATP_CP_GRAPH`** (pure
  representation swap; bit-identical).  Add `Term cp_graph` to
  `AtpState`, a 2-child `Cp[lhs,rhs]` CTR leaf codec, and the
  build flag.  Under the flag every CP mutation
  (`thvm_atp_add_equation`, `atp_push_cps_traced`, the `select_cp`
  pop) maintains `cp_graph` in lockstep and the engine reads CP
  terms from it; the `cp_lhs` / `cp_rhs` arrays are kept as a
  synced mirror so `tests/test_atp.c`'s ~30 direct `s->cp_lhs[i]`
  / `s->n_cps` reads need no edits.  Selection stays the 7c' heap
  over `cp_pri` / `cp_seq` -- INC-priority is 8d, not here.  A
  debug assertion checks `decode(cp_graph)` equals the mirror
  every step.  Flag OFF: today's code, byte-for-byte.
  Acceptance: with the flag ON, identical proof output AND
  identical step counts on `test_atp` (stays 8544/8544) and
  `bin/test_atp_wolfram_bench cpgen` (identical 5-CP set).

- **8b -- shared normalization** (the headline optimal-sharing
  win -- and a deliberate semantic change, NOT a pure swap).
  Today normalization is lazy: `thvm_atp_step` runs
  `atp_rewrite_normalize` only on the *popped* CP.  8b adds
  `atp_normalize_graph(cp_graph, rules)` -- one IC reduction of
  the whole graph when a rule is oriented, so optimal sharing
  contracts each distinct redex once even though it occurs in
  many CPs.  Eagerly normalizing queued CPs changes which become
  trivial-joined when, so 8b is gated on *proof still found* +
  bench cost, NOT on bit-identical step counts (that gate was
  8a's alone).

- **8c -- trivial-join + simplify-reflect by reduction**.  Install
  a built-in reflexivity rule `Eql[x,x] -> ERA`.  During 8b's
  single normalization sweep any CP whose two sides converge
  becomes `Eql[t,t]`, reduces to ERA, and drops out of the graph
  -- the trivial-CP discard and simplify-reflect criteria fall
  out of reduction for free, shared across the whole set.

- **8d -- selection via INC-priority lazy collapse**.  Port the
  `icFindProvenLeaf` lazy-pruner mechanism into C
  (`src/collapse/ordered.c` already has `collapse_walk_pri`).
  Selection = find the minimum-INC-depth leaf of `cp_graph`
  without materializing the rest (lazy take); the selected leaf
  is rewritten to ERA and pruned at the next 8b sweep.  `cp_seq`
  becomes a secondary INC label for tie-breaking.  Retires the
  7c' heap on the IC-native path (it stays the array-engine
  fallback).

- **8e -- SUP-aware matching: the 91%-killer**.  Add
  `thvm_match_multi(query, pattern_graph)` -- `thvm_match` plus
  (a) a SUP-fork case (at a SUP node in the pattern graph the
  match forks left/right against the *same* shared subject node,
  so the subject traversal up to the fork is done once) and (b) a
  `(pattern_cell, subject_cell) -> result` memo so a subterm
  shared by many CPs is matched against a given query subterm
  exactly once.  Forward subsumption and backward subsumption
  each become ONE `thvm_match_multi` call against `cp_graph`
  instead of an O(n_cps) loop.  On the Wolfram axiom, where CPs
  share deep nand-nested subterms, this turns the 16M-call scan
  into roughly O(distinct subterm pairs).  Backward-subsumed
  leaves are rewritten to ERA and pruned at the next 8b sweep.

  **8e RESULT (negative -- the milestone go/no-go signal).**  8e
  shipped as `thvm_match_multi`, routing the forward-subsumption
  scan in `atp_cp_queue_subsumed` through ONE fan-out traversal of
  `cp_graph`.  The `(pattern_cell, subject_cell)` memo from part
  (b) was built, instrumented (`-DATP_MATCH_STATS`), and measured.
  It does **not** work on the flat `CpSet` container, for two
  reasons found by reading + measuring the code, not predicted:

  1. thvm has a **bump allocator, not hash-consing** -- the plan's
     "thvm hash-conses every cell" premise is factually wrong.
     `term_new_ctr` (`src/term/new_ctr.c`) always allocates a
     fresh cell, so two structurally-equal CTR subterms are the
     same `Term` value only when the literal same cell is reused.
     The 8b normalization memo still gets 52% sharing (its key is
     a single cell and CP generation reuses pass-through cells),
     but the 8e memo keyed on a `(P,S)` *pair* needs both cells to
     recur together.
  2. On the flat `CpSet[Cp,Cp,...]` every CP leaf has a **distinct
     top cell**, so `thvm_match` discriminates at the leaf root
     and fails fast *before* descending to any deep subterm CPs
     might share.  A discrimination tree shares term *prefixes*
     (the root-anchored test); a flat hash-consed DAG of
     disjoint-headed CPs shares *subterms* but no prefix.  The
     fan-out re-traverses the subject in full per leaf.

  Measured hit rate on `cpl1`: **0.0%** -- 0 hits over 3.25
  billion subterm-pair lookups at 200 steps, three independent
  measurements.  The memo's per-node hash + probe + partial-subst
  bookkeeping was pure overhead; with it, `cpl1` ran ~16x SLOWER
  than 8b.  Per the plan's "degrades gracefully ... no worse than
  today" requirement, 8e ships **memo-free**: `thvm_match_multi`
  is the fan-out over plain `thvm_match`, which IS the per-CP
  scan -- behavior-identical (`cpl1` `cps=10108`/`40208` at
  100/200 steps, matching 8b) and 8b-cost (`cpl1` 100 steps 4.1s
  vs 8b 3.9s; 200 steps 45.2s vs 8b 45.1s).  `thvm_match` remains
  79.6% of runtime -- the 91% wall is unmoved.

  **Conclusion for the milestone thesis:** the shared-graph
  representation does NOT deliver candidate-retrieval speedup *as
  a flat `CpSet`*.  Maximal subterm sharing is real (8b's 52%) but
  irrelevant to a root-anchored subsumption match -- that needs
  *prefix* sharing, which only the SUP fan-out **container** of
  8f provides.  8e was sequenced first to validate the thesis
  before investing in 8c/8d; the validation says: do not expect
  8e-style matching to pay off until 8f restructures `cp_graph`
  from a flat list into a discrimination-tree-shaped SUP DAG.
  The "a maximally-shared SUP-graph IS a discrimination tree"
  claim holds only if the graph is *built* prefix-first; the
  flat 8a container is not.

- **8f -- CP generation as SUP fan-out + rule set as a shared
  DAG**.  Rewrite `thvm_critical_pairs` (`src/cp/_.c`) so
  superposition of a new rule against the rule set emits its
  overlap candidates as a SUP node, appended to `cp_graph` in
  O(1) (wrap old root + new SUP in a fresh SUP).  The rule set
  becomes a shared `rule_graph` DAG.  Then wire-through (was
  7e): route `TFindEquationalProof` through the IC-native engine
  and decode the trace into a verifier-passing `ProofObject`
  identical to WL's `FindEquationalProof`, fixing the
  bound-symbol encoder bug noted in 7e.

### SUP-label management (the hard part -- called out explicitly)

DUP-SUP annihilation is free duplicate elimination ONLY when
duplicate CPs carry the same SUP label and genuinely distinct CPs
carry different ones.  Labels are a bounded field, so a perfect
"label = identity of the CP" scheme is impossible.  The plan does
not try.  Instead:

- **Structural dedup via labels**: CPs from the same overlap
  family -- same `(rule_i, rule_j, position)` superposition --
  share a label derived from a hash of that triple.  Re-deriving
  an overlap (the most common duplicate source) annihilates on
  contact.  This is the cheap, reliable 80%.
- **General subsumption modulo a substitution stays a
  discriminating walk** -- it is NOT free from sharing.  8e makes
  that walk cheap by sharing per-subterm match work and by only
  ever walking *candidates* the shared-graph traversal surfaces,
  never the full 64k.  The honest claim is "indexing for free,
  the sigma-check still runs -- but only on candidates."

### Risks and fallback

This is the research bet of the arc, stated plainly:

- The optimal-sharing win in 8b is real and low-risk -- it is the
  runtime's core guarantee.  8c falls straight out of 8b.
- 8e's payoff depends on CPs actually sharing subterms.  On the
  Wolfram axiom they demonstrably do (deep shared nand nests);
  on a problem whose CPs are structurally disjoint the memo buys
  little and `thvm_match_multi` degrades gracefully to the
  per-CP scan -- no worse than today.
- SUP-label collisions would mis-annihilate distinct CPs ->
  incompleteness.  Mitigation: the label hash is dedup-only; a
  collision drops a CP that is then re-derived from its overlap
  family, so it is a *performance* bug, not a soundness bug.
- Fallback: every workstream is gated; the milestone-7 array
  engine remains the default (`-DATP_CP_GRAPH` off) and the
  regression oracle.  8 ships only when the bench shows the
  IC-native path proving a lemma the array engine cannot.

### Sequencing and validation

8a -> 8b -> 8c -> 8d -> 8e -> 8f, each gated by:

1. `test_atp` stays 8544/8544 -- a correctness gate for every
   workstream (inference is unchanged throughout).
2. `cpgen` still derives `CriticalPairLemma 1/2` exactly.
3. `tests/test_atp_wolfram_bench.c` -- the orphan-free harness --
   shows the intended cost drop for that workstream (8b:
   normalization cost; 8e: the 91% match scan collapses).
4. Bit-identical step counts are required of **8a only** -- it is
   a pure representation swap.  8b onward deliberately change the
   search (eager normalization, stronger redundancy) and are
   gated on *proof still found* + cost, not step-count identity.
5. Once 8e lands, climb the 7b debug ladder: prove `cpl1`,
   `cpl2`, `subl2` at increasing distance from the axiom; a
   failure near the axiom is an inference regression, far out a
   remaining search-limit.  `thm` (the 54-step DoubleNegation
   target) is the milestone-8 acceptance proof.

This supersedes the bolt-on 7d term index: 8e delivers the same
candidate-retrieval speedup as an emergent property of the shared
representation, with nothing extra to keep coherent.

## Milestone 9: Waldmeister convergence levers

After 7c the engine *converges* on the shallow ladder (`cpl1`,
`cpl2`, `subl2` all prove) but `thm` -- the 54-step DoubleNegation
target -- still diverges: the CP queue runs away ~560/step and
exhausts the heap.  A diagnosis found this is not a single wall but
two: redundancy fires (joinability drops jumped to ~20k after 7c)
yet cannot bound *this* search, and CP selection is goal-agnostic
(`cpl1` / `subl2` / `thm` traced byte-identical trajectories,
because the goal only gated the goal-check).

To pin what bounds a divergent completion, the bundled Waldmeister
source (`waldmeister/`, the Kaiserslautern unfailing-completion
prover) was studied directly.  Techniques our engine lacked:

- **Goal-directed CP selection** -- `CPinGoal` / `GoalinCP`
  (`Clas_CP_Goal.c`): weight a CP by structural match against the
  conjecture, so the search steers toward the goal.
- **Ground-joinability** -- the Martin-Nipkow redundancy criterion
  (`Grundzusammenfuehrung`, "basic merging"): discard a CP joinable
  for all ground instances even when not directly joinable.
- **Orphan deletion** -- `Waisenmord` ("orphan murder"): when a
  parent rule is interreduced away, kill its descendant CPs in the
  queue.
- Periodic interreduction of the passive CP set; a killer-criterion
  classification layer (`Crit_*` / `Act_*`); LPO as an alternative
  ordering (the docs stress the ordering is the single biggest
  lever).

### 9a -- goal-directed CP selection (DONE)

`-DATP_GOAL_HEURISTIC` adds a goal-relatedness term to
`atp_cp_priority`: a CP whose non-trivial subterms one-way-match a
non-trivial goal side is preferred.  The score is a **bounded
additive** penalty (0 / 2 / 5 for double / single / no match), not
a multiplier: a measured x24 multiplicative factor diverged `subl2`
by letting large goal-resembling CPs leapfrog small ones, so the
penalty is capped to reorder only within a few size-units.  Result:
`test_atp` 8544/8544, `cpl1` 13->11 steps, `cpl2` 2->1, `subl2`
14->22 (all still prove fast); `thm` queue ~3-7% smaller but still
divergent.  Selection alone cannot bound a runaway queue -- that is
levers 9b (orphan deletion) and 9c (ground-joinability).

### 9b -- orphan deletion / Waisenmord (DONE)

`-DATP_ORPHAN_KILL`.  When `thvm_atp_interreduce` drops a rule, the
queued CPs descended from it are redundant -- the re-queued reduced
equation regenerates whatever they would contribute.  The trace DAG
makes this cheap: each dropped rule's trace id is captured, and a CP
is an orphan iff its `TRACE_CP` entry names a dead rule as a parent.
Orphans are compacted out of the queue; the heap / FV index /
cp_graph are rebuilt via `thvm_atp_cp_reheapify`.  Result: `test_atp`
8544/8544, the ladder still proves (cpl1 12, cpl2 1, subl2 21);
`thm` still divergent -- orphan deletion is bursty (it fires only on
rule drops) and the trajectory churn roughly cancels the gain on
this single-symbol problem.  9a + 9b together do not crack `thm`;
the strong *steady* redundancy lever is 9c (ground-joinability).

### 9c-foundation -- order-aware rewriting (DONE)

`-DATP_ORDERED_REWRITE`.  Investigating 9c revealed the engine had no
order-dependent rewriting at all: `orient_and_add`'s `KBO_UN` branch
stored an unorientable equation `u = v` as TWO looping rules `u->v`
and `v->u`.  That hack is both a queue-blowup source (doubled rules
-> doubled CP generation) and the reason classical ground-joinability
could not bolt on (no per-order rewriting to case-split).

Proper unfailing-completion rewriting: an unorientable equation is
stored ONCE, and the rewrite step tries every rule in both directions,
applying a direction only when it strictly decreases the redex in the
reduction order.  An oriented rule (`l > r`) is decreasing for every
instance, so it fast-paths forward with no order check; only a genuine
equation pays the both-directions order-gated path.  Variable-safety
gates each direction.  Terminating (every rewrite descends a
well-founded order).

Result: `test_atp` 8544/8544, the ladder still proves.  A correct
general engine improvement -- but, measured, a **no-op for `thm`**:
the ordered-rewriting trajectory is byte-identical to the non-ordered
engine (step 250: `rules=191 cps=59892` in both).  `KBO_UN` never
fires for the Wolfram axiom under this KBO -- one symbol, weight 1, so
every derived rule orients by symbol count -- hence the both-ways hack
was never triggered and there are no unorientable equations to rewrite
in order.  Classical ground-joinability (case-split on variable
orderings) is likewise vacuous here.  The `thm` queue genuinely
diverges: an *accelerating* ~189 -> ~347 -> ~356 CPs/step -- a hard
completion explosion, not a both-ways artefact.  (An earlier draft of
this note misread an early-phase cps average as a rate drop -- the
unindexed ordered rewriter is ~10-20x slower per step, so the
early-phase sample looked smaller; corrected.)

9c proper (ground-joinability) thus cannot help `thm` either.  The
levers that remain are the reduction ordering itself -- Waldmeister's
most-emphasized control, repeatedly deprioritized here -- or a
goal-directed proof procedure instead of blind saturation.

### Ordering experiment -- and the conclusion for `thm`

The bench gained an `ATP_BENCH_ORD` switch (`kbo` default, `kbo0` =
KBO with var_weight 0, `lpo` = lexicographic path ordering -- the
`thvm_lpo` engine code already existed).  Measured on `thm`:

| ordering | step 250            | step 500              |
|----------|---------------------|-----------------------|
| kbo      | rules=191 cps=59892 | rules=357 cps=208628  |
| kbo0     | rules=191 cps=59892 | rules=357 cps=208628  |
| lpo      | rules=191 cps=64312 | rules=357 cps=218241  |

`kbo0` is byte-identical to `kbo`; `lpo` derives the *same* rule
counts but slightly *more* CPs.  The ordering lever does not help
`thm` -- with a single function symbol the ordering space is too
constrained for the KBO/LPO swing the Waldmeister docs describe (their
decisive examples are multi-symbol: distributivity, a precedence
between + and x).

**Conclusion.** `thm` -- saturating the Wolfram NAND axiom to
DoubleNegation -- is not reachable by this saturation-based completion
engine.  Every lever has been measured and exhausted: redundancy
(9a/9b), ordered rewriting + ground-joinability (9c -- vacuous, no
unorientable equations arise), and the reduction ordering (above).
The CP queue diverges with an accelerating rate under all of them.
`thm` needs a fundamentally different proof procedure: goal-directed
proof *search* from the conjecture (the way WL's `FindEquationalProof`
finds its 54-step proof), not blind forward saturation.  That is a
separate engine, not a lever -- the honest stopping point for this
arc.

What the arc delivered: an equational ATP that went from diverging on
the distance-1 lemma to proving `cpl1`/`cpl2`/`subl2`, ~3-74x faster
(7d subsumption index + the normalization-wall fix), with goal-directed
selection (9a), orphan deletion (9b), and order-aware rewriting
(9c-foundation) -- all sound, all flag-gated, `test_atp` 8544/8544
throughout.
