// schedule/rangeify_unified.c
//
// 1-to-1 port of tinygrad/schedule/indexing.py:run_rangeify (lines
// 148-269) + pm_apply_rangeify (lines 101-110).
//
// The unified pass is the only rangeify path; it replaces the
// named-rule bufferize_classify + materialize.c visit() + kernel_lift
// trio with one imperative reverse-topological walk that:
//   - assigns per-axis RANGE expressions to every node in the DAG
//   - decides realize boundaries by looking at the consumer ranges
//     (consumer-divergence, ending-ranges, REDUCE/EXPAND injection)
//   - emits a rewrite map that pm_apply_rangeify uses to replace each
//     node's src with the appropriate BUFFERIZE/INDEX expression
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

// THVM_FUSE_CONV_BWD strand-realize cap: a strand-triggered force-realize
// only fires when the node's output iter product is at or below this many
// elements.  The conv-bwd weight/data-grad reduce boundaries are small
// (cout*cin*kh*kw ~ 0.6M); the unfold MUL product they fuse is multi-GB.
// Capping here realizes the small reduce-output boundary (where the
// strand cleanly closes) while leaving the unfold to fuse as a
// strided-view read.  ~4M elems == 16MB f32, comfortably between the two.
#define RU_STRAND_REALIZE_MAX_NUMEL ((u64)4u * 1024u * 1024u)
#define RU_MAX_ENDING (RU_MAX_NODES * 4u)

// Forward decl: the THVM_FUSE_CONV_BWD gate (defined below) is read by
// ru_apply_movement, which lexically precedes the definition.
static int ru_fuse_conv_bwd_enabled(void);

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

#define RU_FUSE_ALL_REDUCE_AIDS_CAP 512
static u32 RU_FUSE_ALL_REDUCE_AIDS[RU_FUSE_ALL_REDUCE_AIDS_CAP];
static u32 RU_FUSE_ALL_REDUCE_AIDS_N;

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
// observable until ALL its consumers' range_maps were filled.  We
// compute a proper Kahn's-algorithm order on BUFFERIZE_NODES below.
static u32             RU_TOPO_ORDER[RU_MAX_NODES];
static u32             RU_TOPO_ORDER_LEN;
static u32             RU_TOPO_REMAINING[RU_MAX_NODES];  // unprocessed-consumer counter

// Stats / introspection accessors for the new test.
static u32 RU_LAST_NODES_WALKED;

fn u32 rangeify_unified_last_nodes_walked   (void) { return RU_LAST_NODES_WALKED;   }
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

// THVM_RU_FAITHFUL_SEED: seed the rangeify realize-map from tinygrad's
// structural boundaries (ROOT == STORE) only and let the walk derive the
// rest (see the seed loop in run_rangeify_unified).  Shared by the seed
// loop, materialize's boundary gate, and the strand-realize cap below.
fn int ru_faithful_seed_on(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_RU_FAITHFUL_SEED");
    on = (e != NULL && e[0] != '0' && e[0] != '\0');
    known = 1;
  }
  return on;
}

// Whether a bufferize-classify realized node is seeded as a structural
// boundary up front (vs left for the walk to derive).  Default: every
// realized node (the heuristic seed).  Faithful: ROOT only (== tinygrad
// STORE), the rest derived by the walk.  (A drop-MULTI-only middle ground
// was tried and removed: it kept the REDUCE boundaries thvm's codegen needs
// so it was correct, but it cut only ~10 kernels with no speed change -- the
// real granularity win is in fusing REDUCEs, which is the codegen gap.)
fn int ru_seed_boundary_holds(u32 reasons) {
  if (ru_faithful_seed_on()) {
    if (reasons & BUFFERIZE_REASON_ROOT) return 1;
    // Seed every REDUCE output as a boundary.  This is faithful to tinygrad
    // (one reduce per kernel; a REDUCE output always escapes into a buffer)
    // and is the lever that de-fuses the conv data-grad.  Without it the
    // ROOT-only walk leaves the FORWARD conv REDUCE un-realized, so the
    // backward maxpool-grad RECOMPUTES the conv forward inline -- the giant
    // [8,64,8,8] kid (the `_acc8` conv-forward recompute fused with the
    // col2im `in4[divmod]` gather, ~28% of the wall) -- to rebuild the
    // argmax mask, and lowers the data-grad as a per-lane col2im gather.
    // Seeding the REDUCE realizes the conv forward once; the mask then reads
    // an affine materialized buffer and the data-grad lowers as a strided
    // reduce (tinygrad's shape).  THVM_RU_NO_SEED_REDUCE=1 reverts to the
    // ROOT-only seed for A/B comparison.
    static int no_seed_reduce = -1;
    if (no_seed_reduce < 0) {
      char const *e = getenv("THVM_RU_NO_SEED_REDUCE");
      no_seed_reduce = (e != NULL && e[0] == '1') ? 1 : 0;
    }
    if (!no_seed_reduce && (reasons & BUFFERIZE_REASON_REDUCE)) return 1;
    return 0;
  }
  return 1;
}

// THVM_FUSE_CONV_BWD: is `aid` a REDUCE-range / realized-scope axis
// anywhere in the last rangeify pass?  Used by materialize.c's
// would-strand check to mirror the rangeify-side covered-check so a
// hash-cons-aliased foreign axis in a fusing conv-bwd product's value
// does not re-trigger materialize-side realization.  Empty (returns 0)
// when the fuse flag is off.
fn int rangeify_unified_aid_is_fuse_bound(u32 aid) {
  for (u32 g = 0; g < RU_FUSE_ALL_REDUCE_AIDS_N; g++)
    if (RU_FUSE_ALL_REDUCE_AIDS[g] == aid) return 1;
  return 0;
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
  // Always bump RU_RANGE_IDX_COUNTER so the axis_id space mirrors
  // tinygrad's (one axis per output dim, including size-1).
  // Downstream consumers - kernel_apply_opt's apply_opt_dag_find_range,
  // tile/codegen axis lookup, etc. - index into this space by
  // user-facing axis number; collapsing size-1 axes to CONST(0)
  // without bumping the counter would shift every subsequent axis_id
  // down by one.
  u32 axis_id = RU_RANGE_IDX_COUNTER++;
  if (ru_extent_is_one(extent)) {
    // Mirror tinygrad: UOp.const(dtypes.weakint, 0).  The collapsed
    // CONST(0) still consumes axis_id `axis_id`; the loss of the
    // RANGE leaf is harmless at the addr-build layer since size-1
    // axes contribute stride*0 to the address.
    return uop_const(DT_INT32, 0);
  }
  return uop_range(axis_id, axistype, extent);
}

// Walk a UOp subtree and return 1 if any UOP_RANGE leaf has axis_id == aid.
// Used after ru_rewrite_subtree to detect when a REDUCE's body lost the
// reduce-axis range (e.g. a CONST broadcast collapsed past the rewrite,
// leaving the REDUCE iterating an axis that the walker can't measure).
static int ru_subtree_uses_axis(Term t, u32 aid, u32 depth) {
  if (depth > 64) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 0;
  u8 op = term_ext(r);
  if (op == UOP_RANGE) {
    return uop_range_axis_id(r) == aid;
  }
  // Don't descend into UOP_BUFFER (its dim cells are TAG_NUM leaves) or
  // UOP_BUFFERIZE (opaque cross-realize boundary).
  if (op == UOP_BUFFER || op == UOP_BUFFERIZE) return 0;
  u8 ar = uop_arity(op);
  u64 loc = term_val(r);
  for (u8 i = 0; i < ar; i++) {
    if (ru_subtree_uses_axis(heap_read(loc + i), aid, depth + 1)) return 1;
  }
  return 0;
}

