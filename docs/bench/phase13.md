# Phase 13 — perf gating fix + IC-idiomatic optimizer surface

The Phase 13 brief in the parity plan was "tie everything together:
token-by-token GPT-2 generation, JIT'd via Phase 7, KV-cached".  The
gating issue blocking that was the constant-arg kernel-program-cache
miss documented in Phase 11 / 12: any elementwise op against a
host-side numeric scalar took ~60 ms / call (eager) and ~3 s per FFN
block at real GPT-2 dim=768.  Phase 13 fixes the cache miss at the
WL frontend layer and re-shapes the optimizer surface around lazy
in-backend updates.  The 100-token GPT-2 bench moves to Phase 14
once the per-token wall is short enough to be meaningful.

## Headline numbers

| What                                          | Before   | After  | Speedup |
|-----------------------------------------------|----------|--------|---------|
| `x + 1` steady-state (8x64)                   | 60 ms    | 0.17 ms| 350x    |
| `TGELU[x]` steady-state (8x64)                | 270 ms   | 4 ms   | 70x     |
| `forward.wls` e2e (mini GPT-2, seq=4 dim=64)  | 100,000 ms | 694 ms | 150x   |
| `inference.wls` Part 2 single-token forward   | 200,000 ms | 653 ms | 300x   |
| GPT-2 NetModel FFN block @ dim=768            | 6,000 ms | 3,500 ms | ~2x   |

The last one (real GPT-2 dim=768) is the smallest improvement
because the FFN block has ~30 unique elementwise patterns, only
some of which were lifting bare scalars; the rest were already
through the cache-friendly `TLinear` / `TLayerNormAffine` paths.

## Root cause + fix (Plus / Times pre-EXPAND)

`Plus[t_TTerm, scalar]` lifted the scalar via `liftNumeric` to a
shape-`{1}` `TUOpConst` and handed both to `TUOpAdd`.  The
elementwise dispatch's numel-cycle broadcast then aligned the rank-0
scalar across the rank-N TTerm, producing a different `KProgOp[]`
kernel than the rank-matched form.  The differing kernel structure
miss-hashed against the kernel-program-cache, forcing clang JIT
recompile on every call (eager) and a fresh KernelEntry on every
TJit replay too.

The fix: `broadcastScalar` in `wl/THVMLink/Kernel/Tensor.wl` checks
the lifted operand's `tUopShape`; if it's `{}` or `{1}` and the
LHS shape differs, wrap in `TUOpExpand[scalar, lhsShape]` so the
binop kernel always sees rank-matched inputs.  Same approach as
tinygrad's `Tensor._broadcasted` (pre-expands to the common shape
in the frontend before the binop fires).

Side effect: this exposed a real buf-aliasing edge case in the
existing TAdam (more shape-matching intermediates → freelist pop
sometimes returned an externally-pinned buf into a downstream
intermediate).  The TAdam rewrite below dodges it.

## TAdam rewrite

Old shape (caller-threaded host-Adam):
```
{wRef, mRef, vRef} = TAdamHostStep[hosts, grads, mAll, vAll,
                                    lr, beta1, beta2, eps, t]
hostsAll = wRef; mAll = mRef; vAll = vRef;
```

New shape (in-backend, lazy):
```
TAdam[loss, params, mList, vList, t,
      "lr" -> 0.001, "beta1" -> 0.9, "beta2" -> 0.999, "eps" -> 1e-8]
```

Differences:
- `loss` scalar in, grads computed internally via `TGradMany` (one
  realize, shared forward DAG, per-target memo dedup).
- Hyperparameters carried as Wolfram options
  (`opts : OptionsPattern[]`) with `Options[TAdam]` defaults; no
  more 4-arg positional `lr, beta1, beta2, eps` ceremony.
- Update is a lazy `UOP_ASSIGN` chain per buffer:
  `m := beta1*m + (1-beta1)*grad`;
  `v := beta2*v + (1-beta2)*grad*grad`;
  `w := w - lrHat*m / (sqrt(v)*invSqrtB2cor + eps)`.
- No host round-trip; weights stay TTerm tensor handles from init
  through every step, m/v running buffers are also TTerms seeded
  via `TZerosLike /@ params`.

### Three-realize-per-param: documented blocker

The lazy ASSIGN chain currently fires as THREE separate `TRealize`s
per param (one per buffer write) instead of one combined realize.
Cause is in `src/wnf/_.c` lines 140-166: the WNF dispatch on
`UOP_ASSIGN` forces only its IMMEDIATE src/dst children to TAG_TEN.
A src expressed as `TUOpAdd[ASSIGN_m, ASSIGN_v]` stays at TAG_UOP
because non-kernel non-assign UOPs are "WNF by themselves" (line
170) and never recurse into their children.  So a single outer
`TAssign[w, ...]` wrapping two sibling ASSIGNs leaves the inner
ASSIGNs un-fired (verified by /tmp/test_assign_simple.wls during
Phase 13).

Phase 14 fix: extend WNF to drain every ASSIGN reachable from the
redex before settling the parent UOP, OR teach materialize to
insert dependency edges so ASSIGNs fire before downstream kernels
read their dst.  Until then, three realizes is correct; TJit
replay still captures the kernels so steady-state cost is just
3x the per-realize dispatch overhead (~tens of µs).

