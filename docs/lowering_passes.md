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

---

## Ideal pipeline (forward-looking)

The pipeline above is what's in the tree today.  It works but carries
a lot of structural cruft: three parallel IRs (KProgOp[], ScalarUop[],
TileUop[]), a side-channel KernelAxes, six runtime dispatch paths,
two boundary-tracking arrays, and movement ops that exist as both
heap UOps AND KProgOps AND consumed-edge metadata on B_INDEX.

What tinygrad and tilelang teach is that **a single DAG IR with
movement-as-index and axes-as-leaves** removes most of the bookkeeping.
This is the target architecture; everything else is migration cost.

### Target shape: 3 IRs, each with one job

The right number of IRs isn't ONE (tinygrad-flat) or FOUR (current
KProgOp + ScalarUop + TileUop + backend) — it's THREE, each with a
clear scope.  Tilelang is the relevant analogue: their tile-IR is
where memory scopes, thread bindings, and vectorisation live, and
that layer earns its keep.

```
   user / WL
      |
      v
   UOp DAG                                     <- IR 1: tensor expressions
      |   pure-functional, hash-consed.        ALL movement-as-INDEX
      |   simplify (algebraic + movement-      rewrites land here.
      |   to-INDEX) and schedule (insert       Schedule decisions
      |   BUFFERIZE/STORE at boundaries)       (which buffers
      |   are rewrites on this IR.             materialise) are
      |                                        BUFFERIZE node insertion
      |                                        / removal in pass 2.
      |
      |  pass 1: simplify                      tinygrad-style
      |     PERMUTE/RESHAPE/EXPAND/PAD/        PatternMatcher.
      |     SHRINK/FLIP rewrite to INDEX
      |     expressions; algebraic folds;
      |     dtype norm; specialisation match
      |     (gemm, conv).
      |
      |  pass 2: schedule                      named cost-modeled
      |     for each kernel boundary, insert   rules over BUFFERIZE
      |     B_BUFFERIZE + B_STORE + B_INDEX    nodes. Removal
      |     UOp nodes.  Rules promote /        rewrites can fold an
      |     remove boundaries based on cost.   intermediate buffer
      |                                        through movement-as-
      |                                        INDEX.
      v
   Tile IR                                     <- IR 2: scheduled compute.
      |   per-kernel: explicit loop nest,      tilelang's TIR analogue.
      |   axis types (parallel / vectorize /   memory hierarchy
      |   unroll / reduce), memory scopes      (global / shared /
      |   (global / shared / register),        register) lives here,
      |   thread binding, MMA fragments.       NOT in the UOp DAG.
      |                                        autotune (BEAM) is
      |                                        rewrite-search on
      |                                        this IR.
      |
      |  pass 3: lower                         UOp DAG (per kernel)
      |     UOp DAG -> Tile IR.  Each          becomes a TILE_LOOP_NEST
      |     B_STORE-rooted subgraph maps to    with TILE_AXIS leaves
      |     a TILE_LOOP_NEST whose axes        (typed), TILE_REDUCE
      |     come from the kernel's RANGE       wrappers, and a scalar
      |     leaves; reductions become          body subtree of S_*
      |     TILE_REDUCE; the body is the       ops.  This is what
      |     scalar expression tree.            tile.c builds; today it
      |                                        is structural-only,
      |                                        the migration is to
      |                                        ADD the schedule
      |                                        annotations.
      |
      |  pass 4: schedule annotations          per-axis decisions:
      |     rewrite TILE_AXIS leaves with      LOOP / GLOBAL / LOCAL /
      |     types and bind shared-memory       UPCAST / UNROLL /
      |     allocations, vectorisation         GROUP_REDUCE.  Memory
      |     widths, thread-block sizes.        allocations and
      |                                        barriers become
      |                                        explicit TILE_ALLOC
      |                                        and TILE_BARRIER nodes.
      v
   pass 5: render                              pretty-printer.
      | emit MSL / C / CUDA from the           every backend is a
      | fully annotated Tile IR.  No           rendering of the same
      | decisions left; the renderer is        Tile IR with backend-
      | a backend-specific tree walker.        specific dialect quirks.
      v
   one dispatch path                           one MTLComputePipelineState
                                               per kernel; ICB replay
                                               amortises encoder cost.
```

