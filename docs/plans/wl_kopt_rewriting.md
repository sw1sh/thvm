# Plan: WL scaffold for KOpt rewriting -- native Rule design + .wlt cross-validation

Status: design proposal, no code written yet.

## What this is

A WL (Wolfram Language) layer that re-expresses thvm's KOpt vocabulary
as **symbolic rewrite rules** over the **existing `TTermExpr` form**,
then cross-validates against the C implementation through `.wlt`
tests using the existing `VerificationTest` harness. Not a foreign-
function wrapper -- a parallel rule engine using WL's native
`Rule` / `RuleDelayed` / `ReplaceAll`, which is exactly the
abstraction `apply_opt_dag.c` reimplements in 370 LOC of manual
walker.

The win: WL is **rewrite-native**. The 11 KOpts (TC, GLOBAL, UPCAST,
UNROLL, LOCAL, GROUP, GROUPTOP, SWAP, FAST_MATH, SIMD_REDUCE,
VEC_LOAD) are each one-line `RuleDelayed` patterns over the same
`TTermExpr` heads the heap walker already emits. The walker is
`ReplaceAll` (`/.`). The composition is function composition. The
cross-check is structural equality (`===`) between WL-output and
C-output, both produced by the same `TTermExpr` snapshot.

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
opt)`. Six categories (full breakdown in
[mlx_features_to_port.md](mlx_features_to_port.md)).

All 11 active KOpts use a **manual Term-keyed memo walker** because
`uop_graph_rewrite` recurses into rule outputs (would re-fire on its
own outputs and infinite-loop).

### The propose -> apply -> render -> bench loop

```
propose.c::kernel_opts_propose(ke, out_kopts, cap) -> n
apply_opt.c::kernel_apply_opt(ke, opt) -> 1 on success
render_uop.c::cg_render_uop_kernel_root(root, name, fp)
autotune.c BEAM: pick winner by wall-time, cache (shape, opt-seq)
```

## Part 2 -- the existing TTermExpr surface (reuse, don't reinvent)

[wl/THVMLink/Kernel/THVMLink.wl:710](../../wl/THVMLink/Kernel/THVMLink.wl)
defines:

```wolfram
TTermExpr[t_TTerm]   (* term -> nested string-headed expression *)
TTermTree[t_TTerm]   (* same, wrapped in ExpressionTree for visual *)
```

`TTermExpr` walks the heap from a term and emits a structural snapshot
with **string heads** (no Term IDs in the output). Heads come from
`$tagNames` for the outer tag (e.g. `"UOP"`, `"NUM"`, `"LAM"`) and
from `$uopNames` (THVMLink.wl:258-268) for the UOp opcode label.

For a UOp node the shape is:
```wolfram
"UOP"[opname_String, child1, child2, ...]
```
where children come from `uopCellCount[opcode]` heap slots. Atoms are
`"NUM"[v]`, `"VAR"[loc]`, `"ERA"`, etc.

### What's covered today (`$uopNames`)

Opcodes 0-24: `MATERIALIZE`, `KERNEL`, `CONST`, `RESHAPE`, `PERMUTE`,
`EXPAND`, `PAD`, `SHRINK`, `FLIP`, `ADD`, `MUL`, `NEG`, `RECIP`,
`EXP2`, `LOG2`, `SQRT`, `CMPLT`, `REDUCE`, `GRAD`, `FWD`, `CMPEQ`,
`LOAD`, `ASSIGN`, `CAST`, `BITCAST`.

### What needs adding -- $uopNames + uopCellCount extension

Opcodes 25-39 (post-Phase-E additions to thvm.h) aren't in
`$uopNames` yet -- they currently fall back to `"UOP?"<>ToString[ext]`
which makes patterns brittle. **Stage 0 of the rollout**: extend the
two tables in `THVMLink.wl` and `Uop.wl`:

```wolfram
(* THVMLink.wl:258 -- $uopNames *)
$uopNames = <|
    ...existing 0..24...,
    25 -> "RANGE",      26 -> "INDEX_E",
    27 -> "IADD",       28 -> "ISUB",       29 -> "IMUL",
    30 -> "IDIV",       31 -> "IMOD",       32 -> "ILT",
    33 -> "IAND",       34 -> "IWHERE",     35 -> "INVALID",
    36 -> "BUFFER",     37 -> "STORE",      38 -> "AFTER",
    39 -> "OPT"
