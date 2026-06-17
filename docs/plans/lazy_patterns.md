# Lazy Patterns On thvm

Status: proposal.

## Premise

Two sibling Wolfram paclets are *reference reading* for the symbolic
engine that should live next to thvm:

- **`Wolfram/Lazy`** ([../../../wolfram/WolframLazy/](../../../wolfram/WolframLazy)).
  The lazy substrate: `LazyValue`, `LazyList`/`Cons`, `LazyTree`, an
  ExprStruct-backed `LazyExpression` shared-ref DAG, plus a wide
  combinator library (`LazyMap`, `LazyFold`, `LazySplits`,
  `LazyPermutations`, `LazyTuples`, `LazyRange`, `LazyTraverse`,
  `MultiEvaluate`, ...).
- **`Wolfram/Patterns`** ([../../../wolfram/WolframPatterns/](../../../wolfram/WolframPatterns)).
  Reifies every WL pattern primitive (`Blank*`, `Pattern`, `Repeated`,
  `Optional`, `Alternatives`, `OrderlessPatternSequence`,
  `KeyValuePattern`, `OptionsPattern`, `Condition`, `PatternTest`,
  `Except`, `Nested`, ...) into a symbolic match algebra of
  `MatchSum`/`MatchProduct`/`MatchPart`/`MatchValues`.

Both are research prototypes.  thvm does **not** depend on them and we
do **not** vendor them.  They are design references; we reimplement
what we need inside `WolframInstitute`THVMLink``, specialised to the IC backing.

## Insight

HVM/Bend already have *two* established list-shaped representations,
and thvm inherits both via the same heap primitives:

1. **`Cons head tail` / `Nil` (CTR list)** - a sequence of distinct
   elements meant to be iterated *once*.  This is what `map.hvm`
   uses (`#Cons{#Z{}, #Cons{#S{#Z{}}, #Nil{}}}`).  Laziness comes
   from the WNF reducer not forcing the tail until something
   demands it.
2. **`&L{head, &L{head', tail'}}` (SUP stream)** - a superposition
   of *alternatives*, enumerated by the collapser.  This is the
   canonical HVM idiom for a lazy stream:
   `@X = &L{#Z, #S{@X}}` is the infinite-naturals example
   ([TinyHVM/HVM4/test/collapse_9.hvm](../../TinyHVM/HVM4/test/collapse_9.hvm)).
   Same-label SUPs annihilate pairwise; different-label SUPs
   cross-product
   ([clang/eval/collapse.c](../../TinyHVM/HVM4/clang/eval/collapse.c),
   [docs/hvm/collapser.md](../../TinyHVM/HVM4/docs/hvm/collapser.md)).

The two encodings are *not* interchangeable.  CTR lists are inert
linked structures; SUP streams carry the optimal-sharing semantics
that make `f(&L{a, b, c}) = &L{f(a), f(b), f(c)}` via APP-SUP
distribute work across alternatives in one shared computation.
That's the SupGen / NeoGen mechanism, and it is exactly what the
ATP arc's Stage 8.1 CP cross-product enumeration relies on.

Picking the right encoding per use case:

| use case                                    | encoding                            |
| ------------------------------------------- | ----------------------------------- |
| `LazyPermutations[xs]`, `LazySplits[xs, n]` | **SUP**                             |
| `LazyTuples[...]`, `LazySubsets[xs]`        | **SUP**                             |
| `LazyRange[1, 5]` (concrete sequence)       | CTR `Cons`/`Nil`                    |
| `MatchSum[a, b, ...]`                       | **SUP**                             |
| `Alternatives[a, b]` pattern                | **SUP**                             |
| `OrderlessPatternSequence` over `args`      | **SUP** over permutations           |
| `MatchProduct[...]`                         | CTR                                 |
| `MatchPart[path, p, m]`                     | CTR                                 |
| `MatchValues[v, ...]`                       | CTR leaf                            |
| `Condition` / `PatternTest` failed branch   | `ERA` (via `TAG_PRI` predicate)     |

