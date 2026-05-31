# thvm vs tinygrad: Comprehensive ML-Compiler Comparison, Knob Reference, and Coverage-Gap Plan

*Comparison of `thvm` (C port, branch `py-jit-speedup`, `/private/tmp/thvm-jit`) against `tinygrad` (the spec, `/Users/swish/src/tinygrad/tinygrad`). All file:line citations are against those trees. Benchmark figures are warm Metal `beautiful_mnist` train unless noted; the CUDA picture is qualitatively identical.*

> **Revision (2026-06-01).** The first draft of this report was assembled by
> excerpt-reading agents and overstated three gaps; each was disproved by reading the
> full source + runtime tests, and the corrections are folded throughout (see
> `docs/plans/ideal_pipeline_v3.md` for the milestone log):
> - **BEAM is NOT missing.** `src/codegen/autotune.c` (855 lines) is a real
>   propose -> bench -> cache -> pick-winner search, wired in the fire path
>   (`uop_kernel.c:255`) and WL-exposed. It was *blocked on the py/JIT path* by a flag
>   bug (hand_opts set the shared `autotuned` flag, suppressing autotune) -- **fixed**
>   (`3c134358`) -- and the candidate **proposer** (`propose.c`) was narrow -- **expanded
>   for Metal** (`e5c51194`). With both, `BEAM=2` on Metal beautiful_mnist is **2.07 ms**
>   (vs 3.57 ms heuristic-only). The remaining proposer gap (full action space, CUDA
>   re-eval) is medium, not a from-scratch build.
> - **Symbolic rewriting is NOT missing.** `src/uop/{symbolic_rewrite,graph_rewrite,
>   graph_simplify}.c` + constructor-time `rewrite.c`/`index_simplify.c` port a subset
>   of tinygrad `symbolic.py`, wired in materialize + render. The gap is *specific*
>   unported rules (boolean-OR validity union, full view-cancellation, arange-collapse),
>   a medium completion.
> - **The kernel-count gap is NOT reduce-into-reduce.** Audited: of 207 faithful
>   beautiful_mnist boundaries only 14 are REDUCE; forward+backward alone is 123 ~=
>   tinygrad's *total* 120. The excess is the unfused optimizer (84 boundaries) + ~74
>   walk-realized intermediates whose root cause is the missing boolean-OR-of-valids
>   merge in the consumer-divergence realize. There are also **no missing autodiff
>   backward rules** (Tensor ops like maximum/relu decompose to primitives that already
>   have gradients; thvm has no SIN/COS/POW/WHERE *primitives* to differentiate).

---

## 1. Executive summary

`thvm` is a faithful C port of tinygrad's UOP-DAG-to-kernel compiler: it reproduces the same pipeline shape (UOP DAG -> realize-seed classify -> consumer-driven rangeify -> materialize -> render -> backend dispatch), the same consumer-divergence fusion model, a near-line-by-line port of tinygrad's hand-coded optimization heuristic (`hand_coded_optimizations` -> `src/codegen/hand_opts.c`), AND a real BEAM autotuner (`src/codegen/autotune.c`) and symbolic rewrite engine (`src/uop/symbolic_rewrite.c` et al.). The two diverge in three places that matter. **(1) Realize-seeding:** tinygrad seeds *only* structural boundaries (`COPY`/`CONTIGUOUS`/`STORE`, `indexing.py:28-35`) and derives every other realize from the rangeify walk; thvm's *default* mode over-seeds (`ROOT + MULTI + REDUCE + MATMUL + FANIN_CAP` in `bufferize_classify.c`), producing more, smaller kernels, while its opt-in **faithful** mode (`THVM_RU_FAITHFUL_SEED=1`, `rangeify_unified.c:147-166`) seeds `ROOT` only to match tinygrad's structural seed. **(2) Optimization search:** both pair the hand-coded heuristic with a BEAM autotuner, but thvm's candidate **proposer** (`propose.c`) is narrower than tinygrad's ~200-action `search.py` (and was only recently unblocked on the py/JIT path); thvm also has no second-phase warp-shuffle THREAD reduce. **(3) Symbolic depth:** thvm's symbolic engine ports a *subset* of tinygrad's `symbolic.py`; the unported rules (boolean-OR validity merge, full ShapeTracker view-cancellation, arange-collapse) are where its movement/validity handling stays conservative. The measured tradeoff is the report's headline (warm Metal, BS=8): **thvm default 3.57 ms / 328 kernels / 31 MB**, **thvm default + `BEAM=2` 2.07 ms**, **thvm faithful 4.72 ms / 226 kernels / 14.8 MB**, **tinygrad 6.03 ms / 120 kernels / ~1-3 MB**. thvm wins wall-time (over-realization + a thvm-specific occupancy floor maximize per-kernel parallelism; BEAM closes most of the rest); tinygrad wins fusion and memory (heavier fusion + symbolic merge + structural seed). Faithful mode is the middle: it closes ~half the kernel-count gap (328 -> 226) and most of the memory gap (31 -> 14.8 MB) for a ~1.3x wall-time cost. The remaining 226-vs-120 kernel-count gap is **algorithmic** but lives in the **unfused optimizer + the missing boolean-OR-of-valids merge** (the rangeify walk over-realizes ~74 intermediates), NOT in reduce-into-reduce fusion (forward+backward alone already matches tinygrad's kernel count).

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

