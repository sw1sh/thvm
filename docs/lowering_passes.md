# Lowering Passes

This document explains how a high-level UOp graph gets compiled into
GPU/CPU dispatches.  It is meant to be read top-to-bottom by someone
who is new to the schedule layer; the goal is "I now understand what
each pass takes in and produces, and where to look in the code."

## At a glance

```
   user / WL
      |
      v
   UOp graph (TAG_UOP heap cells)              <- src/uop/*.c
      |
      | uop_graph_simplify_materialize         <- src/uop/graph_simplify.c
      |   algebraic + movement rewrites
      v
   bufferize_classify (= former realize_classify)
      |                                        <- src/schedule/bufferize_classify.c
      |   walks DAG, marks ROOT/MULTI/REDUCE
      |   boundaries in BUFFERIZE_NODES[]
      |   then runs named rewrite rules from
      |   bufferize_rewrite.c.  After this
      |   pass each entry has realized=0/1
      |   plus a bufferize-graph mirror
      |   (BUFFERIZE_BUFS[]) carrying B_INDEX
      |   edges, cost-model fields, etc.
      v
   topo_sort_boundaries + plan_kernel_merges    <- materialize.c
      |   per-boundary depth, last-use, and
      |   merge-into hints
      v
   for each boundary in topo order:
      v
   emit_kernel_for_boundary                    <- materialize.c:2099
      |   visit() walks from boundary root,
      |   converts UOp -> KProgOp[] (one
      |   slot per op, all of the same
      |   intrakernel dataflow), populates
      |   KernelEntry { program, input_tids,
      |   output_tid, output_shape, ... }
      |
      |   axes_default_for                     <- src/codegen/axis.c
      |     LOOP per output dim + trailing
      |     REDUCE if last op is UOP_REDUCE
      |
      |   rangeify_try_lower_elementwise       <- src/schedule/rangeify.c
      |     KProgOp[] -> ScalarUop[] (S_RANGE,
      |     S_INDEX, S_LOAD, S_STORE,
      |     S_REDUCE_SUM, S_BUFFERIZE, etc.)
      |     If it fails, ke->scalar_uops stays
      |     empty and we keep the legacy
      |     KProgOp dispatch path.
      |
      |   tile_build_from_scalar               <- src/schedule/tile.c
      |     ScalarUop[] -> TileUop[]
      |     (TILE_LOOP_NEST, TILE_STORE,
      |     TILE_REDUCE, TILE_SCALAR_BODY,
      |     TILE_AXIS).  Plain wrapper today;
      |     future cost-model lives here.
      v
   At dispatch (cpu_interpret / metal_dispatch_kernel)
      |
      |   Try paths in order:                  <- src/backend/metal/_.m
      |     1. metal_alias_reshape (no compute)
      |     2. metal_try_conv2d_flat / gemm
      |     3. metal_try_cpu_small_add
      |     4. cg_supports_metal_reduce_expr ->
      |        metal_jit_encode (legacy single
      |        MSL kernel from KProgOp)
      |     5. tile_supported (rangeify+tile
      |        succeeded AND
      |        cg_tile_metal_dispatch_shape
      |        derived a (groups, threads)) ->
      |        metal_tile_jit_encode
      |     6. fall through: metal_jit_encode
      |        (single-encoder MSL JIT)
      |     7. per-op encoder fallback
```

The pipeline mixes "old" (KProgOp-based) and "new" (ScalarUop / TileUop)
IRs.  Both paths exist concurrently; the new path lives ALONGSIDE the
old, and dispatch picks at runtime per kernel.  When rangeify accepts
a kernel, the tile-jit path runs; otherwise we fall back to the
legacy KProgOp-based renderer.

---

## Pass 1: UOp graph simplify