The ATP arc has already shipped the SUP half: Stage 8.1 encodes the
CP cross product as nested labelled SUPs and lets DUP-SUP commute
drive enumeration; Stage 5.3 wraps priority candidates in `INC^k` so
collapse pops cheapest-first; `TAG_PRI` (Stage 8.1b) routes pure-C
predicates back into the reducer.  Lazy streams and patterns fit on
the same machinery without inventing new tags.

## Goal

A symbolic, WL-native lazy + pattern layer inside thvm, where:

1. **Lazy streams are SUP-collapse-driven on the kernel side from
   day one.**  `TLazyPermutations[xs]`, `TLazySplits[xs, n]`,
   `TLazyTuples[{...}]`, `TLazyRange[from, to]` build labelled SUP
   subgraphs in the heap.  Forcing one element of the stream is one
   `thvm_collapse_grouped` step on the C side.
2. The WL surface (`TLazyList`/`TLazyValue` heads, `TLazyMap`,
   `TLazyFold`, `TLazyTake`, `TLazyFirst`, `TLazyRest`, ...) is a
   thin wrapper over kernel-side handles.  WL composition does not
   eagerly enumerate; it only schedules.
3. Pattern matching is built on top.  `THVMPatternMatch[expr, patt]`
   compiles patt + expr to an IC subgraph (sequence patterns reuse
   the same `TLazySplits` / `TLazyPermutations` SUPs as the lazy
   substrate); the result is a `TMatchSum`-shaped lazy stream.
4. The user writes ordinary WL patterns; results are streamed back
   as `Take[matches, k]` triggers exactly `k` collapse steps.
5. The whole layer interfaces "naturally" with WL.  `TLazyList`
   answers `Map`, `Length`, `First`, `Take`, `Join`, `Fold`,
   `Catenate`, `Part` via UpValues; `TMatchSum` works with the
   `TMatchBindings` / `TMatchApply` / `TMatchExpand` rebuilders.

This is *not* the bufferize plan.  It is the symbolic-engine half of
thvm: the layer that lets WL users speak their native pattern
language and have it run on the IC heap with optimal sharing.  The
ATP arc is its first heavy user.

## Architecture

The bottom-up dependency order is `Lazy.wl` -> `Pattern.wl` -> ATP
integration.  **No new C is needed for the lazy/pattern surface
itself.**  The existing THVMLink bridge already exposes everything
this layer composes:

- `TSup[a, b]` / `TSup[label, a, b]`  - construct a labelled SUP cell.
- `TDup[label, body, k]`               - construct a DUP and run a
                                          continuation `k[dp0, dp1]`.
- `TLam`, `TApp`, `TEra`, `TVarFor`    - the rest of the IC alphabet.
- `TWnf` / `TStep` / `TInteract`       - reduce / single-step.
- `TRedexes`                           - enumerate live redexes.
- `TTermExpr` / `TTermTree` / `THeapRead` - inspect heap shape.
- `Pri.wl`                             - WL-callable `TAG_PRI`
                                          callbacks for `Condition`
                                          and `PatternTest`.

DUP-SUP same-label annihilation, APP-SUP commute, APP-LAM beta,
APP-PRI dispatch, and the rest of the interactions are already in
`src/interact/`.  The ATP arc has shipped both the SUP cross-product
encoding (Stage 8.1) and the `INC^k` priority collapse (Stage 5.3).
Lazy and Pattern are *consumers* of this machinery, not producers
of new C.

### Layer 1 - `wl/THVMLink/Kernel/Lazy.wl`

Pure WL.  Two stream encodings, both built on existing `TSup` /
`TApp` / `TLam` / `TEra` constructors:

