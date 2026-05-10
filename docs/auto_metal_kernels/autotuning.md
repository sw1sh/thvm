# Autotuning: KOpts, BEAM, the apply loop

## The KOpt vocabulary

KOpts are atomic kernel-level rewrites.  Each is a triple
`(op, axis, arg)` of three integers: the opcode, the axis it targets,
and a shape parameter (factor/extent/etc).

Every KOpt corresponds to a UOp DAG mutation in
`src/uop/apply_opt_dag.c`.

| Opcode | C name | What it does | `arg` semantics |
|---|---|---|---|
| 0 | `KOP_NONE` | no-op marker | unused |
| 1 | `KOP_UPCAST` | split axis into outer LOOP + inner UPCAST(factor) | factor (must divide extent) |
| 2 | `KOP_UNROLL` | split into outer LOOP + inner UNROLL(factor) | factor |
| 3 | `KOP_LOCAL` | split into outer LOOP + inner LOCAL(factor) (no OPT wrap) | factor |
| 4 | `KOP_GROUP` | split into outer LOOP + inner GROUP_REDUCE(factor) | factor |
| 5 | `KOP_GROUPTOP` | same as GROUP, top-level invariant | factor |
| 6 | `KOP_SWAP` | swap axis_id `axis` <-> `arg` on every RANGE | other axis_id |
| 7 | `KOP_PADTO` | reserved | unused |
| 8 | `KOP_NOLOCALS` | reserved | unused |
| 9 | `KOP_TC` | wrap inner REDUCE with `OPT(_, TC, factor)` (simdgroup_matrix dispatch) | tile (8, 16, 32) |
| 10 | `KOP_GLOBAL` | flip axis type LOOP -> GLOBAL on `axis` (one TG per index) | unused (pass 0) |
| 11 | `KOP_FAST_MATH` | wrap every `EXP2`/`LOG2`/`SQRT` with `OPT(_, FAST_MATH, 0)` -- emits `fast::exp2` etc | unused |
| 12 | `KOP_SIMD_REDUCE` | wrap every `REDUCE` with `OPT(_, SIMD_REDUCE, 0)` -- emits `simd_sum` / `simd_max` | unused |
| 13 | `KOP_VEC_LOAD` | wrap contiguous `INDEX_E` with `OPT(_, VEC_LOAD, width)` -- vectorized load | width (4 or 8) |

### Axis types after a split

Splits convert `RANGE(axis, LOOP, N)` into:

```
IADD(IMUL(RANGE(axis, LOOP, N/k), CONST(k)),
     OPT(RANGE(axis+1, <innerKax>, k), <optKind>, k))
```

with these mappings:

| KOpt | innerKax | optKind |
|---|---|---|
| `KOP_UPCAST` | `KAX_UPCAST` | `OPT_UPCAST` |
| `KOP_UNROLL` | `KAX_UNROLL` | `OPT_UNROLL` |
| `KOP_LOCAL`  | `KAX_LOCAL`  | (none -- no OPT wrap on inner) |
| `KOP_GROUP`  | `KAX_GROUP_REDUCE` | `OPT_GROUP_REDUCE` |
| `KOP_GROUPTOP` | `KAX_GROUP_REDUCE` | `OPT_GROUP_REDUCE` |

All other RANGEs with axis_id > target are renumbered +1 (the new
inner steals id `axis+1`).  See `apply_opt_dag.c:308` for the C
implementation.

### Idempotency

`FAST_MATH`, `SIMD_REDUCE`, `VEC_LOAD` are idempotent: applying twice
is a no-op (the C walker normalises the wrap factor on already-
wrapped nodes).  This matters for BEAM: you can re-propose the same
KOpt without polluting the DAG.

## `kernel_opts_propose` (the candidate enumerator)

```python
cands = h.kernel_opts_propose(kid, cap=64)
```

Returns up to `cap` `(op, axis, arg)` tuples.  The proposer is in
`src/codegen/propose.c`; current rules:

- For every matmul-shaped REDUCE: propose `KOP_TC(axis, factor)` for
  factor in {8, 16, 32} (gated on `K_dim % factor == 0`).
- For every LOOP axis with extent > 1: propose `KOP_GLOBAL(axis, 0)`.
- For every reducible axis: propose `KOP_UPCAST(axis, factor)` /
  `KOP_UNROLL(axis, factor)` for factor in {2, 4, 8}.