|>;

(* Uop.wl -- uopCellCount table (heap cells, not just compute arity) *)
uopCellCount[$UopRange]    = 3;   (* axis_id, axis_type, extent *)
uopCellCount[$UopIndexE]   = 2;   (* buffer, addr *)
uopCellCount[$UopIAdd | $UopISub | $UopIMul | $UopIDiv |
             $UopIMod | $UopILt | $UopIAnd]                = 2;
uopCellCount[$UopIWhere]   = 3;   (* cond, then, else *)
uopCellCount[$UopInvalid]  = 0;
uopCellCount[$UopBuffer]   = ...; (* variable: ndim+3 cells -- handle in tTreeWalkWith special-case *)
uopCellCount[$UopStore]    = 3;   (* buf, addr, value *)
uopCellCount[$UopAfter]    = 2;   (* node, after_node *)
uopCellCount[$UopOpt]      = 3;   (* target, kind, factor *)
```

After the extension, `TTermExpr` of a matmul-with-TC root looks like:

```wolfram
"UOP"["STORE",
  "UOP"["BUFFER", "NUM"[0], "NUM"[13], "NUM"[2], "NUM"[16], "NUM"[16]],
  "UOP"["IADD",
    "UOP"["IMUL",
      "UOP"["RANGE", "NUM"[0], "NUM"[0], "NUM"[16]],
      "UOP"["CONST", "NUM"[16]]],
    "UOP"["RANGE", "NUM"[1], "NUM"[0], "NUM"[16]]],
  "UOP"["OPT",
    "UOP"["REDUCE",
      "UOP"["MUL", <a_load>, <b_load>],
      "NUM"[0], "NUM"[2]],
    "NUM"[2], "NUM"[8]]]
```

That's the full DAG visible to WL pattern matching, structurally
comparable via `===`, no parallel symbol vocabulary needed.

## Part 3 -- KOpt rules over TTermExpr

All rules operate on the existing `"UOP"[...]` form. Constants for
opcode/axis/dtype kinds get short global names mirroring the C
defines:

```wolfram
KAX$LOOP = 0;  KAX$REDUCE = 1;  KAX$UPCAST = 2;  KAX$UNROLL = 3;
KAX$LOCAL = 4; KAX$GLOBAL = 5;  KAX$GROUP_REDUCE = 6;

OPT$UNROLL = 0; OPT$UPCAST = 1; OPT$TC = 2; OPT$LOCAL = 3;
OPT$GROUP_REDUCE = 4; OPT$CONV = 5;
OPT$FAST_MATH = 6; OPT$SIMD_REDUCE = 7; OPT$VEC_LOAD = 8;

REDUCE$SUM = 0; REDUCE$MAX = 1;
DT$INT32 = 5; DT$FP32 = 13;
```

(All wired from the C-side via `LibraryFunctionLoad` on existing
`thvm_wl_*` getters; mirror the constants resolution in
`py/thvm/thvm.py`.)

### Wrap-target rules (TC, FAST_MATH, SIMD_REDUCE, VEC_LOAD)

```wolfram
(* TC: wrap STORE.value's REDUCE with OPT(_, TC, factor).
   Replaces existing TC factor if already wrapped (idempotent). *)
KOptTC[factor_Integer] := expr |->
  Replace[expr, {
    (* bare REDUCE -- wrap *)
    "UOP"["STORE", b_, a_, r:"UOP"["REDUCE", __]] :>
      "UOP"["STORE", b, a,
        "UOP"["OPT", r, "NUM"[OPT$TC], "NUM"[factor]]],
    (* already wrapped -- replace factor *)
    "UOP"["STORE", b_, a_,
      "UOP"["OPT", r:"UOP"["REDUCE", __], "NUM"[OPT$TC], _]] :>
      "UOP"["STORE", b, a,
        "UOP"["OPT", r, "NUM"[OPT$TC], "NUM"[factor]]]
  }, {0}]

