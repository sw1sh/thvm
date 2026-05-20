# Plan: Ideal Pipeline v2 — honest reset

## Why v2

`ideal_pipeline.md` (v1) was a sound design that **was not executed to completion**. The plan said Phase F enables Phase C enables Phase G. Phase F landed partially: `src/backend/cpu/op/*.c` was deleted in F6, `metal_try_conv2d_flat` / `metal_try_gemm` / `metal_jit_encode` were retired across F2-F4. But Phase C — the largest deletion, the whole point of the plan — never started. Phase G — the dead-code cull that Phase C enables — never started.

Consequence: the codebase now carries **both** the old representation (KProgOp, KernelEntry.program, scalar_uops arena, kernel_program_cache) **and** the new (UOp DAG via `compute_root`, render_uop). The two coexist; bufferize_classify, materialize, rangeify, kernel_lift have all grown ad-hoc rules to bridge them. Recent symptoms:

- `bufferize_classify.c` is 1879 LOC of named-rule plumbing — 11 rules in `bufferize_classify_run_rules`, each papering over a different shape the boundary heuristic gets wrong.
- `materialize.c` is 2862 LOC. Carries the `KernelEntry.program[]` emit path, the per-boundary visit() walker, AND the rangeify hand-off.
- `rangeify.c` is 4157 LOC, half of which is the **legacy composers** v1 explicitly listed for B3-finish deletion.
- `kernel_lift.c` is 1908 LOC, doing a job (UOp→scalar lift) that tinygrad does in `run_rangeify` as part of boundary decision.
- `KernelEntry` carries BOTH `program[]` (KProgOp array) AND `scalar_uops[]` AND `compute_root` (UOp Term). Three representations of the same thing.

This duplication is the root cause of recent failures:
- The W2-grad-through-BN1-backward bug (`Missing[NotATensor, UOP]`) traced to a 64-slot `abs_locs[]` overflow in a NAMED RULE that wouldn't exist in a tinygrad-faithful pipeline.
- The 597-kernel explosion for one W2 gradient: bufferize_classify's `consumer_count >= 2 → realized` seed plus the named-rule unmarking is much weaker than tinygrad's reverse-topo `run_rangeify` walk.
- The Metal `metal-tile-fanin-cap` rule is an Apple arg-count guard placed in the boundary-decision layer; tinygrad keeps that concern in Metal codegen.

The v1 plan blamed "multi-session investment" for the unlanded phases. The honest cause is: Phase C is invasive and the pressure to ship features kept producing band-aids in the named-rule layer instead. Each band-aid made the eventual Phase C harder. The named-rule pipeline metastasized.

## What v2 does differently