- **`TSupStream`** - a `TTerm` whose root tag is `SUP`.  Used for
  *alternative-enumeration* generators (`TLazyPermutations`,
  `TLazySplits`, `TLazyTuples`, `TLazySubsets`).  Consumed by
  walking the SUP tree, with `TWnf` to force branches that contain
  predicates (`TAG_PRI` calls returning `ERA` on failed
  branches).  This is the
  [`@X = &L{#Z, #S{@X}}`](../../TinyHVM/HVM4/test/collapse_9.hvm)
  HVM idiom.
- **`TConsList`** - a `TTerm` shaped as `Cons head tail` / `Nil`
  via existing CTR cells (`TAG_CTR` + label).  Used for *concrete
  sequence* generators (`TLazyRange`) and for the consumer-side
  output of `TLazyToList` etc.  This is the `map.hvm` idiom.

Generators:

- `TLazyRange[from, to, step]`     -> `TConsList`.
- `TLazyPermutations[xs]`          -> `TSupStream`.
- `TLazySplits[xs, n]`             -> `TSupStream`.
- `TLazyTuples[{xs1, xs2, ...}]`   -> `TSupStream`.
- `TLazySubsets[xs]`               -> `TSupStream`.

`TSupStream` generators mint a fresh label per call via
`TFreshLabel[]`; same label across all branches of one generator
so DUP-SUP same-label annihilation lets a caller `TDup` the stream
to fork it.  Different generators get different labels so cross-
producting (e.g. permutations of one set crossed with splits of
another) just nests them and lets the collapser cross-product the
labels naturally.

Consumer combinators are polymorphic over both encodings.  WL
inspects the root tag (`SUP` vs `CTR` with `Cons`/`Nil` label) and
dispatches.  Forcing uses `TWnf`:

- `TLazyTake[s, n]`     - walk `n` left-spine steps, return list.
- `TLazyFirst[s]`       - one step, return head as `TTerm`.
- `TLazyRest[s]`        - return the right-spine subterm.
- `TLazyMap[f, s]`      - on `TSupStream`: `TApp[f, s]` + APP-SUP
                          commute distributes one `f` across all
                          branches with shared work.  On
                          `TConsList`: standard map down the spine.
- `TLazyFold[f, x, s]`  - walk + accumulate WL-side.
- `TLazyToList[s]`      - force every element, return as a WL
                          `List` (collapses a `TSupStream` via
                          left-spine walk; eagerly walks a
                          `TConsList`).
- `TLazySelect[s, p]`   - `TApp[gateAsLam[p], s]` so failed
                          branches collapse to `ERA` via the
                          existing `Pri.wl` `TAG_PRI` callback.

Idiomatic UpValues per [wl/GUIDE.md](../../wl/GUIDE.md) so either
encoding answers `Map`, `Length`, `First`, `Take`, `Drop`, `Fold`,
`Catenate`, `Part` directly.

Formatting box: `MakeBoxes` for both encodings shows the forced
prefix plus a `[...]` tooltip carrying the heap loc and label, so
debug printouts do not silently force the tail.

What we deliberately do **not** ship in this layer:

- `LazyTree` / `LazyTreeEdges` - not on the pattern path.
- `LazyTraverse` / `MultiEvaluate` - not on the pattern path.
- `LazyExpression` ExprStruct refs - not needed; the heap *is* the
  shared-ref DAG.

The point: every "alternative-enumeration" generator on the surface
is *literally* a SUP in the heap, matching the HVM/Bend convention.
Concrete sequences stay as Cons/Nil CTR lists.  WL never enumerates
by recursion in the abstract; it walks a heap term, forcing branches
via `TWnf` only when needed.

### Layer 2 - `wl/THVMLink/Kernel/Pattern.wl`

WL surface for the pattern engine, built on Layer 1:

```wolfram
THVMPatternMatch[expr_, patt_, opts___]   := ...
THVMPatternBindings[expr_, patt_]         := ...
THVMPatternReplace[expr_, lhs_ :> rhs_]   := ...
THVMPatternReplaceAll[expr_, rules_]      := ...
```

