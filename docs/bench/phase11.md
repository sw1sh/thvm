# Phase 11 - GPT-2 building blocks

Phase 11 in the parity plan calls for the WL composition that
turns the Phase 5/6 layer surface into a working GPT-2 forward
pass: token + position embedding lookup, multi-head causal
self-attention, GELU FFN, layer norm with learnable scale/shift.
"No new C/IR work expected" - and that holds: every block below
lowers to existing UOPs.

## What lands

All five GPT-2 helpers go straight into `wl/THVMLink/Kernel/NN.wl`
alongside `TLinear`, `TLayerNorm`, `TAttention` etc.  Tinygrad
keeps the same surface flat in `tinygrad/nn/__init__.py` (no
`gpt2.py` next to `optim.py`), so a dedicated `Gpt2.wl` module
would have been mis-organised; the public surface lives where
the rest of `nn` lives, and the model assembly lives in
`wl/Examples/gpt2/forward.wls` (analogue of tinygrad's
`extra/models/gpt2.py`).

| Helper | Behavior |
|--------|----------|
| `TEmbedding[table, idx]` | Static-int row gather: `SHRINK[{idx, idx+1}, ..] + RESHAPE`. |
| `TEmbeddingMatrix[table, ids]` | Per-id `TEmbedding` + PAD-and-sum stitch into `{Length[ids], D}`. |
| `TGELU[x]` | Tanh-form approximation: `0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))`.  Uses `x*x*x` (no `Power[t_TTerm, n_Integer]` UpValue). |
| `TCausalMask[seq]` | Host-allocated `{seq, seq}` Real32 with 0 / -1e9 pattern. |
| `TLayerNormAffine[x, gamma, beta]` | `TLayerNorm[x] * gamma + beta` with EXPLICIT EXPAND of gamma/beta. |
| `TMultiHeadAttention[Q, K, V, n_heads, mask]` | Reshape + permute to `{nHeads, seq, dHead}`, per-head scaled-dot, PAD-and-sum stitch back to `{seq, dim}`. |

Two corollary additions to the surface:

- `TLinear[x, W, b]` - `nn.Linear` analogue.  Exists explicitly
  because `TMatMul[x, W] + b` for `b:{N}` does NOT broadcast
  correctly via the elementwise numel-cycle (see "Broadcast bug"
  below); it has to go through an explicit `EXPAND[reshape(b, {1,N}), {seq, N}]`.
- `TOnes[shape]` - mirror of `TZeros`; layer-norm gamma init.

## Broadcast bug discovered + fixed

Initial `TLayerNormAffine` reshaped gamma/beta from `{D}` to
`{1, D}` and relied on the elementwise dispatch's "implicit
broadcast" comment in the docstring of `TUOpAdd / TUOpMul`.
That comment was wrong for rank >= 2:

```
{4, 16} + {16}     -> first row correct, rows 1..3 are garbage
                      (literal values like 11843.7 mixed with
                      denormals; looks like buffer-overrun).
{4, 16} + EXPAND[{1,16}, {4,16}]  -> correct.
```

The numel-cycle in `TUOpAdd`/`TUOpMul` only aligns the first
leading-axis row when the right operand has fewer dims than the
left.  Fix: every `TLayerNormAffine` and `TLinear` reshape
explicitly EXPANDs to the full output shape before the elementwise
op.  Documented in both helpers' docstrings.

This was the cause of the initial NaN/Inf in `forward.wls`
(`LibraryFunction::fpexc`); without the fix every per-layer
output past row 0 was already corrupted, and downstream `Exp`
tipped over.

## Bench

`wl/Examples/gpt2/forward.wls` runs a tiny config (vocab=32,
dim=16, n_heads=2, n_layers=1, seq=4) and asserts the output
logits are finite and shape `{seq, vocab}`.  Per-layer eager
profile on M-series CPU:

```
TLayerNormAffine[{seq,dim}]:   2.16 ms
TGELU[{seq,dim}]:            266.67 ms   <- the gap
TMultiHeadAttention:           3.61 ms
TLinear[{seq,dim},{dim,dim}]:  0.87 ms
TEmbeddingMatrix[seq]:         0.11 ms
```

