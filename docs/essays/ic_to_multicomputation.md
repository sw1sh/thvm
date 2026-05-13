# thvm in cells

A tour of the IC + multicomputation surface. Run cells top-to-bottom.
The default `make wl` already builds a trace-enabled dylib so every
cell here works out of the box.  (Pass `WL_TRACE=0` to make to opt
out of the trace machinery -- in that case `TMultiTraceQ[]`
returns `False` and the trace cells return `$Failed`.)

## Setup

Make the paclet visible.

```wolfram
PacletDirectoryLoad[ParentDirectory[NotebookDirectory[]]]
```

Load it.

```wolfram
Get["THVMLink`"]
```

Initialise the runtime.

```wolfram
TInit[]
```

Confirm the trace machinery is compiled in.

```wolfram
TMultiTraceQ[]
```

## Beta

`TReset[]` zeroes the heap, the WNF stack, and the interaction
counter, so each example starts from a clean slate.  (`TInit[]` is
one-shot allocator setup -- already done above -- and is idempotent;
`TReset[]` is what you call between independent reductions.)

```wolfram
TReset[]
```

Identity applied to a literal.

```wolfram
betaTerm = TApp[TLam[x, x], TNum[42]]
```

Pre-WNF tag tree.

```wolfram
TTermExpr[betaTerm]
```

Heap diagram (LAM has a self-loop on its binder var; APP's principal
port is the dangling output).

```wolfram
THeapDiagram[betaTerm]["Arrange"]
```

Same heap, data-flow view.

```wolfram
THeapGraph[betaTerm]
```

Reduce. One APP-LAM fires.

```wolfram
betaOut = TWnf[betaTerm]
```

The value.

```wolfram
TTermVal[betaOut]
```

Post-WNF heap: the result is a single NUM atom (no LAM/APP left).

```wolfram
THeapGraph[betaOut]
```

## DUP and SUP

```wolfram
TReset[]
```

`TDup` returns the pair `{dp0, dp1}`.

```wolfram
{a, b} = TDup[TNum[5]]
```

Both projections reduce to the same NUM.

```wolfram
{TTermVal[TWnf[a]], TTermVal[TWnf[b]]}
```

`TSup[a, b]` packs two values behind one port; `TCollapse` enumerates
the leaves.

```wolfram
TTermVal /@ TCollapse[TSup[1, 2]]
```

The WL `+` is lifted onto `TTerm`, so SUPs participate in ordinary
arithmetic.  `OP2-SUP` is a slide: the `+` descends into each branch.

```wolfram
Sort[TTermVal /@ TCollapse[TSup[1, 2] + 10]]
```

The slide on a single picture: one OP2 cell, two SUP children, one
NUM atom.

```wolfram
THeapDiagram[TSup[1, 2] + 10]["Arrange"]
```

## DUP \[Times] SUP

```wolfram
TReset[]
```

Distinct labels: DUP commutes through SUP — branchial cross product.
Each projection sees both branches.

```wolfram
{dpA, dpB} = TDup[TSup[3, 1, 2]]
```

```wolfram
{Sort[TTermVal /@ TCollapse[dpA]], Sort[TTermVal /@ TCollapse[dpB]]}
```

Shared label: DUP annihilates against SUP — only the diagonal
survives.

```wolfram
{dpC, dpD} = TDup[7, TSup[7, 1, 2]]
```

```wolfram
{TTermVal /@ TCollapse[dpC], TTermVal /@ TCollapse[dpD]}
```

## TMultiTrace

```wolfram
TReset[]
```

Build a DUP'd `+` over a SUP — the canonical cross-product term.

```wolfram
{dp0, dp1} = TDup[TSup[1, 2] + 3]
```

Trace its collapse.

```wolfram
crossOut = TMultiTrace[TCollapse[dp0]]
```

Collapse leaves: `{1+3, 2+3}`.

```wolfram
Sort[TTermVal /@ crossOut["Result"]]
```

Which rule families fired.

```wolfram
Tally[crossOut["Trace"][[All, "family"]]]
```

The full table.  `consumed` is the list of producer event ids whose
output this event read; `produced` is the list of heap locs this
event wrote (last-writer-wins, so cells later overwritten don't
appear here).

```wolfram
Dataset[crossOut["Trace"]][All, {"id", "rule", "family", "consumed", "produced"}]
```

## TCausalGraph

The directed acyclic projection of the trace.  Vertices are colour-
coded by event family (TERM = within-branch compute, SLIDE = re-
foliation, SPLIT = DUP-SUP cross-product, MERGE = DUP-SUP annihilate,
PRUNE = ERA, DIST = sharing housekeeping); the legend names them.

```wolfram
TCausalGraph[crossOut["Trace"], VertexLabels -> Automatic, PlotLegends -> Automatic]
```

## TMultiwayGraph

A different shape: vertices are **terms** (states), not events.
Each step of a `TMultiSteps` reduction is a slice; the slice's active
term is unfolded at the head — every SUP-headed slice contributes one
vertex per leaf (so `SUP{1+3, 2+3}` is two vertices, `SUP{a, b} + 3`
is one).  Edges are events between consecutive slices, drawn as the
cross product of source-leaves × target-leaves so a SPLIT event fans
out into multiple edges.  Family colours the edges (TERM blue,
SLIDE orange, SPLIT purple, MERGE red, PRUNE/DIST grey).

```wolfram
{crossDp0, crossDp1} = TDup[TSup[1, 2] + 3]
```

```wolfram
crossSteps = TMultiSteps[crossDp0, "DiagramSeeds" -> {crossDp1}]
```

```wolfram
TMultiwayGraph[crossSteps, VertexLabels -> Automatic, PlotLegends -> Automatic]
```

`"Branchial" -> True` overlays an undirected clique between sibling
leaves within each SUP-headed slice — the WPP "branchial graph" view
of parallel-world states.

```wolfram
TMultiwayGraph[crossSteps, "Branchial" -> True, VertexLabels -> Automatic]
```

## Label discipline: cross product vs diagonal

```wolfram
TReset[]
```

Labels are the IC's branchial coordinate system.  Two SUPs with
**distinct** labels are independent dimensions -- their combination
under an operator fans out to the full cross product.  Two SUPs with
the **same** label are the same dimension -- they project pointwise
(the diagonal).  Both are correct IC semantics; which one you get is
a property of the term you wrote.

Distinct labels: full cross product `{11, 12, 21, 22}`.

```wolfram
crossSup = TMultiTrace[TCollapse[TSup[1, 1, 2] + TSup[2, 10, 20]]]
```

```wolfram
{Sort[TTermVal /@ crossSup["Result"]],
 Counts[crossSup["Trace"][[All, "family"]]]}
