# Plan: WL scaffold for KOpt rewriting -- native Rule design over symbolic UOp DAG

Status: design proposal, no code written yet.

## What this is

A WL (Wolfram Language) layer that re-expresses thvm's KOpt vocabulary
as **symbolic rewrite rules** over a symbolic UOp DAG, then
cross-validates against the C implementation. Not a foreign-function
wrapper; a parallel rule engine using WL's native `Rule` /
`RuleDelayed` / `ReplaceAll` machinery, which is exactly the
abstraction `apply_opt_dag.c` reimplements in 370 LOC of manual
walker.

The win: WL is **rewrite-native**. The 11 KOpts (TC, GLOBAL, UPCAST,
UNROLL, LOCAL, GROUP, GROUPTOP, SWAP, FAST_MATH, SIMD_REDUCE,
VEC_LOAD) are each one-line `RuleDelayed` patterns over UOp head
symbols. The walker is `ReplaceAll` (`/.`). The composition is
function composition. The cross-check is structural equality between
WL-output and C-output (the latter inspected via `term_*` accessors
through the existing `wl/THVMLink/` LibraryLink bridge).

## Part 1 -- how C-side KOpt rewriting works today

### The vocabulary (`src/thvm.h:529-542`)

```c
KOP_NONE     = 0
KOP_UPCAST   = 1   // split axis: outer LOOP + inner UPCAST(factor)
KOP_UNROLL   = 2   // same shape, axis_type = UNROLL
KOP_LOCAL    = 3   // same shape, axis_type = LOCAL (-> tt)
KOP_GROUP    = 4   // reduce axis split: outer LOOP + inner GROUP_REDUCE
KOP_GROUPTOP = 5   // reduce split, factor on the outer side
KOP_SWAP     = 6   // swap two axes' positions
KOP_PADTO    = 7   // (alignment, not yet wired in DAG path)
KOP_NOLOCALS = 8   // (sentinel, no rewrite)
KOP_TC       = 9   // wrap matmul reduce with OPT(_, TC, factor)
KOP_GLOBAL   = 10  // axis_type swap LOOP -> GLOBAL (-> tg)
KOP_FAST_MATH    = 11  // wrap unary ops with OPT(_, FAST_MATH, _)
KOP_SIMD_REDUCE  = 12  // wrap REDUCE with OPT(_, SIMD_REDUCE, _)
KOP_VEC_LOAD     = 13  // wrap INDEX_E with OPT(_, VEC_LOAD, width)
```

Each KOpt has a matching annotation opcode (`UOP_OPT_*`) wrapped
around the target node, OR a direct mutation of a RANGE leaf's
`axis_type`. The renderer (`src/codegen/render_uop.c`) reads the
post-mutation DAG: `UOP_OPT(_, TC, _)` triggers the simdgroup_matrix
template; `UOP_RANGE.axis_type == KAX_GLOBAL` drops the sgi-guard;
`UOP_OPT(_, FAST_MATH, _)` peels in `rmu_emit_term` to emit
`fast::exp2` etc.

### The DAG-side mutator (`src/uop/apply_opt_dag.c`)

One helper per KOpt class, dispatched by `uop_dag_apply_kopt(root,
opt)`. Six categories:

| Category | KOpts | Mutation shape | LOC |
|---|---|---|---|
| **Wrap-target** | TC, FAST_MATH, SIMD_REDUCE, VEC_LOAD | Find target node(s), wrap with `UOP_OPT(_, kind, factor)` | ~50 each |
| **Axis-type swap** | GLOBAL | Find RANGE(axis, LOOP, ext), rebuild as RANGE(axis, GLOBAL, ext); rewriter substitutes refs | ~20 |
| **Axis split** | UPCAST, UNROLL, LOCAL, GROUP, GROUPTOP | Replace RANGE(axis, LOOP, N) with two RANGEs (outer LOOP N/k, inner kind k); rewrite all addr terms `r` -> `IADD(IMUL(outer, k), inner)` | ~120 |
| **Axis swap** | SWAP | Bidirectional substitution of two axes; manual single-pass walker (uop_graph_rewrite would loop) | ~80 |
| **No-op** | NONE, NOLOCALS | Return root unchanged | 0 |
| **Pending** | PADTO | Not yet wired in DAG path | -- |

