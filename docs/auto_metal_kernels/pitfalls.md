# Pitfalls

A non-exhaustive list of traps from prior agent runs.  Save yourself
the iterations.

## Numerical

### `-INFINITY` poisons online softmax

Symptom: `correctness_err`, `max_abs=NaN`.

Cause: idle / OOB lanes in a softmax reduction set their value to
`-INFINITY`.  When the running-max recurrence runs `normalizer *=
exp(prev_max - new_max)` and both are `-INFINITY`, you compute
`exp(-inf - -inf) = exp(NaN) = NaN`.

Fix: use `-FLT_MAX` (most-negative-finite) instead.  Underflows
cleanly to zero in the recurrence:

```msl
#include <metal_stdlib>
using namespace metal;
// MLX uses Limits<AccT>::min which IS FLT_MIN_NEG -- not -INFINITY
float maxval = (oob ? -FLT_MAX : in_val);
```

This was the iter-1 NaN trap in `agent_softmax_msl`'s log -- one of
the most subtle MLX invariants.

### FP32 reduction order changes the result

Symptom: `correctness_err`, `max_abs ~ 1e-3 to 1e-2` on a kernel
that's logically correct.

Cause: NumPy reduces in left-to-right element order.  Your kernel
reduces in tree / simdgroup-collective order.  Round-off accumulates
differently.

Fix: relax to `max_rel ≤ 1e-3` (the harness already does this).
Don't try to match NumPy bit-exactly for parallel reductions.

### Mixed precision

The kernel signature uses `float`.  If you cast to `half` for
intermediate compute, the answer changes by ~1e-3 to 1e-2 -- often
beyond the harness threshold for fp32 baselines.  Either:
- Stay fp32 throughout (matches the score harness reference)
- Switch the harness to `mlx.float16` baseline + tolerate larger
  error

## Dispatch shape

### Grid is **total threads**, not number of TGs

`grid=(R*256, 1, 1)` with `threadgroup=(256, 1, 1)` runs **R**
threadgroups.  If you write `grid=(R, 1, 1)` thinking R-many TGs,
you actually dispatch R *threads* across `R / 256` TGs.

This is MLX-style (and matches the `mx.fast.metal_kernel` API).
Different from CUDA convention where grid = number of blocks.

### `lsize` must divide your data extent

If you want to read C floats per row with `lsize` threads each
reading `N_READS`:

```
lsize * N_READS >= C  AND  lsize is a multiple of 32
```

Otherwise some lanes will have no data and need OOB padding (see
`-FLT_MAX` above).  Pick `lsize = next_pow2(C / N_READS)` and pad
with sentinels.

### Threadgroup memory cap

M3 Max: ~64 KB per TG.  M1: ~32 KB.  Query at runtime via
`[device maxThreadgroupMemoryLength]` if you need dynamic sizing.

If your declared threadgroup arrays exceed the cap, the compile
silently falls back to a smaller register-only variant or fails at
PSO creation -- watch for cryptic compile errors mentioning
"register pressure" or "spill".

### `maxTotalThreadsPerThreadgroup` per-pipeline limit

