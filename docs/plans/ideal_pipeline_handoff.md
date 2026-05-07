# Ideal Pipeline -- Handoff (2026-05-08)

This is the active continuation point for [ideal_pipeline.md](ideal_pipeline.md).
A long session (~35 commits) closed the F3.5 unblocker and the F6 main goal,
documented several remaining wedges concretely, and ran into a misdirection
worth flagging so the next agent doesn't repeat it.

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
THVM_CPU_JIT_TRACE_FALLBACK=1 (Removed in F6 step 15 along with the fallback.
                              No longer wired.)
THVM_RANGEIFY_BAIL=1       Print rangeify lowering bail reasons. Already in
                          tree; useful for understanding why scalar_uops is
                          NULL on a kernel.
THVM_TILE=0                Force the legacy KProgOp-flat dispatch path off.
                          Default-on after F1.
```