End-to-end one-block forward: ~100 s.  The 1000x gap from "sum
of per-layer pieces" to e2e is the same kernel-program-cache
miss that makes TGELU slow.

## Why TGELU eats 270 ms

Decomposition:

```
                         kernels  wall
x + 1 (Integer 1)            +1   ~64 ms  - DOES NOT cache
1 + Exp[x]                   +2   ~60 ms  - DOES NOT cache
TTanh[x] (= (e^2x-1)/(e^2x+1))  +2  ~155 ms - DOES NOT cache
x * x (TTerm operand)        +1   ~0.1 ms - caches fine
TMatMul                       +1   ~0.1 ms - caches fine
```

Pattern: any elementwise op whose right (or left) operand is a
host-side Integer/Real (lifted via `liftNumeric` -> a fresh
`TUOpConst` -> a fresh TenDesc with a fresh buf_id) misses the
kernel-program-cache because the cache key includes input tids.
Pure-TTerm ops cache cleanly: same tids + same shapes hash to
the same KProgOp[] sequence.

TGELU has ~5 numeric-constant operations in the chain (`0.5`,
`Sqrt[2/Pi]`, `0.044715`, `1`, the `+1` in `1 + Tanh[..]`), each
spawning ~1-3 kernels of ~80 ms clang-JIT cost.  Multiply through
and you get ~270 ms / call, every call.

This is a pre-existing perf gap exposed by Phase 11, not a Phase
11 regression.  Phase 12 path is one of:

1. **TUOpConst hash-cons** - intern `TUOpConst[scalar, "f32"]`
   so identical scalar constants always return the same TenDesc
   loc / buf_id.  Then the kernel-program-cache hashes match.
2. **Constant lifting in cache key** - hash on `(KProgOp[], shape,
   {nonconst input tids})` and let constants be folded into
   the kernel literals.  More invasive (cache key change).
3. **Lift to materialized TTensor once** - in TGELU specifically,
   pre-allocate `oneT = TTensorCreate @ TOnes[..]` of the right
   shape per invocation.  Defeats the point of `TGELU` being a
   one-liner; not the right place to hide the bug.

(1) is the cleanest surgery; (2) is the most general fix.  Either
would also speed up TLayerNormAffine's e2e dramatically (the per-
op cost drops to BLAS-or-cached levels).

For Phase 11 the smoke is correctness-first: forward returns
finite logits.  TJit replay would also paper over the issue
(captured kernels replay regardless of cache miss), but the
small smoke deliberately runs eager so the cache-miss surface
is visible.

## Phase 12 expectations

Per the parity plan, Phase 12 closes the gpt2 inference harness:

- KV cache via `cache.shrink(...).assign(...)` idiom.  Tinygrad's
  trick is `assign` into a SHRINK'd view of the cache; thvm's
  `TAssign` writes the full buf, so a view-aware `TAssign` is the
  prereq.
- TJit-wrapped per-token forward so the constant-arg cache miss
  doesn't compound across tokens.
- Real GPT-2 tokenizer + weight load (or at least matching the
  tinygrad `extra/models/gpt2.py` layer-by-layer numerics on a
  fixed prompt).
- Either the TUOpConst hash-cons (option 1 above) or constant-
  lifted cache key (option 2) if the post-Phase-11 bench still
  bottlenecks on constant-arg kernel JIT cost.

## Files touched this phase

- `wl/THVMLink/Kernel/NN.wl` - added `TEmbedding`, `TEmbeddingMatrix`,
  `TGELU`, `TCausalMask`, `TLayerNormAffine`, `TMultiHeadAttention`,
  `TLinear`, `TOnes`.  No dedicated `Gpt2.wl` (matches tinygrad's
  flat `nn/__init__.py` layout).
- `wl/Examples/gpt2/forward.wls` (new) - synthetic tiny config,
  per-layer profile, one-block forward, sanity assertion.
- `docs/bench/phase11.md` - this document.

Test grid: 404/404 (no regression).