Even though hardware supports 1024 threads/TG, your specific PSO
might be limited to less if the kernel uses many registers.  Query
via `pso.maxTotalThreadsPerThreadgroup` (Metal API, not exposed
via `Metal.compile_msl` -- you'd inspect from Objective-C side).

If you hit this, your dispatch will fail at runtime with an
unhelpful error.  Reduce TG size or the per-thread register
footprint.

## Buffer plumbing

### MTLBuffer alignment

`m.buf_alloc(size_bytes)` allocates with `MTLResourceStorageModeShared`.
Apple aligns to 16 bytes by default, which is fine for `float4`.
For `simdgroup_matrix` you may need 256-byte alignment -- use
explicit padding in your buffer layout.

### Reading back before `waitUntilCompleted`

`Metal.dispatch_timed` already calls `waitUntilCompleted` internally
before returning.  But if you call `m.dispatch` (no `_timed`),
the GPU command may not have run yet -- reading the output buffer
will give you stale data or zeros.

Always either:
- Use `dispatch_timed` (sync), or
- Call `dispatch` then `waitUntilCompleted` manually before
  `buf_read_array` (not exposed in current Python API; use
  `dispatch_timed` and ignore the timing).

## MLX baseline pitfalls

### `mx.eval` is the sync point

```python
c = mx.softmax(x_mx, axis=-1)
mx.eval(c)         # actually run the kernel
```

Without `mx.eval`, MLX defers the computation -- your timing measures
nothing.  The score harnesses call `mx.eval` inside the timed
function:

```python
def fn():
    c = mx.softmax(x_mx, axis=-1)
    mx.eval(c)
```

### MLX's dispatch overhead

For very small inputs (e.g. `(32, 256)`), MLX's Python dispatch
overhead is comparable to or exceeds GPU time.  You may see "MLX
takes 250us, your kernel takes 250us" both running ~10us of actual
GPU work.  In this regime, beating MLX means beating the *Python
overhead*, not the *kernel*.  Check `gpu_ns` vs `wall_ns` to confirm.

### `mlx.compile` vs eager

`mx.compile(fn)` traces and JIT-compiles to one fused kernel; can be
significantly faster than eager for multi-op pipelines.  If you're
beating `mx.softmax` by 1.3x but `mx.compile(lambda: mx.softmax(x))`
matches yours, MLX's eager dispatch overhead is what you beat -- not
the kernel.

## Compile errors

### `error: undefined symbol simd_max`

You forgot `#include <metal_stdlib>` and `using namespace metal;` at
the top.  Or you used the wrong namespace prefix.

### `error: 'simdgroup_matrix' was not declared in this scope`

Add `#include <metal_simdgroup_matrix>`.  This isn't pulled in by
`<metal_stdlib>`.

### `address space mismatch`

You dereferenced a `device` pointer through a `threadgroup` pointer
or vice versa.  MSL is strict about address spaces; you have to
explicitly copy:

```msl
threadgroup float scratch[256];
// can't do: device float *dp = ...; scratch = dp;
// have to do: scratch[lid] = dp[gid];
```

### `compile_err: program_source:N:M: error: too many arguments`

`mx.fast.metal_kernel` prepends the buffer signature for you.  Your
`source` should be the **body only** -- no signature.  If you write
`[[kernel]] void k(...)` in your source, MLX appends another
signature on top and you get this error.

For `Metal.compile_msl`, the opposite: you need the full signature.
Don't mix the two APIs.

### `error: undeclared identifier 'memory_order_acq_rel'`

MSL atomics support **only** `memory_order_relaxed`.  No `acq_rel`,
no `seq_cst`, no `acquire`, no `release`.  Use `memory_order_relaxed`
plus `threadgroup_barrier(mem_flags::mem_device)` if you need a
fence.  Found in the agent_vector_sum run building a "last-TG wins"
counter:

```msl
atomic_uint *ctr = ...;
uint prev = atomic_fetch_add_explicit(ctr, 1u,
    memory_order_relaxed);   // not acq_rel
threadgroup_barrier(mem_flags::mem_device);  // separate fence
```

## Concurrency / barriers

### `threadgroup_barrier` inside a control-flow branch is undefined

If a barrier sits inside `if (lid == 0) { ... }`, lanes that don't
take the branch never reach the barrier and the threadgroup deadlocks
or silently corrupts -- depends on the driver.  Symptom: output
intermittently wrong, often shape-dependent (works at small N, fails
at large N because TG count crosses some threshold).

```msl
// WRONG -- only lane 0 enters the barrier
if (lid == 0) {
    out[gid] = result;
    threadgroup_barrier(mem_flags::mem_device);
}

// RIGHT -- all lanes hit the barrier
if (lid == 0) {
    out[gid] = result;
}
threadgroup_barrier(mem_flags::mem_device);
```

Same rule for `simdgroup_barrier` and any `simd_*` collective
(`simd_sum` etc): every lane in the simdgroup must execute the
collective for the result to be defined.

## Iteration discipline

### Don't trust a single measurement

A 1.3x speedup that vanishes on the second run was never real.  Run
the score harness 3 times before declaring victory; if the median
speedup is below threshold, keep iterating.

### Save every measurement to RESULTS.md

Update the iteration table each time you change kernel.metal.  Future-
you (and the next agent) need to know what worked, what didn't, what
was noisy.  A 12-iteration log shows the actual landscape; one
"final kernel" + "0.9x speedup" entry hides the path.

### One change per iteration

If you change tile size AND vectorization AND reduce strategy in one
edit, you don't know what helped or hurt.  Change one thing,
measure, decide, then change the next.

## Workflow

### Don't run the full `make test` for one MSL change

It takes minutes and tests nothing relevant to your kernel.  The
score harness IS your test.

### Don't commit to main

Spawn-with-worktree gives you an isolated branch.  Commit there,
report back via `RESULTS.md` in your worktree.  Do not `git push`
unless explicitly told.
