# CPU backend and the codegen pipeline

The directory layout hides a real boundary. `src/backend/cpu/` is one
layer (runtime dispatch + buffer lifecycle + UOp-DAG walker and JIT),
and `src/codegen/` is a second, backend-agnostic layer that turns a
lifted UOp DAG into either C or Metal source text. They meet inside
`cpu_dispatch_kernel`, which tries BLAS first, then the UOp walker,
then the clang JIT, then the rangeify scalar-UOp interpreter.

## Runtime layer (`src/backend/cpu/`)

One file per piece of state or per logical operation. The dispatch
order lives in [src/backend/cpu/_.c](../src/backend/cpu/_.c) (the
`Backend` vtable) and [src/backend/cpu/interpret.c](../src/backend/cpu/interpret.c)
(the body of `cpu_dispatch_kernel`).

Vtable wiring + lifecycle:

- [src/backend/cpu/_.c](../src/backend/cpu/_.c): assembles the
  `CPU_BACKEND` vtable. Every `cpu_*` function below is wired here.
- [src/backend/cpu/init.c](../src/backend/cpu/init.c): `cpu_init` /
  `cpu_shutdown`. Walks live `CPU_BUFS[]` on shutdown to release
  owned + external storage.

Buffer lifecycle (one file per primitive, all reading/writing the
same `CPU_BUFS[]` table that `TenDesc.buf_id` indexes into):

