# Phase 10 — BEAM via SUP-collapse

The Phase 10 prompt called for a kernel-variant search built around
SUP / DUP collapse: a kernel with K codegen variants becomes a
`SUP[label, v0, v1, ..., v(K-1)]` term, the IC engine fires each
candidate on a calibration input, records µs into K_PROFILE, and
DUP-collapses to the lowest-microseconds branch.

This phase ships the WL-side scaffold of that idea
(`TBeamPick`); the C-side variant codegen (`cg_emit_variants`) and
the in-IR SUP-of-kernels rewrite are deferred to Phase 10b/c.

## What lands

`wl/THVMLink/Kernel/Beam.wl`:

- `TBeamPick[{fn1, fn2, ...}]` returns a `TBeamClosure`.  Each `fnN`
  is a no-arg `Function[{}, body]` that produces the comparison
  result via in-place `TSet` (so candidates can write to the same
  output buffer).
- First call: each candidate is `TJit`-wrapped and dispatched once.
  `AbsoluteTiming` records µs; `PositionSmallest` picks the winner.
  Side-store keyed by `Hash[closureAssoc]` parks `(winner, timings,
  jits)` for later replays.
- Subsequent calls: dispatch only `jits[[winner]]` -- TJit replay
  semantics, no scheduler, no recapture.

- `TBeamReport[closure]` returns the per-candidate timings as
  `<| 1 -> us1, 2 -> us2, ... |>`.
- `TBeamWinner[closure]` returns the winning index (1-indexed; 0
  before calibration).
- `TBeamReset[closure]` releases the winner's TJit slot + clears
  the side-store entry; next call re-runs calibration.

## Bench

Two-candidate sum-of-squares on a 1024-elem f32 input
(both candidates compute the same scalar):

```
mkA = Function[{}, x = TTensorCreate[xData]; TSet[out, Total[x * x]]]
mkB = Function[{}, x = TTensorCreate[xData]; TSet[out, Total[x^2]]]
```

Both mkA and mkB collapse to the same JIT'd kernel via the
existing kernel-program hash-cons cache (Power[..., 2] folds to
x*x via the existing UpValue), so calibration just measures
TJit's first-call capture cost.  Replay 200x: BEAM 1.6 ms vs eager
mkA 32 ms = ~20x speedup (entirely from TJit replay).

Differentiated-shape variant (4096-elem reduction):

```
mkA = Function[{}, x = TTensorCreate[xData]; TSet[out, Total[x]]]
mkB = Function[{}, x = TTensorCreate[xData];
                   TSet[out, Total @ TUOpReduce[TUOpReshape[x, {n/2, 2}], 1, "SUM"]]]
```

mkA: single-stage reduce.  mkB: two-stage tree reduce.

```
calibration: mkA = 414 ms, mkB = 743 ms          -> winner = mkA
replay 500x: beam=17.25 ms / mkA eager=76.66 ms / mkB eager=104.54 ms
```

BEAM correctly picks the single-stage form.  mkB is genuinely
slower at this size.  Replay vs eager-of-winner is 4.4x (TJit-
replay benefit), and BEAM's choice avoids the eager-of-loser ratio
of 6.1x.

## Why WL-only

The IC-native version embeds the candidates as
`SUP[label, kid_v0, kid_v1, ...]` in the heap and rewrites them
via `interact_sup_collapse` once timed, so the choice persists in
the kernel graph without a WL side-store.  Two prereqs that aren't
in this phase:

- `cg_emit_variants(ke, spec) -> char *[]` -- produces one C/MSL
  source per spec (tile, vector width, unroll).  Today the
  Renderer (`src/codegen/cg.c`) emits one.  Extending to a vector-
  of-sources needs both a `KernelVariantSpec` enumeration AND
  hand-tuned alternatives that beat clang -O2 for thvm's kernel
  shapes.  Without measurement showing an existing variant beats
  the default, the search infrastructure has nothing to optimise
  over.
- `SUP_KERNEL` term + `interact_sup_kernel` rewrite that fires
  each branch under the calibration input + DUP-collapses post-
  measurement.  The IC reduction semantics need a small tweak
  (current SUP duplicates ALL branches; we want to pick ONE), so
  the existing dup_sup interaction can't be reused as-is.

Both prereqs fit better as Phase 12 follow-ups once Phase 11 has
exercised the WL surface enough to identify the kernels worth
variant-searching.

## Where BEAM actually wins on M-series CPU

Apple Accelerate already does tile + vector + unroll search for
matmul / matvec / dot.  thvm's BLAS-dispatched paths inherit that;
no BEAM upside.

The BEAM-relevant kernels are JIT-codegen'd: fused softmax /
layernorm / batchnorm / sparse-CE / per-sample conv-grad chains.
For these, `clang -O2` does some auto-vectorisation but a hand-
tuned NEON inner loop with explicit unroll can be 2-4x faster on
specific shapes (this is the documented BEAM win in tinygrad too;
its CLANG renderer's hand-tuned variant beats the default for
some softmax/layernorm sizes).

`TBeamPick` is the WL-side scaffold ready for those experiments.
Once `cg_emit_variants` lands, the candidates become
auto-generated rather than hand-written.

## Phase 11 expectation

Phase 11 (GPT-2 building blocks: TEmbedding, multi-head attention,
KV cache, GELU, causal mask) is the next concrete deliverable.
Reaching parity on the gpt2 inference harness is a pure
composition exercise on top of the layers Phase 5+6 added; no new
runtime infrastructure should be required, and the BEAM scaffold
above is available for tuning the per-block softmax / attention
kernels once they're in place.