- File: [src/uop/graph_simplify.c](../src/uop/graph_simplify.c)
- Entry: `uop_graph_simplify_materialize(term, depth)`
- Called from: [thvm_materialize](../src/schedule/materialize.c#L2399)

This is a peephole rewriter on the heap-allocated UOp DAG.  Each rule
matches a small structural pattern (e.g.
`REDUCE_SUM(MUL(x, CONST))` -> `MUL(REDUCE_SUM(x), CONST)`) and
returns a rewritten Term.  The driver loops until no rule fires.

Rule families:

- algebraic (commutativity, identity, associativity);
- movement (collapse expand-of-expand, reshape-of-reshape, etc.);
- dtype (cast normalisation);
- mask-aware (where(cond, x, x) -> x);
- a few tinygrad-port rules (collect-mul-add,
  reduce-add-const-distribute, reduce-const-mul-distribute).

The pass is structural; it doesn't know about kernel boundaries and
runs entirely on heap cells.  Fixed-point.

---

## Pass 2: bufferize_classify

- File: [src/schedule/bufferize_classify.c](../src/schedule/bufferize_classify.c)
  (formerly `realize_classify.c`; renamed in commit `6242afa` as part
  of the realize_classify retirement; see `docs/plans/bufferize.md`)
- Entry: `bufferize_classify(Term root)`
- Storage: `BUFFERIZE_NODES[]` (per-walked-UOp dense table) plus
  `BUFFERIZE_BUFS[]` (the bufferize-graph view, ROOT/MULTI/REDUCE
  realized + rule mutations) plus `BUFFERIZE_INDEXES[]` (B_INDEX
  edges between buffers)

What it does:

1. **DAG walk** (`bufferize_walk_rec`) populates `BUFFERIZE_NODES[i]`
   with `{loc, op, consumer_count}` for every reachable UOp.
2. **Boundary seeding** marks `realized=1` plus a reason flag on:
   - the realize root (`BUFFERIZE_REASON_ROOT`),
   - any UOp consumed by ≥2 distinct parents
     (`BUFFERIZE_REASON_MULTI`),
   - every `UOP_REDUCE` (`BUFFERIZE_REASON_REDUCE`).
3. **Bufferize seed** (`bufferize_seed_from_nodes`) projects realized
   nodes into `BUFFERIZE_BUFS[]` with stable 1-based `buffer_id`,
   reason bits, `is_root`, and (later) cost-model fields.
4. **Rule rewrites** (`bufferize_rewrite_apply`) runs named rules
   that mutate the realized set:
   - `inline-constants`,
   - `inline-adjacent-reduce-chains`,
   - `inline-softmax-broadcast-reduce`,
   - `inline-reduce-scalar-tail`,
   - `inline-large-expand-fanout`,
   - `inline-reduce-fanout`,
   - `remove-removable-bufferize`,
   - `remove-by-cost-score` (default-on),
   - `inline-pure-fanout-probe`,
   - `metal-tile-fanin-cap`.
   Each rule wraps `bufferize_set_current_rule(name)` so every
   `realize`/`unrealize` it triggers stamps `added_by`/`removed_by`
   on the affected `BBufferize` record.
5. **Index build** (`bufferize_finalize_stores` ->
   `bufferize_build_indexes`) constructs one `B_INDEX` per
   producer-buffer→consumer-buffer edge, with the movement-op chain
   between them encoded as `BIndexChainOp[]` (op, src/out dims, pad
   widths, axis perm, flip mask).
6. **Cost + lifetime** (`bufferize_compute_costs`,
   `bufferize_compute_lifetimes`) populate the post-rewrite
   `recompute_ops`, `output_numel`, `lifetime_start`/`lifetime_end`,
   `output_bytes` on each realized BBufferize.

Output: a fully populated bufferize graph plus a `realized` bit on
every `BUFFERIZE_NODES` entry.  The materialize pass downstream
iterates realized entries in topological order.

References for further reading: `docs/plans/bufferize.md` (the
authoritative roadmap; phases 0-7 are landed at the data-layer
level), and `bufferize_classify.c`'s file-header comment.

---

## Pass 3: topo_sort + plan_kernel_merges

- File: [src/schedule/materialize.c](../src/schedule/materialize.c)
- After bufferize_classify.

Computes per-boundary `BOUNDARY_DEPTH` (max producer depth + 1) and
`BOUNDARY_LAST_USE` (last consumer depth) tables, then sorts realized
boundaries into `BOUNDARY_ORDER[]`.  `plan_kernel_merges` decides
which child boundary (if any) to splice into a parent kernel as an
extra output (multi-output kernel arc).

The depth/last-use arrays drive the memory planner: when emitting
kernel `i` at depth `D`, every kernel whose last-use depth was `<D` is
"dead" and its output buf is pushed onto the backend freelist for
reuse before allocating this kernel's output.

---

## Pass 4: visit() — UOp DAG -> KProgOp[]

- File: `materialize.c`, function `visit` (called from
  `emit_kernel_for_boundary`)
- Output: `KernelEntry { program: KProgOp[], input_tids[],
  input_views[], output_tid, output_shape, output_numel, ... }`

Walks the boundary's compute subtree top-down, with memoisation
(`VisitMemo`).  At each non-leaf UOp it:

- routes leaves (TEN/VAR aliases, CONST broadcasts) to input slots
  via `input_slot_dedup`,
- falls through chains of pure movement ops where view_resolve
  yields a TenDesc alias (no work to emit),
- otherwise emits one `KProgOp` per UOp.  Each KProgOp records:
  - opcode, dtype, output numel,
  - up to `MAX_UOP_SRC` source references (KSRC_AS_INPUT(slot) or
    earlier program index),
  - shape metadata: `src0_dims`, `out_dims`, `pad_widths`,
    `axis_perm`, `reduce_axes`, plus a per-USE `chain_op_idx` that
    links back to the corresponding `BIndexChainOp` on the
    originating B_INDEX edge,
  - opcode-specific arg field (CONST bits, REDUCE kind+inner, etc.).

`KProgOp.numel` is u64 as of commit `cbc77d2` so kernel programs with
intermediates exceeding 2^32 elements (e.g. BS=512 conv-bwd dInput's
5.24-billion-element EXPAND) store the correct count.  Pre-fix, the
silent u32 overflow trapped via the rangeify reduce-shape divisibility
guard; see `docs/plans/profiling_methodology.md` §4.6.

---

## Pass 5: axes_default_for

- File: [src/codegen/axis.c](../src/codegen/axis.c)
- Entry: `axes_default_for(KernelEntry *ke)`

Default `KernelAxes`: one `KAX_LOOP` axis per output dim, plus a
trailing `KAX_REDUCE` axis sized at `src_numel / out_numel` if the
last KProgOp is `UOP_REDUCE`.  Other axis types (`KAX_GLOBAL`,
`KAX_LOCAL`, `KAX_GROUP_REDUCE`, `KAX_UPCAST`, `KAX_UNROLL`) are
populated by autotune (`apply_opt.c`) — see
`docs/plans/profiling_methodology.md` §4.5 for the deferred
shape-aware GROUP_REDUCE work.

KernelAxes is shared per-program-shape via the `kernel_program_cache`,
so an opt applied to one kernel propagates to every kid with the
same KProgOp signature.

---

## Pass 6: rangeify

- File: [src/schedule/rangeify.c](../src/schedule/rangeify.c)
- Entry: `rangeify_try_lower_elementwise(KernelEntry *ke)`
- Output: `ke->scalar_uops[]` (a parallel per-LOOP-iter scalar program)

Where the heavy lifting happens.  Rangeify takes the KProgOp[] and
produces a ScalarUop graph that expresses the kernel as one
flattened scalar program, with explicit `S_RANGE` axes and `S_INDEX`
addresses.

Major components:

1. **Pre-emit gate**: structural checks (supported dtypes, supported
   opcodes, sane reduce shapes, view offsets within u16, no negative
   strides outside FLIP).  Bails as `RBAIL_PRE`.
2. **Reduce metadata** is collected into `reduce_meta[r]` for each
   `UOP_REDUCE` in program order.  Chain reduces (n_reduces > 1) get
   one entry per reduce; the chain-reduce work in commits `9fc4b16`
   .. `48420ce` made every per-reduce field accessible via
   `region[i]` so the body emit can dispatch the right reduce per
   op.
3. **`region[i]` map**: which reduce's body op `i` belongs to, used
   to key per-region input scope tables (`input_*_in_region[r]`).
4. **Range emission**: one `S_RANGE` leaf per axis (LOOP for output
   dims, REDUCE for each reduce's axes).
5. **Backward walk**: traverses each KProgOp in REVERSE order to
   compute the axis context (`RngsCtx`) each op's sources see.
   Movement ops (RESHAPE, PERMUTE, EXPAND, PAD, SHRINK, FLIP)
   transform the rngs by composing index expressions.
6. **Per-region input loads**: for each input slot used in region
   `r`, emit one `S_LOAD` whose address is a per-axis sum of
   `range[d] * stride[d] + offset`.  Different regions of the same
   input get different LOADs because their iter contexts differ.
7. **Forward emit**: walk KProgOps in order, emit one `S_*` per op:
   - `UOP_CONST` -> `S_CONST`,
   - elementwise ALU -> `S_ADD/MUL/RECIP/...`,
   - `UOP_REDUCE` -> `S_REDUCE_SUM` or `S_REDUCE_MAX` over the right
     reduce's ranges,
   - movement ops -> identity (input loads already absorbed the
     movement).
8. **Output store**: `S_STORE(S_INDEX, final_value)`, wrapped in
   `S_BUFFERIZE` carrying the LOOP ranges so the dispatcher knows
   the loop nest.

Scalar simplification (`scalar_simplify_divandmod`,
`scalar_uops_simplify`) runs after to fold constant addresses and
algebraic identities.

`RBAIL_PRE` / `RBAIL_MID` macros print a one-line diagnostic when
`THVM_RANGEIFY_BAIL=1` is set; commit `35f7ef5` converted the silent
`return 0` sites in the reduce-shape gate to explicit RBAIL messages
so the bail set is visible.

When rangeify succeeds, `ke->scalar_uops != NULL` and the legacy
KProgOp[] also stays available; downstream picks per-dispatch.

---

## Pass 7: tile_build_from_scalar

- File: [src/schedule/tile.c](../src/schedule/tile.c)
- Entry: `tile_build_from_scalar(KernelEntry *ke)`
- Output: `ke->tile_uops[]`

Wraps the ScalarUop graph into a TileUop schedule:

```
TILE_LOOP_NEST(
  TILE_STORE(
    TILE_SCALAR_BODY(value_id)
  ),
  TILE_AXIS, TILE_AXIS, ...        // one per LOOP / REDUCE / etc.
)
```

For reduce kernels:

```
TILE_LOOP_NEST(
  TILE_STORE(
    TILE_REDUCE(TILE_SCALAR_BODY(value_id))
  ),
  ...
)
```

The TileUop layer doesn't add scheduling decisions yet — it's a
faithful nesting view of the ScalarUop graph.  Future cost-model
work (Phase 5+ of `docs/plans/bufferize.md`) lives here.

See `docs/plans/tile_uops.md` for the data-layer contract.

---

## Pass 8: dispatch (cpu_interpret / metal_dispatch_kernel)

- File for Metal: [src/backend/metal/_.m](../src/backend/metal/_.m)
- For CPU: [src/backend/cpu/](../src/backend/cpu/) — interpreter,
  BLAS pattern matcher, clang-JIT.

At runtime the dispatcher tries paths in order, recording the chosen
path via `cg_profile_record` (`KDISPATCH_*` enum):

| Path | Trigger | Notes |
|---|---|---|
| `KDISPATCH_METAL_ALIAS` | `metal_try_alias_reshape` | Pure view, no compute. |
| `KDISPATCH_METAL_CONV` | `metal_try_conv2d_flat` (diagnostics) | Specialised conv2d shader. |
| `KDISPATCH_METAL_GEMM` | `metal_try_gemm` | MMA / matmul. |
| `KDISPATCH_METAL_OP` | `metal_try_cpu_small_add` first; later per-op encoder | Small adds on CPU; otherwise one Metal encoder per KProgOp (slowest). |
| `KDISPATCH_METAL_TILE` | `tile_supported && metal_tile_jit_encode` | Rangeify+tile path; uses the ScalarUop / TileUop programs. |
| `KDISPATCH_METAL_JIT` | `metal_jit_encode` | Single-encoder MSL JIT directly from KProgOp[]; `cg_emit_metal` (`render_metal.c`) renders the kernel.  No GLOBAL/LOCAL axis assignment; threads = `program[n_ops-1].numel`. |

`tile_supported` is `metal_tile_enabled() && cg_tile_metal_dispatch_shape(...)`.
`cg_tile_metal_dispatch_shape` collects axis info via `tile_sync_from_scalar` →
`rmt_collect_kernel_info` → `rmt_axis_mode` (FLAT_GRID / LOCAL_GLOBAL /
GROUP_REDUCE) and computes `(groups, threads)`.  If anything fails
(e.g. no scalar_uops, axis mode unsupported), falls through to
metal_jit_encode.

**The fast path (`KDISPATCH_METAL_TILE`) requires every pass to
have succeeded**: rangeify produced ScalarUops, tile built a nest,
axes mode is supported by the renderer.  The two main fall-throughs
are:

1. Rangeify rejected the kernel (e.g. unsupported opcode, bad
   reduce shape, divisibility guard, > 65535 reduce_size cap).
   Falls to metal_jit_encode.
2. Rangeify accepted but `cg_tile_metal_dispatch_shape` rejected
   (axis mode unsupported, total threads > u32, ...).  Falls to
   metal_jit_encode.

Both fall-through cases run the legacy `cg_emit_metal` (which renders
the KProgOp[] directly) at much lower hardware utilisation.  Closing
this gap is the active work in `docs/plans/bufferize.md` Phase 3 and
`docs/plans/profiling_methodology.md` §4.5.

---

## Cross-cutting infrastructure

- **kernel_program_cache** (`src/schedule/kernel_program_cache.c`):
  hash-cons of KProgOp[] arrays.  Two boundaries with bit-identical
  programs share the underlying array AND a `KernelAxes` slot.  Means
  one autotune decision applies to every kid with the same program
  shape.

- **memory plan** (`src/schedule/kernel_alloc.c`,
  `src/schedule/kernel_gc.c`): consumes BOUNDARY_DEPTH /
  BOUNDARY_LAST_USE to push dead kernel buffers onto the backend
  freelist before each new kernel allocates its output.

- **JIT capture / replay** (`src/jit/capture.c`, `src/jit/replay.c`):
  records the dispatch sequence on first run; replays as a single
  Metal `MTLIndirectCommandBuffer` on subsequent runs, amortising
  encoder overhead across a step's worth of kernels.  Default-on
  via `THVM_METAL_GRAPH_REPLAY`; capacity capped by
  `THVM_METAL_GRAPH_MAX_DISPATCHES`.

- **Profile** (`src/codegen/profile.c`):
  `cg_kernel_dispatch_kind(kid)`, `cg_kernel_total_us(kid)` etc.
  Surfaced through `TKernelProfile[kid]` on the WL side.  Every
  dispatch path records via `cg_profile_record(kid, kind,
  elapsed_us)`.

---

## Where to look when debugging

| Symptom | First thing to try |
|---|---|
| Wrong kernel output | `cpu_interpret` path: legacy KProgOp[] reference; bisect against rangeify by setting `THVM_RANGEIFY=0`. |
| "metal-jit" path running unexpectedly | Set `THVM_RANGEIFY_BAIL=1`; you'll see `rangeify bail (pre-emit/mid-emit): <reason>` for every rejected kernel. |
| Schedule decisions look wrong | `DUMP_BUFFERIZE=1` prints the per-buffer table; `DUMP_BUFFERIZE_CANDIDATES=1` shows removal-rule candidate scores; `THVM_RANGEIFY_BAIL=1` separately covers rangeify. |
| Memory blow-up | `SHOW_MEMORY_PROFILE=1` prints `peak_live` / `peak_retained` etc. |
| Kernel count regression | Profile via `SHOW_PROFILE=1`; group by `TKernelProgramKey` to see shape-distinct kernels. |
| Per-kernel GPU time | `THVM_METAL_PROFILE_PEROP=1` replaces batched ICB execute with per-op encoder dispatches that capture true GPU timestamps; ~3.4× wall inflation but accurate per-kernel breakdown.  Documented in `docs/plans/profiling_methodology.md` §4.3. |

---

## Related plan docs

- `docs/plans/bufferize.md` — the authoritative bufferize-IR roadmap;
  defines what rules SHOULD eventually run on the explicit graph.
- `docs/plans/profiling_methodology.md` — apples-to-apples comparison
  methodology + cross-framework numbers.  §4.6 is the BS=512 table.
- `docs/plans/multi_reduce_refactor.md` — chain-reduce architecture
  in rangeify; commits `9fc4b16` .. `48420ce`.
- `docs/plans/scalar_uops_lowering.md` — ScalarUop op-by-op contract.
- `docs/plans/tile_uops.md` — TileUop data-layer contract.
- `docs/plans/rangeify_apply_movement_op_progress.md` — port log of
  tinygrad's `apply_movement_op`-style INDEX rewrites.