- [buf_alloc.c](../src/backend/cpu/buf_alloc.c): `cpu_buf_alloc`
  (calloc + freelist scan) and `cpu_buf_alloc_external` (zero-copy
  wrap of someone else's bytes, with an `on_release` callback).
- [buf_free.c](../src/backend/cpu/buf_free.c): hard release of one
  slot. Honors `owns_data` vs external.
- [buf_incref.c](../src/backend/cpu/buf_incref.c),
  [buf_decref.c](../src/backend/cpu/buf_decref.c): refcount nudges;
  decref to zero pushes the slot onto the freelist (it does not
  `free`).
- [buf_read.c](../src/backend/cpu/buf_read.c),
  [buf_write.c](../src/backend/cpu/buf_write.c): host-pointer
  read/write of buffer bytes. The runtime uses these for WL
  bridge round-trips and for tests.
- [buf_pool.c](../src/backend/cpu/buf_pool.c): high-water-mark
  scope for `TRealize` brackets. `pool_begin` / `pool_rollback`
  free everything allocated in between, modulo a per-buf
  `preserved` bit.
- [buf_freelist.c](../src/backend/cpu/buf_freelist.c): per-size
  freelist of recyclable slots. Linear scan; fine at our scale.

Compute paths:

- [uop_walk.c](../src/backend/cpu/uop_walk.c): UOp DAG walker.
  Reads the cached lift root (`ke->cached_lift.store_root`) and
  evaluates the DAG directly against the input/output buffers,
  mirroring the traversal pattern of `cg_render_uop_kernel_c`
  without going through clang. Primary CPU compute path for any
  kernel the lifter accepts.
- [interpret.c](../src/backend/cpu/interpret.c): holds the
  `cpu_dispatch_kernel` entry point that pre-materializes
  non-contiguous + multi-view inputs into temp buffers before
  running the dispatch ladder. The legacy interpreters
  (`cpu_interpret`, `cpu_dispatch_scalar`, `cpu_dispatch_tile`) have
  all been deleted; the UOp DAG walker in `uop_walk.c` is the
  primary compute path.
- [blas.c](../src/backend/cpu/blas.c): pattern matcher. Recognizes
  the exact `KProgOp[]` shapes produced by `TDot`, `TMatVec`,
  `TMatMul` and routes them to `cblas_{s,d}{dot,gemv,gemm}`. Returns
  a `KDispatchKind` (`KDISPATCH_BLAS_DOT` / `_GEMV` / `_GEMM`) on
  hit, 0 on miss.
- [jit.c](../src/backend/cpu/jit.c): clang JIT. Hashes the program,
  looks up `CPU_JIT_CACHE[CPU_JIT_CACHE_CAP]`, on miss renders C99
  source by calling `cg_render_uop_kernel_c_root` against
  `ke->cached_lift.store_root`, writes
  `/tmp/thvm_jit_<hash>.c`, shells out to `clang -O2 -fPIC -shared`,
  `dlopen`s the resulting `.dylib`, and `dlsym`s the `k` symbol.

### Dispatch order

`cpu_dispatch_kernel` is the function the rest of the runtime calls
through `Backend.dispatch_kernel`. The body lives at the bottom of
[interpret.c](../src/backend/cpu/interpret.c) and reads:

1. Recover `kid = ke - KERNELS` and snapshot `t0 = cg_now_us()`.
2. `cpu_blas_dispatch(ke, ...)` first. On a non-zero return, record
   the route via `cg_profile_record(kid, blas_kind, ...)` and stop.
3. If `THVM_CPU_UOP_WALK` is not set to `0`, call
   `cpu_uop_walk(ke, ...)`. On success, record
   `KDISPATCH_INTERPRETER` and stop. The walker is the primary CPU
   compute path; the JIT below remains reachable when the walker
   declines.
4. `cpu_jit_dispatch(ke, ...)`. On a 1 return, record
   `KDISPATCH_JIT` and stop.
5. Otherwise return 0 with `KDISPATCH_INTERPRETER` recorded.

Each path writes its elapsed wallclock through `cg_profile_record`
(see [src/codegen/profile.c](../src/codegen/profile.c)) so per-kid
counters are aggregated regardless of which route fired.

## Codegen layer (`src/codegen/`)

Backend-agnostic. Produces source text plus optimization metadata.
The CPU JIT and the Metal backend both consume the same UOp DAG
renderer.

- [axis.c](../src/codegen/axis.c): `axes_default_for(ke)`. Builds the
  default `KernelAxes` for a kernel: one `KAX_LOOP` axis per output
  dim, plus a trailing `KAX_REDUCE` axis sized at `src_numel /
  out_numel` if the program ends in `UOP_REDUCE`.
- [apply_opt.c](../src/codegen/apply_opt.c): `axes_apply_opt(ax,
  opt)`. Mutates the axes to record one `KOpt`. Splits an axis (UPCAST
  / UNROLL / LOCAL / GROUP / GROUPTOP), marks a full LOOP axis as
  GLOBAL, or swaps two (SWAP); appends to `applied_opts[]` either way.
- [propose.c](../src/codegen/propose.c): `kernel_opts_propose(ke, out,
  cap)`. Shape heuristics that nominate `KOpt` candidates: reduce-tail
  kernels get `UNROLL` candidates in `{2, 4, 8, 16}` where divisible;
  flat elementwise kernels get `UPCAST` candidates on the same factor
  set.
- [cg.c](../src/codegen/cg.c): the driver. `cg_supports(ke)` gates
  acceptance (uniform dtype, no movement ops, REDUCE only as the
  final op, kinds restricted to SUM / MAX).
- [render_uop.c](../src/codegen/render_uop.c): the UOp DAG renderer.
  Walks a lifted UOp DAG rooted at a `STORE` and emits either C99
  (via `cg_render_uop_kernel_c_root`, consumed by the CPU JIT) or
  Metal Shading Language (via `cg_render_uop_kernel_root`, consumed
  by the Metal tile JIT).
- [render_metal.c](../src/codegen/render_metal.c): `cg_emit_metal`
  and `cg_emit_tile_metal`. Lifts every accepted kernel shape
  (matmul, conv2d_flat, elementwise, reduce, movement-fused) to a
  UOp DAG and routes through `render_uop`. Multi-output kernels and
  a few legacy shapes fall through to the per-shape MSL emitters in
  this file.
- [profile.c](../src/codegen/profile.c): per-kid counters
  (`cg_profile_record`, `cg_kernel_dispatch_count`,
  `cg_kernel_total_us`) and static FLOPS estimation
  (`cg_kernel_flops`).
- [autotune.c](../src/codegen/autotune.c): `kernel_autotune(kid)`.
  Calls `kernel_opts_propose`, applies each candidate in isolation,
  benches `KAUTOTUNE_N_RUNS` fires per variant, picks the min, and
  leaves the kernel mutated to the winner. Winners are cached per
  program shape in memory via the `autotuned` flag on `KernelAxes`
  and on disk under `$XDG_CACHE_HOME/thvm/autotune` (or
  `$HOME/.cache/thvm/autotune`, or `THVM_AUTOTUNE_CACHE_DIR`).  Set
  `THVM_AUTOTUNE_CACHE=0` or `THVM_AUTOTUNE_DISABLE_CACHE=1` to force
  fresh benchmarking.
- [hand_opts.c](../src/codegen/hand_opts.c): handcrafted opt
  templates for recognized GEMM / conv shapes. Bypasses the propose
  + bench loop for shapes where a known opt sequence is already a
  good answer.
- [tile_anno.c](../src/codegen/tile_anno.c): a façade over
  `applied_opts[]` plus per-axis `KAX_*` types. Single source of
  truth for the JIT hash and for the renderer's split-structure
  reads.

### `KernelAxes` and `TOpt`

`KernelAxes` is the shape-and-layout struct the renderer reads.
Two arrays of length `n_axes`: `axis_types[]` (one of `KAX_LOOP`,
`KAX_REDUCE`, `KAX_UPCAST`, `KAX_UNROLL`, `KAX_LOCAL`,
`KAX_GLOBAL`, `KAX_GROUP_REDUCE`) and `full_shape[]` (the per-axis
size after any splits). A separate `applied_opts[]` records every
`KOpt` that's been folded in, so the JIT cache key can distinguish
two kernels that share a `KProgOp[]` but were autotuned differently.

A `KOpt` (alias `TOpt` on the WL side) is a triple `{op, axis, arg}`.
`op` is one of `KOP_UPCAST`, `KOP_UNROLL`, `KOP_LOCAL`, `KOP_GROUP`,
`KOP_GROUPTOP`, `KOP_GLOBAL`, `KOP_SWAP`, `KOP_PADTO`,
`KOP_NOLOCALS`, `KOP_TC`. `apply_opt.c` translates each split-class
opt into "shrink axis `axis` by factor `arg`, insert a new sibling
axis of the matching `KAX_*` type with size `arg`". `KOP_GLOBAL`
marks an existing full LOOP axis as `KAX_GLOBAL`; this is how a
`LOCAL` split becomes a Metal-style `GLOBAL x LOCAL` plan. `KOP_TC`
is kernel-aware metadata for recognized f32 GEMM/MMA plans; its
`arg` selects the Metal GEMM tile size. `KOP_PADTO` and
`KOP_NOLOCALS` are rejected by `apply_opt.c` until a renderer reads
them, which keeps no-op opts out of `applied_opts[]` and out of JIT
cache keys.

The pipeline is `axes_default_for` -> `kernel_opts_propose` ->
(autotune loop: `axes_apply_opt` -> bench -> revert / keep) ->
`cg_render_uop_kernel_*_root` reads the final `applied_opts[]` and
bakes the surviving hints into the rendered source.

## Where the layers meet

CPU JIT path, end to end:

1. `cpu_dispatch_kernel` reaches `cpu_jit_dispatch(ke, in_buf_ids,
   out_buf_id)` after BLAS, the tile interpreter, and the UOp
   walker have all declined.
2. `cpu_jit_dispatch` early-returns 0 if `cg_supports(ke)` rejects
   the program (movement op, mid-program REDUCE, mixed dtype, ...)
   or if `ke->cached_lift.store_root == 0` (lifter declined at
   materialize time).
3. It computes `key = cpu_jit_hash(ke)`, which folds `n_ops`,
   `n_inputs`, every per-input `numel`, every per-input view stride
   pattern (and any `ShapeTracker` prior-view chain), either the
   lifted `store_root` Term value (preferred) or the raw `KProgOp[]`
   bytes (fallback), and every entry of `applied_opts[]`. The high
   bit is forced on so `key == 0` means empty slot. This is the
   cache key.
4. `cpu_jit_lookup_slot(key)` linear-probes
   `CPU_JIT_CACHE[CPU_JIT_CACHE_CAP]`. On hit, the cached `CpuJitFn`
   is reused.
5. On miss, a warmup gate (`CPU_JIT_WARMUP` fires) holds the
   compile back; the dispatcher returns 0 and the dispatcher falls
   through to `cpu_dispatch_scalar`. Once the threshold is crossed,
   `cpu_jit_build` calls `cg_render_uop_kernel_c_root(store_root,
   "k", fp)` to render the source, writes it to
   `/tmp/thvm_jit_<hash>.c`, shells out to clang, `dlopen`s the
   resulting `/tmp/thvm_jit_<hash>.dylib`, and `dlsym`s the `k`
   symbol. The slot is populated and returned.
6. A view-contiguity check rejects strided inputs that the renderer
   doesn't have stride logic for; on rejection `cpu_jit_dispatch`
   returns 0 and the dispatcher falls through.
7. Otherwise pack input pointers + numels into local arrays and
   call `jfn(out, ins, numel, nums)`.
8. Back in `cpu_dispatch_kernel`, `cg_profile_record(kid,
   KDISPATCH_JIT, elapsed)` lands the route + wallclock in
   `K_PROFILE[kid]` for the `TKernelProfile` / `TKernelDispatchKind`
   surfaces to read.

The cache is keyed at the granularity of "program shape": same
opcodes + same per-input numels + same applied opts hash to the
same slot. Two kernels with identical bodies share one `.dylib`.
A persistent `/tmp/thvm_jit_<hash>.dylib` is honored across
processes (a stat-only freshness check; no source-content
verification).

## Comparison with Metal

The Metal backend at [src/backend/metal/_.m](../src/backend/metal/_.m)
emits MSL through `cg_emit_tile_metal`, which lifts every kernel
shape it accepts to a UOp DAG via `kernel_lift_to_uop` and renders
through `cg_render_uop_kernel_root`. See `docs/lowering_passes.md`
for the pipeline. Both backends share `render_uop.c`; the CPU side
just appends a clang invocation, while the Metal side hands MSL to
the Apple driver.

## Honored axis types and opt classes

Several axis types and opt classes are recorded in `KernelAxes`;
the renderer honors them as follows.

- The UOp DAG renderer in `render_uop.c` lowers tile plans with
  `LOOP`, `UPCAST`, `LOCAL`, `GLOBAL`, `REDUCE`, `UNROLL`, and
  `GROUP_REDUCE` axes. `UPCAST` and `UNROLL` become inline
  expansion / `#pragma clang loop unroll_count` on the CPU side and
  thread-vector / threadgroup splits on the Metal side.
- `KOP_TC`: accepted through the kernel-aware apply path for
  recognized f32 GEMM/MMA plans. It records a tile-size choice in
  `applied_opts[]`, so Metal `metal-gemm` can benchmark 8/16/32
  threadgroup tile variants.
- `KOP_PADTO`, `KOP_NOLOCALS`: reserved and rejected by
  [apply_opt.c](../src/codegen/apply_opt.c) until a renderer reads
  them. This keeps no-op opts out of `applied_opts[]` and out of JIT
  cache keys.

The roadmap for each lives in [PLAN.md](../PLAN.md).