Match algebra heads (under `WolframInstitute`THVMLink``):

- `TMatch[m]`             - opaque match wrapper around a SUP term.
- `TMatchSum[a, b, ...]`  - decoded view of a labelled SUP.
- `TMatchProduct[...]`    - product of conjunct decompositions
                            (a CTR encoded via nested heap cells).
- `TMatchPart[path, p, m]`- path tag + sub-pattern + sub-decomp.
- `TMatchValues[v, ...]`  - leaf bindings (a `TTerm` carrying the
                            captured values).
- `TMatchBindings`, `TMatchApply`, `TMatchExpand`, `TMatchDefault`,
  `TMatchParts`            - rebuilders, pure WL.

Pattern compilation is pure WL too: each WL pattern primitive maps
to a constructor tree built from `TSup` / `TApp` / `TLam` / `TPri` /
`TEra`:

- `Blank` / `Blank[h]`              -> a `TPri` head-test followed
                                        by `TMatchValues`.
- `Pattern[x, p]`                   -> compile `p`, wrap leaves in
                                        `TMatchValues[x -> v]`.
- `Alternatives[a, b, ...]`         -> `TSup[label, compile[a],
                                        TSup[label, compile[b], ...]]`.
- `Except[p]`                       -> `TPri` head-test that returns
                                        `TEra` on match success.
- `Condition[p, c]`                 -> compile `p`, wrap each leaf in
                                        a `TPri` calling `c`; failed
                                        leaves rewrite to `TEra`.
- `PatternTest[p, t]`               -> same with `t[v]`.
- `KeyValuePattern[{...}]`          -> compile per-key, product via
                                        nested CTRs.
- `BlankSequence` / `BlankNullSequence` /
  `Repeated` / `PatternSequence`     -> reuse `TLazySplits`.
- `OrderlessPatternSequence`        -> reuse `TLazyPermutations`
                                        composed with `TLazySplits`.
- `OptionsPattern`                  -> reuse `TLazySubsets`.
- `Optional`                        -> `TSup[label, compile[p],
                                        defaultLeaf]`.
- `Shortest` / `Longest`            -> attribute on the surrounding
                                        SUP traversal order.
- `HoldPattern` / `Verbatim` /
  `IgnoringInactive`                -> compile inner unchanged.
- `Nested` / `NestedNull` /
  `DeepPattern`                     -> recursive SUP over subterm
                                        positions of `expr`.

Sequence cases *reuse* `TLazySplits` and `TLazyPermutations` from
Layer 1 - the SUP that powers `TLazyPermutations[args]` is the same
SUP that powers `OrderlessPatternSequence` matching against `args`.
No duplicate machinery.

`Condition` / `PatternTest` reach the WL predicate via `Pri.wl`:
the predicate is registered as a `TAG_PRI` callback at compile time,
and the kernel calls back during `TWnf`.  This already works for the
ATP arc; we are reusing it.

### Layer 3 - Decoding `TMatchSum` lazily

The decoded match tree is itself a SUP-rooted `TTerm`.  `Take[matches,
5]` walks the SUP left-spine 5 times, calling `TWnf` on each branch
to force any `TPri` predicates (`Condition` / `PatternTest`) and
discard `TEra` failures.  Five matches <-> five forces.  The contract
is checked from WL by inspecting `TItrs[]` deltas.

`TMatchBindings`, `TMatchApply`, `TMatchExpand` are pure WL on top
of the decoded tree, ported slot-for-slot from the upstream
`Wolfram/Patterns` algebra (no upstream runtime dep).

### Layer 4 - ATP integration

The ATP arc currently uses bespoke `thvm_match` / `thvm_unify`
primitives over CTR/FVR terms.  Once the WL pattern engine works,
add it as a parallel narrowing path: any rule whose LHS uses
sequence / alternatives / conditions can be tried via
`tpattern_match_first` instead of `thvm_match`.  First user is the
Stage 10a Wolfram-axiom Boolean corpus.

