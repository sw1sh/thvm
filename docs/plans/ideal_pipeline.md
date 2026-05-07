# Plan: Migrate to the Ideal Pipeline (2 IRs, 1 dispatch path)

## STATUS: Phase F PARTIAL -- 14 wedges landed on `wedge/ideal-pipeline-g0` (worktree `/private/tmp/thvm-ip-wedge`, branch off main 7752c10)

```
8eaf519f fix(metal): scope lift fallback to tile_sync declining outright -- matmul correctness fix
1ceddf5c feat(metal): cg_tile_metal_dispatch_shape lifter fallback -- path 4 widens
eb7cc477 docs(plans): move ideal pipeline plan into docs/plans/
ae9d0aa0 test(metal-real): matmul M=N=K=16 parity against CPU (F3.5 verify)
91043530 fix(render-uop): simdgroup matmul outer M/N step by 8, explicit row strides
94d44146 fix(uop): matmul recogniser rejects conv2d-shape stores -- correctness
9a3507d0 refactor(metal): tile_jit_encode dispatches before metal_try_gemm (F3.4b)
04714259 feat(uop): uop_classify_matmul + cg_emit_via_uop declines K%8!=0 (F3.4a)
8368f5e1 test(render-uop-metal): recogniser+template e2e on bare matmul (F3.3)
dd3f4da2 feat(uop): UPat matmul recogniser wraps STORE w/ OPT(_, TC) (F3.1)
178567ad feat(upat): trust explicit nsrc when op is pinned (UP1)
b226b7d9 refactor(metal): delete dead metal_try_cpu_small_add (F5)
92fbf2c1 refactor(metal): default-on THVM_TILE (F1)
fc9e71bd refactor(metal): delete dead cg_supports_metal_reduce_expr (G0)
```

## Investigation update (2026-05-07 late)

A focused attempt to land BOTH bug fixes (lifter view.strides +
simdgroup `if (sgi==0u && tg==0u)` guard) was reverted after the
matmul parity test still failed 254/256 with both in place. Probing
`out[tid] = (float)(tid + 1)` confirmed the kernel dispatches and
writes; probing `out[i] = in0[i]` for i in [0,8) returned all `-2`,
matching `A[0,0] = -2` repeated rather than `A[0,0..7]`.

Conclusion: the schedule **materialises** the EXPAND view into an
actual 4096-element buffer (16x16x16) where rows of A are repeated.
The matmul kernel's `in0` is the materialised buffer, not the
underlying 256-element A. So:

  - The lifter's row-major dim-product addressing
    (`stride[d] = product(dims[d+1..])` -> `m*256+k*16+n`) is
    actually CORRECT for the materialised input.
  - View.strides on this input would be `[256, 16, 1]` (contiguous
    3D), giving the same result.
  - The "view shows broadcast (stride=0)" hypothesis was wrong --
    the schedule already collapsed it.

So bug #1 is more subtle than the original framing. The actual
mystery: with the materialised buffer, why does the test fail at
all under the `tg=0 && sgi=0` simdgroup guard with NAIVE scalar
matmul? The naive scalar matmul over `(a0*16) + a2` addresses
should produce correct output values regardless of what
in0/in1 actually contain (they're treated as flat float arrays).
Yet the values come back wrong.

This needs deeper investigation than a single iteration can
support. The campaign's wedges are correctness-safe (conservative
state passes 530/530); resuming the simdgroup-path widening
requires understanding the schedule's materialisation behaviour
first.

## Two layered correctness bugs discovered (2026-05-07)

When the test suite's M=N=K=16 matmul parity test (`ae9d0aa0`) was
forced through render_uop via a wider `cg_tile_metal_dispatch_shape`
fallback, output was wrong on every cell. Investigating the failure
uncovered TWO bugs that compound -- fixing one exposes the next.

