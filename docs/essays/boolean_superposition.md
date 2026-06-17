# Boolean model-finding by superposition

A worked example of the three things the Interaction Calculus (IC) runtime
gives you at once: **parallelism** (a superposition explores every choice
simultaneously), **sharing** (a value used many times is computed once and
stays consistent), and **inspectability** (the multiway / causal / branchial
views show the reduction as it happens).

The task: given a boolean formula, find which variable assignments satisfy
it -- by evaluating the formula over a *superposition* of all assignments
and collapsing to the models.

Run cells top-to-bottom. The default `make wl` builds a trace-enabled dylib,
so every view here works out of the box.

## Setup

Make the paclet visible and load it.

```wolfram
PacletDirectoryLoad[ParentDirectory[NotebookDirectory[]]];
```

```wolfram
Get["WolframInstitute`THVMLink`"];
```

```wolfram
TInit[];
```

Confirm the multicomputation trace machinery is compiled in.

```wolfram
TMultiTraceQ[]
```

## Booleans as bits

We encode `False` as `0` and `True` as `1`, then build the connectives out
of integer arithmetic (`TOp2`). `NOT a` flips the bit; `AND` is just the
product; `OR` is De Morgan. Each helper returns an unevaluated `TOp2` graph
the runtime reduces lazily.

```wolfram
bnot[a_] := TOp2["-", TNum[1], a]
```

```wolfram
band[a_, b_] := TOp2["*", a, b]
```

```wolfram
bor[a_, b_] := bnot[band[bnot[a], bnot[b]]]
```

A **bit** is a superposition of both values -- a single term that *is* `0`
and `1` at the same time. `TSup[{0, 1}]` builds it.

```wolfram
bit[] := TSup[{0, 1}]
```

## One variable: trying both at once

`TCollapse` walks the superposition tree and enumerates the leaves. A bare
bit collapses to both values.

```wolfram
TReset[];
TTermVal /@ TCollapse[bit[]]
```

Apply `NOT` to a bit. The negation slides into each branch of the
superposition (the `OP2-SUP` interaction), so one term computes both
results -- `{1, 0}`.

```wolfram
TReset[];
TTermVal /@ TCollapse[bnot[bit[]]]
```

## Sharing keeps a variable consistent

Here is the subtle, important part. Consider `x AND x` where `x` is a single
bit used **twice**. We want the two uses to agree -- `x` is one variable, so
each world should pick one value for it. The answer must be `{0, 1}` (the
diagonal `0&0=0`, `1&1=1`), *not* the cross-product `{0&0, 0&1, 1&0, 1&1} =
{0,0,0,1}`.

```wolfram
TReset[];
TTermVal /@ TCollapse[TLam[x, band[x, x]][bit[]]]
```

`{0, 1}` -- consistent. The IC mechanism behind this is *same-label
annihilation*: when the lambda's binder is used more than once, the runtime
inserts an auto-`DUP` to fan the argument out; collapsing pairs the two
projections diagonally (the `DUP-SUP-ANN` rule), so the shared `x` resolves
to a single value per world. We will see that rule fire in the multiway view
below.

## The multiway view: parallelism made visible

Now trace `a AND b` over two **independent** bits and draw the multiway
graph. Each vertex is a distinct intermediate state; each edge is one
interaction, coloured by its rule family. `"Branchial" -> True` overlays
dashed edges between sibling states born from the same fan-out.

```wolfram
TReset[];
satSteps = TMultiTrace[TLam[a, TLam[b, band[a, b]]][bit[]][bit[]], All];
```

```wolfram
TMultiwayGraph[satSteps, "Branchial" -> True, "PlotLegends" -> Automatic, ImageSize -> 900]
```

Reading the graph from the top:

- **`APP-LAM`** (blue, `TERM`) -- the two beta-reductions substitute the bits.
- **`OP2-SUP`** (orange, `SLIDE`) -- the `*` slides into each superposition.
- **`DUP-SUP-COM`** (teal, `SPLIT`) -- the two *independent* bits cross-
  product: this is where the world count fans from 2 to 4.
- **`DUP-NUM`** (grey, `DIST`) -- a number copies into both projections.
- **`OP2-NUM-NUM`** (blue, `TERM`) -- the final folds, `0`/`1`.

The four worlds `{0,0,0,1}` converge to the two distinct canonical values
`{0, 1}` at the bottom.

## Sharing vs. independence, side by side

Trace the *shared* `x AND x` and watch for a different rule.

```wolfram
TReset[];
shareSteps = TMultiTrace[TLam[x, band[x, x]][bit[]], All];
```

```wolfram
TMultiwayGraph[shareSteps, "Branchial" -> True, ImageSize -> 700]
```

The decisive edge is **`DUP-SUP-ANN`** (red, `MERGE`). Where two independent
bits *commute* (`DUP-SUP-COM`, teal, fanning out), a single shared bit
*annihilates* (`DUP-SUP-ANN`, red, pairing in). That red edge is the
consistency rule -- it is why `x AND x` has two leaves, not four.

## The causal view: sharing is a DAG

The causal graph drops the state labels and shows only the interaction
events and which earlier event produced each one's inputs. Sharing shows up
as fan-out: one producer feeding several consumers.

```wolfram
TCausalGraph[satSteps, "VertexLabels" -> Automatic, "PlotLegends" -> Automatic, ImageSize -> 900]
```

## A real formula

Put it together on a 3-variable CNF: `(a OR b) AND (NOT a OR c)`. Each
variable is a shared bit; the formula reuses `a` across both clauses, so the
same-label machinery keeps it consistent. Collapsing gives the
satisfaction bit for all eight assignments.

```wolfram
TReset[];
cnf = TLam[a, TLam[b, TLam[c, band[bor[a, b], bor[bnot[a], c]]]]];
satVector = TTermVal /@ TCollapse[cnf[bit[]][bit[]][bit[]]]
```

The assignments are enumerated in the order `a` slowest, `c` fastest
(`000, 001, 010, ...`). Cross-check against a brute-force truth table.

```wolfram
bruteForce = Flatten @ Table[
    Boole[(a || b) && (! a || c)],
    {a, {False, True}}, {b, {False, True}}, {c, {False, True}}]
```

```wolfram
Sort[satVector] === Sort[bruteForce]
```

The number of models is the number of satisfying assignments.

```wolfram
Count[satVector, 1]
```

## Why this is multicomputation

Reading `docs/multicomputation.md`: a superposition is a *slice* of parallel
worlds, reduction is *slice evolution*, and `TCollapse` is an *observer* that
reads out the leaves. The boolean model-finder is a multicomputation whose
worlds are the variable assignments:

- **Parallelism** -- the superposed bits explore all `2^n` assignments in one
  term; `OP2-SUP` / `DUP-SUP-COM` are the branchings.
- **Sharing** -- a variable reused across clauses is one auto-`DUP`; the
  `DUP-SUP-ANN` pairing keeps it consistent (the causal graph is a DAG, not a
  tree).
- **Inspectability** -- the multiway graph is the assignment lattice, the
  branchial edges link simultaneous worlds, and the causal graph exposes the
  sharing structure.

Scaling past a handful of variables is where the multicomputation framing
earns its keep: the shared sub-formulas are evaluated once per distinct
partial assignment rather than once per leaf.
