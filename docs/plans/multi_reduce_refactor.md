# Multi-Reduce-Per-Kernel Refactor

## Why

At BS=128 on the bounded beautiful-mnist canary, **84% of wall time
(575 / 687 ms)** comes from two metal-op fallback kernels that have
two `UOP_REDUCE` ops in their KProgOp programs.  The gate at
[src/schedule/rangeify.c:1410](src/schedule/rangeify.c) bails any
program with `> 1 reduce`, forcing them onto the per-op encoder:

```
case UOP_REDUCE:
  if (reduce_pos != -1) RBAIL_PRE("> 1 reduce");
  reduce_pos = (int)i;
  break;
```

These are BN-grad fused kernels (dgamma + dbeta from the same dy
over the same batch+spatial axes).  Tinygrad handles them in one
kernel via two threadgroup-shared accumulators in a shared inner
loop; that's the missing structural piece.

This refactor is also Phase 5 of [docs/plans/bufferize.md] ("Reduce-
Aware Bufferize").  Lifting the gate is the most concrete bit of
single-reduce assumption left in the post-Phase-7 pipeline.

## Pipeline impact

The refactor spans three layers:

```
KProgOp[] (legacy, on KernelEntry.program)
  ↓ rangeify  (semantic lowering)
ScalarUop[] (per-kernel scalar dataflow)
  ↓ tile_build_from_scalar  (schedule/memory plan)
TileUop[] (TILE_LOOP_NEST / TILE_REDUCE / TILE_STORE / TILE_SCALAR_BODY)
  ↓ render
Metal MSL / C source
```

| Layer | Current state | Needs |
|---|---|---|
| Rangeify | Bails at `> 1 reduce` (rangeify.c:1410); single-reduce metadata in scalars `red`, `reduce_kind`, `reduce_size`, `reduce_inner`, `reduce_n_ranges`, `reduce_extents`. ~119 refs to these. | Per-reduce arrays, emit N `S_REDUCE_SUM` ops, lift gate. |
| Tile | `tile_build_from_scalar` wraps ONE TILE_REDUCE; `tile_find_nested_scalar_reduce` discovers the other case (tile.c:942). | Wrap N TILE_REDUCEs, share inner loop when ranges match. |
| Render | `rmt_emit_value_with_reduce` emits ONE `_acc%u` register + reduce loop; substitutes acc into surrounding expression. | Emit N accumulators, merge inner loops when shared ranges. |

## Strategy

The refactor breaks into 5 atomic commits, each ending with `make
test` green:

### Commit 1 — Introduce per-reduce metadata in rangeify (no behavior change)

- Define a struct `ReduceRunMeta { i32 pos; KProgOp const *op; u32 kind; u32 size; u32 inner; u32 n_ranges; u32 extents[MAX_DIM]; }`.
- Define `MAX_REDUCES_PER_KERNEL = 4` (enough for BN-grad + a couple of related cases; can grow).
- Add `ReduceRunMeta reduces[MAX_REDUCES_PER_KERNEL]; u32 n_reduces = 0;` in `rangeify_try_lower_elementwise`.
- In the existing detect block, populate `reduces[0]` with the same data that goes into the existing scalars.  Keep the existing scalars as duplicate state for now.
- Keep the gate at `n_reduces > 1` (still bails).
- Stops behavior identical; just adds parallel state.

### Commit 2 — Migrate downstream reads to `reduces[0]`

- Find all `red->`, `reduce_kind`, `reduce_size`, etc. uses below the
  detect block.
- Replace with `reduces[0].op->`, `reduces[0].kind`, etc.
- Remove the now-redundant scalars.
- Behavior unchanged.  Pure mechanical refactor.

### Commit 3 — Allow multi-reduce shape detection

- In the gate, when `n_reduces > 1`, populate `reduces[1..N-1]` from
  the additional KProgOp `UOP_REDUCE` entries.
- For each, validate that `n_ranges` and `extents[]` match
  `reduces[0]` exactly (shared reduce-range axes).  If not,
  bail with `"multi-reduce ranges differ"` (a lighter restriction
  than the current blanket `> 1 reduce`).
- Still emit only one reduce in the body (use `reduces[0]`); just
  don't bail.  This lets the gate accept BN-grad's dgamma+dbeta but
  CONTINUES to emit only one of the reduces' value -- semantically
  WRONG.  Gated by `THVM_RANGEIFY_MULTI_REDUCE=1` so the canary
  default stays correct until Commit 4 lands.

### Commit 4 — Emit N S_REDUCE_SUM ops sharing the inner loop

- In the body emit pass, after computing the inner-reduce body
  expression, iterate `n_reduces`.  For each, compute its specific
  body subexpression (different inputs / pre-reduce ops) and emit
  one `S_REDUCE_SUM` consuming the shared reduce-range nest.
- Multi-output structure: each reduce becomes one of the kernel's
  output buffers (use the existing
  `KernelEntry.extra_output_*[]` schema introduced in step-1 of the
  multi-output kernel work).
- Tile build: extend `tile_build_from_scalar` to wrap each reduce
  in its own TILE_REDUCE that shares the same TILE_LOOP_NEST axes.
- Render: extend `rmt_emit_value_with_reduce` to walk N
  accumulators in the shared loop.

### Commit 5 — Make `THVM_RANGEIFY_MULTI_REDUCE=1` default-on

- Run BS=128 canary, confirm metal-op fallbacks lift to metal-tile.
- Run BS=32 canary, confirm no regression.
- Update `docs/plans/profiling_methodology.md` Tables A/B with new
  numbers.
- Flip default to ON.

## Side cleanups (orthogonal, can land any time)

- `KDISPATCH_METAL_GEMV = 12` — retired enum value, only the
  comment uses it.  Removing requires updating the WL-side
  `$dispatchKindNames` table at [wl/THVMLink/Kernel/Kernel.wl:281]
  too; otherwise the integer 12 maps to `"unknown"` after retirement.
- The "legacy" path comments in `materialize.c` for the
  single-output multi-output schema (around line 700) — the schema
  has shipped; some comments still describe it as "legacy single-
  output path", which is now the n=0-extras case rather than a
  legacy fallback.  Tighten language.
- `realize_classify` -> bufferize migration is the larger Phase
  5+ work tracked in [docs/plans/bufferize.md]; mention it in the
  "Future Work" section.

## Testing

- Each commit lands with `make test` green (274/274).
- Add focused unit tests in `tests/test_rangeify.c` (or extend an
  existing one) for the multi-reduce shapes:
  - Two REDUCE_SUM over the same axes from the same source (the
    cleanest BN-grad shape).
  - Two REDUCE_SUM with different bodies but shared ranges.
- Canary check at BS=32 (must stay competitive) and BS=128 (target
  metric: kid=6 / kid=9 lift to metal-tile, total wall drops to
  ~150ms).

## Risk / Bail Conditions

- If Commit 3's gate relaxation surfaces multi-reduce kernels with
  DIFFERENT reduce-range shapes (less-uniform than BN-grad), bail
  early; the simple shared-loop approach won't cover them.  Add
  a `n_reduces == 1` exit from rangeify and let those kernels fall
  through to per-op metal-op (existing behavior).  Don't try to
  cover the general case in this refactor.
