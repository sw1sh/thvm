// schedule/rangeify_unified.c
//
// 1-to-1 port of tinygrad/schedule/indexing.py:run_rangeify (lines
// 148-269) + pm_apply_rangeify (lines 101-110).
//
// The unified pass replaces the named-rule bufferize_classify +
// materialize.c visit() + kernel_lift trio with one imperative
// reverse-topological walk that:
//   - assigns per-axis RANGE expressions to every node in the DAG
//   - decides realize boundaries by looking at the consumer ranges
//     (consumer-divergence, ending-ranges, REDUCE/EXPAND injection)
//   - emits a rewrite map that pm_apply_rangeify uses to replace each
//     node's src with the appropriate BUFFERIZE/INDEX expression
//
// Gated behind THVM_UNIFIED_RANGEIFY (env var, default 1).
// THVM_UNIFIED_RANGEIFY=0 keeps the OLD path alive for bisects.
//
// THVM-SIDE DATA STRUCTURES vs TINYGRAD:
//
//   tinygrad                            | thvm
//   ------------------------------------+-------------------------------
//   tsink.toposort(gate_kernel_sink)    | BUFFERIZE_NODES (post bufferize_walk_rec)
//   consumer_map: Dict[UOp, List[UOp]]  | CMAP_LL + cmap_head
//   range_map: Dict[UOp, (in,out)]      | RU_RANGE_MAP[node_idx]
//   realize_map: Dict[UOp, None|list]   | RU_REALIZE_MAP[node_idx]
//   ending_ranges: dict[UOp, List]      | RU_ENDING_RANGES[node_idx]
//
// pm_apply_rangeify in tinygrad is one PatternMatcher rewrite; in thvm
// we run an equivalent imperative walk that updates a substitution
// table (RU_SUBST[node_idx]) and writes UOP_BUFFERIZE Terms onto the
// main heap at realize boundaries.
//
// Mirror source: tinygrad/schedule/indexing.py:148-269.

#define RU_MAX_NODES  BUFFERIZE_NODES_CAP
#define RU_MAX_AXES   MAX_DIM
#define RU_MAX_ENDING (RU_MAX_NODES * 4u)

// Per-node range map. Out_rngs[i] is the per-axis index expression a
// consumer threads into this node; in_rngs[i] is the equivalent at this
// node's source (after movement op swizzle / REDUCE axis injection).
// Both store Term (TAG_NUM 0 = "no range" / unfilled).
typedef struct {
  Term out_rngs[RU_MAX_AXES];
  Term in_rngs [RU_MAX_AXES];
  u8   out_ndim;
  u8   in_ndim;
  u8   has_ranges;        // 1 once out_rngs has been assigned
} RuRangeMap;

// Per-node realize state. Mirrors tinygrad's realize_map[x]:
//   realized_full == 0 && realized_partial == 0 : not realized
//   realized_full == 1                          : fully realized (all axes)
//   realized_partial == 1                       : partial-realize (mask)
typedef struct {
  u8  realized_full;
  u8  realized_partial;
  u8  axes_mask;          // bit i set iff axis i is in realize list (partial mode)
  u8  n_realized_axes;
} RuRealizeEntry;

// Per-node ending-ranges list. We store up to RU_ENDING_PER_NODE entries
// (axis_ids of the RANGE leaves that have been "closed" by an EXPAND
// in this node's consumer chain).
#define RU_ENDING_PER_NODE 16
typedef struct {
  u32 axis_ids[RU_ENDING_PER_NODE];
  u8  n;
} RuEndingRanges;

// Per-node reduce-range vector.  Mirrors tinygrad's
// `UOp(Ops.REDUCE, dtype, src=(value,)+tuple(new_ranges), arg=(kind, ()))`
// at tinygrad/schedule/indexing.py:90-96 (convert_reduce_to_reduce_with_ranges).
// Filled by run_rangeify_unified when it visits a UOP_REDUCE node; read
// by materialize.c to enumerate the reduce-axes the emit walker needs to
// iterate. n == 0 for non-REDUCE nodes or for REDUCE nodes with
// all-extent-1 reduce axes.
typedef struct {
  Term ranges[MAX_DIM];
  u8   n;
} RuReduceRanges;

static RuRangeMap      RU_RANGE_MAP    [RU_MAX_NODES];
static RuRealizeEntry  RU_REALIZE_MAP  [RU_MAX_NODES];
static RuEndingRanges  RU_ENDING_RANGES[RU_MAX_NODES];
static RuReduceRanges  RU_REDUCE_RANGES[RU_MAX_NODES];
static u32             RU_RANGE_IDX_COUNTER;  // monotonic axis_id source

// Reverse-topological order (consumers before producers).  Mirror
// source: tinygrad/uop/ops.py:consumer_map_from_toposort +
// `reversed(tsink.toposort(gate_kernel_sink))` at
// schedule/indexing.py:161.  bufferize_walk_rec's DFS PRE-order does
// NOT strictly satisfy this on DAGs with shared subexpressions: a
// shared producer reachable via the LAST-visited consumer path lands
// in BUFFERIZE_NODES AFTER that consumer.  This caused softmax /
// attention regressions because EXP's consumer-divergence wasn't
// observable until ALL its consumers' range_maps were filled (see
// nn.wlt softmax-* tests under THVM_RANGEIFY_DIRECT=1 with the MULTI
// seed dropped).  We compute a proper Kahn's-algorithm order on
// BUFFERIZE_NODES below.
static u32             RU_TOPO_ORDER[RU_MAX_NODES];
static u32             RU_TOPO_ORDER_LEN;
static u32             RU_TOPO_REMAINING[RU_MAX_NODES];  // unprocessed-consumer counter

// Stats / introspection accessors for the new test.
static u32 RU_LAST_NODES_WALKED;
static u32 RU_LAST_NEW_REALIZES;       // realize decisions made by THIS pass
static u32 RU_LAST_FULL_REALIZES;
static u32 RU_LAST_PARTIAL_REALIZES;

fn u32 rangeify_unified_last_nodes_walked   (void) { return RU_LAST_NODES_WALKED;   }
fn u32 rangeify_unified_last_new_realizes   (void) { return RU_LAST_NEW_REALIZES;   }
fn u32 rangeify_unified_last_full_realizes  (void) { return RU_LAST_FULL_REALIZES;  }
fn u32 rangeify_unified_last_partial_realizes(void) { return RU_LAST_PARTIAL_REALIZES;}
fn u32 rangeify_unified_range_idx_counter   (void) { return RU_RANGE_IDX_COUNTER;   }

// Was this node assigned ranges? (For test introspection.)
fn int rangeify_unified_has_ranges_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_RANGE_MAP[node_idx].has_ranges ? 1 : 0;
}
fn u32 rangeify_unified_out_ndim_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_RANGE_MAP[node_idx].out_ndim;
}
fn Term rangeify_unified_out_rng_at(u32 node_idx, u32 axis) {
  if (node_idx >= BUFFERIZE_NODES_LEN || axis >= RU_MAX_AXES) return 0;
  return RU_RANGE_MAP[node_idx].out_rngs[axis];
}
fn int rangeify_unified_is_realized(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_REALIZE_MAP[node_idx].realized_full
      || RU_REALIZE_MAP[node_idx].realized_partial;
}

