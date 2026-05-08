# softmax via thvm UOp DAG -- agent task

Goal: write a row-wise softmax kernel for Apple M3 Max via thvm's UOp
DAG path, score it against numpy + mlx baselines.

## What "row-wise softmax" means

Given `x` shape `(R, C)` fp32:
```
m[r]      = max over c of x[r, c]
y[r, c]   = exp(x[r, c] - m[r])
z[r]      = sum over c of y[r, c]
out[r, c] = y[r, c] / z[r]
```

## Tools

- `Thvm` (`from py.thvm import Thvm, K`) -- UOp DAG construction
- `Metal` -- in-process compile + dispatch + timing
- `Thvm.kernel_apply_opt(kid, kopt)` -- apply KOpt to mutate the DAG
  (vocab: KOP_TC, KOP_GLOBAL, KOP_UPCAST, KOP_UNROLL, KOP_LOCAL,
   KOP_GROUP, KOP_GROUPTOP, KOP_SWAP)
- `Thvm.render(root, name)` -- post-mutation DAG -> MSL string

## Reference baselines

- `numpy`: `(np.exp(x - x.max(axis=1, keepdims=True))).T / sum_per_row`
- `mlx`: `mx.softmax(x, axis=-1)` -- the bar to beat (or get close to)

## Approaches to try (in order)

1. **Build the softmax UOp DAG** directly. Single STORE root with the
   full expression inlined (the renderer hoists each REDUCE to its own
   accumulator preamble). REDUCE_MAX + REDUCE_SUM are both supported.
   Note: thvm has UOP_EXP2 and UOP_LOG2 but NOT UOP_EXP directly --
   build `exp(x) = exp2(x * log2(e))` if needed.

2. **Render and inspect** -- print the MSL the renderer produces.
   Verify it has both reduce loops + the final normalize loop.

3. **Compile + dispatch** -- compare to numpy reference within
   `rtol=1e-4 atol=1e-5`.

4. **Parallelize**: apply `KOP_GLOBAL` on the row axis. Each
   threadgroup owns one row. Compare to MLX.

5. **Tile sizes**: KOP_LOCAL with row-relative C-axis split (each
   simdgroup handles a slice of the row, threadgroup-shared
   accumulator).

## Honest expectation

This is the **first non-matmul agent run** on the new Phase E surface.
You'll likely hit gaps -- e.g. propose may not return softmax-aware
candidates yet (it's matmul-gated only), or some specific UOp pattern
may not render as expected. Document what you hit; the friction is
the data we want.

## Final report

- The kernel.py you ended on, with comments
- One-line-per-iteration log: `[iter N] change: <X>; result: <Y>`
- Final speedup_vs_mlx + correctness numbers
- 2-3 things that surprised you or hurt
- 2-3 things you'd try next given more budget
