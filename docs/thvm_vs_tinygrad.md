# thvm vs tinygrad: Comprehensive ML-Compiler Comparison, Knob Reference, and Coverage-Gap Plan

*Comparison of `thvm` (C port, branch `py-jit-speedup`, `/private/tmp/thvm-jit`) against `tinygrad` (the spec, `/Users/swish/src/tinygrad/tinygrad`). All file:line citations are against those trees. Benchmark figures are warm Metal `beautiful_mnist` train unless noted; the CUDA picture is qualitatively identical.*

---

## 1. Executive summary

`thvm` is a faithful C port of tinygrad's UOP-DAG-to-kernel compiler: it reproduces the same pipeline shape (UOP DAG -> realize-seed classify -> consumer-driven rangeify -> materialize -> render -> backend dispatch), the same consumer-divergence fusion model, and a near-line-by-line port of tinygrad's hand-coded optimization heuristic (`hand_coded_optimizations` -> `src/codegen/hand_opts.c`). The two diverge in three places that matter. **(1) Realize-seeding:** tinygrad seeds *only* structural boundaries (`COPY`/`CONTIGUOUS`/`STORE`, `indexing.py:28-35`) and derives every other realize from the rangeify walk; thvm's *default* mode over-seeds (`ROOT + MULTI + REDUCE + MATMUL + FANIN_CAP` in `bufferize_classify.c`), producing more, smaller kernels, while its opt-in **faithful** mode (`THVM_RU_FAITHFUL_SEED=1`, `rangeify_unified.c:147-166`) seeds `ROOT` only to match tinygrad's structural seed. **(2) Optimization search:** tinygrad pairs the hand-coded heuristic with a full BEAM autotuner (`codegen/opt/search.py`, ~200 actions/kernel); thvm has *no BEAM* and runs the heuristic exactly once per kernel shape. **(3) Symbolic depth:** thvm's movement-op/ShapeTracker handling and validity-merge are hand-coded and conservative where tinygrad uses full symbolic graph rewriting. The measured tradeoff is the report's headline: **thvm default 3.63 ms / 328 kernels / 31 MB**, **thvm faithful 4.72 ms / 226 kernels / 14.8 MB**, **tinygrad 6.03 ms / 120 kernels / ~1-3 MB**. thvm wins wall-time (its over-realization plus a thvm-specific occupancy floor maximizes per-kernel parallelism); tinygrad wins fusion and memory (BEAM + symbolic merge + structural seed). Faithful mode is the middle point: it closes roughly half the kernel-count gap (328 -> 226 vs tinygrad's 120) and most of the memory gap (31 -> 14.8 MB) for a ~1.3x wall-time cost. The remaining 226-vs-120 gap is **algorithmic**, not a seed-mode artifact: it lives in reduce-into-reduce fusion, ShapeTracker merge, and arange/late-rewrite collapses that thvm has not yet ported.

---

## 2. Pipeline architectures, side by side

Both compilers lower a tensor program to device kernels through structurally identical stages. thvm splits tinygrad's single `run_rangeify` into an explicit `bufferize_classify` pre-pass plus a `rangeify_unified` walk (the latter being the 1:1 port), then a separate `materialize` stage.

### thvm stages

1. **UOP DAG construction** (`src/uop/*.c`) - immutable, heap-consed (`TAG_UOP`) so identical subtrees dedup. Opcodes: data (`UOP_LOAD`/`STORE`/`CONST`/`CAST`/`BITCAST`), compute (`ADD`/`MUL`/`NEG`/`DIV`/`REDUCE`), movement (`RESHAPE`/`PERMUTE`/`EXPAND`/`PAD`/`SHRINK`/`FLIP`), special (`RANGE`/`BUFFER`/`OPT`). Early `graph_simplify.c` constant-folds and applies algebraic identities.
2. **Bufferize classify / realize-seed** (`src/schedule/bufferize_classify.c`) - marks boundary nodes that must materialize, builds `BUFFERIZE_NODES[]` (per-node UOpInfo: opcode, consumer count, reasons bitmask, realized flag) and `CMAP_LL[]` (linked-list consumer map). This is the seed-mode decision point.
3. **Rangeify unified** (`src/schedule/rangeify_unified.c`) - the port of `run_rangeify`. Kahn-order reverse-topo walk (`rangeify_unified.c:591-647`) assigning `RU_RANGE_MAP` (per-node in/out range tuples), `RU_REALIZE_MAP` (full/partial realize flags), `RU_ENDING_RANGES` (axes closed by downstream EXPANDs), `RU_REDUCE_RANGES` (injected reduce axes). Emits `UOP_BUFFERIZE` + INDEX at boundaries.
4. **Materialize** (`src/schedule/materialize.c` + `kernel_lift.c`) - one `KernelEntry` per boundary; `visit()` inlines non-realized upstream UOPs or binds cross-realize buffers; `kernel_lift_to_uop` packages a standalone `UOP_STORE` (`kernel_lift.c:52-88`); cross-realize dedup via `MATERIALIZED_LOC_CACHE`; strand-realize gate for `THVM_FUSE_CONV_BWD`.
5. **Render** (`src/codegen/render_uop.c` + `hand_opts.c` + `render_metal.c`/`render_ptx.c`) - hand-opts mutate the axis schedule, then a shared bottom-up walk emits MSL / PTX / C99.
6. **Backend dispatch + JIT** (`src/backend/{cpu,cuda,metal}/*.c`, `src/jit/capture.c`) - record/replay with buffer pinning and cross-realize span dedup.

### tinygrad stages

1. **UOp DAG / capture** (`engine/jit.py:TinyJit`).
2. **Realize-map generation** (`schedule/indexing.py:28-35` `pm_generate_realize_map`) - structural seed only.
3. **run_rangeify** (`schedule/indexing.py:148-269`) - reverse-topo walk; range_map + realize_map; `pm_apply_rangeify` inserts BUFFERIZE/INDEX.
4. **Kernelization** (`schedule/rangeify.py:get_kernel_graph`) - movement-op folding, `pm_remove_bufferize` (inline buffers when cheap), `pm_add_buffers` (BUFFERIZE -> BUFFER+STORE), `split_kernels` (STORE = kernel boundary), WAR `AFTER` injection.
5. **Optimization** (`codegen/opt/heuristic.py` + `search.py`) - hand-coded heuristic, then optional BEAM.
6. **Lowering** (`codegen/__init__.py:full_rewrite_to_sink`) - expander, group-for-reduce, devectorizer, gpudims, decompositions, renderer rewrite.
7. **Codegen + JIT** (`Device.compiler`, `engine/jit.py:jit_lower`, `GraphRunner`) - memory plan, graph batching.

### Stage-to-stage map

| tinygrad stage | thvm stage | Fidelity |
|---|---|---|
| UOp DAG construction | `src/uop/*.c` | Faithful (immutable, hash-consed) |
| `pm_generate_realize_map` (structural seed) | `bufferize_classify.c` (default: heuristic seed) / `ru_seed_boundary_holds` faithful (`rangeify_unified.c:163`) | **Simplified by default; faithful matches structural seed** |
| `run_rangeify` reverse-topo walk | `rangeify_unified.c:733-999` | Faithful walk; conservative on movement/validity merge |
| `pm_apply_rangeify` rewrite | `rangeify_unified.c:1001+` (BUFFERIZE/INDEX emit) | Faithful |
| `get_kernel_graph` / `pm_remove_bufferize` / `split_kernels` | `materialize.c` + `kernel_lift.c` | Faithful intent; thvm uses explicit strand checks where tinygrad uses symbolic coverage proofs |
| `hand_coded_optimizations` | `hand_opts.c` | Near line-by-line port; 3 sections skipped |
| BEAM `search.py` | **(none)** | **Missing** |
| Expander / devectorizer (`late/*.py`) | `uop/expander.c`, `devectorize.c` | **Ported but not wired into production render (test-only)** |
| Renderers (llvmir/cstyle/ptx/metal) | `render_uop.c` + backend renderers | Faithful (CPU/CUDA/Metal only) |
| `memory_plan_rewrite` | `THVM_ARENA_PLAN` (TLSF arena) | Partial; no reuse-distance layout opt |
| Graph batching (`graph_split_rewrite`) | `THVM_METAL_GRAPH_REPLAY` / `THVM_CUDA_JIT_GRAPH` + `jit/capture.c` | Partial |
| Gradient (`gradient.py`, post-kernelize UOP) | `interact/uop_grad.c` (term/AST level) | Different layer; fewer backward rules |

**Faithful-vs-simplified summary:** the rangeify walk, INDEX/BUFFERIZE rewrite, hand-coded heuristic core, and renderers are faithful. The *default* realize seed is intentionally over-conservative (a perf choice, not a porting gap). Genuinely simplified: movement-op/ShapeTracker merge, validity-mask union, ending-ranges PCONTIG, reduce-into-reduce fusion. Genuinely missing: BEAM, ImageDType, mask-UPCAST, THREAD, late arange/reduce-collapse rewrites, wired expander/devectorizer.

---

## 3. Scheduling & fusion: the core algorithmic comparison

This is where the 120-vs-226-vs-328 kernel-count gap is born.

### 3.1 Realize-boundary seeding

| Aspect | tinygrad | thvm default | thvm faithful (`THVM_RU_FAITHFUL_SEED=1`) |
|---|---|---|---|
| Structural seed | `COPY`, `CONTIGUOUS`, `STORE` only (`indexing.py:29`) | `ROOT` + `MULTI` + `REDUCE` + `MATMUL` + `FANIN_CAP` | `ROOT` only (`rangeify_unified.c:164`) |
| Derivation | divergence walk derives all multi-consumer/reduce boundaries | pre-seeded, walk sees them already realized | divergence + ending-ranges walk derives the rest |
| `ALWAYS_CONTIGUOUS` never-realize set | `CONTIGUOUS, AFTER, COPY, BUFFER, CONST, BIND, DEVICE, MSELECT, MSTACK, PARAM` (`indexing.py:10-11`) | no `BIND`; const/buffer handled ad hoc | same |
| Kernels (beautiful_mnist) | ~120 | 328 | 226 |
| Peak memory | ~1-3 MB | 31 MB | 14.8 MB |

The key code is `ru_seed_boundary_holds` (`rangeify_unified.c:163-166`): in default it returns `1` for every realized node; in faithful it returns true only for `BUFFERIZE_REASON_ROOT`. The header comment there records that a tried "drop-MULTI-only middle ground was removed: it kept the REDUCE boundaries thvm's codegen needs (so it was correct) but cut only ~10 kernels with no speed change - **the real granularity win is in fusing REDUCEs, which is the codegen gap**." That single comment localizes the residual 226-vs-120 gap: thvm's codegen emits one REDUCE per kernel, so faithful seeding cannot fuse the reduce chains tinygrad fuses.

### 3.2 Movement ops & ShapeTracker

- **tinygrad** (`indexing.py:129-145`, `apply_movement_op`): swizzles *ranges* symbolically post-hoc; movement ops never force realization. Consecutive RESHAPE/SHRINK/PERMUTE views are symbolically simplified/merged (graph_rewrite + `pm_simplify_valid`) *before* divmod-decompose, so a split+resplit cancels back to the source iterator.
- **thvm** (`rangeify_unified.c:835-837`): special-cases movement ops in the multi-consumer branch - if `!all_all_same && is_movement`, force `all_all_same=1` (treat as VIEW, recompute per consumer rather than materialize). This reproduces tinygrad's *intent* (no 1.3 GB broadcast materialization) via different mechanics. But thvm's `ru_compose_view_chain` (`rangeify_unified.c:1255-1337`) decomposes each `prior_views` step independently; it lacks tinygrad's cross-step symbolic merge, so split+resplit chains (unfold + col2im in conv-backward) don't cancel. `THVM_FUSE_CONV_BWD` routes RESHAPE through `apply_movement_op_reshape_composed` (a placeholder round-trip, `rangeify_unified.c:480-487`) as a hand-built partial substitute that lets IDIV/IMOD recombine instead of leaking independent decode axes into the cross-product.

### 3.3 Reduce grouping & multi-reduce

- **tinygrad**: reduces are always realized or surface via divergence; *no explicit multi-reduce fusion* - chained reduces (softmax EXP -> SUM -> DIV) fuse implicitly because single-consumer inputs inline into the reduce kernel.
- **thvm**: `REDUCE` seeded as `BUFFERIZE_REASON_REDUCE`. Named rule `bufferize_reduce_consumer_rule_drop_multi` (`bufferize_classify.c:728-822`) unmarks MULTI from reduces feeding a single chain (enables softmax fusion). `THVM_FUSE_REDUCE_INTO_REDUCE` / `THVM_BUFFERIZE_REDUCE_FUSE_MULTI` are opt-in gates for broadcast-reduce chains. But the one-reduce-per-kernel codegen rule means conv-backward emits separate weight-grad and data-grad kernels where tinygrad can keep them in one.

### 3.4 Ending ranges (EXPAND-triggered realization)

- **tinygrad** (`indexing.py:222-232`): an EXPAND marks its axis `ended`; a downstream REDUCE/elementwise with an ended range in a non-realized axis realizes *that axis only*, gated by a per-axis `PCONTIG` contiguity check (`indexing.py:227`).
- **thvm** (`rangeify_unified.c:864-888`): same trigger, but conservatively marks *all* non-realized axes as realized (no PCONTIG per-axis check), losing fusions where some axes were contiguous.

### 3.5 Recompute-vs-materialize decision

- **tinygrad**: implicit in the walk; `pm_remove_bufferize` later inlines buffers whose recompute cost is acceptable, relying on symbolic coverage proofs.
- **thvm**: explicit strand checks - `bufferize_value_would_strand` (`materialize.c:2465-2499`) and `strand_loop_product` (`materialize.c:2841-2865`) detect a RANGE axis that would be left unbound (a reduce loop iterating 0 or wrong times) and force a boundary. `THVM_FUSE_CONV_BWD`'s strand cap (`RU_STRAND_REALIZE_MAX_NUMEL`, ~4M elem / ~16 MB f32) lets large unfold products fuse as views while still catching genuinely stranded axes. These are correct but more conservative than tinygrad's symbolic proof, so they gate some legal fusions.

### 3.6 Multi-consumer validity union

- **tinygrad** (`indexing.py:211-213`): merges per-axis validity masks as a boolean OR (`minimum_valid`) so divergent consumers with different boundary guards still fuse.
- **thvm** (`rangeify_unified.c:838-862`): when axes don't all agree it falls back to `consumer[0]`'s ranges - no boolean-OR of valids (thvm's term algebra lacks the boolean-merge ops), losing those fusions.

### 3.7 Worked examples

**Softmax (EXP -> SUM -> DIV).** tinygrad: SUM seeded, EXP single-consumer inlines, DIV single-consumer inlines -> one kernel. thvm default: SUM seeded realized; `drop_multi` must fire for EXP to inline; often 2+ kernels. thvm faithful: only ROOT seeded, no divergence in the chain -> all fused (matches tinygrad).

**Conv backward (broadcast unfold MUL feeding weight-grad + data-grad reduces).** tinygrad: MUL is 2-consumer with divergent reduce axes -> partial realize / per-consumer recompute, *no 1.3 GB intermediate*. thvm default: MUL seeded MULTI -> materializes the 1.3 GB tensor. thvm faithful alone: MUL is a *computation*, not a movement op, so the `is_movement` special case doesn't fire -> still partial-realizes. **`THVM_FUSE_CONV_BWD=1`** is required: it composes the RESHAPE round-trip so the decode axes recombine, dropping the strand product enough to fuse without explosion.

---

## 4. The optimization heuristic: coverage matrix

`hand_opts.c` is a near-line-by-line port of `heuristic.py`; its header enumerates ported/skipped sections.

| heuristic.py section (lines) | OptOp(s) | thvm status | Notes |
|---|---|---|---|
| Tensor Cores (27-46) | `TC`, `GLOBAL` | **PARTIAL** | Metal 8x8 simdgroup_matrix + CUDA WMMA; M/N UPCAST + N LOCAL ported; **no `TC_OPT>=1`** (multi-reduce / CAST'd buffers) or true `TC_SELECT` over multiple TC specs. GPU only; CPU routes through cBLAS. |
| IMAGE float4 (51-62) | `UPCAST` | **MISSING** | thvm has no `ImageDType` |
| MATVEC (65-82) | `GROUP`+`LOCAL`+`UPCAST` | **PORTED** | `hand_opt_match_matvec` (`hand_opts.c:241-281`); same `MV_*` defaults (4/8/4) |
| GROUPTOP early gate (84-90) | `GROUPTOP` | **PORTED + ENHANCED** | Selects *largest* divisible reduce axis (not just 0,1,2); load-bearing on CUDA (2.2x: 10.8 ms on vs 23.5 ms off); `THVM_GROUPTOP`, `THVM_GROUP_SZ` |
| Mask UPCAST (97-106) | `UPCAST` | **MISSING** | no `IWHERE`/`WHERE` mask-axis detection in DAG walk |
| Main UPCAST loop (108-134) | `UPCAST` | **PORTED + ENHANCED** | stride heuristic 1:1; thvm adds reduce-heavy **occupancy floor** (`THVM_UPCAST_REDUCE_MIN_GRID`, auto = SM_count x 80) - no tinygrad equivalent |
| UNROLL last reduce (138-150) | `UNROLL` | **PARTIAL** | split-by-4 only; **full-extent `UNROLL(last,0)` disabled** (renderer bug in `rmu_emit_store_reduce`: matcher assumes axis survives) |
| Easy UPCAST fallback (153-155) | `UPCAST` | **PORTED** | identical |
| LOCAL placement (159-176) | `LOCAL` | **PORTED** | identical ranking + factor picking; `THVM_LOCAL_INNER_FIRST` tuning knob (net within-noise) |
| THREAD (180-189) | `THREAD` | **MISSING** | no `KOP_THREAD`; CPU goes through cBLAS |
| NOLOCALS gate | `NOLOCALS` | **PORTED** | reads `NOLOCALS`; no-op when backend declines |

**OptOps enum coverage** (tinygrad `opt/__init__.py:6-8` vs thvm `thvm.h:645-657`): `TC` partial, `UPCAST` ported, `UNROLL` partial, `LOCAL` ported, `THREAD` **missing**, `GROUP` ported, `GROUPTOP` ported, `NOLOCALS` ported, `PADTO` ported-but-unused, `SWAP` ported. thvm-only markers (`KOP_GLOBAL`, `KOP_FAST_MATH`, `KOP_SIMD_REDUCE`, `KOP_VEC_LOAD`) exist; only `GLOBAL` is live (TC parallel promotion).

**BEAM (the fundamental gap).** tinygrad's `search.py` enumerates ~200 actions/kernel (UPCAST 6x8, UNROLL 3x5, LOCAL 7x6, GROUPTOP 8x3, GROUP 4x3, optional PADTO, TC specs, SWAP perms, THREAD 10x3), compiles + times each on device (3 runs, early-stop at 3x best), keeps a Pareto top-`amt` per level, iterates to convergence, and disk-caches by `(AST, device, amt)`. **thvm has none of this**: it applies a fixed heuristic order once (`TC -> MATVEC -> GROUPTOP -> UPCAST -> UNROLL -> fallback -> LOCAL`). There is a `BEAM`/`AUTOTUNE` env reader in `codegen/autotune.c` but no search harness behind it. This is the single biggest opt-coverage gap and the reason thvm cannot recover when its heuristic order is suboptimal for a shape.

---

## 5. Complete knob/lever reference

### 5.1 thvm knobs

Grouped by purpose. Defaults from source; "toggle" = any non-`0`/non-empty value enables.

**Scheduling / realize-seed**

| Name | Effect | Default | Mode it changes |
|---|---|---|---|
| `THVM_RU_FAITHFUL_SEED` | Seed `ROOT` only, derive rest (fuse more: 226 vs 328 kernels). Auto-enables realize-dedup. | 0 | Fusion granularity |
| `THVM_FUSE_CONV_BWD` | RESHAPE composed round-trip + strand cap; fuses conv-bwd unfold instead of materializing | 0 | Conv-backward fusion |
| `THVM_JIT_REALIZE_DEDUP` | Defer `materialized_loc` clear across a JIT span; `on \|\| ru_faithful_seed_on()` (`materialize.c:188-200`) | 0 (auto-on under faithful) | Cross-realize kernel reuse |

**Bufferize / classify**

| Name | Effect | Default |
|---|---|---|
| `THVM_BUFFERIZE_REDUCE_FUSE_MULTI` | Fuse multiple REDUCE ops vs single-reduce gate | on (multi) |
| `THVM_BUFFERIZE_KEEP_NONMATMUL_REDUCE` | Keep REDUCE realized when consumers are broadcast chains | on (keep) |
| `THVM_BUFFERIZE_SKIP_REDUCE_INTO_REDUCE_SEED` | Skip seeding reduce->reduce; let walk derive | off |
| `THVM_FUSE_REDUCE_INTO_REDUCE` | Experimental reduce-into-reduce fusion across layout-only ops | off |
| `THVM_BUFFERIZE_SKIP_SMALL_EXPAND` | Skip realizing small EXPAND nodes | on (skip) |
| `THVM_BUFFERIZE_SOFTMAX_REDUCE_TILE_CAP` | Tile-feasibility cap for softmax multi-reduce fusion | on |
| `THVM_REDUCE_UNMARK_CAP` | Cap on reduces unmarked (fused) per slot | unset |
| `THVM_BUFFERIZE_REMOVE_SCORE_LIFT_REDUCE_GATE` | Reduce-boundary lift scoring gate | off |
| `THVM_TILE` | Enable Metal conv2d tiling (autotune cache key) | unset |
| `THVM_METAL_FUSION_MAX_INPUTS` | Cap inputs to a fused Metal kernel | unset |

**Hand-coded optimization heuristic** (GPU only)

| Name | Effect | Default |
|---|---|---|
| `HAND_CODED_OPTS` / `THVM_HAND_CODED_OPTS` | Master toggle for UPCAST/UNROLL/LOCAL/GROUPTOP | 1 |
| `NOOPT` / `THVM_NOOPT` | Inverse-sense disable (wins over HAND_CODED_OPTS) | 0 |
| `THVM_GROUPTOP` | Gate GROUPTOP early pass | 1 |
| `THVM_GROUP_SZ` | Cooperative GROUP_REDUCE size | 16 |
| `THVM_UPCAST_CAP` | upcast_size ceiling (product of UPCAST/UNROLL extents) | 32 |
| `THVM_UPCAST_REDUCE_MIN_GRID` | Reduce-heavy occupancy floor (CUDA): >0 floor, 0 auto (SM x 80), <0 off | 0 (auto) |
| `THVM_LOCAL_CAP` | LOCAL block-size cap | 128 |
| `THVM_LOCAL_INNER_FIRST` | Reverse LOCAL axis ranking (non-expand first) | 0 |
| `THVM_LOOP_UNROLL_MAX` | Reduce-loop unroll threshold (`render_uop.c`) | unset (16) |
| `MV`, `MV_BLOCKSIZE`, `MV_THREADS_PER_ROW`, `MV_ROWS_PER_THREAD` | Matvec params | 1, 4, 8, 4 |

**Autotune (skeleton; search not implemented)**

| Name | Effect | Default |
|---|---|---|
| `BEAM` | Beam width; env-read only, no search harness | 0 |
| `AUTOTUNE` | Enable autotune (BEAM takes precedence) | off |
| `BEAM_RUNS` | Timing runs/candidate (cap 1000) | built-in |
| `AUTOTUNE_CACHE` / `AUTOTUNE_CACHE_DIR` / `AUTOTUNE_DISABLE` | Disk-cache control | on / `$XDG_CACHE_HOME/thvm/autotune` / off |

**JIT capture / replay**

| Name | Effect | Default |
|---|---|---|
| `THVM_JIT_REPLAY_DEDUP` | Dedup identical kernel dispatches in replay | on |
| `THVM_JIT_REPLAY_DEDUP_WINDOW` | Lookback window (0 = unbounded) | 0 |
| `THVM_JIT_REPLAY_NOSKIP` | Force every dispatch to run | off |
| `THVM_JIT_REPLAY_PACK` | Pack replay ops (inline inputs -> sidecar) | on |
| `THVM_JIT_STRIDED` | Strided-load sidecar slab in replay | off |
| `THVM_JIT_NO_PIN` | Disable buffer pinning during capture | off |
| `THVM_METAL_GRAPH_REPLAY` / `THVM_METAL_GRAPH_MAX_DISPATCHES` | Metal command-buffer graph replay + cap | off / unset |
| `THVM_PRECISE_FIRE_MEMO` | Precise fire memoization | off |

**Memory / arena / GC**

| Name | Effect | Default |
|---|---|---|
| `THVM_MAX_BUF_BYTES` | Per-buffer ceiling (0 = unlimited) | 1 GiB |
| `THVM_MAX_LIVE_BYTES` | Total live backend buffer ceiling | 8 GiB |
| `THVM_ARENA_PLAN` | TLSF arena buffer reuse | on |
| `THVM_REUSE_BUFS` / `THVM_METAL_REUSE_BUFS` / `THVM_CUDA_REUSE_BUFS` | Per-backend buffer reuse | on / backend / backend |
| `THVM_GC` / `THVM_GC_KB` / `THVM_KGC` / `THVM_HEAP_CELLS` | Heap GC / threshold / kernel GC / cell pool | on / auto / on / built-in |

**Backend / codegen**

| Name | Effect | Default |
|---|---|---|
| `DEV` | `cpu`\|`cuda`\|`metal` | system |
| `THVM_THREADS` / `AOT_THREADS` | Worker / AOT thread counts | auto / built-in |
| `THVM_CPU_UOP_WALK` / `THVM_CPU_JIT_WARMUP` | CPU interpreter vs clang-JIT; warmup | off / built-in |
| `THVM_CUDA_PTX` / `THVM_CUDA_PTX_MAX` | Direct PTX render vs nvrtc; cap | off / unset |
| `THVM_CUDA_FAST_MATH` | `--use_fast_math` | off |
| `THVM_CUDA_SYNC` | Synchronize after each kernel | on |
| `THVM_CUDA_JIT_GRAPH` / `THVM_CUDA_KE_CACHE` / `THVM_CUDA_DISK_CACHE` / `THVM_CUDA_CACHE_DIR` | CUDA graph cache / KE cache / disk cache / dir | off / off / off / computed |
| `THVM_CUDA_GPU_TIME` | CUDA-event timing | off |
| `CUDA_PATH` / `CUDA_HOME` | Toolchain location | env |

**Debug / trace** (no behavior change): `THVM_BUFFERIZE_DEBUG`, `DUMP_BUFFERIZE[_CANDIDATES]`, `DUMP_MATMUL_DETECT`, `THVM_DUMP_BOUNDARY_ORDER`, `THVM_DUMP_STORE_TREE`, `THVM_DUMP_DIRECT_COUNT`, `THVM_DUMP_STRAND_GUARD`, `THVM_DUMP_KERNEL_SHAPE`, `THVM_DUMP_KERNEL_SRC`, `THVM_DUMP_LIFT_REJECT`, `THVM_DUMP_LIFT_COVERAGE`, `THVM_UPCAST_TRACE`, `THVM_GROUP_REDUCE_TRACE`, `THVM_HANDOPT_TRACE`, `THVM_ROUTE_TRACE`, `THVM_KCNT`, `THVM_KERNEL_PROFILE`, `THVM_DISPATCH_TRACE`, `THVM_UOP_GRAPH_SIMPLIFY`, `DUMP_UOP_REWRITE`, `THVM_CUDA_*` (LOG_COMPILES, DUMP_ALL_PTX, DUMP_FAIL_PTX, DUMP_KID, DUMP_SHARED, DUMP_FAILED_KERNEL, DUMP_DISPATCH, LAUNCH_TRACE, RENDER_TRACE, ALLOC_TRACE, JIT_GRAPH_TRACE), `THVM_CPU_UOP_WALK_TRACE`, `THVM_TRACE_UOP_WALK_DECLINE`, `THVM_JIT_REPLAY_TRACE`, `THVM_JIT_REPLAY_DEDUP_TRACE`, `THVM_ARENA_DUMP[_BUFS]`, `THVM_WL_TENSOR_VIEW_DEBUG`, `THVM_AOT_METAL_DUMP`.

**ATP / Wolfram (out of ML-kernel scope):** `THVM_ATP_TRACE_MAX`, `THVM_ATP_FLATTERM`, `THVM_ATP_CP_WEIGHT`, `THVM_ATP_AUTO_MAXW`, `THVM_ATP_MNF`, `THVM_ATP_CP_DATASET`, `THVM_NF_PARALLEL_STEP_SESSION`.

### 5.2 tinygrad knobs (`helpers.py:231-266` unless noted)

**Scheduling / fusion**

| Name | Effect | Default | Mode it changes |
|---|---|---|---|
| `PCONTIG` | Partial-contiguity in rangeify (per-axis merge aggressiveness) | 0 | Fusion granularity |
| `SCACHE` | Scheduler/rangeify output cache | 1 | Recompile avoidance |
| `SPLIT_REDUCEOP` | Two-phase split of large reduces | 1 | Reduce kernels |
| `REDUCEOP_SPLIT_THRESHOLD` / `REDUCEOP_SPLIT_SIZE` (`rangeify.py:94,106`) | Split trigger / chunk exp | 32768 / 22 | Reduce splitting |
| `FUSE_OPTIM` | Fuse all param updates into one kernel | 0 | Optimizer kernels |
| `DEBUG_RANGEIFY` | Trace rangeify decisions | 0 | (debug) |

**Optimization heuristic + BEAM** (`helpers.py`, `search.py`)

| Name | Effect | Default |
|---|---|---|
| `NOOPT` | Disable hand-coded opts | 0 |
| `BEAM` | Enable BEAM search (depth) | 0 |
| `JITBEAM` | BEAM depth inside JIT (overrides BEAM) | BEAM |
| `IGNORE_JIT_FIRST_BEAM` | Skip BEAM on first JIT compile | 0 |
| `IGNORE_BEAM_CACHE` | Force re-search | 0 |
| `PARALLEL` / `BEAM_MAX_TASKS_PER_CHILD` | Worker pool / respawn | cpu_count / 16 |
| `BEAM_TIMEOUT_SEC` | Per-kernel compile timeout | 10 |
| `BEAM_UOPS_MAX` | Reject kernel > N UOps | 3000 |
| `BEAM_UPCAST_MAX` / `BEAM_LOCAL_MAX` | Max upcast / local product | 256 / 1024 |
| `BEAM_MIN_PROGRESS` | Early-exit improvement floor (us) | 0.01 |
| `BEAM_PADTO` / `BEAM_STRICT_MODE` / `BEAM_DEBUG` / `BEAM_ESTIMATE` / `BEAM_DEV_TIMEOUT` / `BEAM_LOG_SURPASS_MAX` | search controls | 0/0/0/1/1/0 |
| `NOLOCALS` | Disable shared memory | 0 |
| `MV`, `MV_BLOCKSIZE`, `MV_THREADS_PER_ROW`, `MV_ROWS_PER_THREAD` (`heuristic.py:65`) | Matvec | 1, 4, 8, 4 |

**Tensor cores / dtype / vectorize**

| Name | Effect | Default |
|---|---|---|
| `TC` (`USE_TC`) | 0 off / 1 on / 2 shape-only | 1 |
| `TC_OPT` | 0 single-reduce / 1 multi-reduce+CAST / 2 padding | 0 (2 during BEAM) |
| `TC_SELECT` | Which TC spec (-1 auto) | -1 |
| `IMAGE` / `IMAGE_PITCH_ALIGN` / `IMAGE_BASE_ALIGN` | ImageDType float4 | 0 / 256(mac)/64 / 64 |
| `FLOAT16` / `DEFAULT_FLOAT` / `SUM_DTYPE` / `OPTIM_DTYPE` | dtype controls | 0 / float / float32 / float32 |
| `DEVECTORIZE` / `EXPAND_SSA` (`cstyle.py:205`) | Devectorize level / SSA expand | 1 / 0 |
| `TRANSCENDENTAL` | Math lib level | 1 |
| `DISABLE_FAST_IDIV` / `CORRECT_DIVMOD_FOLDING` | idiv / divmod folding | 0 / 0 |
| `ALLOW_TF32` | TF32 on NVIDIA | 0 |
| `WINO` | Winograd conv | 0 |

**Memory / multi-device / execution**

| Name | Effect | Default |
|---|---|---|
| `NO_MEMORY_PLANNER` / `LRU` | Disable planner / LRU allocator | 0 / 1 |
| `MAX_BUFFER_SIZE` / `MAX_KERNEL_BUFFERS` | Buffer / per-kernel caps | 0 / 0 |
| `JIT` / `JIT_BATCH_SIZE` | JIT mode / graph batch size | 2(mac x86) or 1 / 32 |
| `CAPTURING` / `TRACEMETA` / `UNSAFE_ALLOW_JIT_BUFFER` | Capture controls | 1 / 1 / off |
| `RING` / `ALL2ALL` / `ALLREDUCE_CAST` / `RING_ALLREDUCE_THRESHOLD` / `LATE_ALLREDUCE` | Multi-device allreduce | 1 / 0 / 1 / 256000 / 1 |
| `USE_ATOMICS` | Atomic embedding backward | 0 |
| `CACHELEVEL` / `CCACHE` | Disk cache level / ccache | 2 / 1 |
| `CPU_COUNT` / `THREADS` (`llvmir.py:197`) / `ALIGNED` | parallelism / CPU threading / aligned access | cpu_count / 1 / 1 |
| `DEV` / `DEBUG` / `VIZ` / `PROFILE` / `NO_COLOR` | device / verbosity / viz / profile / color | "" / 0 / 0 / abs(VIZ) / 0 |
| `VALIDATE_WITH_CPU` / `CHECK_OOB` / `SPEC` / `ASSERT_COMPILE` | correctness/debug | 0 / 0 / 1 / off |
| `CONST_LR` (`optim.py:24`) | constant LR vs 1-elem tensor | 0 |

---

## 6. What thvm is MISSING vs tinygrad

**Scheduling / fusion (algorithmic)**

- **BEAM search / autotuner** - *large*. No per-kernel variant exploration; fixed heuristic order. Where: new `src/codegen/beam_search.c` (action enumerator + device timing harness + Pareto + disk cache), port of `search.py`. Biggest single lever.
- **ShapeTracker / movement-op symbolic merge** - *large*. `ru_compose_view_chain` decomposes per-step; can't cancel split+resplit (unfold+col2im). Where: `rangeify_unified.c:1255-1337` + port of `indexing.py:141-145` symbolic merge.
- **Reduce-into-reduce fusion** - *large/medium*. One-reduce-per-kernel codegen rule; conv-backward emits 2 reduce kernels vs tinygrad's fused chains. This is *the* residual 226-vs-120 gap (per the `rangeify_unified.c:160` comment). Where: relax `bufferize_classify.c` reduce seed + multi-output `kernel_lift`.
- **Late rewrite / arange-reduce collapse** - *medium*. No `REDUCE(SUM,[RANGE]) -> range*extent` collapse (`simplify.py:146-155`); misses reduce-epilogue inlining.
- **PCONTIG per-axis ending-ranges check** - *small*. thvm realizes *all* non-realized axes on ending ranges; tinygrad checks per-axis contiguity (`indexing.py:227`). Where: `rangeify_unified.c:864-888`.
- **Boolean OR validity union** - *medium*. Falls back to `consumer[0]` instead of OR-merging valids (`indexing.py:211-213`). Needs term-algebra boolean ops.
- **BIND const-fold UOp + `ALWAYS_CONTIGUOUS` set** - *small*. No `Ops.BIND`; may leak scalar-load kernels. Where: `bufferize_classify.c` recognition + rangeify skip-op gate.
- **Stranded-range symbolic coverage proof** - *medium*. Strand checks are conservative; tinygrad proves coverage symbolically. Gates some broadcast-reduce fusions.

**Optimization heuristic (opt coverage)**

- **ImageDType float4 UPCAST** - *medium* (small if no image workloads). No `ImageDType`.
- **Mask UPCAST (WHERE detection)** - *medium/small*. No `IWHERE` mask-axis walker; masked dims count against occupancy.
- **THREAD axis + warp-shuffle reduce** - *large*. No `KOP_THREAD`; CPU via cBLAS; CUDA uses 16-thread GROUP_REDUCE instead of 32-lane warp-shuffle (2-3x slower per-warp reduce).
- **Full-extent UNROLL** - *medium*. Disabled by `rmu_emit_store_reduce` renderer bug; split-by-4 only.
- **Multi-reduce TC (`TC_OPT>=1`)** - *small*. `hand_opt_classify_matmul` requires single reduce axis. Where: `hand_opts.c:192-202`.
- **Wired expander/devectorizer** - *medium*. `uop/expander.c`/`devectorize.c` exist but are test-only, not in production render. Affects vector-register pressure on reduce-heavy kernels.
- **Dynamic local-size tuning** (`realize.py:63-82`) - *medium*. No micro-benchmark local-size loop.

**Infrastructure**

- **CL / ROCm backends** - *large*. CPU/Metal/CUDA only.
- **Graph batching maturity** - *large*. Partial Metal/CUDA graph replay; tinygrad batches up to `JIT_BATCH_SIZE` kernels per device-graph call.
- **Reuse-distance memory planner + layout opt** - *large*. Only TLSF arena; tinygrad's `memory_plan_rewrite` is more aggressive (ties to the known ~7x peak-mem-vs-tinygrad item).
- **MultiBuffer / MSELECT / MSTACK** - *large*. Distributed-training device replication is basic.

**Autodiff / optimizers** (`gradient.py` has these; `interact/uop_grad.c` does not)

- **SIN/COS backward** - *small* (`gradient.py:52`).
- **POW backward** - *medium* (`gradient.py:58-59`, multi-case with `b==0`/`e==0` edges).
- **Element-wise MAX backward** - *small* (`gradient.py:60-61`, 0.5 tie factor).
- **WHERE backward** - *small* (`gradient.py:63`, three-way split).
- **CONTIGUOUS / CONTIGUOUS_BACKWARD / COPY backward** - *small* (`gradient.py:65-66,75`).
- **MULTI / AFTER / TUPLE / FUNCTION+grad_fxn** - *large*. No custom grad functions; blocks higher-order grads.
- **Higher-order gradients (grad of grad)** - *large*. thvm grads are AST Terms, not differentiable Tensors; would need `TENS[tid].grad` rearchitected as a Tensor.
- **LARS / Muon optimizers** - *medium each*. Only SGD + Adam; Muon aliased to Adam.
- **zero_grad safety** - *small*. thvm accumulates into C-side `TENS[tid].grad`; missing `ten_clear_grad()` silently NaN-blows-up (`optim.py:46-58`). A runtime guard would help.

---

## 7. Plan to close the algorithmic coverage gap

Ordered by payoff-per-effort and dependency. Each milestone: what, why, expected payoff, how to verify. Tie-back: the headline gap is faithful (226 k / 14.8 MB) -> tinygrad (120 k / ~1-3 MB), at a 1.3x wall-time cost; the `rangeify_unified.c:160` comment localizes the residual to reduce fusion + symbolic merge, and the heuristic is shape-competitive except for the missing BEAM exploration.

### Milestone 0 - Correctness/quick-win cleanups (small, do first)

- **Autodiff backward rules:** add SIN/COS, POW, element-wise MAX, WHERE, CONTIGUOUS(/_BACKWARD), COPY to `interact/uop_grad.c` (mirror `gradient.py:49-75`). *Why:* unblocks differentiating common ops; cheap. *Verify:* numeric grad-check tests per op vs finite differences and vs tinygrad.
- **zero_grad guard:** in `py/thvm/tensor.py:backward()`, assert/warn if any requires_grad leaf has non-zero `TENS[tid].grad` at entry. *Verify:* a test that omits zero_grad and expects the warning.
- **PCONTIG ending-ranges per-axis check:** replace the "realize all non-realized axes" fallback (`rangeify_unified.c:864-888`) with a per-axis contiguity test using `prior_views` strides, gated by a `PCONTIG`-style knob. *Why:* small, directly recovers EXPAND-downstream elementwise fusions. *Payoff:* a handful of kernels on broadcast-heavy graphs. *Verify:* faithful-mode kernel count on softmax/layernorm + regression suite.
- **Multi-reduce TC (`TC_OPT>=1`):** relax `hand_opt_classify_matmul` (`hand_opts.c:192-202`) to accept >=1 reduce axis behind a `TC_OPT` gate. *Verify:* batch-matmul kernel uses simdgroup_matrix/WMMA; GFLOPS up, output matches.

### Milestone 1 - Reduce-into-reduce fusion (the named codegen gap)

The `rangeify_unified.c:160` comment is explicit: faithful seeding is correct but capped by one-reduce-per-kernel codegen. **What:** (a) relax `bufferize_classify.c` so a reduce feeding a single-consumer reduce chain isn't force-seeded; (b) handle multiple REDUCE outputs in `RU_REDUCE_RANGES` derivation; (c) extend `kernel_lift` for multi-reduce/multi-output kernels in `materialize.c`. **Why:** this is the largest remaining fusion gap after seed mode; conv-backward weight+data grads collapse to one kernel. **Payoff:** expect faithful kernel count to drop materially toward tinygrad's 120 and conv-bwd memory to fall. **Verify:** conv-backward kernel count (target: 2 reduce kernels -> 1), `beautiful_mnist` faithful kernel count, output parity vs tinygrad on both weight grads, regression-clean on grad.wlt/nn.wlt.

### Milestone 2 - ShapeTracker symbolic merge + late arange/reduce-collapse

**What:** port tinygrad's symbolic Term simplification (`graph_rewrite` + `pm_simplify_valid`) into `ru_compose_view_chain` and `apply_movement_op_reshape` so consecutive RESHAPE/SHRINK/PERMUTE views cancel; add the `reduce_collapse`/`reduce_load_collapse` late rewrites (`simplify.py:146-155`). **Why:** removes the need for the `THVM_FUSE_CONV_BWD` placeholder hack (it becomes a real symbolic cancellation), fixes unfold+col2im non-cancellation, and enables reduce-epilogue inlining. This also unblocks the boolean-OR validity union (Milestone 2b: add term-algebra boolean ops, then OR-merge valids at `rangeify_unified.c:838-862`). **Payoff:** further fusion + lower memory on conv/attention; retires a hand-coded special case. **Verify:** split+resplit test cancels to identity ranges; conv-backward fuses without the FUSE_CONV_BWD env flag; kernel-count + peak-memory regression vs tinygrad.

### Milestone 3 - BEAM search harness (the headline opt gap)

**What:** `src/codegen/beam_search.c` - (1) action enumerator mirroring `search.py:13-25`; (2) UOP-DAG mutation API (apply Opt, re-lift, re-render); (3) device timing (CUDA events / Metal timestamp queries, L2 clear, 3-run early-stop at 3x best); (4) Pareto top-`amt` per level with `BEAM_UOPS_MAX`/`UPCAST_MAX`/`LOCAL_MAX` rejection; (5) disk cache keyed `(AST, device, amt)`; (6) thread-pool orchestration (`PARALLEL`). Wire the existing `BEAM`/`AUTOTUNE` env readers in `codegen/autotune.c` to it. **Why:** the only mechanism that recovers when the fixed heuristic order is wrong for a shape; tinygrad's primary peak-perf lever. **Payoff:** 1.5-3x on compute-bound kernels (matmul/conv) per tinygrad's own data; lets thvm match tinygrad's fused-kernel timings. **Verify:** beautiful_mnist warm train with `BEAM=2`: each hot kernel's GFLOPS vs heuristic baseline; total wall-time; cache hit on second run; correctness via `VALIDATE_WITH_CPU`-style cross-check.

### Milestone 4 - Opt-section completion

- **Wire expander + devectorizer** into production `render_uop.c` (currently test-only in `uop/linearize.c`); tune vs compile-time. *Verify:* reduce-heavy kernel register pressure / GFLOPS; no regressions.
- **Full-extent UNROLL:** fix `rmu_emit_store_reduce` to follow the UNROLL axis spine so `UNROLL(last,0)` works for extent<=32. *Verify:* small-reduce kernels straight-line; output parity.
- **Mask UPCAST:** add a WHERE/IWHERE mask-axis walker; exclude mask axes from upcastable-dim occupancy counting. *Verify:* attention-mask kernel occupancy.
- **THREAD axis + warp-shuffle:** add `KOP_THREAD`, emit `shfl_xor` warp reduce on CUDA, OpenMP `#pragma omp for` on CPU (pattern-match-safe so cBLAS routing survives). *Verify:* CUDA per-warp reduce 2-3x vs GROUP_REDUCE; CPU thread scaling.

### Milestone 5 - Memory planner + graph batching (ties to the queued ~7x peak-mem item)

**What:** reuse-distance/linear-scan buffer planner with alias-friendly reduce-output layout (extend the TLSF arena), and mature Metal/CUDA graph batching (group up to a `JIT_BATCH_SIZE` analog per device-graph call). **Why:** closes the 14.8 MB -> ~3 MB peak-memory gap and the 10-50% small-kernel dispatch overhead; directly addresses the standing memory-parity follow-up. **Payoff:** peak memory toward tinygrad-flat; dispatch overhead down. **Verify:** peak working-set ratio vs tinygrad on beautiful_mnist + a transformer block; dispatch count / wall-time delta from batching.

### Milestone 6 - Optimizers + higher-order grads (independent track)

**What:** implement LARS and a real Muon (Newton-Schulz, `ns_steps`/coefficients) in `py/thvm/optim.py`; then the larger rearchitecture making `TENS[tid].grad` a differentiable Tensor to support grad-of-grad and FUNCTION/`grad_fxn`. **Why:** enables fine-tuning workloads (LARS), second-order methods (Muon), and custom/higher-order autograd. **Payoff:** feature parity for training research. **Verify:** optimizer convergence vs tinygrad on a small net; second-derivative grad-check for higher-order.

### Milestone 7 - Backend breadth (large, lowest priority for ML-kernel parity)

CL/ROCm backends mirroring the CUDA vtable, and MultiBuffer/MSELECT/MSTACK for multi-device. *Verify:* existing CUDA/CPU/Metal correctness suites ported to the new backend; distributed allreduce parity.

**Recommended ordering rationale:** M0 (cheap correctness) and M1 (the named codegen gap) give the most fusion/memory parity per effort and are prerequisites for honestly evaluating M3's BEAM gains. M2 retires a hand-coded hack and unlocks symbolic-dependent wins. M3 is the headline peak-perf lever but is only worth its large cost once M1/M2 stop bounding fusion. M4-M5 are incremental opt/memory parity. M6-M7 are independent feature tracks.

---

*Relevant source anchors:* thvm scheduling `/private/tmp/thvm-jit/src/schedule/rangeify_unified.c:147-166,480-487,591-647,733-999,835-888,1255-1337`, `bufferize_classify.c:728-822`, `materialize.c:180-200,2465-2499,2841-2865`; hand-opts `/private/tmp/thvm-jit/src/codegen/hand_opts.c:1-52,192-202,241-281,297-346,369-396`; grad `/private/tmp/thvm-jit/src/interact/uop_grad.c`, `py/thvm/optim.py:46-58`. tinygrad spec `/Users/swish/src/tinygrad/tinygrad/schedule/indexing.py:10-11,28-35,129-145,148-269,211-232`, `codegen/opt/heuristic.py:27-189`, `codegen/opt/search.py:13-25,120-193`, `gradient.py:49-82`, `helpers.py:231-266`.