**BEAM (proposer-coverage gap, not a missing engine).** tinygrad's `search.py` enumerates ~200 actions/kernel (UPCAST 6x8, UNROLL 3x5, LOCAL 7x6, GROUPTOP 8x3, GROUP 4x3, optional PADTO, TC specs, SWAP perms, THREAD 10x3), compiles + times each on device (3 runs, early-stop at 3x best), keeps a Pareto top-`amt` per level, iterates to convergence, and disk-caches by `(AST, device, amt)`. **thvm has the equivalent search engine** -- `src/codegen/autotune.c` (`kernel_autotune`: propose -> bench-each-variant with `BEAM_RUNS` timing runs -> sequence-build up to `KAUTOTUNE_SEQ_MAX` -> pick-winner -> disk-cache), gated on `BEAM>0`/`AUTOTUNE=1` and wired in the fire path (`uop_kernel.c:255`) + WL (`thvm_wl_kernel_autotune`). Two real gaps remain, both addressed or scoped: (a) a flag bug had hand_opts set the shared `autotuned` flag, suppressing autotune on every non-WL path -- **fixed** (`3c134358`, split `hand_coded_done`); (b) the candidate **proposer** (`propose.c`) enumerates far fewer actions than `search.py` -- **expanded for Metal** to UPCAST/LOCAL/GROUPTOP per kernel (`e5c51194`), still missing the full action space (PADTO, SWAP, THREAD) and broad CUDA coverage. So the gap is "widen the proposer + the per-level Pareto beam", NOT "build BEAM". Measured: with the fixes, `BEAM=2` on Metal beautiful_mnist is 2.07 ms vs 3.57 ms heuristic-only.

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

**Autotune / BEAM** (real search in `autotune.c`; narrower proposer than tinygrad)

| Name | Effect | Default |
|---|---|---|
| `BEAM` | Beam width; `>0` drives the `kernel_autotune` propose->bench->cache search | 0 |
| `AUTOTUNE` | Enable autotune (`BEAM>0` also enables) | off |
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

- **BEAM proposer coverage** - *medium* (engine EXISTS, not missing). `autotune.c`'s search is real + wired; the `propose.c` candidate set is narrow vs `search.py`'s ~200 actions. Where: widen `propose.c` (PADTO/SWAP/THREAD + the full UPCAST/LOCAL/GROUP grid, CUDA too) and add a per-level Pareto beam in `autotune.c`. The flag bug that suppressed it on non-WL paths is already fixed (`3c134358`); the Metal proposer is already expanded (`e5c51194`).
- **Boolean-OR validity union** - *medium/large*. The consumer-divergence realize (`rangeify_unified.c:838-862`) falls back to consumer[0] / per-axis realize where tinygrad OR-merges the per-axis validity masks and stays a view (`indexing.py:211-213`). **This is the actual driver of the kernel-count/memory gap** (~74 walk-realized intermediates). Needs term-algebra boolean ops first.
- **ShapeTracker / movement-op symbolic-merge completion** - *medium/large* (engine exists, subset ported). `ru_compose_view_chain` decomposes per-step and can't cancel split+resplit (unfold+col2im); the symbolic rewrite engine is wired but lacks the view-cancellation + arange/reduce-collapse rules. Where: `rangeify_unified.c:1255-1337` + extend `uop/symbolic_rewrite.c` toward `symbolic.py`/`indexing.py:141-145`.
- **Optimizer fusion** - *large*. The Adam step emits ~84 boundaries (3+ assigns/param x 16) where tinygrad fuses across params; needs multi-output kernels (a `kernel_lift` that emits N independent stores) / a `FUSE_OPTIM` analog. (NOTE: reduce-into-reduce fusion is NOT a gap -- only 14 of 207 faithful boundaries are REDUCE and forward+backward already matches tinygrad's kernel count; the v1 draft's "reduce-into-reduce is the residual" claim is retracted.)
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

- **No missing per-op backward RULES** (corrected). `uop_grad.c` covers every thvm
  primitive (ADD/MUL/NEG/RECIP/SQRT/EXP2/LOG2/REDUCE/RESHAPE/EXPAND/PERMUTE/PAD/SHRINK/
  FLIP/CAST...). thvm has no SIN/COS/POW/MAX/WHERE *primitive UOPs*; the Tensor ops that
  exist (`maximum`/`minimum`/`relu`) DECOMPOSE to CMPLT+MUL+ADD, so their gradients flow
  through the existing rules. SIN/COS/POW/WHERE are absent Tensor *features* (small
  decomposed adds if a workload needs them), not autodiff gaps.