// Number of reduce-ranges attached to this node.  Non-zero only for
// UOP_REDUCE nodes whose reduce axes have extent > 1.  Mirrors
// tinygrad's `len(x.src[1:])` after convert_reduce_to_reduce_with_ranges
// runs (indexing.py:94).
fn u32 rangeify_unified_reduce_n_ranges_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_REDUCE_RANGES[node_idx].n;
}

fn Term rangeify_unified_reduce_range_at(u32 node_idx, u32 i) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  if (i >= RU_REDUCE_RANGES[node_idx].n) return 0;
  return RU_REDUCE_RANGES[node_idx].ranges[i];
}

// === Mirror source: tinygrad/uop/ops.py:resolve  ===
// In tinygrad, `resolve(s != 1)` returns False iff s is structurally 1.
// We mirror with the static-extent check on a literal.
static int ru_extent_is_one(u32 extent) {
  return kvar_extent_is_var(extent) ? 0 : (kvar_extent_static(extent) == 1);
}

// === Mirror source: indexing.py:51-54 IndexingContext.new_range ===
// `if isinstance(s, UOp) and s.op is Ops.RANGE: return s`
//   -- already covered by hash-cons via uop_range
// `if resolve(s != 1)`: build a fresh RANGE; else CONST(0).
static Term ru_new_range(u32 extent, u32 axistype) {
  if (ru_extent_is_one(extent)) {
    // Mirror tinygrad: UOp.const(dtypes.weakint, 0)
    return uop_const(DT_INT32, 0);
  }
  u32 axis_id = RU_RANGE_IDX_COUNTER++;
  return uop_range(axis_id, axistype, extent);
}

// Returns 1 if `t` is a UOp_RANGE leaf.
static int ru_is_range(Term t) {
  return term_tag(t) == TAG_UOP && term_ext(t) == UOP_RANGE;
}

// Walk addr-expression Term `t` recursively, collecting axis_ids of
// UOP_RANGE leaves into `out_axes` (bounded by `cap`). Returns the
// number written.  Mirrors tinygrad's UOp.ranges accessor.
static u32 ru_collect_range_axes(Term t, u32 *out_axes, u32 cap, u32 depth) {
  if (depth > 32) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u8 op = term_ext(t);
  if (op == UOP_RANGE) {
    if (cap == 0) return 1;
    out_axes[0] = uop_range_axis_id(t);
    return 1;
  }
  u8 ar = uop_arity(op);
  u32 n = 0;
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar; i++) {
    Term c = heap_read(loc + i);
    n += ru_collect_range_axes(c,
                               (out_axes != NULL && n < cap) ? (out_axes + n) : NULL,
                               (n < cap) ? (cap - n) : 0,
                               depth + 1);
  }
  return n;
}

// Append ending-range axes from a child node up to this node, but only
// the unique ones (tinygrad uses `sum([...], [])` then implicitly dedups
// at the eligibility check; we dedup at append to keep the per-node
// table bounded).
static void ru_ending_append(RuEndingRanges *dst, const RuEndingRanges *src) {
  for (u8 i = 0; i < src->n; i++) {
    u32 axis_id = src->axis_ids[i];
    u8 dup = 0;
    for (u8 j = 0; j < dst->n; j++) if (dst->axis_ids[j] == axis_id) { dup = 1; break; }
    if (!dup && dst->n < RU_ENDING_PER_NODE) {
      dst->axis_ids[dst->n++] = axis_id;
    }
  }
}

// Build the shape of node at loc/op for use as the new ranges'
// extents. We use term_shape_in. Returns 1 on success.
static int ru_node_shape(u64 loc, u8 op, Shape *out) {
  (void)op;
  Term t = term_new(0, TAG_UOP, op, loc);
  return term_shape_in(t, 0, out);
}

// Build the shape of node's source (heap[loc + 0]). For movement ops
// in tinygrad, x.src[0].shape is the pre-swizzle shape. We always read
// slot 0 here -- caller guarantees the op carries a single source at
// slot 0 (true for all Movement ops + REDUCE).
static int ru_node_src_shape(u64 loc, Shape *out) {
  Term src = term_resolve(heap_read(loc));
  return term_shape_in(src, 0, out);
}

// === Mirror source: indexing.py:155-160 (consumer_map gather) ===
// We rely on the CMAP_LL/cmap_head table: per producer node, the
// linked-list of consumer locs is already populated by bufferize_walk_rec.
// This wrapper resolves consumer locs -> node indices and trims invalid
// entries (consumer never made it into BUFFERIZE_NODES).
#define RU_MAX_CONSUMERS 256
static u32 ru_consumers_for_node(u32 node_idx,
                                 u32 *out_consumer_idxs, u32 cap) {
  u64 buf[RU_MAX_CONSUMERS];
  u32 n_raw = bufferize_consumers_for_loc(BUFFERIZE_NODES[node_idx].loc,
                                          buf, RU_MAX_CONSUMERS);
  if (n_raw > RU_MAX_CONSUMERS) n_raw = RU_MAX_CONSUMERS;
  u32 n_out = 0;
  for (u32 i = 0; i < n_raw; i++) {
    u32 cidx = bufferize_info_find(buf[i]);
    if (cidx == 0xFFFFFFFFu) continue;
    if (out_consumer_idxs != NULL && n_out < cap) {
      out_consumer_idxs[n_out] = cidx;
    }
    n_out++;
  }
  return n_out;
}

// Mirror source: indexing.py:165-173 (skip-class predicates inside the
// main walk).
static int ru_is_skip_op(u8 op) {
  // tinygrad skips: DEVICE, UNIQUE, CALL, FUNCTION, LINEAR, AFTER,
  // MSTACK, MSELECT, and dtype==weakint.
  // thvm currently has none of DEVICE/UNIQUE/CALL/FUNCTION/LINEAR/MSTACK/MSELECT.
  // UOP_AFTER exists; KERNEL acts as the opaque-kernel boundary.
  return op == UOP_AFTER || op == UOP_KERNEL;
}

// Mirror source: indexing.py:181 -- gather consumer in_rngs.
//   `consumer_rngs = [rctx.range_map[c][0] for c in consumer_map[x] if c in rctx.range_map]`
// We return up to RU_MAX_AXES per consumer (since each consumer's
// in_rngs has at most this node's out shape rank).
typedef struct {
  Term rngs[RU_MAX_AXES];
  u8   ndim;
} RuConsumerRangs;

static u32 ru_gather_consumer_rngs(u32 node_idx, u8 my_ndim,
                                   RuConsumerRangs *out, u32 cap) {
  u32 consumer_idxs[RU_MAX_CONSUMERS];
  u32 n_c = ru_consumers_for_node(node_idx, consumer_idxs, RU_MAX_CONSUMERS);
  u32 n_out = 0;
  for (u32 i = 0; i < n_c; i++) {
    u32 ci = consumer_idxs[i];
    if (!RU_RANGE_MAP[ci].has_ranges) continue;
    // The consumer's in_rngs at the slot where this node is the source.
    // Tinygrad's per-consumer `range_map[c][0]` is rank-equal to the
    // PRODUCER's out shape (the consumer's `in` for this edge), so we
    // simply mirror in_rngs as-is. Caller capped at cap.
    if (n_out >= cap) break;
    RuConsumerRangs *r = &out[n_out++];
    r->ndim = my_ndim;
    for (u8 a = 0; a < my_ndim && a < RU_MAX_AXES; a++) {
      // If consumer's in_rngs rank doesn't match (heterogeneous reach;
      // mostly during partial port), fall back to CONST(0).
      r->rngs[a] = (a < RU_RANGE_MAP[ci].in_ndim)
                 ? RU_RANGE_MAP[ci].in_rngs[a]
                 : uop_const(DT_INT32, 0);
    }
  }
  return n_out;
}

