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

// === Env-gate. Mirrors the THVM_* env-pattern used elsewhere. ===
// Default ON; set THVM_UNIFIED_RANGEIFY=0 to opt back into the OLD
// named-rule pipeline.
fn int rangeify_unified_enabled(void) {
  char const *e = getenv("THVM_UNIFIED_RANGEIFY");
  if (e == NULL) return 1;
  return e[0] != '0';
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

// === Mirror source: indexing.py:148-269 run_rangeify ===
//
// Reverse-topo walk over BUFFERIZE_NODES. Since BUFFERIZE_NODES is
// populated post-order by bufferize_walk_rec (children before parents),
// iterating it in REVERSE is equivalent to tinygrad's
// `reversed(tsink.toposort(gate_kernel_sink))`. Parents (consumers)
// come first; children (producers) follow.
//
// Pre-condition: caller must have run bufferize_classify(root) first;
// this fills BUFFERIZE_NODES + CMAP_LL + the existing realized-bit.
//
// Side effects: populates RU_RANGE_MAP + RU_REALIZE_MAP + RU_ENDING_RANGES.
// The realize decision is kept in RU_REALIZE_MAP separate from
// BUFFERIZE_NODES.realized; bufferize_classify_project_unified
// reconciles them after the walk.

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
  // The OLD path's bufferize_classify already produces a realize set
  // (root + multi-consumer + REDUCE + matmul-protect). Mirror that
  // here by seeding RU_REALIZE_MAP from the existing UOpInfo.realized
  // bit -- this is the closest analog to running pm_generate_realize_map
  // without re-implementing the whole rewrite. Tinygrad's pm_generate_realize_map
  // realizes COPY/CONTIGUOUS/STORE + srcs of COPY/MSELECT/MSTACK; thvm
  // doesn't have those opcodes today, so the seed equals BUFFERIZE_NODES.realized.
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (BUFFERIZE_NODES[i].realized) {
      RU_REALIZE_MAP[i].realized_full = 1;
      RU_REALIZE_MAP[i].n_realized_axes = MAX_DIM;  // placeholder; filled when shape is known
    }
  }

  // *** Mirror indexing.py:161 (reverse-topo walk) ***
  // BUFFERIZE_NODES is populated by bufferize_walk_rec in DFS PRE-order:
  // parents are appended before recursing into children. So iterating
  // FORWARD (idx 0 = root, last = deepest leaf) is consumers-before-
  // producers, which mirrors tinygrad's `reversed(tsink.toposort())`.
  // (Tinygrad's toposort returns producers-first; `reversed` flips to
  // consumers-first; our DFS pre-order already gives consumers-first.)
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    u32 node_idx = i;
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

// Builds the "INDEX expression" Term that a consumer references when it
// reads this node. Mirrors tinygrad's
//   `new_src = new_src.index(*ctx.range_map[x][0])`
// We model `.index(*r)` as a chain of UOP_INDEX_E.addr over an IADD-tree
// of (range_i * stride_i) -- tinygrad's INDEX is multi-arg but the
// downstream stride-collapse simplification yields the same scalar.
static Term ru_build_index_addr(RuRangeMap const *rm) {
  if (rm->out_ndim == 0) return uop_const(DT_INT32, 0);
  // Row-major: addr = ... ((r0*d1 + r1)*d2 + r2)*d3 + ...
  // We don't have the consumer's shape dims handy without rederiving,
  // so we emit a flat IADD of all ranges (real-stride wiring is a
  // follow-up that needs the producer's shape).
  Term acc = rm->out_rngs[0];
  for (u8 a = 1; a < rm->out_ndim; a++) {
    acc = uop_int_binary(UOP_IADD, acc, rm->out_rngs[a]);
  }
  return acc;
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

fn void pm_apply_rangeify(Term root) {
  (void)root;
  // Clear substitute + bufferize tables.
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    RU_SUBST[i]           = 0;
    RU_BUFFERIZE_TERM[i]  = 0;
  }
  RU_LAST_BUFFERIZES_EMITTED = 0;

  // Walk nodes bottom-up (children-first) so each consumer sees its
  // producers' substitutes. BUFFERIZE_NODES is DFS PRE-order
  // (parents first), so we iterate REVERSE here -- the opposite of
  // run_rangeify_unified's consumer-first walk.
  for (i64 ii = (i64)BUFFERIZE_NODES_LEN - 1; ii >= 0; ii--) {
    u32 i = (u32)ii;
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
      // The main-heap node lands via uop_bufferize_new.
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
      Term b = uop_bufferize_new(self, addrspace, removable,
                                 n_closed, closed_ranges);
      RU_BUFFERIZE_TERM[i] = b;
      RU_LAST_BUFFERIZES_EMITTED++;
      // The consumer-side INDEX expression now references the
      // BUFFERIZE node, mirroring indexing.py:78
      //   `new_src = new_src.index(*[r for ... if i in realized_ranges])`.
      Term addr = ru_build_index_addr(rm);
      RU_SUBST[i] = uop_index_e(b, addr);
    } else {
      // Mirror indexing.py:107 (passthrough INDEX without BUFFERIZE wrap).
      // The consumer reads the producer's source directly under the
      // producer's in_rngs.  We re-use the node Term as a placeholder
      // for inspection.
      Term addr = ru_build_index_addr(rm);
      RU_SUBST[i] = uop_index_e(self, addr);
    }

    // Mirror indexing.py:98-99 (remove_movement_op_after_rangeify):
    //   movement ops are gone after rangeify; substitute points to the
    //   producer's substitute directly.
    int is_movement = (info->op == UOP_RESHAPE || info->op == UOP_PERMUTE
                    || info->op == UOP_EXPAND  || info->op == UOP_PAD
                    || info->op == UOP_SHRINK  || info->op == UOP_FLIP);
    if (is_movement) {
      // Forward to producer's substitute if it exists; otherwise keep
      // our own (the swizzler already pushed ranges to in_rngs, but the
      // consumer still references this node's loc).
      Term producer = term_resolve(heap_read(info->loc));
      if (term_tag(producer) == TAG_UOP) {
        u32 pidx = bufferize_info_find(term_val(producer));
        if (pidx != 0xFFFFFFFFu && RU_SUBST[pidx] != 0) {
          RU_SUBST[i] = RU_SUBST[pidx];
        }
      }
    }
  }
}