// Build f32 CONST term with given value.
static u32 ru_f32_bits(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

// Repair a REDUCE term whose body lost the reduce-axis range (e.g. a
// stride-0 broadcast collapsed past ru_rewrite_subtree).  The
// cpu_uop_walk's uwalk_run_reduce walks the body for a UOP_RANGE with
// axis_id == r_aid to recover the loop extent; without one it falls back
// to the reduce identity (0 for SUM, -INF for MAX) and the kernel writes
// zeros.  Repair: the reduce of a body that's constant along the
// reduce axis equals `body * extent` for SUM (and just `body` for
// MAX/MIN).
static Term ru_reduce_repair_broadcast_body(Term reduce_t,
                                            Term const *reduce_ranges,
                                            u32 n_ranges) {
  if (term_tag(reduce_t) != TAG_UOP || term_ext(reduce_t) != UOP_REDUCE) {
    return reduce_t;
  }
  if (n_ranges == 0) return reduce_t;
  // Repair fires only when the body is INVARIANT over EVERY reduce axis:
  // a stride-0 broadcast source collapsed each reduce-axis RANGE to
  // CONST(0) past ru_rewrite_subtree, so the body references none of them
  // and the walker's r_extent==0 fallback would yield 0.  SUM then equals
  // body * prod(extents); MAX/MIN equals body.  Handles 1 OR MORE reduce
  // axes (a `bias.expand(...,H,W).sum((H,W))` conv-bias / broadcast grad
  // reduces 2+ broadcast axes -- single-axis-only repair left those at 0).
  // When the body still uses some reduce axis we bail and let the walker
  // reduce the surviving ranges as-is (the partial-collapse case).
  u32 n_axes = uop_reduce_n_axes(reduce_t);
  u32 kind   = uop_reduce_kind(reduce_t);
  Term body  = uop_reduce_src(reduce_t);
  for (u32 a = 0; a < n_axes; a++) {
    if (ru_subtree_uses_axis(body, uop_reduce_axis(reduce_t, a), 0)) {
      return reduce_t;
    }
  }
  if (kind == REDUCE_SUM) {
    u64 prod = 1;
    for (u32 r = 0; r < n_ranges; r++) {
      Term rng = reduce_ranges[r];
      if (term_tag(rng) != TAG_UOP || term_ext(rng) != UOP_RANGE) {
        return reduce_t;
      }
      u32 extent = uop_range_extent(rng);
      if (extent == 0) return reduce_t;
      prod *= (u64)extent;
    }
    if (prod == 1) return body;
    Term k = uop_const(DT_FP32, ru_f32_bits((f32)prod));
    return uop_binary(UOP_MUL, body, k);
  }
  // MAX/MIN of a body constant over every reduce axis == body.
  return body;
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
// the op isn't a movement op the swizzler handles, or if the RESHAPE
// shape pair needs pm_simplify_valid (not yet ported).
static int ru_apply_movement(u64 loc, u8 op,
                              Term const *out_rngs, u32 out_ndim,
                              Term *in_rngs, u32 *in_ndim) {
  if (op == UOP_RESHAPE) {
    // Decompose the reshape into matching axis groups and swizzle the
    // consumer's flat ranges into producer-axis-aligned IDIV/IMOD
    // expressions.  Declines the fold when the shapes don't decompose
    // cleanly (the stride-trick rank-merge tinygrad routes through
    // `pm_simplify_valid`, not yet ported).
    Shape src_shape;
    if (!ru_node_src_shape(loc, &src_shape)) {
      *in_ndim = 0;
      return 0;
    }
    u32 out_dims[RU_MAX_AXES] = {0};
    for (u32 i = 0; i < out_ndim; i++) {
      out_dims[i] = (u32)term_val(heap_read(loc + 2 + i));
    }
    u32 in_dims[RU_MAX_AXES] = {0};
    u32 src_ndim = src_shape.ndim;
    if (src_ndim > RU_MAX_AXES) src_ndim = RU_MAX_AXES;
    for (u32 i = 0; i < src_ndim; i++) in_dims[i] = src_shape.dims[i];
    // THVM_FUSE_CONV_BWD: route the RESHAPE swizzle through the placeholder
    // round-trip (indexing.py:140-143).  Substituting each consumer free
    // RANGE for a clean single placeholder before the flat-decompose lets
    // the divmod-recombine fire symbolically, so a consecutive split +
    // re-split (the `_pool` unfold feeding the col2im) recombines back to
    // its source iter instead of leaking independent 6/24 decode axes into
    // a 10^14-iter cross-product.  Flag-OFF keeps the bare path bit-exact.
    if (ru_fuse_conv_bwd_enabled()) {
      if (apply_movement_op_reshape_composed(out_ndim, out_dims, src_ndim,
                                             in_dims, out_rngs, in_rngs)) {
        *in_ndim = src_ndim;
        return 1;
      }
      *in_ndim = 0;
      return 0;
    }
    if (apply_movement_op_reshape(out_ndim, out_dims, src_ndim, in_dims,
                                  out_rngs, in_rngs)) {
      *in_ndim = src_ndim;
      return 1;
    }
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
    // Broadcast aligns by SUFFIX: when target rank > src rank, the new
    // axes are prepended at the front and contribute extent 1 each.
    u32 ins[RU_MAX_AXES], outs[RU_MAX_AXES];
    u32 rank_diff = out_ndim > src_shape.ndim ? out_ndim - src_shape.ndim : 0;
    for (u32 i = 0; i < out_ndim; i++) {
      ins[i]  = (i < rank_diff) ? 1 : src_shape.dims[i - rank_diff];
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
// unvisited (cycle / inconsistent consumer_count); the forward order
// still satisfies the consumer-after-producer invariant for any
// already-toposorted prefix and degrades gracefully on pathological
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
  // THVM_RU_FAITHFUL_SEED=1: seed ONLY structural realize boundaries
  // (ROOT == tinygrad's STORE; pm_generate_realize_map seeds only
  // COPY/CONTIGUOUS/STORE, indexing.py:28-35).  The MULTI (consumer>=2),
  // REDUCE, MATMUL and FANIN_CAP reasons are thvm heuristics that tinygrad
  // does NOT seed -- it DERIVES realization in the walk below via
  // multi-consumer range divergence (indexing.py:196-220) and the
  // reduce/elementwise ending-ranges rule (indexing.py:222-232).  Seeding
  // them up front pre-empts that derivation and over-realizes (the
  // 240-vs-5 kernel beautiful_mnist gap).  Default OFF until the walk's
  // derivation is proven to cover every case the heuristic seeds did
  // (MaxPool / Softmax / BatchNorm / CE).
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (!BUFFERIZE_NODES[i].realized) continue;
    if (!ru_seed_boundary_holds(BUFFERIZE_NODES[i].reasons)) continue;
    RU_REALIZE_MAP[i].realized_full = 1;
    RU_REALIZE_MAP[i].n_realized_axes = MAX_DIM;
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

    // A rank-changing RESHAPE -- the pool windowing
    // `x.reshape(B,C,Ho,k,Wo,k)` (split) and the pool-gradient merge
    // `reshape((B,C,Ho,k,Wo,k)->(B,C,Ho,Wo))` -- swizzles the consumer's
    // ranges into compound IDIV/IMOD/affine index expressions
    // (apply_movement_op_reshape) that flow into the reshape SOURCE's
    // out_rngs.  The source no longer needs a force-realize band-aid: the
    // general strand covered-check below (ru_expr_references_aid) now
    // recognises the swizzled leaf axes inside those compound out_rngs as
    // covered, so the source FUSES into the consuming reduce/elementwise
    // kernel instead of materializing.  Mirrors tinygrad, which lowers
    // relu->max_pool2d->sum to a single fused reduce kernel
    // (schedule/indexing.py `_apply_reshape` + symbolic simplify +
    // `remove_movement_op_after_rangeify`).

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
      // A broadcast EXPAND that injects (stride-0) range axes is the one
      // movement op kept fused on divergence: its divergent consumer
      // ranges are exactly the reduce axes of the consuming REDUCEs,
      // covered by their ending_ranges (indexing.py:223-232,249-250), so
      // tinygrad never materializes it (a conv-bwd out_grad broadcast is
      // 1.3 GB).  thvm's ru_all_same_axis compares RANGE Term identity,
      // seeing those reduce-injected axes as divergent; the share-as-view
      // shortcut recovers the fused EXPAND.
      //
      // SHRINK / RESHAPE / PERMUTE / PAD / FLIP are partial-realized on
      // genuine divergence, faithful to tinygrad: a SHRINK (getitem) or
      // PERMUTE (transpose) view of a computed source whose consumers map
      // the SAME producer axis to DIFFERENT downstream axes (q.reshape(M,
      // N,1) vs q.reshape(M,1,N); Gw vs Gw.T) CANNOT collapse to consumer
      // 0's ranges -- that carries consumer 0's stride into consumer 1's
      // read (silent wrong result).  tinygrad mints a fresh range + marks
      // the axis realized (indexing.py:213-215), so each consumer LOADs
      // from a real buffer with its own ShapeTracker.  thvm does the same
      // via the per-axis else branch below + the realized-movement
      // BUFFERIZE boundary (see the RU_SUBST = BUFFERIZE keep further
      // down).  Restrict the share-as-view shortcut to EXPAND only.
      if (!all_all_same && info->op == UOP_EXPAND) {
        all_all_same = 1;
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
          // Mirror indexing.py:75: a BUFFERIZE whose realized_ranges cover
          // EVERY output axis (len(range_map[s][1]) == len(realized_ranges))
          // is a full GLOBAL boundary, not a LOCAL partial.
          //
          // A MOVEMENT op (SHRINK / RESHAPE / PERMUTE / FLIP) that realizes
          // on consumer-divergence is FULLY realized regardless of how many
          // axes diverged: it is a pure VIEW, so a partial-realize over just
          // the divergent axis would leave the materialized buffer indexed
          // by the NON-divergent axes only, and the divergent consumers (a
          // getitem c[:,0] vs c[:,1], a transpose Gw vs Gw.T) would read it
          // with the realized axis UNBOUND -- the begin offset / permuted
          // stride is silently dropped (two different slices collapse to the
          // same read).  tinygrad realizes the whole view (the RESHAPE/SHRINK
          // gets realize_map[x] = list(range(ndim)) once any axis diverges),
          // giving each consumer its own ShapeTracker over a contiguous
          // buffer.  REDUCE / elementwise partial-realizes that legitimately
          // drop a keepdim-reduced axis (softmax denom, layer-norm mean) are
          // NOT movement ops, so they stay partial and keep their fusion.
          // EXPAND is force-shared above (line ~912) and never reaches this
          // branch; PAD carries a value-side WHERE guard handled separately.
          // Only the index-remapping views land here.
          int is_mv = (info->op == UOP_RESHAPE || info->op == UOP_PERMUTE
                    || info->op == UOP_SHRINK  || info->op == UOP_FLIP);
          if (n_realized == my_ndim || is_mv) {
            RU_REALIZE_MAP[node_idx].realized_full = 1;
            RU_REALIZE_MAP[node_idx].realized_partial = 0;
            RU_REALIZE_MAP[node_idx].axes_mask =
                (my_ndim < 8) ? (u8)((1u << my_ndim) - 1u) : 0xFFu;
            RU_REALIZE_MAP[node_idx].n_realized_axes = my_ndim;
            // Close ALL axes: mint a fresh LOOP range for each non-divergent
            // axis too, so the materialized buffer is iterated over its full
            // contiguous shape (the divergent axes already got fresh ranges
            // above).
            for (u8 a = 0; a < my_ndim; a++) {
              if (!(partial_mask & (u8)(1u << a))) {
                out_rngs[a] = ru_new_range(shape.dims[a], KAX_LOOP);
              }
            }
          } else {
            RU_REALIZE_MAP[node_idx].realized_partial = 1;
            RU_REALIZE_MAP[node_idx].axes_mask        = partial_mask;
            RU_REALIZE_MAP[node_idx].n_realized_axes  = n_realized;
          }
        }
      }
    }

    // *** Mirror indexing.py:223-232 ***
    // "if this element is a reduce and there's ended ranges, we might
    //  have to end some other ranges (...)"
    int is_elementwise = uop_is_unary_elementwise(info->op)
                      || uop_is_binary_elementwise(info->op)
                      || uop_is_ternary_elementwise(info->op);
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
      // Multi-axis REDUCE: every axis in the node's axes-list gets a
      // fresh KAX_REDUCE range; non-reduce dims thread out_rngs through.
      //
      // RU_REDUCE_RANGES[node_idx] records every fresh REDUCE range
      // Term so materialize.c can enumerate them via accessor without
      // re-deriving from the heap (thvm equivalent of tinygrad's
      // convert_reduce_to_reduce_with_ranges packing them into
      // src=(value,)+tuple(new_ranges) at indexing.py:94).
      Shape src_shape;
      Term red_t = term_new(0, TAG_UOP, info->op, info->loc);
      u32 n_axes = uop_reduce_n_axes(red_t);
      if (!ru_node_src_shape(info->loc, &src_shape)) {
        for (u8 a = 0; a < my_ndim; a++) in_rngs[a] = out_rngs[a];
      } else {
        u8 is_reduce_axis[RU_MAX_AXES] = {0};
        for (u32 ai = 0; ai < n_axes; ai++) {
          u32 ax = uop_reduce_axis(red_t, ai);
          if (ax < RU_MAX_AXES) is_reduce_axis[ax] = 1;
        }
        u8 dst_ndim = (u8)src_shape.ndim;
        if (dst_ndim > RU_MAX_AXES) dst_ndim = RU_MAX_AXES;
        u8 src_cursor = 0;
        RU_REDUCE_RANGES[node_idx].n = 0;
        for (u8 a = 0; a < dst_ndim; a++) {
          if (is_reduce_axis[a]) {
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
// inference declined.  Consumed by kernel_lift.c via the
// rangeify_unified_store_root_at accessor.
static Term RU_STORE_ROOT[RU_SUBST_CAP];

// Side table: per-axis range terms preserved at INDEX_E construction.
// Mirror: tinygrad's `BUFFERIZE.index(*per_axis_ranges)` carries per-axis
// range terms directly in the INDEX node's args
// (tinygrad/schedule/indexing.py:78).  thvm's `uop_index_e(buffer, addr)`
// is a 2-arg form with a flat addr (sum over stride*range products);
// the per-axis info is otherwise lost the moment the flat addr is built
// by `ru_build_input_addr_for`.  Materialize.c's bypass rewriter needs
// the per-axis info to substitute closed_ranges -> consumer in_rngs
// (the spec at indexing.py:78 `new_src.index(*[r for i,r in enumerate
// (ctx.range_map[x][0]) if i in realized_ranges])`); without it the
// rewriter has to RE-DECOMPOSE the flat addr and fails when the
// addr contains movement-op swizzlers (IDIV/IMOD/inner-IADD).
//
// Key = INDEX_E heap loc (`term_val(index_e_term)`).  Linear-scan
// lookup, capped at RU_INDEX_AXIS_CAP entries per pass.
#define RU_INDEX_AXIS_CAP 16384
static u64  RU_INDEX_AXIS_KEY  [RU_INDEX_AXIS_CAP];
static u8   RU_INDEX_AXIS_NDIM [RU_INDEX_AXIS_CAP];
static Term RU_INDEX_AXIS_RNGS [RU_INDEX_AXIS_CAP][RU_MAX_AXES];
static u32  RU_INDEX_AXIS_N;

static void ru_index_axes_register(Term index_e_term, Term const *rngs,
                                    u8 ndim) {
  if (RU_INDEX_AXIS_N >= RU_INDEX_AXIS_CAP) return;
  if (term_tag(index_e_term) != TAG_UOP) return;
  u64 loc = term_val(index_e_term);
  // Dedup: same INDEX_E loc may be constructed multiple times (hash-cons
  // returns the same term).  Skip if already registered.
  for (u32 i = 0; i < RU_INDEX_AXIS_N; i++) {
    if (RU_INDEX_AXIS_KEY[i] == loc) return;
  }
  RU_INDEX_AXIS_KEY [RU_INDEX_AXIS_N] = loc;
  RU_INDEX_AXIS_NDIM[RU_INDEX_AXIS_N] = ndim > RU_MAX_AXES ? RU_MAX_AXES : ndim;
  for (u8 a = 0; a < RU_INDEX_AXIS_NDIM[RU_INDEX_AXIS_N]; a++) {
    RU_INDEX_AXIS_RNGS[RU_INDEX_AXIS_N][a] = rngs[a];
  }
  RU_INDEX_AXIS_N++;
}

// Public wrapper for ru_index_axes_register.  Used by materialize.c's
// bypass rewriter to propagate per-axis info when it rebuilds an
// INDEX_E (e.g. BUFFERIZE->BUFFER promotion).  Without this the
// rebuilt INDEX_E loses its side-table entry.
fn void rangeify_unified_index_axes_register(Term index_e_term,
                                             Term const *rngs, u8 ndim) {
  ru_index_axes_register(index_e_term, rngs, ndim);
}

// Reverse lookup: given a BUFFERIZE Term, find its producer node_idx
// in RU_BUFFERIZE_TERM[].  Used by materialize.c's bypass rewriter to
// resolve orphan BUFFERIZEs (those not in BOUNDARY_BUFFERIZE_TERM[]
// because their node didn't get a boundary kernel).  Returns
// 0xFFFFFFFFu when no match.
// Per-axis realize mask from RU_REALIZE_MAP.  Bit i set iff axis i
// is in the realized set (closed in BUFFERIZE).  Returns 0 for nodes
// out of bounds.
fn u8 rangeify_unified_axes_mask_at(u32 node_idx) {
  if (node_idx >= RU_MAX_NODES) return 0;
  return RU_REALIZE_MAP[node_idx].axes_mask;
}

// 1 iff the node is fully realized (all output axes closed); 0 for
// partial-realize or not-realized.  The materialize bypass rewriter
// uses this with rangeify_unified_out_rng_at to recover the positional
// closed-range -> output-axis map (mirroring ru_collect_closed_ranges).
fn int rangeify_unified_realized_full_at(u32 node_idx) {
  if (node_idx >= RU_MAX_NODES) return 0;
  return RU_REALIZE_MAP[node_idx].realized_full ? 1 : 0;
}

fn u32 rangeify_unified_node_idx_for_bufferize(Term buf) {
  if (buf == 0) return 0xFFFFFFFFu;
  for (u32 i = 0; i < RU_SUBST_CAP; i++) {
    if (RU_BUFFERIZE_TERM[i] == buf) return i;
  }
  return 0xFFFFFFFFu;
}

fn u8 rangeify_unified_index_axes_lookup(u64 index_loc, Term *out_rngs,
                                          u8 cap) {
  for (u32 i = 0; i < RU_INDEX_AXIS_N; i++) {
    if (RU_INDEX_AXIS_KEY[i] != index_loc) continue;
    u8 n = RU_INDEX_AXIS_NDIM[i];
    if (n > cap) n = cap;
    for (u8 a = 0; a < n; a++) out_rngs[a] = RU_INDEX_AXIS_RNGS[i][a];
    return n;
  }
  return 0;
}

// Per-tid flag: set to 1 by ru_compose_view_chain when the unified pass
// folds td->prior_views into the INDEX_E.addr for a TAG_TEN input.
// materialize.c's input_slot_dedup reads this and sets the matching
// ke->input_chain_composed[slot]=1 so the backend's pre-mat pass skips
// the gather (the addr already encodes the chain walk).  Mirrors the
// rangeify.c emit_chained_index_from_addr + input_chain_composed
// bookkeeping; without it the backend would gather the strided view
// into a contig buffer AND the kernel would read at our chain-composed
// addr -- double-application, wrong reads.
static u8 RU_TID_CHAIN_COMPOSED[TENS_CAP];

fn int rangeify_unified_tid_chain_composed(u32 tid) {
  if (tid == 0 || tid >= TENS_CAP) return 0;
  return RU_TID_CHAIN_COMPOSED[tid] != 0;
}

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

// Build a row-major linear addr Term from an array of per-axis range
// expressions and the producing tensor's shape.  Each range may be a
// bare UOP_RANGE leaf (the elementwise case) or an arithmetic
// expression over RANGE leaves (the RESHAPE swizzler's IDIV/IMOD
// decomposition). Strides come from the dims[] argument when supplied;
// when dims[] is NULL we recover them from each UOP_RANGE leaf's
// extent (heap field 2) and fall back to a flat IADD if any axis isn't
// a bare RANGE -- caller responsibility for the elementwise path.
static Term ru_build_addr_with_dims(Term const *rngs, u8 ndim,
                                     u32 const *dims) {
  if (ndim == 0) return uop_const(DT_INT32, 0);
  u32 local_dims[RU_MAX_AXES] = {0};
  if (dims == NULL) {
    // Derive per-axis extents from each rng.  RANGE leaves contribute
    // their extent; CONST(0) (broadcast-axis placeholder) contributes
    // 1 (no stride mass).  Any other shape (a swizzler IADD/IMUL,
    // CONST(non-zero), etc.) defaults to 1 too -- the surrounding
    // call already knows the underlying numel from src_shape when
    // it matters; this path is the dims-NULL fallback.
    for (u8 a = 0; a < ndim; a++) {
      Term r = rngs[a];
      if (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE) {
        local_dims[a] = (u32)term_val(heap_read(term_val(r) + 2));
      } else {
        local_dims[a] = 1;
      }
    }
    dims = local_dims;
  }
  u32 strides[RU_MAX_AXES] = {0};
  strides[ndim - 1] = 1;
  for (i8 a = (i8)ndim - 2; a >= 0; a--) {
    strides[a] = strides[a + 1] * dims[a + 1];
  }
  Term acc = 0;
  for (u8 a = 0; a < ndim; a++) {
    if (strides[a] == 0) continue;
    Term term = rngs[a];
    // Drop CONST(0) addends and stride-1 IMULs to keep the tree tight.
    if (term_tag(term) == TAG_UOP && term_ext(term) == UOP_CONST
        && term_val(heap_read(term_val(term) + 0)) == 0) {
      continue;
    }
    if (strides[a] != 1) {
      term = uop_int_binary(UOP_IMUL, term,
                            uop_const(DT_INT32, strides[a]));
    }
    acc = (acc == 0) ? term : uop_int_binary(UOP_IADD, acc, term);
  }
  return acc != 0 ? acc : uop_const(DT_INT32, 0);
}

static Term ru_build_addr_from_ranges(Term const *rngs, u8 ndim) {
  return ru_build_addr_with_dims(rngs, ndim, NULL);
}

// Builds the "INDEX expression" Term that a consumer references when it
// reads this node. Mirrors tinygrad's
//   `new_src = new_src.index(*ctx.range_map[x][0])`
// We model `.index(*r)` as a chain of UOP_INDEX_E.addr over an IADD-tree
// of (range_i * stride_i) -- tinygrad's INDEX is multi-arg but the
// downstream stride-collapse simplification yields the same scalar.
// When the node's own output shape (heap-resolvable via `self_term`) is
// known we thread its dims through so the addr's row-major strides match
// the realized BUFFER's stride layout.  Without dims, size-1 axes hash-
// cons to UOP_CONST(0) and ru_build_addr_with_dims falls back to a
// stride-1 IADD chain (the non-bare-RANGE bail-out), which produces
// wrong addrs when any axis is degenerate.
static Term ru_build_index_addr_for(RuRangeMap const *rm, Term self_term) {
  Shape out_shape;
  if (self_term != 0 && term_shape_in(self_term, 0, &out_shape)
      && out_shape.ndim == rm->out_ndim) {
    u32 dims[RU_MAX_AXES] = {0};
    for (u8 a = 0; a < rm->out_ndim; a++) dims[a] = out_shape.dims[a];
    return ru_build_addr_with_dims(rm->out_rngs, rm->out_ndim, dims);
  }
  return ru_build_addr_from_ranges(rm->out_rngs, rm->out_ndim);
}

// Body/input addr: includes the reduce range (and any movement-op
// swizzle). For non-REDUCE nodes this is identical to the output addr
// because in_rngs == out_rngs; for REDUCE the reduce range was injected
// at raxis in run_rangeify_unified's UOP_REDUCE branch.  When `loc` is
// supplied we read the producer's shape (heap[loc + 0]) and use its
// dims for strides -- needed when the RESHAPE / PAD swizzler produced
// IDIV/IMOD expressions whose extents can't be recovered from a bare
// UOP_RANGE leaf.
// Heap loc 0 is a legitimate heap allocation -- it can't double as
// "no loc" without colliding.  Use HEAP_NEXT (one past the high-water
// mark) as the sentinel.
#define RU_NO_LOC ((u64)~0ull)

static Term ru_build_input_addr_for(RuRangeMap const *rm, u64 loc) {
  Shape src_shape;
  if (loc != RU_NO_LOC && ru_node_src_shape(loc, &src_shape)
      && src_shape.ndim == rm->in_ndim) {
    u32 dims[RU_MAX_AXES] = {0};
    for (u8 a = 0; a < rm->in_ndim; a++) dims[a] = src_shape.dims[a];
    return ru_build_addr_with_dims(rm->in_rngs, rm->in_ndim, dims);
  }
  return ru_build_addr_from_ranges(rm->in_rngs, rm->in_ndim);
}

// === prior_views chain composition into INDEX_E.addr ===
//
// Mirrors rangeify.c's `input_chain_foldable` +
// `emit_chained_index_from_addr` over scalar UOps -- here emitted as
// UOp DAG IDIV/IMOD/IMUL/IADD via uop_int_binary.  The unified pass
// builds `in_addr` as a row-major flat index into the producer's iter
// cube, which for a TAG_TEN with `td->nviews > 0` equals the
// public-view shape's row-major flat index (assuming the public view
// is contiguous -- the common case after a RESHAPE on a non-contig
// source appended a fresh canonical outermost view via
// tensor_view_chain_append).  Compose prior_views[nviews-1] ...
// prior_views[0] via decompose-by-shape: coord[d] = (flat /
// suffix[d]) % dims[d], buffer_idx += coord[d] * strides[d] + offset.
// Returns 0 on bail (non-foldable chain, negative strides, packed
// dtype, non-contig public view); caller then leaves in_addr alone
// and the backend pre-mat handles the chain at dispatch time.
static int ru_chain_foldable(u32 tid) {
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  TenDesc const *td = &TENS[tid];
  if (td->nviews == 0 || td->prior_views == NULL) return 0;
  if (dtype_is_packed(td->dtype)) return 0;
  // Outermost public view must be contig+offset0 so the unified pass's
  // row-major flat addr lands at the right entry of prior_views[nviews-1].
  if (!td->view.contiguous || td->view.offset != 0) return 0;
  for (u8 k = 0; k < td->nviews; k++) {
    View const *pv = &td->prior_views[k];
    if (pv->shape.ndim < 1 || pv->shape.ndim > MAX_DIM) return 0;
    if (pv->offset < 0) return 0;
    for (u32 d = 0; d < pv->shape.ndim; d++) {
      if (pv->strides[d] < 0) return 0;
    }
  }
  return 1;
}

// Build the per-view decompose-by-shape map as a UOp DAG over `cur`.
// `cur` is a flat index into pv->shape; returns sum_d coord_d*pv->strides[d]
// + pv->offset where coord_d = (cur / suffix[d]) % pv->shape.dims[d].
// Broadcast axes (strides[d]==0) contribute nothing.  Mirror of
// rangeify.c's build_addr_from_flat_iter over the UOp DAG.
static Term ru_compose_one_view(Term cur, View const *pv) {
  u32 ndim = pv->shape.ndim;
  if (ndim == 0) {
    return pv->offset != 0 ? uop_const(DT_INT32, pv->offset) : uop_const(DT_INT32, 0);
  }
  u32 suffix[MAX_DIM];
  suffix[ndim - 1] = 1;
  for (i32 d = (i32)ndim - 2; d >= 0; d--) {
    suffix[d] = suffix[d + 1] * pv->shape.dims[d + 1];
  }
  Term acc = pv->offset != 0 ? uop_const(DT_INT32, pv->offset) : 0;
  for (u32 d = 0; d < ndim; d++) {
    if (pv->strides[d] == 0) continue;          // broadcast: no contribution
    Term coord = cur;
    if (suffix[d] != 1) {
      coord = uop_int_binary(UOP_IDIV, coord, uop_const(DT_INT32, suffix[d]));
    }
    if (d != 0) {
      coord = uop_int_binary(UOP_IMOD, coord, uop_const(DT_INT32, pv->shape.dims[d]));
    }
    if (pv->strides[d] != 1) {
      coord = uop_int_binary(UOP_IMUL, coord, uop_const(DT_INT32, pv->strides[d]));
    }
    acc = (acc == 0) ? coord : uop_int_binary(UOP_IADD, acc, coord);
  }
  return acc != 0 ? acc : uop_const(DT_INT32, 0);
}

// Walk the chain prior_views[nviews-1] -> prior_views[0], composing
// each view's decompose-by-shape into the running flat index.  Returns
// the original addr on bail; sets RU_TID_CHAIN_COMPOSED[tid]=1 on
// success so input_slot_dedup can flip ke->input_chain_composed[].
static Term ru_compose_view_chain(Term addr0, u32 tid) {
  if (addr0 == 0) return addr0;
  if (!ru_chain_foldable(tid)) return addr0;
  TenDesc const *td = &TENS[tid];
  Term cur = addr0;
  for (i32 k = (i32)td->nviews - 1; k >= 0; k--) {
    cur = ru_compose_one_view(cur, &td->prior_views[k]);
    if (cur == 0) return addr0;
  }
  RU_TID_CHAIN_COMPOSED[tid] = 1;
  return cur;
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

// Per-axis range substitution table used when splicing a non-realized
// producer's RU_SUBST into a consumer.  The producer minted its
// out_rngs against one of its consumers (typically the first one
// reached in topological order); a different consumer reading the same
// shared producer has its own in_rngs at the same axis positions.
// Without rewiring, the spliced subtree carries the producer-scope
// axis_ids verbatim and uop_subtree_has_stranded_range trips because
// cpu_uop_walk only sets up loop slots for the consumer's iter axes.
//
// CAP at 2*RU_MAX_AXES so we have room for the OUT_RNGS rewire pairs
// (one per axis, up to RU_MAX_AXES) PLUS the fresh-range injections
// (also up to RU_MAX_AXES) used when a producer-internal range that
// was a REDUCE-axis got stripped past `ru_rewrite_subtree` and needs a
// fresh consumer-scope range with an enclosing REDUCE_SUM scaffold.
#define RU_AXIS_SUBST_CAP (RU_MAX_AXES * 2)
typedef struct {
  u32 from_aid[RU_AXIS_SUBST_CAP];   // producer-side axis_id to replace
  Term to_term [RU_AXIS_SUBST_CAP];  // consumer-side replacement Term
  u8  n;
} RuAxisSubst;

static int ru_axis_subst_lookup(RuAxisSubst const *m, u32 aid, Term *out) {
  for (u8 i = 0; i < m->n; i++) {
    if (m->from_aid[i] == aid) {
      *out = m->to_term[i];
      return 1;
    }
  }
  return 0;
}

// Free-axis tracker used to find producer-scope RANGE leaves in a
// spliced subtree without an enclosing UOP_REDUCE.  As we descend into
// a UOP_REDUCE we push its axis_id onto the bound-set; on exit we pop.
// A UOP_RANGE leaf whose axis_id is NOT in the bound set is "free" --
// the caller's iter scope is responsible for iterating it.
//
// We cap the bound set at RU_AXIS_SUBST_CAP since reduce-axes nest at
// most as deep as the producer's value subtree, which is bounded by
// RU_MAX_AXES * 2 in practice (each REDUCE level adds one axis).
typedef struct {
  u32 bound_aids[RU_AXIS_SUBST_CAP];
  u8  n_bound;
  u32 free_aids[RU_AXIS_SUBST_CAP];
  u8  free_types[RU_AXIS_SUBST_CAP];  // axis_type of each free leaf
  u8  n_free;
} RuFreeAxisSet;

// Does expression `t` reference a UOP_RANGE leaf with axis_id == aid
// anywhere in its DAG?  Stops at BUFFER/BUFFERIZE/KERNEL boundaries
// (their internal ranges are scoped separately).  Used by the strand
// covered-check so a free axis that appears inside a COMPOUND out_rngs
// entry (the IDIV/IMOD/affine swizzle a rank-changing RESHAPE builds,
// `aid_hi*k + aid_lo`) counts as covered: the consumer iterates those
// leaf axes in its own scope and substitutes the whole compound
// expression on splice (ru_subtree_rewrite_ranges via in_rngs).
static int ru_expr_references_aid(Term t, u32 aid, u32 depth) {
  if (depth > 64) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 0;
  u8 op = term_ext(r);
  if (op == UOP_RANGE) return uop_range_axis_id(r) == aid;
  if (op == UOP_BUFFER || op == UOP_BUFFERIZE || op == UOP_KERNEL) return 0;
  u8 ar = uop_arity(op);
  u64 loc = term_val(r);
  for (u8 i = 0; i < ar; i++)
    if (ru_expr_references_aid(heap_read(loc + i), aid, depth + 1)) return 1;
  return 0;
}

static int ru_free_axis_bound_has(RuFreeAxisSet const *s, u32 aid) {
  for (u8 i = 0; i < s->n_bound; i++) if (s->bound_aids[i] == aid) return 1;
  return 0;
}

static int ru_free_axis_free_has(RuFreeAxisSet const *s, u32 aid) {
  for (u8 i = 0; i < s->n_free; i++) if (s->free_aids[i] == aid) return 1;
  return 0;
}

static void ru_free_axis_add_free(RuFreeAxisSet *s, u32 aid, u8 atype) {
  if (ru_free_axis_free_has(s, aid)) return;
  if (s->n_free < RU_AXIS_SUBST_CAP) {
    s->free_types[s->n_free] = atype;
    s->free_aids[s->n_free++] = aid;
  }
}

// Walk subtree `t`, populate `s->free_aids` with axis_ids of UOP_RANGE
// leaves that are NOT inside the scope of a UOP_REDUCE with matching
// axis.  Stops at UOP_BUFFER / UOP_BUFFERIZE / UOP_KERNEL leaves
// (opaque boundaries; their internal ranges are scoped separately).
static void ru_collect_free_axes_rec(Term t, RuFreeAxisSet *s, u32 depth) {
  if (depth > 128) return;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return;
  u8 op = term_ext(r);
  if (op == UOP_RANGE) {
    u32 aid = uop_range_axis_id(r);
    if (!ru_free_axis_bound_has(s, aid))
      ru_free_axis_add_free(s, aid, (u8)uop_range_axis_type(r));
    return;
  }
  if (op == UOP_BUFFER || op == UOP_BUFFERIZE || op == UOP_KERNEL) return;
  u64 loc = term_val(r);
  if (op == UOP_REDUCE) {
    // Push EVERY reduce axis onto the bound set, recurse into the body,
    // then pop them all.  Multi-axis support: a REDUCE binds all its
    // axes simultaneously (mirrors tinygrad's per-REDUCE .ranges set;
    // uop/ops.py: ended_ranges contain every axis in src[1:]).
    u32 n_axes = uop_reduce_n_axes(r);
    u32 pushed_count = 0;
    for (u32 i = 0; i < n_axes; i++) {
      u32 r_aid = uop_reduce_axis(r, i);
      if (!ru_free_axis_bound_has(s, r_aid)
          && s->n_bound < RU_AXIS_SUBST_CAP) {
        s->bound_aids[s->n_bound++] = r_aid;
        pushed_count++;
      }
    }
    ru_collect_free_axes_rec(uop_reduce_src(r), s, depth + 1);
    s->n_bound -= pushed_count;
    return;
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    ru_collect_free_axes_rec(heap_read(loc + i), s, depth + 1);
  }
}

// Opt-in (THVM_FUSE_CONV_BWD): collect the axis_ids of every REDUCE
// range bound at a CONSUMER REDUCE scope reachable from `node_idx`
// through a chain of movement-op consumers.  Used by the strand
// covered-check so a free axis that this node leaks but whose proper
// binding lives at the OUTER consumer's REDUCE counts as covered (the
// conv-backward contraction product, op=MUL ndim=7: its reduce axes are
// bound by the downstream _pool-scatter REDUCE, NOT at this movement-op
// level).  Without this the product is force-realized -> the 1.31GB
// unfold materializes.  Walk is hop-capped and unique-consumer per hop
// (multi-consumer fan-out means the binding is genuinely ambiguous, so
// we stop and let the strand check realize).
static int ru_fuse_conv_bwd_enabled(void) {
  char const *e = getenv("THVM_FUSE_CONV_BWD");
  return (e != NULL && e[0] == '1');
}

static void ru_collect_reduce_aids_into(u32 ci, u32 *out_aids, u8 *n_out, u8 cap) {
  for (u8 k = 0; k < RU_REDUCE_RANGES[ci].n; k++) {
    u32 aid = uop_range_axis_id(RU_REDUCE_RANGES[ci].ranges[k]);
    u8 dup = 0;
    for (u8 j = 0; j < *n_out; j++) if (out_aids[j] == aid) { dup = 1; break; }
    if (!dup && *n_out < cap) out_aids[(*n_out)++] = aid;
  }
}

#define RU_CONSRED_BFS_CAP 256
#define RU_CONSRED_AIDS_CAP 64
// BFS the consumer DAG from `node_idx` through movement + elementwise
// consumers, collecting the reduce-range aids of every REDUCE reached.
// The conv-backward contraction product fans out into MULTIPLE
// _pool-scatter chains (data-grad reduces cout, weight-grad reduces
// b/h/w), each scattering through pad/repeat/shrink/reshape (and an
// ADD/MUL accumulation hop) before a distinct REDUCE; the product's
// leaked window axes are bound at those scatter REDUCEs.  A FUSED
// (non-realized) consumer reduce is walked PAST -- riri folds it into
// its own consumer, so the combined kernel binds both scopes' axes.  A
// realized reduce is a hard boundary.  We must visit them all (a
// unique-consumer walk only sees one scope and leaves the others
// genuinely stranded).
static void ru_collect_consumer_reduce_aids(u32 node_idx, u32 *out_aids,
                                            u8 *n_out, u8 cap, u32 hop_cap) {
  u32 queue[RU_CONSRED_BFS_CAP];
  u32 qhead = 0, qtail = 0;
  u32 visited[RU_CONSRED_BFS_CAP];
  u32 n_visited = 0;
  queue[qtail++] = node_idx;
  u32 budget = hop_cap;
  while (qhead < qtail && budget-- > 0) {
    u32 cur = queue[qhead++];
    u32 cidxs[RU_MAX_CONSUMERS];
    u32 nc = ru_consumers_for_node(cur, cidxs, RU_MAX_CONSUMERS);
    for (u32 c = 0; c < nc; c++) {
      u32 ci = cidxs[c];
      u8 seen = 0;
      for (u32 v = 0; v < n_visited; v++) if (visited[v] == ci) { seen = 1; break; }
      if (seen) continue;
      if (n_visited < RU_CONSRED_BFS_CAP) visited[n_visited++] = ci;
      u8 cop = BUFFERIZE_NODES[ci].op;
      if (RU_REDUCE_RANGES[ci].n > 0) {
        // Reached a REDUCE -- record its axes.  If it is itself FUSED
        // (not realized -- e.g. a seed-skipped conv-bwd contraction that
        // riri folds into ITS consumer scatter-reduce), keep walking past
        // it: the combined fused kernel binds BOTH this reduce's axes and
        // the downstream scatter-reduce's window axes, so the producer's
        // leaked window axes are covered by that outer scope.  A REALIZED
        // reduce is a hard boundary (its body is its own scope) -- stop.
        ru_collect_reduce_aids_into(ci, out_aids, n_out, cap);
        int red_realized = RU_REALIZE_MAP[ci].realized_full
                        || RU_REALIZE_MAP[ci].realized_partial;
        if (!red_realized && qtail < RU_CONSRED_BFS_CAP) queue[qtail++] = ci;
        continue;
      }
      int is_movement = (cop == UOP_RESHAPE || cop == UOP_PERMUTE
                      || cop == UOP_EXPAND  || cop == UOP_SHRINK
                      || cop == UOP_PAD     || cop == UOP_FLIP);
      // Also traverse elementwise accumulation hops (the conv-bwd scatter
      // chain crosses an ADD/MUL/WHERE between reduces).
      int is_ew = uop_is_unary_elementwise(cop)
               || uop_is_binary_elementwise(cop)
               || uop_is_ternary_elementwise(cop);
      if ((is_movement || is_ew) && qtail < RU_CONSRED_BFS_CAP) queue[qtail++] = ci;
    }
  }
}

// Walk a spliced subtree and rebuild it with each UOP_RANGE leaf whose
// axis_id matches m->from_aid[k] replaced by m->to_term[k].  Mirrors
// tinygrad's variable-substitution sweep used inside
// create_bufferize_and_index_based_on_ranges when a producer's index
// expression is reused across consumers (indexing.py:56-81 substitutes
// via `.substitute` over the per-consumer range_map).  Other Terms
// (CONST, IADD, etc.) are descended structurally; UOP_BUFFERIZE /
// UOP_BUFFER / UOP_KERNEL are opaque boundaries we never enter.
static Term ru_subtree_rewrite_ranges(Term t, RuAxisSubst const *m,
                                       u32 depth) {
  if (depth > 64 || m->n == 0) return t;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return t;
  u8 op = term_ext(r);
  if (op == UOP_RANGE) {
    Term repl;
    if (ru_axis_subst_lookup(m, uop_range_axis_id(r), &repl)) return repl;
    return t;
  }
  if (op == UOP_BUFFER || op == UOP_BUFFERIZE || op == UOP_KERNEL) return t;
  u8 ar = uop_arity(op);
  if (ar == 0) return t;
  Term srcs[MAX_UOP_SRC] = {0};
  u64 loc = term_val(r);
  int changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_c = heap_read(loc + i);
    Term new_c = ru_subtree_rewrite_ranges(old_c, m, depth + 1);
    srcs[i] = new_c;
    if (new_c != old_c) changed = 1;
  }
  if (!changed) return t;
  Term rebuilt = uop_graph_rebuild_with_srcs(r, srcs);
  // If we're rebuilding an INDEX_E that had per-axis info registered
  // for its original loc, propagate the registration to the rebuilt
  // loc with the substitution m applied to each axis range.  Without
  // this, the bypass rewriter's axis-table lookup on the rebuilt
  // INDEX_E misses and falls back to flat-addr decomposition.
  if (op == UOP_INDEX_E && rebuilt != t) {
    Term old_ax_rngs[MAX_DIM];
    u8 old_n = rangeify_unified_index_axes_lookup(
        term_val(t), old_ax_rngs, MAX_DIM);
    if (old_n > 0) {
      Term new_ax_rngs[MAX_DIM];
      for (u8 a = 0; a < old_n; a++) {
        Term cur = old_ax_rngs[a];
        if (term_tag(cur) == TAG_UOP && term_ext(cur) == UOP_RANGE) {
          Term repl;
          if (ru_axis_subst_lookup(m, uop_range_axis_id(cur), &repl)) {
            cur = repl;
          }
        }
        new_ax_rngs[a] = cur;
      }
      ru_index_axes_register(rebuilt, new_ax_rngs, old_n);
    }
  }
  return rebuilt;
}

// Add a (producer_aid -> consumer_term) pair to `m` if the producer aid
// isn't already mapped.  Dedup keeps the first binding (the per-axis
// walk visits positionally-aligned leaves).
static void ru_axis_subst_add(RuAxisSubst *m, u32 from_aid, Term to_term) {
  for (u8 k = 0; k < m->n; k++) if (m->from_aid[k] == from_aid) return;
  if (m->n < RU_AXIS_SUBST_CAP) {
    m->from_aid[m->n] = from_aid;
    m->to_term [m->n] = to_term;
    m->n++;
  }
}

// Walk a producer out_rng expression `pr` and the consumer in_rng `cr`
// in PARALLEL, mapping each producer UOP_RANGE leaf to the structurally
// aligned consumer subterm.  Both are `(low + high*k)`-style affine
// decodes of the SAME axis (a rank-changing RESHAPE swizzle produces the
// identical tree shape on both sides -- the consumer's in_rngs are the
// producer's out_rngs re-expressed over the consumer's fresh iter).
// When a producer RANGE leaf aligns with a consumer RANGE, map aid->cr;
// when it aligns with a compound consumer subterm (the consumer fused an
// extra split), map aid->that subterm.  Structural mismatch (different
// op) stops descent on that branch -- the bare-leaf fast path already
// covered the common case, so a mismatch just means no extra binding.
static void ru_axis_subst_match_pair(RuAxisSubst *m, Term pr, Term cr,
                                     u32 depth) {
  if (depth > 32) return;
  Term p = term_resolve(pr);
  Term c = term_resolve(cr);
  if (term_tag(p) != TAG_UOP) return;
  u8 pop = term_ext(p);
  if (pop == UOP_RANGE) {
    u32 paid = uop_range_axis_id(p);
    if (p != c) ru_axis_subst_add(m, paid, c);
    return;
  }
  // Only descend through the affine index ops a RESHAPE swizzle builds.
  if (pop != UOP_IADD && pop != UOP_IMUL && pop != UOP_IDIV
      && pop != UOP_IMOD) return;
  // Require the consumer to have the SAME top op so the operands align.
  if (term_tag(c) != TAG_UOP || term_ext(c) != pop) return;
  u64 ploc = term_val(p);
  u64 cloc = term_val(c);
  ru_axis_subst_match_pair(m, heap_read(ploc + 0), heap_read(cloc + 0), depth + 1);
  ru_axis_subst_match_pair(m, heap_read(ploc + 1), heap_read(cloc + 1), depth + 1);
}

// Build the producer->consumer axis_id substitution map for the edge
// where consumer `consumer_rm` reads producer at node `pidx`.  Producer
// minted out_rngs[a] at its own iter scope; consumer reads at the same
// axis position with rm->in_rngs[a].  Bare-range out_rngs map directly.
// COMPOUND out_rngs (a rank-changing RESHAPE swizzles the axis into an
// affine `(low + high*k)` tree) are matched leaf-by-leaf against the
// consumer's structurally-identical in_rng so EVERY producer range leaf
// -- not just a top-level bare range -- gets rebound.  Without the
// compound match, a window-low leaf inside a swizzled out_rng leaks into
// the consumer's value tree as a free LOOP range (the conv-bwd col2im
// read of a realized activation buffer on the 2nd train step strands the
// window -> store-inside-loop 6.8e12-iter hang).  Returns the map.
static RuAxisSubst ru_build_axis_subst(u32 pidx,
                                       RuRangeMap const *consumer_rm) {
  RuAxisSubst m;
  m.n = 0;
  if (pidx >= BUFFERIZE_NODES_LEN) return m;
  RuRangeMap const *prm = &RU_RANGE_MAP[pidx];
  if (!prm->has_ranges) return m;
  u8 nd = prm->out_ndim;
  if (nd > consumer_rm->in_ndim) nd = consumer_rm->in_ndim;
  for (u8 a = 0; a < nd && m.n < RU_AXIS_SUBST_CAP; a++) {
    Term pr = prm->out_rngs[a];
    Term cr = consumer_rm->in_rngs[a];
    if (pr == cr) continue;
    if (term_tag(pr) == TAG_UOP && term_ext(pr) == UOP_RANGE) {
      ru_axis_subst_add(&m, uop_range_axis_id(pr), cr);
    } else if (ru_fuse_conv_bwd_enabled()) {
      // Compound out_rng leaf-match: gated so the flag-OFF baseline keeps
      // the old "skip compound axis" behavior bit-identical.
      ru_axis_subst_match_pair(&m, pr, cr, 0);
    }
  }
  return m;
}

// Rebuild the producer Term at (loc, op) by REWRITING each child slot:
//   - child has RU_SUBST[child_idx] != 0
//       -> if RU_SUBST is a UOP_BUFFERIZE, wrap with uop_index_e(BUFFERIZE, in_addr)
//          so the load happens at THIS consumer's iter (mirrors tinygrad
//          indexing.py:78 -- BUFFERIZE.index(*consumer_ranges)).
//       -> otherwise (non-realized producer's rewritten value) splice in
//          with a producer->consumer axis_id rewrite: the producer minted
//          out_rngs against one of its consumers but other consumers read
//          at different axis_ids, so without the rewrite uop_subtree_has_
//          stranded_range trips on the producer-side leaves.
//   - child is a leaf tensor (TAG_TEN)    ->  wrap with uop_index_e(child, in_addr)
//   - everything else (atoms, passthroughs)  ->  keep
// in_addr is the addr at which TAG_TEN/BUFFERIZE INPUTS are loaded at
// THIS op's iteration -- for elementwise it matches the op's own
// out-addr, for REDUCE it must include the reduce range (built from
// rm->in_rngs).
// Mirror: tinygrad/schedule/indexing.py:create_bufferize_and_index_based_on_ranges
// (lines 56-81), `new_srcs` accumulation loop + final `x.replace(src=tns)`.
static Term ru_rewrite_subtree(Term self, u64 loc, u8 op, Term in_addr,
                                RuRangeMap const *consumer_rm) {
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
          // Stash per-axis range terms alongside the flat addr so the
          // bypass rewriter can substitute closed_ranges directly per
          // tinygrad's spec (indexing.py:78) rather than re-decomposing
          // the flat addr (which fails on movement-op swizzlers).
          if (consumer_rm != NULL) {
            ru_index_axes_register(new_child, consumer_rm->in_rngs,
                                   consumer_rm->in_ndim);
          }
        } else {
          // Non-realized producer: splice in its value subtree, but
          // first rewrite producer-scope RANGE axis_ids to the
          // consumer's equivalent at the same axis position.
          if (consumer_rm != NULL) {
            RuAxisSubst m = ru_build_axis_subst(cidx, consumer_rm);
            new_child = ru_subtree_rewrite_ranges(sub, &m, 0);
          } else {
            new_child = sub;
          }
        }
      }
    } else if (ctag == TAG_TEN || ctag == TAG_VAR) {
      // TAG_VAR (shape-annotated TLam-bound variable): treated as a
      // symbolic input slot just like TAG_TEN.  visit() maps it to
      // KSRC_AS_INPUT via input_slot_dedup_var; the bypass rewriter
      // (unified_rewrite_buffer_for_var in materialize.c) substitutes
      // it with the matching UOP_BUFFER before cpu_uop_walk binds
      // runtime input pointers.
      //
      // For TAG_TEN whose TenDesc carries a non-trivial prior_views
      // chain we fold the ShapeTracker decompose-by-shape into the
      // INDEX_E.addr (mirrors rangeify.c's
      // emit_chained_index_from_addr).  In practice most chained
      // descs are minted by view_resolve INSIDE materialize.c's visit
      // -- not visible here -- so materialize.c's
      // unified_rewrite_rec_sub runs an equivalent compose pass over
      // the rebuilt INDEX_E; this branch handles the rarer case
      // where the chained desc is already on the TAG_TEN at
      // unified-pass time (cross-realize reuse of a chained
      // intermediate).
      Term addr = in_addr;
      if (ctag == TAG_TEN) {
        u32 tid = (u32)term_val(resolved);
        addr = ru_compose_view_chain(in_addr, tid);
      }
      new_child = uop_index_e(resolved, addr);
    }
    srcs[i] = new_child;
    if (new_child != old_child) changed = 1;
  }
  if (!changed) return self;
  return uop_graph_rebuild_with_srcs(self, srcs);
}

// PAD value-side guard: build IWHERE(cond, body, INVALID) where `cond`
// is the AND of per-axis `(out_rng >= begin) & (out_rng < begin + in_dim)`.
// Mirrors tinygrad's PAD-as-WHERE rule (schedule/indexing.py:108) and the
// kernel_lift S_PAD case in src/schedule/kernel_lift.c.  Without this wrap,
// the PAD's "outside the kept window" cells inherit whatever value the
// inner subtree computes -- correct only when the inner is an INDEX_E into
// a tensor whose address swizzler already injected the INVALID guard.
// CONST / EXPAND / WHERE bodies (e.g. a SHRINK gradient's gy=CONST(1)
// seed) have no addr-driven read to gate on, so the WHERE-INVALID has to
// land on the value side.
static Term ru_pad_wrap_where(u64 loc, RuRangeMap const *rm, Term body) {
  Shape src_shape;
  if (!ru_node_src_shape(loc, &src_shape)) return body;
  u32 ndim = rm->out_ndim;
  if (ndim == 0 || src_shape.ndim != ndim) return body;
  Term cond_acc = 0;
  for (u32 d = 0; d < ndim; d++) {
    u32 begin = (u32)term_val(heap_read(loc + 2 + 2 * d));
    u32 in_dim = src_shape.dims[d];
    Term r = rm->out_rngs[d];
    if (r == 0) continue;
    Term lo = 0;
    if (begin > 0) {
      Term s_m1 = uop_const(DT_INT32, begin - 1);
      lo = uop_int_binary(UOP_ILT, s_m1, r);   // (begin-1) < r  iff  r >= begin
    }
    Term hi_bound = uop_const(DT_INT32, in_dim + begin);
    Term hi = uop_int_binary(UOP_ILT, r, hi_bound);
    Term axis_cond = (lo == 0) ? hi : uop_int_binary(UOP_IAND, lo, hi);
    cond_acc = (cond_acc == 0) ? axis_cond
                               : uop_int_binary(UOP_IAND, cond_acc, axis_cond);
  }
  if (cond_acc == 0) return body;
  return uop_iwhere(cond_acc, body, uop_invalid());
}

fn void pm_apply_rangeify(Term root) {
  (void)root;
  // Clear substitute + bufferize tables.
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    RU_SUBST[i]           = 0;
    RU_BUFFERIZE_TERM[i]  = 0;
    RU_STORE_ROOT[i]      = 0;
  }
  // Clear chain-composed tid bookkeeping; the live TENS window is
  // [1, TENS_NEXT) so the reset only touches in-use slots.
  for (u32 t = 1; t < TENS_NEXT && t < TENS_CAP; t++) {
    RU_TID_CHAIN_COMPOSED[t] = 0;
  }
  RU_LAST_BUFFERIZES_EMITTED = 0;
  // Reset per-pass per-axis index side table; populated when
  // pm_apply_rangeify constructs INDEX_E nodes from each consumer's
  // in_rngs.  Consumed by materialize.c's bypass rewriter to
  // substitute closed_ranges via per-axis terms (tinygrad spec at
  // indexing.py:78) instead of re-decomposing the flat addr.
  RU_INDEX_AXIS_N = 0;

  // Opt-in (THVM_FUSE_CONV_BWD): collect the axis_ids of every REDUCE
  // range in the graph.  When the conv-backward contraction fuses, a
  // product's strided-unfold INDEX_E addr can carry hash-cons-shared
  // RANGE leaves whose binding lives at a REDUCE scope NOT reachable
  // through this node's consumer DAG (a different layer's backward; the
  // addr expression hash-conses to the same Term).  Those leaves index
  // CORRECTLY when the product splices into the contraction reduce
  // (verified grad-exact), so they must NOT trip the strand check.  A
  // genuine producer-internal strand (a stripped partial-realize
  // closed_range) is NOT a reduce-range axis, so it still strands.
  RU_FUSE_ALL_REDUCE_AIDS_N = 0;
  if (ru_fuse_conv_bwd_enabled() || ru_faithful_seed_on()) {
    for (u32 n = 0; n < BUFFERIZE_NODES_LEN; n++) {
      // Every REDUCE range axis in the graph...
      for (u8 k = 0; k < RU_REDUCE_RANGES[n].n; k++) {
        u32 aid = uop_range_axis_id(RU_REDUCE_RANGES[n].ranges[k]);
        u8 dup = 0;
        for (u32 j = 0; j < RU_FUSE_ALL_REDUCE_AIDS_N; j++)
          if (RU_FUSE_ALL_REDUCE_AIDS[j] == aid) { dup = 1; break; }
        if (!dup && RU_FUSE_ALL_REDUCE_AIDS_N < RU_FUSE_ALL_REDUCE_AIDS_CAP)
          RU_FUSE_ALL_REDUCE_AIDS[RU_FUSE_ALL_REDUCE_AIDS_N++] = aid;
      }
      // ...plus the LOOP axes owned by a REALIZED scope.  A realized
      // node's out_rngs are closed into its own BUFFERIZE; any of those
      // leaves that hash-cons-aliases into a fusing product's addr is
      // bound at the realized scope (the consumer substitutes it on
      // splice), so it must not trip the strand check either.
      int rn_realized = RU_REALIZE_MAP[n].realized_full
                     || RU_REALIZE_MAP[n].realized_partial;
      if (rn_realized && RU_RANGE_MAP[n].has_ranges) {
        for (u8 a = 0; a < RU_RANGE_MAP[n].out_ndim; a++) {
          u32 ax[RU_MAX_AXES];
          u32 na = ru_collect_range_axes(RU_RANGE_MAP[n].out_rngs[a],
                                         ax, RU_MAX_AXES, 0);
          for (u32 z = 0; z < na; z++) {
            u8 dup = 0;
            for (u32 j = 0; j < RU_FUSE_ALL_REDUCE_AIDS_N; j++)
              if (RU_FUSE_ALL_REDUCE_AIDS[j] == ax[z]) { dup = 1; break; }
            if (!dup && RU_FUSE_ALL_REDUCE_AIDS_N < RU_FUSE_ALL_REDUCE_AIDS_CAP)
              RU_FUSE_ALL_REDUCE_AIDS[RU_FUSE_ALL_REDUCE_AIDS_N++] = ax[z];
          }
        }
      }
    }
  }

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

    // Compute the rewritten subtree FIRST for both realize / non-realize
    // paths.  The realize path uses it as the BUFFERIZE's stored value;
    // the non-realize path uses it directly as the splice substitute.
    // After computing, we apply REDUCE-axis rewire / repair / trivial-
    // strip, then run the strand check that may force-realize.
    Term in_addr   = ru_build_input_addr_for(rm, info->loc);
    Term rewritten = ru_rewrite_subtree(self, info->loc, info->op,
                                        in_addr, rm);
    // Rewire UOP_REDUCE's axes from the original shape-axis indices
    // (e.g. "axes (1, 3) of {3,2,4,5}") to the freshly minted
    // UOP_RANGE.axis_ids.  Multi-axis: one fresh RANGE per axis in
    // RU_REDUCE_RANGES[i].ranges -- the rewritten REDUCE inherits
    // every axis_id from those ranges, dropping shape-coord axes.
    // Downstream walkers (cpu_uop_walk's uwalk_run_reduce, render_uop)
    // match against RANGE.axis_id, not the shape index.
    if (info->op == UOP_REDUCE && RU_REDUCE_RANGES[i].n >= 1
        && term_tag(rewritten) == TAG_UOP
        && term_ext(rewritten) == UOP_REDUCE) {
      u32 n_rrng = RU_REDUCE_RANGES[i].n;
      u32 new_axes[MAX_DIM];
      for (u32 k = 0; k < n_rrng && k < MAX_DIM; k++) {
        Term rng = RU_REDUCE_RANGES[i].ranges[k];
        new_axes[k] = (u32)term_val(heap_read(term_val(rng) + 0));
      }
      u32 kind  = uop_reduce_kind(rewritten);
      Term src  = uop_reduce_src(rewritten);
      rewritten = uop_reduce_multi(kind, n_rrng, new_axes, src);
      // Repair: if the rewritten body lost ALL its reduce-axis RANGE
      // leaves (a stride-0 broadcast collapsed them past
      // ru_rewrite_subtree), collapse the REDUCE to `body * prod(extents)`
      // (SUM) or `body` (MAX/MIN).  Handles 1+ reduce axes -- the
      // multi-axis broadcast case (conv bias, broadcast grads reducing
      // 2+ widened axes) previously fell through and computed 0.
      rewritten = ru_reduce_repair_broadcast_body(
          rewritten, RU_REDUCE_RANGES[i].ranges, n_rrng);
      // THVM_FUSE_CONV_BWD: absorb stranded window-low LOOP axes into the
      // reduce.  When this reduce reads a now-realized activation/param
      // buffer through a col2im window (the conv data-grad on the 2nd
      // train step contracts the `144=6*24`-decoded kh*kw window), the
      // `6`-high half is already a reduce axis but the `24`-low half is a
      // fresh LOOP range minted as the realized buffer's own output axis.
      // It is not in this reduce's output addr and not a reduce axis, so
      // it would render as an outer loop with the store inside it (576x
      // overwrite + 6.8e12 iters).  Such a leaf can only be the
      // contraction window: fold it into the reduce so it accumulates
      // correctly.  Bounded to LOOP leaves that are neither an output axis
      // nor an existing reduce axis -- a genuine output/broadcast axis is
      // always in out_rngs and never absorbed.
      if (ru_fuse_conv_bwd_enabled()
          && term_tag(rewritten) == TAG_UOP
          && term_ext(rewritten) == UOP_REDUCE) {
        RuFreeAxisSet bs;
        bs.n_bound = 0;
        bs.n_free  = 0;
        ru_collect_free_axes_rec(uop_reduce_src(rewritten), &bs, 0);
        u32 absorb[MAX_DIM];
        u32 n_absorb = 0;
        for (u8 fi = 0; fi < bs.n_free && n_absorb < MAX_DIM; fi++) {
          if (bs.free_types[fi] != KAX_LOOP) continue;
          u32 faid = bs.free_aids[fi];
          int is_out = 0;
          for (u8 a = 0; a < rm->out_ndim && !is_out; a++)
            if (ru_expr_references_aid(rm->out_rngs[a], faid, 0)) is_out = 1;
          if (is_out) continue;
          int is_red = 0;
          for (u32 k = 0; k < n_rrng && !is_red; k++)
            if (new_axes[k] == faid) is_red = 1;
          if (is_red) continue;
          absorb[n_absorb++] = faid;
        }
        if (n_absorb > 0) {
          u32 merged_axes[2 * MAX_DIM];
          u32 m_n = 0;
          for (u32 k = 0; k < n_rrng && m_n < 2 * MAX_DIM; k++)
            merged_axes[m_n++] = new_axes[k];
          for (u32 k = 0; k < n_absorb && m_n < 2 * MAX_DIM; k++)
            merged_axes[m_n++] = absorb[k];
          rewritten = uop_reduce_multi(uop_reduce_kind(rewritten), m_n,
                                       merged_axes, uop_reduce_src(rewritten));
        }
      }
    }
    // Trivial REDUCE (extent-1 reduce axis): ru_new_range collapsed
    // the reduce-range to UOP_CONST(0), leaving RU_REDUCE_RANGES[i].n
    // == 0.  cpu_uop_walk's uwalk_run_reduce would find no UOP_RANGE
    // in the body and return 0 (the r_extent==0 fallback at uop_walk.c
    // line 567).  Unwrap the REDUCE shell to its source:
    // sum_{i in [0,1)} f(i) == f(0).
    if (info->op == UOP_REDUCE && RU_REDUCE_RANGES[i].n == 0
        && term_tag(rewritten) == TAG_UOP
        && term_ext(rewritten) == UOP_REDUCE) {
      rewritten = heap_read(term_val(rewritten) + 0);
    }

    // Force-realize when the rewritten subtree contains FREE RANGE
    // leaves whose axis_ids aren't covered by either:
    //   (a) the producer's out_rngs (consumers substitute these on
    //       splice via ru_build_axis_subst), OR
    //   (b) the producer's own REDUCE shells in the subtree (the
    //       walker tracks bound-axes via REDUCE descent).
    // Any other free axis_id is a "stranded" producer-internal range:
    // a partial-realize child's closed_range leaked through the
    // BUFFERIZE inline rewriter, or a REDUCE-axis whose enclosing
    // REDUCE got stripped past ru_rewrite_subtree.  Splicing such a
    // subtree into a consumer trips uop_subtree_has_stranded_range
    // because the consumer's iter scope has no slot for that axis.
    // Force-realize converts the producer to a UOP_BUFFERIZE leaf so
    // the consumer reads via INDEX_E(BUFFERIZE, addr) at its own iter
    // and the inner ranges stay scoped to the producer's BUFFERIZE
    // body (whose closed_ranges = out_rngs cover them).
    //
    // Producer's iter scope (out_rngs) must include at least one
    // non-extent-1 axis -- otherwise this is a scalar producer (all
    // axes collapsed to CONST(0)) and force-realize would emit a
    // 1-element BUFFERIZE which the materialize.c rewriter mis-handles
    // as a broadcast read (numel-1 path).  For scalar producers, the
    // free RANGEs in the rewritten subtree come from a deeper
    // producer's iter that should remain inlined; the strand check is
    // a false positive at this node and the deeper producer (with
    // non-trivial out_rngs) is where realize should actually fire.
    int has_nontrivial_out_axis = 0;
    for (u8 a = 0; a < rm->out_ndim; a++) {
      Term pr = rm->out_rngs[a];
      if (term_tag(pr) == TAG_UOP && term_ext(pr) == UOP_RANGE) {
        has_nontrivial_out_axis = 1;
        break;
      }
    }
    // PAD / SHRINK carry an OFFSET shift between in_rngs and out_rngs
    // (PAD inserts begin zeros; SHRINK skips begin entries).  When
    // force-realized at this node, the realize branch emits a BUFFERIZE
    // whose closed_ranges = out_rngs (covering only OUTPUT-iter axes);
    // any deeper-producer iter axis that leaked into the rewritten
    // subtree survives as a free RANGE because the BUFFERIZE-promotion
    // at materialize.c rebuilds the consumer's in_addr from out_rngs.
    // For conv-im2col's backward path the leaked axes come from an
    // EXPAND-of-broadcast input feeding a multi-reduce chain: their
    // proper binding lives at the OUTER consumer's REDUCE scope, not at
    // this movement-op level.  Defer the realize decision to a deeper
    // non-shift producer where out_rngs actually scopes the strand.
    // RESHAPE / PERMUTE / EXPAND / FLIP are pure swizzles (stride
    // remap, broadcast, sign-flip) so the conservative skip doesn't
    // apply -- their realize at this level still nets a valid
    // BUFFERIZE for downstream INDEX_E.
    int is_shift_op_skip = (info->op == UOP_PAD || info->op == UOP_SHRINK);
    if (!realized && has_nontrivial_out_axis && !is_shift_op_skip) {
      RuFreeAxisSet fs;
      fs.n_bound = 0;
      fs.n_free  = 0;
      ru_collect_free_axes_rec(rewritten, &fs, 0);
      // Opt-in: a free axis whose proper binding is a CONSUMER REDUCE
      // (conv-backward contraction product -> _pool-scatter -> REDUCE)
      // counts as covered.  Force-realizing here would materialize the
      // 1.31GB unfold; deferring lets the product fuse into the
      // scatter-reduce kernel as a strided-view read.
      u32 cons_red_aids[RU_CONSRED_AIDS_CAP];
      u8  n_cons_red = 0;
      if ((ru_fuse_conv_bwd_enabled() || ru_faithful_seed_on()) && fs.n_free > 0) {
        ru_collect_consumer_reduce_aids(i, cons_red_aids, &n_cons_red,
                                        RU_CONSRED_AIDS_CAP, RU_CONSRED_BFS_CAP);
      }
      int stranded = 0;
      for (u8 fi = 0; fi < fs.n_free; fi++) {
        u32 aid = fs.free_aids[fi];
        int covered = 0;
        for (u8 cr = 0; cr < n_cons_red && !covered; cr++) {
          if (cons_red_aids[cr] == aid) covered = 1;
        }
        // Hash-cons-aliased REDUCE / realized-scope axis from a disjoint
        // scope (a different layer's backward whose addr expression
        // hash-conses to the same Term): covered.  These index correctly
        // when the product splices into the contraction reduce (verified
        // grad-exact on conv2_bwd + 2-layer); a genuine producer-internal
        // strand is neither a reduce axis nor a realized-scope axis.
        //
        // Gate the blanket cover on the free leaf's axis_type: only a
        // genuinely REDUCE-typed (KAX_REDUCE) leaf is bound when the
        // product splices into a foreign contraction reduce.  A LOOP-typed
        // free leaf is a real loop-iter axis; on the 2nd train step a
        // col2im read of a now-realized activation buffer mints a fresh
        // LOOP window-low range whose id collides with a reduce id, and
        // the un-gated cover would suppress its strand -> the window
        // renders as an outer loop with the store inside it (the
        // 6.8e12-iter hang).  Keep such a LOOP leaf tripping the strand so
        // its (small reduce-output) producer realizes; the consumer then
        // reads it via a clean INDEX_E addr at its own iter.
        if (!covered && (ru_fuse_conv_bwd_enabled() || ru_faithful_seed_on())
            && fs.free_types[fi] == KAX_REDUCE) {
          for (u32 g = 0; g < RU_FUSE_ALL_REDUCE_AIDS_N && !covered; g++)
            if (RU_FUSE_ALL_REDUCE_AIDS[g] == aid) covered = 1;
        }
        // An axis is covered when it appears ANYWHERE inside one of the
        // producer's out_rngs expressions -- not only as a top-level plain
        // RANGE.  A rank-changing RESHAPE swizzles the consumer ranges into
        // compound IDIV/IMOD/affine out_rngs (`aid_hi*k + aid_lo`); the leaf
        // axes inside those expressions are iterated by the consumer and
        // substituted as a whole on splice (ru_subtree_rewrite_ranges via
        // the consumer's in_rngs).  Treating them as stranded force-realized
        // the reshape source unnecessarily (the old reshape-source band-aid).
        for (u8 a = 0; a < rm->out_ndim && !covered; a++) {
          if (ru_expr_references_aid(rm->out_rngs[a], aid, 0)) covered = 1;
        }
        if (!covered) { stranded = 1; break; }
      }
      if (stranded) {
        // Force-realize this node so the strand closes into its BUFFERIZE
        // output and the consumer reads it via a clean INDEX_E addr.
        //
        // Guard against realizing the multi-GB conv-bwd unfold MUL: its
        // out_rngs span the full `[..,N,kh,kw]` window cube, so its iter
        // product dwarfs the small reduce-output (`[cout,cin,kh,kw]`)
        // boundary where the strand also surfaces.  The producer-first
        // walk visits the MUL before its consuming REDUCE, so capping the
        // realize at a small output-product (<= RU_STRAND_REALIZE_MAX_NUMEL)
        // skips the unfold and lets the strand re-fire (and realize) at the
        // small reduce, which fuses the unfold as a strided-view read.
        // Only the FUSE flag changes behavior; flag-OFF never reaches the
        // type-gated strand (its blanket cover is unchanged for KAX_REDUCE
        // leaves and there is no LOOP-typed window strand without the
        // placeholder-composed reshape).
        // Use the node's ACTUAL output shape (== what emit_kernel_for_boundary
        // passes to tensor_alloc via term_shape_in), NOT the out_rngs RANGE-leaf
        // product.  The latter skips CONST-collapsed axes and can read far below
        // the true allocation size (a 7-D conv-bwd unfold whose out_rngs carry
        // CONST entries measured < the 4M cap while term_shape_in is 1.31GB),
        // evading the cap and force-realizing the multi-GB unfold cube.
        u64 out_numel = 1;
        {
          Shape ns;
          if (ru_node_shape(info->loc, info->op, &ns)) {
            for (u8 a = 0; a < ns.ndim; a++) out_numel *= (u64)(ns.dims[a] ? ns.dims[a] : 1);
          } else {
            for (u8 a = 0; a < rm->out_ndim; a++) {
              Term pr = rm->out_rngs[a];
              if (term_tag(pr) == TAG_UOP && term_ext(pr) == UOP_RANGE)
                out_numel *= (u64)uop_range_extent(pr);
            }
          }
        }
        // Cap the big-strand force-realize under EITHER the conv-bwd fuse
        // flag OR the faithful realize-seed: both want the multi-GB unfold
        // MUL to fuse into its consuming reduce (strand resolved by reduce-
        // axis absorption) rather than materialize the window cube.
        int cap_big_strand = ru_fuse_conv_bwd_enabled() || ru_faithful_seed_on();
        int small_enough = !cap_big_strand
                        || out_numel <= RU_STRAND_REALIZE_MAX_NUMEL;
        // Only force-realize a SMALL boundary.  The multi-GB conv-bwd
        // unfold MUL is left to fuse into its consuming reduce as a
        // strided-view read; its window-low strand is resolved by the
        // reduce-axis absorption above (a stranded LOOP window leaf in a
        // reduce body is folded into the reduce), not by realizing the
        // unfold cube.
        if (small_enough) {
          RU_REALIZE_MAP[i].realized_full = 1;
          RU_REALIZE_MAP[i].n_realized_axes = rm->out_ndim;
          RU_REALIZE_MAP[i].axes_mask =
              (rm->out_ndim < 8) ? (u8)((1u << rm->out_ndim) - 1u) : 0xFFu;
          realized = 1;
        }
      }
    }

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
      Term my_addr = ru_build_index_addr_for(rm, self);
      // The realized-store value subtree for movement-op nodes (EXPAND,
      // RESHAPE, PERMUTE, PAD, SHRINK, FLIP): ru_rewrite_subtree returns
      // the rebuilt movement shell wrapping an INDEX_E whose addr
      // already encodes the swizzle.  The non-realized branch (below)
      // strips that shell into the inner INDEX_E so consumers splice in
      // just the value tree.  Apply the same strip when building
      // RU_STORE_ROOT.value so cpu_uop_walk (no value-layer movement-op
      // handler) sees the indexed read, not the wrapper.  RU_BUFFERIZE_TERM
      // keeps the unstripped form for upstream-boundary identity checks.
      Term store_value = rewritten;
      int is_movement_op = (info->op == UOP_RESHAPE || info->op == UOP_PERMUTE
                         || info->op == UOP_EXPAND  || info->op == UOP_PAD
                         || info->op == UOP_SHRINK  || info->op == UOP_FLIP);
      if (is_movement_op && term_tag(store_value) == TAG_UOP
          && term_ext(store_value) == info->op) {
        Term inner = heap_read(term_val(store_value) + 0);
        if (inner != 0) store_value = inner;
      }
      if (info->op == UOP_PAD) {
        store_value = ru_pad_wrap_where(info->loc, rm, store_value);
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
      // Assemble UOP_STORE(out_buf, addr, value) for the lifter's
      // `out->store_root`: out_buf is shaped by the n_closed RANGE
      // extents, dtype comes from the rewritten value.  Only emit if
      // dtype is recoverable; without a recoverable dtype this slot's
      // store_root stays 0 and kernel_lift_to_uop will reject the
      // kernel.  Note: DT_BOOL == 0 is a valid dtype, so we MUST use
      // term_dtype_in's return value rather than gating on
      // `store_dtype != 0` -- the latter would silently drop every
      // cast-to-bool kernel and leave its output buffer at the init
      // zeros.
      u32 store_dtype = 0;
      if (term_dtype_in(self, 0, &store_dtype)) {
        Shape out_shape = {0};
        if (term_shape_in(self, 0, &out_shape) && out_shape.ndim <= MAX_DIM) {
          Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, store_dtype,
                                         out_shape.ndim, out_shape.dims, 0);
          RU_STORE_ROOT[i] = uop_store(out_buf, my_addr, store_value);
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
      // RU_SUBST is the REWRITTEN subtree (computed and finalized above):
      // TAG_TEN children are wrapped with INDEX_E at the input addr
      // (in_rngs for REDUCE, out_rngs otherwise), and downstream
      // producer RU_SUBSTs are spliced in.  The consumer's own
      // ru_rewrite_subtree then plugs this in verbatim (its TAG_UOP
      // child branch hits our RU_SUBST entry).
      RU_SUBST[i] = rewritten;
    }

    // Mirror indexing.py:98-99 (remove_movement_op_after_rangeify):
    //   `if x in ctx.range_map ... : return x.src[0]` -- but ONLY for a
    //   NON-realized movement op.  A movement op that landed in the
    //   realize_map (full OR partial) stays a boundary: its consumers
    //   wrap it in a BUFFERIZE and index it per-consumer
    //   (create_bufferize_and_index_based_on_ranges, indexing.py:63-78),
    //   so forwarding it to its producer's substitute would discard the
    //   just-emitted BUFFERIZE and re-share the producer view across the
    //   divergent consumers (the exact corruption a partial-realize is
    //   meant to break).  Keep RU_SUBST[i] = b for ANY realized movement
    //   op; only NON-realized movement ops forward.
    int is_movement = (info->op == UOP_RESHAPE || info->op == UOP_PERMUTE
                    || info->op == UOP_EXPAND  || info->op == UOP_PAD
                    || info->op == UOP_SHRINK  || info->op == UOP_FLIP);
    // A realized movement op keeps its BUFFERIZE boundary (RU_SUBST[i] =
    // b, set above).  This is REQUIRED for partial-realize of a SHRINK /
    // RESHAPE / PERMUTE view of a computed source shared by divergent
    // consumers: each consumer must INDEX_E the materialized buffer with
    // its own ShapeTracker rather than inherit consumer 0's stride.  The
    // PERMUTE/RESHAPE-realized swizzle-keep path below is for NON-realized
    // producers only, so it is unreachable here; bail before it.
    if (is_movement && realized) {
      continue;
    }
    if (is_movement) {
      // Forward to producer's substitute by default.  RESHAPE / PERMUTE
      // need a special case when the producer realized to a UOP_BUFFERIZE:
      // their movement-shell's slot 0 already holds INDEX_E(producer_buf,
      // swizzled_addr) where the swizzle encodes the rank merge or axis
      // permutation.  Forwarding to the bare BUFFERIZE drops that
      // swizzle and the consumer re-wraps with INDEX_E(BUFFERIZE,
      // consumer_addr) against the unswizzled output range, reading the
      // wrong element (this is what gd-loss / mse-grad / SGD-step / etc.
      // exercise -- their dot-product chains realize through PERMUTE
      // and RESHAPE).  Unwrap the shell in those cases so the swizzled
      // INDEX_E survives.  EXPAND / PAD / SHRINK / FLIP stick with
      // bare-forward; the bn_grad dgamma chain has an EXPAND whose
      // realized BUFFERIZE producer is laid out so the consumer's own
      // re-wrap is correct, and injecting the pre-swizzled INDEX_E
      // there reads stale data.
      Term producer = term_resolve(heap_read(info->loc));
      if (term_tag(producer) == TAG_UOP) {
        u32 pidx = bufferize_info_find(term_val(producer));
        if (pidx != 0xFFFFFFFFu && RU_SUBST[pidx] != 0) {
          Term psub = RU_SUBST[pidx];
          int psub_is_bufferize = (term_tag(psub) == TAG_UOP
                                && term_ext(psub) == UOP_BUFFERIZE);
          int keep_swizzle = psub_is_bufferize
                          && (info->op == UOP_PERMUTE
                           || info->op == UOP_RESHAPE);
          if (!keep_swizzle) {
            // For non-realized PAD the consumer would otherwise read
            // psub verbatim at every iter -- including iters outside
            // the kept window where the producer's bytes are stale /
            // unrelated.  Wrap psub with the value-side WHERE guard
            // so out-of-window reads collapse to INVALID -> 0.  When
            // psub is a bare BUFFERIZE we first wrap it in INDEX_E at
            // the PAD's input addr (rm->in_rngs already encodes the
            // shifted iter), mirroring ru_rewrite_subtree's
            // BUFFERIZE-as-leaf substitution path.
            Term fwd = psub;
            if (info->op == UOP_PAD && !realized) {
              if (psub_is_bufferize) {
                Term in_addr = ru_build_input_addr_for(rm, info->loc);
                fwd = uop_index_e(psub, in_addr);
                // Stash per-axis range terms for the bypass rewriter's
                // axis-table inline.  Mirror: tinygrad's BUFFERIZE.index
                // (indexing.py:78) at the PAD-wrap path.
                ru_index_axes_register(fwd, rm->in_rngs, rm->in_ndim);
              }
              fwd = ru_pad_wrap_where(info->loc, rm, fwd);
            } else if (psub_is_bufferize
                       && (info->op == UOP_SHRINK || info->op == UOP_FLIP)) {
              // A non-realized SHRINK (getitem) / FLIP over a producer that
              // realized to a BUFFERIZE must read that buffer at its OWN
              // swizzled iter, NOT forward the bare buffer.  The SHRINK's
              // in_rngs already carry the begin offset (apply_movement_op_
              // shrink: in_rng[a] = out_rng[a] + begin), so an INDEX_E at
              // this op's input addr reads the right slice.  Without the
              // wrap, two different getitem slices of one realized source
              // (c[:,0] vs c[:,1], the fused-QKV head split) forward the
              // IDENTICAL bare buffer -- the begin offset is dropped and
              // both reads collapse to the same element.  Mirror tinygrad
              // create_bufferize_and_index_based_on_ranges (indexing.py:78,
              // BUFFERIZE.index(*consumer_ranges)) + the realized-source
              // SHRINK in apply_movement_op (indexing.py:131).
              Term in_addr = ru_build_input_addr_for(rm, info->loc);
              fwd = uop_index_e(psub, in_addr);
              ru_index_axes_register(fwd, rm->in_rngs, rm->in_ndim);
            }
            RU_SUBST[i] = fwd;
            continue;
          }
        }
      }
      // Unwrap the movement shell: RU_SUBST[i] is currently the rebuilt
      // movement-op Term (e.g. UOP_EXPAND(UOP_INDEX_E(...), ndim, dims))
      // and we want just the inner INDEX_E. Slot 0 holds it directly.
      Term cur = RU_SUBST[i];
      if (term_tag(cur) == TAG_UOP) {
        Term inner = heap_read(term_val(cur) + 0);
        if (inner != 0) {
          if (info->op == UOP_PAD && !realized) {
            inner = ru_pad_wrap_where(info->loc, rm, inner);
          }
          RU_SUBST[i] = inner;
        }
      }
    }
  }
}
