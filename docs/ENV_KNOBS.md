# thvm environment knobs

Single source of truth for the `THVM_*` environment variables read at runtime.
Every knob below is **live**: it is consumed by a `getenv` / `env_flag_on` /
`hand_opt_getenv_int` call in C, an `os.environ.get` in Python, or an
`Environment[...]` lookup in Wolfram Language. Knobs absent from this list are
either compile-time `#ifdef` defines (e.g. `THVM_HAS_CUDA`) or do not exist.

Conventions:
- Boolean knobs are "on" when the value's first character is `'1'` unless noted.
- Defaults reflect the value used when the variable is unset.
- "Behavior-preserving" means toggling the knob changes performance / diagnostics
  only, never numerical results. "Gate" means it changes the realized math and
  must be validated before use.

This index is maintained by hand. When you add or remove a `THVM_*` getenv,
update the matching row here in the same change.

---

## Correctness gates (change realized math -- validate before enabling)

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_JIT_REALIZE_DEDUP` | off | Schedule-level cross-realize dispatch dedup. Bit-exact on CPU (~17% faster). On CUDA: correct + ~6% faster on plain conv models (simple-model 7.8->7.3ms, beats tinygrad's 7.6ms) after the kernel_gc_sweep backend-aware liveness fix; but on BatchNorm/max_pool models (beautiful_mnist) the replay still DIVERGES at iter2 and is ~14% SLOWER -- a separate BN/pool dedup-replay bug, under investigation. Keep off for BN models. |
| `THVM_FUSE_CONV_BWD` | off | Fuse the conv-backward reduce into a strided-view reduce (cuts peak memory). Gated; validate grads before enabling. |

## Core scheduling & JIT

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_JIT` (Python) | on (`1`) | Enable the Python JIT capture/replay. `0` disables. |
| `THVM_JIT_NO_PIN` | off | Disable JIT buffer pinning. |
| `THVM_JIT_STRIDED` | off | Allow strided JIT capture of movement-op views. |
| `THVM_JIT_REPLAY_DEDUP` | off | Experimental: dedup redundant dispatches during JIT replay. |
| `THVM_JIT_REPLAY_DEDUP_WINDOW` | (impl) | Lookback window size for replay dedup. |
| `THVM_JIT_REPLAY_DEDUP_TRACE` | off | Trace replay-dedup decisions. |
| `THVM_JIT_REPLAY_NOSKIP` | off | Disable replay skip optimization. |
| `THVM_JIT_REPLAY_PACK` | off | Experimental: pack replay dispatch records. |
| `THVM_JIT_REPLAY_TRACE` | off | Trace JIT replay execution. |
| `THVM_BUNDLE_ASSIGNS` (Python) | on (`1`) | Bundle tensor assignments into one realize; `0` for the cross-dependent-assign edge case. |
| `THVM_NF_PARALLEL_STEP_SESSION` | off | Parallel normal-form step session. |
| `THVM_PRECISE_FIRE_MEMO` | off | Precise dispatch-fire memoization. |
| `THVM_DISPATCH_TRACE` | off | Trace dispatch memoization. |
| `THVM_RU_FAITHFUL_SEED` | off | Realize-seed only structural boundaries (ROOT) + REDUCE outputs, deriving the rest in the rangeify walk (tinygrad's structural seed); default mode also seeds MULTI/MATMUL/FANIN. Faithful CPU beats tinygrad + default (see [[project_faithful_cpu_breakthrough]]). |
| `THVM_RU_NO_SEED_REDUCE` | off | A/B revert: under faithful seed, go back to ROOT-only (drop the REDUCE-output seed). ROOT-only under-realizes -> conv data-grad becomes a col2im gather (faithful beautiful_mnist CPU 5.18ms -> 77.6ms). For comparison only. |

## Bufferize & kernel lift

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_BUFFERIZE_DEBUG` | off | Debug the bufferize classifier. |
| `THVM_BUFFERIZE_REDUCE_FUSE_MULTI` | (impl) | Multi-reduce fusion in bufferize. |
| `THVM_BUFFERIZE_SOFTMAX_REDUCE_TILE_CAP` | (impl) | Softmax reduce tile cap. |
| `THVM_BUFFERIZE_KEEP_NONMATMUL_REDUCE` | (impl) | Keep non-matmul reduces realized. |
| `THVM_BUFFERIZE_SKIP_SMALL_EXPAND` | (impl) | Skip bufferizing small EXPAND ops. |
| `THVM_BUFFERIZE_SKIP_REDUCE_INTO_REDUCE_SEED` | (impl) | Skip the reduce-into-reduce seed. |
| `THVM_BUFFERIZE_REMOVE_SCORE_LIFT_REDUCE_GATE` | (impl) | Gate the cost-score removal rule's reduce-lift scoring. |
| `THVM_FUSE_REDUCE_INTO_REDUCE` | (impl) | Fuse consecutive reduces. |
| `THVM_REDUCE_UNMARK_CAP` | (impl) | Reduce-unmark capacity. |
| `THVM_UOP_GRAPH_SIMPLIFY` | on | Simplify the UOp graph; `0` disables. |

## Hand-coded codegen optimizations (behavior-preserving tuning)

Read via `hand_opt_getenv_int` in `src/codegen/hand_opts.c`.

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_UPCAST_CAP` | 32 | Cap upcast size (tinygrad-faithful default 32). |
| `THVM_UPCAST_REDUCE_MIN_GRID` | 0 | Per-kernel occupancy floor, reduce-heavy kernels only. |
| `THVM_UPCAST_TRACE` | off | Trace UPCAST / UNROLL decisions. |
| `THVM_GROUPTOP` | on (`1`) | GROUP_REDUCE heuristic gate; `0` disables. |
| `THVM_GROUP_SZ` | 16 | Cooperative block size for reduces. |
| `THVM_GROUP_REDUCE_TRACE` | off | Trace GROUP_REDUCE. |
| `THVM_LOCAL_INNER_FIRST` | 0 | Local-var ordering (`1` reverses expand/reduce order). |
| `THVM_LOCAL_CAP` | 128 | Local thread-block cap. |
| `THVM_TILE` | off | Tile-size tuning for the tiled codegen path. |
| `THVM_LOOP_UNROLL_MAX` | (impl) | Max loop-unroll iterations. |
| `THVM_HANDOPT_TRACE` | off | Trace hand-coded optimization decisions. |
| `THVM_NO_FAST_IDIV` | off | A/B revert (read in `index_simplify.c`, not hand_opts): disable the fast_idiv/magicgu late-rewrite that lowers `x//c` -> `(x*m)>>s` at the C-render root (tinygrad's decompositions.py). On = keep literal IDIV/IMOD. |

## Memory, GC & buffer management

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_GC` | on | Heap GC; `0` disables. |
| `THVM_GC_KB` | (impl) | Heap GC trigger threshold (KB). |
| `THVM_KGC` | (impl) | Kernel GC control. |
| `THVM_HEAP_CELLS` | (impl) | Initial heap cell count. |
| `THVM_MAX_BUF_BYTES` | (impl) | Max single-buffer allocation (KB). |
| `THVM_MAX_LIVE_BYTES` | (impl) | Max live-buffer memory. |
| `THVM_REUSE_BUFS` | (impl) | Buffer reuse across backends. |
| `THVM_CUDA_REUSE_BUFS` | (impl) | CUDA-specific buffer reuse. |
| `THVM_METAL_REUSE_BUFS` | (impl) | Metal-specific buffer reuse. |
| `THVM_ARENA_PLAN` | (impl) | Buffer arena planning. |
| `THVM_ARENA_DUMP` | off | Dump arena alloc/dealloc events. |
| `THVM_ARENA_DUMP_BUFS` | off | Dump the arena buffer list. |

## CPU backend

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_THREADS` | (impl) | CPU worker-thread count. |
| `THVM_CPU_JIT_WARMUP` | (impl) | CPU JIT warmup iterations. |
| `THVM_CPU_UOP_WALK` | (impl) | CPU UOp-walk control. |
| `THVM_CPU_UOP_WALK_TRACE` | off | Trace each UOp-walk entry (lift outcome + root op). |
| `THVM_TRACE_UOP_WALK_DECLINE` | off | Trace why the UOp walk declined a kernel. |
| `THVM_CPU_BLAS_DISABLE` | off | Disable the cblas dispatch path (force the generic walker). |
| `THVM_BLAS_CONTRACTION_TRACE` | off | Trace BLAS contraction dispatch. |

## CUDA backend

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_CUDA_FAST_MATH` | on | Enable PTX fast-math. |
| `THVM_CUDA_PTX` | (impl) | PTX emission control. |
| `THVM_CUDA_PTX_MAX` | (impl) | Max PTX register count. |
| `THVM_CUDA_SYNC` | off | Force a CUDA sync after each kernel. |
| `THVM_CUDA_GPU_TIME` | off | Print GPU execution time. |
| `THVM_CUDA_JIT_GRAPH` | (impl) | Enable the CUDA JIT graph. |
| `THVM_CUDA_JIT_GRAPH_TIME` | off | Print CUDA JIT-graph build/run time. |
| `THVM_CUDA_JIT_GRAPH_TRACE` | off | Trace CUDA JIT-graph construction. |
| `THVM_CUDA_DISK_CACHE` | (impl) | Disk-cache compiled CUDA kernels. |
| `THVM_CUDA_CACHE_DIR` | (impl) | CUDA kernel cache directory. |
| `THVM_CUDA_KE_CACHE` | (impl) | Kernel-entry cache control. |
| `THVM_CUDA_LOG_COMPILES` | off | Log CUDA compilations. |
| `THVM_CUDA_ALLOC_TRACE` | off | Trace CUDA buffer allocation. |
| `THVM_CUDA_LAUNCH_TRACE` | off | Trace CUDA kernel launches. |
| `THVM_CUDA_RENDER_TRACE` | off | Trace CUDA PTX rendering. |
| `THVM_CUDA_DUMP_DISPATCH` | off | Dump dispatch metadata. |
| `THVM_CUDA_DUMP_KID` | off | Dump kernel IDs. |
| `THVM_CUDA_DUMP_ALL_PTX` | off | Dump all generated PTX. |
| `THVM_CUDA_DUMP_FAIL_PTX` | off | Dump PTX for failed compiles. |
| `THVM_CUDA_DUMP_FAILED_KERNEL` | off | Dump the failed kernel program. |
| `THVM_CUDA_DUMP_SHARED` | off | Dump shared-memory usage. |

## Metal backend

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_METAL_GRAPH_REPLAY` | (impl) | Metal graph replay. |
| `THVM_METAL_GRAPH_MAX_DISPATCHES` | (impl) | Max dispatches per Metal graph. |
| `THVM_METAL_GRAPH_TRACE` | off | Trace Metal graph construction. |
| `THVM_METAL_FUSION_MAX_INPUTS` | (impl) | Max inputs to a fused Metal kernel. |
| `THVM_METAL_BATCH` | (impl) | Metal dispatch batching. |
| `THVM_METAL_DEFER_BYTES` | (impl) | Deferred-allocation byte threshold. |
| `THVM_METAL_FREELIST_BYTES` | (impl) | Metal freelist byte cap. |
| `THVM_METAL_JIT_STATS` | off | Print Metal JIT stats. |
| `THVM_METAL_PROFILE_PEROP` | off | Per-op Metal profiling. |
| `THVM_METAL_PSO_CACHE` | (impl) | Pipeline-state-object cache. |
| `THVM_METAL_PSO_CACHE_DIR` | (impl) | PSO cache directory. |
| `THVM_METAL_PSO_CACHE_STATS` | off | Print PSO cache stats. |
| `THVM_AOT_METAL_DUMP` | off | Dump AOT Metal IR. |
| `THVM_AOT_METAL_KEEP_BOOK` | off | Keep the AOT Metal bookkeeping artifacts. |
| `THVM_CONV_POOL` (WL) | on | NN conv-pool path; `0` disables. |

## Diagnostics, tracing & dumps (behavior-preserving)

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_KERNEL_PROFILE` | off | Per-kernel wall-time profile (top N rows). |
| `THVM_DUMP_BOUNDARY_ORDER` | off | Dump kernel emit order. |
| `THVM_DUMP_DIRECT_COUNT` | off | Dump the direct kernel count. |
| `THVM_DUMP_KERNEL_SHAPE` | off | Dump kernel shapes. |
| `THVM_DUMP_KERNEL_SRC` | off | Dump generated kernel source. |
| `THVM_DUMP_TILE_JIT_SRC` | off | Dump tiled-JIT kernel source. |
| `THVM_DUMP_LIFT_COVERAGE` | off | Dump kernel-lift coverage stats. |
| `THVM_DUMP_LIFT_REJECT` | off | Dump rejected lifts. |
| `THVM_DUMP_MATMUL_DETECT` | off | Dump matmul detection. |
| `THVM_DUMP_STORE_TREE` | off | Dump the store tree. |
| `THVM_DUMP_STRAND_GUARD` | off | Dump stranded-buffer guards. |
| `THVM_DEBUG_BYPASS_LAST` | off | Debug last-kernel bypass. |
| `THVM_ROUTE_TRACE` | off | Trace PTX routing. |
| `THVM_KCNT` | off | Kernel-count debug. |

## ATP (automated theorem proving)

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_ATP_FLATTERM` | (impl) | Flatterm normalization in the ATP engine. |
| `THVM_ATP_TRACE_MAX` | (impl) | ATP proof-trace capacity. |
| `THVM_ATP_CP_DATASET` | unset | When set to a filename, capture the ENIGMA CP dataset (WL bridge). |

## Misc

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_CACHE` (Python) | `~/.cache/thvm` | Dataset cache directory. |
| `THVM_BENCH_OPT` | off | Benchmark optimization selector (`tools/xbackend_bench`). |
| `THVM_WL_TENSOR_VIEW_DEBUG` | off | WL tensor-view debug output. |

---

## A/B gate command (V100, CUDA)

`THVM_JIT_REALIZE_DEDUP` is bit-exact and correct on CPU + CUDA. The A/B
comparison on a V100 pod is:

```
# baseline (knob off)
DEV=cuda BS=32 python3 py/examples/beautiful_mnist_train.py
# with cross-realize dedup
DEV=cuda BS=32 THVM_JIT_REALIZE_DEDUP=1 python3 py/examples/beautiful_mnist_train.py
```

Loss must match step-for-step; CUDA warm-step drops ~6% (simple-model
7.8->7.3ms, ahead of tinygrad's 7.6ms).