- **MULTI / AFTER / TUPLE / FUNCTION+grad_fxn** - *large*. No custom grad functions; blocks higher-order grads.
- **Higher-order gradients (grad of grad)** - *large*. thvm grads are AST Terms, not differentiable Tensors; would need `TENS[tid].grad` rearchitected as a Tensor.
- **LARS / Muon optimizers** - *medium each*. Only SGD + Adam; Muon aliased to Adam.
- **zero_grad footgun** (not a code gap). thvm's `backward()` accumulates into the
  C-side `TENS[tid].grad` (tinygrad-faithful `grads[k]+=v`); you must call
  `opt.zero_grad()` (a bare `p.grad=None` only drops the Python handle). The benches now
  do (`bb24342d`). A warning guard was considered and SKIPPED: it would false-positive
  on legitimate gradient-accumulation-over-microbatches, and tinygrad doesn't warn either.

---

## 7. Plan to close the algorithmic coverage gap

> The **authoritative, audited, executable** milestone plan lives in
> `docs/plans/ideal_pipeline_v3.md` (with a per-commit status log). The draft plan that
> was here was built on the three premises corrected in the Revision banner above
> (build-BEAM, reduce-into-reduce-is-the-residual, missing-backward-rules) and is
> superseded. The corrected priorities are summarized below.

**Already landed** (branch `py-jit-speedup`): `e14f7da9` faithful conv-bwd compiles;
`bb24342d` bench zero_grad; `d595fcb7` py dylib links the real Metal backend;
`3c134358` BEAM flag-conflict fix (autotune now runs on the py/JIT path);
`e5c51194` general Metal BEAM proposer (Metal `BEAM=2` 3.57 -> 2.07 ms).

**Corrected priority order** (tie-back: thvm already wins wall-time on both backends;
the open gap is fusion/memory parity, plus widening BEAM's reach):

1. **Finish BEAM coverage** (medium): widen `propose.c` to the full `search.py` action
   space (PADTO/SWAP/THREAD + the UPCAST/LOCAL/GROUP grid, CUDA too); add a per-level
   Pareto beam in `autotune.c`. Re-evaluate hand_opts vs BEAM per backend (hand_opts is
   net-negative on Metal post-bench-fix) and decide the default policy. *Verify:* per-
   kernel GFLOPS + warm wall-time, CUDA + Metal, cache hit on 2nd run, output parity.
2. **Boolean-OR validity union** (medium/large): the real kernel-count/memory lever.
   Add term-algebra boolean ops, then OR-merge valids at `rangeify_unified.c:838-862`
   instead of realizing on divergence (mirrors `indexing.py:211-213`). *Verify:* faithful
   walk-realized boundary count drops; peak-memory vs tinygrad; grad/nn regression-clean.
3. **Symbolic-merge completion** (medium/large): view-cancellation in
   `ru_compose_view_chain` + arange/reduce-collapse late rewrites; retires the
   `THVM_FUSE_CONV_BWD` hack. *Verify:* split+resplit cancels to identity; conv-bwd
   fuses without the env flag.
4. **Optimizer fusion** (large): multi-output `kernel_lift` / a `FUSE_OPTIM` analog so
   the Adam step stops emitting ~84 boundaries (3+/param x 16). Ties to memory parity +
   graph batching.
5. **Opt-section completion** (M4): full-extent UNROLL renderer fix, mask-UPCAST,
   THREAD + CUDA warp-shuffle, multi-reduce TC (`TC_OPT>=1`).
6. **Memory planner + graph batching** (M5); **optimizers (LARS/Muon) + higher-order
   grads** (M6); **backend breadth (CL/ROCm, multi-device)** (M7).

**Cross-cutting clean track** (co-equal "clean" goal): retire/document the env-knob
sprawl, sweep transient comments, and refactor the `render_uop.c` reduce-emission path
once the symbolic-merge work retires the strand/cap hacks.

---

*Relevant source anchors:* thvm scheduling `/private/tmp/thvm-jit/src/schedule/rangeify_unified.c:147-166,480-487,591-647,733-999,835-888,1255-1337`, `bufferize_classify.c:728-822`, `materialize.c:180-200,2465-2499,2841-2865`; hand-opts `/private/tmp/thvm-jit/src/codegen/hand_opts.c:1-52,192-202,241-281,297-346,369-396`; grad `/private/tmp/thvm-jit/src/interact/uop_grad.c`, `py/thvm/optim.py:46-58`. tinygrad spec `/Users/swish/src/tinygrad/tinygrad/schedule/indexing.py:10-11,28-35,129-145,148-269,211-232`, `codegen/opt/heuristic.py:27-189`, `codegen/opt/search.py:13-25,120-193`, `gradient.py:49-82`, `helpers.py:231-266`.