### Why three IRs and not one (correction)

I previously suggested collapsing tile.c into the UOp DAG.  That was
wrong — the UOp DAG is the right place for **what to compute**
(values, indices, reductions); the Tile IR is the right place for
**how to compute it on a GPU** (loop nest, memory scope, thread
binding, vectorisation, MMA).  Mixing those into one IR is what
makes tinygrad's `linearize` pass complex: the same UOp graph has to
encode both at once.

Tilelang's lesson: keep them separate.  The Tile IR has explicit
`T.thread_binding`, `T.alloc_buffer`, `T.copy`, `T.gemm`, etc.  These
are tile-IR-level constructs, not tensor-IR-level.  Autotune lives at
the Tile IR layer because that's where the cost surface is —
parallel-dim choices, vectorisation widths, shared-memory tile sizes
— not at the value-computation layer.

### What each existing pass collapses into

| Today | Ideal | Why it merges or moves |
|---|---|---|
| `uop_graph_simplify` | pass 1 (simplify) | Already a PatternMatcher; expanded to include movement-to-INDEX. |
| `bufferize_classify` walk + seeding | pass 2 (schedule) | The walker is "find ROOT/MULTI/REDUCE and insert BUFFERIZE" — a rewrite, not a separate pass. |
| `bufferize_rewrite_apply` named rules | pass 2 (schedule) | Same rule family, same driver. |
| `topo_sort_boundaries` + `plan_kernel_merges` | falls out of pass 2 | Topo order is the DAG; multi-output is one BUFFERIZE with multiple STORE children. |
| `visit()` UOp -> KProgOp[] | **gone** | KProgOp[] disappears.  Pass 3 lowers the UOp subgraph between two BUFFERIZE nodes directly into Tile IR. |
| `rangeify_try_lower_elementwise` | split into pass 1 + pass 3 | Movement-to-INDEX (~30% of rangeify) becomes pass-1 rules.  The remaining range emission + reduce structure (~70%) becomes pass 3 (UOp -> Tile IR), which is much shorter than today's rangeify because it doesn't need to bridge two semantically-different IRs. |
| `tile_build_from_scalar` | **stays as pass 3, evolves** | Currently produces a structural view; needs to grow real schedule annotations (memory scopes, thread bindings, vectorisation).  The file STAYS; it just gets more semantic content. |
| `axes_default_for` + `propose.c` + `apply_opt.c` | pass 4 (annotations) | All become rewrites on TILE_AXIS leaves.  Defaults are seed rules; autotune is BEAM search over the rewrite set. |
| `cg_emit` + `cg_emit_metal` + `metal_tile_jit_encode` + `metal_jit_encode` + `cg_supports_metal_reduce_expr` + `metal_try_*` specialisations | pass 5 (render) | All collapse into one Tile-IR renderer per backend.  Specialisations (gemm, conv2d_flat) are pattern-matched in pass 1 and rewritten to canonical UOp shapes; pass 3 lowers them to canonical Tile-IR (TILE_GEMM, TILE_CONV2D); the renderer emits one MSL function template per shape. |

### Concrete deletion list (revised)

What disappears from `src/`:

- `schedule/rangeify.c` (3.2k lines): **delete**.  Replaced by:
  - movement-to-INDEX rules added to `uop_graph_simplify.c` (pass 1)
  - a new ~600-line `schedule/lower_to_tile.c` (pass 3) that walks
    the per-kernel UOp subgraph and emits Tile IR.
- `schedule/kernel_program_cache.c`: **delete**.  With UOp hash-consing in `uop/*`, kernel programs are subgraphs and dedup naturally.
- `codegen/render_c_scalar.c`: **delete** (the C and Metal renderers stay).
- `codegen/axis.c` + `codegen/apply_opt.c` + `codegen/propose.c`: **collapse** into one ~300-line `tile_anno.c` (pass 4) that owns axis-type rewrites; autotune (`codegen/autotune.c`) drives BEAM over those rewrites.
- The `KProgOp` struct in `thvm.h` (~80 lines) plus everything that consumes it (`backend/cpu/op/*.c` opcode interpreters, `backend/cpu/jit.c`, the `cg_supports*` family in `codegen/cg.c`): **delete**.  CPU dispatch routes through the same Tile-IR renderer (emit C, clang-JIT, run).
- The dispatch ladder in `backend/metal/_.m` (alias / conv / gemm / op / tile / jit): **collapse** to one path.  Pattern matching in pass 1 produces specialised UOp shapes; pass 3 lowers them to canonical Tile-IR; renderer emits one MSL function template per shape.
- `BUFFERIZE_NODES[]` vs `BUFFERIZE_BUFS[]` storage split (Step 6 of the realize_classify retirement that was deferred): **collapse**, since "the graph" is the storage.

