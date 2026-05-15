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

   Outstanding: decode the winning leaf's SUP-path into a chain
   of (axiom, direction, side) records so the IC search produces
   a full verifier-ready ProofObject -- then `TFindEquationalProof`
   can route through IC reduction (CPU or Metal) instead of the
   BFS.  Until that decoder lands, the BFS still synthesizes the
   chain; the IC oracle is a sound provability cross-check.

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

## Scope cuts (v1)

- No INC-labels-as-precedence yet.  Branch order is left-first
  in the SUP-tree (matches `TCollapse` enumeration).
- No KBO.  Depth-bounded BFS-equivalent via SUP fan-out.
- No critical-pair generation.  We don't need it: the SUP
  enumerates rewrite *applications*, not derived rules.
- No infinite-rule support.  Depth bound caps the search.
- No witness extraction (existential goals).  Defer to v2.
