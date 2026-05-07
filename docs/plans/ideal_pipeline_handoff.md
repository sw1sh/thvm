# Ideal Pipeline -- Handoff (2026-05-08)

This is the active continuation point for [ideal_pipeline.md](ideal_pipeline.md).
A long session (~35 commits) closed the F3.5 unblocker and the F6 main goal,
documented several remaining wedges concretely, and ran into a misdirection
worth flagging so the next agent doesn't repeat it.

## Status post Wave 1-6 (2026-05-08, six parallel-agent merge waves)

Six waves of parallel worktree-isolated agents landed F4, F6 (full), Phase E1-E8,
and Phase C slices 1-7. Cumulative LOC delta is strongly negative:

- **F4**: conv2d UPat recognizer + `rmu_emit_conv` template + F4-prune cleanup.
  +611 LOC net (template is purely additive; the -400 LOC handoff target
  was overstated -- the generic accumulator path still serves softmax /
  plain reduce / TC `K%8 != 0` fallback).
- **F6 (full)**: `cpu_uop_walk` walker (Wave 1), FLIP lifter widening
  (Wave 2), multi-output STORE-AFTER lifter (Wave 3), F6 cleanup (Wave 4)
  deleted `cpu/op/*.c` (21 files), `cpu_interpret()`, dispatch ladder
  rung 6, and 21 `#includes` in `thvm.c`. **-1312 LOC net** in F6 cleanup
  alone. `cpu_interpret` no longer exists; bisection knob `THVM_CPU_UOP_WALK=0`
  preserved (multi-output kernel-merge test legitimately fails under it).
- **Phase E**: E1 accessors + rewriter primitive, E2 KOP_GLOBAL,
  E3 KOP_SWAP (full-history simulation), E4-E6 split-class
  (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP, all in `uop_apply_kop_split`,
  pragmatic stamp-only), E7 KOP_TC (metadata-only, position-invariant),
  E8 `uop_arity` + `uop_graph_rebuild_with_srcs` extension for INDEX
  descent. `tests/test_uop_range_axis_type.c` 0 -> 224 checks.
- **Phase C**: slice 1 `compute_root` dual-write, slice 2 `cached_lift`
  full-lift cache (4 consumers stopped re-lifting), slice 3 structural
  `rmu_buf_name` via `UOP_BUFFER.instance`, slice 4 `metal_kernel_supported`
  / propose flipped to UOp DAG, slice 5 DAG-side Metal encoder
  (reuses existing UOP_*-keyed shader cache because `KProgOp.opcode` IS
  a `UOP_*` tag), slice 7 single-write migration default-on
  (`THVM_PHASE_C7_FREE_PROGRAM=0` reverts).

### Slice 8 RE-FRAMING (important)

The original "delete `tile.c` KProgOp gemm/gemv recognisers in favor of
`recognise_tc`/`recognise_conv`" framing is wrong. `tile_analyze_gemm`
is a **feeder** for the UOp-side `recognise_tc`, not a redundant peer:

- `kernel_lift_from_gemm` (kernel_lift.c:952) calls `tile_collect_mma_plan`
  -> `tile_analyze_gemm` to **synthesize** the UOp DAG when
  `scalar_uops == NULL` (the dedicated GEMM dispatch path bypasses
  rangeify). recognise_tc runs *after* the lifter.
- `cpu_blas_dispatch` reads `gemm.M/N/K/ldA/ldB/flags/a_input/b_input`
  to call `cblas_sgemm` directly via Apple Accelerate.
- `kernel_apply_opt`'s KOP_TC gate consumes it.
- `propose.c` autotune uses it for TC tile-size proposals.

Real gating for "delete `tile.c` gemm recognisers": (a) `kernel_lift_from_gemm`
retires (rangeify+scalar_uops covers GEMM), (b) `cpu/blas.c` retires,
(c) `apply_opt`'s KOP_TC gate moves to Phase E. That's Phase G territory,
not a free-standing slice.

### Wave 6 perf regression flag

Phase C slice 7 default-on single-write causes documented perf regressions
(slice 7 commit message + this handoff):

- `cpu_blas_dispatch` early-bails on `n_ops != 2` -> lift-eligible matmul
  regresses to JIT/scalar fallback.
- `tile_analyze_gemm/conv2d_flat_impl` early-bail on `program == NULL`
  -> lift-eligible gemm/conv MSL regresses to per-element DAG encoder.
- `metal_try_alias_reshape` early-bails on `n_ops != 1` -> reshape-aliasable
  kernels lose zero-copy.