## Phases

### Phase 0 - `Lazy.wl` over existing primitives

This is the starting point.  Goal: `TLazyPermutations[xs]`,
`TLazySplits[xs, n]`, `TLazyTuples[...]`, `TLazyRange[...]` build
SUP-rooted heap terms; `TLazyTake[stream, k]` walks the SUP left
spine `k` times and forces minimally.

**No C changes.**  Everything composes existing THVMLink
constructors (`TSup`, `TApp`, `TLam`, `TEra`) and existing reducers
(`TWnf`, `TStep`, `TInteract`).

Tasks:

1. `wl/THVMLink/Kernel/Lazy.wl` ships the generators
   (`TLazyRange`, `TLazyPermutations`, `TLazySplits`, `TLazyTuples`,
   `TLazySubsets`) and the consumer combinators (`TLazyTake`,
   `TLazyFirst`, `TLazyRest`, `TLazyMap`, `TLazyFold`,
   `TLazyToList`, `TLazySelect`, `TLazyJoin`, `TLazyCatenate`).
2. Idiomatic UpValues: a SUP-rooted `TTerm` answers `Map`, `Length`,
   `First`, `Take`, `Drop`, `Fold`, `Catenate`, `Part`.
3. `MakeBoxes` for SUP-rooted `TTerm`: forced prefix + `[...]`
   tooltip with heap loc + label.
4. `wl/THVMLink/Tests/lazy.wlt`:
   - `TLazyTake[TLazyRange[5], 5]` -> `{1, 2, 3, 4, 5}`.
   - `TLazyToList @ TLazyPermutations[{a, b, c}]` -> 6 perms.
   - `TLazyToList @ TLazySplits[{a, b, c, d}, 2]` -> 5 2-splits.
   - `TLazyMap[f, TLazyRange[3]] // TLazyToList` ->
     `{f[1], f[2], f[3]}`.
   - Laziness contract: `TItrs[]` delta from a fresh state to
     `TLazyTake[TLazyPermutations[Range[6]], 10]` is bounded
     linearly in 10 - i.e. taking 10 elements does not force the
     remaining 710 permutations.

Acceptance: `make wl-test` passes `lazy.wlt`; lint-clean per
[wl/GUIDE.md](../../wl/GUIDE.md); the laziness contract holds.

### Phase 1 - Match algebra heads + non-sequence patterns

`wl/THVMLink/Kernel/Pattern.wl` lands the match algebra heads,
`TMatchBindings` / `TMatchApply` / `TMatchExpand`, and pattern
compilation for the *non-sequence* primitives.  These compile to
small SUP graphs without needing `TLazySplits` / `TLazyPermutations`,
so they are independent of Phase 0's sequence cases:

- `Blank`, `Blank[h]`, `Pattern`, `HoldPattern`, `Verbatim`,
  `Alternatives`, `Except`, `Condition`, `PatternTest`,
  `KeyValuePattern` (flat keys).

`Condition` and `PatternTest` register WL predicates as `TAG_PRI`
callbacks via `Pri.wl`'s existing API; no new C work for the
predicate plumbing.

Tests (`pattern_basics.wlt`):

- One assertion per primitive comparing
  `TMatchBindings @ THVMPatternMatch[expr, patt]` to the expected
  list of binding associations.
- Replace `THVMPatternReplace[expr, lhs :> rhs]` agrees with WL
  `Replace[expr, lhs :> rhs]` for the supported primitive set.

Acceptance: bindings agree with WL's built-in `MatchQ` / `Cases` /
`ReplaceList` for every fixture in the supported primitive set.

### Phase 2 - Sequence patterns reusing Phase 0 generators

Implement sequence primitives by *reusing* `TLazySplits` and
`TLazyPermutations`:

- `BlankSequence` / `BlankNullSequence` / `Repeated` /
  `RepeatedNull` / `PatternSequence` -> `TLazySplits`.
- `OrderlessPatternSequence` -> `TLazyPermutations` then
  `TLazySplits`.
- `OptionsPattern` -> `TLazySubsets` over the option keys.
- `Shortest` / `Longest` -> attribute on the SUP enumeration order.

Tests: parity with WL `Cases` / `ReplaceList` on a fixture set
covering each primitive.  Stress: deep `OrderlessPatternSequence`
to exercise SUP fan-out depth.

Acceptance: the engine matches every fixture in
[`Wolfram/Patterns/Documentation/English/ReferencePages/Symbols/`](../../../wolfram/WolframPatterns/Documentation/English/ReferencePages/Symbols)
that uses a primitive we have implemented.

### Phase 3 - WL `Replace` / `ReplaceAll` integration

Once `THVMPatternMatch` is solid, point built-in `/.` and `//.` at
the IC driver via `Driver -> "thvm"` on `THVMPatternReplaceAll`.
This is the "natural WL" outcome: any rewrite expressed in WL syntax
runs on the IC heap.

### Phase 4 - ATP wiring

Add the WL pattern engine as a parallel narrowing path in the ATP
loop.  First user: Stage 10a Wolfram-axiom Boolean corpus.

Acceptance: at least one ATP fixture proves with WL-pattern rules
and matches the existing CTR/FVR baseline byte-for-byte where rule
shapes overlap.

## Tests

Cross-cutting:

- `wl/THVMLink/Tests/lazy.wlt`               (Phase 0)
- `wl/THVMLink/Tests/pattern_basics.wlt`     (Phase 1)
- `wl/THVMLink/Tests/pattern_sequence.wlt`   (Phase 2)
- `wl/THVMLink/Tests/pattern_replace.wlt`    (Phase 3)
- `wl/THVMLink/Tests/atp_pattern.wlt`        (Phase 4)

Laziness is asserted through `TItrs[]` deltas, not new C tests:
`k` forced elements should produce a `TItrs[]` delta linear in `k`,
not in the full SUP fan-out size.

## Non-Goals

- Not the bufferize plan ([bufferize.md](bufferize.md)).
- Not a replacement for `thvm_match` / `thvm_unify`; the ATP CTR/FVR
  paths stay authoritative until Phase 4 proves parity.
- No new tags.  SUP, DUP, CTR, ERA, FVR, PRI, INC are all we need;
  the ATP arc has shipped them.
- No vendoring or submoduling.  `Wolfram/Lazy` and `Wolfram/Patterns`
  stay reference reading only.
- No pure-WL fallback driver.  The Lazy and Pattern surfaces *are*
  the IC backing from day one; an oracle, if one is wanted during
  development, is built ad-hoc against WL's own `MatchQ` / `Cases` /
  `ReplaceList` rather than against a parallel pure-WL clone.

## Open Questions

- `Pri.wl`'s callback path already handles WL functions invoked
  during `TWnf`; verify it survives nested `TWnf` re-entry that a
  predicate inside a SUP branch may trigger.  Quick smoke test
  before Phase 1 lands.
- SUP fan-out for `TLazyPermutations[xs]` with `Length[xs]` large
  costs `n!` heap cells at *construction* time even though
  enumeration is lazy.  For our use cases (pattern matching on
  short sequences, ATP CP enumeration) this is fine; if a future
  caller wants thunked construction we add a generator that emits
  a SUP whose right branch is an unforced `TApp` to a
  rest-builder lambda.

## Next Step

Phase 0.  Build `wl/THVMLink/Kernel/Lazy.wl` over existing
`TSup` / `TWnf` so that `TLazyTake[TLazyPermutations[Range[6]], 10]`
returns 10 elements with a `TItrs[]` delta linear in 10, not in
720.  No C changes.  Everything else gates on this contract.