Not yet proposed automatically (open work): `KOP_FAST_MATH`,
`KOP_SIMD_REDUCE`, `KOP_VEC_LOAD`.  Apply these by hand if they fit
the pattern.

## `kernel_apply_opt` (the mutator)

```python
new_root = h.kernel_apply_opt(kid, (op, axis, arg))
```

Returns the new STORE root or `None` if the KOpt was rejected
(invalid axis, doesn't divide, etc).  Single-shot: applies one KOpt;
the BEAM loop is in user code.

When `cached_lift.store_root != 0` (set via `kernel_set_cached_lift`),
this routes through `uop_dag_apply_kopt()` -- direct DAG mutation,
no axis log.  Without `cached_lift`, falls back to the legacy
`KpSchedule.applied_opts[]` log path (deprecated in F6).

## A typical BEAM loop (Python)

The pattern from `py/examples/matmul_beam_loop.py`:

```python
def make_kernel(M, N, K_dim, *, kopts):
    h = Thvm()
    root = build_matmul(h, M, N, K_dim, with_tc=False)
    kid = h.kernel_alloc()
    out_buf = h.buffer(K.SCOPE_GLOBAL, K.FP32, (M, N), instance=0)
    a_buf = h.buffer(K.SCOPE_GLOBAL, K.FP32, (M, K_dim), instance=1)
    b_buf = h.buffer(K.SCOPE_GLOBAL, K.FP32, (K_dim, N), instance=2)
    h.kernel_set_cached_lift(kid, root, out_buf, [a_buf, b_buf])
    for kopt in kopts:
        new_root = h.kernel_apply_opt(kid, kopt)
        if new_root is None:
            raise RuntimeError(f"kernel_apply_opt({kopt}) failed")
        root = new_root
    return kid, root, h.render(root, name="k")
```

Then for each candidate sequence:

1. `make_kernel(...)` -> `(kid, root, msl)`.
2. `m.compile_msl(msl, fn="k")` -> `pso`.
3. Allocate buffers, write inputs.
4. `m.dispatch_timed(...)` 20-30 times, take p10/p50.
5. Compare to numpy (or MLX) reference for correctness.
6. Record `(cand, p50, gflops, correct)`.
7. Sort, pick winner.

The full driver in `matmul_beam_loop.py` runs ~6 candidates against
`mx.matmul` baseline and prints a table.  Use it as a template; copy
to your run dir, swap `build_matmul` for your fixture builder, swap
the `dispatch_for(M, N, K, parallel=...)` shape selector for your
op's grid layout.

## When NOT to use BEAM (and reach for raw MSL instead)

The `kernel_opts_propose` candidate set is **what thvm's renderer can
emit today**.  It does not include:

- Two-stage threadgroup reductions (extent > one simdgroup's worth).
- Online softmax (running max + scale-prev-norm).
- `simdgroup_matrix` cooperative load + multi-frag accumulator tiles.
- `cp.async`-style software pipelining (Apple has no `cp.async`, so
  this means manual multi-stage threadgroup buffers).
- Any kernel-fusion (softmax + matmul, layernorm + linear, etc).

If you need any of the above, write raw MSL; see
[msl_writing.md](msl_writing.md).  The current BEAM is most useful
for matmul + simple reductions, where `KOP_TC` + `KOP_GLOBAL` + maybe
`KOP_SIMD_REDUCE` cover ~80% of the search space.

## Quick KOpt sanity check (no DAG needed)

If you only want to know whether a KOpt sequence renders to valid
MSL for your shape, drive `kernel_apply_opt` once and call `render`:

```python
from py.thvm import Thvm, K
h = Thvm()
# ... build root ...
kid = h.kernel_alloc()
h.kernel_set_cached_lift(kid, root, out_buf, [a_buf, b_buf])
for kopt in [(K.KOP_TC, 0, 8), (K.KOP_GLOBAL, 0, 0)]:
    new_root = h.kernel_apply_opt(kid, kopt)
    if new_root is None:
        print(f"reject: {kopt}")
        break
    root = new_root
print(h.render(root, name="k"))
```

Compile-fail on `Metal.compile_msl` is your hard signal that the KOpt
sequence produced ill-formed MSL -- record the error string and the
sequence in `RESULTS.md`.
