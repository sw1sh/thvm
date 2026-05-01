# CPU backend and the codegen pipeline

The directory layout hides a real boundary. `src/backend/cpu/` is one
layer (runtime dispatch + buffer lifecycle + a per-UOp interpreter),
and `src/codegen/` is a second, backend-agnostic layer that produces
the source text the CPU JIT then hands to clang. They meet inside
`cpu_dispatch_kernel`, which tries the fast or specialized routes
first: BLAS pattern match, optional tile dispatch, C JITs, then the
scalar or legacy interpreters.

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

- [interpret.c](../src/backend/cpu/interpret.c): tree-walker.
  Allocates one scratch slot per program op, dispatches each
  `KProgOp` to a `cpu_op_*` kernel, and lands the final write on
  the caller's `out_buf_id`. Pre-materializes non-contiguous +
  multi-view inputs into temp buffers up front so the per-op
  kernels stay flat.
- [op/*.c](../src/backend/cpu/op/): one file per UOp opcode
  (`add.c`, `mul.c`, `reduce.c`, `expand.c`, `permute.c`, `cast.c`,
  ...). Each defines a single `cpu_op_<name>(dst, srcs, src_numels,
  p, n_elem)` body.
- [blas.c](../src/backend/cpu/blas.c): pattern matcher. Recognizes
  the exact `KProgOp[]` shapes produced by `TDot`, `TMatVec`,
  `TMatMul` and routes them to `cblas_{s,d}{dot,gemv,gemm}`. Returns
  a `KDispatchKind` (`KDISPATCH_BLAS_DOT` / `_GEMV` / `_GEMM`) on
  hit, 0 on miss.
- [jit.c](../src/backend/cpu/jit.c): clang JIT. Hashes the program,
  looks up `CPU_JIT_CACHE[CPU_JIT_CACHE_CAP]`, on miss calls
  `cg_emit(ke, &C_RENDERER)` to render C source, writes
  `/tmp/thvm_jit_<hash>.c`, shells out to `clang -O2 -fPIC -shared`,
  `dlopen`s the resulting `.dylib`, and `dlsym`s the `k` symbol.
  The same cache also carries scalar-UOp and tile-UOp generated C
  variants under distinct hash sentinels.

### Dispatch order

`cpu_dispatch_kernel` is the function the rest of the runtime calls
through `Backend.dispatch_kernel`. The body lives at the bottom of
[interpret.c](../src/backend/cpu/interpret.c) and reads:

1. Recover `kid = ke - KERNELS` and snapshot `t0 = cg_now_us()`.
2. `cpu_blas_dispatch(ke, ...)` first. On a non-zero return, record
   the route via `cg_profile_record(kid, blas_kind, ...)` and stop.
3. If `THVM_TILE=1`, try `cpu_jit_dispatch_tile(ke, ...)` for simple
   generated C tile plans, then `cpu_dispatch_tile(ke, ...)` for the
   tile interpreter fallback. On a hit, record `KDISPATCH_CPU_TILE`
   and stop.
4. Else `cpu_jit_dispatch(ke, ...)`. On a 1 return, record
   `KDISPATCH_JIT` and stop.
5. Else `cpu_jit_dispatch_scalar(ke, ...)`. On a 1 return, record
   `KDISPATCH_JIT` and stop.
6. Else, if scalar UOps exist, run `cpu_dispatch_scalar(ke, ...)` and
   record `KDISPATCH_INTERPRETER`.
7. Else fall through to `cpu_interpret(ke, ...)` and record
   `KDISPATCH_INTERPRETER`.

Each path writes its elapsed wallclock through `cg_profile_record`
(see [src/codegen/profile.c](../src/codegen/profile.c)) so per-kid
counters are aggregated regardless of which route fired.

## Codegen layer (`src/codegen/`)

Backend-agnostic. Produces source text plus optimization metadata.
The CPU JIT is its only consumer today; the Metal backend invokes
the same pipeline through `cg_emit_metal`.

- [axis.c](../src/codegen/axis.c): `axes_default_for(ke)`. Builds the
  default `KernelAxes` for a kernel: one `KAX_LOOP` axis per output
  dim, plus a trailing `KAX_REDUCE` axis sized at `src_numel /
  out_numel` if the program ends in `UOP_REDUCE`.
- [apply_opt.c](../src/codegen/apply_opt.c): `axes_apply_opt(ax,
  opt)`. Mutates the axes to record one `KOpt`. Splits an axis (UPCAST
  / UNROLL / LOCAL / GROUP / GROUPTOP) or swaps two (SWAP); appends
  to `applied_opts[]` either way.
- [propose.c](../src/codegen/propose.c): `kernel_opts_propose(ke, out,
  cap)`. Shape heuristics that nominate `KOpt` candidates. Today:
  reduce-tail kernels get `UNROLL` candidates in `{2, 4, 8, 16}`
  where divisible; flat elementwise kernels get `UPCAST` candidates
  on the same factor set.
- [cg.c](../src/codegen/cg.c): the driver. `cg_supports(ke)` gates
  acceptance (uniform dtype, no movement ops, REDUCE only as the
  final op, kinds restricted to SUM / MAX). `cg_emit(ke, renderer)`
  walks `program[]` and calls into the renderer's prologue / loop
  open / per-op emitters / loop close / epilogue.
- [render_c.c](../src/codegen/render_c.c): `Renderer C_RENDERER`.
  Emits a C99 `void k(void *out_v, const void *const *ins_v,
  unsigned n, const unsigned *in_numels)` with a fused per-output
  loop or, for reduce-tail, an outer `oi` loop wrapping an inner
  `_k` accumulator.
- [render_c_scalar.c](../src/codegen/render_c_scalar.c):
  `cg_emit_scalar` and `cg_emit_tile`.  Emits generated C from the
  rangeified scalar graph for f32/f64 elementwise and reduction
  kernels, including symbolic `S_INDEX_E` addresses built from `S_I*`
  expressions.  The tile variant lowers a validated `TILE_LOOP_NEST`
  to nested C loops before emitting the same scalar expression body.
- [render_metal.c](../src/codegen/render_metal.c): `cg_emit_metal`.
  Same shape, MSL flavor. One thread per output element instead of
  an outer loop; `device` / `device const` qualifiers; no `f` suffix
  on intrinsics.
- [profile.c](../src/codegen/profile.c): per-kid counters
  (`cg_profile_record`, `cg_kernel_dispatch_count`,
  `cg_kernel_total_us`) and static FLOPS estimation
  (`cg_kernel_flops`).
- [autotune.c](../src/codegen/autotune.c): `kernel_autotune(kid)`.
  Calls `kernel_opts_propose`, applies each candidate in isolation,
  benches `KAUTOTUNE_N_RUNS` fires per variant, picks the min, and
  leaves the kernel mutated to the winner. Cached per program shape
  via the `autotuned` flag on `KernelAxes`.

### `KernelAxes` and `TOpt`

`KernelAxes` is the shape-and-layout struct the renderer reads.
Two arrays of length `n_axes`: `axis_types[]` (one of `KAX_LOOP`,
`KAX_REDUCE`, `KAX_UPCAST`, `KAX_UNROLL`, `KAX_LOCAL`,
`KAX_GROUP_REDUCE`) and `full_shape[]` (the per-axis size after any
splits). A separate `applied_opts[]` records every `KOpt` that's
been folded in, so the JIT cache key can distinguish two kernels
that share a `KProgOp[]` but were autotuned differently.

A `KOpt` (alias `TOpt` on the WL side) is a triple `{op, axis, arg}`.
`op` is one of `KOP_UPCAST`, `KOP_UNROLL`, `KOP_LOCAL`, `KOP_GROUP`,
`KOP_GROUPTOP`, `KOP_SWAP`, `KOP_PADTO`, `KOP_NOLOCALS`, `KOP_TC`.
`apply_opt.c` translates each split-class opt into "shrink axis
`axis` by factor `arg`, insert a new sibling axis of the matching
`KAX_*` type with size `arg`".

The pipeline is `axes_default_for` -> `kernel_opts_propose` ->
(autotune loop: `axes_apply_opt` -> bench -> revert / keep) ->
`cg_emit` reads the final `applied_opts[]` and bakes the surviving
hints into the rendered source.

## Where the layers meet

CPU JIT path, end to end:

1. `cpu_dispatch_kernel` calls `cpu_jit_dispatch(ke, in_buf_ids,
   out_buf_id)`.
2. `cpu_jit_dispatch` early-returns 0 if `cg_supports(ke)` rejects
   the program (movement op, mid-program REDUCE, mixed dtype, ...).
3. It computes `key = cpu_jit_hash(ke)`, which folds `n_ops`,
   `n_inputs`, every per-input `numel`, every byte of `program[]`,
   and (when present) every entry of `axes->applied_opts[]`. The
   high bit is forced on so `key == 0` means empty slot. This is
   the cache key.
4. `cpu_jit_lookup_slot(key)` linear-probes `CPU_JIT_CACHE[256]`.
   On hit, the cached `CpuJitFn` is reused.
5. On miss, `cpu_jit_build` calls `cg_emit(ke, &C_RENDERER)` (in
   [cg.c](../src/codegen/cg.c)) to render the source, writes it to
   `/tmp/thvm_jit_<hash>.c`, shells out to clang, `dlopen`s the
   resulting `/tmp/thvm_jit_<hash>.dylib`, and `dlsym`s the `k`
   symbol. The slot is populated and returned.
6. A view-contiguity check rejects strided inputs (the renderer
   reads `in0[i]` flat and has no stride logic yet); on rejection
   `cpu_jit_dispatch` returns 0 and the dispatcher falls through
   to the interpreter.
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
runs the same codegen pipeline (`cg_emit_metal`) but the dispatch
shape is asymmetric. `cpu_dispatch_kernel` delegates to a
`cpu_jit_dispatch` helper that owns the JIT decision; if the JIT
bails the dispatcher falls through to a separate `cpu_interpret`.
`metal_dispatch_kernel` inlines the same decision in its own body:
it tries `metal_jit_encode` first, then on miss falls through to a
per-op interpreter loop that's also inlined in the same function.

The two dispatchers do the same thing logically; only the CPU side
has been factored into separate translation units. This is
historical, not a deliberate design: the Metal path was written
when the JIT was still a single shape and never got split. Cited
here so a reader doesn't conclude the two backends use different
strategies.

## What's not yet wired

Several axis types and opt classes are recorded in `KernelAxes` but
not honored by the renderer. They exist as scaffolding for the
upcoming structured-nest emitter; do not waste time grepping for
their codegen logic.

- `KAX_UPCAST`, `KAX_UNROLL`, `KAX_LOCAL`: the axis-type tags get
  written into `axis_types[]` by `axes_apply_opt`, but `render_c.c`
  only honors `KAX_PARALLEL` / `KAX_LOOP` (the flat per-output loop)
  and reads `applied_opts[]` directly to extract the LAST `UPCAST` /
  `UNROLL` factor as a single `#pragma clang loop unroll_count`
  hint. The split structure recorded in `axis_types[]` /
  `full_shape[]` is ignored.
- `KOP_PADTO`, `KOP_NOLOCALS`, `KOP_TC`: see the comment block in
  [apply_opt.c](../src/codegen/apply_opt.c) -- they are appended to
  `applied_opts[]` but the apply itself is a no-op on the axis
  structure, and no renderer reads them.

The roadmap for each lives in [PLAN.md](../PLAN.md) and
[kernelization.md](kernelization.md).