What **stays and grows** (the Tile IR layer):

- `src/schedule/tile.c` — **stays, evolves**.  Today's TILE_LOOP_NEST / TILE_REDUCE / TILE_STORE / TILE_SCALAR_BODY structure becomes the foundation; the migration adds **TILE_ALLOC** (shared-mem allocations), **TILE_BARRIER** (threadgroup syncs), **TILE_GEMM / TILE_CONV2D** (specialised compute), and richer **TILE_AXIS** with memory-scope and vectorisation annotations.
- `src/schedule/uop_meta.c` — stays, used by both pass 1 (shape inference for INDEX rewrites) and pass 3 (Tile-IR lowering).
- `src/schedule/bufferize.c` — keep AS the schedule-decision primitives.  B_BUFFERIZE / B_STORE / B_INDEX stay as first-class UOps in the DAG; the SECONDARY storage (BUFFERIZE_BUFS[] mirror) goes away.

What stays unchanged (backend infrastructure):

- `src/uop/*.c` — UOp constructors. Already the right shape.
- `src/jit/capture.c` + `src/jit/replay.c` — ICB capture/replay.
- `src/schedule/realize.c` — one-shot WNF + materialize wrapper. Becomes a thin caller of pass 1..5.
- `src/schedule/kernel_alloc.c` + `kernel_gc.c` — Metal buffer allocation + lifetime.
- `src/codegen/render_metal.c` — keep, simplified to a fully Tile-IR-typed renderer.

Net: `src/schedule/` drops from 16 files to ~7 (the new
`lower_to_tile.c` plus `tile.c` evolves with new node types).
`src/codegen/` drops from 9 files to ~4.  Total ~4k lines deleted
from compile-pipeline code, plus ~800 lines added in tile-IR
schedule annotations and the new lower pass, for a net ~3.2k
reduction.

### Key tinygrad insights

1. **Movement ops are NOT data**.  PERMUTE / RESHAPE / EXPAND / PAD /
   SHRINK / FLIP are NEVER materialised as buffers OR as KProgOps.
   They rewrite the consumer's INDEX expression.  An EXPAND with
   src `[1, N]` and out `[M, N]` becomes `INDEX(src, [m * 0 + n * 1])`
   — `m * 0` is the broadcast.  No materialisation, no copies.

2. **ShapeTracker = stack of Views**.  The mechanism behind (1).
   Each tensor has a `ShapeTracker` that's a STACK of `View` records;
   each movement op pushes a new View on top.  At consumer time, the
   stack is COMPRESSED into a single symbolic INDEX expression.
   Reshape-of-permute-of-reshape doesn't allocate intermediate
   buffers — the View stack composes the address arithmetic.  thvm's
   `View` struct is per-tensor (one View, not a stack); the stack
   form is what makes complex movement chains free.

3. **Symbolic INDEX simplifier**.  After movement-as-INDEX, indices
   are symbolic expressions over RANGE leaves: `+`, `*`, `//`, `%`.
   A constant-folder + range-aware simplifier collapses common
   patterns: `x % N` where `x < N` → `x`; `(x // M) * M + (x % M)`
   → `x`; stride-0 axes drop out.  Without this simplifier the
   addresses computed at every iteration are gigantic ASTs that
   the MSL compiler can't optimise.  thvm has `scalar_simplify_*`
   but the ranges-aware folds aren't yet there.

4. **Boundaries are nodes, not flags**.  `BUFFERIZE(value)` is a UOp
   that means "this value gets a backing buffer".  The schedule pass
   inserts and removes BUFFERIZE nodes; everything downstream just
   walks the graph.  No separate `realized` bit on a side-channel
   table.