(* FAST_MATH: wrap every unary op with OPT(_, FAST_MATH, 0).
   Stop-leaf for already-wrapped to avoid OPT-of-OPT.  The //. operator
   does fixed-point rewrite which together with the stop-leaf is
   idempotent. *)
KOptFastMath := expr |->
  expr //. {
    "UOP"["OPT", inner_, "NUM"[OPT$FAST_MATH], _] :>
      "UOP"["OPT", inner, "NUM"[OPT$FAST_MATH], "NUM"[0]],
    u:"UOP"[name_String /; MemberQ[
        {"EXP2", "LOG2", "SQRT", "NEG", "RECIP"}, name], __] :>
      "UOP"["OPT", u, "NUM"[OPT$FAST_MATH], "NUM"[0]]}

(* SIMD_REDUCE: wrap every REDUCE with OPT(_, SIMD_REDUCE, 0). *)
KOptSimdReduce := expr |->
  expr //. {
    "UOP"["OPT", r:"UOP"["REDUCE", __], "NUM"[OPT$SIMD_REDUCE], _] :>
      "UOP"["OPT", r, "NUM"[OPT$SIMD_REDUCE], "NUM"[0]],
    r:"UOP"["REDUCE", __] :>
      "UOP"["OPT", r, "NUM"[OPT$SIMD_REDUCE], "NUM"[0]]}

(* VEC_LOAD: wrap every INDEX_E with contiguous-shaped addr. *)
KOptVecLoad[width_Integer] := expr |->
  expr //. {
    "UOP"["OPT", e:"UOP"["INDEX_E", __], "NUM"[OPT$VEC_LOAD], _] :>
      "UOP"["OPT", e, "NUM"[OPT$VEC_LOAD], "NUM"[width]],
    e:"UOP"["INDEX_E", _,
      "UOP"["IADD", "UOP"["IMUL", _, _], "UOP"["RANGE", __]]] :>
      "UOP"["OPT", e, "NUM"[OPT$VEC_LOAD], "NUM"[width]]}
```

### Axis-type swap (GLOBAL)

```wolfram
KOptGlobal[axis_Integer] := expr |->
  expr /. "UOP"["RANGE", "NUM"[axis], "NUM"[KAX$LOOP], extN_] :>
          "UOP"["RANGE", "NUM"[axis], "NUM"[KAX$GLOBAL], extN]
```

### Axis split (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP)

```wolfram
KOptSplit[innerKax_, axis_Integer, k_Integer] := expr |->
  expr /. "UOP"["RANGE", "NUM"[axis], "NUM"[KAX$LOOP], "NUM"[n_]] :>
    "UOP"["IADD",
      "UOP"["IMUL",
        "UOP"["RANGE", "NUM"[axis],     "NUM"[KAX$LOOP],
              "NUM"[Quotient[n, k]]],
        "UOP"["CONST", "NUM"[k]]],
      "UOP"["RANGE",  "NUM"[axis + 1], "NUM"[innerKax],
            "NUM"[k]]]

KOptUpcast  [a_, k_] := KOptSplit[KAX$UPCAST,        a, k]
KOptUnroll  [a_, k_] := KOptSplit[KAX$UNROLL,        a, k]
KOptLocal   [a_, k_] := KOptSplit[KAX$LOCAL,         a, k]
KOptGroup   [a_, k_] := KOptSplit[KAX$GROUP_REDUCE,  a, k]
KOptGrouptop[a_, k_] := KOptSplit[KAX$GROUP_REDUCE,  a, k]
```

### Axis swap (SWAP)

Three-step via a fresh placeholder symbol -- `\[FormalA]` --
sidesteps the bidirectional rewrite loop the C `apply_opt_dag` SWAP
walker handles via single-pass walking:

```wolfram
KOptSwap[a_Integer, b_Integer] := expr |->
  expr /. {"UOP"["RANGE", "NUM"[a], t_, e_] :>
             "UOP"["RANGE", "NUM"[\[FormalA]], t, e],
           "UOP"["RANGE", "NUM"[b], t_, e_] :>
             "UOP"["RANGE", "NUM"[a], t, e],
           "UOP"["RANGE", "NUM"[\[FormalA]], t_, e_] :>
             "UOP"["RANGE", "NUM"[b], t, e]}