// Mirror source: indexing.py:205 (`all_same(local_rngs)` per axis).
// Two Term values are "the same" iff structurally identical (the
// constructors hash-cons, so equality on the packed Term is enough).
static int ru_all_same_axis(RuConsumerRangs const *rs, u32 n, u8 axis) {
  if (n <= 1) return 1;
  Term first = rs[0].rngs[axis];
  for (u32 i = 1; i < n; i++) {
    if (rs[i].rngs[axis] != first) return 0;
  }
  return 1;
}

// Mirror source: indexing.py:246
//   `if x.op in GroupOp.Movement: rngs = apply_movement_op(...)`
// We call into the Phase-1c apply_movement_op_* family in indexing.c.
//
// Returns 1 if a swizzle was applied (in_rngs/in_ndim filled), 0 if
// the op isn't a movement op the swizzler handles.  RESHAPE is parked
// until pm_simplify_valid_apply is sharpened beyond its identity stub.
static int ru_apply_movement(u64 loc, u8 op,
                              Term const *out_rngs, u32 out_ndim,
                              Term *in_rngs, u32 *in_ndim) {
  if (op == UOP_RESHAPE) {
    // Tinygrad's RESHAPE goes through _apply_reshape which needs
    // pm_simplify_valid (identity stub today).  Fall through to identity
    // at the producer's source shape -- known thvm-side gap relative to
    // tinygrad; sharpen pm_simplify_valid_apply per failing case.
    *in_ndim = 0;
    return 0;
  }

  if (op == UOP_PERMUTE) {
    u32 perm[RU_MAX_AXES];
    for (u32 i = 0; i < out_ndim; i++) {
      perm[i] = (u32)term_val(heap_read(loc + 2 + i));
    }
    apply_movement_op_permute(out_ndim, perm, out_rngs, in_rngs);
    *in_ndim = out_ndim;
    return 1;
  }

  Shape src_shape;
  if (!ru_node_src_shape(loc, &src_shape)) {
    *in_ndim = 0;
    return 0;
  }

  if (op == UOP_SHRINK) {
    u32 be[2 * RU_MAX_AXES];
    for (u32 i = 0; i < out_ndim; i++) {
      be[2 * i]     = (u32)term_val(heap_read(loc + 2 + 2 * i));
      be[2 * i + 1] = (u32)term_val(heap_read(loc + 2 + 2 * i + 1));
    }
    apply_movement_op_shrink(out_ndim, be, out_rngs, in_rngs);
    *in_ndim = out_ndim;
    return 1;
  }

  if (op == UOP_PAD) {
    u32 be[2 * RU_MAX_AXES];
    u32 ins[RU_MAX_AXES];
    for (u32 i = 0; i < out_ndim; i++) {
      be[2 * i]     = (u32)term_val(heap_read(loc + 2 + 2 * i));
      be[2 * i + 1] = (u32)term_val(heap_read(loc + 2 + 2 * i + 1));
      ins[i]        = (i < src_shape.ndim) ? src_shape.dims[i] : 1;
    }
    apply_movement_op_pad(out_ndim, ins, be, out_rngs, in_rngs);
    *in_ndim = out_ndim;
    return 1;
  }

  if (op == UOP_EXPAND) {
    u32 ins[RU_MAX_AXES], outs[RU_MAX_AXES];
    for (u32 i = 0; i < out_ndim; i++) {
      ins[i]  = (i < src_shape.ndim) ? src_shape.dims[i] : 1;
      outs[i] = (u32)term_val(heap_read(loc + 2 + i));
    }
    apply_movement_op_expand(out_ndim, ins, outs, out_rngs, in_rngs);
    *in_ndim = out_ndim;
    return 1;
  }

  if (op == UOP_FLIP) {
    u32 fb = (u32)term_val(heap_read(loc + 1));
    u32 ins[RU_MAX_AXES], mask[RU_MAX_AXES];
    for (u32 i = 0; i < out_ndim; i++) {
      ins[i]  = (i < src_shape.ndim) ? src_shape.dims[i] : 1;
      mask[i] = (fb >> i) & 1u;
    }
    apply_movement_op_flip(out_ndim, ins, mask, out_rngs, in_rngs);
    *in_ndim = out_ndim;
    return 1;
  }

  *in_ndim = 0;
  return 0;
}

// Mirror source: tinygrad/uop/ops.py:consumer_map_from_toposort.
// Builds RU_TOPO_ORDER[] such that for every (consumer, producer)
// edge, consumer's index in the array is strictly less than
// producer's.  Equivalent to tinygrad's
// `reversed(tsink.toposort(gate_kernel_sink))` at
// schedule/indexing.py:161.
//
// Implementation: Kahn's algorithm over BUFFERIZE_NODES seen as a
// DAG with one edge per (node -> source) pair.  In-degree = number of
// CONSUMERS of the node within BUFFERIZE_NODES (= consumer_count
// minus consumers outside the node table; in practice that's 0 since
// bufferize_walk_rec adds every reachable UOp).  We seed the worklist
// with consumer_count == 0 nodes (the SINK -- typically a single root,
// possibly multiple roots if bufferize_classify was called with a
// nested SINK).  Each dequeue decrements the in-degree of all UOP
// producers reachable via heap[loc + arity] slots; producers whose
// counter hits 0 are enqueued.
//
// Falls back to forward BUFFERIZE_NODES order if Kahn's leaves nodes
// unvisited (cycle / inconsistent consumer_count) -- this matches the
// pre-fix behaviour and keeps tests green even on pathological
// graphs.
static void ru_compute_topo_order(void) {
  RU_TOPO_ORDER_LEN = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    RU_TOPO_REMAINING[i] = BUFFERIZE_NODES[i].consumer_count;
  }
  // Seed worklist with nodes that have no consumers in BUFFERIZE_NODES
  // (the SINK / root).  Push them onto the order array directly --
  // RU_TOPO_ORDER doubles as worklist; new entries appended at the
  // tail are processed in FIFO order.
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (RU_TOPO_REMAINING[i] == 0) {
      RU_TOPO_ORDER[RU_TOPO_ORDER_LEN++] = i;
    }
  }
  u32 head = 0;
  while (head < RU_TOPO_ORDER_LEN) {
    u32 idx  = RU_TOPO_ORDER[head++];
    UOpInfo const *info = &BUFFERIZE_NODES[idx];
    u8 ar = uop_arity(info->op);
    u64 seen[MAX_UOP_SRC] = {0};
    u8  n_seen = 0;
    for (u8 c = 0; c < ar; c++) {
      Term child = term_resolve(heap_read(info->loc + c));
      if (term_tag(child) != TAG_UOP) continue;
      if (term_ext(child) == UOP_KERNEL) continue;
      u64 cloc = term_val(child);
      u8 dup = 0;
      for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
      if (dup) continue;
      seen[n_seen++] = cloc;
      u32 cidx = bufferize_info_find(cloc);
      if (cidx == 0xFFFFFFFFu) continue;
      if (RU_TOPO_REMAINING[cidx] == 0) continue;     // already enqueued
      RU_TOPO_REMAINING[cidx]--;
      if (RU_TOPO_REMAINING[cidx] == 0) {
        RU_TOPO_ORDER[RU_TOPO_ORDER_LEN++] = cidx;
      }
    }
  }
  // Fallback: append any nodes that didn't reach in-degree 0 (broken
  // consumer_count / cycle).  Preserves the previous "iterate every
  // node" invariant even if Kahn's was incomplete.
  if (RU_TOPO_ORDER_LEN < BUFFERIZE_NODES_LEN) {
    u8 *enqueued = (u8 *)calloc(BUFFERIZE_NODES_LEN, 1);
    if (enqueued != NULL) {
      for (u32 k = 0; k < RU_TOPO_ORDER_LEN; k++) {
        enqueued[RU_TOPO_ORDER[k]] = 1;
      }
      for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
        if (!enqueued[i]) {
          RU_TOPO_ORDER[RU_TOPO_ORDER_LEN++] = i;
        }
      }
      free(enqueued);
    }
  }
}