```

Shared label: pointwise diagonal `{11, 22}`.  The trace shows a MERGE
(DUP-SUP annihilate) where the distinct-label version showed a SPLIT
(DUP-SUP commute).

```wolfram
diagSup = TMultiTrace[TCollapse[TSup[1, 1, 2] + TSup[1, 10, 20]]]
```

```wolfram
{Sort[TTermVal /@ diagSup["Result"]],
 Counts[diagSup["Trace"][[All, "family"]]]}
```

Side-by-side causal graphs.  Same arithmetic shape, different branch
event in the middle -- the cross-label case shows a SPLIT (purple)
where the shared-label case shows a MERGE (red).

```wolfram
Grid[{
    {Style["distinct labels (cross)",  Bold],
     Style["shared label  (diagonal)", Bold]},
    {TCausalGraph[crossSup["Trace"], VertexLabels -> Automatic, PlotLegends -> Automatic],
     TCausalGraph[diagSup["Trace"],  VertexLabels -> Automatic, PlotLegends -> Automatic]}},
    Frame -> All]
```

And the multiway view side-by-side.  Build a TMultiSteps run for each
seed (TMultiwayGraph consumes step records, not raw traces -- the
slice-by-slice term snapshots come from there).  The cross-label
multiway has multiple vertices per slice after the SPLIT (the SUP-leaf
unfold); the shared-label one stays single-track because the MERGE
keeps the head non-SUP.

```wolfram
{crossSupDp0, crossSupDp1} = TDup[TOp2["+", TSup[1, 1, 2], TSup[2, 10, 20]]];
crossSupSteps = TMultiSteps[crossSupDp0, "DiagramSeeds" -> {crossSupDp1}]
```

```wolfram
{diagSupDp0, diagSupDp1} = TDup[TOp2["+", TSup[1, 1, 2], TSup[1, 10, 20]]];
diagSupSteps = TMultiSteps[diagSupDp0, "DiagramSeeds" -> {diagSupDp1}]
```

```wolfram
Grid[{
    {Style["distinct labels (cross)",  Bold],
     Style["shared label  (diagonal)", Bold]},
    {TMultiwayGraph[crossSupSteps, "Branchial" -> True, PlotLegends -> Automatic],
     TMultiwayGraph[diagSupSteps,  "Branchial" -> True, PlotLegends -> Automatic]}},
    Frame -> All]