```

### Composition

```wolfram
ApplyKOptSeq[seq_List] := RightComposition @@ seq
(* ApplyKOptSeq[{KOptTC[8], KOptGlobal[0], KOptGlobal[1]}][expr] *)
```

## Part 4 -- the C bridge

Two FFI surfaces already exist:
- [wl/THVMLink/](../../wl/THVMLink/) -- LibraryLink paclet wrapping
  thvm. `TTermExpr[t]` works today.
- [py/csource/thvm_py.c](../../py/csource/thvm_py.c) -- ctypes
  wrapper exposing `uop_dag_apply_kopt`. Same C call we want from WL.

What this proposal adds: **one new LibraryLink entry point** (mirror
of `py_uop_dag_apply_kopt`):

```c
/* wl/THVMLink/CSource/thvmlink.c -- new entrypoint */
DLLEXPORT int thvm_wl_uop_dag_apply_kopt(WolframLibraryData libData,
                                         mint argc, MArgument *args,
                                         MArgument result) {
    Term root = MArgument_getInteger(args[0]);
    u8   op   = (u8)MArgument_getInteger(args[1]);
    u8   axis = (u8)MArgument_getInteger(args[2]);
    u32  arg  = (u32)MArgument_getInteger(args[3]);
    KOpt opt = { op, axis, arg };
    Term out = uop_dag_apply_kopt(root, opt);
    MArgument_setInteger(result, (mint)out);
    return LIBRARY_NO_ERROR;
}
```

WL-side wrapper (in a new `wl/THVMLink/Kernel/Rewrite.wl`):

```wolfram
$applyKOptFn := $applyKOptFn = load["thvm_wl_uop_dag_apply_kopt",
                                    {Integer, Integer, Integer, Integer},
                                    Integer]

(* C-side apply: returns a fresh TTerm wrapping the new root *)
TUOpDagApplyKOpt[t_TTerm, op_Integer, axis_Integer, arg_Integer] :=
  TTerm[$applyKOptFn[ttermRaw[t], op, axis, arg]]
```

That's the entire bridge -- ~30 LOC including tests. The WL-side
rules don't touch C; the C-side function is invoked separately for
cross-validation.

## Part 5 -- .wlt cross-validation tests (the deliverable)

`wl/THVMLink/Tests/rewrite.wlt` -- the heart of the spec.

For each KOpt + each test fixture, run `VerificationTest` asserting
**WL rule output `===` C apply output**, both rendered through
`TTermExpr`.

```wolfram
(* === KOpt cross-validation infrastructure ============================ *)

(* Build a canonical 16x16 matmul DAG via existing TUOp* constructors;
   returns TTerm[root]. *)
buildMatmul16[] := Module[{a, b, c, m, n, k, kc, addrA, addrB, addrC,
                           load_a, load_b, mul, red, store},
    a = TUOpBufferInst[$ScopeGlobal, $DtFp32, {16, 16}, 1];
    b = TUOpBufferInst[$ScopeGlobal, $DtFp32, {16, 16}, 2];
    c = TUOpBufferInst[$ScopeGlobal, $DtFp32, {16, 16}, 0];
    m = TUOpRange[0, $KaxLoop,   16];
    n = TUOpRange[1, $KaxLoop,   16];
    k = TUOpRange[2, $KaxReduce, 16];
    kc = TUOpConst[$DtInt32, 16];
    addrA = TUOpIAdd[TUOpIMul[m, kc], k];
    addrB = TUOpIAdd[TUOpIMul[k, kc], n];
    addrC = TUOpIAdd[TUOpIMul[m, kc], n];
    load_a = TUOpIndexE[a, addrA];
    load_b = TUOpIndexE[b, addrB];
    mul = TUOpMul[load_a, load_b];
    red = TUOpReduce[$ReduceSum, 2, mul];
    TUOpStore[c, addrC, red]
]

