# Normal-form reducers

Four reducers, layered.  Each is the "right primitive" for a different
caller: pick the shallowest one that exposes what you need.

| Reducer | Source | What it exposes | What it doesn't |
| ------- | ------ | --------------- | --------------- |
| `wnf(t)` | [src/wnf/_.c](../src/wnf/_.c) | head WHNF of the spine | redexes inside arguments; DUP-XXX through compound bodies (Levy-opaque) |
| `cnf(t)` | [src/cnf/_.c](../src/cnf/_.c) | head WHNF with SUPs lifted to the top; DUP-XXX fired during readback | redexes that aren't on the path to lifting a SUP |
| `thvm_collapse(t)` / `thvm_collapse_ordered(t)` / `eval_collapse(t)` | [src/collapse/](../src/collapse/), [src/eval/collapse.c](../src/eval/collapse.c) | the set of pure leaves of the SUP-tree (multi-valued evaluation) | nothing new beyond `cnf`; this is a walk of cnf-driven heads |
| `nf(t)` | [src/wnf/nf.c](../src/wnf/nf.c) | every redex fired (parallel pool drain) | --- |

Use the shallowest one that exposes the work you need.  `wnf` is right
when only the head matters (OP2 strict eval, APP-LAM re-entry).  `cnf`
is right when a caller needs SUPs surfaced for branching (collapse, MAT
dispatch on a DP-headed scrutinee).  `thvm_collapse_ordered` (or
`eval_collapse`) is right when a caller wants every multi-valued leaf
enumerated with `TAG_INC` driving the priority.  `nf` is right when a
caller wants every reachable redex fired -- inspection of a value, or
to drive a redex-free graph into `materialize` for kernel emission.

The wnf path is modeled on HVM4's `clang/wnf/_.c`; the cnf path on
HVM4's `clang/cnf/_.c`; `thvm_collapse_ordered` mirrors HVM4's priority
collapse; the parallel `nf` pool is thvm's own (HVM4 has no equivalent
because nothing in core IC needs an off-spine drain).

## `wnf`: spine reducer

```
enter:                          apply:
   walk left until WHNF             pop frames LIFO
   pushing eliminator               dispatch each frame's
   frames as we go                  active-pair interaction
```

State lives in `TContext`:

```c
ctx->wnf_stack            // Term[]; eliminator frames
ctx->wnf_s_pos            // current top
ctx->wnf_last_stack       // snapshot for step-bounded bail
ctx->wnf_last_stack_len
```

`WNF_STACK` and friends in the source resolve to those fields via
macros.  We snapshot `base = WNF_S_POS` at entry so a nested `wnf()`
call (e.g. from a frame that needs to drive an argument to WHNF first
-- `TAG_UOP / UOP_ASSIGN`, `TAG_APP` over `TAG_MAT`, `TAG_F_UOP_CHILD`)
still terminates at its own base.

The step-bounded form `wnf_n(t, max_steps)` checks the budget just
*before* each interaction-firing site (every place the reducer would
bump `ITRS`).  Free reductions -- `VAR`-SUB / `DUP` follows, frame
pushes -- always run, so a single step resolves to a meaningful WHNF
rather than stopping mid-deref.  On bail, the still-pending eliminator
frames are snapshotted into `WNF_LAST_STACK` (innermost first) and the
reducer unwinds via the same "stuck term" path the apply loop uses for
non-redex pairs.  Heap mutations from already-fired interactions
stick, so calling `wnf` again on the returned root resumes the
reduction.

### Enter dispatch