All 11 active KOpts use a **manual Term-keyed memo walker** because
`uop_graph_rewrite` recurses into rule outputs (would re-fire on its
own outputs and loop).

### The propose -> apply -> render -> score loop

```
propose.c::kernel_opts_propose(ke, out_kopts, cap) -> n
  - Reads ke->cached_lift.store_root (DAG-aware classifiers)
  - Returns small list of KOpt(op, axis, arg) candidates per shape

apply_opt.c::kernel_apply_opt(ke, opt) -> 1 on success
  - DAG path: ke->cached_lift.store_root = uop_dag_apply_kopt(root, opt)
  - Legacy path: ke->schedule->applied_opts[] mutation

render_uop.c::cg_render_uop_kernel_root(root, name, fp)
  - Walks the post-mutation DAG, emits MSL string
  - Pattern-matches OPT wrappers + axis_type to fire specialized templates

bench (autotune.c BEAM):
  - For each candidate, apply -> compile metallib -> dispatch -> time
  - Pick winner, cache key = (shape, opt sequence)
```

### Properties the WL scaffold preserves

- **Hash-cons**: thvm interns Terms by `(opcode, args)`; identical
  rewrites yield identical Term IDs. WL's pattern-rewrite is
  structurally idempotent on identical input.
- **Composition associativity**: applying `[KOptTC, KOptGlobal]`
  produces the same DAG as `[KOptGlobal, KOptTC]` for non-overlapping
  rewrite targets. WL `Composition` enforces this.
- **Stop-on-no-change**: when a KOpt's pattern doesn't match, the DAG
  is returned unchanged (Term identity). WL `ReplaceAll` returns the
  same expression on no-match.

## Part 2 -- why WL is the right host for the symbolic spec

WL's expression rewriter is the canonical implementation of exactly
the operation `apply_opt_dag.c` performs. Concretely:

```wolfram
(* C: apply_opt_dag_global_rewrite scans every term, rebuilds parents *)
(* WL: one rule, applied via /. (ReplaceAll) walks the expression tree *)
KOptGlobal[axis_] := expr |->
  expr /. URange[axis, KAX$LOOP, ext_] :> URange[axis, KAX$GLOBAL, ext]
```

WL handles the parent-reconstruction + memoization automatically.
Hash-consing by structural equality is built-in. The 80 LOC of manual
SWAP walker in `apply_opt_dag.c` becomes:

```wolfram
KOptSwap[a_, b_] := expr |->
  expr /. {URange[a, t_, e_] :> URange[b, t, e],
           URange[b, t_, e_] :> URange[a, t, e]}
```

