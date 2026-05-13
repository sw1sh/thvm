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

No intermediate `KProgOp[]`. No `scalar_uops[]`. No `kernel_program_cache`. No 11 named bufferize rules. `KernelEntry` collapses to `{compute_root, output_tid, n_inputs, input_*, view info}`. `materialize.c` walks the lowered UOp DAG and emits one KernelEntry per BUFFERIZE; `kernel_lift.c` deletes (its job is folded into the unified rangeify pass).

## Phases (no time bounds, dependencies only)

### Phase 1: Substrate for the unified pass

Goal: make the data structures the tinygrad walk needs available on the tensor-level UOp heap.

- 1a. Add `consumer_map`: per UOp loc, list of consumer UOp locs. Currently `BUFFERIZE_NODES[i].consumer_count` is an integer; we need the list. Source: `tinygrad/schedule/indexing.py:155-160` (where `cmap` is built).
- 1b. Make `UOP_RANGE` allocatable at the tensor-level heap phase (not just inside kernel-lift). RANGE carries `(idx, AxisType)` per tinygrad `UOp.range`. Add C helper `uop_range_new(idx, axis_type, extent)`.
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

### Phase 4: Delete the OLD path

Goal: remove the dead code that the unified pass replaced. Each commit is a single targeted deletion; reverting any one of them brings back working old-path code (gated behind THVM_UNIFIED_RANGEIFY=0).

- 4a. Delete `bufferize_classify`'s 11 named rules (`inline-constants`, `inline-adjacent-reduce-chains`, ..., `metal-tile-fanin-cap`). The boundary-decision side-table flag stays; only the rule-based mutations go.
- 4b. Delete `KernelEntry.program[]` / KProgOp / `kernel_program_cache.c` / `uop_to_scalar.c`. Backends already consume `compute_root` post-F0 (`render_uop.c:1099`).
- 4c. Delete `KernelEntry.scalar_uops` / ScalarUop arena / S_* opcodes. The unified pass produces the lowered UOp DAG directly; no separate scalar arena needed.
- 4d. Delete `kernel_lift.c` standalone lifting. The unified pass produces the lowered form during boundary decision; there's nothing to lift after.
- 4e. Delete the standalone tile representation: `tile_anno.c`'s KOpt struct, `apply_opt.c`'s KernelAxes mutation path, `propose.c`'s reads from the side struct. The tile.c IR concepts MOVE — to first-class UOp ops in the unified DAG (`UOP_MULTI`, `UOP_MSELECT`, `UOP_MSTACK`, `UOP_DEVICE_NUM`, `UOP_ALLREDUCE`) per tinygrad's MULTI direction and tilelang's Fragment correspondence. The matmul/conv/reduce template recognizers move to UPatRule (already planned in v1 Phase E) consulting the unified DAG.
- 4f. Delete `THVM_UNIFIED_RANGEIFY` env gate. Plain code.

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
