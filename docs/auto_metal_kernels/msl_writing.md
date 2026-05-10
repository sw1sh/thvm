# Writing raw MSL

You have two ways to compile and dispatch MSL from Python:

| API | Where it lives | When to use |
|---|---|---|
| `Metal.compile_msl(src, fn=...)` | `py/thvm/thvm.py:431+` | Full control: you write the kernel signature and dispatch shape |
| `mx.fast.metal_kernel(...)` | MLX | MLX adds the buffer signature for you; you write the body |

Both compile MSL on first invocation, cache the PSO, then dispatch.
For an apples-to-apples comparison with MLX baselines, `Metal.compile_msl`
is the cleaner choice -- you control the same buffers and dispatch
shape MLX would, with no MLX-specific wrapping.

## `Metal.compile_msl` -- thvm's in-process MSL compiler

```python
from py.thvm import Metal
m = Metal()

src = """
#include <metal_stdlib>
using namespace metal;

[[kernel]] void k(
    device const float *in     [[buffer(0)]],
    device       float *out    [[buffer(1)]],
    constant     int   &axis_size [[buffer(2)]],
    uint gid [[threadgroup_position_in_grid]],
    uint lid [[thread_position_in_threadgroup]],
    uint simd_lane_id [[thread_index_in_simdgroup]]) {
    // ... your code ...
}
"""

pso = m.compile_msl(src, fn="k")
```

`fn` is the entry kernel name -- must match the `[[kernel]]`
declaration.

`compile_msl` returns a PSO handle.  Reuse across many dispatches.
Release with `m.pso_release(pso)` when done.

### Compile errors

If `compile_msl` raises `RuntimeError`, the message contains the
xcrun-metal compiler diagnostic.  In an agent loop, catch it and
record:

```python
try:
    pso = m.compile_msl(src, fn="k")
except RuntimeError as e:
    print("status=compile_err")
    print(f"reason={str(e)[:300]}")
    return 1
```

The `score.py` template in `py/examples/agent_softmax_msl/score.py`
shows the full pattern.

## `mx.fast.metal_kernel` -- MLX's MSL compiler

```python
import mlx.core as mx

kernel = mx.fast.metal_kernel(
    name="agent_kernel",
    input_names=["a", "b"],
    output_names=["c"],
    source=BODY_ONLY_NO_SIGNATURE,
    header="#include <metal_stdlib>\n#include <metal_simdgroup_matrix>\nusing namespace metal;",
    ensure_row_contiguous=True,
)

c, = kernel(
    inputs=[a_mx, b_mx],
    output_shapes=[(M, N)],
    output_dtypes=[a_mx.dtype],
    template=[("T", mx.float32)],
    grid=(Gx, Gy, Gz),
    threadgroup=(Tx, Ty, Tz),
)
mx.eval(c)
```

MLX prepends the buffer signature for you (`device const T *a [[buffer(0)]]`,
etc).  Your `source` is just the body.

`bench/metal-problems/runner/score_one.py` uses this path -- read it
for a full working example.

## Dispatch shape conventions

Both APIs use **MLX-style total threads**:

- `grid` is the total number of threads across the dispatch
  (`Gx * Gy * Gz` total).
- `threadgroup` is the threadgroup size (`Tx * Ty * Tz` threads
  per TG).
- The number of TGs dispatched is `(Gx/Tx, Gy/Ty, Gz/Tz)`.

Common patterns:

| Op | Common dispatch |
|---|---|
| Per-row kernel (softmax, layernorm) | `grid=(R*tg, 1, 1)`, `threadgroup=(tg, 1, 1)` -- one TG per row |
| Per-tile matmul | `grid=((M/BM)*(N/BN)*BM*BN, 1, 1)`, `threadgroup=(BM*BN, 1, 1)` |
| Elementwise | `grid=(numel/k, 1, 1)`, `threadgroup=(256, 1, 1)`, each thread does k |
| Tree reduction | First pass `grid=(numel/k, 1, 1)`, second pass `grid=(num_partials, 1, 1)` |