**Bug #1 (lifter EXPAND-residue address):** kernel_lift's
ScalarUop walker computes addresses as `sum(range[d] * stride[d])`
where `stride[d] = product(buf.dims[d+1..])` (row-major). For a
buffer whose View is the EXPANDED form (`shape=[16,16,16]` for a
TMatMul-shape A whose underlying storage is `[16,16]`), this gives
`m*256 + k*16 + n` instead of the correct `m*16 + k`. The View's
actual strides would be `[16, 1, 0]` (broadcast on N), but the
lifter ignores `view.strides` and reconstructs from `view.shape`.
**Fix:** pass `ke->input_views[slot].strides[d]` into the offset
computation and skip terms where `stride == 0`.

**Bug #2 (simdgroup multi-simdgroup race):** with bug #1 patched,
the matmul DAG lifts to correct addresses (`m*16+k` for A), and
`uop_recognise_tc` correctly wraps with `OPT(_, TC, 0)`. render_uop
emits the simdgroup_matrix template. But the dispatch ladder binds
the kernel with `output_numel = 256` total threads in one
threadgroup -- 8 simdgroups of 32 threads. The simdgroup template
writes to the same `out[a0*16+a1]` address from every simdgroup; on
M3 this race produces garbage values (test got `16 -10 10 -16` vs
correct `-189 -33 -38 -181`). **Fix:** either (a) emit the simdgroup
template guarded by `if (simdgroup_index_in_threadgroup == 0) { ... }`
so only one simdgroup writes (idle the others), or (b) change
dispatch shape calc to bind exactly one simdgroup (32 threads, 1
group) when the OPT(_, TC) shape is detected, or (c) emit a
threadgroup-distributed template where each simdgroup writes a
different output tile (cleanest, real win).

These are the **real** F3.5 / F4 / F2 prerequisites. Both fixes are
substantial; an attempt at bug #1 alone reproduced bug #2 (256
elements wrong with the lifter fix in place). The conservative
dispatch-ladder state preserves correctness today by routing
TMatMul-shape kernels through `metal_jit_encode` (path 7,
KProgOp-flat shader), which computes addresses from KProgOp's
per-op shape metadata directly and dispatches one-thread-per-output
without simdgroup ops.

The wedges in this campaign (G0/F1/F5/UP1/F3.1/F3.3/F3.4a/F3.4b/
conv-gate/F3.5-template-fix/F3.5-test/F3.5-fallback-revert) are all
**correctness-safe and ready** for the day the lifter bug is fixed.
The matmul correctness parity test is the seam that catches any
attempt to widen render_uop's coverage prematurely.

**Honest finding from F3.5 verify**: the M=N=K=16 matmul parity test
shows the kernel actually dispatches via `metal_jit_encode` (path 7,
KProgOp-flat shader) -- NOT through render_uop's simdgroup template
nor through `metal_try_gemm`. `cg_tile_metal_dispatch_shape`
returns 0 for this kernel because `tile_sync_from_scalar` needs
either a clean matmul-shape (tile_analyze_gemm) or scalar_uops set
(rangeified). The TMatMul-equivalent UOp DAG (RESHAPE+EXPAND+MUL+
REDUCE) doesn't fit either today. So the F3.1+F3.4 wedges are
correctness-safe but largely DORMANT -- the simdgroup path waits
for kernels that produce a recognized matmul shape AND succeed at
`cg_tile_metal_dispatch_shape`. The simdgroup template fix
(91043530) ensures correctness when that day comes.

Dispatch ladder after F3.4b:

```
1. metal_try_alias_reshape          (zero-copy, no kernel)
2. (gated) metal_try_conv2d_flat    (THVM_METAL_SPECIALIZED tests only)
3. metal_tile_jit_encode (render_uop)   <-- now FIRST attempt
4. metal_try_gemm (post-pre-mat)        <-- K%8!=0 matmul fallback
5. metal_jit_encode (KProgOp-flat)      <-- kernels render_uop can't lift
6. per-op interpreter                   <-- kernels metal_jit_encode rejects
```