5. **One dispatch path**.  Tinygrad doesn't have "metal-op fallback"
   or "metal-jit fallback".  Every kernel goes through `linearize`
   (the rangeify analogue) and the renderer.  Specialised shapes
   (conv2d_flat, gemm) are detected as PATTERNS and rewritten to
   canonical forms; the renderer emits one MSL function for every
   kernel.

6. **Autotune is rewrite search**.  BEAM proposes axis-type rewrites
   on the same UOp graph and runs the result.  No `apply_opt`
   mutation of side state, no `KernelAxes.applied_opts[]` log to
   replay.  The chosen axis-type configuration IS the lowered graph.

7. **PatternMatcher as universal rewrite engine**.  Every pass —
   simplify, schedule, linearize, even the renderer's per-target
   dialect adjustments — is a `PatternMatcher` of `UPat` trees and
   their replacements.  ONE engine, hundreds of rules.  Easy to
   extend, easy to test (each rule has a tiny unit test).  thvm has
   `realize_rewrite.c` for one rule family and ad-hoc switches for
   the rest; consolidating onto a single matcher is its own win.

### Key tilelang insights

1. **Schedule is in the IR**.  TileLang's tile-IR has axis types
   (parallel / unroll / vectorise / shared-memory / register) AS
   IR-level annotations on loop nodes.  Lowering = walk the IR and
   emit code that respects the annotations.  No "axes_default_for
   side-table".

2. **Memory hierarchy is THE perf story**.  For matmul, conv,
   attention — the perf gap to peak isn't in arithmetic, it's in
   memory placement.  Tilelang carries `T.alloc_buffer(scope=...)`
   where scope ∈ `{global, shared, local, register}` AS IR
   constructs.  Without this, the renderer can't emit shared-memory
   tiling, and matmul caps at memory bandwidth (~1/100th of peak
   on M3 Max).  thvm's Tile IR needs `TILE_ALLOC(scope, shape)` and
   `TILE_BARRIER` nodes for this to be expressible.

3. **Software pipelining via `T.pipelined`**.  Annotates a loop with
   "overlap memory + compute across iterations" — the compiler
   inserts double-buffered shared-mem loads.  Critical on Ampere /
   Hopper / M3 Max for matmul throughput.  Generates the
   producer/consumer split + async copy.  Currently invisible to
   thvm's pipeline.

4. **Async copy for tensor cores**.  TMA on Hopper, `cp.async` on
   Ampere, M3-style copy primitives.  Tilelang exposes `T.copy`
   and `T.gemm`, the compiler lowers to backend-specific async
   primitives.  In thvm-speak: `TILE_COPY(src, dst, scope)` and
   `TILE_GEMM(A_frag, B_frag, C_frag)` as Tile-IR nodes; backend
   renderer emits `metal::simdgroup_matrix` ops on Apple Silicon.

5. **Pattern-matched specialisations**.  `T.gemm`, `T.copy`,
   `T.reduce` etc. are FUNCTIONS in the source language that the
   compiler matches and lowers to specialised tile-IR.  In
   thvm-speak: `tile_analyze_conv2d_flat` becomes a pattern in
   pass 1 that produces a `TILE_CONV2D` / `TILE_GEMM` Tile-IR node,
   not a runtime check in dispatch.  The renderer has ONE template
   per Tile-IR shape.

6. **Backend-aware rewrites**.  TileLang has separate rewrite passes
   per backend (CUDA / HIP / Metal) that lower the same tile-IR
   differently — not separate IRs, separate **rewrites on one IR**.
   Mirrors what pass 5 (render) should look like.

### Deeper architectural insights (final audit)

Beyond the per-IR mechanisms, there are six architectural choices
the tinygrad/tilelang ecosystem has settled on that thvm hasn't fully
internalised yet.  These are second-order from "ideal pipeline" but
each is a meaningful perf and complexity lever.

**1. Backend-agnostic Tile IR + per-backend renderer.**
Today thvm has `backend/cpu/op/*.c` (24 hand-written CPU op
interpreters: `add.c`, `mul.c`, `reduce.c`, `pad.c`, ...) AND
`codegen/render_metal.c` (Metal MSL renderer).  These are TWO
implementations of the same compute semantics.  Bug fixes have to
land twice; backend-specific quirks leak everywhere.