1. **No "gated on" sequencing that delays the central change.** Phase C is the central change. It happens first. Everything else falls out of it.
2. **No promised LOC deletions to motivate.** The deletions happen as a CONSEQUENCE of replacing the named-rule + program[] + scalar_uops stack with the unified pass. Don't count them in advance.
3. **No rule-by-rule porting of `run_rangeify`.** The prior session tried that; the agent correctly identified that `run_rangeify` is one imperative walk, not a pattern-matcher of independent rules. Port it as a walk.
4. **Tinygrad source is the spec.** Every concept added cites `tinygrad/.../X.py:line` in the commit message. If a port runs into a thvm data-structure gap, extend the thvm data structures to match tinygrad — do NOT approximate.
5. **No tile.c band-aid carryover.** `tile.c`, `tile_anno.c`, `uop_to_scalar.c`, `kernel_program_cache.c`, the named-rule pipeline, scalar_uops, KProgOp — all on the cull list. None get "kept for compatibility" past the migration window.
6. **The MNIST training perf target is the only acceptance signal.** Kernel count under ~100 per training step at BS=15 (tinygrad's beautiful_mnist is in that range). No "make tests pass" gate alone; we already have all-green tests with 597 kernels for one W2 grad.

## The unified pipeline (target shape)

```
TENSOR-LEVEL UOP DAG (TAG_UOP, opcodes ADD/MUL/REDUCE/RESHAPE/EXPAND/...)
        |
        v
[unified rangeify pass]   <-- port of tinygrad run_rangeify
        |   - reverse-topo walk
        |   - per-node consumer_map (list of consumer locs)
        |   - per-node out_rngs (RANGE values from each consumer)
        |   - per-node ending_ranges (RANGEs killed by EXPAND in consumer chains)
        |   - emits: (boundary realize-map, BUFFERIZE/INDEX/RANGE/STORE rewrite)
        v
LOWERED UOP DAG  (boundaries are BUFFERIZE; addresses are INDEX of RANGE)
        |
        v
[backend dispatch]
        |   - CPU: cg_render_uop_kernel_c -> clang JIT (already landed)
        |   - Metal: cg_render_uop_kernel_metal -> render_uop.c (already landed)
        v
KERNEL DISPATCH
```

No intermediate `KProgOp[]`. No `scalar_uops[]`. No `kernel_program_cache`. No 11 named bufferize rules. `KernelEntry` collapses to `{cached_lift.store_root, output_tid, n_inputs, input_*, view info}` (the redundant `compute_root` view of the same Term was retired post-Phase 4b).  `materialize.c` walks the lowered UOp DAG and emits one KernelEntry per BUFFERIZE; `kernel_lift.c` shrinks to a thin store-root lookup + KernelUopLift packager.

## Phases (no time bounds, dependencies only)

### Phase 1: Substrate for the unified pass

Goal: make the data structures the tinygrad walk needs available on the tensor-level UOp heap.

- 1a. Add `consumer_map`: per UOp loc, list of consumer UOp locs. Currently `BUFFERIZE_NODES[i].consumer_count` is an integer; we need the list. Source: `tinygrad/schedule/indexing.py:155-160` (where `cmap` is built).
- 1b. Verify `UOP_RANGE` allocatable at tensor-level phase: `uop_range(axis_id, axis_type, extent)` in `src/uop/index.c` is already phase-agnostic (raw `heap_alloc(3)` + hash-cons through `uop_mov_cache`, no caller gate). KAX_LOOP/REDUCE/GLOBAL/VIRT match tinygrad's AxisType enum (KAX_VIRT == AxisType.PLACEHOLDER per thvm.h:920). **No new helper.** Phase 1b reduces to a verification commit; substrate gap closes in 1c via the first non-kernel-lift caller (apply_movement_op).
- 1c. Port `apply_movement_op` (tinygrad/schedule/rangeify.py:`apply_movement_op`) for SHRINK/PERMUTE/FLIP/EXPAND/PAD over RANGE values. RESHAPE deferred to Phase 2 (needs `pm_simplify_valid`).
- 1d. Port `pm_simplify_valid` and `pm_drop_and_clauses` (tinygrad/uop/symbolic.py). These canonicalize `% / //` expressions; required for RESHAPE's index decomposition.
- 1e. Verify: existing tests all green. Substrate is additive; nothing else uses it yet.

### Phase 2: The unified rangeify pass

Goal: implement the tinygrad walk and emit the lowered UOp DAG. The OLD path (bufferize_classify named rules + materialize.c visit() + kernel_lift) stays in place but unused.

- 2a. Implement `run_rangeify_unified` (new function in a new file, `src/schedule/rangeify_unified.c`). 1-to-1 port of `tinygrad/schedule/indexing.py:run_rangeify` lines 148-269. Reverse-topo walk over BUFFERIZE_NODES; per-node out_rngs / ending_ranges; realize decision per the consumer-divergence and ending-ranges conditions.
- 2b. Implement `pm_apply_rangeify` (1-to-1 port of `tinygrad/schedule/indexing.py:pm_apply_rangeify`). Replaces each node's src with the BUFFERIZE/INDEX expression built from `range_map`.
- 2c. Gate the unified pass behind `THVM_UNIFIED_RANGEIFY=1` (default 0). All existing tests run on the OLD path; the new path is exercised by a single new test that builds a small graph and verifies the lowered DAG shape.
- 2d. Verify: existing tests still all green (OLD path); the new test passes.

### Phase 3: Cut over

Goal: flip the default. The unified pass becomes the only path; OLD path becomes dead code.

- 3a. Flip the default to `THVM_UNIFIED_RANGEIFY=1`. Run the full suite. Bisect any regression.
- 3b. Verify the BN-train backward kernel count drops to within tinygrad parity for the equivalent topology. probe_w2_bs3 from current 597 → target under 100; probe_w2_noBN from 47 → unchanged or lower.
- 3c. If the cut-over reveals a gap (something the OLD path handled that the new one doesn't), pick ONE specific case, port the corresponding tinygrad rule, repeat. Each round commits independently. NO bundling.
- 3d. Verify: full suite all green, kernel counts within tinygrad parity bands.

### Phase 4a-pre: UOP_BUFFERIZE first-class + main-heap mutation (substrate) — LANDED 2026-05-13

Substrate that unblocks Phase 4a's REDUCE-seed removal. All 5 sub-steps committed; full suite green; probe_w2_bs3 dropped from 597 → 543 kernels (additive substrate; main reduction comes when Phase 4a drops the REDUCE seed and Phase 4d folds materialize.c onto the lowered DAG).

Why this exists: Phase 2 wrote the unified pass output to side-tables
(`RU_RANGE_MAP` / `RU_REALIZE_MAP` / `RU_SUBST` in
`src/schedule/rangeify_unified.c`). The Phase 3a cut-over wiring activated
the pass as an additive seed contributor but did not mutate
`BUFFERIZE_NODES.realized`. Phase 4a wants to REMOVE the OLD-path
REDUCE-as-boundary seed in `bufferize_classify.c`; doing so without a
new boundary contract breaks `materialize.c` (which walks the OLD-path
realized bits + emits one `KernelEntry` per realized REDUCE).

Phase 4a-pre lands the new contract:

- 4a-pre-1. **UOP_BUFFERIZE opcode + main-heap allocator.** Add
  `UOP_BUFFERIZE` to the opcode space and `uop_bufferize_new(value,
  addrspace, removable, n_ranges, ranges)` to `src/uop/index.c` (hash-cons
  via `uop_mov_cache`). Accessors: `uop_bufferize_value/_addrspace/
  _removable/_n_ranges/_range_at`. Mirror: tinygrad/schedule/indexing.py:77.
  **LANDED.**
- 4a-pre-2. Unified pass writes UOP_BUFFERIZE on main heap at realize
  boundaries (replaces `RU_SUBST` side-table writes). Mirror:
  `create_bufferize_and_index_based_on_ranges`.
- 4a-pre-3. REDUCE-via-RANGE production: rewrite `REDUCE(op, axis)` as
  `REDUCE(op)` with explicit RANGE args. Mirror:
  `convert_reduce_to_reduce_with_ranges`.
- 4a-pre-4. **Project UOP_BUFFERIZE → BUFFERIZE_NODES.realized.** Additive
  projection via `bufferize_classify_project_unified` after
  `run_rangeify_unified`; new `BUFFERIZE_REASON_UNIFIED` flag tags
  unified-pass boundaries. `materialize.c` reads `.realized` unchanged.
  OLD seeds (multi-consumer / REDUCE / matmul) stay; dropping REDUCE
  here broke MaxPool/Softmax/BN-train/CE (Phase 4a's job). **LANDED**
  (commit `72ad620d`).
- 4a-pre-5. **Re-probe.** probe_w2_bs3: 597 → 543 kernels; target <100
  not reached at substrate level (additive). **LANDED.** Target 100
  blocked on Phase 4a (REDUCE seed drop) + Phase 4d (materialize.c
  walks lowered DAG directly, not projected `.realized` bits).

### Phase 4: Delete the OLD path — DIRECT default ON, all suites green (2026-05-14)

Goal: remove the dead code that the unified pass replaced. Done iteratively: scaffolding behind the `THVM_RANGEIFY_DIRECT` env gate which is now default ON (commit `7ba967b8`). Legacy path stays reachable via `THVM_RANGEIFY_DIRECT=0` until 4b/4c/4d/4e land.

Status under default (`THVM_RANGEIFY_DIRECT=1`):

- **4a. Named removal rules**: 9 of 10 disabled (commits `3ceced8b` + `f5eefed0`). `inline-softmax-broadcast-reduce` stays - confirmed empirically that dropping it regresses `fusion_count.wlt` (8/8 → 7/8), so unified pass still doesn't sharpen softmax max→exp→sum fusion. Phase 5c follow-up: port the corresponding tinygrad sharpening from the softmax pattern matcher.
- **4d-final. REDUCE seed drop**: matmul-only (commit `7896878f`); other REDUCEs fuse inline via the unified pass + render_uop's `_accN` accumulator hoist.
- **REDUCE chain-guard**: disabled - always-keep non-matmul REDUCE as a boundary (commit `eec25fb1`). The cmap-based chain-guard missed inter-reduce iter dependencies in probe_w2's larger backward graph, regressing W2 grad to 0. Always-keep trades ~13% kernel-count reduction (549 vs 477) for correctness. The real fix path is render_uop's nested-reduce-iter handler.
- **DIRECT default flipped** to ON (commit `7ba967b8`). All 8 standard suites green: nn 55/55, grad 62/62, bn_grad 3/3, conv_im2col 6/6, fusion_count 8/8, grad_edge 11/11, core 39/39, assign 6/6. probe_w2_bs3 produces correct grad (absmean 0.329054).

Remaining 4b/4c/4d/4e/4f deletions are now unblocked but require multi-file C surgery across the scheduler + backends:

- 4b. Delete `KernelEntry.program[]` / KProgOp / `kernel_program_cache.c` / `uop_to_scalar.c`. The plan's earlier claim "Backends already consume `compute_root` post-F0 (`render_uop.c:1099`)" is empirically PARTIAL: `src/backend/cpu/jit.c:100-101` still hashes `ke->program` bytes as the JIT cache key and `jit.c:318` reads `ke->program[ke->n_ops-1].numel`, both firing when `cached_lift.store_root == 0` (lift declined: gemm/conv2d-only kernels, multi-output splices, oversized n_inputs). 219 reads of `ke->program` / `n_ops` / `ops_cap` survive in 17 files post-4b/2. Decomposition required (2026-05-14 reconnaissance):
    - **4b/2 LANDED (commit `4a7431b7`)**: deleted `kernel_program_cache.c` (-413 LOC net). `kernel_program_key` / `kernel_rangeified_key` migrated to static helpers in `codegen/autotune.c` to preserve on-disk cache key stability; `capture.c` and `thvmlink.c` stub to 0 (their callers were already in `program_shared==1` / `schedule != _local_schedule` branches that the cache deletion makes unreachable). Also removed `KernelEntry.program_shared` field. All 8 suites green under DIRECT default.
    - **4b/3.a (LANDED across A1.1-A1.6 sub-steps, 2026-05-14)**: the ScalarUop arena (`KernelEntry.scalar_uops` / `n_scalar_uops` / `scalar_uops_cap` + 37-member `ScalarOp` enum) had FIVE direct readers across 293 references in 17 files. A1.1 through A1.6 closed the dispatcher reader (`cpu_dispatch_scalar`). Arc:
        - **A1.1 (commits `c92b2e21` + `09e014d6`)**: walker decline-reason telemetry under `THVM_TRACE_UOP_WALK_DECLINE`; cataloged the 19 hits.
        - **A1.2 (commit `ae0b299c`)**: two new `kernel_lift.c::lift_scalar_index` fast paths (broadcast-leading-iter + flatten-iter on low-ndim bufs). Closed 12 of 19 declines.
        - **A1.3 (commit `8165af67`)**: 83 `lift_reject_log` instrumentation sites added across all previously-silent fail paths in `kernel_lift_to_uop`; the remaining 17 declines now fully classified.
        - **A1.4 (commits `012464e7` + `6291674a`)**: two real leaf bugs fixed - FP32 S_IWHERE misrouting (split out of int-only `scalar_to_uop`; lift y/n via `lift_scalar_value`) and S_RESHAPE_V size-1 folded-S_ICONST out_ext inference.
        - **A1.5 (commit `ee882fe5`)**: final leaf - relax legacy S_RESHAPE handler from `out_numel != in_numel` to `out_numel > in_numel` (grad-through-EXPAND emits stride-0 broadcast reads where `out_numel < in_numel`).
        - **A1.6 (commit `3e837817`)**: `cpu_dispatch_scalar` deleted (-92 LOC in `src/backend/cpu/interpret.c`). All 8 suites 190/190 green; `THVM_TRACE_UOP_WALK_DECLINE` 0 hits across the run.
    - **A1.7 (commit `5f881562`)**: capture.c decoupled (-95 LOC). 3 helpers deleted (`jit_scalar_index_param_slot`, `jit_scalar_same_index_coord`, `jit_kernel_store_index`); `jit_assign_sink_safe` stubbed to return 0 (Metal AOT optimization disabled until ported on UOp DAG); `n_scalar_uops` export replaced with 0.
    - **A1.8 (commit `6a01b936`)**: propose.c decoupled (-120 LOC). Stubbed `propose_scalar_reduce_axis_size` + `propose_metal_tile_scalar_reduce_kernel`; dropped arena guards in `propose_metal_tile_kernel`; deleted `propose_scalar_op_carries_kernel_dtype` + `propose_metal_tile_scalar_reduce_op_ok`.
    - **A1.9 (commit `bf129d3d`)**: axis.c stubbed `axes_scalar_reduce_extent`.
    - **A1.10 (commit `57dfd76e`)**: autotune.c dropped scalar_uops byte-hash from cache key + structural keys.
    - **A1.11 (commit `b3ece1e9`)**: metal/_.m routed kvar collection + PSO hash through cached_lift.store_root.
    - **A1.12 (commit `f60b5307`)**: kvar.c deleted `kvar_collect_from_scalar` (no callers left).
    - **A1.13 (Phase 4e-1, commit `fa7b395f`)**: `cpu_dispatch_tile` + ScalarUop interpreter helpers (`eval_scalar`, `eval_index`, `eval_iter_ref_extent`, `ScalarCtx`, `scalar_load_typed`, `scalar_store_typed`) deleted from `src/backend/cpu/interpret.c` (-909 LOC net across 5 files; interpret.c shrunk 1073 -> 198 lines). `tests/test_tile_graph.c` retained (its primary subjects are tile-plan arena lifecycle, conv2d analysis, kernel-axes seeding — not the deleted interpreter).
    - **A1.14 (Phase 4d-partial)**: `kernel_lift.c` ScalarUop walker deleted. Dispatcher counter trace under `THVM_DUMP_LIFT_DISPATCH` (added during this arc and removed in the same commit once read) proved both `kernel_lift_from_kprog` (n_extra_outputs>0 branch) and `kernel_lift_from_conv2d` (empty-scalar-arena branch) fired 0 times across the 8-test suite (177 tests) AND bench-train BS=15 14-param backward (2885 kernels). `kernel_lift.c` shrunk 630 → 105 LOC; the slim `kernel_lift_to_uop` now does only the unified short-circuit (look up rangeify_unified store_root, package as KernelUopLift). `uop_to_scalar.c` was already retired in a previous sweep (no longer in tree). The arena writer (`rangeify.c`) remains as test scaffolding for test_kernel_lift / test_metal_real / test_tile_graph; those tests cover dead paths and pre-date this commit (10/44, 5/7, 81967/81987 fails baselined at HEAD).
    - **A1.15-A1.17 (Phase 4d-followups)**: probe + delete sweep over `ke->program` / `ke->n_ops` readers that gate on `cached_lift.store_root == 0`.  Each path was probed under an env-gated fprintf; zero hits across the standard 13 WL test files (239 tests).  Landed deletions:
        - `src/codegen/render_metal.c`: two dead `kernel_lift_to_uop(ke, &fresh)` test-infra fallbacks (-33 LOC, commit `40f03175`).
        - `src/interact/uop_grad.c`: `grad_kernel_backprop` + 6 KProgOp-walk helpers; both call sites already had `KERNELS[kid].source_uop` primary path (-313 LOC, commit `cd702905`).
        - `src/codegen/propose.c`: `propose_kprog_reduce_axis_size` + the per-op KProgOp gate inside `propose_metal_reduce_unroll_kernel` (-56 LOC, commit `faa1ea23`).
        - `src/schedule/tile.c`: legacy program[] fallback branch inside `tile_analyze_conv2d_flat` + `TILE_CONV2D_FLAT_LEGACY` counter + accessor.
        - `src/schedule/kernel_lift.c`: dispatcher trimmed to the unified short-circuit (-555 LOC, commit `80ce86d7`).
    - **4b/3.a remaining (multi-session)**: two ScalarUop arena readers still alive and block field deletion:
        - `src/schedule/rangeify.c` (125 LOC) - arena bookkeeping only (rangeify_reserve / rangeify_emit*). Used by test scaffolding + `thvm_wl_kernel_scalar_uops` (WL introspection bridge). Plan: delete dead unit tests + bridge, then this file.
        - `thvm_wl_kernel_scalar_uops` - WL introspection of the arena. Will go with the unit tests once the WL-side dependency is mapped.
    - **4b/3.b probe pattern**: env-gated `fprintf` at a candidate dead reader, run targeted WL suite, delete if 0 hits.  Per-iteration deletions cleaning the `cached_lift.store_root != 0` "DAG primary, KProgOp fallback" pattern:
        - `src/codegen/cg.c` (commit `ea8a8034`): -123 LOC.  `cg_supports` short-circuits on store_root; the post-short-circuit per-op opcode allowlist + uniform-dtype + float-only-on-int gates are dead.  Removed cg_program_dtype + cg_src_numel + cg_op_is_float_only + cg_dtype_supported helpers.
        - `src/codegen/axis.c` (commit `cee8409c`): -238 LOC.  axes_will_have_reduce_axis, axes_compute_axis_types, axes_compute_full_shape each had "DAG path returns" + "fall-through replay" shape.  Replay paths walked applied_opts + ke->program tail-REDUCE; all dead under unified pass.  Removed axes_scalar_reduce_extent stub + axis_kop_to_axis_type helper.
        - `src/backend/cpu/jit.c` (commit `4467d654`): -21 LOC.  Cache-key byte-hash else branch + output-numel last-op fallback.
        - `src/uop/dag_scan.c`: comment-only references to `THVM_PHASE_C7_FREE_PROGRAM` flag (which is no longer the gating condition); scrubbed to describe current state.
    - **A1.23-A1.25 (Phase 4d-followups continued)**: collapse the "program[] primary, DAG fallback" classifiers to DAG-only now that THVM_PHASE_C7_FREE_PROGRAM=1 (default) frees program[] post-lift, making the program[] branch unreachable.  Landed deletions:
        - `src/codegen/profile.c`: `cg_kernel_flops` 32-line per-op accumulator (commit `952efd46`).
        - `src/jit/capture.c`: `jit_capture_kernel_op_count` program[] early-return (commit `952efd46`).
        - `src/codegen/autotune.c`: `kautotune_program_key` byte-hash + structural_key program[] branch (commit `cee6a983`).
    - **A1.26 (no-op stub deletion)**: `axes_default_for` + `axes_ensure_scalar_reduce` were both `(void)ke;` stubs once the DAG-side resolvers took over.  Deleted both + 5 call sites (materialize.c, tile_anno.c, 3 metal test files) + thvm.h decls + the "writer trio" prose (commit `379dd0a1`).
    - **Remaining `ke->program` reader files (5)**: writers + internal materialize-time readers (materialize.c, kernel_alloc.c), GC state check (kernel_gc.c), diagnostic logging (uop_walk.c).  Retiring these requires the materialize.c writer itself to be refactored -- visit() walks the source UOp tree building KProgOp entries as intermediate state, then materialize frees the array.  kernel_lift_to_uop doesn't read program[] (only ke->source_uop), so the writer's output is consumed only by the writer itself.  Open question: can visit() be rewritten to track dtype/numel via term_dtype_in / term_shape_in on the visited UOp instead of via prior-KProgOp slots?
    - **visit() refactor LANDED (commit `2122638d`)**: -319 LOC.  Replaced visit()'s per-opcode KProgOp emitter branches with a recursive walk that propagates VISIT_BAIL, populates input slots via input_slot_dedup, and stores a VISIT_OK sentinel in the memo.  Helpers prog_chain_propagate / prog_chain_break / src_dtype / src_numel / op_is_chain_movement deleted along with the post-lift free block.  239/239 green; 4 pre-existing test_bufferize / test_materialize_v2 failures unchanged.
    - **Phase 4b/3.b LANDED (commit `3c9d9595`)**: -774 LOC across 28 files.  Deleted `KernelEntry.program` / `n_ops` / `ops_cap` fields, the entire `KProgOp` struct (incl. `KPROG_INIT_OPS` / `_MAX_OPS` macros), `kernel_program_reserve`, the Metal per-op KProgOp interpreter loop (~200 LOC inside metal_dispatch_kernel), `metal_encode_op`, THVM_DUMP_KID_PROGRAM diagnostic dump, kernel_entry_prog_chain_op stub, `splice_child_into_host_premerge`'s `n_ops != 0` guard, and the test_metal_real KProgOp-fixture blocks.  WL bridge `thvm_wl_kernel_info` returns empty `"program"` always.
- **Phase 4c LANDED (commit `41b3f466`)**: -1705 LOC across 35 files.  Deleted the entire ScalarUop arena: `src/schedule/rangeify.c` file (arena bookkeeping), `KernelEntry.scalar_uops` / `n_scalar_uops` / `scalar_uops_cap` fields, the `ScalarUop` struct + 37-entry `S_*` enum, `SCALAR_MAX_SRC` / `SUOP_*` macros, `rangeify_emit*` / `rangeify_reserve` / `rangeify_free` / `scalar_op_name` forward decls, the WL bridge `thvm_wl_kernel_scalar_uops` + `TKernelScalarUops` paclet symbol, four dead unit tests (test_kernel_lift, test_kernel_lift_coverage, test_metal_pso_cache, test_metal_variable_pso_hit), and two stale .wlt files (lowering.wlt, rangeify_gaps.wlt).  239/239 green; test_metal_real now 81904/81904 (was 81967/81987 fail at HEAD baseline).
- **Phase 4e-2 LANDED (commit `aa6b6a84`)**: `tile_build_from_scalar` + 13 obsolete tile_uops test cases deleted (-670 LOC). `tile_uops.wlt` retired (-286 LOC, commit `00728dfc`). `tile_sync_from_scalar` no-op stub + all 8 callers deleted (-18 LOC, commit `728ebe1c`).
- **Phase 4f LANDED (commit `5b99749d`)**: THVM_RANGEIFY_DIRECT=0 legacy path retired (-1255 LOC, 9 named bufferize rules + ~30 helpers).
- **Phase 5c LANDED (commit `099cbbea`)**: `inline-softmax-broadcast-reduce` moved inline pre-seed; the post-pass rule + re-run gone.
- **Phase 4f-2 LANDED (commit `a5b6de98`)**: THVM_UNIFIED_RANGEIFY=0 OLD path retired (-121 LOC, `bufferize_rangeify_enabled` gate + OLD-else body).
- **Phase 4f-3 LANDED (commits `320b9253` + `dd653362` + `7b9c1f70`)**: bufferize_rewrite_apply harness retired, attribution helpers + dead reason flags removed (-137 LOC), test_bufferize_classify cleaned to 29/29 green (commit `76f99d1c`, -9 obsolete tests).
- **Bypass-arc bn_grad regression LANDED (commit `e93e0408`)**: under `THVM_LIFT_FROM_UNIFIED=1` the 3 bn_grad finite-diff tests went 0/3 -> 3/3 (and nn.wlt +8) via two fixes: (1) `rangeify_unified.c` strips the movement-op shell (RESHAPE/PERMUTE/EXPAND/PAD/SHRINK/FLIP) from `RU_STORE_ROOT.value` so cpu_uop_walk's value layer (no movement-op handler) sees the indexed read; (2) `materialize.c` adds an `INDEX_E(BUFFERIZE(CONST), _) -> CONST` collapse and a per-kernel safety gate (`uop_subtree_has_residual_bufferize`) that leaves the legacy `store_root` on kernels where the rewriter can't lower every BUFFERIZE -- other kernels still get the bypass.  ~203 tests still fail under bypass (separate triage task).
    - **4b/3.b**: delete `KernelEntry.program` / `n_ops` / `ops_cap` field. Only after 4b/3.a; verify by stubbing `ke->program = NULL` and re-running the 8 suites.
    - **4b/3.c**: delete `KProgOp` struct (49 hits in `rangeify.c` + `materialize.c` + `thvm.h`; fields `chain_op_idx`, `chain_edge_idx`, `chain_input_slot`, `source_uop`, `store_extra_plus_one` route the bufferize-chain wedge, multi-output splice, and `uop_grad`'s numel recovery).
- 4c. **LANDED**: see `Phase 4c LANDED` line above for the full sweep.
- 4d. Delete `kernel_lift.c` standalone lifting.  Largely done: the slim file (102 LOC) now does ONLY the unified-store-root lookup + KernelUopLift packaging + global counter accessors.  The counters survive because render_metal.c (which is `#include`d before this TU in the unity build) bumps them on cg_emit_via_uop entry/exit and thvm.c reads them for THVM_DUMP_LIFT_COVERAGE logging.  Full retirement requires moving the counters into render_metal.c or restructuring the unity-build include order.
- 4e. Delete the standalone tile representation: `tile_anno.c`'s KOpt struct, `apply_opt.c`'s KernelAxes mutation path, `propose.c`'s reads from the side struct. The tile.c IR concepts MOVE — to first-class UOp ops in the unified DAG (`UOP_MULTI`, `UOP_MSELECT`, `UOP_MSTACK`, `UOP_DEVICE_NUM`, `UOP_ALLREDUCE`) per tinygrad's MULTI direction and tilelang's Fragment correspondence. The matmul/conv/reduce template recognizers move to UPatRule (already planned in v1 Phase E) consulting the unified DAG.
- 4f. Delete `THVM_UNIFIED_RANGEIFY` + `THVM_RANGEIFY_DIRECT` env gates. Plain code.

### Phase 5: Move misplaced concerns to their proper layer

Goal: dropped named-rules whose concerns are real but were in the wrong layer.

- 5a. `metal-tile-fanin-cap`: an Apple-Metal arg-count guard. Moves to Metal codegen / dispatch (split a kernel into multiple dispatch calls if arg count would exceed the limit). Tinygrad does this in its Metal renderer via `arg_buf` indexing; port that.
- 5b. `matmul-protect`: a GEMM-shape recognizer that wants to keep elementwise off the matmul kernel. Becomes a UPatRule that emits `UOP_OPT(_, TC)` on the matmul-shape root (v1 Phase F3 already partially landed; finish). The recognizer reads the unified UOp DAG (with Phase 4e's tile-primitive ops landed) rather than the old side struct.
- 5c. `inline-softmax-broadcast-reduce` and friends: these were band-aids for the old boundary heuristic. Verify the unified pass handles their cases natively (it should, per tinygrad parity for softmax / BN). If not, file specific gaps as separate ports from tinygrad source.

## Tile-primitive correspondence (target for Phase 4e + 5b)

Picture the user posted (2026-05-13): the new tinygrad ops are 1-to-1 with tilelang's Fragment primitives. Phase 4e + 5b port thvm's tile.c IR concepts into the unified UOp DAG as these ops:

| New tinygrad / thvm UOp | Tilelang Fragment |
| --- | --- |
| `.device : tuple[str,...]`           | `local.fragment` scope on a buffer |
| `Ops.MULTI(src, axis)` marker         | `LayoutInferencePass` attaching a Fragment to the buffer |
| `_device_num` symbolic Variable       | implicit thread var inside `forward_thread` |
| `SHRINK` with `_device_num*sz` bounds | `forward_index` → per-thread register slice |
| Lowered by `multi_pm` to per-device shape | `lower_tile_op.cc:36` rewrites buffer to per-thread `OutputShape()` |
| Tuple device + no MULTI = replicated  | `Fragment::FullyReplicated` (`layout.cc:843`) |
| `MSELECT(x, i)` picks i-th device buffer | indexing into a single thread's local buffer |
| `MSTACK` combines per-device buffers   | inverse layout / cross-thread gather |
| `Ops.ALLREDUCE` (`schedule/allreduce.py`) | warp shuffles emitted by `reduce.cc:243` |

Thvm-side ops to add (Phase 4e): `UOP_MULTI`, `UOP_DEVICE_NUM`, `UOP_MSELECT`, `UOP_MSTACK`, `UOP_ALLREDUCE`. `SHRINK` already exists in the rangeify substrate (Phase 1c). The `multi_pm`-equivalent rewrite lives next to the unified rangeify pass (Phase 2).

## Honest carry-over from v1

What v1 promised that v2 inherits:
- Phase F: dispatch ladder collapse — DONE (cpu/op/*.c deleted, metal_try_gemm/conv2d_flat/metal_jit_encode retired). render_uop is the only renderer.
- Phase F3/F4: matmul + conv2d UPat recognizers — PARTIAL. matmul recognizer landed; conv2d not yet. Phase 5b inherits this.

What v1 promised that v2 disowns:
- "Phase E apply_opt -> UPatRule port" as a parallel track. Realistically this is mostly subsumed by Phase 5b. Treat it as a follow-up after the central cut-over, not a parallel commitment.
- "Net deletion estimate ~15K LOC" — useful-sounding number, not actionable. Deletions happen because the unified pass replaces them; counting LOC was motivational fluff in v1 and produced no urgency.

## Tests / verification gates

- Per-commit: `make wl`, `wolframscript -f /tmp/run_nn_wlt.wls` 55/55, `wolframscript -f /tmp/run_suites.wls` (grad, bn_grad, conv_im2col, fusion_count, grad_edge, core, assign) all green.
- Per-phase: probe_w2_bs3 kernel count tracked (current 597); probe_w2_noBN kernel count tracked (current 47). Phase 3 hard gate: probe_w2_bs3 under 100.
- Final gate: beautiful-mnist BS=15 N_STEPS=3 trains; loss descends; total kernel count per step within 2x of tinygrad's equivalent.

## Things NOT promised

- No deadlines.
- No multi-session promises beyond the dependency chain above.
- No claims about which session a given phase lands in. The phases are sequenced by dependency, not calendar.
- No "deletes 15K LOC" headline. Deletions are a side effect, not the goal.
- No "kernel count drops 10x" — we measure and report; the target is tinygrad parity, not a multiplier.

## User-decided constraints (2026-05-13)

1. **Everything can break during migration.** No "keep Metal working through the cut-over" carve-out. If Phase 3 regresses the suite, that's fine — fix it in the same or following commits, don't gate the cut-over on backward compatibility. Land the plan.
2. **`tile.c` primitives stay as UOp ops — only the standalone IR layer retires.** The tile primitives (Fragment, Layout, per-thread/per-device slicing, ALLREDUCE) ARE the right target; tilelang has them and tinygrad is moving toward them (`Ops.MULTI(src, axis)` marker + `_device_num` symbolic Variable + `SHRINK` with `_device_num*sz` bounds + `MSELECT`/`MSTACK`/`Ops.ALLREDUCE` per `tinygrad/schedule/allreduce.py`, mirroring tilelang's `local.fragment` / `LayoutInferencePass` / `Fragment::FullyReplicated` / warp shuffles in `reduce.cc:243`). What was wrong in thvm: the parallel data-structure layer — `tile_anno.c`'s standalone KOpt struct, `apply_opt.c` mutating `KernelAxes` outside the UOp DAG, `propose.c` reading the side struct — duplicated concepts that should live as first-class UOp ops on the main heap. Phase 5b ports tile.c's IR concepts INTO the unified UOp DAG as `UOP_MULTI` / `UOP_MSELECT` / `UOP_MSTACK` / `UOP_DEVICE_NUM` / `UOP_ALLREDUCE` (1-to-1 names following tinygrad's MULTI direction), then deletes the standalone KOpt/tile_anno/apply_opt-side-struct layer. The matmul/conv pattern recognition itself stays (via UPatRule); it just consults the unified UOp DAG instead of the side struct.
3. **Unified pass rewrites the main heap.** UOP_RANGE / UOP_INDEX_E / BUFFERIZE nodes land in the main tensor-level heap directly, not in a side-table. The heap already has these opcodes (currently used only post-kernel-lift); the unified pass makes them first-class at the tensor-level phase. `BUFFERIZE_NODES` side-table becomes vestigial in Phase 2 and deletes in Phase 4.