Correctness gate: `uop_classify_matmul` requires both INDEX_E
addresses to reference exactly 2 distinct UOP_RANGE leaves -- matmul
satisfies it (m+k for A, k+n for B), conv's X address has 4+
(bi/ci/oh+kh/ow+kw) so the recogniser doesn't mis-wrap conv kernels
with OPT(_, TC) and emit broken simdgroup_load reads.

Surgical regression set holds across all wedges:
metal_real 274/274, aot_metal 146/146, aot_metal_run 65/65,
render_uop_metal 8/8, kernel_lift 36/36, kernel_lift_coverage 9/9,
uop_recognise_tc 26/26, render_uop 64/64, uop_upat 37/37,
bufferize 315/315, bufferize_classify 62/62, tile_graph 490/490,
tile_render_msl 39/39.

**Next blocker (F3.5)**: render_uop has no tile-shared-mem fallback
inside the OPT(_, TC) template; K%8!=0 still routes to metal_try_gemm
(fast path) via the F3.4a decline gate. Deleting metal_try_gemm
requires render_uop to grow a tile-shared-mem template that emits a
TILExTILE threadgroup GEMM kernel for K%8!=0 shapes. That's a
substantial wedge: new kernel signature (thread_position_in_threadgroup
+ threadgroup_position_in_grid), new dispatch shape calculation in
cg_tile_metal_dispatch_shape, threadgroup-shared As/Bs arrays in MSL.
Multi-iteration -- not a 5-min hack -- but unblocks F3.5 deletion of
metal_try_gemm + metal_gemm_pipeline + metal_gemm_tile_index +
METAL_GEMM_PSOS cache.

**Parallel wedge available (F4)**: metal_try_conv2d_flat is gated on
THVM_METAL_SPECIALIZED today (default-off in production). Its CONV
shader source is at metal/_.m:1718-. To delete it cleanly the
render_uop side needs a CONV template, plus a UPat recogniser that
wraps conv-shape stores with OPT(_, CONV). render_uop already has a
generic accumulator path that handles conv shape correctly (the
matmul recogniser's 2-range gate now ensures conv falls through
there); a specialised template is a perf optimisation, not a
correctness need.

## STATUS: Phase F PARTIAL (2026-05-07, honest correction)

The previous status note ("Phase F LANDED 2026-05-06") was a session
overclaim. What actually landed:

  - `cg_emit_metal` and `cg_emit_tile_metal` both now forward to
    `cg_emit_via_uop` -> `kernel_lift_to_uop` + `cg_render_uop_kernel`
    in `src/codegen/render_uop.c` (1099 LOC).
  - `cg_supports_metal_reduce_expr` returns 0 unconditionally.
  - `THVM_RENDER_VIA_UOP` gate was removed.
  - `tests/test_tile_graph` reports 11/11 lift, 11/11 compile.
  - All 100 binary tests pass with both `THVM_TILE` unset AND
    `THVM_TILE=1`.