```

## Three ways to step

Three single-interaction primitives, each at a different layer:

- `TStep[t]` -- fires one interaction on `t`'s **WHNF spine**.  The
  reducer picks the redex; you get the new term.  This is the
  canonical foliation.
- `TInteract[r, t]` -- fires the **specific redex** `r` (which you
  picked out of `TRedexes[t]`).  Returns a richer record.  This is
  the *observer's free choice* -- pick any antichain through the
  causal graph.
- `TMultiSteps[t]` -- loops `TStep` end-to-end and snapshots
  `THeapDiagram[term]` + the trace event after every step.  This is
  `TStep` + diary.

### TStep

```wolfram
TReset[]
```

`TStep` mutates the heap and returns a **new** Term wrapper that
points at the rewritten root.  To walk the reduction, you must chain
the returned wrapper through each successive `TStep` -- re-stepping
the *original* wrapper would walk back through stale cells and
return the substituted binder value instead of the new root.

A small APP-LAM-then-OP2 term.

```wolfram
stepTerm = TApp[TLam[x, x + TNum[1]], TNum[5]]
```

Interaction counter before stepping.

```wolfram
TItrs[]
```

Fire one.  `out1` is the new root (the LAM body with `x` pending
substitution).

```wolfram
out1 = TStep[stepTerm]
```

```wolfram
TTermExpr[out1]
```

```wolfram
TItrs[]
```

Fire the next one -- on `out1`, not on `stepTerm`.

```wolfram
out2 = TStep[out1]
```

```wolfram
TTermExpr[out2]
```

```wolfram
TItrs[]
```

### TRedexes + TInteract

```wolfram
TReset[]
```

`TInteract[r, t]` returns `<|"result" -> newRoot, "fresh" -> {...}|>`.
The next round of `TRedexes` should walk **from `newRoot`**, not
from the original `t` -- same reason as above.

**Scope:** `TRedexes` lists *directly-fireable* interactions only --
APP-LAM, OP2-NUM-NUM, DUP-SUP, ERA prunes, and similar **fold** /
**annihilate** rules.  **Slide** rules (OP2-SUP, APP-SUP, MAT-SUP,
EQL/AND/OR-SUP) and the wnf-inline grad/MAT-dispatch rules are NOT
classified as redexes; they fire only via WHNF's stack machine (so a
term whose only active interaction is a slide returns `{}` here).
For complete reduction stepping use `TStep` / `TMultiSteps`.

Build a fresh redex.

```wolfram
interactTerm = TApp[TLam[x, x + TNum[1]], TNum[5]]
```

List every active pair on the heap.

```wolfram
TRedexes[interactTerm]
```

Fire it explicitly.  The record exposes both the rewritten root and
any newly-active redexes the fire produced.

```wolfram
fire1 = TInteract[TRedexes[interactTerm][[1]], interactTerm]
```

`fire1["result"]` is the new root we'll step from next.

```wolfram
TTermExpr[fire1["result"]]
```

What's reducible now -- list against the new root.

```wolfram
TRedexes[fire1["result"]]
```

Fire that one.

```wolfram
fire2 = TInteract[TRedexes[fire1["result"]][[1]], fire1["result"]]
```

```wolfram
TTermExpr[fire2["result"]]
```

`fire2["result"]` is a `NUM` atom -- no head-spine redex left.
(`TRedexes` lists every active pair on the heap, including pre-trace
cells from earlier experiments, so it may still return entries; what
matters is the *root* shape.)

```wolfram
TRedexes[fire2["result"]]
```

### TMultiSteps

```wolfram
TReset[]
```

Same reduction, but each record carries the per-step diagram, the
event(s) fired, and the cumulative ITRS.

```wolfram
steps = TMultiSteps[TApp[TLam[x, x + TNum[1]], TNum[5]]]
```

Just the rules.

```wolfram
steps[[All, "Events"]] /. e_Association :> e["rule"]
```

Per-step heap snapshots.  Each diagram was rendered before the next
`TStep` mutated the heap, so the picture matches the state at that
step.

```wolfram
Grid[
    Prepend[
        Map[
            r |-> {
                "step " <> ToString[r["Step"]],
                If[r["Events"] === {},
                   "(initial)",
                   r["Events"][[1, "rule"]]],
                r["Diagram"]["Arrange"]},
            steps],
        {Style["#", Bold],
         Style["rule", Bold],
         Style["heap before next step", Bold]}],
    Frame -> All]
```

Same idea on a SUP-bearing term: stepping `dpStep` to WHNF walks the
`OP2-SUP` slide and the `DUP-SUP-COM` commute.  After the commute,
the heap holds **both** new SUPs -- `x0` (the active side, returned)
and `x1` (substituted into the DUP's body cell, waiting for the
sibling projection to consume).  `x1` is only reachable from
`dpStepOther`, so we pass it as `"DiagramSeeds"` to keep both halves
of the commute in the picture.

```wolfram
{dpStep, dpStepOther} = TDup[TSup[1, 2] + 3]
```

```wolfram
splitSteps = TMultiSteps[dpStep, "DiagramSeeds" -> {dpStepOther}]
```

```wolfram
splitSteps[[All, "Events"]] /. e_Association :> e["rule"]
```

The diagrams as a grid -- step 2 now shows both SUPs that
`DUP_SUP_COM` produced.

```wolfram
Grid[
    Prepend[
        Map[
            r |-> {
                "step " <> ToString[r["Step"]],
                If[r["Events"] === {},
                   "(initial)",
                   r["Events"][[1, "rule"]]],
                r["Diagram"]["Arrange"]},
            splitSteps],
        {Style["#", Bold],
         Style["rule", Bold],
         Style["heap state", Bold]}],
    Frame -> All]
```

## Where next

`docs/multicomputation.md` -- the conceptual essay (SUP = slice,
collapser = observer, INC = foliation).
`docs/plans/multicomputation_trace.md` -- the C-side build trajectory
through milestones M0-M4.
