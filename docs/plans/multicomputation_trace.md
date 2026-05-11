# Multicomputation trace -- a branch-cylinder-tagged token-event log

Status: **M0 spike landed** (one rule wired -- `interact_app_lam` --
plus the full compile-time / runtime gating discipline, and a C-side
test that passes in both build variants); M1-M4 + the WL surface +
the remaining `interact_*` rules are still open.  See
[M0 spike outcome](#m0-spike-outcome) below for what shipped and what
deliberately did not.  Conceptual companion:
[docs/multicomputation.md](../multicomputation.md) reads the
SUP / DUP / INC machinery through Wolfram's multicomputation paradigm
(a SUP-term is a *slice*; reduction is *slice evolution*; the
collapser is an *observer*; `INC` is a *foliation*).  *This* memo is
the thvm-side build trajectory: a trace format you can attach to a
single thvm run from which the Wolfram-style multiway, branchial,
causal, and token-event graphs reconstruct deterministically, with
the priority-collapse foliation falling out as the canonical one.

Vocabulary lives in
[docs/glossary.md](../glossary.md#multicomputation-slices-observers-and-the-reduction-trace);
the runtime pieces it leans on are `TAG_SUP` / `TAG_DUP` / `TAG_INC` in
[src/thvm.h](../../src/thvm.h), the CNF readback in
[src/cnf/_.c](../../src/cnf/_.c), the priority collapser in
[src/collapse/ordered.c](../../src/collapse/ordered.c), and the hot-
counter instrumentation pattern in
[src/instrument/hot_counters.c](../../src/instrument/hot_counters.c).

## 1. Goal

From a single thvm reduction, recover -- after the fact, from a logged
trace -- the four Wolfram-style views Stephen Wolfram draws:

- the **multiway states graph** (states + state-evolving events, with
  reconvergence);
- the **branchial graph** at any antichain (states alive in a slice +
  branchial proximity);
- the **causal graph** (events + causal dependency between them);
- the **token-event graph** (the raw object the other three are
  quotients of);

plus the **set of foliations** (orderings the observer could
sequentialise the slice in -- the priority-collapse order picked by
`TAG_INC` decorations is one of them).  The trace should be cheap to
produce, replayable to validate, and rich enough that the views are
purely a host-side derivation (no second pass through the C reducer).

The pitch in one line: **an interaction net is already a token-event
graph**; the job is to (a) not throw the history away, (b) tag each
event with its branch cylinder, and (c) drop a thin WL layer on top
that builds the `Graph[...]` objects.

## 2. Where it hooks

The plan lands on infrastructure that already exists:

- The **per-rule interaction counter** `ITRS` (defined in
  [src/thvm.h](../../src/thvm.h), incremented by every `interact_*`
  function under [src/interact/](../../src/interact/)).  Every event
  we want to log corresponds 1-to-1 to an `ITRS` bump.
- The **per-context hot-counters instrumentation pattern**
  ([src/instrument/hot_counters.c](../../src/instrument/hot_counters.c)
  and `THotCountersDelta` in
  [wl/THVMLink/Kernel/Profile.wl](../../wl/THVMLink/Kernel/Profile.wl)):
  fields on `TContext`, accessor macros, snapshot+reset on the WL side.
  We mirror this *exactly* for the trace, so reviewers reading the
  hot-counter code can read the trace code without learning a second
  pattern.
- The **`DUP-SUP` label compare** that already lives in
  [src/interact/dup_sup.c](../../src/interact/dup_sup.c) (and the
  matching commute-case to-be-implemented, per
  [docs/interact/dup_sup.md](../interact/dup_sup.md)).  That same
  compare decides our `MULTI_MERGE` vs `MULTI_SPLIT` family tag -- no extra
  branching logic.
- The **CNF readback** ([src/cnf/_.c](../../src/cnf/_.c)) -- "lift the
  first SUP to the head" is the canonical *slide* event.
- The **priority collapser** ([src/collapse/ordered.c](../../src/collapse/ordered.c))
  -- the concrete observer whose foliation we want to expose as a
  first-class object.
- The **`TAG_INC` priority wrapper**
  ([src/thvm.h](../../src/thvm.h) at the `TAG_INC` line) -- already a
  no-op for state and a key-shifter for the queue; we just label its
  events as foliation-only.

What we add: one new directory [src/instrument/multi/](../../src/instrument/)
holding the trace record, the emit hook, the wire-provenance side
table, and the cylinder arena; one WL module
[wl/THVMLink/Kernel/Multicomputation.wl](../../wl/THVMLink/Kernel/)
holding the host-side view constructors; tests under
`tests/test_multi_trace.c` and a `wl/THVMLink/Tests/multicomputation.wlt`.

The design is **two-tier**: a v0 cheap **redex log** that is
replayable end-to-end (so you can derive everything offline by
simulation), and a v1 upgrade that adds **wire provenance** and
**live-forest tracking** for constant-time causal-edge derivation.

**Trace is optional, and free when disabled.**  The hot path of the
reducer -- the `interact_*` functions under
[src/interact/](../../src/interact/) that account for the bulk of
every benchmark we care about -- must remain byte-for-byte identical
to today's compiled code when tracing is off.  Unlike `HOT_*`
(always-on counters), the trace is not free in absolute terms, so we
use **two stacked switches**:

1. A **compile-time flag** `THVM_TRACE` (off by default, like
   `-DNDEBUG`).  When it is not defined, the `multi_emit` macro
   expands to `((void)0)`, the per-context fields (`events`,
   `wire_prov`, `cell_sup`, `multi_cyl`, ...) do not exist on
   `TContext`, the `WIRE_PROV_BUMP` macro inside `heap_set` /
   `heap_alloc` expands to nothing, and
   [src/instrument/multi/](../../src/instrument/) is not linked into
   the binary.  Every `interact_*`, `heap_set`, `heap_alloc` compiles
   to the same instruction sequence as today; the compiler proves the
   trace code dead and eliminates it.  Default thvm builds --
   including all production `bench-train`, `bench-atp`, AOT, and
   user-facing WL runs -- pay nothing.
2. A **runtime flag** `TContext.trace` (also off by default), used
   only inside trace-enabled builds, so you can toggle the trace on
   and off from WL within a single session without rebuilding -- e.g.
   `TMulticompTrace[expr]` flips it on for `expr`, captures the
   trace, flips it back off, returns.  When toggled *off* in a
   trace-enabled build, the cost is one well-predicted branch per
   `interact_*` and per `heap_set` (`if (UNLIKELY(ctx->trace))
   multi_emit_body(...);`); when toggled *on*, the cost is one record
   append per `interact_*` plus one `wire_prov[]` write per `heap_set`
   / `heap_alloc`.

So the only build that pays anything is one explicitly compiled with
`-DTHVM_TRACE`, and even that build pays only a single predicted
branch per event unless the runtime flag is up.  Reviewers running
benches should see zero delta from this work.

## 3. The `MultiEvent` record

Lives in [src/instrument/multi/event.h](../../src/instrument/) (proposed),
appended to a growable `EVENTS[]` on `TContext`, gated by
`TContext.trace`.  One record per `interact_*` call; `len(EVENTS)` must
end equal to `ITRS`.

```c
// instrument/multi/event.h.  TContext gets:
//   MultiEvent *events; u64 events_len, events_cap;
//   u32     *wire_prov;   // v1: WIRE_PROV[loc] = (event_id << 24) | gen
//   u8      *cell_sup;    // v1: heap[loc]'s nearest enclosing-SUP loc
//   u64     *multi_cyl;      // v1: cylinder arena (label, side) pairs
//   u64      multi_cyl_len, multi_cyl_cap;
//   u8       trace;       // 0 = off, 1 = redex log, 2 = full (v1)
typedef struct {
  u64    id;            // monotone; == ITRS at the point this rule fired
  u8     rule;          // RULE_APP_LAM, RULE_DUP_SUP_ANN, RULE_DUP_SUP_COM, ...
  u8     family;        // MULTI_TERM | MULTI_SLIDE | MULTI_FORK | MULTI_SPLIT
                        //                    | MULTI_MERGE | MULTI_PRUNE | MULTI_PLUMB
  // active pair, captured *before* the rule fires:
  u64    loc_a, loc_b;  // the two redex cells in Heap[]
  u64    term_a, term_b;// their packed Term words (enough to replay)
  // products (v0: just count + base loc of fresh allocations; v1: ids):
  u32    n_alloc; u64 alloc_base;
  // wire provenance (v1):  WireId = (producing_event << 24) | gen
  //   consumed[] and produced[] are filled from the WIRE_PROV side table.
  WireId consumed[MULTI_MAX_PORTS]; u8 n_consumed;
  WireId produced[MULTI_MAX_PORTS]; u8 n_produced;
  // branch position (v1+):
  u32    cyl_len;       // partial label->side map, packed into multi_cyl:
  u64    cyl_off;       //   multi_cyl[cyl_off .. cyl_off+cyl_len) = {(label, side)}
  // branch-structure delta (only meaningful for FORK/SPLIT/MERGE/PRUNE):
  u32    delta_label;   // the SUP/DUP label involved
} MultiEvent;
```

Two intrusive requirements, both small:

1. **Stable wire identity across rewrites.**  A `loc` is *not* stable
   -- thvm's bump allocator (see [docs/heap.md](../heap.md) and
   [src/heap/](../../src/heap/)) reuses freed cells.  Maintain
   `wire_prov[loc] = {producing_event, generation}`, bumped on every
   `heap_set` / fresh `heap_alloc` *when tracing is on*.  A wire's
   identity is then `(producing_event, generation, slot)` -- never
   reused.  Realised via a single macro `WIRE_PROV_BUMP(loc)` placed
   inside each heap mutation site; without `THVM_TRACE` the macro
   expands to `((void)0)` and the compiler removes the call entirely,
   so default builds pay nothing.  In a `THVM_TRACE` build with
   `ctx->trace == 0`, the cost is one well-predicted branch per heap
   mutation; with `ctx->trace == 1`, one extra indexed write.  (v0
   skips this table; see below.)
2. **Don't GC the history.**  `EVENTS[]` and the cylinder arena
   `multi_cyl[]` grow monotonically for the duration of a traced
   reduction; they're parallel structures, the heap itself is
   untouched.  Snapshot + reset on demand, mirroring `THotCountersDelta`
   in [src/instrument/hot_counters.c](../../src/instrument/hot_counters.c).

**v0 without (1)**: log only `{id, rule, family, loc_a, loc_b, term_a,
term_b, n_alloc, alloc_base, delta_label}`.  Because every `interact_*`
is deterministic, a host-side replay of this log reconstructs the full
net at every step, and the causal edges fall out of the replay
("event *j* wrote the cell event *i* later read").  v0 is correct for
single-branch programs (no `cyl_*`) and is enough to validate the rule
classification and the term / slice split.  It is also the natural
**M0** scope below.

## 4. Rule classification (the `family` field)

| interaction(s) | family | role in the multiway graph |
|---|---|---|
| `APP-LAM` (beta), `OP2-NUM-NUM`, `MAT-CTR` / `SWI-NUM`, `USE-VAL`, an `EQL` leaf, `AND` / `OR` on a literal | `MULTI_TERM` | a real multiway event; replicated over the redex's cylinder |
| `APP-SUP`, `OP2-(NUM-)SUP`, `MAT-SUP`, `DSU-SUP`, `USE-SUP`, and `cnf`'s lift-first-SUP move ([src/cnf/_.c](../../src/cnf/_.c)) | `MULTI_SLIDE` | re-foliation; state-set invariant; not a states-graph edge |
| every `INC` rule (`APP-INC`, `OP2-INC`, `MAT-INC`, `AND` / `OR-INC`, `EQL-INC`, `USE-INC`, `DDU-INC`, `DSU-INC`) | `MULTI_SLIDE` (foliation-only) | reorders the observer's queue; carries no states / branchial content -- droppable when you want "the graph, period" |
| a seeded `&L{a,b}`; `DUP-LAM`, `DUP-CTR`, `DUP-NOD` (a `DUP` commuting through a constructor manufactures a SUP) | `MULTI_FORK` | a state fans out 1 -> 2 (may be plumbing if a same-label `MULTI_MERGE` follows -- the states-graph replay sorts it out) |
| `DUP-SUP`, *different* label ([docs/interact/dup_sup.md](../interact/dup_sup.md), commute case) | `MULTI_SPLIT` | branchial cross product; state count * 2 |
| `DUP-SUP`, *same* label ([docs/interact/dup_sup.md](../interact/dup_sup.md), annihilate case) | `MULTI_MERGE` | two branches reconverge / are identified; a diamond closes |
| `ERA`-anything (`APP-ERA`, `DUP-ERA`, `OP2-ERA`, ... and ERA propagation) | `MULTI_PRUNE` | a dead branch is dropped (also: failed candidate -> ERA in collapse) |
| `DUP-VAR`, `DUP-DP0` / `DUP-DP1` | `MULTI_PLUMB` | pure sharing housekeeping; not a multiway event |

Implementation: each `interact_*` function in
[src/interact/](../../src/interact/) already begins (or ends) with
`ITRS++;` -- add an adjacent `multi_emit(rule, family, ...)` call.
The hook is a macro, not a function, so the compile-time switch can
delete it entirely from default builds:

```c
// src/instrument/multi/emit.h  (always included from src/interact/)
#ifdef THVM_TRACE
fn void multi_emit_body(u8 rule, u8 family, u64 loc_a, u64 loc_b,
                        Term term_a, Term term_b, u32 delta_label);
#define multi_emit(rule, family, ...)                                  \
    do {                                                               \
      if (UNLIKELY(CURRENT_CTX->trace)) {                              \
        multi_emit_body((rule), (family), __VA_ARGS__);                \
      }                                                                \
    } while (0)
#else
#define multi_emit(rule, family, ...) ((void)0)
#endif
```

```c
// src/instrument/multi/emit.c  (only compiled when THVM_TRACE is on)
fn void multi_emit_body(u8 rule, u8 family, u64 loc_a, u64 loc_b,
                        Term term_a, Term term_b, u32 delta_label) {
  MultiEvent *e = multi_events_push();
  e->id          = ITRS;             // captured before the *next* ITRS++
  e->rule        = rule;
  e->family      = family;
  e->loc_a       = loc_a;
  e->loc_b       = loc_b;
  e->term_a      = term_a;
  e->term_b      = term_b;
  e->delta_label = delta_label;
  // n_alloc / alloc_base filled in by multi_alloc_snapshot() right after.
  // wire prov / cylinder fields filled by v1 hooks when trace == 2.
}
```

In default builds (no `-DTHVM_TRACE`), the `multi_emit` call site
inside every `interact_*` expands to `((void)0)` and disappears in the
generated assembly; `multi_emit_body` and the whole of
[src/instrument/multi/](../../src/instrument/) are not linked.  In a
`THVM_TRACE` build with the runtime flag off, the call site expands to
one well-predicted branch returning immediately.  Only with the
runtime flag on does the record actually get appended.  Same pattern
goes for `WIRE_PROV_BUMP(loc)` inside `heap_set` and `heap_alloc`.

The `family` for `DUP-SUP` is decided by the label compare that's
already in [src/interact/dup_sup.c](../../src/interact/dup_sup.c)
(see the worked example in [docs/interact/dup_sup.md](../interact/dup_sup.md)):
`lab == sup_lab` -> `MULTI_MERGE`, else `MULTI_SPLIT`.  The classification
introduces no new branching anywhere.

## 5. The branch cylinder

A **cylinder** is a partial map `label -> side`: a *cone* of branches
(every leaf below that point in the live SUP-label forest).  Every
event has one -- the cone the redex lives in -- and a *shared* sub-term
contributes its cylinder to *every* event happening inside it ("do it
once, it counts everywhere it's shared"; this is the branchial
restatement of Levy-optimal sharing, see
[docs/plans/levy_optimal.md](levy_optimal.md)).

Maintain the **live SUP-label forest** during the traced reduction:

- `MULTI_FORK` adds a node (with the new `SUP`'s label);
- `MULTI_SPLIT` multiplies (a `&L`-node sprouts under a `&R`-node);
- `MULTI_MERGE` deletes a node, identifying its two children;
- `MULTI_PRUNE` deletes a subtree.

When an event fires at cell `loc`, its **cylinder** is the path from
the forest root down to the SUP node `loc` sits under -- read off as a
partial map `label -> side`, packed into `multi_cyl[]`, with
`(cyl_off, cyl_len)` stored on the event.

**Cheap exact representation.**  Tag every heap cell, when tracing,
with the loc of its nearest enclosing SUP (a `u32` side array
`cell_sup[]` parallel to `Heap[]`, propagated as SUPs are pushed up
by `MULTI_SLIDE` events).  Then "the cylinder of `loc`" is a walk up
`cell_sup` until it hits zero.  The only place the heap representation
needs an auxiliary word, and only under the trace flag -- thvm's
`Heap[]` itself is untouched.

## 6. Reconstructing the Wolfram views (host side)

From `EVENTS[]` (plus `wire_prov` in v1), all four Wolfram views are
deterministic projections.  Implemented in
[wl/THVMLink/Kernel/Multicomputation.wl](../../wl/THVMLink/Kernel/)
(proposed).

Soundness note: each view is a property of the *program*, not of the
reducer's schedule.  Interaction nets are *causally invariant* in
Wolfram's sense (a strictly stronger property than confluence -- see
[docs/multicomputation.md, §3.1 layer (4) and §5](../multicomputation.md#31-a-finer-point-about-confluence)):
the per-active-pair rule discipline makes every event determined by
the net and every causal dependency determined by wire-provenance,
neither of which depends on the order our `interact_*` happened to
fire redexes in.  So two runs of the same program that fired redexes
in different orders produce traces that differ only in event *ids* (=
clock time); the derived causal / branchial / multiway-states graphs
are isomorphic.  This is what makes "the causal graph of this
program" a well-typed object rather than an artifact of
[src/collapse/ordered.c](../../src/collapse/ordered.c)'s queue.

- **Causal graph.**  Nodes = events.  Edge `j -> i` iff
  `consumed(i) ∩ produced(j) != {}`.  v1: a set intersection on
  `WireId`s, O(1) per edge with a small per-wire hash.  v0: derived by
  replay -- step through `EVENTS[]`, maintain the net at each step,
  attribute each `consumed` slot to whichever event last wrote it.
  Branchlike vs spacelike vs timelike separation of two
  causally-incomparable events = the relation between their cylinders
  (disjoint cylinders => branchlike).
- **Branch tree / branchial graph at antichain *k*.**  Replay the
  `MULTI_FORK` / `MULTI_SPLIT` / `MULTI_MERGE` / `MULTI_PRUNE` stream up to *k*;
  the *leaves* of the resulting forest are the states alive in that
  slice; the tree metric is branchial distance; `MULTI_MERGE` edges are
  the reconvergence edges.
- **Multiway states graph.**  Replay the `MULTI_TERM` events, each
  *expanded over its cylinder*: an event with cylinder `C` contributes
  `|leaves(C)|` parallel edges in the expanded graph (state `s|_b ->
  s'|_b`, one per branch `b` in the cone).  Canonicalise states (alpha
  + DUP-floating + the slide-quotient -- well-defined because IC is
  confluent, see [docs/normal_form.md](../normal_form.md) for the
  reducer's notion of normal form) and identify; `MULTI_MERGE` events
  supply the diamond-closing edges; `MULTI_FORK` events the 1 -> 2
  fan-outs.  `MULTI_SLIDE` events contribute *no* edges.
- **Foliations.**  Every antichain of the causal graph consistent with
  cylinder structure is a foliation; the BFS priority queue in
  [src/collapse/ordered.c](../../src/collapse/ordered.c) (with `INC`
  shifting keys) picks the canonical one.  `INC`-only events matter
  *only* here -- they reorder the antichain traversal, they don't
  change which antichain.
- **Token-event graph.**  You already have it: tokens = `WireId`s that
  persist, events = `MultiEvent`s, the rest of the views are quotients of
  it.

## 7. WL surface

LibraryLink bridge ([docs/wl.md](../wl.md)), echoing `THotCounters` /
`THeapGraph` / `THeapDiagram`:

- **`TMulticompTrace[expr]`** -- run `expr` with tracing on, return the
  `EVENTS[]` log as a list of associations (or a WXF blob for big
  runs):

  ```mathematica
  <| "id" -> _Integer
   , "rule" -> "APP_LAM" | "DUP_SUP_ANN" | "DUP_SUP_COM" | ...
   , "family" -> "TERM" | "SLIDE" | "FORK" | "SPLIT" | "MERGE" | "PRUNE" | "PLUMB"
   , "cylinder" -> <| label_Integer -> 0 | 1, ... |>
   , "consumed" -> {__WireId}
   , "produced" -> {__WireId}
   , "deltaLabel" -> _Integer  (* FORK/SPLIT/MERGE/PRUNE only *)
   |>
  ```

- **`TMultiwayGraph[trace]`** -> `Graph[...]` of canonicalised states +
  `MULTI_TERM` / `MULTI_FORK` / `MULTI_MERGE` edges.  Optional
  `"Frame" -> incScheme` re-derives the *emission order* for a given
  `INC` decoration (same graph, different traversal numbering).
- **`TBranchialGraph[trace, k]`**, **`TCausalGraph[trace]`**,
  **`TTokenEventGraph[trace]`** -- the other three views.
- **`TFoliations[trace]`** -- antichain enumeration; cross-checks that
  the default one matches the actual order
  [src/collapse/ordered.c](../../src/collapse/ordered.c) used.

These deliberately produce ordinary `Graph[]` objects so they sit
beside Wolfram's `ResourceFunction["MultiwaySystem"]`-style output for
direct comparison, and so they render in the IDE workflow the rest of
the project lives in (cf. [docs/heap_graph.md](../heap_graph.md),
[docs/diagrams.md](../diagrams.md)).

## 8. Milestones

- **M0 -- redex log (v0).**  `EVENTS[]` behind the trace flag; the
  `family` classification on every `interact_*` in
  [src/interact/](../../src/interact/); `TMulticompTrace[expr]`.
  Validate:
  - **No bench delta in default builds.**  `bench-train`, `bench-atp`,
    and the AOT bench (no `-DTHVM_TRACE`) match their pre-M0 numbers
    within noise; the disassembly of a representative `interact_*` is
    unchanged.  This is the load-bearing acceptance gate for the whole
    plan: if a default build pays anything, the gating discipline is
    wrong and the patch is rejected.
  - `Length[trace] == ITRS` after `expr` reaches normal form (in a
    `THVM_TRACE` build, runtime flag on);
  - hand-check the families on tiny terms (`!&0{a,b}=&0{1,2}; a+b` ->
    `MULTI_FORK` then `MULTI_MERGE` then a `MULTI_TERM`);
  - confirm `MULTI_SLIDE` events leave the collapse result unchanged
    when excised (re-run with all slides batched at the end).
  Files: `src/instrument/multi/{emit.h,emit.c,push.c,snapshot.c,event.h}`,
  one-line `multi_emit(...)` call added next to `ITRS++` in every
  `src/interact/*.c`, `wl/THVMLink/Kernel/Multicomputation.wl` (just
  `TMulticompTrace`), `tests/test_multi_trace.c`,
  `wl/THVMLink/Tests/multicomputation.wlt`.

- **M1 -- causal graph (v1).**  Add `wire_prov[]`; emit
  `consumed` / `produced` on each event; `TCausalGraph[trace]`.
  Validate:
  - the graph is a DAG;
  - `#events == ITRS`;
  - on a tiny example, the causal graph matches one drawn by hand from
    the trace (compare against [docs/heap_graph.md](../heap_graph.md)
    snapshots);
  - **default-build bench delta still zero.**  `WIRE_PROV_BUMP(loc)`
    inside `heap_set` / `heap_alloc` is a macro that expands to
    `((void)0)` without `THVM_TRACE`; re-run the M0 bench check to
    confirm the heap hot path is unchanged.
  Files: extend `src/instrument/multi/` (the `WIRE_PROV_BUMP` macro
  definition lives there), add a one-line `WIRE_PROV_BUMP(loc)` to
  `src/heap/set.c` and `src/heap/alloc.c`,
  `wl/THVMLink/Kernel/Multicomputation.wl` (adds `TCausalGraph`).

- **M2 -- branch tree.**  Track the live SUP-label forest; emit
  `MULTI_FORK` / `MULTI_SPLIT` / `MULTI_MERGE` / `MULTI_PRUNE` precisely;
  `TBranchialGraph[trace, k]`.  Validate:
  - label-collision detection: a deliberately bad SAT encoding
    (independent variables given the same label, per
    [docs/research/sat_solver_paths.md](../research/sat_solver_paths.md))
    must show a *spurious* `MULTI_MERGE` -- the failure mode IC's label
    discipline is meant to prevent;
  - on the SupGen-encoded SAT bench, total `MULTI_FORK` count matches the
    number of decision variables; total `MULTI_SPLIT` count matches the
    expected combinatorial expansion.
  Files: `src/instrument/multi/forest.c` (forest tracking),
  `wl/THVMLink/Kernel/Multicomputation.wl` (adds `TBranchialGraph`).

- **M3 -- cylinders + states graph.**  Per-cell enclosing-SUP tag
  (`cell_sup[]`); `cyl_*` on events;
  `TMultiwayGraph[trace]` = replay `MULTI_TERM` over cylinders +
  canonicalise + add `MULTI_MERGE` edges.  Validate:
  - `Collapse[expr] == VertexLeaves[TMultiwayGraph[trace]]` (`Collapse`
    via [src/collapse/_.c](../../src/collapse/_.c) /
    [src/collapse/ordered.c](../../src/collapse/ordered.c));
  - leaf count matches `collapse_ordered`'s emitted count exactly.
  Files: extend `src/instrument/multi/` (cylinder arena +
  `cell_sup[]` maintenance under `MULTI_SLIDE`),
  `wl/THVMLink/Kernel/Multicomputation.wl` (adds `TMultiwayGraph`).

- **M4 -- observers / foliations.**  `TFoliations[trace]`;
  `TMultiwayGraph[trace, "Frame" -> incScheme]`.  Validate:
  - two `INC` schemes give the *same* `TMultiwayGraph` but different
    emission order (confluence in pictures);
  - the cost-ordered observer of
    [docs/plans/waldmeister_ic_atp.md](waldmeister_ic_atp.md) walks
    the entailment cone cheapest-first, as one foliation among many.
  Files: `wl/THVMLink/Kernel/Multicomputation.wl` (foliation
  enumeration + frame re-traversal);
  `wl/THVMLink/Tests/multicomputation_atp.wlt` (worked example).

## 9. What this unlocks

Three concrete payoffs once M0-M3 are in:

1. **A multiway visualiser for IC programs.**  "See the branchial
   structure of this collapse" becomes one WL call.  Spurious
   `MULTI_MERGE`s (= label collisions) and runaway `MULTI_SPLIT`s (= a
   branch dimension you didn't mean to add) become *visible* in the
   branch tree; both are bugs that, today, only manifest as "the
   collapse count is wrong by a factor of two and I don't know why."
   The branch-tree view will close those bugs trivially on the SAT
   bench ([docs/research/sat_solver_paths.md](../research/sat_solver_paths.md))
   and the IC-native ATP ([docs/plans/waldmeister_ic_atp.md](waldmeister_ic_atp.md)).
2. **The observer becomes a first-class, inspectable object.**  "This
   `TAG_INC` decoration is this foliation is this traversal order"
   stops being folklore about
   [src/collapse/ordered.c](../../src/collapse/ordered.c) and becomes
   a `Graph` you can look at.  The foliation knob (which today is a
   `+/-` integer shift hidden in a queue) gets a visualisation, which
   is exactly what you want when you're writing a cost-ordered
   observer for ATP search.
3. **A direct comparison with Wolfram's own multiway pictures.**  The
   output is ordinary `Graph[]`, so `TMultiwayGraph[trace]` sits
   beside `ResourceFunction["MultiwaySystem"][...]` in a notebook for
   side-by-side comparison.  The contrast (IC's free confluence,
   branchial space as literal heap-sharing, `INC` as foliation -- see
   [docs/multicomputation.md, §5](../multicomputation.md)) becomes
   showable rather than just statable.

## 10. M0 spike outcome

A minimum-viable vertical slice of M0 landed.  The discipline holds
end-to-end; the next-step shape is clear.

### What landed

- **The struct + constants + gating macros** live in
  [src/thvm.h](../../src/thvm.h) alongside `HotCounters`:
  `MultiEvent`, the `MULTI_TERM / MULTI_SLIDE / MULTI_FORK /
  MULTI_SPLIT / MULTI_MERGE / MULTI_PRUNE / MULTI_PLUMB` family
  constants, the `RULE_APP_LAM` rule constant (just one for the
  spike), and the `multi_emit(...)` macro with its two-tier gate
  (`#ifdef THVM_TRACE` + `if (__builtin_expect(MULTI_TRACE_ON, 0))`).
  Without `-DTHVM_TRACE`, `multi_emit` is literally `((void)0)`; the
  per-context fields don't exist on `TContext`.
- **The runtime side-table** is one file:
  [src/instrument/multi.c](../../src/instrument/multi.c) -- holding
  `multi_emit_body`, `multi_events_push`, `multi_trace_init`,
  `multi_trace_reset`, `multi_trace_free`, `multi_trace_count`,
  `multi_trace_get`.  Everything inside the file is wrapped in
  `#ifdef THVM_TRACE`, so the translation unit is empty in default
  builds.  Hooked into the runtime by one line in
  [src/thvm.c](../../src/thvm.c) right after the existing
  `hot_counters.c` include.
- **One rule wired up**:
  [src/interact/app_lam.c](../../src/interact/app_lam.c) calls
  `multi_emit(RULE_APP_LAM, MULTI_TERM, (u64)lam, (u64)arg, 0);`
  immediately after `ITRS++`.  This is the vertical-slice proof; the
  remaining ~60 `interact_*` rules will follow the same pattern.
- **Surgical test** at
  [tests/test_multi_trace.c](../../tests/test_multi_trace.c), built
  twice from the same source via two Makefile targets:
  `bin/test_multi_trace` (default CFLAGS) and `bin/test_multi_trace_on`
  (`-DTHVM_TRACE`).  Default-build coverage: 2/2.  Trace-build
  coverage: 22/22 -- runtime-flag off, runtime-flag on, mid-run
  toggle, capacity growth.

### Acceptance gate verified

`make bin/test_app_lam bin/test_multi_trace` (both default CFLAGS):
the inlined `interact_app_lam` portion of `_wnf` has *4022 instructions
in both* binaries, and a `diff` of the disassembly (stripped of
absolute addresses) shows differences only in immediate operands
that reflect overall binary layout -- the opcode sequence is
character-for-character identical.  `nm bin/test_multi_trace | grep
multi_emit_body` returns nothing; `nm bin/test_multi_trace_on | grep
multi_emit_body` returns one symbol.  The compile-time gating
discipline holds: a default-build reducer pays zero instructions.

### Simplifications taken vs the original plan

- **One file, not a directory.**  The plan had
  `src/instrument/multi/{event.h,emit.h,emit.c,push.c,snapshot.c}`;
  the implementation collapsed those into a single
  [src/instrument/multi.c](../../src/instrument/multi.c) mirroring the
  [src/instrument/hot_counters.c](../../src/instrument/hot_counters.c)
  precedent.  Split it back out if the file grows past ~300 lines.
- **`emit.h` macro lives in `src/thvm.h`.**  Header-side declarations
  in this codebase live in `thvm.h` (next to where `HOT_*` macros
  live), not under `src/instrument/`.  The plan's
  `instrument/multi/emit.h` is therefore the relevant block of
  `src/thvm.h`.
- **Slimmer struct for v0.**  `MultiEvent` carries `id, rule, family,
  term_a, term_b, delta_label` only.  `loc_a / loc_b / n_alloc /
  alloc_base / consumed[] / produced[] / cyl_off / cyl_len` are not
  in the struct yet -- they land at M1 (wire provenance) and M3
  (cylinders).
- **Id semantics**: `e->id = ITRS - 1` rather than `ITRS`.  The
  convention is `ITRS++` at the head of every `interact_*` then
  emit; capturing `ITRS - 1` makes id 0 the very first event of the
  session, which lines up with the obvious `itrs_before` reading.
- **No WL surface yet.**  `TMulticompTrace[expr]` and friends are
  deferred to a follow-up; the M0 trace is consumed directly by the
  C test through the `multi_trace_*` API.
- **No `WIRE_PROV_BUMP` in heap mutations yet.**  M1 territory.

### Followups, in dependency order

1. **Extend the rule enum and wire the remaining `interact_*`.**
   Add `RULE_APP_ERA`, `RULE_APP_SUP`, `RULE_DUP_LAM`,
   `RULE_DUP_ERA`, `RULE_DUP_SUP_ANN`, `RULE_DUP_SUP_COM`,
   `RULE_ERA_*`, `RULE_OP2_*`, `RULE_MAT_*`, `RULE_INC_*`,
   `RULE_USE_*`, `RULE_EQL_*`, `RULE_AND_*`, `RULE_OR_*`, etc.
   Family classification per the §4 table.  One-line `multi_emit(...)`
   addition to each `src/interact/*.c` (next to its `ITRS++`).
   Validate on a deliberately bad SAT encoding (independent variables
   given the same label, per
   [docs/research/sat_solver_paths.md](../research/sat_solver_paths.md))
   that a spurious `MULTI_MERGE` shows up.
2. **WL surface.**  Add `thvm_wl_multi_trace_*` LibraryLink wrappers
   to [wl/THVMLink/CSource/thvmlink.c](../../wl/THVMLink/CSource/thvmlink.c)
   mirroring the `thvm_wl_hot_counters_*` pattern, and a new module
   `wl/THVMLink/Kernel/Multicomputation.wl` with `TMulticompTrace[expr]`
   that snapshots `EVENTS[]` into a list of associations.  Requires
   the WL bridge to be built with `-DTHVM_TRACE` (separate dylib
   variant -- mirroring how the metal bridge is conditionally built).
3. **M1: wire provenance.**  Add `wire_prov[]` to `TContext`,
   `WIRE_PROV_BUMP(loc)` macro at every `heap_set` / `heap_alloc`
   site, populate `consumed[]` / `produced[]` on `MultiEvent`.
   Re-run the disassembly gate.
4. **M2-M4** per the existing milestone list (branch tree, cylinders,
   foliations).