// Accessor for testing.
fn u32 rangeify_unified_topo_order_len(void) { return RU_TOPO_ORDER_LEN; }
fn u32 rangeify_unified_topo_order_at(u32 i) {
  if (i >= RU_TOPO_ORDER_LEN) return 0xFFFFFFFFu;
  return RU_TOPO_ORDER[i];
}

// === Mirror source: indexing.py:148-269 run_rangeify ===
//
// Reverse-topo walk over BUFFERIZE_NODES via RU_TOPO_ORDER (Kahn's,
// consumers strictly before producers).  Mirrors tinygrad's
// `reversed(tsink.toposort(gate_kernel_sink))`.
//
// Pre-condition: caller must have run bufferize_classify(root) first;
// this fills BUFFERIZE_NODES + CMAP_LL + the existing realized-bit.
//
// Side effects: populates RU_RANGE_MAP + RU_REALIZE_MAP + RU_ENDING_RANGES.
// The realize decision is kept in RU_REALIZE_MAP separate from
// BUFFERIZE_NODES.realized.

fn void run_rangeify_unified(Term root) {
  // Clear per-node state.
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    RU_RANGE_MAP[i].out_ndim   = 0;
    RU_RANGE_MAP[i].in_ndim    = 0;
    RU_RANGE_MAP[i].has_ranges = 0;
    for (u32 a = 0; a < RU_MAX_AXES; a++) {
      RU_RANGE_MAP[i].out_rngs[a] = 0;
      RU_RANGE_MAP[i].in_rngs [a] = 0;
    }
    RU_REALIZE_MAP[i].realized_full    = 0;
    RU_REALIZE_MAP[i].realized_partial = 0;
    RU_REALIZE_MAP[i].axes_mask        = 0;
    RU_REALIZE_MAP[i].n_realized_axes  = 0;
    RU_ENDING_RANGES[i].n = 0;
    RU_REDUCE_RANGES[i].n = 0;
    for (u32 a = 0; a < MAX_DIM; a++) RU_REDUCE_RANGES[i].ranges[a] = 0;
  }
  RU_RANGE_IDX_COUNTER    = 0;
  RU_LAST_NODES_WALKED    = 0;
  RU_LAST_NEW_REALIZES    = 0;
  RU_LAST_FULL_REALIZES   = 0;
  RU_LAST_PARTIAL_REALIZES= 0;

  if (term_tag(root) != TAG_UOP) return;
  if (term_ext(root) == UOP_KERNEL) return;

  u32 root_node = bufferize_info_find(term_val(root));
  if (root_node == 0xFFFFFFFFu) return;

  // *** Mirror indexing.py:152-153 (generate realize map) ***
  // Seed RU_REALIZE_MAP from BUFFERIZE_NODES.realized -- the
  // post-bufferize_classify-rules realize set.  Tinygrad's
  // pm_generate_realize_map seeds only COPY/CONTIGUOUS/STORE
  // (indexing.py:28-35); thvm doesn't have those opcodes, so we
  // inherit the OLD path's realize bit (= ROOT seed + MULTI seed +
  // REDUCE/MATMUL seed + matmul-input-protect + the 11 named
  // removal rules' result).
  //
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (BUFFERIZE_NODES[i].realized) {
      RU_REALIZE_MAP[i].realized_full = 1;
      RU_REALIZE_MAP[i].n_realized_axes = MAX_DIM;
    }
  }

  // *** Mirror indexing.py:161 (reverse-topo walk) ***
  // bufferize_walk_rec's PRE-order does NOT strictly satisfy
  // "consumers-before-producers" on DAGs with shared subexpressions
  // (e.g. softmax's EXP node is reached via MUL first, but its
  // REDUCE_SUM consumer is appended later via the RECIP path).  We
  // compute a true reverse-topological order via Kahn's algorithm and
  // iterate it here.  Mirror: tinygrad/uop/ops.py
  // :consumer_map_from_toposort, used at
  // schedule/indexing.py:157 (`consumer_map = ...`).
  ru_compute_topo_order();
  for (u32 oi = 0; oi < RU_TOPO_ORDER_LEN; oi++) {
    u32 node_idx = RU_TOPO_ORDER[oi];
    UOpInfo *info = &BUFFERIZE_NODES[node_idx];
    if (ru_is_skip_op(info->op)) continue;
    RU_LAST_NODES_WALKED++;

    Shape shape;
    if (!ru_node_shape(info->loc, info->op, &shape)) {
      // Shape inference failed -- mirror tinygrad's `if x.dtype.scalar()
      // == dtypes.weakint: continue` (we have no weakint, so falling
      // through w/o shape == no ranges).
      continue;
    }
    u8 my_ndim = (u8)shape.ndim;
    if (my_ndim > RU_MAX_AXES) my_ndim = RU_MAX_AXES;

    // ending_ranges[x] = sum([ending_ranges.get(u, []) for u in consumer_map[x]], [])
    {
      u32 cidxs[RU_MAX_CONSUMERS];
      u32 nc = ru_consumers_for_node(node_idx, cidxs, RU_MAX_CONSUMERS);
      for (u32 c = 0; c < nc; c++) {
        ru_ending_append(&RU_ENDING_RANGES[node_idx],
                         &RU_ENDING_RANGES[cidxs[c]]);
      }
    }

    // *** Mirror indexing.py:181 gather consumer rngs ***
    RuConsumerRangs consumer_rngs[RU_MAX_CONSUMERS];
    u32 n_crngs = ru_gather_consumer_rngs(node_idx, my_ndim,
                                          consumer_rngs, RU_MAX_CONSUMERS);

    Term out_rngs[RU_MAX_AXES] = {0};

    if (RU_REALIZE_MAP[node_idx].realized_full
        || RU_REALIZE_MAP[node_idx].realized_partial) {
      // *** Mirror indexing.py:182-189: realize_map entry pre-seeded ***
      // "create new ranges (at the output); ending_ranges[] = []; mark
      //  all axes as realized."
      for (u8 a = 0; a < my_ndim; a++) {
        out_rngs[a] = ru_new_range(shape.dims[a], KAX_LOOP);
      }
      RU_REALIZE_MAP[node_idx].realized_full = 1;
      RU_REALIZE_MAP[node_idx].realized_partial = 0;
      RU_REALIZE_MAP[node_idx].axes_mask = (my_ndim < 8) ? ((1u << my_ndim) - 1u) : 0xFFu;
      RU_REALIZE_MAP[node_idx].n_realized_axes = my_ndim;
      RU_ENDING_RANGES[node_idx].n = 0;
      RU_LAST_FULL_REALIZES++;
    } else if (n_crngs == 0) {
      // *** Mirror indexing.py:190-192 ***
      // "if no consumers have ranges and this isn't realized, this
      //  doesn't have ranges either."
      continue;
    } else if (n_crngs == 1) {
      // *** Mirror indexing.py:193-195 ***
      // "if this has one consumer, it inherits the ranges from it."
      for (u8 a = 0; a < my_ndim; a++) {
        out_rngs[a] = consumer_rngs[0].rngs[a];
      }
    } else {
      // *** Mirror indexing.py:196-220 ***
      // Two-or-more consumers: per-axis decide whether to share or
      // realize anew. tinygrad's `all_all_same` short-circuit:
      // if every axis's local_rngs are structurally the same, just
      // share; else realize (mark partial-realize on the diverging axes).
      u8 partial_mask = 0;
      u8 n_realized = 0;
      // First pass: detect all_all_same.
      int all_all_same = 1;
      for (u8 a = 0; a < my_ndim; a++) {
        if (!ru_all_same_axis(consumer_rngs, n_crngs, a)) {
          all_all_same = 0; break;
        }
      }
      if (all_all_same) {
        // Mirror "OR of valids" path (indexing.py:211-213). Without a
        // full symbolic-bool merger we collapse to consumer 0's range
        // -- safe because all_all_same means they're equal anyway.
        for (u8 a = 0; a < my_ndim; a++) {
          out_rngs[a] = consumer_rngs[0].rngs[a];
        }
      } else {
        // Per-axis decide.
        for (u8 a = 0; a < my_ndim; a++) {
          if (ru_all_same_axis(consumer_rngs, n_crngs, a)) {
            out_rngs[a] = consumer_rngs[0].rngs[a];
          } else {
            out_rngs[a] = ru_new_range(shape.dims[a], KAX_LOOP);
            partial_mask |= (u8)(1u << a);
            n_realized++;
          }
        }
        if (n_realized > 0) {
          RU_REALIZE_MAP[node_idx].realized_partial = 1;
          RU_REALIZE_MAP[node_idx].axes_mask        = partial_mask;
          RU_REALIZE_MAP[node_idx].n_realized_axes  = n_realized;
          RU_LAST_PARTIAL_REALIZES++;
          RU_LAST_NEW_REALIZES++;
        }
      }
    }

    // *** Mirror indexing.py:223-232 ***
    // "if this element is a reduce and there's ended ranges, we might
    //  have to end some other ranges (...)"
    int is_elementwise = uop_is_unary_elementwise(info->op)
                      || uop_is_binary_elementwise(info->op);
    if (RU_ENDING_RANGES[node_idx].n
        && (is_elementwise || info->op == UOP_REDUCE)) {
      // Mark every non-realized axis as realized (without PCONTIG > 1
      // we always realize -- the conservative branch in tinygrad).
      u8 mask = RU_REALIZE_MAP[node_idx].axes_mask;
      u8 added = 0;
      for (u8 a = 0; a < my_ndim; a++) {
        if (mask & (u8)(1u << a)) continue;
        mask |= (u8)(1u << a);
        out_rngs[a] = ru_new_range(shape.dims[a], KAX_LOOP);
        added++;
      }
      if (added) {
        RU_REALIZE_MAP[node_idx].realized_partial = 1;
        RU_REALIZE_MAP[node_idx].axes_mask = mask;
        RU_REALIZE_MAP[node_idx].n_realized_axes += added;
        RU_LAST_NEW_REALIZES++;
      }
      RU_ENDING_RANGES[node_idx].n = 0;
    }

    // *** Mirror indexing.py:243-254 ***
    // "rngs is the input ranges" -- apply movement-op swizzle / REDUCE
    // axis injection.
    Term in_rngs[RU_MAX_AXES] = {0};
    u32  in_ndim = my_ndim;
    int  is_movement = (info->op == UOP_RESHAPE || info->op == UOP_PERMUTE
                     || info->op == UOP_EXPAND  || info->op == UOP_PAD
                     || info->op == UOP_SHRINK  || info->op == UOP_FLIP);
    if (is_movement) {
      u32 swizzled_ndim = 0;
      if (ru_apply_movement(info->loc, info->op, out_rngs, my_ndim,
                            in_rngs, &swizzled_ndim)) {
        in_ndim = swizzled_ndim;
      } else {
        // Identity fallback when the swizzler can't infer the shape.
        for (u8 a = 0; a < my_ndim; a++) in_rngs[a] = out_rngs[a];
      }

      // Mirror indexing.py:249-250 (EXPAND seeds ending_ranges).
      if (info->op == UOP_EXPAND) {
        Shape src_shape;
        if (ru_node_src_shape(info->loc, &src_shape)) {
          for (u8 a = 0; a < my_ndim; a++) {
            u32 in_dim  = (a < src_shape.ndim) ? src_shape.dims[a] : 1;
            u32 out_dim = (u32)term_val(heap_read(info->loc + 2 + a));
            if (in_dim != out_dim) {
              // Range "ended" by this expand: collect the axis_id of
              // every RANGE in out_rngs[a] but not in in_rngs[a].
              u32 axes_out[RU_MAX_AXES];
              u32 n_axes_out = ru_collect_range_axes(out_rngs[a],
                                                     axes_out, RU_MAX_AXES, 0);
              for (u32 j = 0; j < n_axes_out
                            && RU_ENDING_RANGES[node_idx].n < RU_ENDING_PER_NODE; j++) {
                u32 ax = axes_out[j];
                u8 dup = 0;
                for (u8 k = 0; k < RU_ENDING_RANGES[node_idx].n; k++) {
                  if (RU_ENDING_RANGES[node_idx].axis_ids[k] == ax) {
                    dup = 1; break;
                  }
                }
                if (!dup) {
                  RU_ENDING_RANGES[node_idx].axis_ids[RU_ENDING_RANGES[node_idx].n++] = ax;
                }
              }
            }
          }
        }
      }
    } else if (info->op == UOP_REDUCE) {
      // Mirror indexing.py:253-254
      //   `tuple(rctx.new_range(s, axistype=AxisType.REDUCE) if i in arg[1] else r ...`
      // thvm UOP_REDUCE stores a single axis at heap[loc + 2].
      //
      // We also record the fresh REDUCE range Term in
      // RU_REDUCE_RANGES[node_idx] so materialize.c can enumerate the
      // reduce-axes via accessor without re-deriving them from the
      // heap. This is the thvm-side equivalent of
      // tinygrad's `convert_reduce_to_reduce_with_ranges` storing the
      // ranges in `src=(value,) + tuple(new_ranges)` at indexing.py:94.
      Shape src_shape;
      if (!ru_node_src_shape(info->loc, &src_shape)) {
        for (u8 a = 0; a < my_ndim; a++) in_rngs[a] = out_rngs[a];
      } else {
        u32 raxis = (u32)term_val(heap_read(info->loc + 2));
        // tinygrad keeps the input rank == producer.shape rank; with
        // a single axis collapse, src_shape == out_shape with raxis
        // expanded. Re-thread out_rngs and inject a fresh REDUCE range
        // at raxis.
        u8 dst_ndim = (u8)src_shape.ndim;
        if (dst_ndim > RU_MAX_AXES) dst_ndim = RU_MAX_AXES;
        u8 src_cursor = 0;
        RU_REDUCE_RANGES[node_idx].n = 0;
        for (u8 a = 0; a < dst_ndim; a++) {
          if (a == raxis) {
            Term rng = ru_new_range(src_shape.dims[a], KAX_REDUCE);
            in_rngs[a] = rng;
            // Record only true UOP_RANGE leaves (extent-1 reduce axes
            // collapse to UOP_CONST(0) per ru_new_range; tinygrad's
            // convert_reduce_to_reduce_with_ranges filters via
            // `len(x.arg[1])` and the `i in x.arg[1]` check at line 93).
            if (term_tag(rng) == TAG_UOP && term_ext(rng) == UOP_RANGE) {
              u8 k = RU_REDUCE_RANGES[node_idx].n;
              if (k < MAX_DIM) {
                RU_REDUCE_RANGES[node_idx].ranges[k] = rng;
                RU_REDUCE_RANGES[node_idx].n = k + 1;
              }
            }
          } else {
            in_rngs[a] = (src_cursor < my_ndim)
                       ? out_rngs[src_cursor]
                       : uop_const(DT_INT32, 0);
            src_cursor++;
          }
        }
        in_ndim = dst_ndim;
      }
    } else {
      // Elementwise / CAST / BITCAST / etc: in_rngs == out_rngs.
      for (u8 a = 0; a < my_ndim; a++) in_rngs[a] = out_rngs[a];
    }

    // Commit per-node range map.
    RU_RANGE_MAP[node_idx].out_ndim   = my_ndim;
    RU_RANGE_MAP[node_idx].in_ndim    = (u8)in_ndim;
    RU_RANGE_MAP[node_idx].has_ranges = 1;
    for (u8 a = 0; a < my_ndim; a++) RU_RANGE_MAP[node_idx].out_rngs[a] = out_rngs[a];
    for (u8 a = 0; a < in_ndim; a++) RU_RANGE_MAP[node_idx].in_rngs [a] = in_rngs [a];
  }

  // *** Mirror indexing.py:268: graph_rewrite(pm_apply_rangeify) ***
  pm_apply_rangeify(root);
}