## Purged surface

Removed entirely (per the user's "make in-backend lazy version
work already" directive):

- `TAdamHostInit` / `TAdamHostStep` — caller-threaded host-side
  Adam helpers.
- `TAdamSessionInit` / `TAdamSessionStep` / `TAdamSessionDrop` +
  `$adamSessions` — session-scoped wrappers around the above.
- `tF32` / `tZerosLike` private helpers — replaced by
  `TUOpConst[N @ x]` and the public `TZerosLike`.
- `wl/THVMLink/Tests/adam_host.wlt`,
  `wl/THVMLink/Tests/adam_session.wlt` — entire test files for
  the deleted helpers.
- 2 tests in `optim.wlt` for the private helpers.
- `wl/Examples/_bench/baseline.wls` — outdated bench harness
  built around the host-Adam pattern.

Updated callers:
- `wl/Examples/lenet-mnist/train.wls` and `verify.wls` rewritten
  to keep weights as TTerms throughout, use `TGradMany` for batched
  grads, and call `TAdam[loss, params, mState, vState, k]` (default
  hyperparams) per step.
- `wl/Examples/linear-train/train.wls` switched to plain `{gW, gB}
  = TGradMany[loss, {w, b}]` destructuring (the old `body`
  callback parameter was no-op sugar).

## Idiomatic UpValues

A handful of WL-builtin / NetModel forms now lower to TTerm graphs
automatically (no per-call alias):

| WL form                       | UpValue lowering                |
|-------------------------------|---------------------------------|
| `a . b` (rank-2 @ rank-2)     | `TMatMul[a, b]`                 |
| `Transpose[t]`                | reverse-axis permute            |
| `ArrayReshape[t, shape]`      | `TUOpReshape`                   |
| `Power[t, n_Integer ? Positive]` | folded MUL chain             |
| `Tanh[t]`                     | `TTanh`                         |
| `SoftmaxLayer[axis][t]`       | `TSoftmaxAxis[t, axis-1]`       |

The SoftmaxLayer-call form uses the layer-bound TTerm UpValue
trick to side-step Wolfram's own Layer-call dispatch:

```
TTerm /: l_SoftmaxLayer[t_TTerm ? tensorTermQ] :=
    TSoftmaxAxis[t, NetExtract[l, "Parameters"]["Level"] - 1]
```

`l_SoftmaxLayer` matches any `SoftmaxLayer[...]` and binds the
whole layer to `l`; the rule still attaches to TTerm.  Same
pattern generalises to any `XLayer[...][_TTerm]` form.

Plus the `TGradMany[y, targets]` simplification: the prior 3-arg
form took a `body` callback that was a no-op wrapper around `List`.
Now returns the list of TGrad TTerms directly; callers destructure
with `{ga, gb} = TGradMany[loss, {a, b}]`.

## What's NOT done (Phase 14)

- **WNF / materialize fix** to drain sibling ASSIGNs in one
  realize (let TAdam collapse to a single TRealize per step).
- **TJit-wrapped per-token GPT-2 forward** (the inference loop on
  the synthetic mini-config); without it the 100-token bench
  vs tinygrad is still hours per run at real GPT-2 scale.
- **AttentionLayer per-head NetGraph wiring** inside `TFromNet`
  (Phase 12 deferral).
- **KV cache via view-aware `TAssign`**.
- **Heap exhaustion on multi-step LeNet train**.
  `wl/Examples/lenet-mnist/train.wls` runs out of GC heap on
  step 1 of 4: `heap_alloc: from-space exhausted (need 10 cells,
  HEAP_NEXT=8388601, cap=8388608)`.  TGradMany over 8 LeNet
  weights produces a deep enough graph that one realize fills
  the 64 MB Cheney from-space; the GC trigger threshold doesn't
  collect mid-realize.  Workaround: `THVM_GC_KB=131072
  wolframscript wl/Examples/lenet-mnist/train.wls` or batch the
  step into one weight-group at a time.  Real fix in Phase 14:
  either grow the default heap or trigger gc_collect inside the
  realize loop when from-space crosses a threshold.

## Files touched

- `wl/THVMLink/Kernel/Tensor.wl` — `broadcastScalar` in Plus/Times
  UpValues, simplified `TGradMany`.
- `wl/THVMLink/Kernel/Optim.wl` — `TAdam` rewritten (loss in,
  grads via TGradMany, lazy ASSIGN per buffer, Options pattern);
  host-Adam helpers removed.
- `wl/THVMLink/Tests/{nn,grad,optim}.wlt` — tests for the new
  TAdam signature, `TGradMany` 2-arg form.
- `wl/THVMLink/Tests/{adam_host,adam_session}.wlt` — deleted.
- `wl/Examples/lenet-mnist/{train,verify}.wls` — revamped.
- `wl/Examples/linear-train/train.wls` — destructure-on-result.
- `wl/Examples/_bench/baseline.wls` — deleted.
- `wl/GUIDE.md` — new "Never split a binary operator's operands
  across lines" rule.
- `docs/bench/phase13.md` — this document.

WL grid: 393/393 (down from 404 because of the 11 host-Adam tests
removed in this purge).
