# Recursive loop profile

Two patterns for expressing a training loop in thvm, with very
different scaling characteristics.

## TL;DR

Use the **ASSIGN pattern** for any loop you want to scale to
non-toy iteration counts.  Pre-materialize the step graph,
recurse with `TPriForce` to re-fire the same kernel each iter.
One kernel total, regardless of `n`.

The **bound-w lambda pattern** (sgd.wlt) is correct but emits a
fresh kernel per iter and hits a cubic wallclock cliff past
n~12.  Useful for understanding the IC machinery; not for real
training.

| pattern  | n=10  | n=100 | n=1000 | kernels   |
|----------|-------|-------|--------|-----------|
| bound-w  | 0.02s | (timeout, n>=20) | -      | n         |
| ASSIGN   | 0s    | 0s    | 0.006s | **1**     |

Tests: `wl/THVMLink/Tests/training_loop.wlt` (5 cases).

## Why kernels emit per-iter in the bound-w pattern

`TGrad` and the step graph reference `w` as a `TLam`-bound
variable (`TVAR`).  The IC machinery substitutes `w → w0`,
`w → step1`, `w → step2`, ... per iter via APP-LAM beta.  Each
iter's substituted body lives at *different heap locs*
(allocated fresh by `alo_realize`), so `thvm_materialize`'s
boundary classifier sees N independent compute graphs and emits
N kernels with bit-for-bit identical programs but distinct
input tids.

Could materialize compile the *body template once*, before
substitution, and dispatch with new inputs each iter?  In
principle yes -- but `TVAR` carries no shape.  Shape inference
would need to track shapes *through* `TApp` (looking at the
applied argument and propagating into the binder's body), or
require explicit shape annotations on `TLam`.  Open follow-up.

## Why the ASSIGN pattern emits one kernel

The step expression references `w` by **tid** (a concrete
`TTen`), not as a bound variable:

```wolfram
w = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
gradExpr = TGrad[TL2Loss[TUOpAdd[w, TUOpNeg[tgt]]], w];
step = TAssign[w, TUOpAdd[w, TUOpNeg[TUOpMul[lr, gradExpr]]]];
matStep = TMaterialize[TNf[step]];
```

`TAssign[w, ...]` rewrites `w`'s buffer in place and returns
`w`'s tid unchanged (verified by
`training-loop/assign-preserves-weight-tid`).  The recursive
loop just re-fires the same `matStep` kernel each iter:

```wolfram
TDef["loop", TLam[m,
    TIfZero[m, TUOpConst[0.0, "f32"],
        TPriForce[TRef["step"],
            TApp[TRef["loop"], TOp2["-", m, TNum[1]]]]]]];
TDef["step", matStep];
TWnf @ TApp[TRef["loop"], TNum[1000]];
```

The lambda body has no UOP-with-TVAR -- the bound `m` is just
the recursion counter, type-checked through `TIfZero` /
`TOp2`.  Materialize sees the step graph once (during the
`TMaterialize` call); the loop just sequences re-fires.

## Bound-w pattern: per-iter linear growth (good)

| n  | cells | kernels | ITRS | cells/iter |
|----|-------|---------|------|------------|
| 1  | 218   | 2       | 57   | 218        |
| 5  | 722   | 6       | 273  | 144        |
| 10 | 1352  | 11      | 723  | 135        |
| 13 | 1730  | 14      | 1089 | 133        |
| 14 | 1856  | 15      | 1227 | 132        |
| 15 | 1982  | 16      | 1373 | 132        |

Per-iter overhead: **~130 cells, +1 kernel, ~70 ITRS**, all
linear in `n`.

## Per-iter cell tag breakdown (n=5)

```
ALO=85 UOP=91 ERA=55 NUM=48 VAR=29 TEN=17 LAM=9 APP=7 MAT=4 REF=4 OP2=1
```

Top growers per iter:
- `ALO` (+40/iter): lazy wrappers from `alo_realize` for each new
  recursive REF unfold.
- `UOP` (+28/iter): the actual compute terms (the per-iter
  `Add/Neg/Mul/Reduce/...` graph).
- `ERA` (+25/iter): markers from `heap_subst_var` (APP-LAM beta)
  and `alo_force` memoization (the second cell flips to ERA
  after force).
- `NUM` (+17/iter): constants embedded in kernels and OP2 args.

## Kernel emission (after gy-resolve fix)