What did NOT land but was claimed to:

  - **The metal/_.m dispatch ladder still has 6 paths**
    ([src/backend/metal/_.m:1954-2105](src/backend/metal/_.m#L1954)):
    `metal_try_alias_reshape`, `metal_try_conv2d_flat` (gated),
    `metal_try_gemm`, `metal_try_cpu_small_add` (gated),
    `metal_jit_encode`, `metal_tile_jit_encode`, plus the per-op
    interpreter fall-through.
  - **`render_uop` only fires when `THVM_TILE=1`**. Default path still
    goes through `metal_jit_encode` (KProgOp-flat shader) and the
    per-op interpreter for kernels that don't fit. The "default-on"
    claim conflated the legacy `THVM_RENDER_VIA_UOP=1` gate (now
    removed) with the orthogonal `THVM_TILE=1` gate (still present
    and still default-off).
  - **None of Phase G** has happened: `src/schedule/tile.c` (2005
    LOC), `src/codegen/tile_anno.c` (300 LOC), `tests/test_tile_*.c`
    (8 files), `src/backend/cpu/op/*.c` (22 files),
    `src/schedule/uop_to_scalar.c` (136 LOC), `kernel_program_cache.c`
    (356 LOC), `ScalarUop[]` arena, `S_*` opcodes, and the entire
    `KProgOp[]` machinery are all alive in tree.
  - rangeify.c is **bigger** (3721 LOC, was 3427 LOC at last claim)
    because the B3 path was added alongside, not replacing, the
    legacy composers.

## What is actually left (revised wedge list)

The keystone shifted again: it is no longer "rewrite the renderer"
(that's done) but **"flip THVM_TILE to default-on, then delete the
legacy paths that the lifter already covers."** The `make test` 100/100
result with `THVM_TILE=1` is the green-light: every test-suite kernel
shape lifts and compiles through render_uop today.

The campaign now decomposes into seven small wedges, each ending with
`make test` green and a real LOC delta. No multi-week phases; no
"Phase X LANDED" without showing the deletion in `git diff --stat`.

## Context

Today's pipeline carries structural cruft accumulated through incremental
landings: four parallel kernel IRs (`KProgOp[]` legacy, `ScalarUop[]`
mid-IR, `TileUop[]` skeleton, `KernelAxes` side channel), six-plus
runtime dispatch paths in [src/backend/metal/_.m:1954-2105](src/backend/metal/_.m#L1954),
two boundary-tracking arrays (`BUFFERIZE_NODES[]` + `BUFFERIZE_BUFS[]`),
and movement ops that exist as heap UOps AND `KProgOp` slots AND consumed-
edge metadata on `BIndexChainOp`. Performance evidence
([docs/plans/profiling_methodology.md](docs/plans/profiling_methodology.md)
§4.6) shows the cost: at BS=512 we run 36x slower than tinygrad and 173x
slower than torch-MPS, dominated by per-kernel parallelism (default
LOOP-only axes) and metal-jit fallbacks for kernels rangeify rejects.

[docs/lowering_passes.md](docs/lowering_passes.md) §"Ideal pipeline"
diagnosed the right shape: **2 IRs (UOp DAG / target source),
movement-as-INDEX, schedule-in-IR, one dispatch path**.

**The UPat mechanism (2026-05-06)** is the new tool that makes the
remaining rewrites cheap. `src/uop/upat.c` (94 LOC) provides:

  - `UPat`: declarative pattern node (op, nsrc, dtype, bind slot,
    children, optional `op_alt[]` opcode-set match).
  - `UPatRule[]`: pattern + rewrite-fn table.
  - `uop_pattern_rewrite(root, rules, n, ctx)`: bridge to
    `uop_graph_rewrite`.

First production consumers are in
[src/schedule/bufferize_classify.c](src/schedule/bufferize_classify.c)
(matmul recognizer, matmul-input-protect marker, scalar-tail walker
shared `bufferize_upat_alu2` table). Every remaining rewrite in this
plan -- specialised dispatch recognisers (gemm/conv2d -> UOP_OPT(_, TC)),
reduce-broadcast lowering, axis-type rewrites in apply_opt -- becomes
a `UPatRule[]` table rather than a hand-rolled C function. UPat is
the mechanism that makes Phases C/E/F finish-able without doubling
the in-tree complexity.

**Critical revision (2026-05-05)**: an earlier draft of this plan
treated Tile IR as a separate `TileUop[]` array. Inspecting tinygrad's
new direction (TileLang surface mapping + `Fragment <-> Ops.MULTI`
analogy) showed this is the wrong abstraction. Tile-level concepts --
shared-memory allocation, register fragments, async copy, barriers,
GEMM, conv2d, multi-device sharding -- all inhabit the **same UOp DAG**
as the input language, expressed through a small set of additional
opcodes plus annotations. `UOP_BUFFER(scope=GLOBAL|LOCAL|REG)`,
`UOP_STORE`, `UOP_AFTER`, `UOP_OPT(target, kind, factor)` already
landed for this. There is no separate Tile IR.

## TileLang correspondence (kept for reference)

| Surface (TileLang / tinygrad) | UOp DAG shape |
|---|---|
| `T.Tensor` argument | `UOP_BUFFER(shape, scope=GLOBAL)` |
| `T.alloc_shared` | `UOP_BUFFER(shape, scope=LOCAL)` |
| `T.alloc_fragment` | `UOP_BUFFER(shape, scope=REG)` |
| `T.copy` | `UOP_STORE(dst, addr, src) + UOP_AFTER` |
| `T.async_copy` | `UOP_STORE + UOP_AFTER` with `Linear` ordering annotation |
| `T.Pipelined` | `UOP_RANGE` + `Linear/toposort` annotation |
| `T.serial` | `UOP_RANGE(axis_type=LOOP)` |
| `T.unroll` | `UOP_RANGE + UOP_OPT(target, UNROLL, factor)` |
| `T.Parallel` | `UOP_RANGE` with parallel `axis_type` (LOCAL/GLOBAL) |
| `T.gemm` | matmul-shape UOp + `UOP_OPT(target, TC)` annotation |
| `T.reduce_*` | existing `UOP_REDUCE` |
| `T.exp/log/sqrt/etc` | existing decomposed elementwise UOps |
| Multi-device sharding | `UOP_MULTI(src, axis)` (out of scope) |

## Pre-flight: current state (audit-confirmed 2026-05-07)

| File | LOC | State |
|---|---|---|
| [src/uop/upat.c](src/uop/upat.c) | 94 | LANDED. UPat + adapter. Used by bufferize_classify.c. |
| [src/uop/graph_rewrite.c](src/uop/graph_rewrite.c) | 351 | LANDED. Bottom-up walker w/ memo + named stats. |
| [src/uop/graph_simplify.c](src/uop/graph_simplify.c) | 605 | LANDED. 10+ rules; movement-chain + symbolic INDEX folds. Hand-written. UPat-port deferred. |
| [src/uop/index.c](src/uop/index.c) | 108 | LANDED (B0). |
| [src/uop/index_simplify.c](src/uop/index_simplify.c) | 415 | LANDED (B2). Hand-written. UPat-port deferred. |
| [src/uop/movement_index.c](src/uop/movement_index.c) | 228 | LANDED (B1). |
| [src/uop/opt.c](src/uop/opt.c) | -- | LANDED (D'4). UOP_OPT + helpers. |
| [src/uop/buffer.c](src/uop/buffer.c) | -- | LANDED (D'1). |
| [src/codegen/render_uop.c](src/codegen/render_uop.c) | 1099 | LANDED (F0). Reads UOP_BUFFER/STORE/AFTER/OPT/RANGE/INDEX_E/REDUCE. |
| [src/codegen/render_metal.c](src/codegen/render_metal.c) | 200 | THIN. cg_emit_metal -> cg_emit_tile_metal -> cg_emit_via_uop. cg_supports_metal_reduce_expr returns 0. |
| [src/schedule/kernel_lift.c](src/schedule/kernel_lift.c) | 1533 | LANDED. KProgOp -> UOp DAG lifter. 100% test_tile_graph coverage. |
| [src/schedule/rangeify.c](src/schedule/rangeify.c) | 3721 | DUAL-PATH. Movement composers + UOp-INDEX path coexist. Legacy composers still load-bearing for non-LOAD-site uses. |
| [src/schedule/tile.c](src/schedule/tile.c) | 2005 | LIVE. TileUop[] skeleton. SLATED FOR DELETION. |
| [src/codegen/tile_anno.c](src/codegen/tile_anno.c) | 300 | LIVE. KernelAxes <-> TILE_AXIS facade. SLATED FOR DELETION. |
| [src/schedule/uop_to_scalar.c](src/schedule/uop_to_scalar.c) | 136 | LIVE. UOp-INDEX -> ScalarUop bridge. Used by rangeify B3 fallback. |
| [src/schedule/kernel_program_cache.c](src/schedule/kernel_program_cache.c) | 356 | LIVE. KProgOp[] cache. SLATED FOR DELETION (UOp hash-cons). |
| [src/schedule/materialize.c](src/schedule/materialize.c) | 2576 | LIVE. Emits KProgOp[]. Phase C target. |
| `src/codegen/{axis,apply_opt,propose}.c` | 110+132+501 | LIVE. KernelAxes mutators. Phase E target. |
| [src/codegen/autotune.c](src/codegen/autotune.c) | 838 | LIVE. BEAM over KOpt[]. Phase E target. |
| [src/backend/metal/_.m](src/backend/metal/_.m) | -- | 6-path dispatch ladder still alive. Phase F target. |
| [src/backend/cpu/op/*.c](src/backend/cpu/op/) | ~22 files | LIVE. Per-op interpreter. SLATED FOR DELETION. |
| `tests/test_tile_*.c` | 8 files, ~1500 LOC | LIVE. SLATED FOR DELETION. |

## Phases (revised)

### Phase A -- Bufferize-IR data layer (in tree, partial)

Tracked separately in [docs/plans/bufferize.md](docs/plans/bufferize.md);
not duplicated here.

### Phase B -- Movement-as-INDEX (B0..B3 LANDED; B3-finish gated on Phase F)

B0 (UOp-level INDEX repr), B1 (6 movement-to-INDEX rules), B2 (symbolic
simplifier), B3 (rangeify consumes UOP_INDEX_E at LOAD-site) all LANDED.
B3-finish (delete the 6 `rngs_ctx_*` composers entirely) blocks on
Phase F finishing -- once render_uop is the only renderer, the legacy
composers have no remaining consumer.

### Phase D' -- UOp-DAG opcodes (D'1, D'2, D'4 LANDED; D'3 OUTSTANDING)

D'1 (UOP_BUFFER), D'2 (UOP_STORE/UOP_AFTER), D'4 (UOP_OPT) all LANDED
with focused tests (test_uop_buffer, test_uop_store_after, test_uop_opt).

D'3 OUTSTANDING: reduce-broadcast lowering rule. Pattern:
`BUFFERIZE(REDUCE(x)) -> EXPAND -> MUL/ADD/etc.` lowers to
`UOP_BUFFER(scope=LOCAL) + UOP_STORE + UOP_AFTER + UOP_INDEX_E`. The
mechanism here is a `UPatRule[]` table in graph_simplify -- this is
where UPat first proves out as the lowering-rule mechanism.

### Phase F -- One dispatch path (PARTIAL; this is the keystone)

The renderer landed (F0). What remains is making it the only path:

  - **F1 -- ungate THVM_TILE**: flip `metal_tile_enabled` to default-on;
    delete the env gate from `propose_metal_tile_enabled`,
    `kautotune_metal_tile_enabled`, and the equivalents in
    bufferize_classify.c, autotune.c, propose.c, interpret.c. Tests
    already pass under both modes today.
  - **F2 -- collapse the metal_jit_encode dead branch**: with cg_supports
    returning 0 unconditionally, the `if (!tile_supported && cg_supports...)`
    block at metal/_.m:2108-2127 is unreachable. Delete it and the
    helper.
  - **F3 -- migrate metal_try_gemm to UPat -> UOP_OPT(_, TC)**: the
    structural recogniser becomes a `UPatRule[]` table that emits a
    UOP_OPT(_, TC) annotation on the matmul-shape root. render_uop
    pattern-matches and emits the specialised template. The
    metal_try_gemm hand-rolled branch deletes.
  - **F4 -- migrate metal_try_conv2d_flat similarly**: UPat recognises
    the canonical conv2d-flat shape, emits UOP_OPT(_, CONV);
    render_uop emits the conv2d template. The metal_try_conv2d_flat
    branch deletes.
  - **F5 -- delete metal_try_cpu_small_add**: with kernels going through
    render_uop (CPU-JIT for the CPU side), this CPU-fallback
    on the Metal path is redundant. Delete the branch + the
    `metal_cpu_small_add_enabled` env gate.
  - **F6 -- delete the per-op interpreter fall-through and metal_jit_encode**:
    once F1-F5 land, the dispatch ladder becomes alias-reshape +
    render_uop, and the per-op fallback at metal/_.m:2228+ is
    unreachable. Delete it together with metal_jit_encode (line 769)
    and the metal_jit_pipeline / metal_jit_build / metal_jit_cache_*
    helpers if no other consumer remains. CPU-side equivalent:
    introduce a CPU-JIT path (clang -shared) reading render_uop
    output retargeted to C, and delete `src/backend/cpu/op/*.c`.

### Phase E -- Collapse KernelAxes (PARALLEL with Phase F)

`UOP_RANGE.axis_type` becomes the single source of truth.
`codegen/{axis,apply_opt,propose}.c` (743 LOC) collapse into one
`uop_anno.c` (~300 LOC). Each apply_opt mutation becomes a
`UPatRule[]` rewrite (an axis-type swap, an UPCAST insertion via
UOP_OPT, a GROUP_REDUCE split). BEAM autotune becomes a search over
UPatRule applications.

### Phase C -- Collapse KProgOp[] (largest deletion; gated on Phase F)

`KernelEntry.program` becomes `KernelEntry.compute_root` (Term --
a UOp DAG node). materialize.c rewrites to emit a UOp subgraph
reference; consumers walk the UOp subgraph rooted at compute_root
with the kernel's input set as boundary. kernel_program_cache.c
deletes (UOp hash-cons replaces).

### Phase G -- Delete the dead paths (after F+C+E)

Mechanical at this point. Targets:

  - `src/schedule/tile.c` (2005 LOC)
  - `src/codegen/tile_anno.c` (300 LOC) -- replaced by `uop_anno.c`
  - `src/schedule/uop_to_scalar.c` (136 LOC)
  - `src/schedule/kernel_program_cache.c` (356 LOC)
  - `src/backend/cpu/op/*.c` (22 files, ~3K LOC) -- replaced by
    render_uop -> CPU-JIT
  - `tests/test_tile_*.c` (8 files, ~1500 LOC)
  - `TileUop` / `TileAxisInfo` / `TileAllocInfo` types in thvm.h
  - `tile_uops` / `tile_axes_version` fields on KernelEntry
  - `ScalarUop[]` arena + S_* opcodes + `scalar_uops` field on
    KernelEntry (last consumer is rangeify B3 fallback; gated on
    B3-finish)
  - the rangeify legacy composers
  - `KProgOp` type + `program` / `n_ops` fields on KernelEntry

## Wedge sequence (each ends with `make test` 100/100 green)

| Wedge | Description | Files | Net LOC | Status |
|---|---|---|---|---|
| G0 | Delete `cg_supports_metal_reduce_expr` and the metal/_.m:2108 dead branch (unreachable: cg_supports returns 0 unconditionally, and even if true the block was guarded by `!tile_supported`). | metal/_.m, render_metal.c | -25 | NEXT |
| F1 | Default `metal_tile_enabled` to true; delete the `THVM_TILE` env gate everywhere it appears (5 files). Verify tests pass without setting THVM_TILE. | metal/_.m, propose.c, autotune.c, bufferize_classify.c, interpret.c | -30 | NEXT |
| F2 | Delete `metal_jit_encode`, `metal_jit_pipeline`, `metal_jit_build`, `metal_jit_cache_*`, and the per-op interpreter fall-through in metal/_.m. Provable safe once F1 makes render_uop the path. Backed by `make test` + a focused canary that exercises shapes the lifter must accept. | metal/_.m | -800 | depends on F1 |
| F3 | UPat recognizer for matmul -> emits UOP_OPT(_, TC). Render_uop emits the gemm-template (logic already in metal_try_gemm; transplant onto the UOp DAG path). Delete metal_try_gemm. | uop/, render_uop.c, metal/_.m | -300 | depends on F2 |
| F4 | Same shape as F3 for conv2d_flat. | uop/, render_uop.c, metal/_.m | -400 | depends on F3 |
| F5 | Delete metal_try_cpu_small_add + its env gate. | metal/_.m | -150 | depends on F4 |
| F6 | CPU-JIT via render_uop -> C; delete src/backend/cpu/op/*.c. | render_c.c, cpu/jit.c, cpu/op/* | -3000 | depends on F1..F5 |
| E1..En | Per-mutation port from KernelAxes apply_opt to UPatRule[] over UOP_RANGE. Each mutation type lands separately. | codegen/ | -700 | parallel with F |
| B3-finish | Delete the rangeify legacy composers; rangeify becomes pure UOp-DAG schedule. | rangeify.c | -800 | depends on F |
| C1..Cn | materialize emits compute_root (Term); each consumer walks the UOp subgraph; kernel_program_cache deletes. | materialize.c, rangeify.c, codegen/, autotune.c | -1500 | depends on F |
| G* | Mechanical deletions per Phase G list. | -- | -8000 | depends on C+E |

Net deletion estimate: **~15K LOC** (up from the previous ~5K because
the per-op interpreter joins the cull list once Phase F is real).

## Tests / verification

Per-wedge gates:

| Wedge | Test seam |
|---|---|
| G0 | `make test` 100/100; verify `cg_supports_metal_reduce_expr` is unreferenced. |
| F1 | `make test` 100/100 with THVM_TILE unset (was already passing with =1). |
| F2 | `make test` 100/100; bench-train.wls BS=32 wall stable; histogram of dispatch_kind shows no METAL_JIT entries. |
| F3 | `make test`; gemm canary (two_linears) BS=32 + BS=128 stable. |
| F4 | conv2d canary (mnist forward) BS=32 stable. |
| F5..G* | `make test` after each. |

End-to-end gates:
1. `make test` 100/100 after every wedge.
2. BS=32 canary (`bench-train.wls`): wall <= current 19.7ms within noise.
3. BS=128 + BS=512 canary: report wall, kernels, peak_retained.
4. `DUMP_REWRITE=1` shows new UPatRule firing on expected shapes.

## Non-goals

- Custom GEMM, Conv, or FlashAttention kernels as primary fusion strategy.
  Specialised ops are pattern-matched into canonical UOp shapes via
  UPat; render_uop emits one template per shape.
- A separate "Tile IR" data structure. tile-level concepts inhabit
  the UOp DAG; there is no `TileUop[]`.
- Backwards-compatibility shims. When a structure deletes, every consumer
  rewrites; no `_legacy` aliases.
- New WL API surface. The migration is internal to the schedule/codegen
  pipeline.

## Out-of-scope follow-ups

- ICB replay capture/replay infrastructure ([src/jit/](src/jit/)) -- already
  the right shape; passes through unchanged.
- Memory planner.
- Backend additions (CUDA/HIP/Vulkan).
- Multi-device sharding (UOP_MULTI / UOP_MSELECT / UOP_MSTACK /
  UOP_ALLREDUCE).

## Honesty discipline (added 2026-05-07)

For every claim of "Phase X LANDED" the file MUST show:

  1. The `git diff --stat` LOC delta that proves the structural
     deletion happened. "The renderer was rewritten" without the
     legacy renderer being deleted is NOT a landing.
  2. A test-suite signature: `make test N/N` with N stable or growing.
  3. A canary signature: bench-train BS=32 wall stable.

If any of (1)(2)(3) is missing, the phase is PARTIAL or PENDING, not
LANDED. The 2026-05-06 status note violated this discipline; the
2026-05-07 audit corrects it.