These match the slice 8 territory now documented above. Until that's
unblocked, set `THVM_PHASE_C7_FREE_PROGRAM=0` if you hit a perf cliff.

### Wave 7 audit results (2026-05-08)

**Phase E9 audit — BLOCKED.** No deletion of `KernelAxes.axis_types[]` is
safe today. The E1-E8 UPatRules (`uop_apply_kop_{global,swap,split,tc}`)
have ZERO production callers — they're invoked only from
`tests/test_uop_range_axis_type.c`. `axis_types[]` has 7 production
readers (`codegen/axis.c:83-84`, `codegen/tile_anno.c:72,254`,
`schedule/kernel_lift.c:1556-1557,1650`, `schedule/tile.c:1649`,
`schedule/kernel_program_cache.c:223`) and 3 writer entry points
(`axes_apply_opt`, plus `tile_anno_axis_append/insert/set` facades that
still route to direct `axes->axis_types[]` writes).

Six prerequisite wedges named (in dependency order):

1. **Wire UPatRules into a post-lift production pass** — call
   `uop_apply_kernel_opts(root, ke->axes->applied_opts, n_applied)`
   after `kernel_lift_to_uop` returns.
2. **`uop_range_split` primitive + INDEX_E address rewiring** — replaces
   `kernel_lift.c`'s structural-replay split block. Named explicitly in
   the E4-E6 file header.
