# tilelang scout report: ideas to port into thvm

Scope: Microsoft `tilelang` (a Python DSL for tile-level GPU kernels, built on TVM TIR)
checked out at [tilelang/](../tilelang/) (symlink to `/Users/swish/src/tilelang`). This
report maps tilelang's design onto thvm's existing lowering and autotune layers and
calls out what is portable, what isn't, and what we'd buy by porting it.

References to thvm internals use repo-relative paths so they're clickable. References
to tilelang use absolute paths since the tree is vendored as a sibling repo.

## TL;DR

tilelang is a tile-level kernel DSL (think CUTLASS as a Python embedded language)
plus an autotuner that picks tile sizes, pipeline depth, and warp/thread counts
against a small set of templates ("carver"). Below TVM, it owns about 60 IR passes
that turn high-level tile ops (`T.gemm`, `T.copy`, `T.reduce`) into MMA/WGMMA/cp.async
intrinsics with explicit shared-memory layout swizzles and software pipelining.

Three subsystems are directly relevant to thvm:

1. **Carver** (template-based config space generator + roofline ranker) is the
   cleanest answer to "where do candidate tile sizes come from?" It maps naturally
   onto our [src/codegen/propose.c](../src/codegen/propose.c) and could replace its
   hardcoded constant lists with shape/arch-aware enumeration.
2. **Layout inference + swizzle templates** are the right shape for what we'd need
   if we ever target Metal threadgroup memory beyond the small fixed tile set in
   [src/backend/metal/shaders/](../src/backend/metal/shaders/). Until then, the swizzle
   constants are useful as a reference, not as code to port.
3. **AutoTuner caching/keying** (source-hash + arg-shape + pass-config) is a clean
   pattern we should adopt verbatim for [src/codegen/autotune.c](../src/codegen/autotune.c)
   when we eventually persist autotuned configs to disk.

What we should NOT port: TVM TIR, the 60-pass pipeline, `@T.prim_func`, the
producer-consumer warp-specialization pass, anything Hopper/Blackwell-specific. thvm
is pre-Hopper-feature-set; the value is in the algorithms and partitioning, not the
code.

## 1. Pipeline at a glance

tilelang Python source flows:

```
  @tilelang.jit / @T.prim_func
        |
        v
  TVM TIR PrimFunc (a thin Python -> TIR layer)
        |
        v
  PreLowerSemanticCheck
  LowerAndLegalize       (~40 passes; layout inference, pipeline planning,
                          tile-op lowering, intrinsic selection, sync injection)
  OptimizeForTarget      (target-specific finalization)
        |
        v
  codegen_cuda / codegen_hip / codegen_c / codegen_metal / codegen_cutedsl
        |
        v
  JITKernel + an Adapter (TVMFFI, NVRTC, Metal, Cython, Torch, CuTeDSL)
```

Pipeline orchestration lives in `tilelang/engine/phase.py`. The bulk of the C++ in
`src/transform/`, `src/op/`, `src/layout/`, `src/target/` is callable from Python
through TVM's FFI.

thvm's analogue is the `realize -> classify -> rangeify -> tile_build -> codegen`
chain in [src/schedule/](../src/schedule/) and [src/codegen/](../src/codegen/), and
the classification/lowering plan documented in
[docs/plans/scalar_uops_lowering.md](plans/scalar_uops_lowering.md) and
[docs/plans/tile_uops.md](plans/tile_uops.md). The rough correspondence:

| tilelang stage | thvm analogue |
| --- | --- |
| Python `@T.prim_func` | UOp DAG built from WL via [src/uop/](../src/uop/) and TUOp WL pattern lib |
| `LowerAndLegalize` | `realize_classify` -> `rangeify` -> `tile_build_from_scalar` |
| `LowerTileOp` (intrinsic selection) | [src/codegen/render_metal.c](../src/codegen/render_metal.c) tile-MMA path |
| `LayoutInference` | (no analogue; layouts implicit in Metal shader templates) |
| `InjectSoftwarePipeline` | (no analogue) |
| `codegen_cuda/hip/metal/c` | [src/codegen/render_c.c](../src/codegen/render_c.c), [src/codegen/render_metal.c](../src/codegen/render_metal.c) |
| `JITKernel` + adapters | [src/jit/capture.c](../src/jit/capture.c) + backend dispatch tables |
| `AutoTuner` | [src/codegen/autotune.c](../src/codegen/autotune.c) + `kernel_opts_propose` |
| `carver` | the constant lists in [src/codegen/propose.c](../src/codegen/propose.c) |