// === pm_apply_rangeify (mirror indexing.py:101-110) ===
//
// In tinygrad this is a PatternMatcher rewrite that:
//   1. REDUCE(op, axis_tuple) -> REDUCE(op) with explicit RANGE args
//   2. PAD -> WHERE (because apply_movement_op_pad already produced the
//      WHERE-INVALID guard; this rule wraps the PAD source value with it)
//   3. Generic node -> rewrite each src either to INDEX expr or to a
//      BUFFERIZE+INDEX when the src is in realize_map
//   4. Movement ops post-rangeify: drop them (rngs already swizzled in).
//
// thvm-side implementation: walk every node and compute the canonical
// substitute Term.  RU_SUBST holds the INDEX expression a consumer
// threads in; at realize boundaries we additionally emit a UOP_BUFFERIZE
// Term on the MAIN HEAP via uop_bufferize_new and stash it in
// RU_BUFFERIZE_TERM for materialize.c to walk.

#define RU_SUBST_CAP RU_MAX_NODES
static Term RU_SUBST[RU_SUBST_CAP];
// Main-heap UOP_BUFFERIZE Term per realized node.  Zero means "no
// boundary emitted here".  Mirrors tinygrad's
// `UOp(Ops.BUFFERIZE, ..., src=(value,)+closed_ranges, arg=opts)`
// landing in the tsink graph at the realize boundary.
static Term RU_BUFFERIZE_TERM[RU_SUBST_CAP];
// Main-heap UOP_STORE Term per realized node, structurally equivalent
// to what `kernel_lift_to_uop` emits as `cached_lift.store_root`.
// Built from the rewritten subtree + a fresh UOP_BUFFER sized by the
// boundary's closed-range extents.  Zero if no boundary or if dtype
// inference declined.  Consumed under THVM_LIFT_FROM_BUFFERIZE=1 by
// materialize.c to bypass the legacy ScalarUop-arena walker.
static Term RU_STORE_ROOT[RU_SUBST_CAP];