3. **Replace `axes_has_reduce_axis` reads** with applied_opts walk
   (these fire pre-lift, can't read UOP_RANGE.axis_type).
4. **Rewire `tile_emit_axes_from_kernel_axes`** producer of TILE_AXIS
   leaves — needs alt source.
5. **Rewire `kp_slot_equal` cache-key** — compare TILE_AXIS arrays /
   applied_opts sequences instead of `axis_types[]`.
6. **Migrate ~30 hand assignments** in `tests/test_metal_real.c` +
   `tests/test_tile_graph.c` that drive `ke->axes` directly without
   `axes_apply_opt`.

**Phase G audit — most targets BLOCKED.** Concrete enumeration:

- `cpu/op/*.c` (21 files): **ALREADY DELETED** in F6 cleanup (Wave 4).
- `kernel_program_cache.c`: KAXIS_CACHE half is independent and live;
  KP_CACHE half gated on slice 8 re-framing (`tile_analyze_gemm` is
  a feeder, not redundant). No safe sub-piece deletable now on current
  main.
- `tile.c` (~1961 LOC): live consumers in `kernel_lift_from_gemm`,
  `cpu_blas_dispatch`, autotune, apply_opt KOP_TC gate. `tile_build_conv2d_from_info`
  has zero callers but its TILE_CONV2D root is referenced by dump/render/validate
  paths that may consume it in a future Phase F renderer iteration. Don't
  delete speculatively.
- `tile_anno.c` (~300 LOC): writer-side facades route to KernelAxes;
  retirement is gated on Phase E9.
- `uop_to_scalar.c` (136 LOC): two production consumers in
  `kernel_lift.c:180,574` + `rangeify.c:938,951`. Gated on rangeify
  side migrating off `r->refs[d]` scalar slots.
- `tests/test_tile_*.c` (~2238 LOC across 8 files): every file
  exercises live tile_*/tile_anno_* APIs.
- `KProgOp` type: live in 50+ files. Gated on slice 8 + Phase E9 +
  Phase F.
- rangeify legacy composers (rangeify.c:1014-1027): active emit path,
  not dead.

**Total Phase G deletable on current main right now: ~0 LOC.** The
campaign reaches a natural pause point here. Remaining work is
multi-session foundational (the 6 E9 prerequisites above).

### Cumulative test state (2026-05-08, post Wave 6)

```
test_metal_real          554/554    (was 530; slice 5 added 24 DAG-encoder checks)
test_aot_metal           146/146
test_aot_metal_run       FAIL 61/65 (PRE-EXISTING in user's atp-ic stream;
                                     "5/6 cases match CPU" per dd0d96a8 commit msg;
                                     NOT caused by this campaign)
test_render_uop          182/182    (was 175; F4 added 7 conv-template assertions)
test_render_uop_metal      8/8
test_kernel_lift          36/36
test_kernel_lift_coverage  7/7
test_tile_graph          490/490
test_bufferize           303/303    (was 315; slice 7 default-on gates 12
                                     dual-write checks; pass under
                                     THVM_PHASE_C7_FREE_PROGRAM=0 too)
test_bufferize_classify   62/62
test_uop_recognise_tc     26/26
test_uop_recognise_conv   30/30     (NEW from F4)
test_uop_range_axis_type 224/224    (NEW E1+E2+E3+E4-E6+E7+E8 cumulative)
test_compute_root_dual_write 83/83  (NEW C1+C2+C3+C4+C7 cumulative)
test_metal_stub            6/6
test_tile_render_msl      39/39
test_view_flip            35/35
test_materialize_v2       14/14
```

### Active env knobs (post Wave 6)

```
THVM_CPU_UOP_WALK=0          Bisection: opt out of cpu_uop_walk; falls back
                             to cpu_jit_dispatch only (no per-op interp).
                             Multi-output kernel-merge test fails under =0
                             since cpu/op/* is gone.
THVM_CPU_UOP_WALK_TRACE=1    Trace walker entries.
THVM_PHASE_C7_FREE_PROGRAM=0 Revert slice 7 single-write to dual-write
                             (program[] populated alongside cached_lift).
                             Use if you hit slice-8-territory perf regressions.
THVM_RANGEIFY_BAIL=1         Print rangeify lowering bail reasons.
THVM_TILE=0                  Force legacy KProgOp-flat dispatch off (default).
THVM_KERNEL_MERGE=1          Force multi-output fusion (dev exercise).
THVM_CPU_JIT_VIA_UOP=0       Bisection: opt out of render_uop_c JIT.
```

`THVM_CPU_INTERPRET_TRACE` and `THVM_CPU_JIT_TRACE_FALLBACK` are NO LONGER
WIRED (their consumers were deleted in F6 cleanup).

---

(Original handoff content from before Wave 1 follows below for archival.)

## What landed

### F3.5 -- lifter view.strides fix (UNBLOCKER)
Single commit `099c78f6` changed `kernel_lift_to_uop`'s INDEX_E address
computation from dim-product strides to `ke->input_views[slot].strides[d]`,
skipping stride=0 broadcast terms. Was supposedly a multi-session blocker; in
practice was one focused diagnostic + one targeted fix. The previous plan note
("schedule materialises EXPAND, lifter is correct") was misdiagnosis -- a
probe MSL `out[tid] = in0[tid] + 1000.0f*in1[tid]` revealed `in0[1]=-8` (=`A[0][1]`),
not `-2` (=`A[0][0]` broadcast on n), proving the EXPAND stays virtual.

### Dispatch ladder collapse 6 -> 3 paths
- `97d58c32` deleted `metal_try_conv2d_flat` (diagnostic-only, gated on
  `THVM_METAL_SPECIALIZED` which no test sets). -142 LOC.
- `88f536c3` deleted `metal_jit_encode` (path 7, KProgOp-flat shader). -124 LOC.
- `4e30432b` deleted `metal_try_gemm` + tiled GEMM MSL. -158 LOC.
- `30bfbeec`, `505c29c4`, `67b37962` periphery: orphan `metal_isqrt_exact`,
  unused `thvm_metal_jit_build_*` accessors, KERNEL_LIFT_COMPILES counters.

Final ladder:
```
1. metal_try_alias_reshape          (zero-copy)
2. metal_tile_jit_encode (render_uop) -- handles ALL lifter-eligible kernels
3. per-op interpreter                  -- when lifter declines
```

### F6 -- legacy CPU C99 renderer DELETED
`eab0b9de` -> `5b4d721c` (15 commits) built `cg_render_uop_kernel_c` (C99
emit from UOp DAG), validated through clang compile + dlopen + invoke +
multi-input/unary/REDUCE_SUM/REDUCE_MAX/sqrt/BITCAST coverage tests, flipped
the env-gated path to default-on after passing the full surgical suite under
env-on, removed the legacy fallback, and finally deleted `render_c.c` +
`cg_emit` + `Renderer` struct + `CgBuf` + `cg_append` (~600 LOC in step 16).
WL FFI's diagnostic call retargeted in same commit. Net F6 LOC: ~-700.

`cg_supports` is kept as the CPU-JIT pre-build gate; helpers
`cg_dtype_supported`, `cg_op_is_float_only`, `cg_src_numel` survive only as
its private helpers. `cg.c` shrank from 349 to 141 LOC.

### Stale-reference cleanup post-F6
~10 commits across src/, docs/, tests/, wl/ removing dead references to
`render_c.c`, `cg_emit`, `C_RENDERER`, `Renderer struct`, `render_c_scalar.c`.
Comment-level pass; no semantic change.

## What's pending

### F4 -- conv2d UPat recognizer + render_uop CONV template
**Scope:** -400 LOC at completion. Pure perf optimization -- render_uop's
generic accumulator path correctness-handles conv shape today; a CONV
template just makes it faster. Plan in [ideal_pipeline.md](ideal_pipeline.md)
row F4. Recognizer alone is dead code without consumer, so the template
must land first or together.

**Concrete shape:**
1. Write `uop_recognise_conv` mirroring `uop_recognise_tc` (`src/uop/recognise_tc.c`)
2. Add `UOP_OPT_CONV` opcode constant
3. Add `rmu_emit_conv` template in `src/codegen/render_uop.c`
4. Wire into `cg_emit_via_uop` (alongside `uop_recognise_tc` call)
5. Tests in `tests/test_uop_recognise_tc.c` extension or new file

### F6 finish -- delete cpu/op/*.c (~1100 LOC, 21 files)
**This was misframed in the plan**, see "Misdirection" below. Actual blocker:
`cpu_interpret` is the steady-state interpreter for sub-warmup-count kernels,
not a niche fallback. Three real paths:

- **(a)** Drop the `CPU_JIT_WARMUP=5` gate so `cpu_jit_dispatch` fires on
  first dispatch. Cleanest deletion, but **regresses** one-shot kernel
  perf (compile cost paid per unique kernel even if used once). Tinygrad
  amortizes via persistent compile cache; we'd need the same to make this
  acceptable.
- **(b)** Build a UOp-DAG walker interpreter to replace `cpu_interpret`. It
  would reuse the lifter (`kernel_lift_to_uop`) and walk the resulting DAG
  via a tree-walker that mirrors `cg_render_uop_kernel_c`'s emit logic but
  evaluates instead of emitting. Right architecture; multi-session.
  **PARTIALLY LANDED 2026-05-08:** `cpu_uop_walk`
  (`src/backend/cpu/uop_walk.c`) fires AHEAD of `cpu_jit_dispatch` in the
  dispatch ladder. Default-on after step 1 + step 2 (bit-equal flip);
  bisection knob `THVM_CPU_UOP_WALK=0` falls back to the legacy path.
  Surgical suite passes bit-equal. Trace count unchanged (3) because the
  remaining hits aren't reachable through the dispatcher: `test_bufferize`
  has direct `cpu_interpret(...)` calls in test infra (validating
  `cpu_interpret` itself), and `test_view_flip`'s strided-consumer kernel
  has `scalar_uops == NULL` (rangeify bails on negative-stride FLIP) so
  the lifter declines and the walker can't help. Deleting `cpu/op/*.c`
  still requires the lifter to widen for negative-stride FLIP residue.
- **(c)** Accept that `cpu_interpret` + `cpu/op/*.c` stays as the warmup
  interpreter. This is what's in tree today.

**The trace knob** `THVM_CPU_INTERPRET_TRACE=1` (commit `1f42df3c` /
`242f26b3`) lets you measure how often the per-op fallback fires under any
workload. On the surgical suite: 3 hits across ~2179 checks (test_view_flip
once, test_bufferize twice, all elementwise programs that didn't cross the
warmup threshold).

### Phase E -- KernelAxes -> UOP_RANGE.axis_type (-700 LOC)
`apply_opt.c` mutates `KernelAxes.axis_types[]` BEFORE rangeify creates
UOP_RANGE nodes. A direct port to UPatRule[] over UOP_RANGE doesn't compose
because the rangeify lowering happens later. Two real shapes:

1. Persist apply_opt mutations as UOP_OPT annotations on the kernel root,
   then have rangeify or a pre-render pass apply them as UPatRules over
   UOP_RANGE leaves. Keeps the existing flow ordering.
2. Flip the order: rangeify produces default UOP_RANGE nodes (axis_type=LOOP)
   first, then apply_opt mutates UOP_RANGE.axis_type via UPatRules. Cleaner
   conceptually but is a fundamental flow restructure.

Either is multi-session. The plan doc describes E1..En but each Ek is its own
wedge.

### Phase C -- KProgOp[] -> compute_root Term (-1500 LOC)
`KernelEntry.program` becomes `KernelEntry.compute_root` (Term -- UOp DAG
node). `materialize.c` rewrites to emit a UOp subgraph reference; consumers
walk the subgraph rooted at compute_root. Largest deletion target since
`kernel_program_cache.c` (356 LOC) deletes when UOp hash-cons replaces it,
and most call sites currently iterate `ke->program[i]`.

Touch points: `materialize.c` (~2576 LOC) is the producer; `cpu_interpret`,
`cpu_jit_hash`, `metal_kernel_supported`, `metal_dispatch_kernel`,
`tile_anno_*`, `propose.c`, `autotune.c` are consumers.

### Phase G -- mechanical deletions (gated on F+C+E)
Per [ideal_pipeline.md](ideal_pipeline.md) row G. ~8000 LOC of slated deletions
(`tile.c`, `tile_anno.c`, `uop_to_scalar.c`, `kernel_program_cache.c`,
`backend/cpu/op/*.c`, `tests/test_tile_*.c`, `TileUop[]`/`ScalarUop[]` arenas,
rangeify legacy composers, `KProgOp` type). Mechanical at this point because
their consumers will all be gone.

## Misdirection (avoid repeating)

I almost spent multiple iterations widening `S_INDEX` strides from `u16` to
`i16`/`i32` to unblock rangeify-on-FLIP-inputs. **Don't.** `S_INDEX` is part
of `ScalarUop[]`, which is on the Phase G deletion list. Extending its
encoding extends the life of dead infrastructure.

The right shape: keep effort pointed at the surviving UOp DAG IR. The lifter
(`kernel_lift_to_uop`) handles signed arithmetic natively via UOP_IADD/IMUL,
so widening the lifter to handle KProgOp programs without scalar_uops is the
correct route to shrink the cpu_interpret hit set (option (b) above).

## Test invariants to maintain

Surgical suite that stayed green throughout (~2179 checks):
```
test_metal_real    530/530
test_aot_metal     146/146
test_aot_metal_run  65/65
test_render_uop    175/175
test_render_uop_metal 8/8
test_kernel_lift   36/36
test_kernel_lift_coverage 7/7
test_tile_graph    490/490
test_bufferize     315/315
test_bufferize_classify 62/62
test_uop_recognise_tc 26/26
test_render_uop    175/175
test_cpu_jit_via_uop 320/320
test_metal_stub    6/6
test_tile_render_msl 39/39
```

`THVM_CPU_JIT_VIA_UOP=1` env-on validation (during F6 step 9, commit
`70a997f0`): the same suite passed bit-equal. The current default is on;
`THVM_CPU_JIT_VIA_UOP=0` is the bisection knob.

## Key commits

```
099c78f6 fix(kernel-lift): use view.strides for INDEX_E address -- F3.5 unblocker
88f536c3 refactor(metal): delete metal_jit_encode (path 7) -- F2/F6 step 1
4e30432b refactor(metal): delete metal_try_gemm + tiled GEMM shader -- F3 step
eab0b9de feat(render-uop): cg_render_uop_kernel_c -- emit UOp DAG as C99 (F6 step 1)
4736c573 feat(cpu-jit): gated cg_render_uop_kernel_c integration -- F6 step 4
70a997f0 docs(plans+cpu-jit): F6 step 9 -- env-on validation pass + plan-doc update
fc40c60a feat(cpu-jit): F6 step 10 -- flip default ON, render_uop_c is the primary path
3b60fa7c refactor(codegen): F6 step 16 -- delete render_c.c + cg_emit + Renderer struct
1f42df3c feat(cpu-interpret): F6-audit -- THVM_CPU_INTERPRET_TRACE diagnostic
```

## Diagnostic env knobs in tree

```
THVM_CPU_JIT_VIA_UOP=0    Opt out of render_uop_c, fallback to (gone) cg_emit
                          -- with F6 step 15, this no longer routes anywhere;
                          if cpu_jit_build can't lift, dispatch goes to
                          cpu_interpret.
THVM_CPU_INTERPRET_TRACE=1 Print one-line per cpu_interpret entry (op codes
                          dumped). Use to measure F6-finish progress.
THVM_CPU_UOP_WALK=0       Bisection knob -- opt out of the UOp DAG walker
                          (cpu_uop_walk) and route the dispatcher straight
                          to cpu_jit_dispatch / cpu_interpret as before.
                          Default-on after F6-finish (b) step 2 flip.
THVM_CPU_UOP_WALK_TRACE=1 Print one line per cpu_uop_walk entry summarising
                          (n_inputs, root op, return code). Use to measure
                          how many kernels the walker handles end-to-end.
THVM_CPU_JIT_TRACE_FALLBACK=1 (Removed in F6 step 15 along with the fallback.
                              No longer wired.)
THVM_RANGEIFY_BAIL=1       Print rangeify lowering bail reasons. Already in
                          tree; useful for understanding why scalar_uops is
                          NULL on a kernel.
THVM_TILE=0                Force the legacy KProgOp-flat dispatch path off.
                          Default-on after F1.
```