Each iter emits exactly **one** 13-op compute kernel of shape
`[CONST(lr), NEG, ADD, MUL, ADD, NEG, ADD, MUL, ADD, ADD, MUL, NEG, ADD]`
with 4 inputs (`w_iter`, `target`, plus two CONST cells folded in
during chain rule).  Plus **one** scalar-zero CONST kernel
(shared across all iters via the const cache).

Kernel programs are bit-for-bit identical across iters; only the
`input_tids` differ (each iter's `w` is a fresh tensor).  A program
hash-cons would save ~13 KProgOp slots per iter (~1 KB / 100 iters
of f32-3-vector loop) but wouldn't reduce kernel count -- each
iter still genuinely produces a distinct output tensor.

## Wallclock scaling cliff (bad)

| n  | time     | ratio |
|----|----------|-------|
| 10 | 0.016s   | -     |
| 11 | 0.030s   | 1.9x  |
| 12 | 0.074s   | 2.5x  |
| 13 | 0.188s   | 2.5x  |
| 14 | 0.539s   | 2.9x  |
| 15 | 1.625s   | 3.0x  |
| 16 | 4.715s   | 2.9x  |
| 18 | 46.9s    | -     |

Time grows ~3x per iter past n=10, far exceeding the 1.1x growth
in `ITRS`, `HEAP_NEXT`, `redex_enumerate` calls and
`heap_replace` cells-scanned (each ~1.10-1.15x per iter,
measured via inline counters).  So the cliff is **not** in:

- `heap_replace`'s O(HEAP_NEXT) scan
- `redex_enumerate` (already de-quadratic'd via the 32K-slot
  hash table in `redex_collect_one`)
- `is_redex` call count

It's hiding inside the interaction firing path itself --
candidates: `interact_kernel` re-firing, `alo_realize` walks per
chain-rule round, `wnf` recursion through deeply substituted
lambda bodies.  Counter instrumentation is still pending here.

## Optimisations applied so far

- **`gy` resolve at chain-rule entry** (`a9873a6`).  Was causing
  N identical "ones-at-shape" kernels (one per iter); now shared.
  Cell delta n=5: -15, kernel delta: -5.
- **Variable-identity match in `interact_grad`** (`eb281e9`).
  Without this, recursive iters can't propagate gradient because
  the bound `w` substitutes to a not-yet-materialized UOP graph.
- **Hash-cons binary/unary UOPs** (`a9873a6`).  Catches identical
  ADD/MUL/NEG/RECIP terms; small per-iter cell saving.
- **O(1) dedup in `redex_collect_one`** (`785c0b5`).  Drops
  N*R quadratic from `redex_enumerate` to N.

## ASSIGN pattern scaling

Same expression numerically (n iters of `w_{i+1} = 0.8 w_i + 0.2 tgt`,
target `{1, 2, 3}`):

| n    | cells  | kernels | ITRS    | time    |
|------|--------|---------|---------|---------|
| 1    | 103    | 1       | 29      | 0s      |
| 10   | 562    | 1       | 362     | 0s      |
| 100  | 5152   | 1       | 17K     | 0s      |
| 500  | 25K    | 1       | 385K    | 0.001s  |
| 1000 | 51K    | 1       | 1.52M   | 0.006s  |

Per-iter ~50 cells, **always 1 kernel**.  ITRS scales O(n^2)
because each REF-unfold + APP-LAM beta + heap_replace cascade
costs ~n; total time is still <10 ms at n=1000 because
heap_replace's per-fire cost is on a small heap (no compute
graph piling up -- ASSIGN reuses the same buffers).

## What's been landed

**Kernel program hash-cons** (`c83c29b`, src/schedule/kernel_program_cache.c).
The `KProgOp[]` array is shared across boundaries with bit-for-
bit identical programs (opcode + dtype + n_src + arg + numel +
src[] + shape/perm/pad bytes).  Each `KernelEntry` keeps its
own `input_tids[] / output_tid` -- per-instance I/O is
unchanged.  Concretely:

  bound-w n=5:  6 kernels,  **2 distinct programs**
  bound-w n=10: 11 kernels, **2 distinct programs**
  bound-w n=100: 101 kernels, 2 distinct programs (extrapolated)

Tracked by `TKernelProgramCacheSize[]`; asserted in
`training-loop/bound-w-kernel-program-hash-cons`.

