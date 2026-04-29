# Phase 14 — single-realize ASSIGN chain + faster TGradMany + 100-token gen

Three plumbing fixes that close real correctness gaps in the WL +
materialize pipeline; one perf win on a hot WL-side helper that
was masking the actual chain-rule cost.  Lenet 4-step convergence
is still gated on the per-grad C-side compute (separate Phase 15
work, see "What's still slow").

## What lands

### 1. WNF / ASSIGN drain via `materialize_inner_assigns`

`TRealize @ TAssign[w, ...nested ASSIGNs...]` previously fired only
the FIRST encountered ASSIGN per realize.  Sibling ASSIGNs in a
compound src stayed at TAG_UOP and never reduced to a TEN -- TAdam
had to issue THREE `TRealize`s per param to make m / v / w fire.

Root cause: in `src/schedule/materialize.c`, the OUTERMOST
`thvm_materialize` call special-cased UOP_ASSIGN and recursively
materialised its src, but a NESTED ASSIGN deeper in the UOP DAG
never went through that handler -- its src stayed as a raw UOP, so
wnf later couldn't reduce it to a TEN.

Fix: a small recursive descent `materialize_inner_assigns(term)`
walks the UOP DAG before the kernel-emit pass, finds every nested
UOP_ASSIGN, and recursively materialises its src in place via
`heap_set` on the `cell+1` slot.  After the walk, every ASSIGN's
src is either a TAG_TEN or a UOP_KERNEL chain that wnf fires
cleanly.

Verified: Adam math executes correctly in a single TRealize.  All
three buffer writes (m, v, w) happen with the textbook output.

### 2. TAdam collapses 3 TRealizes -> 1

`wl/THVMLink/Kernel/Optim.wl`'s TAdam body now does:

```wolfram
Block[{
    mAfter = TAssign[mTen, beta1*mTen + (1-beta1)*gTen],
    vAfter = TAssign[vTen, beta2*vTen + (1-beta2)*gTen*gTen]
},
    denom = Sqrt[vAfter] * invSqrtB2cor + eps;
    TRealize @ TAssign[wTen, wTen - lrHat*mAfter/denom]]
```

Same numeric output, one realize per param.

### 3. `uop_leaf_tids` C-side walk replaces recursive WL helper

`gradLeafTids[y]` was a recursive WL function that walked the UOP
DAG looking for TAG_TEN-leaf tids (used by `TGrad`/`TGradMany` to
build the chain rule's per-target DUP nest).  Without memoization
it re-visited shared sub-UOPs exponentially.  On LeNet's 8-weight
forward the walk took 1.6 s in WL; one full `TGradMany` over 8
weights took 13.3 s -- entirely the recursive walk repeated 8x.

Replaced with a C function `uop_leaf_tids` in `src/uop/leaf_tids.c`:
iterative, heap-loc-keyed visited bitmap, sub-millisecond on the
same graph.  Generic walk -- nothing grad-specific -- exposed via a
LibraryLink wrapper `thvm_wl_uop_leaf_tids`.

Bench (LeNet 8-weight forward graph):
- WL recursive walk:  ~1600 ms / call
- C iterative walk:   ~1 ms / call
- TGradMany (8 wts): 13270 ms -> 4 ms (**3000x**)

### 4. Default heap bump 16M -> 64M cells (128 MiB -> 512 MiB)

`HEAP_CAP` doubled twice from 1<<24 to 1<<26 cells.  Cheney
semi-spaces split this in half (256 MiB per side).  Removes the
"heap_alloc: from-space exhausted" crash on lenet train without
needing the THVM_GC_KB env override.  Mid-realize gc_collect is
risky because materialize stores heap-loc'd metadata in side tables
that don't get patched by GC; bumping the heap is the surgical
fix until those tables move into the relocate pass.

### 5. 100-token GPT-2 generation in `wl/Examples/gpt2/inference.wls`

Part 3 of the inference smoke now generates 100 tokens (env var
`N_GEN` overrides), benching eager vs TJit-replay variants
end-to-end.  Synthetic mini-config (vocab=32, dim=64, n_heads=4,
n_layers=1, seq=4):

```
eager 100-token wall: 53916 ms,  539.2 ms/token
jit   100-token wall: 40396 ms,  404.0 ms/token  (1.33x speedup)
TJit captured 30472 ops/step
generated tokens IDENTICAL between eager and TJit (capture-replay
correctness verified end-to-end on a real 100-step generation)
```

Real GPT-2-small (124M params) parity claim vs tinygrad still
deferred -- depends on the per-element kernel dispatch cost
(~13 µs / op observed; tinygrad gpt2 is ~10-50 ms/token for the
WHOLE forward).  Closing that needs the kernel-variant codegen
(Phase 14+ Opt/TKernelOpts slot already prepared).

## What's still slow (Phase 15)

The lenet 4-step convergence target is NOT met today.  The
per-step cost is dominated by C-side chain-rule expansion + per-op
kernel emit, NOT WL-side helpers and NOT heap pressure:

- `TRealize @ loss` (forward only, ~600 ms): emits ~30 kernels for
  the LeNet forward graph; one clang-JIT compile per fresh kernel
  shape, kid-cached on subsequent steps.
- `TRealize @ TGrad[loss, w_i]` (backward, dominant cost): the
  chain rule unrolls per-op grad terms via the existing
  `interact_grad` machinery, then emits ~20-50 kernels per param.
  At 8 params per step the per-step compute is in the seconds.

Per-grad memo + kernel batching are the two routes:
1. **Per-realize grad memo**: cache `interact_grad(uop, gy)`
   results keyed on `(uop_loc, gy_loc)` so when 8 weights all read
   through the same forward DAG, the chain-rule rewrites collapse.
2. **Kernel fusion across boundaries**: today each REDUCE / movement
   op forces a kernel boundary.  The Phase 10 `TKernelOpts` /
   `Opt[...]` slot is the user-facing knob; the missing piece is the
   variant emitter (`cg_emit_variants`) that consumes the
   `axis_types[]` view to emit fused tile/unroll variants.

## Files touched

- `src/schedule/materialize.c` -- new `materialize_inner_assigns`
  recursive descent helper called from `thvm_materialize`.
- `src/uop/leaf_tids.c` (new) -- iterative UOP-DAG -> distinct
  leaf tids walk.
- `src/thvm.c`, `src/thvm.h` -- include + decl for the new walk.
- `src/uop/grad.c` -- removed the in-flight `grad_leaf_tids` (moved
  to leaf_tids.c with a generic name).
- `src/thvm.h` -- `HEAP_CAP` 1<<24 -> 1<<26 (128 MiB -> 512 MiB).
- `wl/THVMLink/CSource/thvmlink.c` -- new
  `thvm_wl_uop_leaf_tids` LibraryLink wrapper.
- `wl/THVMLink/Kernel/THVMLink.wl` -- load $uopLeafTidsFn.
- `wl/THVMLink/Kernel/Tensor.wl` -- `uopLeafTids` calls the C
  walk; `tGradWithLeaves` accepts pre-computed leaves so
  `TGradMany` shares them across targets.
- `wl/THVMLink/Kernel/Optim.wl` -- TAdam body collapsed to one
  TRealize.
- `wl/Examples/gpt2/inference.wls` -- Part 3 generates 100 tokens
  (N_GEN env override).
- `docs/bench/phase14.md` -- this document.

WL grid: 393/393.