## 2. Language layer

The DSL exposes a small fixed vocabulary, all imported as `T.*`. Worth scanning since
it's a compact taxonomy of "what a tile-kernel needs":

Allocation: `T.alloc_shared`, `T.alloc_fragment`, `T.alloc_local`, `T.alloc_barrier`,
`T.alloc_wgmma_desc`, `T.alloc_tcgen05_instr_desc` (last two are Hopper/Blackwell).

Control flow: `T.Kernel(grid_x, grid_y, threads=N)`, `T.Pipelined(extent, num_stages=K)`,
`T.Parallel(extent_m, extent_n)`, `T.serial`, `T.unroll`, `T.vectorized`.

Tile operations: `T.copy`, `T.gemm`, `T.reduce`/`T.reduce_sum`/`T.reduce_max`,
`T.fill`, `T.clear`, `T.gemm_sp` (2:4 sparse).

Annotations: `T.annotate_layout(buf, layout)`, `T.use_swizzle(panel_size=10)`,
`T.annotate_restrict_buffers([..])`, `T.annotate_min_blocks_per_sm(N)`.

Hardware intrinsics: `T.wgmma_gemm`, `T.tcgen05_gemm`, `T.ldg32/64/128/256`,
`T.stg32/64/128/256`, `T.atomic_add`, `T.atomic_max`.

Files: `tilelang/language/{__init__.py, allocate.py, gemm_op.py, copy_op.py, loop.py, kernel.py}`.

For thvm: the *semantic* split of "alloc / kernel / loop / tile-op / annotation"
maps onto our scalar-UOp + tile-UOp split fairly well. `T.Pipelined(num_stages=K)`
has no analogue and shouldn't until we have a real reason; `T.Parallel` corresponds
roughly to `KAX_LOOP` + `KAX_LOCAL`/`KAX_GLOBAL` in [src/thvm.h:474+](../src/thvm.h);
`T.alloc_shared` corresponds to `TILE_LOCAL_ALLOC` in
[src/schedule/tile.c](../src/schedule/tile.c).

The annotation set is the cleanest part to steal: a few opt-in hints
(`use_swizzle`, `annotate_min_blocks_per_sm`) that the compiler can use or ignore.
We have nothing like this and our autotuner has to discover everything from
scratch. A minimal `KOptHint` channel attached to a UOp would let the user say
"prefer LOCAL=128 here" and have propose.c short-circuit.

## 3. IR passes worth understanding

There are ~60 passes; only a dozen carry ideas worth porting.

**LayoutInference** (`src/transform/layout_inference.cc`, ~51K). Walks the IR after
PipelinePlanning, looks at how each shared-memory buffer is read/written by the
neighbouring tile ops, and assigns a swizzle pattern that avoids bank conflicts and
matches MMA fragment expectations. Works because layouts are decoupled from access
expressions: a buffer has a layout and access is `buf[i,j]`, and the layout decides
what linear address that becomes.

What we'd buy: ability to swap `Metal threadgroup` storage layouts (interleaved,
swizzled, padded) on a kernel-by-kernel basis without rewriting shader code.
Currently we have one fixed layout per shader.

What it costs: a layout type-system. Worth doing only if we hit Metal bank-conflict
hotspots, which we haven't measured.

**PipelinePlanning** (`src/transform/pipeline_planning.cc`, ~84K) +
**InjectSoftwarePipeline** (`src/transform/inject_pipeline.cc`, ~134K). The first
analyzes producer-consumer chains in a `T.Pipelined` loop and computes how many
buffer versions ("stages") you need to overlap loads with compute; the second
inserts the prologue/epilogue and multi-versioned buffers.

Not portable: this is GPU-async-copy machinery (cp.async, TMA). On Apple GPUs and
CPU we don't have the underlying primitives.