(* Verify: applying KOpt via WL rules produces the same TTermExpr
   as applying it via C. *)
xvalidKOpt[root_TTerm, kopName_String, op_Integer,
           axis_Integer, arg_Integer, wlRule_] := With[{
    cAfter = TTermExpr[TUOpDagApplyKOpt[root, op, axis, arg]],
    wlAfter = wlRule[TTermExpr[root]]
}, wlAfter === cAfter]

(* === per-KOpt VerificationTests ===================================== *)

VerificationTest[
    xvalidKOpt[buildMatmul16[], "TC", $KopTC, 0, 8, KOptTC[8]],
    True,
    TestID -> "xvalid-tc-matmul-factor8"]

VerificationTest[
    xvalidKOpt[buildMatmul16[], "TC", $KopTC, 0, 16, KOptTC[16]],
    True,
    TestID -> "xvalid-tc-matmul-factor16"]

VerificationTest[
    xvalidKOpt[buildMatmul16[], "GLOBAL", $KopGlobal, 0, 0,
               KOptGlobal[0]],
    True,
    TestID -> "xvalid-global-axis-m"]

VerificationTest[
    xvalidKOpt[buildMatmul16[], "GLOBAL", $KopGlobal, 1, 0,
               KOptGlobal[1]],
    True,
    TestID -> "xvalid-global-axis-n"]

VerificationTest[
    xvalidKOpt[buildMatmul16[], "FAST_MATH", $KopFastMath, 0, 0,
               KOptFastMath],
    True,
    TestID -> "xvalid-fast-math-no-unary"]   (* matmul has no exp/log *)

VerificationTest[
    xvalidKOpt[buildSoftmax[16], "FAST_MATH", $KopFastMath, 0, 0,
               KOptFastMath],
    True,
    TestID -> "xvalid-fast-math-softmax"]    (* exp/log/recip get wrapped *)

VerificationTest[
    xvalidKOpt[buildSoftmax[16], "SIMD_REDUCE", $KopSimdReduce, 0, 0,
               KOptSimdReduce],
    True,
    TestID -> "xvalid-simd-reduce-softmax"]

(* ... continue for each (KOpt, fixture) pair *)

(* === composition tests =============================================== *)

VerificationTest[
    With[{root = buildMatmul16[]},
      TTermExpr[ApplyKOptC[root, {{$KopTC, 0, 8},
                                  {$KopGlobal, 0, 0},
                                  {$KopGlobal, 1, 0}}]]
        === ApplyKOptSeq[{KOptTC[8], KOptGlobal[0], KOptGlobal[1]}][
                TTermExpr[root]]],
    True,
    TestID -> "xvalid-compose-tc-global-global"]
