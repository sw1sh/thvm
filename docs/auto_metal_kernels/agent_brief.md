# Agent brief: write a Metal kernel that beats MLX

## Mission

Pick or accept a workload from `bench/metal-problems/`.  Write a
Metal Shading Language kernel that beats `mlx`'s implementation of
the same op on Apple M-series GPU.

You measure success with the score harness:

```
candidate=p50:Xus p10:Yus
mlx_baseline=p50:Xus p10:Yus
speedup_vs_mlx=Kx
```

**Stop conditions:**
- `speedup_vs_mlx ≥ 1.05x` median across 3 fresh score runs at
  every shape your problem requires, OR
- 12 iterations completed, OR
- Time cap reached.

If you stop on iterations or time without hitting 1.05x, that's
still a useful run -- log every iteration in `RESULTS.md` so the next
agent can pick up the work.

## What you need to know

Read these in order, only what's relevant to your op:

1. **[setup.md](setup.md)** -- 5 min.  Build deps, venv, sanity check.
2. **[python_api.md](python_api.md)** -- 5 min.  `Metal.compile_msl`,
   `dispatch_timed`, the `K` constants.
3. **[mlx_reference.md](mlx_reference.md)** -- 10 min.  The five
   techniques (`simd_*`, `fast::exp`, N_READS, two-stage TG reduce,
   `-FLT_MAX`).  Read the exact MLX source for your op.
4. **[msl_writing.md](msl_writing.md)** -- 5 min.  MSL syntax,
   dispatch shape conventions, what's available on Apple GPUs.
5. **[profiling.md](profiling.md)** -- 5 min.  Score harness contract,
   how many samples, p10 vs p50.
6. **[pitfalls.md](pitfalls.md)** -- 5 min.  The NaN trap, dispatch
   shape gotchas, MLX timing traps.
7. **[autotuning.md](autotuning.md)** -- only if you choose the
   autotune track (KOpts via `kernel_apply_opt`).

## Pick your track

### Track A: Raw MSL (recommended for first iteration)

You write `kernel.metal` + `dispatch.json`, score with `./score.sh`.
Full control over MSL.  Maximum freedom; minimum scaffolding.

Workspace template: `py/examples/agent_softmax_msl/` -- copy the
whole directory, edit kernel.metal, edit dispatch.json, run
`./score.sh R C`.

Iteration loop:
1. Edit `kernel.metal` (one change, see [pitfalls.md](pitfalls.md)
   "one change per iteration").
2. Run `./score.sh <shape>`.
3. Append a row to the iteration log in `RESULTS.md`.
4. Repeat.

### Track B: Autotune via thvm KOpts