The `dispatch.json` file in agent workspaces lets you choose between
explicit `grid` or `grid_per_row` (multiplied by R at score time):

```json
{"grid_per_row": [256, 1, 1], "threadgroup": [256, 1, 1]}
```

Means: one TG of 256 threads per row.  `grid` becomes `R*256`.

## What MSL features are available

- All of `<metal_stdlib>`: `float`, `float2/3/4`, `half`, `int*`, etc.
- `simdgroup_matrix<T, 8, 8>` via `<metal_simdgroup_matrix>`.
- Simdgroup collectives: `simd_sum`, `simd_max`, `simd_min`,
  `simd_and`, `simd_or`, `simd_xor`, `simd_shuffle`,
  `simd_shuffle_xor`, `simd_broadcast`, `simd_ballot`.
- `fast::exp`, `fast::log`, `fast::sin`, `fast::cos` -- ~10x faster
  than precise variants, ~2 ULP error.
- `[[threadgroup]]` shared memory: declare `threadgroup float
  scratch[256];` inside the kernel.
- Barriers: `threadgroup_barrier(mem_flags::mem_threadgroup)` and
  `simdgroup_barrier(mem_flags::mem_simdgroup)`.

## What's NOT available on Apple GPUs

- **No `cp.async`**: no async global-to-shared copies.
  Software-pipelined matmul on Apple uses two threadgroup-shared
  staging buffers and explicit barrier on the alternate buffer.
- **No TMA / no warp specialization**: you can't dedicate a
  simdgroup to "load only" while others "compute only" with hardware
  enforcement.  Manual scheduling via barrier patterns.
- **No tensor cores in the NVIDIA sense**: `simdgroup_matrix<float, 8, 8>`
  is the closest equivalent, but it's a 1-instruction MMA per
  simdgroup -- not a hardware accelerator like Turing/Hopper.
  Throughput ~17-18 cycles for the FP32 8x8 frag.

## AIR / metallib (offline path -- usually not needed)

If you need to compile MSL ahead of time (e.g. for thvm's persistent
metallib cache):

```bash
xcrun -sdk macosx metal -c kernel.metal -o kernel.air
xcrun -sdk macosx metallib kernel.air -o kernel.metallib
```

`Metal.compile_msl` does this in-memory using `MTLDevice
newLibraryWithSource:`, which goes through the same pipeline.  For
agent work the in-memory path is what you want.

## Inspecting compiled AIR

To see what the metal compiler did to your kernel:

```bash
xcrun -sdk macosx metal -S kernel.metal -o kernel.air.s
```

Useful for confirming `simdgroup_matrix` actually emitted, that
`fast::exp` lowered to the intrinsic and not a software fallback,
etc.  Probably overkill for the first iteration -- reach for it when
your kernel has the structure you want but is mysteriously slower
than expected.

## What thvm's renderer emits

If you use the UOp-DAG path instead of writing raw MSL, the renderer
in `src/codegen/render_uop.c` emits:

- One `[[kernel]] void k(...)` per `STORE` root.
- Buffer args ordered by `instance` field (0 = output, 1.. = inputs).
- A `for` loop nest matching the RANGE structure.
- `OPT(_, kind, factor)` annotations get translated:
  - `OPT_FAST_MATH` -> `fast::exp2 / fast::log2 / fast::sqrt`.
  - `OPT_SIMD_REDUCE` -> simdgroup-strided loop + `simd_sum`/`simd_max`.
  - `OPT_VEC_LOAD(width)` -> `(device const floatN*)addr` reinterpret.
  - `OPT_TC` -> `simdgroup_matrix<float, 8, 8>` MMA scaffolding (matmul-shaped).
- The kernel signature includes `thread_index_in_simdgroup` etc.
  on demand (the renderer adds builtins as KOpts use them).

You can dump rendered MSL via `h.render(root, name="k")` and compare
against what you'd hand-write.  Often the easiest way to learn what
the renderer is capable of is to run the matmul BEAM loop and read
the MSL it produced.