fn Term rangeify_unified_subst_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_SUBST[node_idx];
}

// Accessor for the UOP_BUFFERIZE Term emitted at this node's realize
// boundary.  0 if the node is not a boundary.
fn Term rangeify_unified_bufferize_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_BUFFERIZE_TERM[node_idx];
}

// Accessor for the UOP_STORE Term emitted at this node's realize
// boundary, structurally equivalent to `cached_lift.store_root`.
fn Term rangeify_unified_store_root_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_STORE_ROOT[node_idx];
}

// Resync RU_REALIZE_MAP[i].realized_full from BUFFERIZE_NODES[i].realized.
// bufferize_classify's prune rules (inline-softmax-broadcast-reduce et al.)
// mutate BUFFERIZE_NODES.realized after run_rangeify_unified has populated
// RU_REALIZE_MAP. Without this resync a re-invocation of pm_apply_rangeify
// would emit BUFFERIZE Terms for stale-realized nodes whose realized bit
// has since been cleared. Partial-realize state is left untouched.
fn void rangeify_unified_resync_realize_from_nodes(void) {
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (BUFFERIZE_NODES[i].realized) {
      RU_REALIZE_MAP[i].realized_full = 1;
    } else {
      RU_REALIZE_MAP[i].realized_full = 0;
    }
  }
}

// Build a row-major linear addr Term from an array of UOP_RANGE leaves.
// Strides come from each range's extent (heap field 2); the result is
//   r[0]*prod(d[1..]) + r[1]*prod(d[2..]) + ... + r[n-1].
// Used both for the OUTPUT addr (rm->out_rngs) when threading a producer
// into its consumer, and for the BODY/INPUT addr (rm->in_rngs) when
// wrapping TAG_TEN leaves under a REDUCE (whose body iterates over
// out_rngs + the injected reduce range).
static Term ru_build_addr_from_ranges(Term const *rngs, u8 ndim) {
  if (ndim == 0) return uop_const(DT_INT32, 0);
  u32 dims[RU_MAX_AXES] = {0};
  for (u8 a = 0; a < ndim; a++) {
    Term r = rngs[a];
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) {
      // Fallback: flat IADD if any range isn't a recognisable extent.
      Term acc = rngs[0];
      for (u8 b = 1; b < ndim; b++) {
        acc = uop_int_binary(UOP_IADD, acc, rngs[b]);
      }
      return acc;
    }
    dims[a] = (u32)term_val(heap_read(term_val(r) + 2));
  }
  u32 strides[RU_MAX_AXES] = {0};
  strides[ndim - 1] = 1;
  for (i8 a = (i8)ndim - 2; a >= 0; a--) {
    strides[a] = strides[a + 1] * dims[a + 1];
  }
  Term acc = 0;
  for (u8 a = 0; a < ndim; a++) {
    Term term = rngs[a];
    if (strides[a] != 1) {
      term = uop_int_binary(UOP_IMUL, term,
                            uop_const(DT_INT32, strides[a]));
    }
    acc = (acc == 0) ? term : uop_int_binary(UOP_IADD, acc, term);
  }
  return acc != 0 ? acc : uop_const(DT_INT32, 0);
}

// Builds the "INDEX expression" Term that a consumer references when it
// reads this node. Mirrors tinygrad's
//   `new_src = new_src.index(*ctx.range_map[x][0])`
// We model `.index(*r)` as a chain of UOP_INDEX_E.addr over an IADD-tree
// of (range_i * stride_i) -- tinygrad's INDEX is multi-arg but the
// downstream stride-collapse simplification yields the same scalar.
static Term ru_build_index_addr(RuRangeMap const *rm) {
  return ru_build_addr_from_ranges(rm->out_rngs, rm->out_ndim);
}