```

When all `xvalid-*` tests pass for the corpus (matmul, softmax,
layernorm, vector_sum, conv2d shapes x each KOpt), **the WL rule set
is a verified executable spec for the C implementation**. Any future
C drift (e.g. accidentally double-wrapping in apply_opt_dag) fails
the corresponding xvalid test.

## Part 6 -- staged rollout

| Stage | What | LOC | Days |
|---|---|---|---|
| 0 | Extend `$uopNames` (THVMLink.wl) + `uopCellCount` (Uop.wl) for opcodes 25-39 | ~30 | 0.25 |
| 1 | Bridge: `thvm_wl_uop_dag_apply_kopt` LibraryLink + `TUOpDagApplyKOpt` WL wrapper | ~50 | 0.5 |
| 2 | `wl/THVMLink/Kernel/Rewrite.wl` -- 11 KOpt rules + helpers | ~150 | 1 |
| 3 | `wl/THVMLink/Tests/rewrite.wlt` -- xvalid corpus for all 11 KOpts x 5 fixture shapes (~55 tests) + composition tests | ~250 | 1 |
| 4 (optional) | WL MSL renderer for end-to-end check | ~800 | 4 |

**Stage 0-3 total: ~480 LOC, 2.75 days**, lands a verified spec
testable via `bash wl/THVMLink/Tests/run.sh rewrite.wlt`.

## Part 7 -- what this enables

### Educational

The 11 KOpts are now expressible as 11 one-line WL rules over the
**existing** `TTermExpr` form. New contributors read the rules and
immediately understand each KOpt's semantics. The C code is the
implementation; the WL is the spec.

### Exploratory

New rewrites can be drafted in WL, tested via WL's REPL, and only
ported to C once the rule shape is stable. `UOP_OPT_TG_REDUCE
deep-tree` (the next architectural feature in
[mlx_features_to_port.md](mlx_features_to_port.md)) requires ~150
LOC of C + tests. With the WL scaffold, the rule is drafted in WL
first (~10 LOC), exercised on a softmax DAG via REPL, then ported.

### Spec-driven safety

Phase E ([ideal_pipeline.md](ideal_pipeline.md)) puts the KOpt
vocabulary at the center of the autotune loop. An external WL spec
that the C must match prevents silent drift as the C implementation
evolves. The `.wlt` xvalid suite runs in CI alongside `make test`.

### Comparison artifact for tinygrad and TileLang

Both have similar Opt vocabularies (see
[tilelang_scout.md](tilelang_scout.md)). Neither has a
declarative spec. Publishing the WL rule set is a useful
contribution back to the broader ecosystem -- it documents
"what kernel rewrites mean" more cleanly than reading any
implementation.

## Part 8 -- non-goals

- **Not** a replacement for `apply_opt_dag.c`. Performance and
  in-process integration require the C path.
- **Not** a frontend WL surface for users. Users still write
  `TUOp*` graphs (existing Optim.wl) or build via Python (py/thvm/).
  The WL rewrite layer is internal -- a spec + cross-validator +
  experimentation REPL.
- **Not** a runtime BEAM driver. The autotune search loop stays in
  C (`autotune.c`) for performance. WL helps design the rewrites
  the search composes, not the search itself.
- **Not** a parallel symbol vocabulary. We reuse `TTermExpr`'s
  `"UOP"[...]` heads -- no `URange` / `UOpt` / `UStore` invented.

## Open questions

1. **`uopCellCount[$UopBuffer]` is variable-arity** (3 + ndim
   cells). The walker needs a special case in `tTreeWalkWith` to
   read `ndim` from the heap and emit `Table[child, {i, 0, ndim+2}]`.
   Or report a fixed minimum (3) and surface dims as `"NUM"[...]`
   trailing args via a one-off tweak.
2. **OPT-of-OPT detection in WL rules**: with `//.` (fixed-point
   replace), the stop-leaf rule must come BEFORE the wrap rule in
   the rule list, since WL tries rules in order. The `KOptFastMath`
   sketch above already does this; verify per-KOpt.
3. **PADTO**: not yet wired in C. Adding the WL rule first as the
   spec lets us prototype the rewrite shape before C implementation.

## References

- [TTermExpr definition](../../wl/THVMLink/Kernel/THVMLink.wl) (line
  710 -- 1-line definition over a 100-LOC heap walker)
- [Uop.wl per-opcode tables](../../wl/THVMLink/Kernel/Uop.wl) --
  arity, name, shape inference
- [apply_opt_dag.c](../../src/uop/apply_opt_dag.c) -- the C
  implementation that the WL rules spec
- [mlx_features_to_port.md](mlx_features_to_port.md) -- feature
  pipeline that drove the new KOpt additions
- [Existing .wlt examples](../../wl/THVMLink/Tests/) -- core.wlt,
  beautiful_mnist.wlt etc. for `VerificationTest` style