The right shape: ONE Tile IR.  CPU dispatch becomes "render to C,
clang-JIT, run" — same Tile IR walked by a `render_c.c` instead of
`render_metal.c`.  Per-op CPU interpreters delete entirely (~3k
lines).  Adding a new backend (CUDA / HIP / Vulkan) is then ~500
lines of new renderer, not a parallel `backend/cuda/op/*.c` family.

**2. Shape-folding (contiguous axis collapse).**
`Shape [2, 3, 4]` with strides `[12, 4, 1]` is bit-identical to
`Shape [24]` with stride `[1]`.  A pre-vectorisation pass folds
consecutive contiguous axes into one.  Without this:

  - Every axis becomes a separate loop, killing vectorisation.
  - The trailing axis used for `vload4`/`vload8` is too small.
  - Memory bandwidth on stride-1 access tops out at ~1/4 peak.

Tinygrad's `simplify_movement` runs this fold; thvm's
`uop_graph_simplify` doesn't yet.  Adding it is ~150 lines.

**3. Layout inference.**
Conv2d on M3 Max wants `NHWC` for stride-1 channel access; matmul
wants `NK + KM` (k as inner).  Picking the right layout at COMPILE
time (and inserting PERMUTE only at the kernel-input boundary) is
the single biggest lever for conv/matmul throughput after MMA.

Tinygrad's BEAM includes layout-permute as a search dimension;
tilelang exposes `T.transform` for explicit layout rewrites.  thvm
keeps the user-supplied layout (NCHW everywhere from WL); the
`PERMUTE` ops in conv-bwd's KProgOp programs ARE thvm's layout
machinery, but they materialise instead of getting absorbed.  Once
movement-to-INDEX lands (Phase B), layout becomes a free rewrite at
the UOp DAG level.

**4. Explicit fusion criteria, not just boundary marking.**
"Boundaries are nodes" is half the schedule story.  The OTHER half
is: WHICH ops fuse into one kernel?  Tinygrad's criteria:

  - At most ONE reduce per kernel (relaxed: chain reduces fuse if
    the second reduce's inputs include the first reduce's output —
    thvm's chain-reduce work covers this).
  - No elementwise-after-reduce-then-broadcast unless the broadcast
    is to the reduce's input shape (BN-grad pattern).
  - Producer numel ≤ consumer numel (avoid materialising larger
    intermediate than the consumer reads).
  - Movement-only chains absorb into the consumer (free).
  - User-realized roots split.

thvm has `inline-constants`, `inline-softmax-broadcast-reduce`,
`inline-large-expand-fanout`, etc. — these are FUSION rules but each
is an ad-hoc pattern, not the same uniform criteria tinygrad uses.
The bufferize-graph removal-rule layer (Phase 4 of bufferize.md) is
the right home for a CONSOLIDATED fusion-criteria predicate.

**5. Reduce-broadcast fusion at the codegen layer.**
The `reduce → broadcast back → elementwise` pattern (BN forward,
softmax, layernorm) wants ONE kernel that:

  - reads the input once,
  - computes the reduce into a shared-memory accumulator,
  - barriers,
  - broadcasts the reduce result inline,
  - emits the elementwise output.

That's a SINGLE kernel with TWO loop-nests (reduce, then
broadcast-elementwise) sharing the input.  thvm fuses this at the
schedule layer (`inline-softmax-broadcast-reduce` rule keeps them
in one KernelEntry) but the codegen-layer EMISSION emits it as two
sequential loop nests within one kernel — without explicit
shared-memory accumulator + barrier.  At small reduce-sizes the
register-tier substitution works, but for BS=512 BN-grad with
reduce_size=204800, shared-mem with threadgroup-cooperative reduce
is the only path to good throughput.

This is `TILE_REDUCE → TILE_ALLOC(scope=shared) → TILE_BARRIER →
TILE_LOAD(shared) → TILE_SCALAR_BODY` as Tile-IR.  Pattern
matcher in pass 1 detects the reduce-broadcast shape; pass 3
lowers to this Tile-IR template.