| Tag | Action |
| --- | ------ |
| `TAG_VAR` | unsubstituted -> WHNF; substituted -> strip SUB, re-enter the value |
| `TAG_DP0`/`TAG_DP1` | grad-flag -> dispatch to grad-fwd/bwd; otherwise SUB-follow or push DP frame + descend into cell body |
| `TAG_BJ0`/`TAG_BJ1` | book-time projection -- Levy-opaque under wnf; SUB-follow only, then WHNF |
| `TAG_APP` | push APP frame, descend into function |
| `TAG_DUP` | walk past (DUP is a carrier, not an eliminator) |
| `TAG_REF` | `alo_realize` the named definition; no ITRS bump (structural) |
| `TAG_ALO` | `alo_force` one layer; no ITRS bump (structural) |
| `TAG_UOP` | UOP_KERNEL -> `interact_kernel`; UOP_ASSIGN -> resolve src+dst then `interact_assign_with`; otherwise scan for active children, push F_UOP_CHILD frame for the first one |
| `TAG_OP2` | push OP2 frame, descend into x |
| `TAG_EQL` | push EQL frame, descend into a |
| `TAG_AND` / `TAG_OR` / `TAG_WHEN` | push frame, descend into the strict slot (a / a / cond) |
| `TAG_ANN` | push ANN frame, descend into typ (the strict slot) |
| `TAG_DSU` / `TAG_DDU` | push frame, descend into label |
| `TAG_LAM` / `TAG_ERA` / `TAG_SUP` / `TAG_MAT` / `TAG_BRI` / `TAG_INC` / default | already WHNF |

`TAG_INC` is a passive priority wrapper -- wnf leaves it alone; only
the collapse layer observes it.

### Apply dispatch

Each frame pops LIFO and dispatches on the just-reduced `whnf`'s tag.
Three outcomes per pop:

- **produce a new term to reduce** (`goto enter` with the result):
  used when the result is structurally new and needs head-reduction
  again, e.g. APP-LAM returning a body that may itself be reducible.
- **produce a fresh WHNF directly** (`continue` with the result as the
  new `whnf`): used when the result is a leaf or constructor that the
  next frame can dispatch against without re-entry, e.g. APP-ERA
  returning ERA.
- **get stuck**: no interaction matches the active pair.  Put the WHNF
  back into the original heap cell and treat the frame itself as the
  new WHNF.  The result is a well-formed but unreduced node.

The full apply dispatch:

| Frame | WHNF tag | Action |
| ----- | -------- | ------ |
| APP | LAM | `interact_app_lam`, re-enter |
| APP | ERA | `interact_app_era`, continue |
| APP | BRI | `interact_app_bri`, re-enter |
| APP | PRI | `interact_app_pri` (accumulate arg into PRI's buffer), re-enter |
| APP | SUP | `interact_app_sup` (APP-SUP commute), re-enter |
| APP | MAT | resolve arg (cnf if DP-headed) and dispatch APP-MAT-NUM / APP-MAT-CTR (destructure with cell reuse) / APP-MAT-SUP (commute) / fallback |
| APP | other | stuck, rebuild |
| DP0/DP1 (grad-flag) | --- | `interact_grad`; stuck if `y` can't pattern-match yet |
| DP0/DP1 | SUP | `interact_dup_sup` (annihilate same-label, commute different-label), re-enter |
| DP0/DP1 | ERA | `interact_dup_era`, continue |
| DP0/DP1 | LAM / BRI / CTR | `interact_dup_lam` / `_bri` / `_ctr`, re-enter |
| DP0/DP1 | NUM / ANY / TEN | `interact_dup_num` / `_any` / `_ten`, continue |
| DP0/DP1 | UOP | `interact_dup_uop` (commute through lazy UOP; stuck for active UOP_KERNEL / UOP_ASSIGN) |
| DP0/DP1 | OP2 / MAT / EQL / AND / OR / WHEN / ANN / DSU / DDU / INC | generic n-ary commute via `interact_dup_nod` (matches HVM4's `wnf_dup_nod`) |
| DP0/DP1 | APP | **intentionally stuck** -- HVM4's `// DO NOT ADD` comment: eager DUP-APP would duplicate a beta-equivalent redex |
| DP0/DP1 | other (REF/ALO/VAR/another DP) | stuck, rebuild |
| OP2 | DP-headed | drive through `cnf` first |
| OP2 | NUM (and `y` is NUM) | fire op inline -- emits `RULE_OP2_NUM_NUM` |
| OP2 | NUM (and `y` not yet NUM) | stash WHNF'd x at `heap[loc+0]`, push F_OP2_NUM frame keyed on `loc`, descend into y |
| OP2 | SUP | `interact_op2_sup` (clone y at SUP's label) |
| OP2 | other | stuck, rebuild |
| F_OP2_NUM | DP-headed | drive through `cnf` first |
| F_OP2_NUM | NUM | fire op (x rehydrated from `heap[loc+0]`) -- emits `RULE_OP2_NUM_NUM` |
| F_OP2_NUM | SUP | `interact_op2_num_sup` (no DUP needed; NUM is atomic) |
| F_OP2_NUM | other | stuck, rebuild OP2(x, whnf) in place at `loc` |
| F_UOP_CHILD | --- | re-drive the child's WHNF if it's a UOP with active descendants; advance to next active sibling or finalize parent |
| EQL | ERA / ANY | EQL-ERA-L / EQL-ANY-L short-circuit |
| EQL | SUP | EQL-SUP-L commute with DUP on b |
| EQL | other | store a's WHNF, push F_EQL_R frame, descend into b |
| F_EQL_R | ERA / ANY / SUP | EQL-ERA-R / EQL-ANY-R / EQL-SUP-R |
| F_EQL_R | NUM (and a is NUM) | compare values, emit `RULE_EQL_NUM` |
| F_EQL_R | other | stuck, rebuild |
| AND | ERA / NUM(0) | short-circuit ERA / NUM(0) |
| AND | NUM(non-zero) | descend into b |
| AND | SUP | AND-SUP commute with DUP on b |
| OR | ERA / NUM(non-zero) | short-circuit |
| OR | NUM(0) | descend into b |
| OR | SUP | OR-SUP commute |
| WHEN | ERA / NUM(0) | short-circuit (ERA result) |
| WHEN | NUM(non-zero) | descend into body |
| WHEN | SUP | WHEN-SUP commute |
| ANN | LAM / BRI | `interact_ann_lam` / `_bri` |
| ANN | other | stuck, rebuild |
| DSU | NUM / ERA / SUP | DSU-NUM / DSU-ERA / DSU-SUP |
| DSU | DP-headed | drive label through `cnf` first |
| DSU | other | stuck, rebuild |
| DDU | NUM / ERA / SUP | DDU-NUM / DDU-ERA / DDU-SUP |
| DDU | DP-headed | drive label through `cnf` first |
| DDU | other | stuck, rebuild |

Adding an interaction means:

1. Write `src/interact/<active_pair>.c` defining `interact_<active_pair>(...)`.
2. Declare it in [src/thvm.h](../src/thvm.h).
3. Add a case to the relevant frame's apply dispatch in
   [src/wnf/_.c](../src/wnf/_.c) AND the matching arm in
   [src/wnf/redex.c](../src/wnf/redex.c) (so the parallel pool can fire
   it through `redex_fire`).
4. Decide if it's a DUP-XXX readback rule: if so, also add it to
   [src/cnf/_.c](../src/cnf/_.c)'s `cnf_dp` switch.
5. Pick a `RULE_*` code, add a `multi_emit` call inside the rule, list
   it in [MULTI_RULE_NAMES](../src/instrument/multi.c) so the trace
   surfaces a readable name.
6. Add or extend a test under [tests/](../tests/) and document the
   rule under [docs/interact/](interact/).

## `cnf`: SUP-lift readback

`cnf` reduces to WHNF, then walks compound nodes to lift any SUP child
to the top.  It's where DUP-XXX duplication actually happens: plain
(non-grad) DP projections are Levy-opaque under `wnf` (see the apply
table's "DP0/DP1 -> APP stuck" entry), so the readback layer is what
fires them.

Lift shape (mirrors HVM4):

```
cnf(NODE(c_0, ..., c_{i-1}, SUP^L(a, b), c_{i+1}, ..., c_{n-1}))
  = SUP^L(NODE(c_0_dup, ..., a, ..., c_{n-1}_dup),
          NODE(c_0_dup, ..., b, ..., c_{n-1}_dup))
```

Each non-SUP sibling is wrapped in a fresh DUP under label `L` so the
two branches share work via the existing dup-cell mechanism.  `ERA` /
`INC` pass through; SUP at the head is the lifted result; pure
(DP-free, SUP-free) terms pass through unchanged.

Per-tag handlers in [src/cnf/_.c](../src/cnf/_.c):

| Tag | Handler |
| --- | ------- |
| ERA / NUM / TEN / VAR / ANY / REF / ALO / PRI / INC / SUP | pass through |
| DP0 / DP1 | `cnf_dp`: drive body through cnf, dispatch DUP-XXX inline |
| BJ0 / BJ1 | pass through -- book-time projections unfold only during `alo_realize` |
| LAM | `cnf_lam`: cnf body, lift if it returns SUP |
| CTR | `cnf_ctr`: cnf each child slot, scan for first SUP, lift |
| APP / OP2 / EQL / AND / OR / WHEN / ANN | `cnf_node2`: cnf both slots, re-drive parent through wnf, lift if either is SUP |

The "re-drive parent through wnf" step in `cnf_node2` is what lets a
DP-headed child resolve to something the parent can dispatch (e.g.
APP(LAM, arg) after a DP-LAM fires inside the readback).

## Collapse layers

`thvm_collapse` and `thvm_collapse_ordered` walk the SUP-tree of a
term, returning the set of pure leaves (one per branch of every SUP
encountered, minus ERA-pruned ones).  Both drive children through
`cnf` rather than plain `wnf` so DP-headed children (Levy-opaque under
wnf) get fired during the walk.

### `thvm_collapse` (naive)

[src/collapse/_.c](../src/collapse/_.c): a straight recursive walk.
At each step, `cnf` the term:

- `TAG_SUP` -> recurse on both children.
- `TAG_ERA` -> drop the branch.
- otherwise -> emit as a leaf.

### `thvm_collapse_ordered` (INC-priority)

[src/collapse/ordered.c](../src/collapse/ordered.c): the multicomputation
foliation primitive.  Same shallow walk, but each leaf is tagged with
the number of `TAG_INC` wrappers seen on the path from the root.
Lower INC-depth = higher priority = emitted first; ties are broken by
original DFS position (stable sort).

`TAG_INC` is a passive WNF atom: heap layout is `[body]`, `wnf` leaves
it alone, only the collapse layer observes it.  Wrapping a sub-term in
`INC` says "this branch is preferred" -- in interaction-net terms it's
a foliation marker, splitting the multiway tree into priority levels
that the observer enumerates in order.

### `eval_collapse` (HVM4-style priority queue)

[src/eval/collapse.c](../src/eval/collapse.c): variant with finer-
grained priority semantics:

- `INC` *decreases* the key (raises priority).
- `SUP` *increases* the key (lowers priority -- deeper alternatives
  come later).
- `ERA` drops the branch.

This is the form used for ATP CP selection (enumerate cheapest
candidates first) and matches HVM4's `clang/eval/collapse.c` minus
HVM4's parallel work-stealing queue (single-threaded FIFO with qsort
per round).

### The multicomputation premise

The whole point of the multicomputation surface is **collapse with
priority INC observers**:

- A `TAG_SUP` term is a *slice* of the multiway system -- two parallel
  states with shared structure.
- DUP-SUP commute is a *split* (cross product across an outer DUP);
  DUP-SUP annihilate (same label) is a *merge*; ERA is a *prune*.
- `TAG_INC` is the *foliation observer*: it imposes a partial order on
  branches that the collapser respects.  Without it, collapse is just
  enumeration; with it, you get a priority semantics that's
  Lévy-compatible (collapse is shaped by where the observer sits, not
  by the order interactions happen to fire underneath).

`thvm_collapse_ordered` and `eval_collapse` are the C-side observers
honoring INC.  The WL-side observer (`TMultiSteps`) currently does
not -- see "Design smells" below.

## `nf`: parallel pool drain

`nf` fires every reachable redex via a parallel pool of Chase-Lev
work-stealing deques (one per worker).  Default is T=1 (single-worker
drain on the calling thread); `THVM_THREADS=N` (or `nf_set_threads(N)`
from the WL bridge) spawns N pthreads.  Process-global pool reused
across calls; workers wait on `start_cond` between drains.

Per-fire flow:

1. Seed the bag with `redex_enumerate(&root, 1, ...)`'s output.
2. Workers pop, call `redex_fire`, which:
   - dispatches the rule (mirror of `wnf`'s apply table -- same
     `interact_*` functions, plus REF/ALO/UOP-SUP commutes that wnf
     doesn't fire eagerly because they're not on the head spine),
   - patches every heap cell holding the old redex via `heap_replace`,
   - pushes the result + newly allocated cells back onto the local
     worker's bag via `nf_work_push`.
3. T=1 with a step session attached: parent-promotion redexes (where
   `heap_replace` patched a child slot, flipping the parent's
   `is_redex`) get surfaced via the inverse index in
   [src/wnf/redex.c](../src/wnf/redex.c) and pushed straight into the
   worker bag, no full-heap re-enumerate.
4. T>1: legacy O(HEAP_NEXT) re-enumerate after each round catches
   promotions (the inverse-index lift for the parallel path is
   deferred -- the step session's globals race under MT).

Composition with `materialize`: `thvm_realize` (src/schedule/realize.c)
wraps an `nf` / `materialize` loop --

```c
Term res = nf(expr);
for (int iter = 0; iter < 16; iter++) {
  u32 kn0 = KERNELS_NEXT;
  Term mat = thvm_materialize(res);
  if (KERNELS_NEXT == kn0) { res = mat; break; }
  kernel_compute_consumer_counts();
  res = nf(mat);
}
```

`nf` reduces every UOp redex it can; `materialize` compiles the
remaining lazy compute into `KERNELs` whose dispatch is itself a
redex; the next `nf` fires those.  Loop converges when `materialize`
emits no fresh kernel.

### Redex identification

[src/wnf/redex.c](../src/wnf/redex.c) defines:

- `is_redex(t)` -- predicate.  APP-with-{LAM, ERA, MAT}-function,
  DP-with-{SUP/LAM/ERA/NUM/TEN/CTR/ANY/BRI/UOP/OP2/MAT/EQL/AND/OR/
  WHEN/ANN/DSU/DDU/INC}-body (mirroring HVM4's `wnf_dup_nod` dispatch;
  APP intentionally absent), grad-flag DPs (always eligible), REF
  (always), ALO (always), OP2 (both slots NUM), UOP_KERNEL (always),
  UOP_ASSIGN (both slots TEN), UOP-SUP commutation (binary/unary
  elementwise UOPs with a SUP child).
- `redex_fire(t)` -- dispatches the matching `interact_*`, patches the
  heap via `heap_replace`, returns the result.  Mirrors `wnf`'s apply
  table.
- `redex_enumerate(roots, n_roots, out, cap)` -- DFS over roots PLUS a
  linear scan of the live heap.  Dedup by packed Term value via a
  dynamically-sized open-addressed hash (`REDEX_DEDUP`).

The step session is a side index that turns `heap_replace` from
O(HEAP_NEXT) into O(uses-of-old).  See `redex_step_attach` /
`redex_step_fire` / `redex_step_drain_fresh` / `redex_step_detach`.

## Tracing

Tracing is live in the default build via `WL_TRACE ?= 1` in the
[Makefile](../Makefile); pass `WL_TRACE=0` to opt out for benching.

`ITRS` (the global interaction counter, [src/thvm.h](../src/thvm.h))
gets bumped once at the head of every `interact_*` rule.  In a
THVM_TRACE build, each bump is followed by a `multi_emit(rule, family,
term_a, term_b, delta_label)` call that appends a `MultiEvent` to
`ctx->multi_events`.  The macro `multi_emit` is `((void)0)` in non-trace
builds, so the call sites are free.

### Wire provenance

Every heap mutator stamps `ctx->multi_wire_prov[loc] = ITRS - 1` via
`WIRE_PROV_BUMP`.  An event's `consumed[]` field is filled at emit
time by reading `multi_wire_prov[term_val(term_a)]` and
`multi_wire_prov[term_val(term_b)]` -- the producer event ids of the
two active-pair payloads.  After the trace ends, `produced[]` is
derived dually by inverting `wire_prov`: for each loc, the event that
last wrote it is that event's producer.

### Event families

| Family | Meaning | Rules |
| ------ | ------- | ----- |
| TERM | within-branch compute | APP_LAM, OP2_NUM_NUM, MAT_DISPATCH, GRAD_FWD/BWD, DUP_LAM/CTR/NUM/TEN/UOP/ANY, AND_NUM, OR_NUM, WHEN_NUM, EQL_NUM/ANY, ANN_LAM/BRI, ... |
| SLIDE | re-foliation (no fork, no merge): APP-SUP commute, INC traversal, OP2-SUP, EQL-SUP, AND/OR/WHEN-SUP, DSU/DDU-SUP, DUP_NOD | structural rules that move a SUP through a compound |
| FORK | 1 -> 2 | (placeholder family; current rule set folds forks into SPLIT) |
| SPLIT | DUP-SUP different-label cross product | DUP_SUP_COM |
| MERGE | DUP-SUP same-label annihilate | DUP_SUP_ANN |
| PRUNE | ERA at any active pair | APP_ERA, DUP_ERA, AND_ERA, OR_ERA, WHEN_ERA, EQL_ERA |
| PLUMB | sharing housekeeping | (currently unused; reserved for explicit copy/share events) |

See [src/instrument/multi.c](../src/instrument/multi.c) for the
`MULTI_RULE_NAMES` and `MULTI_FAMILY_NAMES` tables.

### WL surface

[wl/THVMLink/Kernel/Multicomputation.wl](../wl/THVMLink/Kernel/Multicomputation.wl):

- `TMultiTrace[expr]` -- run `expr` with tracing on, return
  `<|"Result" -> ..., "Trace" -> {event, ...}|>`.
- `TMultiTraceQ[]` -- True iff the dylib was built with `-DTHVM_TRACE`.
- `TMultiSteps[term, maxSteps]` -- iterate `TStep` one interaction at
  a time, snapshotting `THeapDiagram` between steps.  In the
  current implementation, when the head reaches WHNF and `term_tag` is
  `TAG_SUP`, the SUP is split (push right onto a frontier stack,
  continue on left) so the stepper drives past WHNF into a CNF-style
  collapse.  See "Design smells" -- this walk is naive left-right
  DFS, not INC-priority.
- `TCausalGraph[trace]` -- builds the wire-provenance causal graph
  from `consumed[]`.

## Design smells and inconsistencies

These are known mismatches between the implementation and the "right"
shape.  Listed so the next maintainer doesn't have to rediscover them.

1. **`TMultiSteps` ignores INC priorities** ([Multicomputation.wl](../wl/THVMLink/Kernel/Multicomputation.wl)).
   The WL stepper does a naive left-right collapse walk with a
   frontier stack; the C-side `thvm_collapse_ordered` and
   `eval_collapse` honor `TAG_INC` as a priority observer.  The
   multicomputation premise is "collapse with priority INC observers"
   -- the WL surface should drive observation through one of the
   priority collapsers (or grow its own INC-aware frontier) rather
   than the naive walk.  This is the largest user-visible gap right
   now.

2. **No WL constructor for `TAG_INC`**.  There's `term_new_inc` in
   [src/term/new_inc.c](../src/term/new_inc.c) and an INC arm in
   `is_redex` / `cnf` / `interact_dup_nod`, but no `TInc[body]` in
   [wl/THVMLink/Kernel/THVMLink.wl](../wl/THVMLink/Kernel/THVMLink.wl)
   for users to mark priorities.  As a result it's impossible to
   demonstrate the priority semantics from a notebook today.

3. **`F_OP2_NUM` heap-roundtrip is unconditional** ([src/wnf/_.c](../src/wnf/_.c)).
   The OP2 frame stashes the WHNF'd x into `heap[loc+0]` before
   pushing F_OP2_NUM so the frame can carry the OP2's original `loc`
   as a wire-provenance carrier; F_OP2_NUM then re-reads x from the
   heap to fire the op.  Trace-correct, but the store/load pair is
   pure overhead when tracing is off.  A `#ifdef THVM_TRACE` split
   (or a separate frame tag for the trace-on path) would eliminate
   it.  No benchmark has shown the cost matters yet -- before doing
   this, measure the impact on OP2-heavy workloads.

4. **AOT cache key is hand-rolled** ([Makefile](../Makefile) or hash
   logic).  When the TContext layout changes (e.g. adding
   `multi_events[]` / `multi_wire_prov[]` for the trace machinery),
   stale AOT modules crash unless a magic number is XOR'd into the
   trace-build hash key.  The right shape is a content hash that
   includes the TContext struct definition (or any header that defines
   structs the AOT module reads).  Until then, every TContext field
   added in a trace build needs a new bump.

5. **`interact_dup_nod`'s `ari=0` fallback is unused**.  The dispatch
   table maps every dispatchable tag to a non-zero arity, and atom
   tags (NUM/ANY/ERA/...) have their own dispatch arms in `wnf`'s DP
   apply switch.  The `ari=0` arm in [src/interact/dup_nod.c](../src/interact/dup_nod.c)
   exists only as defensive fallback for a "compound tag with no
   children" case that can't happen.  Drop it or document it as
   intentional dead code.

6. **Two parallel re-enumerate paths in `nf`**.  T=1 uses the step
   session's inverse index; T>1 falls back to O(HEAP_NEXT) re-enumerate
   after every drain round.  The MT inverse-index path is gated by
   `THVM_NF_PARALLEL_STEP_SESSION=1` and has "remaining ordering bugs
   that surface as duplicate fires on some shared-substructure
   workloads" (per [src/wnf/nf.c:138](../src/wnf/nf.c#L138)).  Until
   that's fixed, T>1 perf suffers on heaps where most fires are
   parent-promotions of a long chain.

7. **`TAG_BJ0`/`TAG_BJ1` Levy-opacity is asymmetric**.  Both `wnf` and
   `cnf` pass BJ through unchanged -- duplication unfolds only when
   `alo_realize` copies a book template into dyn-heap and rewrites BJ
   back to plain DP at the binding site.  This means a BJ-rooted term
   passed to `cnf` reports as "stuck CNF" even though it logically
   represents the same multi-way state a DP-rooted term would.  Fine
   for ALO consumers but surprising for direct inspection of book
   terms.

8. **`MULTI_RULE_NAMES` and `MULTI_FAMILY_NAMES` are append-only**.
   Adding a new rule code without adding a `[RULE_FOO] = "FOO"` entry
   leaves a `NULL` slot, and `multi_rule_name` returns `"RULE?"`.
   That's correct as a fail-safe but means the trace shows `RULE?` in
   the diagram caption with no other diagnostic.  A compile-time
   `_Static_assert` on the table size matching `RULE_COUNT` would
   force the maintainer to update it.

9. **`step_session` globals race under MT** ([src/wnf/redex.c](../src/wnf/redex.c) STEP_* statics).
   `STEP_USE_HEAD`, `STEP_PRE`, `STEP_FRESH_BUF` etc. are file-static
   pointers, not per-context fields.  `nf` works around this by
   refusing to attach a session at T>1.  The right shape is to move
   these into `TContext` alongside `multi_events[]` -- once that's
   done, the T>1 inverse-index path is unblocked.