You build a UOp DAG in Python, propose KOpt candidates, apply them in
sequence, render to MSL, time, pick the winner.  Less freedom (only
what thvm's renderer can emit) but no MSL writing required.

Workspace template: `py/examples/matmul_beam_loop.py` -- copy and
adapt for your op.

Read [autotuning.md](autotuning.md) for the KOpt vocabulary and the
proposer.  This track is most effective for matmul + simple
reductions where `KOP_TC` + `KOP_GLOBAL` + `KOP_SIMD_REDUCE` cover
the search space.

### Track C: Raw MSL + thvm autotune (advanced)

Write a parameterised raw MSL template, sweep parameters from
Python, score each.  Hybrid of A and B.  Useful when you want
shape-table dispatch (MLX's pattern -- see
[mlx_reference.md](mlx_reference.md) section 4 of AUTOTUNE.md).

## Workspace setup

You were spawned with an isolated git worktree.  `cd` to the
worktree path you were given.  Create your run directory:

```
mkdir -p py/examples/agent_<op>_<your-id>/
cp py/examples/agent_softmax_msl/{score.sh,score.py,STARTER.md} \
   py/examples/agent_<op>_<your-id>/
# now edit your run dir's kernel.metal + dispatch.json
```

Or for the autotune track:

```
mkdir -p py/examples/agent_<op>_<your-id>/
cp py/examples/matmul_beam_loop.py py/examples/agent_<op>_<your-id>/loop.py
# adapt loop.py for your op
```

## What to write at the end

`RESULTS.md` in your workspace dir.  Required sections:

- **Iteration log** (table: iter | change | speedup at each shape)
- **Final kernel** (or "see kernel.metal" / "see loop.py")
- **Final dispatch** (json or grid/threadgroup tuple)
- **3-run variance** (table: run | cand p50 | mlx p50 | speedup)
- **One thing surprising about MLX** (the line of MLX source you
  didn't expect)
- **2-3 micro-optimizations to try with more budget**

The variance row is non-negotiable.  A single 1.5x measurement is
not a 1.5x speedup -- the noise floor on Mac GPU timing is ~2x for
sub-millisecond kernels.

## Hard rules

- **Don't commit to `main`**.  You're on a worktree branch.
- **Don't `git push`**.  Unless told.
- **Don't change anything outside `py/examples/agent_<op>_<your-id>/`**
  unless you're porting a feature back into thvm's renderer
  (rare; ask first).
- **Don't trust a single measurement.**  Run 3 times.
- **One change per iteration.**  Bisect-friendly.

## Hints for specific ops

### softmax
- Read `external/mlx/mlx/backend/metal/kernels/softmax.h` lines
  11-98 (`softmax_single_row`).
- `-FLT_MAX` not `-INFINITY` for OOB.
- Two-stage simdgroup reduce: per-thread max -> `simd_max` ->
  partials[] -> barrier -> first SG reduces partials.
- `fast::exp` not `exp` for the kernel.
- `N_READS = 4` or `8`; `lsize = 256` or `512`.

### matmul
- Read `external/mlx/mlx/backend/metal/kernels/steel/gemm/`.
- `simdgroup_matrix<float, 8, 8>` MMA.
- Multi-frag accumulator: `simdgroup_matrix<...> Cregs[BM/8 *
  BN/8]` -- chain MMA calls per tile.
- Cooperative load via threadgroup-shared staging.
- Tile (BM, BN, BK) ~ (32, 32, 32) is a reasonable starting point;
  bigger is better up to register cap.

### vector_sum / reduce
- `simd_sum` for inner reduce.
- Two-stage if input > one TG worth: per-TG partial -> second
  kernel reduces partials.
- For very small inputs, a single 32-thread simdgroup is enough.

### layernorm
- Welford fused mean+var pass: one loop over the row, accumulating
  `(mean, M2)` so you don't need a second pass.
- See `external/mlx/mlx/backend/metal/kernels/layer_norm.h`.
- One TG per row, same `N_READS` vectorization as softmax.

### vector_add (elementwise)
- Memory-bandwidth bound.  All you can do is `float4` loads + grid-
  stride loop.
- Hard to beat MLX here; the kernel is one MMA away from peak BW
  even for naive Metal.

## Time budget guidance

- **First iteration in 5 minutes**: get a working naive kernel
  scored, even if it's 0.2x MLX.  Confirms the harness works for
  your op + shape.
- **Iterations 2-6 in 10-15 min each**: port one MLX technique per
  iteration (vectorize loads, then `simd_*`, then `fast::exp`, then
  two-stage TG reduce, then tune `lsize`/`N_READS`).
- **Iterations 7-12**: variance reduction, edge-case handling
  (small / large shapes), 3-run variance check.
- **Total**: ~2-3 hours of focused work for a softmax/reduce; longer
  for matmul.

## When you're done

- Update `RESULTS.md` (use the format above).
- Stage and commit on your worktree branch:
  ```
  git add py/examples/agent_<op>_<your-id>/
  git commit -m "agent_<op>_<your-id>: <speedup>x at <shape>"
  ```
- Report back: terminal print the path to your worktree + the
  one-line summary from RESULTS.md.  Do not push.
