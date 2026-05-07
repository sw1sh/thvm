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

4. **`TFindEquationalProof` rebuild**: replace the BFS chain
   synth with the IC search.  Keep `ProofObject` shape stable so
   `ProofFunction` still verifies.  Decode the winning leaf's
   SUP-path into a chain of (axiom, direction, side) records that
   feed `chainEntry`.  ~200 LOC.

5. **Pattern axioms via pre-instantiation**: extend the encoder
   to enumerate substitutions for pattern-var axioms.  Validates
   the commutativity/associativity battery cases.  ~100 LOC.

6. **Retire `src/atp/_.c`**: once the IC path covers the same
   cases at competitive speed, mark the C ATP for removal.  Keep
   `thvm_wl_atp_run_file` (Waldmeister .pr ingestion) since it's
   useful infra.

Stages 4-6 only after 1-3 land cleanly.

## Scope cuts (v1)

- No INC-labels-as-precedence yet.  Branch order is left-first
  in the SUP-tree (matches `TCollapse` enumeration).
- No KBO.  Depth-bounded BFS-equivalent via SUP fan-out.
- No critical-pair generation.  We don't need it: the SUP
  enumerates rewrite *applications*, not derived rules.
- No infinite-rule support.  Depth bound caps the search.
- No witness extraction (existential goals).  Defer to v2.