// Body/input addr: includes the reduce range (and any movement-op
// swizzle). For non-REDUCE nodes this is identical to the output addr
// because in_rngs == out_rngs; for REDUCE the reduce range was injected
// at raxis in run_rangeify_unified's UOP_REDUCE branch.
static Term ru_build_input_addr(RuRangeMap const *rm) {
  return ru_build_addr_from_ranges(rm->in_rngs, rm->in_ndim);
}

// Count of UOP_BUFFERIZE nodes emitted on the main heap by the most
// recent pm_apply_rangeify run.  Stats / introspection.
static u32 RU_LAST_BUFFERIZES_EMITTED;
fn u32 rangeify_unified_last_bufferizes_emitted(void) {
  return RU_LAST_BUFFERIZES_EMITTED;
}

// Collect "closed ranges" for a realize boundary at node `i`. Mirror:
// tinygrad/schedule/indexing.py:66 (`closed_ranges = tuple([r for i,r in
// enumerate(ctx.range_map[s][1]) if i in realized_ranges])`) -- i.e. the
// out_rngs at the realized axes. Returns the number of ranges written
// (>= 0, <= MAX_DIM).
//
// For realized_full, every axis is closed.  For realized_partial only
// the axes in axes_mask are closed.  RANGEs that hash-cons to
// UOP_CONST(0) (extent==1 axes) are dropped, mirroring tinygrad's
// `if r.op is Ops.RANGE` filter at tinygrad/schedule/indexing.py:69.
static u32 ru_collect_closed_ranges(u32 i, Term *out_ranges, u32 cap) {
  RuRangeMap const *rm = &RU_RANGE_MAP[i];
  RuRealizeEntry const *rl = &RU_REALIZE_MAP[i];
  u32 n = 0;
  for (u8 a = 0; a < rm->out_ndim && n < cap; a++) {
    int axis_realized = rl->realized_full
                     || ((rl->axes_mask >> a) & 1u);
    if (!axis_realized) continue;
    Term r = rm->out_rngs[a];
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
    out_ranges[n++] = r;
  }
  return n;
}

// Rebuild the producer Term at (loc, op) by REWRITING each child slot:
//   - child has RU_SUBST[child_idx] != 0
//       -> if RU_SUBST is a UOP_BUFFERIZE, wrap with uop_index_e(BUFFERIZE, in_addr)
//          so the load happens at THIS consumer's iter (mirrors tinygrad
//          indexing.py:78 -- BUFFERIZE.index(*consumer_ranges)).
//       -> otherwise (non-realized producer's rewritten value) splice in
//          as-is; its TAG_TEN leaves were already wrapped at its own iter.
//   - child is a leaf tensor (TAG_TEN)    ->  wrap with uop_index_e(child, in_addr)
//   - everything else (atoms, passthroughs)  ->  keep
// in_addr is the addr at which TAG_TEN/BUFFERIZE INPUTS are loaded at
// THIS op's iteration -- for elementwise it matches the op's own
// out-addr, for REDUCE it must include the reduce range (built from
// rm->in_rngs).
// Mirror: tinygrad/schedule/indexing.py:create_bufferize_and_index_based_on_ranges
// (lines 56-81), `new_srcs` accumulation loop + final `x.replace(src=tns)`.
static Term ru_rewrite_subtree(Term self, u64 loc, u8 op, Term in_addr) {
  u8 ar = uop_arity(op);
  if (ar == 0) return self;
  Term srcs[MAX_UOP_SRC] = {0};
  int changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_child = heap_read(loc + i);
    Term new_child = old_child;
    Term resolved  = term_resolve(old_child);
    u8 ctag = term_tag(resolved);
    if (ctag == TAG_UOP) {
      u32 cidx = bufferize_info_find(term_val(resolved));
      if (cidx != 0xFFFFFFFFu && RU_SUBST[cidx] != 0) {
        Term sub = RU_SUBST[cidx];
        // BUFFERIZE substitute (realized producer): re-index at OUR iter.
        if (term_tag(sub) == TAG_UOP && term_ext(sub) == UOP_BUFFERIZE) {
          new_child = uop_index_e(sub, in_addr);
        } else {
          new_child = sub;
        }
      }
    } else if (ctag == TAG_TEN) {
      new_child = uop_index_e(resolved, in_addr);
    }
    srcs[i] = new_child;
    if (new_child != old_child) changed = 1;
  }
  if (!changed) return self;
  return uop_graph_rebuild_with_srcs(self, srcs);
}