WL is **also** the right host for the *spec*: a rule reads as the
mathematical statement of the rewrite ("any RANGE on axis k with type
LOOP becomes GLOBAL"). The C implementation is the same statement
encoded in C control flow. Diverging the two is a real risk; having
WL as the source of truth for what a KOpt *should* do gives us a
spec-test against C drift.

## Part 3 -- the WL design

### 3.1 Symbolic UOp DAG representation

One head per UOp opcode. Lowercase prefix `U` to namespace, mirroring
existing `TUOp*` constructors in [wl/THVMLink/Kernel/Optim.wl](../../wl/THVMLink/Kernel/Optim.wl)
but symbolic (no Term IDs):

```wolfram
(* Buffers and ranges are leaves *)
UBuffer[scope_, dtype_, dims_List, instance_]
URange[axis_, axisType_, extent_]
UICONST[value_]
UFCONST[value_]
UInvalid[]

(* Index-layer arithmetic *)
UIAdd[a_, b_]
UIMul[a_, b_]
UIDiv[a_, b_]
UIMod[a_, b_]
UILT[a_, b_]
UIAnd[a_, b_]
UIWhere[cond_, t_, e_]

(* INDEX_E pairs a buffer with an address tree *)
UIndexE[buffer_, addr_]

(* FP arithmetic *)
UAdd[a_, b_]; UMul[a_, b_]; UNeg[x_]; URecip[x_]
UExp2[x_]; ULog2[x_]; USqrt[x_]
UCmpLt[a_, b_]; UCmpEq[a_, b_]
UCast[src_, dtype_]; UBitCast[src_, dtype_]

(* Reduces and stores *)
UReduce[kind_, axis_, src_]
UStore[buf_, addr_, value_]
UAfter[node_, after_]
ULoad[src_]

(* OPT annotation -- the pivot for KOpt wrapping *)
UOpt[target_, kind_, factor_]

(* Constants for axis_type, OPT kind, dtype *)
KAX$LOOP = 0;  KAX$REDUCE = 1;  KAX$UPCAST = 2;  KAX$UNROLL = 3
KAX$LOCAL = 4; KAX$GLOBAL = 5;  KAX$GROUP_REDUCE = 6
OPT$UNROLL = 0; OPT$UPCAST = 1; OPT$TC = 2; OPT$LOCAL = 3
OPT$GROUP_REDUCE = 4; OPT$CONV = 5
OPT$FAST_MATH = 6; OPT$SIMD_REDUCE = 7; OPT$VEC_LOAD = 8
DT$INT32 = 5; DT$FP32 = 13
REDUCE$SUM = 0; REDUCE$MAX = 1
```

### 3.2 KOpt rules (one rule each)

```wolfram
(* === Wrap-target rules ===========================================
 * Each takes a target shape and wraps with UOpt[_, kind, factor].
 * Idempotent: re-applying the same KOpt at the same factor returns
 * the same expression (structural equality). *)

(* TC: wrap inner REDUCE inside STORE.value with OPT(_, TC, factor).
 * Replaces existing TC factor if already wrapped. *)
KOptTC[factor_Integer] := expr |->
  Replace[expr, {
    UStore[b_, a_, UReduce[k_, ax_, body_]] :>
      UStore[b, a, UOpt[UReduce[k, ax, body], OPT$TC, factor]],
    UStore[b_, a_, UOpt[UReduce[k_, ax_, body_], OPT$TC, _]] :>
      UStore[b, a, UOpt[UReduce[k, ax, body], OPT$TC, factor]]}, {0}]

(* FAST_MATH: wrap every UExp2/ULog2/USqrt/UNeg/URecip with OPT(_, FAST_MATH, 0).
 * Stop-leaf for already-wrapped to avoid OPT-of-OPT. *)
KOptFastMath := expr |->
  expr //. {
    UOpt[u_, OPT$FAST_MATH, _] :> UOpt[u, OPT$FAST_MATH, 0],
    op_[args__] /; MatchQ[Head[op_], UExp2|ULog2|USqrt|UNeg|URecip] &&
                   FreeQ[Head[op[args]], UOpt] :>
      UOpt[op[args], OPT$FAST_MATH, 0]}

(* SIMD_REDUCE: wrap every UReduce with OPT(_, SIMD_REDUCE, 0). *)
KOptSimdReduce := expr |->
  expr //. {
    UOpt[u_UReduce, OPT$SIMD_REDUCE, _] :> UOpt[u, OPT$SIMD_REDUCE, 0],
    r_UReduce /; FreeQ[Hold[r], UOpt] :>
      UOpt[r, OPT$SIMD_REDUCE, 0]}

(* VEC_LOAD: wrap every UIndexE whose addr is contiguous-shaped
 * (UIAdd[UIMul[outer, stride], inner_URange]) with OPT(_, VEC_LOAD, width). *)
KOptVecLoad[width_Integer] := expr |->
  expr //. UIndexE[buf_, addr:UIAdd[UIMul[_, _], _URange]] :>
    UOpt[UIndexE[buf, addr], OPT$VEC_LOAD, width]

(* === Axis-type swap ==============================================
 * GLOBAL: replace RANGE(axis, LOOP, ext) with RANGE(axis, GLOBAL, ext).
 * Hash-cons makes the new range a fresh value -- WL ReplaceAll walks
 * the whole expression and rebuilds parents whose children changed. *)
KOptGlobal[axis_Integer] := expr |->
  expr /. URange[axis, KAX$LOOP, ext_] :> URange[axis, KAX$GLOBAL, ext]

(* === Axis split (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP) ============
 * Split RANGE(axis, LOOP, N) into outer + inner (kind=innerKax),
 * substituting every reference to the original RANGE with
 * IADD(IMUL(outer, k), inner). *)
KOptSplit[innerKax_, axis_Integer, k_Integer] := expr |-> Module[
  {old, outer, inner, sub},
  old   = URange[axis, KAX$LOOP, _];
  outer = URange[axis, KAX$LOOP, _ /; True];          (* extent N/k via match *)
  inner = URange[axis + 1, innerKax, k];
  sub   = UIAdd[UIMul[outer, UICONST[k]], inner];
  expr /. URange[axis, KAX$LOOP, n_] :>
    UIAdd[UIMul[URange[axis, KAX$LOOP, Quotient[n, k]], UICONST[k]],
          URange[axis + 1, innerKax, k]]]

KOptUpcast[axis_, k_]   := KOptSplit[KAX$UPCAST,        axis, k]
KOptUnroll[axis_, k_]   := KOptSplit[KAX$UNROLL,        axis, k]
KOptLocal[axis_, k_]    := KOptSplit[KAX$LOCAL,         axis, k]
KOptGroup[axis_, k_]    := KOptSplit[KAX$GROUP_REDUCE,  axis, k]
KOptGrouptop[axis_, k_] := KOptSplit[KAX$GROUP_REDUCE,  axis, k]   (* differs only in factor placement *)

(* === SWAP: bidirectional axis swap ============================== *)
KOptSwap[a_Integer, b_Integer] := expr |->
  expr /. {URange[a, t_, e_] :> URange[\[FormalA], t, e],
           URange[b, t_, e_] :> URange[a, t, e],
           URange[\[FormalA], t_, e_] :> URange[b, t, e]}
  (* Three-step via a fresh symbol \[FormalA] avoids the bidirectional
   * loop the C apply_opt_dag SWAP rewriter sidesteps via a single-pass
   * walker; in WL we just stage through a placeholder. *)
```

### 3.3 Composition

KOpts compose via WL function composition. Order matters when KOpts
target overlapping subgraphs (e.g. SIMD_REDUCE then TC both wrap a
REDUCE).

```wolfram
ApplyKOptSeq[seq_List] := RightComposition @@ seq
(* ApplyKOptSeq[{KOptTC[8], KOptGlobal[0], KOptGlobal[1]}][dag] *)
```

### 3.4 The C bridge for cross-validation

Two existing infrastructure pieces:
- [wl/THVMLink/](../../wl/THVMLink/) -- LibraryLink paclet that already
  wraps `term_*` accessors and `TUOp*` constructors. Returns Term IDs
  (u64 atoms in WL).
- [py/csource/thvm_py.c](../../py/csource/thvm_py.c) -- ctypes wrapper
  exposing `kernel_apply_opt`, `uop_dag_apply_kopt`, render. Same
  surface from WL via `LibraryFunctionLoad` if we want to skip Python.

Bridge functions (added to a new `wl/THVMLink/Kernel/Rewrite.wl`):

```wolfram
(* Build C-side DAG mirroring a symbolic WL expression. Returns a
 * Term ID. Hash-consed automatically by thvm. *)
TUOpFromSymbolic[expr_] := expr /. {
  UBuffer[s_, dt_, dims_, inst_]   :> TUOpBuffer[s, dt, dims, inst],
  URange[ax_, t_, e_]              :> TUOpRange[ax, t, e],
  UICONST[v_]                      :> TUOpIConst[v],
  UFCONST[v_]                      :> TUOpFConst[v],
  UIAdd[a_, b_]                    :> TUOpIAdd[TUOpFromSymbolic[a], TUOpFromSymbolic[b]],
  ...
  UReduce[k_, ax_, src_]           :> TUOpReduce[k, ax, TUOpFromSymbolic[src]],
  UOpt[t_, k_, f_]                 :> TUOpOpt[TUOpFromSymbolic[t], k, f],
  UStore[b_, a_, v_]               :> TUOpStore[TUOpFromSymbolic[b],
                                                TUOpFromSymbolic[a],
                                                TUOpFromSymbolic[v]]}

(* Read a C-side DAG back into symbolic form. Walk via term_tag/ext/val
 * and recursive heap_read. *)
TUOpToSymbolic[term_Integer] := <recursive walk>

(* Apply a KOpt sequence on the C side via uop_dag_apply_kopt. *)
TUOpApplyKOptSeq[term_Integer, seq_List] := Fold[
  TUOpDagApplyKOpt[#1, #2[[1]], #2[[2]], #2[[3]]] &,
  term, KOptToTriple /@ seq]
```

### 3.5 Cross-validation harness

Two equivalent rewrites should produce structurally-equal DAGs:

```wolfram
CrossValidateKOpt[dag_, seq_List] := Module[{wlOut, cTerm, cBack},
  wlOut = ApplyKOptSeq[seq][dag];                        (* WL rule rewrite *)
  cTerm = TUOpApplyKOptSeq[TUOpFromSymbolic[dag], seq];  (* C rewrite *)
  cBack = TUOpToSymbolic[cTerm];                          (* read back *)
  If[wlOut === cBack,
    <|"ok" -> True|>,
    <|"ok" -> False, "wl" -> wlOut, "c" -> cBack,
      "diff" -> SymbolicDiff[wlOut, cBack]|>]]
```

When this returns `ok -> True` for a representative test set
(matmul x{TC, GLOBAL, UPCAST, ...}, softmax x{SIMD_REDUCE, FAST_MATH,
GLOBAL, ...}, conv2d x{...}), the WL rule set is a verified spec for
the C implementation. When it returns `ok -> False`, either WL or C
has a bug -- the diff localizes the divergence.

Optional escalation: also compare rendered MSL strings. The WL side
needs a MSL renderer (a much bigger effort -- see Stage 4 below).

## Part 4 -- staged rollout

### Stage 1 -- the bridge (~1 day)

- `wl/THVMLink/Kernel/Rewrite.wl` -- new module
- `TUOpFromSymbolic` (8 head families x 1 line each = ~30 LOC)
- `TUOpToSymbolic` (recursive heap walk via existing `TUOp*` getters
  in `wl/THVMLink/CSource/thvmlink.c` -- already exposed for grad)
- `TUOpDagApplyKOpt` -- LibraryLink wrapper around
  `uop_dag_apply_kopt` (mirror of the Python `py_uop_dag_apply_kopt`
  binding in `py/csource/thvm_py.c`; <30 LOC)

### Stage 2 -- the rules (~1 day)

- `wl/THVMLink/Kernel/Rewrite.wl` -- `KOpt*` rule definitions
  (~80 LOC for all 11)
- `wl/THVMLink/Tests/Rewrite_test.wlt` -- per-rule unit tests
  (build a tiny DAG, apply KOpt, assert structural equality with
  expected output)

Each rule is a one-liner. The WL syntax IS the rule -- no walker, no
memo, no manual recursion. This is the readability win.

### Stage 3 -- cross-validation (~0.5 day)

- `CrossValidateKOpt` harness in Rewrite.wl
- `wl/THVMLink/Tests/Rewrite_xvalid.wlt` -- runs WL vs C on a corpus
  of (DAG, KOpt-sequence) pairs. Corpus drawn from the existing
  `tests/test_apply_opt_dag.c` test fixtures (matmul + softmax +
  reduce shapes).

When this passes for the full corpus, **the WL rules are an
executable spec for the C implementation**. Any future C change that
diverges from the WL spec fails the cross-validation test.

### Stage 4 (optional, larger) -- WL renderer for end-to-end check

Port `cg_render_uop_kernel_root` (~600 LOC of C) into WL. A direct
port is large but tractable, and the WL version becomes the
human-readable spec for what each opcode emits.

End-to-end test: WL renders DAG_after_kopt to MSL string; C renders
the same; assert byte-for-byte (or AST-for-AST modulo whitespace)
equal. Catches renderer drift the structural-DAG check can't see.

Defer until Stage 1-3 land and prove valuable.

## Part 5 -- what this enables

### Educational

The 11 KOpts are now expressible as 11 one-line WL rules vs 370 LOC
of C walker. New contributors read the rules and immediately
understand each KOpt's semantics. The C code is the implementation;
the WL is the spec.

### Exploratory

New rewrites can be drafted in WL, tested via WL's REPL, and only
ported to C once the rule shape is stable. Today, designing
`UOP_OPT_TG_REDUCE deep-tree` (the `TG_REDUCE` deep-tree feature in
[mlx_features_to_port.md](mlx_features_to_port.md)) requires writing
~150 LOC of C + tests. With the WL scaffold, the rule is drafted
in WL first (10 LOC), exercised on a softmax DAG, then ported.

### Spec-driven safety

The Phase E plan ([ideal_pipeline.md](ideal_pipeline.md)) has the
KOpt vocabulary as the load-bearing rewrite framework for the entire
autotune loop. Having an external spec (WL rules) that the C must
match prevents silent drift as the C implementation evolves.

### Comparison artifact for tinygrad and TileLang

Both have similar Opt vocabularies (see
[tilelang_scout.md](tilelang_scout.md)) but neither has a
declarative spec. Publishing the WL rule set is a useful
contribution back to the broader ecosystem -- it documents
"what kernel rewrites mean" more cleanly than reading any of the
three implementations.

## Part 6 -- non-goals

- **Not** a replacement for `apply_opt_dag.c`. Performance and
  in-process integration require the C path.
- **Not** a frontend WL surface for users. Users still write
  `TUOp*` graphs (existing surface in `wl/THVMLink/Kernel/Optim.wl`)
  or build via Python (`py/thvm/`). The WL rewrite layer is
  internal -- a spec + cross-validator + experimentation REPL.
- **Not** a runtime BEAM driver. The autotune search loop stays in
  C (`autotune.c`) for performance. WL helps design the rewrites
  the search composes, not the search itself.

## Open questions

1. **Symbol-mode vs Term-id mode**: should `TUOpFromSymbolic` actually
   build C-side Terms eagerly (so `TUOpDagApplyKOpt` can take an
   already-allocated Term), or build symbolic only and convert
   on-demand? Eager is simpler; symbolic-only allows pure-WL rule
   experimentation without thvm running. Probably support both via
   `TUOpFromSymbolic` for eager and a parallel `SymbolicOnly`
   constructor set for pure WL.
2. **Hash-cons in WL**: WL doesn't auto-intern; `URange[0, KAX$LOOP, 16]`
   appearing twice creates two equal-but-distinct expressions.
   Structural equality (`===`) handles comparison fine, but cost
   could matter for big DAGs. Optional: an in-WL canonicalizer that
   memoizes via `Module` + `AssociationThread`.
3. **PADTO**: not yet wired in C. Adding it to WL first (Stage 2)
   prototypes the rule shape; then port to C.

## Effort summary

| Stage | What | LOC | Days |
|---|---|---|---|
| 1 | Bridge (Rewrite.wl + LibraryLink wrapper) | ~120 | 1 |
| 2 | KOpt rules + per-rule unit tests | ~200 | 1 |
| 3 | Cross-validation harness + corpus tests | ~150 | 0.5 |
| 4 | WL MSL renderer (optional) | ~800 | 4 |

Total Stage 1-3: **~470 LOC, 2.5 days**. Lands a complete spec for
the C implementation with cross-validation against the existing
apply_opt_dag.c surface.
