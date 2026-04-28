# Recursive lambda loop profile

Snapshot of `sgd_loop` running n iterations of L2-loss SGD from
`wl/THVMLink/Tests/sgd.wlt` against `target = {1, 2, 3}`,
`w0 = {0, 0, 0}`, `lr = 0.1`.  All numbers from a fresh `TInit[]`
per iter so heap state isn't shared across `n` values.

## Per-iter linear growth (good)

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

## Open follow-ups

1. Profile interactions inside `redex_fire`'s case dispatch to
   localise the 3x-per-iter cliff.
2. Heap compaction.  Per-iter cell growth is linear so n=1000
   gives ~130K cells (1MB heap).  Tolerable but unbounded; a mark-
   and-sweep that drops cells unreachable from the result tensor
   would let arbitrarily long training loops run.
3. Replace `heap_replace`'s O(HEAP_NEXT) cascade with HVM4-style
   SUB-bit substitution.  Removes one per-fire linear cost.
4. Kernel program hash-cons: minor memory win, useful when
   programs are large (deep MLPs).