**6. Const inlining at codegen.**
Today thvm passes runtime constants as kernel inputs (a
`MTLBuffer` with one f32).  Each kernel binds the buffer, the
shader reads `params[k]` per thread.  Tinygrad inlines literals
into the MSL source: `mul(x, 2.0f)` becomes a literal `2.0f` in
the shader, no buffer bind.  Wins:

  - One fewer buffer per kernel → smaller `MTLArgumentTable`,
    fewer `setBuffer:` calls.
  - The shader compiler can constant-fold (`mul by 1.0` → identity).
  - Encoder-side: ~50ns saved per `setBuffer:` × N kernels × M
    dispatches per step.  At 100 kernels × 10 dispatches that's
    50µs/step recovered.

Implementation: pass-1 pattern that converts `LOAD(const_buf, [0])`
into `CONST(literal)` when the buffer's contents are known at
compile time.  Renderer emits the literal directly.  ~80 lines.

### Insights I previously glossed over

A first draft of this section had three bullets per system; the
current six + six is the audit.  The big things I missed initially:

- **ShapeTracker stack-of-views** — said "movement-as-INDEX" without
  explaining the mechanism (compose Views into one symbolic
  expression at consumer time).
- **Symbolic index simplifier** — necessary complement to (1); without
  it the absorbed INDEX expressions are huge ASTs that hurt codegen.
- **Memory hierarchy explicit in IR** — THE perf story for matmul
  / conv / attention.  Tilelang's biggest contribution.
- **Software pipelining** — required for matmul throughput on
  modern GPUs; tilelang exposes it as `T.pipelined`.
- **Async copy / MMA fragments** — `T.copy` + `T.gemm` produce
  cp.async / TMA / simdgroup_matrix at codegen time.
- **Pattern matcher uniformity** — every rewrite is the same engine,
  not ad-hoc switches.

These insights change what the Tile IR needs to carry: not just
loop nests + axis types, but explicit memory scopes, MMA fragments,
async copies, and pipelining annotations.  Phase D of the migration
order grows in scope accordingly.

### Migration order

The realize_classify retirement (commits `0fafd96` .. `e8d6815`,
`7ee461f`) showed the rename phase is straightforward.  The
IR migration is the actual work, broken into phases that each end
with `make test` green:

1. **Phase A** (in tree, partial): bufferize-IR data layer.
   `B_BUFFERIZE` / `B_INDEX` / `B_STORE` exist as records.
   `docs/plans/bufferize.md` Phases 0-7 cover this.
2. **Phase B** (next, ~2 weeks): movement-to-INDEX rewrites land in
   `uop_graph_simplify.c` as pass-1 rules.  Bufferize Phase 3 in the
   plan doc.  Consumers stop reading `KProgOp.pad_widths` /
   `axis_perm` / etc.; rangeify reads B_INDEX edges directly.
3. **Phase C** (~3 weeks): collapse `KProgOp` into the UOp DAG.
   `visit()` becomes a UOp-graph walker.  `rangeify_try_lower_elementwise`
   shrinks dramatically because it no longer bridges two
   semantically-different IRs — its movement-handling code is gone
   (now in pass 1) and its range-emission code stays as pass 3
   (UOp -> Tile IR), which now operates on one IR.
4. **Phase D** (~2 weeks): grow the Tile IR.  Add TILE_ALLOC,
   TILE_BARRIER, richer TILE_AXIS annotations (memory scope,
   vectorisation), TILE_GEMM / TILE_CONV2D specialised compute
   nodes.  Pass 4 (annotations) is born from the existing
   `axis.c`/`apply_opt.c`/`propose.c` trio.
5. **Phase E** (~2 weeks): collapse `KernelAxes` side channel.
   `TILE_AXIS.type` becomes the only axis state.  Autotune (BEAM)
   becomes a rewrite-search over Tile-IR transformations.
6. **Phase F** (~1 week): collapse the dispatch ladder.
   Specialised paths (alias / conv / gemm) become pattern matches
   that produce TILE_GEMM / TILE_CONV2D nodes; the Metal backend
   has one renderer that emits MSL per Tile-IR shape.
7. **Phase G**: delete the dead code paths.

Total: 8-10 weeks of focused work.  Result: 3 IRs (UOp DAG / Tile
IR / target source), 1 dispatch path, ~3.2k lines deleted, faster
compile, autotune lives at the right layer (Tile IR), and a clean
extension surface for the actual perf wins (memory hierarchy,
vectorisation, MMA).