This caps **program** memory at the number of distinct
structural shapes (typically 1-2 per loop), which is the
groundwork the next path needs.

## Path 3: shape inference for compile-once-dispatch-many (landed)

Three commits, ~400 lines total:

1. **`src/lam/shape.c`** -- side table mapping LAM heap loc to a
   bound-var shape.  Propagated through `clone_to_book_rec`
   (dyn -> book) and `alo_realize` (book -> dyn) so each
   instantiation of a recursive REF inherits the annotation.
   `term_shape_in(TVAR(loc))` consults this table when the
   binding cell isn't SUB-marked yet (= pre APP-LAM beta).

2. **`materialize.c` visit() TVAR branch** -- when a `TAG_VAR`
   has a shape annotation, allocate a symbolic input slot
   (`input_tids[i] = 0`, `input_terms[i] = VAR-Term`).  The
   kernel program references it via `KSRC_AS_INPUT(slot)`.
   At fire time `interact_kernel` resolves the VAR through
   SUB to whatever APP-LAM beta has bound it to and reads
   that tensor's buffer.

3. **`interact_app_lam` JIT path** -- when a TLam is applied,
   if the body is a UOP graph and the argument carries a shape
   (TEN or shape-inferable UOP), APP-LAM infers the bound
   variable's shape from the argument, registers it on
   `lam_shape`, materializes the body into a UOP_KERNEL, and
   then proceeds with the standard `heap_subst_var` beta.
   No flag, no separate constructor: every `TLam` whose body
   is compute goes through this path.  Bodies that aren't
   compute (curried lambdas, `TIfZero`, `TApp`-headed) skip
   the JIT step -- materialize would be a no-op anyway.

(We considered using the existing `TAG_ANN` / `TAG_BRI`
machinery for shape propagation -- ICC's type-directed
reduction would naturally push annotations to var sites -- but
the existing rules do full type-checking with BRI bridges,
which is heavier than what shape inference needs.  The side
table is opt-in, has zero overhead for unannotated LAMs, and
keeps the ICC reduction rules untouched.)

End-to-end test (`lam-shape/tlam-jit-on-first-apply`):

```wolfram
lam = TLam[w, TUOpAdd[w, w]]                    (* body unmaterialized *)
ten = TTensorCreate[NumericArray[{1, 2, 3}, "Real32"]]
TKernelCount[] - 1                              -> 0 (no kernel yet)
TWnf[TApp[lam, ten]]
   -> {2., 4., 6.}                              (* shape {3} inferred from ten *)
TKernelCount[] - 1                              -> 1   (JIT emitted one kernel)
TKernelProgramCacheSize[]                       -> 1
```

`TLamShape[shape, x, body]` is still available for the rare case
where the body needs to materialize *before* any TApp (e.g.
direct `TMaterialize` on the body for inspection, or when the
argument's shape can't be inferred at first APP).

### What's still missing for fully-automatic loops

`TLamMaterialized` makes the user opt in.  For the recursive
REF/ALO sgd_loop pattern in `sgd.wlt` to benefit
automatically, the realize loop would need to walk into
REF bodies, detect shape-annotated lambdas, materialize their
bodies once, and cache by `(book_loc, arg_shape_signature)`.
Each iter's `App(REF, arg_K)` then hits the cache and just
dispatches with `arg_K`.

Approximate work:
  - `realize_classify` recognizes `App(REF, arg)` with
    shape-annotated REF body.
  - `materialize_lam_body` (new) compiles the body once,
    keyed on (book_loc, arg_shape).
  - `interact_kernel` extended for the TVAR-resolve at fire
    time (already done in 9e66ab3 -- input_tids[i]=0 +
    input_terms[i]!=0 path).

Not landed yet; ~200 lines.

## Other open follow-ups

1. **Heap compaction**.  Per-iter cell growth is linear so
   n=1000 gives ~130K cells (1MB heap).  Tolerable but
   unbounded; a mark-and-sweep that drops cells unreachable from
   the result tensor would let arbitrarily long training loops
   run.
2. **Replace `heap_replace`'s O(HEAP_NEXT) cascade with HVM4-
   style SUB-bit substitution**.  Removes one per-fire linear
   cost.
3. **Profile interactions inside `redex_fire`'s case dispatch**
   to localise the cubic cliff in the bound-w pattern (already
   ruled out: heap_replace, redex_enumerate, is_redex).
