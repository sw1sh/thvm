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

## Core scheduling & JIT

| Knob | Default | Effect |
| --- | --- | --- |
| `JIT` (Python, tinygrad's name) | on (`1`) | Enable the Python JIT capture/replay. `0` disables. |
| `THVM_JIT_NO_PIN` | off | Disable JIT buffer pinning. |
| `THVM_JIT_REPLAY_NOSKIP` | off | Disable replay skip optimization. |
| `THVM_JIT_REPLAY_PACK` | off | Experimental: pack replay dispatch records. |
| `THVM_PRECISE_FIRE_MEMO` | off | Precise dispatch-fire memoization. |
| `THVM_DISPATCH_TRACE` | off | Trace dispatch memoization. |
| `THVM_MAX_KERNEL_BUFFERS` | Metal 31, else no cap | tinygrad's `MAX_KERNEL_BUFFERS`: a positive value caps the distinct buffers (output included) one kernel may bind on every device, splitting wider elementwise fusions (`ru_limit_bufs_node`). `0` turns the cap off everywhere, Metal included. |

## Codegen tuning

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_TILE` | off | Tile-size tuning for the tiled codegen path. |
| `THVM_NO_FAST_IDIV` | off | A/B revert (read in `index_simplify.c`, not hand_opts): disable the fast_idiv/magicgu late-rewrite that lowers `x//c` -> `(x*m)>>s` at the C-render root (tinygrad's codegen/decomp/op.py). On = keep literal IDIV/IMOD. Upstream defaults the rewrite off (`DISABLE_FAST_IDIV=1`); thvm defaults it on for the C target. |

## Memory, GC & buffer management

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_GC_KB` | (impl) | Heap GC trigger threshold (KB). |
| `THVM_RECLAIM_MB` | 64 | py: free dead tensors' backend buffers once resident bytes exceed the last reclaim's survivors by max(this many MB, half the survivors); 0 reclaims after any growth. |
| `THVM_HEAP_CELLS` | (impl) | Initial heap cell count. |
| `THVM_MAX_BUF_BYTES` | (impl) | Max single-buffer allocation (KB). |
| `THVM_MAX_LIVE_BYTES` | (impl) | Max live-buffer memory. |
| `THVM_CUDA_REUSE_BUFS` | (impl) | CUDA-specific buffer reuse. |
| `THVM_METAL_REUSE_BUFS` | (impl) | Metal-specific buffer reuse. |

## CPU backend

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_THREADS` | (impl) | CPU worker-thread count. |
| `THVM_CPU_JIT_WARMUP` | 0 | The fire at which a kernel source first runs compiled: with W, fires 1 to W-1 run in the UOp walker and fire W compiles (0 and 1 compile on the first fire). The count gates every JIT path: the batched precompile at realize (`THVM_CPU_JIT_PARALLEL`), the in-memory compile cache and the dylibs cached under `/tmp`. A threshold no run reaches, `THVM_CPU_JIT_WARMUP=1000000000`, forces the walker for every kernel the JIT would take, however warm the caches are; kernels that match a BLAS pattern still dispatch to cblas unless `THVM_CPU_BLAS_DISABLE=1`. Read once per process. |
| `THVM_CPU_JIT_PARALLEL` | on | Batched cold-start precompile: at realize, every kernel whose next fire passes `THVM_CPU_JIT_WARMUP` compiles into one combined dylib before the dispatch fires it. `0` leaves each kernel to compile at its own fire. |
| `THVM_CPU_CC` | `$CC`, else clang / cc / gcc | C compiler for CPU JIT kernels (clang-only flags are added only for clang). |
| `THVM_CPU_JIT_VERBOSE` | off | Show the C compiler's diagnostics when a JIT compile fails (a failed kernel runs in the interpreter; thvm warns once). |
| `THVM_CPU_UOP_WALK_TRACE` | off | Trace each UOp-walk entry (lift outcome + root op). |
| `THVM_TRACE_RENDER_DECLINE` | off | Print why the C kernel renderer declined a kernel (overflow, untyped slot, or a body reading an unclaimed input slot), sending it to the CPU walker. |
| `THVM_TRACE_UOP_WALK_DECLINE` | off | Trace why the UOp walk declined a kernel. |
| `THVM_CPU_BLAS_DISABLE` | off | Disable the cblas dispatch path (force the generic walker). |

## CUDA backend

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_CUDA_FAST_MATH` | on | Enable PTX fast-math. |
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
| `THVM_DUMP_KERNEL_SRC` | off | Dump generated kernel source. |
| `THVM_DUMP_TILE_JIT_SRC` | off | Dump tiled-JIT kernel source. |
| `THVM_KCNT` | off | Kernel-count debug. |

## ATP (automated theorem proving)

The ATP engine + `bin/FindProof` CLI read well over 200 `THVM_ATP*` /
`THVM_FOL*` / `THVM_PROOFCHECK*` variables. Every STRATEGY knob is
discoverable via `bin/FindProof --list-knobs` (a 127-row
`--flag -> env -> Method-name` table -- each CLI flag is exactly an env
setter); the engineering-level subset is documented in
`docs/atp/engineering.md`. The rows below are the load-bearing
operational ones.

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_ATP_RSS_ABORT_MB` | 8192 (schedule slices: 4500) | Live-RSS GROWTH abort cap (MB, `0` disables): bounds the run's own RSS growth over the baseline sampled at its first step-0 poll, and the run degrades to a clean SZS `ResourceOut` instead of flooding the host. Growth-relative (not absolute) so a large-ambient host process -- WolframKernel after a cold-JIT `TNetTrain` sits at 8-9 GB for a while -- does not insta-abort every following proof (the silent post-train `$Failed` wedge). |
| `THVM_ATP_SCHED_MAX_PAR` | 5 | `--schedule` concurrency width (clamped to the slice count). The StarExec run script pins it to the core count. |
| `THVM_ATP_VERBOSE` | off | Restore engine progress chatter on the quiet competition path (same as `--verbose`). |
| `THVM_ATP_PROFILE` | off | Per-phase wall-time + counter breakdown at exit. |
| `THVM_ATP_DIAG` | off | Diagnostics level. |
| `THVM_ATP_TRACE_MAX` | (impl) | ATP proof-trace capacity. |
| `THVM_ATP_RULE_TRACE` | off | Print each rule as it is added (trajectory A/B debugging). |
| `THVM_ATP_GC_MB` | (impl) | Lower the ATP heap-GC trigger (MB) to collect more often. |
| `THVM_ATP_HEAP_ABORT_FRAC` | (impl) | Semi-space fraction past which the saturation aborts cleanly. |
| `THVM_ATP_WALDMEISTER` | off | Bench shorthand for the Waldmeister-preset runtime knobs (SelectionRatio 51, RHSInterreduce, UnfailingCP, lazy normalize, ...). |
| `THVM_ATP_SINE` / `_SINE_TOL` / `_SINE_GT` / `_SINE_RC` | on / (impl) | SInE axiom selection on oversized knowledge bases; `THVM_ATP_SINE=0` disables. |
| `THVM_ATP_ENIGMA_EXPORT` | unset | Path to a TSV: record per-CP features and, on PROVED, append proof-DAG-labelled rows (the ENIGMA training corpus). |
| `THVM_ATP_CP_DATASET` | unset | When set to a filename, capture the ENIGMA CP dataset (WL bridge). |
| `THVM_FOL_AGE_RATIO` | 5 | FOF clausal pre-pass pick-given interleave (1 pure FIFO, 0 pure best-first). |
| `THVM_FOL_GOAL_TIER` | on | FOF pre-pass conjecture-ancestry selection tier; `0` drops it. |
| `THVM_FOL_INDEX` | on | FOF pre-pass literal indexing (partner lists + subsumption buckets); `0` restores the full scans (byte-identical trajectory). |
| `THVM_FOL_AVATAR` | pre-pass on | AVATAR-lite variable-disjoint component splitting in the FOF clausal pre-pass; `0` restores the unsplit calculus exactly, `2` scopes guard spend to input + conjecture-descended clauses. Engine-level states created outside the pre-pass default off. |
| `THVM_PROOFCHECK_WALL` / `_CAP` / `_DEEP` | (impl) | Proof-checker (`--check` / CheckProof) wall, per-step budget cap, and 10x deep-validation mode. |
| `THVM_ATP_GOAL_DRIVE` | off | Refutational goal driver (`--goal-drive`): admit the ground negated goal as an ACTIVE backward condensed-detachment participant, regressing it through the raw ifeq-guarded equations interleaved with completion (unified-engine Phase 2, see `docs/plans/unified_automatic_engine.md`). Opt-in; a no-op unless the problem carries a detected `ifeq(A,A,B,C)=B` dispatcher. Sound (every closure is a ground instance of R union the dispatcher rules) but does not yet crack the deep '-10' CD gates -- the residual is goal-directed SELECTION quality. |
| `THVM_ATP_GOAL_DRIVE_STRIDE` / `_NODES` / `_DEPTH` / `_CELLS` / `_SIZE` | 100 / 15000 / 12 / 75%-of-heap / 6x-goal | Goal-driver budgets: rule-growth interleave cadence, per-run node cap, iterative-deepening depth cap, bump-heap cell cap, subgoal size ceiling. `THVM_ATP_GOAL_DRIVE_DEBUG` traces per-run variant/fact counts + the decomposed dispatcher rules. |

## Misc

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_BENCH_OPT` | off | Benchmark optimization selector (`tools/xbackend_bench`). |
| `THVM_WL_TENSOR_VIEW_DEBUG` | off | WL tensor-view debug output. |
