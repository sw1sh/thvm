# Phase 12 - declarative TFromNet @ NetModel + idiomatic WL surface

The Phase 12 brief in the parity plan was "tie everything together:
token-by-token generation, JIT'd via Phase 7, KV-cached via Phase 11,
kernels BEAM'd via Phase 10".  In practice the constant-arg
kernel-program-cache miss documented in Phase 11 (~270 ms per
constant-multiply at any size) makes a full GPT-2 forward unrunnable
in eager mode -- one FFN block on real GPT-2 dim=768 takes ~6 s, so a
12-layer forward is ~minutes per token.

Phase 12 therefore re-scopes to the prerequisite work that closes the
declarative-surface gap and lets the inference harness READ as
Mathematica.  The actual 100-token-vs-tinygrad bench is
Phase 13's deliverable, gated on the C-side cache fix.

## What lands

### 1. TFromNet now handles real GPT-2 sub-graphs

`wl/THVMLink/Kernel/NN.wl` gains dispatch for the layer kinds the
`NetModel["GPT Transformer ..."]` uses on its forward path:

| Layer kind                 | TFromNet dispatch                              |
|----------------------------|------------------------------------------------|
| `EmbeddingLayer`           | `TEmbeddingMatrix` (1-indexed -> 0-indexed)    |
| `SequenceIndicesLayer`     | host-side `Range[1, len]`                       |
| `NormalizationLayer`       | `TLayerNormAffine` (Scaling + Biases)           |
| `DropoutLayer`             | identity (inference-mode)                       |
| `ThreadingLayer[Plus|Times]` | `Total[xs]` / `Times @@ xs`                  |
| `ElementwiseLayer`         | apply Function -- WL UpValues construct the UOp graph automatically |
| `NetMapOperator[LinearLayer]` | `TLinear` (transposes Wolfram's `{out,in}` weights) |
| `NetGraph`                 | top-level topological eval over `NetExtract[g, All]` keys + `LayersGraph` edges (projected to top-level via prefix matching) |

Multi-input layers in a NetGraph (residual `+`) get the NetGraph's
input port prepended automatically when the predecessor count is
fewer than `Information[layer, "InputPortNames"]` arity.  This
covers the residual-add convention.

### 2. Idiomatic UpValues

`wl/THVMLink/Kernel/Tensor.wl` gains UpValues so user code reads as
ordinary Mathematica plus implicit thvm dispatch:

| WL form                       | UpValue lowering                       |
|-------------------------------|----------------------------------------|
| `a . b`                       | `TMatMul[a, b]` (rank-2) / `TDot` (rank-1) |
| `Transpose[t]`                | reverse-axis permute                   |
| `Transpose[t, perm]`          | 1-indexed perm -> 0-indexed permute    |
| `ArrayReshape[t, shape]`      | `TUOpReshape`                          |
| `Power[t, n_Integer ? Positive]` | folded MUL chain                    |
| `Tanh[t]`                     | `TTanh[t]`                             |
| `SoftmaxLayer[axis][t]`       | `TSoftmaxAxis[t, axis-1]`              |

The SoftmaxLayer-call form is non-obvious: a plain `TTerm /:
SoftmaxLayer[axis_][t_TTerm]` does NOT fire because Wolfram's own
`Layer[...][input]` dispatch runs first.  The fix is binding the
WHOLE layer in the UpValue pattern:

```
TTerm /: l_SoftmaxLayer[t_TTerm ? tensorTermQ] :=
    TSoftmaxAxis[t, NetExtract[l, "Parameters"]["Level"] - 1]
```

`l_SoftmaxLayer` matches any `SoftmaxLayer[...]` and binds it to
`l`; layer parameters come out via `NetExtract[l, "Parameters"]`.
Same trick generalises to NormalizationLayer, ElementwiseLayer,
etc. when those need direct call-form dispatch.

### 3. Surface cleanups

- `TGELU` is now the textbook formula
  `0.5 * x * (1 + Tanh[Sqrt[2/Pi] * (x + 0.044715 * x^3)])` --
  the `t^3` lowers cleanly via the new Power UpValue.
- `TAttention` reads as
  `SoftmaxLayer[2][(q . Transpose[k]) / Sqrt[N @ dk]] . v`,
  matching the textbook scaled-dot formula.
- Dropped explicit `"f32"` dtype tags at every TUOpConst /
  liftNumeric call site -- the parameter already defaults to `"f32"`.
- Replaced 3 `Fold[Plus, First[xs], Rest[xs]]` patterns with `Total[xs]`
  (the WL Plus UpValue catches the expansion).
- Deleted the dedicated `Gpt2.wl` module; everything lives in NN.wl
  next to the rest of the layer surface, matching tinygrad's flat
  `tinygrad/nn/__init__.py` layout.
- Added `TLinear[x, W, b]` (nn.Linear analogue) and `TOnes[shape]`.
- Added forward-decls for `TTanh`, `TMatMul`, `TDot`, `TSoftmaxAxis`
  in Tensor.wl so the new UpValues bind to public symbols (Tensor.wl
  loads alphabetically before NN.wl).

## Numerical parity vs Wolfram

`wl/Examples/gpt2/inference.wls` runs Phase 12's smoke:

```
=== Part 1: TFromNet conversion of real GPT-2 sub-graphs ===
embed sub-NetGraph children: {embeddingtokens, posembed, embeddingpos, inputCombine, dropout}
  embed thvm wall: 31. ms
  shape: {4, 768} (expected {4, 768})
  max abs diff vs Wolfram: 0.
FFN  sub-NetGraph children: {linear1, gelu, linear2, dropout, +, norm}
  ffn thvm wall: 4879. ms
  shape: {4, 768} (expected {4, 768})
  max abs diff vs Wolfram: 2.35e-6
PASS: TFromNet matches Wolfram on embed + FFN sub-graphs

=== Part 2: synthetic mini-GPT-2 forward (single token) ===
  config: vocab=32 dim=64 n_heads=4 n_layers=1 seq=4
  prompt ids: {7, 13, 21, 30}
  single-forward wall: 201927. ms
  logits shape: {4, 32}
  greedy next token id: 24
PASS
```

The embed sub-NetGraph matches Wolfram's own evaluation BIT-EXACT
(diff = 0).  The FFN sub-graph matches to 2.35e-6 -- f32
accumulation tolerance.  These are real numerical-parity claims
against the canonical reference.

## What's NOT done (Phase 13)

### AttentionLayer / per-head NetGraph wiring

The attention sub-NetGraph in GPT-2's NetModel is a 14-node
NetGraph with 12 per-head clusters of `{key, query, value,
scaling, attention}` plus `{13: CatenateLayer, 14: NetMapOperator[
LinearLayer]}` (the output projection).  TFromNet's NetGraph
dispatch handles single-input topo-sorted graphs, but the
per-head AttentionLayer takes an ORDERED `{Q, K, V}` triple from
three named predecessors -- and Wolfram's `LayersGraph` flattens
those into untagged edges, so we can't recover the input order
from the graph alone without parsing the original NetGraph
specification (which `Information[g, ...]` doesn't expose
positionally).

Phase 13 path: extract the connection spec (`NetGraph`'s third
positional arg, accessible via Wolfram internal API or by
inspecting `g[[Key["Connections"]]]` / similar) and use the
SOURCE-PORT names to thread inputs to AttentionLayer in the
correct positional order.

Until that lands, the `TMultiHeadAttention[q, k, v, n_heads, mask]`
helper is the thvm-side path; users compose it inline rather
than getting it for free from `TFromNet @ NetModel[...]`.

### Constant-arg kernel-program-cache miss

Documented in `docs/bench/phase11.md`.  The 270 ms-per-call cost
is the SAME steady-state cost (TJit replay doesn't help; the
kernels themselves miss the cache and re-JIT each replay).
Without `TUOpConst` hash-cons (or a constant-lifted cache key),
the predicted 100-token GPT-2 inference bench is unrunnable on
real GPT-2 dimensions.  Phase 13 prerequisite.

### KV cache + view-aware TAssign

Tinygrad's `cache.shrink(...).assign(...)` idiom needs a
view-aware `TAssign` that writes only into a SHRINK'd slice of
the cache buf rather than the whole buf.  Existing `TAssign`
materializes the full source and memcpys; needs a
`uop_assign_view` path.

## Files touched this phase

- `wl/THVMLink/Kernel/NN.wl` -- TFromNet dispatch extensions, GPT-2
  sub-graph traversal, refactor of TAttention / TMultiHeadAttention
  to the idiomatic surface, Total-style stitching.
- `wl/THVMLink/Kernel/Tensor.wl` -- Power[t, n_Integer], Tanh,
  Transpose, Dot, ArrayReshape, SoftmaxLayer-call UpValues; forward
  decls.
- `wl/THVMLink/Kernel/Optim.wl` -- drop explicit "f32" in tF32.
- `wl/Examples/gpt2/inference.wls` (new) -- the Phase 12 smoke
  (sub-graph parity + single-token forward).
- `docs/bench/phase12.md` -- this document.

Test grid: 404/404 (no regression).