**LowerTileOp** (`src/transform/lower_tile_op.cc`, ~56K). The dispatcher: takes a
`tl.gemm` Call node plus its target/dtype/shape and selects an intrinsic family
(wmma, mma, wgmma, tcgen05, mfma) to lower to. This is the closest analogue to
[src/codegen/render_metal.c:render_metal_gemm](../src/codegen/render_metal.c)
selecting between simdgroup MMA shader variants.

Worth porting: the *table* of "(target, dtype, M, N, K) -> intrinsic + tile shape".
We currently grow this ad hoc. A small lookup table would clean up
`propose.c` and `render_metal.c` together.

**MergeSharedMemoryAllocations**. Liveness-based reuse of shared-memory regions
across non-overlapping tile ops. We do similar work in
[src/schedule/materialize.c](../src/schedule/materialize.c) for global buffers but
don't yet do it for Metal threadgroup memory (we don't have many threadgroup
allocations to merge today; revisit once we do).

**LayoutReducer** + **InstructionAnnotation**. Tag each tile op with the chosen
intrinsic kind (`mma`, `tma`, `wgmma`, `cp_async`, `sync`), so later passes can
specialise without re-discovering. Cheap and clean; if we ever add more than two
or three Metal MMA variants, we should adopt this.

**ProducerConsumerWarpSpecialized**. Splits a threadblock into "producer warps"
(loads only) and "consumer warps" (compute only) with an mbarrier between them.
Hopper-specific; nothing for us today.

**Simplify / NarrowDataType / ConfigIndexBitwidth**. Standard expression
simplification, int64->int32 narrowing, common-subexpression elimination. We get
this from C compiler optimization today. No reason to port.

## 4. Tile-op codegen and intrinsics

`tilelang/tileop/gemm/inst.py` defines per-intrinsic descriptors:

- `WMMA` (SM70-89): warp-level 16x16x16 only.
- `MMA` (SM70-80): tensor-core 16x16x16 / 8x16x16, with sparse variants.
- `WGMMA` (SM90+): warp-group async MMA; dynamic shape.
- `TCGEN05` (SM100+): dual-SM async; dynamic shape.
- `MFMA` (CDNA): AMD matrix-fabric instructions, multiple block shapes.

The macro generators in `tilelang/intrinsics/` emit inline-PTX or HIP fragments
for each. The runtime helpers live in `src/tl_templates/cuda/{mma.h, wgmma.h,
tcgen_05.h, gemm_smXX.h, copy.h, copy_smXX.h, reduce.h, barrier.h, cluster.h}`
and `src/tl_templates/hip/`.

For Metal there is no equivalent yet in tilelang; their Metal support stops at
basic SIMT codegen via `src/target/codegen_metal*` (the file exists but is small).
Apple's `simdgroup_matrix` MMA isn't wired up in tilelang at all. So we get no
free lunch on the Metal MMA front.

The *sparse* path (`gemm_sp.cc` + `mma_sp_macro_generator.py`) is interesting if
we ever target 2:4 weights but not before then.

## 5. Layout system

`tilelang/layout/` defines:

- `T.Layout` (shape + stride + swizzle + dtype).
- `Fragment` (how a warp's registers hold a tile).

Swizzle factories in `tilelang/layout/swizzle.py`:

- `make_swizzled_layout`, `make_volta_swizzled_layout` (V100).
- `make_wgmma_swizzled_layout` (H100), `make_tcgen05mma_swizzled_layout` (B100).
- `make_full_bank_swizzled_layout`, `make_half_bank_swizzled_layout`,
  `make_quarter_bank_swizzled_layout` (shared-memory bank conflict avoidance).
- `make_linear_layout` (no swizzle).

Fragment factories: `make_gemm_fragment_8x8`, `make_gemm_fragment_8x8_transposed`,
`make_fully_replicated_layout_fragment`.

The C++ engine is `src/layout/{layout.cc, gemm_layouts.cc, tcgen05_layout.cc}`.

This is a clean, self-contained subsystem and the source is short. If we hit bank
conflicts in Metal threadgroup memory (we haven't profiled for this), the
constants are reusable; the dispatching machinery is too tied to TVM TIR to lift.

## 6. Backends and runtime templates

Targets: SM70/75/80/86/89/90/99/100, HIP CDNA, Metal, CPU/LLVM. The CuTeDSL backend
is new and emits CUTLASS CuTE templates rather than raw PTX.

Codegen files (sizes give a rough picture of complexity):

- `src/target/codegen_cuda.cc` ~193K
- `src/target/ptx.cc` ~65K (PTX intrinsic emission, inline-asm templates)
- `src/target/codegen_hip.cc` ~67K
- `src/target/codegen_cutedsl.cc` ~100K
- `src/target/codegen_c.cc` ~16K (CPU)
- `src/target/codegen_py.cc` ~23K (debug)

Runtime templates in `src/tl_templates/` are header-only C++ helpers per arch. The
CUDA tree has `gemm_smXX.h` for X in 70..100, plus per-kind copy/reduce/barrier.
The HIP tree mirrors this. The CPU tree has SIMD helpers. The Metal tree is
notably absent.

For thvm the interesting design point is *runtime templates as a portability
boundary*. tilelang's codegen knows targets only at the level of "emit a call to
`tl::gemm_sm80<M,N,K>(A, B, C)`"; the actual schedule lives in the header. We
already do this for Metal: the renderer in [src/codegen/render_metal.c](../src/codegen/render_metal.c)
selects among offline-compiled shader variants, and the shader source in
[src/backend/metal/shaders/](../src/backend/metal/shaders/) is the runtime-template
analogue. Worth keeping that boundary clean as we add tile shapes.

## 7. Autotuner

Two front doors:

- Decorator: `@tilelang.autotune(configs=fn, warmup=25, rep=100, timeout=60)` on top of `@tilelang.jit`.
- Class: `AutoTuner.from_kernel(kernel_factory, configs).run() -> AutotuneResult`.

`configs` is callable; given problem-size args it returns a list of dicts of
config knobs. Both forms compile-then-benchmark each config and remember the
winner.

Architecture (in `tilelang/autotuner/{tuner.py, param.py, capture.py}`):

- `JITImpl.par_compile(configs, num_workers=4)` compiles candidates in a thread
  pool, with each worker setting its own CUDA device.
- Worker count via `TILELANG_AUTO_TUNING_CPU_COUNTS` (default 90% of cores).
- Timeouts via POSIX signals on Unix; Windows fallback.
- `Profiler.do_bench(warmup_ms, rep_ms, quantiles)` runs N reps with L2 flush
  between them.

Caching (worth porting verbatim):

- Disk: `$TILELANG_CACHE_DIR/autotuner/` (default `~/.tilelang/cache/autotuner`).
- Key includes: tilelang version, function source, free vars of closure, config
  list, compile args, profile args. Hash collision means recompile.
- Stored: `best_config.json`, `latency.json`, kernel sources, compiled `.so` /
  `.cubin`.
- Disable via `TILELANG_AUTO_TUNING_DISABLE_CACHE=1`.

Correctness: per-config check against a reference program; defaults to torch
matmul/etc. for known shapes. Tolerance `rtol=atol=1e-2` by default.

Tensor supply: builtin distributions (`Auto`, `Uniform`, `Normal`, `Randn`,
`Zero`, `One`, `Integer`) or custom callable; supports a `set_autotune_inputs`
context manager so a user can pin specific tensors.

This is the closest match to thvm's [src/codegen/autotune.c](../src/codegen/autotune.c)
plus [src/codegen/propose.c](../src/codegen/propose.c). thvm's current autotune
is in-process and in-memory (`KpCacheSlot` retains the autotuned variant per
program-shape), with no on-disk persistence. Porting tilelang's caching key
recipe is straightforward and high value: it would let users (and
[src/jit/capture.c](../src/jit/capture.c) replay) survive process restarts and
share autotuned configs across runs.

## 8. Carver: the config space generator

This is the part of tilelang most worth porting in spirit, even if not in code.

`tilelang/carver/` is a shape- and arch-aware *template* engine that, given a
problem (matmul of MxN @ NxK fp16, on RTX-4090) and a hardware model (SM count,
shared-memory cap, warp size, cache levels), enumerates plausible tile/warp/stage
configurations and ranks them by a roofline estimate.

Templates:

- `MatmulTemplate` (matmul + dequant variants).
- `GEMVTemplate` (matrix-vector).
- `GeneralReductionTemplate` (SSR loop nests).
- `ElementwiseTemplate`.
- `FlashAttentionTemplate`.

Architectures:

- `CUDA("nvidia/geforce-rtx-4090")`, `CUDA("nvidia/h100-pcie")`, etc.
- `CDNA("amd/mi300x")`.
- `Metal` (basic; just shared-mem and warp size).

Output of `recommend_hints(problem, arch, top_k=5)` for matmul looks like:

```
{
  block:        [32, 64],          # threadblock M, N
  warp:         [16, 32],          # warp tile (M, N)
  rstep:        [128],             # K reduction step
  use_tc:       True,              # use tensor cores
  vectorize:    {A: 8, B: 8},      # load vector width
  num_stages:   3,                 # software-pipeline depth
  num_threads:  256,
  ...
}
```

The roller (`tilelang/carver/roller/`) is the search/ranking component; the
analysis (`tilelang/carver/matmul_analysis.py`) computes arithmetic intensity and
checks against arch limits.

This maps onto thvm's `kernel_opts_propose()` in
[src/codegen/propose.c:226-324](../src/codegen/propose.c). Today that function:

- Detects MUL+REDUCE_SUM matmul patterns and proposes `KOP_TC` with tile sizes from a hardcoded list `{32, 16, 8}`.
- Proposes `KOP_GROUP` for scalar tile reduces with full axis or `{16,8,4,2}`.
- Proposes `KOP_UNROLL`/`KOP_LOCAL`/`KOP_UPCAST` from similar fixed lists.

A carver-style port would look like:

1. A shape-and-arch-aware enumerator that respects: threadgroup memory cap,
   max threads per group, register budget per thread, vector load width.
2. Per-pattern templates: gemm, gemv, reduce, elementwise, conv. Each template
   takes the matched shape and emits a list of candidates.
3. A simple roofline scorer to prune candidates to top-K before benchmarking;
   this is how carver keeps the search short.

The arch model itself is small: for Metal, we'd need
`{max_threads_per_threadgroup, threadgroup_mem_bytes, simd_width=32,
register_budget_per_thread}` plus a few dispatch costs. For CPU, even less.

This is the highest-leverage idea in the report.

## 9. JIT and cache

`tilelang.jit(kernel_fn)` returns a `JITKernel` which wraps a compiled PrimFunc.
Two compile modes:

- Lazy: user returns `@T.prim_func` from the jit-decorated factory; first call
  with shape args builds and caches.
- Eager: user annotates inputs with tensor types; `kernel(A, B, C)` returns the
  output directly.

Backends (adapters in `tilelang/jit/adapter/`):

- `TVMFFIKernelAdapter` (default, low overhead via TVM C++ FFI).
- `CythonKernelAdapter` (legacy).
- `NVRTCKernelAdapter` (compile PTX to cubin at runtime).
- `CuTeDSLKernelAdapter` (new CUTLASS CuTE backend).
- `MetalKernelAdapter` (MetalKit shader compilation).
- `TorchKernelAdapter` (Torch tensors via DLPack).

Cache (`tilelang/cache/kernel_cache.py`):

- In-memory `KernelCache` keyed by source-hash + input dtypes + shape constants.
- Disk cache layered behind it.
- Symbolic shapes via `T.const('M N K')` get propagated through the cache key
  as variables; runtime check confirms.

For thvm, the cache structure to mirror is:

```
$XDG_CACHE_HOME/thvm/autotune/
   <arch>/
       <prog-shape-hash>/
           best_opt.json
           timing.json
           kernel.metal / kernel.so
```

with a key recipe of: thvm git hash + program opcode sequence + tensor shape +
dtype + backend + propose.c version. We do not need the source-hash dimension
(our programs are derived, not user-written).

## 10. Examples worth knowing exist

`tilelang/examples/`:

- `gemm/` (basic matmul with pipelining + layout opt).
- `dequantize_gemm/` (INT4/INT8 weight + fp16 activation, dequant-fused).
- `flash_attention/` (block-wise tiling, no full-softmax materialization).
- `linear_attention/` (RetNet, Mamba variants).
- `deepseek_mla/` (Multi-Head Latent Attention decode).
- `deepseek_nsa/` (Native Sparse Attention).
- `convolution/` (im2col-based).
- `blocksparse_attention/`.
- `gemm_sp/` (2:4 sparse).
- `grouped_gemm/`.
- `warp_specialize/` (producer-consumer warp split).
- `attention_sink/` (long-context streaming).
- `plot_layout/` (layout visualization helpers; could be useful for debugging
  Metal threadgroup layouts).

The `plot_layout/` utility is the only example that would directly help us today.
The rest are useful as reference kernels if/when we tackle attention.

## 11. CPU and Metal specifics

CPU: `codegen_c.cc` + `tl_templates/cpu/` produce nested loops + SIMD intrinsics
(SSE/AVX/AVX2/AVX512), with OpenMP for parallel tile loops. TVM's
`ThreadAllreduce` pass handles cross-thread reductions. We don't need any of
this; our CPU backend is already a tight scalar interpreter and the C-renderer
in [src/codegen/render_c.c](../src/codegen/render_c.c) handles the JIT path.

Metal: bare-bones in tilelang.

- Arch model: `tilelang/carver/arch/metal.py` (just shared-mem/warp size).
- Adapter: `tilelang/jit/adapter/torch/metal.py`.
- Pass: `tilelang/transform/metal/mark_host_metal_context.py`.
- Codegen: emits MSL via TVM's existing Metal codegen, with no MMA / tensor-core
  story (Apple's `simdgroup_matrix` isn't wired up).
- No `tl_templates/metal/` directory (CUDA and HIP each have one).

So tilelang's Metal support is *behind* thvm's. We have specialized Metal conv,
gemv, and tile-reduction kernels (recent commits 4956913, a7f6b07, 05e8d89);
tilelang has SIMT codegen and that's it. Don't lift Metal from tilelang; if we
copy anything Metal-shaped, copy the autotuner harness and carver shape, point
them at our existing shaders.

## 12. What to port, in priority order

These are ranked by leverage-per-effort given thvm's current state.

**Tier 1 (do first, low risk, high payoff):**

1. **AutoTuner cache key + on-disk persistence** for
   [src/codegen/autotune.c](../src/codegen/autotune.c). Recipe: SHA256 over
   `(thvm git hash, backend, program opcode sequence, input/output shapes/dtypes,
   propose.c version)`. Storage: `$XDG_CACHE_HOME/thvm/autotune/<key>.json`
   holding the winning `KOpt` set + benchmark result. Survives process restarts;
   makes [src/jit/capture.c](../src/jit/capture.c) replay durable.
2. **Carver-shaped propose.c**: factor the hardcoded constant lists into
   per-pattern templates parameterized by an arch model. Start with two
   templates (matmul, reduce) and a Metal arch struct (`max_tg_threads`,
   `tg_mem_bytes`, `simd_width=32`). Emit candidate lists from arch +
   shape rather than from constants. Plumb through the same `KOpt` interface so
   `kernel_autotune()` doesn't change.

**Tier 2 (do once we have a real reason):**

3. **Optimization-hint annotations on UOps**: a single optional `KOptHint` field
   the user can set ("prefer LOCAL=128", "skip TC", "this is i8") that
   `propose.c` consults to skip enumeration. tilelang has this as
   `T.annotate_layout`/`T.use_swizzle`/`T.annotate_min_blocks_per_sm`; we'd want
   the WL DSL to expose it.
4. **Roofline-pruning before benchmarking**: even a crude estimator (peak
   bandwidth + peak FLOPs from arch) will cut benchmark time when the candidate
   list grows past ~10. Below that don't bother.
5. **Per-kernel reference-program correctness check** during autotune. tilelang
   does this cheaply by re-running the unoptimized kernel; we could do the same
   by keeping the baseline `KOpt=0` variant as the reference and comparing
   output buffers within tolerance. Reduces the risk of the autotuner picking
   a numerically-wrong fastest variant.

**Tier 3 (only if we hit a wall):**

6. **Layout/swizzle abstraction** for Metal threadgroup memory. Not warranted
   until we measure bank-conflict stalls; we have one fixed layout per shader
   today and that's fine.
7. **Warp-specialization / pipelined loops**. Apple GPUs don't expose the right
   primitives. Skip.
8. **CuTeDSL-style emission of CUTLASS templates**. Only if we ever add a CUDA
   backend, which is not on the roadmap.

**Don't port:**

- TVM TIR. We already have UOps/scalar-UOps/tile-UOps; another IR layer is pure
  cost. tilelang only has TIR because it's a TVM extension.
- `LowerHopperIntrin`, `LowerSharedTmem`, `InjectFenceProxy`,
  `FuseMBarrierArriveExpectTx`, `InjectTcgen05Fence`, `LowerBlackwell2SM`. These
  are Hopper/Blackwell features.
- `MergeIfStmt`, `LoopUnswitching`, `Simplify`, `NarrowDataType`. C compiler does
  this for us at codegen time.
- `MergeSharedMemoryAllocations`. We don't have enough threadgroup allocations
  for this to matter.

## 13. Concrete pointers

For the Tier 1 work, the relevant code on both sides:

| What | tilelang reference | thvm site |
| --- | --- | --- |
| Cache key recipe | `tilelang/autotuner/tuner.py` (`_make_cache_key`) | new helper in [src/codegen/autotune.c](../src/codegen/autotune.c) |
| Cache disk layout | `tilelang/cache/kernel_cache.py` | new dir under `$XDG_CACHE_HOME/thvm/autotune/` |
| Per-config compile worker | `JITImpl.par_compile` in `tilelang/jit/__init__.py` | extend `kernel_autotune` in [src/codegen/autotune.c:186-243](../src/codegen/autotune.c) |
| Profile harness | `tilelang/profiler/bench.py` (`do_bench`) | `kernel_bench_fire` in [src/codegen/autotune.c](../src/codegen/autotune.c) |
| Tensor supply | `tilelang/profiler/__init__.py` (`TensorSupplyType`) | seed input tensors in autotune harness |
| Carver matmul template | `tilelang/carver/template/matmul.py` | refactor TC branch in [src/codegen/propose.c](../src/codegen/propose.c) |
| Carver arch model | `tilelang/carver/arch/cuda.py`, `metal.py` | new `arch_metal_t` struct in [src/codegen/](../src/codegen/) |
| Roofline scorer | `tilelang/carver/matmul_analysis.py` | new `propose_score()` helper |

## 14. Open questions

These are decisions we'd need to make before writing any port:

- Cache invalidation: do we key on thvm git hash (cheap, conservative) or on a
  hash of [src/codegen/propose.c](../src/codegen/propose.c) + relevant codegen
  files (precise, more work)? Probably git hash for now; revisit when iteration
  on propose.c slows down.
- Where does the carver arch model live? Same file as `propose.c` is fine for v1;
  split out only when we add a second backend's template set.
- Do we need a config-list "callable" like tilelang's `configs(M, N, K)`? Today
  `propose.c` reads shape from the `KernelEntry`; the callable indirection only
  matters if user code wants to override. Defer until we see a need.
- WL surface: does `TGemm[A, B, opts]` accept hints (`"PreferLocal" -> 128`)? If
  yes, we need a stable `KOptHint` channel from WL through TUOp into the kernel.
  Defer.

## 15. Tilelang docs we read

For a deeper dive, these files in tilelang's own tree are the best starting points:

- `tilelang/docs/programming_guides/autotuning.md` (autotuner user guide)
- `tilelang/docs/programming_guides/language_basics.md` (DSL basics)
- `tilelang/docs/tutorials/auto_tuning.md` (concrete examples)
- `tilelang/tilelang/carver/README.md` (carver overview)
- `tilelang/examples/quickstart.py` (the simplest GEMM)

That's the map. Recommended first patch is the on-disk autotune cache (Tier 1
item 1); it's the cleanest win and unblocks measuring the value of the
carver-shaped refactor.
