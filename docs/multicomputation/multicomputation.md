# Interaction Calculus as a multicomputational substrate

> A reading of the Interaction Calculus -- specifically its
> superposition / duplication / priority-wrapper machinery -- through
> Stephen Wolfram's *multicomputation* paradigm
> ([Wolfram, 2021](https://writings.stephenwolfram.com/2021/09/multicomputation-a-fourth-paradigm-for-theoretical-science/)).
> §§1-5 read the calculus as it stands; §6 says what such a reading
> is good for.  The essay is self-contained: the only prerequisite is
> Wolfram's article.

## 1. A quick tour of the Interaction Calculus

The **Interaction Calculus** (IC) is a small term-rewriting language
whose evaluation is exactly the rewriting of an *interaction net*
(Lafont, 1990): a graph of agents connected by wires.

- An **agent** is a node with a fixed *arity* (number of ports) and
  one distinguished port called its **principal port**.  The remaining
  ports are *auxiliary*.  Each port is the end of exactly one wire.
- An **active pair** (or *redex*) is two agents whose principal ports
  meet on the same wire.  Reduction is "fire one active pair":
  consult the *interaction rule* for that ordered pair of agent kinds
  and rewrite the local subgraph to whatever the rule says.  A net is
  in *normal form* when no active pairs remain.
- The discipline -- redexes only at principal ports, *at most one*
  rule per ordered pair of agent kinds -- buys **strong local
  confluence** for free: two non-overlapping redexes commute, so the
  order they fire in cannot change the final normal form.  (IC is not
  terminating in general; it can encode the untyped lambda calculus.
  But local confluence holds, which is the half of Church-Rosser that
  matters below.)

The agents we'll need:

| agent | meaning | arity | principal port |
|---|---|---|---|
| `LAM` | `λx.body` | 3 | "the lambda" (toward whoever applies it) |
| `APP` | application `(f x)` | 3 | "the function" (toward `f`) |
| `ERA` | erasure / discard | 1 | the one port |
| `DUP_L` | **duplication** with label `L` | 3 | "the value being copied" |
| `SUP_L` | **superposition** with label `L` | 3 | "the superposed value" |

`LAM` carries a "binder" and a "body" port; `APP` carries a "function"
and an "argument" port.  Together they encode the lambda calculus: the
bound variable `x` is just a wire from the binder port into the body,
and the **`APP`-`LAM`** active-pair rule is *beta reduction*
(application meets lambda, the argument flows into the binder, the
body becomes the result).

`DUP_L` and `SUP_L` are the *sharing extension*.  A lambda-calculus
variable may be used many times, but a wire is linear -- so to feed a
value to two consumers we insert a duplicator:

- **`DUP_L`** has the value being copied on its principal port and two
  outputs `x0`, `x1`.  Wherever the source program uses a value
  twice, the compiler inserts a `DUP_L` and routes both consumers
  through it.
- **`SUP_L`** is the *dual*: one output (the superposed value) and two
  inputs `a`, `b`.  As a graph shape it is the same triangle as
  `DUP_L`, flipped about its principal port.  Its meaning is "this
  value is both `a` and `b`, at once."  We will write a superposition
  in surface notation as **`&L{a, b}`**.

The interaction rules involving these two are where everything
interesting happens:

- **`DUP_L` meets a constructor** (a `LAM`, a tuple, a `SUP` with a
  *different* label, an integer literal viewed as a constant agent,
  ...): *commute*.  Duplicating `λx.body`, for example, yields two
  lambdas `λx0.body0`, `λx1.body1`, with the binders `x0, x1` packaged
  into a fresh `&L{x0, x1}` (so the source variable `x` becomes a
  superposition inside the bodies), and with a fresh `DUP_L` placed
  inside the body to produce `body0, body1`.  This is how a
  duplication step *manufactures* a superposition.
- **`DUP_L` meets `SUP_M`, same label (`L == M`)**: *annihilate*.  The
  two duplicator outputs simply take the two superposition inputs:
  `x0 <- a`, `x1 <- b`.  The duplicator and the superposition were
  "the same branching seen twice"; they cancel.
- **`DUP_L` meets `SUP_M`, different labels (`L != M`)**: *commute*.
  Each input of the `SUP_M` is itself duplicated under `DUP_L`, and
  the four resulting wires are repackaged into two new `SUP_M`s, one
  per `DUP_L` output.  Branch count *doubles* across this rule.
- **Erasure**: `ERA` meets any agent and the agent is destroyed;
  `ERA`s propagate out down each of its other ports.  We will write
  `*` for an eraser.

(There are analogous rules for whatever else lives in the calculus:
constructors, numeric operators, pattern matches, equality tests, and
so on.  The shape is always "active pair plus rule.")

Three further conventions we'll lean on:

- **Sharing notation.**
  - `&L{a, b}` is a `SUP_L` with inputs `a, b`.
  - `! &L{x0, x1} = v; body` is "share `v` as both `x0` and `x1` inside
    `body`, via a `DUP_L`."
  - `*` is `ERA`.

- **Evaluation: reduce, then *collapse*.**  An evaluator drives the net
  toward a weak head normal form (the outermost structure is fixed,
  sub-nets may still contain redexes).  If the net contains
  superpositions, the head itself is superposed -- the value is "several
  values at once."  A separate pass, *collapse* (also called *readback*
  in the literature), walks the superposition tree and hands out the
  underlying values one by one, as a stream.  Collapse is what turns
  the compressed multivalued representation back into ordinary
  enumerable results.

- **Priority wrapper.**  A unary agent we'll call **`INC`** -- short
  for "*increase the observer's priority by commuting upward*" -- which
  wraps any term and has commute rules with every consumer:
  `(↑f) x`, `↑f` plus an operator, `↑f` inside an equality test, and so
  on, all reduce by floating the `↑` outward.  Semantically `INC` is a
  no-op.  Operationally it **biases the collapse walk**: collapse is a
  priority-queue traversal of the superposition tree (lower keys
  popped first); descending into a `SUP` *raises* the key, hitting an
  `↑` *lowers* it.  Because `↑` bubbles upward through every consumer,
  the queue eventually sees it and the branch wins priority.  `↑` is
  the calculus's knob for *observer-side scheduling*; it is not an
  arithmetic increment.

That is the full toolkit.  The takeaways for the rest of the essay:

- **`SUP`** is first-class branching data.
- **`DUP`** is its sharing dual, and the **labels** carried by both
  distinguish independent branch choices from correlated ones.
- The **interaction discipline** delivers confluence by construction.
- **`INC`** steers the readback.

Now multicomputation.

## 2. The reframing: a `SUP`-term is a slice, not a branch point

It is tempting to say "`&L{a, b}` is a fork in the multiway graph."
That is the wrong granularity.  A *single IC term that contains
superpositions* already represents an entire **slice** of a multiway
system: the whole set of branches that are alive *right now*, with
everything the branches still agree on factored out and shared.
Reading the term out -- collapse -- enumerates the states in that slice.
So:

| Wolfram multiway system | Interaction Calculus |
|---|---|
| a *state* (one string / expression / hypergraph) | one branch of a `SUP`-term, i.e. one leaf of its superposition-label tree |
| a *slice* (the state-set at one moment of multiway time) | **one IC term with embedded `SUP`s** -- the slice in shared form |
| an *event* (a rule application carrying one state to a successor) | a within-branch interaction (beta-reduction, a numeric op, a pattern match, ...) |
| the *multiway graph* (states + events, with state-merging) | the **recorded trace** of interactions, quotiented by confluence |
| the *branchial graph* at step *k* | the superposition-label tree restricted to one causal antichain |
| a *foliation* of multiway time | an antichain order the collapser walks the slice in |
| an *observer* | the collapser -- it sequentialises the slice into a stream of states |

The graph is the *trace of slice evolution*.  The catch is that an IC
interaction comes in flavours, and only some of them are graph edges.

### 2.1 Two flavours of step, four branch-structure mutations

Reduce a slice (a `SUP`-term) by one active pair.  The step is one of:

- **Term evolution** -- a *within-branch* compute step: beta-reduction
  (an application meeting a lambda), a numeric operation on two
  literals, a pattern match against a known head, an equality leaf, a
  short-circuit `AND` / `OR` on a literal.  This is a genuine multiway
  event: in every branch the redex lives in, that branch's state
  advances.  These are the edges of the multiway *states* graph.

- **Slice evolution** -- a *re-foliation*.  Two sub-cases.
  - **Boundary moves.**  An application or operator meeting a
    superposition (`(f &L{a,b})` becomes `&L{(f a), (f b)}`), or the
    collapse pass lifting an interior `SUP` to the head.  These do
    **not** change which states the slice represents -- `(f &L{a,b})`
    and `&L{(f a), (f b)}` are the same two states.  They only
    relocate the `SUP` boundary inside the term.
  - **Priority shifts.**  Every `INC` interaction.  These reorder
    which branch the observer sees first; they change nothing about
    the state-set or its branchial structure.

  Neither sub-case is an edge in the states graph.  Both are the
  price of carrying the slice in compressed form: *the slice changes
  while the per-branch terms don't.*

- **Branch-structure mutation** -- a step that changes the *topology*
  of the slice (how many branches, how they're nested):

  | mutation | the interaction | effect on the multiway graph |
  |---|---|---|
  | **fork** | a seeded `&L{a, b}`, or `DUP_L` commuting through a constructor (which manufactures a `SUP`) | a state fans out 1 -> 2 |
  | **split** | `DUP_L` meets `SUP_M`, `L != M` (commute) | branchial cross product -- state count *2 |
  | **merge** | `DUP_L` meets `SUP_L`, same label (annihilate) | two separately-tracked branches reconverge / are identified -- a diamond closes |
  | **prune** | `ERA` absorbing a neighbour (and `ERA` propagation) | a dead-end branch is dropped from the slice |

  (`DUP` meeting a variable or another duplication projection is pure
  sharing plumbing, not a multiway event of any kind.)

One subtlety up front: **whether a given `SUP` is "real" branching or
just sharing machinery is not a local fact.**  A duplication commuting
through a lambda creates a `SUP_L` whose only job may be to route a
duplication and which then annihilates against a same-label `DUP`
later in the run (a fork followed by a merge -- a 1 -> 2 -> 1 that
canonicalises to a no-op because the two halves are equal).  Or that
same `SUP` may survive, split against another label, and reach
collapse as two genuinely distinct results.  You cannot tell from the
single fork event alone; it depends on the rest of the trace.  Which
is exactly the multicomputational picture: the multiway *states* graph
(built by replaying term-events and canonicalising) sorts it out
automatically, and "what counts as a real branch" turns out to be a
derived, observer-dependent property, not a primitive one.

### 2.2 A worked micro-slice

```
   (λn. n + 10)  &A{1, 2}
```

A boundary-move (slice evolution -- the two states
`{(λn.n+10) 1, (λn.n+10) 2}` are unchanged, the `SUP` boundary just
shifts outward):

```
   &A{ (λn. n + 10) 1 ,  (λn. n + 10) 2 }
```

Two beta steps (two *term* events; the redex is not shared here, so
these are two separate events, one per branch):

```
   &A{ 11, 12 }
```

Collapse emits `11` then `12`.  The order is fixed by whatever `INC`
decoration is in scope -- see §4.

Now contrast `(&A{1, 2} + &A{10, 20})`.  The two superpositions carry
the *same* label `A`, so the operator-meets-`SUP` rule pairs them via
the same-label (annihilating) case -- this is a *merge*, not a split:

```
   &A{ 1 + 10, 2 + 20 }   ==   &A{ 11, 22 }
```

i.e. the two coins are *entangled*: branch-left is "1 paired with 10".
If the second operand had been `&B{10, 20}` with `B != A`, you'd get
the 2x2 *split* -- the branchial cross product:

```
   &A{ &B{1+10, 1+20}, &B{2+10, 2+20} }   ==   &A{ &B{11, 21}, &B{12, 22} }
```

Same-label = correlated branch choices (one measurement fixes the
other); distinct labels = independent (separable) choices.  That is
the entire meaning of the `DUP` / `SUP` label discipline, read
branchially -- and it is why a *label collision* is exactly a *spurious
merge*: two unrelated branch choices wrongly identified, because IC
uses label-equality (not state-equality) to decide which branches
reconverge.  A string multiway system, which merges by directly
comparing the states it produces, has no analogous error available to
make.

## 3. Relation to ordinary (substitution / rewriting) multiway systems

Wolfram's canonical multiway system is a *string* (or hypergraph)
substitution system.  Take `AB -> BA` on `ABAB`: the *state* is the
whole string, an *event* is one application of the rule at one
position, the system **branches because the rule matches in several
places**, and the multiway graph is built by following all those
applications with *state-merging* -- two derivations that reach the
same string get identified by direct equality of the strings they
produced.  The
*branchial* graph at step *k* connects states sharing a parent; the
*causal* graph has events as nodes, with an edge whenever one event's
output region overlaps a later event's input region.

The Interaction Calculus is a multicomputational system too, but it
relocates *where the nondeterminism lives*:

- **There is no search for "where the rule applies."**  An active pair
  is a redex that already exists in the net; the only freedom is the
  *order* in which redexes fire, and IC is strongly confluent, so that
  order does not matter.  The "the rule matched in two places"
  nondeterminism -- the thing that makes a string system branch -- is
  the *harmless* kind in IC.
- **The branching that does matter is `SUP`, and `SUP` is data.**  A
  superposition is "this value genuinely is two values," sitting in
  the term.  IC takes the structure a string system *spreads across a
  graph* and *packs it into one term*; the graph is recovered
  afterward, as the trace.  In a string system branching is *implicit*
  (it falls out of rule + state); in IC it is *explicit* (you wrote a
  `SUP`, or a `DUP`-vs-constructor commute manufactured one).

With that relocation, the fork / split / merge vocabulary translates
cleanly:

- **fork** (1 -> 2 in the states graph).  In a string system this is
  one node with two outgoing edges -- "the rule matched here *and*
  there, giving `s1` and `s2`."  The IC act of *creating* that fan-out
  is: seed `&L{s1, s2}` ("the state is both"), or let a `DUP` commute
  through a constructor (duplicating a value that branches, branches).
  So a string system's "rule matched in two places" becomes IC's
  "a `SUP` is now in the term."

- **split** (`DUP_L` meets `SUP_M`, `L != M`, commute).  In a string
  system, if two *disjoint* regions each have a branching rule, the
  string forks 1 -> 2, then each fork forks again -> 4, and those 4
  are the *cross product* of the two independent choices.  In IC that
  is `&A{...}` and `&B{...}` with `A != B`, and the `DUP`-`SUP`
  commute rule is what *computes* that cross product, on demand:
  `! &A{x0, x1} = &B{p, q}` rewrites to a `&B`-headed pair of
  `&A`-duplications.  The "n -> 2n" multiplication of state count is a
  **split** -- the branchial tensor product.  (This is the rule that
  lets IC carry multi-valued normal forms while keeping its rewrites
  confluent; §3.1 below pulls apart the three senses of "confluence"
  involved.)

- **merge** (`DUP_L` meets `SUP_L`, same label, annihilate).  In a
  string system, when two derivation paths produce the *same* string,
  the runtime sees the strings are equal and identifies them: the
  multiway graph reconverges, a diamond closes.  IC doesn't compare
  states for equality; reconvergence happens when a `DUP` meets a
  `SUP` *carrying its label* and they **annihilate** -- each
  projection takes its branch (`x0 <- a`, `x1 <- b`), i.e. the two
  branches that were being tracked separately were "the same choice
  seen twice" and now collapse to one.  Diamond-closing = a **merge**.
  And this is *why* independent choices need distinct labels.  A label
  collision is a spurious merge -- two unrelated branch choices
  identified for no reason -- a failure mode unique to IC's
  label-as-branch-identity scheme, which a string system (merging by
  direct state equality) has no analogous way to commit.

- **prune** (`ERA`).  A state with no valid successors -- a dead end.
  `ERA` absorbs its neighbour and propagates; the branch vanishes from
  collapse.  In Boolean satisfiability (SAT) terms it's the
  unsatisfying assignment, dropped silently; in automated theorem
  proving (ATP) it's a failed inference candidate.

- **slide** (every boundary-move and every `INC` interaction).  **No
  analogue in a naive string system**, because that system never
  compresses -- every state is a separate string, there is no
  "boundary" to move.  The slide is the housekeeping of the *shared*
  representation: push the `SUP` up through a consumer without
  changing which states are represented.  It is the IC-specific
  bookkeeping that makes "the slice evolves while the per-branch terms
  don't" a real, observable phenomenon -- and the reason the trace
  carries two edge colours, "term" and "slice", instead of one.

The one-line summary: **a string multiway system stores the multiway
graph uncompressed and re-branches by searching; IC stores it
maximally compressed (the `DUP` sharing) and re-branches by computing
with first-class `SUP` data, with slide steps doing the compression
bookkeeping and merge steps doing the canonicalisation that the
string system gets from direct state equality.**  And IC's compressed form is
*strictly more informative* than the uncompressed graph: the
sharing -- which branches still point at the same heap -- *is* the
branchial proximity, so you read it off directly instead of
recomputing it from common ancestry.

### 3.1 A finer point about confluence

§1's "IC is strongly confluent by construction" looks at first glance
to contradict any working description of `DUP`-`SUP` as "the rule that
makes interaction nets non-confluent."  Both are true, at three
different levels of the word:

1. **Rewrite-system confluence (the local diamond).**  Given any two
   distinct redexes in a net, you can fire either order and end up at
   terms that re-converge.  For IC this is *strict*: the diamond
   closes in one step, because "one rule per ordered pair of agent
   kinds, principal ports only" makes non-overlapping active pairs
   completely independent.  The `DUP`-`SUP` rule is itself
   deterministic -- a label-compare picks one of two outcomes -- and
   doesn't overlap with anything else.  At this level **IC is, and
   remains, confluent.**  That is the §1 claim and it stands.

2. **Result confluence (a normal form is a single value).**  The
   lambda calculus has it: if `t` normalises, the normal form is a
   single term.  IC with `SUP`s **does not**: a net containing
   `&L{1, 2}` is in normal form *and* represents two values.  The
   "result" of the computation is a structured bag whose shape is the
   `SUP`-label tree, and collapse enumerates it.  In the colloquial
   sense "does this program have one answer?", IC's answer is *no, in
   general*, and the `DUP`-`SUP` rule is the propagator that exposes
   that fact -- it lets branchial structure travel anywhere in the
   term.

3. **The historical non-confluence the labels actually fix.**  If you
   try to implement lambda-calculus cloning with an *unlabelled* fan /
   dup combinator -- one "duplicator" symbol, no labels distinguishing
   independent duplications -- you get *real* rewrite-system
   non-confluence: two fans of different origin meeting can either
   annihilate (wrong) or commute (right), and you cannot tell which
   from the local graph alone.  This is the "fan-mismatch" problem
   identified by Lévy (1978) and solved by Lamping (1990) with extra
   markers (croissants, brackets, levels); Asperti-Laneve (1996) and
   later optimal-sharing runtimes simplified the markers into integer
   labels.  **The `DUP` / `SUP` labels are exactly that fix.**  Without
   them, the system is genuinely non-confluent (and computes wrong
   things); with them, the rewrite system is confluent again, and the
   would-be non-confluence is relocated from "ambiguous rewrites"
   to "first-class branching data."

4. **Causal invariance, strictly stronger than (1).**  Wolfram's term
   for "any two derivations produce *isomorphic causal graphs* -- the
   same events in the same partial order, only interleaved differently
   in clock time."  This is properly stronger than confluence:
   confluence is about *endpoint states* agreeing; causal invariance
   is about the *entire event structure* agreeing.  Many confluent
   rewrite systems are not causally invariant (you can reach the same
   normal form via different sets of intermediate events), and many of
   the string-rewriting systems the Wolfram Physics Project studies
   satisfy it only *asymptotically* (the early branching washes out in
   the limit).  **Interaction nets satisfy it strictly**, because the
   discipline -- one rule per ordered pair of agent kinds, principal-
   port redexes only -- means every event is determined by the net
   (each redex fires exactly once) and every causal dependency comes
   from wire-provenance (event *j* produced the wire event *i*
   consumed), neither of which depends on the schedule.  Different
   reduction orders are interleavings of the *same* labelled DAG, not
   different DAGs that happen to share endpoints.  This is the version
   that actually matters for the multicomputation reading; §5 spells
   out why.

So the precise statement: **the labelled `DUP`-`SUP` rule is the
mechanism that lets a causally-invariant rewrite system carry
multi-valued computation.**  Layers (1) and (4) stay (rewrites are
confluent, and more: the causal graph is schedule-independent); layer
(2) arrives (normal forms are multi-valued); and layer (3) explains
the price of admission (the labels, distinguishing independent vs
entangled choices, are what keeps (1) and (4) alive in the presence
of (2)).  The "control" in any informal "non-confluent in a
controlled way" line is exactly that label discipline: independent
choices get distinct labels (cross product, the commute case);
correlated choices share a label (annihilate, the merge); the
multi-valuedness is therefore not chaos but a precisely-typed
branchial space.

This is the distinction §5 will lean on.  Layer (1) -- rewrite
confluence -- is what makes the multiway *states* graph a well-defined
quotient (identify equal intermediate states; slide events are
non-edges; the quotient is a graph, not a tree with arbitrary cuts).
Layer (4) -- causal invariance -- is the stronger property the
Wolfram Physics Project actually leans on, and the one that makes
"the causal graph of a trace" a property of the program rather than
an artifact of the reducer's schedule.  Result multi-valuedness *is*
the multiway evolution.  And all of it coexists *because* of the
labels.

## 4. Observers and reference frames

Slice evolution by itself produces a slice.  To get a *history* -- a
definite sequence of states someone could experience -- you need an
**observer**, and in the Interaction Calculus the observer is a
concrete piece of machinery: the priority collapser.  It takes a
slice (a `SUP`-term) and, because it is a bounded computation, cannot
present the slice "all at once" as a single classical value -- it has
to *walk the branches in some order* and *emit a stream* of normal
forms.  It must choose an order.  That choice is its reference frame.
(In Wolfram's language: "the observer is a bounded computational
system that conflates / sequentialises branches"; here, it is a
priority-keyed work queue.)

### 4.1 Superposition labels are the branchial coordinate system; cylinders are positions in it

The set of superposition labels alive in a slice forms a *tree*: each
`&L{...}` has a left and a right child, each of which is itself a
(sub)tree of superpositions.  A single branch of the slice is a
*path* down that tree -- at each label, left or right.  A *partial*
commitment -- you've chosen sides for some labels and not others --
names a **branch cylinder**: a *cone* of branches, namely every leaf
below the point you've reached.

A cylinder is the right object for "where the observer is in
branchial space while it's part-way through a collapse," and -- crucially
-- it is also where a *shared sub-term* lives.  A shared sub-term sits
at some node of the label tree, hence "is in" the cone of all leaves
below it.  That's the **cylinder of a shared node**.

This is why one IC interaction can be *many* multiway events at once.
A beta step whose redex is inside a shared sub-term is an event that
happens *simultaneously in every branch in that sub-term's cylinder* --
"do it once, it counts everywhere it's shared."  That sentence is the
branchial restatement of *Lévy-optimal* reduction (Lévy 1978, Lamping
1990, Asperti-Laneve 1996): **don't recompute what branches still
agree on.**  Maximal sharing = the minimal representation of the slice;
optimality = the invariant that never duplicates work two branches
still share.

Entanglement, in this vocabulary: two superpositions with the *same*
label are **not** independent coordinates -- choosing "left" at that
label fixes it for both occurrences.  Same-label `SUP`s are an
entangled pair of branch choices; a `DUP` meeting that label is the
"measurement" that propagates the choice through (the annihilating
case of `DUP`-`SUP`); distinct labels are separable (the cross
product, the commuting case).  The `DUP` / label discipline *is* the
bookkeeping of which choices are entangled with which.

### 4.2 `INC` is the foliation -- the observer's reference frame

The collapser is a priority queue: lower keys popped first.  Descending
into a `SUP` *raises* the key (visit that branch later); an `INC`
wrapper *lowers* it (visit sooner); and because `INC` commutes upward
through every consumer it bubbles to the head where the queue can see
it.  So an `INC` decoration of a term is *a choice of how to slice
branchtime* -- which branch the observer experiences first.

Two different `INC` decorations of the same term are **two different
observers over the same multiway evolution.**  Because IC is
confluent, the *set* of states they eventually emit is identical --
only the *order* differs.  That is precisely "different reference
frames, same physics," and "the foliation is a free choice; the
causal graph is not" is the IC statement of general covariance.
Concretely:

- the **one-branch observer** -- an `INC` scheme that drives the queue
  straight to a single leaf and stops: a classical, "collapsed"
  measurement, one definite outcome;
- the **fair observer** -- the default breadth-first walk: it never
  falls down one infinite branch, so it is *complete* -- every
  reachable normal form is eventually emitted;
- the **cost-ordered observer** -- the ATP / SAT use case: `INC`
  weights are a search heuristic, and the observer walks the
  entailment cone cheapest-first.  The branching heuristic of a solver
  *is* a reference frame.

When you want "the multiway graph as observed by *this* collapser,"
keep the slice / `INC` events; when you want "the multiway graph,
period," drop them -- they carry no states-graph and no branchial
content, only foliation order.

## 5. Relation to the Wolfram Physics Project

The Wolfram Physics Project (WPP) studies hypergraph rewriting where the
*multiway* structure -- *given* causal invariance -- yields relativity
(the causal graph foliated different ways = different reference frames,
all physically equivalent) and the *branchial* structure yields quantum
mechanics (branchial space as an emergent metric, amplitudes as path
counts, "a quantum observer is a classical observer in branchial
space").  Two of its load-bearing ideas land directly on the
Interaction Calculus:

- **Causal invariance, not just confluence, is IC's free lunch.**
  Confluence says any two reduction orders *end at the same state*;
  causal invariance (Wolfram's term) is strictly stronger -- any two
  reduction orders *produce isomorphic causal graphs*, i.e. the same
  events in the same partial order, only interleaved differently in
  clock time.  WPP has to engineer this property into its rules; for
  most string-rewriting systems it holds only asymptotically (initial
  branchings wash out in the limit), and you study the rules that
  satisfy it because *those* are the rules for which a
  foliation-independent picture of physics is recoverable.

  Interaction nets satisfy it strictly, by construction (§3.1 layer
  (4)): the per-active-pair rule discipline makes every event
  determined by the net (each redex fires once) and every causal
  dependency determined by wire-provenance, so different schedules
  are interleavings of the *same* labelled DAG.  Every IC program is
  automatically in the regime where the *content* of a collapse (the
  multiset of normal forms) *and the entire causal / branchial
  structure of how it got there* are foliation-independent; only the
  *order* an observer perceives them in is free -- which is what
  `INC` sets.  WPP's hard-won general covariance is, for IC, a
  theorem you get for nothing.  It is also what makes both "the
  multiway *states* graph of a trace" a well-defined quotient
  (identify equal intermediate states; slide events are non-edges;
  the quotient is a graph, not a tree with arbitrary cuts) *and*
  "the causal graph of a trace" a program invariant rather than an
  artifact of the reducer's schedule -- so any host-side
  reconstruction of these views from a logged reduction is well-posed.

- **Branchial space is IC's heap, and optimal reduction keeps it
  tight.**  The Wolfram Physics Project *draws* branchial space as an
  emergent metric on states ("how recently did they share an
  ancestor").  IC *is that metric, as a data structure*: two states
  are branchially close iff they still share structure in memory, and
  Lévy-optimal sharing is the discipline that never copies a sub-term
  two branches still agree on -- i.e. it keeps the representation of
  the slice exactly as large as the branchial geometry forces, and no
  larger.  Read through this lens, the IC optimality theorem is a
  statement about *not paying for branchial structure you don't need.*
  Variants of IC with *dynamic* labels (where `SUP` and `DUP` labels
  are computed at run-time rather than fixed at parse time) are then a
  way to *grow* the branchial coordinate system as a computation
  proceeds -- new branch dimensions whose identities only exist once a
  decision has been taken to introduce them.

- **The ruliad, in miniature.**  Interaction combinators are universal
  (Lafont) and IC is causally invariant, so "the multiway system of
  interaction combinators" is a faithful, causally-invariant slice of
  the ruliad
  (Wolfram's name for the entangled limit of all computations under
  all rules); a single IC program with `SUP`s is a finite window onto
  it; an `INC`-guided collapse is an observer sampling that window
  along one foliation.  IC is, modestly, a tiny concrete
  ruliad-explorer in which the observer (the collapser) is itself a
  first-class, programmable object.

A caveat so the analogy isn't oversold: the Wolfram Physics Project's
branching is the *implicit* kind (a rule matches in many places, and
you must *establish* causal invariance), while IC's is the *explicit /
data* kind (`SUP`) plus the *harmless* implicit kind (redex order,
invariant automatically).  IC is not a model *of* WPP physics; it is a
*different* multicomputational system that happens to sit, by
construction, in the corner the Wolfram Physics Project has to work to
reach.  The honest translation: **WPP's causal invariance is what the
interaction-net discipline delivers strictly (rather than only
asymptotically); WPP's emergent branchial space is IC's literal
heap-sharing; WPP's reference-frame freedom is IC's `INC` foliation.**

## 6. Why bother

- A **multiway visualiser for IC programs** -- "see the branchial
  structure of this collapse" -- is the natural debugger for anything
  superposition-heavy: SAT enumeration, IC-native ATP search,
  generators that enumerate witnesses by superposing the search space.
  Spurious merges (label collisions) and runaway splits (a branch
  dimension you didn't mean to add) are exactly the bugs that are
  invisible at the term level and obvious in the branch tree.
- It makes the **observer a first-class, inspectable object**: "this
  `INC` scheme is this foliation is this traversal order" stops being
  folklore about the collapser and becomes a graph you can look at.
- And it puts a small, *causally-invariant by construction*
  multicomputational system -- with first-class observers -- next to
  Wolfram's, where the contrast (free confluence, branchial space as
  literal sharing, `INC` as foliation) is the interesting part.