fn void pm_apply_rangeify(Term root) {
  (void)root;
  // Clear substitute + bufferize tables.
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    RU_SUBST[i]           = 0;
    RU_BUFFERIZE_TERM[i]  = 0;
    RU_STORE_ROOT[i]      = 0;
  }
  RU_LAST_BUFFERIZES_EMITTED = 0;

  // Walk nodes bottom-up (children-first) so each consumer sees its
  // producers' substitutes.  RU_TOPO_ORDER is consumer-first
  // (computed by ru_compute_topo_order at run_rangeify_unified entry);
  // reversing it gives producer-first / children-before-parents.
  for (i64 oi = (i64)RU_TOPO_ORDER_LEN - 1; oi >= 0; oi--) {
    u32 i = RU_TOPO_ORDER[oi];
    UOpInfo const *info = &BUFFERIZE_NODES[i];
    if (ru_is_skip_op(info->op)) continue;
    RuRangeMap const *rm = &RU_RANGE_MAP[i];
    if (!rm->has_ranges) {
      // No ranges = this node has no consumers w/ ranges and isn't
      // realized; the OLD path inlines it into its consumer (PASS-THRU
      // semantics). We leave RU_SUBST[i] = 0 to signal "passthrough".
      continue;
    }

    int realized = RU_REALIZE_MAP[i].realized_full
                || RU_REALIZE_MAP[i].realized_partial;

    Term self = term_new(0, TAG_UOP, info->op, info->loc);

    if (realized) {
      // Mirror indexing.py:75-77:
      //   new_src = UOp(Ops.BUFFERIZE, s.dtype, src=(new_src,)+closed_ranges,
      //                 arg=opts)
      // The main-heap node wraps the REWRITTEN subtree so
      // uop_bufferize_value returns the lowered form with INDEX_E around
      // realized-producer references.  RU_SUBST keeps using `self` (the
      // ORIGINAL op Term) because downstream materialize.c consumers
      // resolve through it via term_resolve / heap_read identity.
      // my_addr is the OUTPUT addr (uop_store(buf, my_addr, value)).
      // in_addr threads INPUT (TAG_TEN / cross-realize BUFFERIZE) loads
      // at this op's INPUT iter: REDUCE injects the reduce range, and
      // movement ops (EXPAND, PAD, SHRINK, PERMUTE, FLIP) swizzle axes
      // so a stride-0 broadcast axis becomes CONST(0). Built from
      // rm->in_rngs uniformly; the elementwise case is the identity
      // in_rngs == out_rngs and falls through to the same shape.
      Term my_addr   = ru_build_index_addr(rm);
      Term in_addr   = ru_build_input_addr(rm);
      Term rewritten = ru_rewrite_subtree(self, info->loc, info->op, in_addr);
      // Same REDUCE-axis rewire as the non-realized branch below: when
      // this node itself is a REDUCE that landed at the realize boundary,
      // its axis cell still holds the original shape-axis index. The
      // walker expects the fresh UOP_RANGE.axis_id.
      if (info->op == UOP_REDUCE && RU_REDUCE_RANGES[i].n == 1
          && term_tag(rewritten) == TAG_UOP
          && term_ext(rewritten) == UOP_REDUCE) {
        Term rng = RU_REDUCE_RANGES[i].ranges[0];
        u32 r_aid = (u32)term_val(heap_read(term_val(rng) + 0));
        u32 kind  = (u32)term_val(heap_read(term_val(rewritten) + 1));
        Term src  = heap_read(term_val(rewritten) + 0);
        rewritten = uop_reduce(kind, r_aid, src);
      }
      Term closed_ranges[MAX_DIM];
      u32 n_closed = ru_collect_closed_ranges(i, closed_ranges, MAX_DIM);
      // addrspace mirrors tinygrad's BufferizeOpts.addrspace:
      // full-realize -> GLOBAL device memory; partial-realize falls back
      // to LOCAL (threadgroup-shared) because the per-axis collapse
      // matches tinygrad's `len(range_map[s][1]) != len(realized_ranges)`
      // branch at tinygrad/schedule/indexing.py:75-76.
      u32 addrspace = RU_REALIZE_MAP[i].realized_full
                    ? UOP_SCOPE_GLOBAL
                    : UOP_SCOPE_LOCAL;
      // removable mirrors `removable = x.op is not Ops.COPY and s.op
      // not in ALWAYS_CONTIGUOUS` from indexing.py:73. thvm has no
      // COPY opcode at the tensor-level UOp layer yet (TCopy lives in
      // backend dispatch); we default removable=1 here.
      u32 removable = 1;
      Term b = uop_bufferize_new(rewritten, addrspace, removable,
                                 n_closed, closed_ranges);
      RU_BUFFERIZE_TERM[i] = b;
      RU_LAST_BUFFERIZES_EMITTED++;
      // Mirror kernel_lift_to_uop's `out->store_root`: assemble a
      // UOP_STORE(out_buf, addr, value) where out_buf is shaped by
      // the n_closed RANGE extents and dtype comes from the
      // rewritten value.  Only emit if dtype is recoverable; the
      // legacy lifter remains the fallback otherwise.
      u32 store_dtype = 0;
      if (term_dtype_in(self, 0, &store_dtype) && store_dtype != 0) {
        Shape out_shape = {0};
        if (term_shape_in(self, 0, &out_shape) && out_shape.ndim <= MAX_DIM) {
          Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, store_dtype,
                                         out_shape.ndim, out_shape.dims, 0);
          RU_STORE_ROOT[i] = uop_store(out_buf, my_addr, rewritten);
        }
      }
      // RU_SUBST holds the bare BUFFERIZE; ru_rewrite_subtree wraps
      // each downstream consumer with uop_index_e(BUFFERIZE, consumer_in_addr)
      // so cross-realize loads use the CONSUMER's iter (mirrors
      // tinygrad/schedule/indexing.py:78 `BUFFERIZE.index(*consumer_ranges)`).
      // The pre-wrapped `uop_index_e(b, my_addr)` form would pin reads to
      // the PRODUCER's range axis_id -- correct only when consumer and
      // producer share the same RANGE Term, which fails when both are
      // realized (each minted a fresh range at lines 621-622).
      RU_SUBST[i] = b;
    } else {
      // Mirror indexing.py:107 (passthrough INDEX without BUFFERIZE wrap).
      // The consumer threads our VALUE EXPRESSION at the current iter.
      // RU_SUBST is the REWRITTEN subtree: TAG_TEN children are wrapped
      // with INDEX_E at the input addr (in_rngs for REDUCE, out_rngs
      // otherwise), and downstream producer RU_SUBSTs are spliced in.
      // The consumer's own ru_rewrite_subtree then plugs this in
      // verbatim (its TAG_UOP child branch hits our RU_SUBST entry).
      Term in_addr   = ru_build_input_addr(rm);
      Term rewritten = ru_rewrite_subtree(self, info->loc, info->op, in_addr);
      // Rewire UOP_REDUCE's axis field from the original shape-axis index
      // (e.g. "axis 1 of {3,2}") to the freshly minted UOP_RANGE's axis_id.
      // uop_graph_rebuild_with_srcs preserves the original axis cell from
      // info->loc, but downstream walkers (cpu_uop_walk's uwalk_run_reduce,
      // render_uop's rmu_emit_store_reduce) match the REDUCE's stored axis
      // against the body's UOP_RANGE.axis_id -- which is now the fresh
      // RU_REDUCE_RANGES[i].ranges[0]'s axis_id, not the shape-axis index.
      if (info->op == UOP_REDUCE && RU_REDUCE_RANGES[i].n == 1
          && term_tag(rewritten) == TAG_UOP
          && term_ext(rewritten) == UOP_REDUCE) {
        Term rng = RU_REDUCE_RANGES[i].ranges[0];
        u32 r_aid = (u32)term_val(heap_read(term_val(rng) + 0));
        u32 kind  = (u32)term_val(heap_read(term_val(rewritten) + 1));
        Term src  = heap_read(term_val(rewritten) + 0);
        rewritten = uop_reduce(kind, r_aid, src);
      }
      RU_SUBST[i] = rewritten;
    }

    // Mirror indexing.py:98-99 (remove_movement_op_after_rangeify):
    //   movement ops are gone after rangeify; substitute points to the
    //   producer's substitute directly.
    int is_movement = (info->op == UOP_RESHAPE || info->op == UOP_PERMUTE
                    || info->op == UOP_EXPAND  || info->op == UOP_PAD
                    || info->op == UOP_SHRINK  || info->op == UOP_FLIP);
    if (is_movement) {
      // Forward to producer's substitute if it exists; otherwise unwrap
      // the movement-op shell built by ru_rewrite_subtree -- its slot 0
      // already holds the INDEX_E(producer, swizzled_addr) we want
      // consumers to splice in (the swizzler folded the broadcast /
      // permute into in_addr). Leaving the bare RESHAPE/EXPAND/etc.
      // wrapper in RU_SUBST[i] surfaces it in downstream consumer DAGs
      // (e.g. matmul's REDUCE-of-MUL body) where the walker has no
      // movement-op handler. Mirror: tinygrad's
      // remove_movement_op_after_rangeify strips the outer op
      // unconditionally (indexing.py:98-99).
      Term producer = term_resolve(heap_read(info->loc));
      if (term_tag(producer) == TAG_UOP) {
        u32 pidx = bufferize_info_find(term_val(producer));
        if (pidx != 0xFFFFFFFFu && RU_SUBST[pidx] != 0) {
          RU_SUBST[i] = RU_SUBST[pidx];
          continue;
        }
      }
      // No upstream BUFFERIZE_NODES entry (typically a TAG_TEN leaf).
      // Unwrap the movement shell: RU_SUBST[i] is currently the rebuilt
      // movement-op Term (e.g. UOP_EXPAND(UOP_INDEX_E(...), ndim, dims))
      // and we want just the inner INDEX_E. Slot 0 holds it directly.
      Term cur = RU_SUBST[i];
      if (term_tag(cur) == TAG_UOP) {
        Term inner = heap_read(term_val(cur) + 0);
        if (inner != 0) RU_SUBST[i] = inner;
      }
    }
  }
}
