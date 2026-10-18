// schedule/rangeify_unified.c - tinygrad's run_rangeify
// (schedule/indexing.py:189-320) and pm_apply_rangeify (indexing.py:136-147).
//
// One reverse-topological walk assigns the nodes their per-axis RANGE
// expressions and decides where to realize (consumer divergence, ending ranges,
// REDUCE axes).  pm_apply_rangeify, a PatternMatcher rewrite in tinygrad, is a
// walk here: it fills a substitution table (RU_SUBST) with each node's
// rewritten value and puts a UOP_BUFFERIZE on the main heap at every realize
// boundary.
//
//   tinygrad                          thvm
//   tsink.toposort(gate_kernel_sink)  RU_TOPO_ORDER over BUFFERIZE_NODES
//   consumer_map                      CMAP_LL + cmap_head
//   range_map[x] = (in, out)          RU_RANGE_MAP[node_idx]
//   realize_map                       RU_REALIZE_MAP[node_idx]
//   ending_ranges                     RU_ENDING_RANGES[node_idx]

#define RU_MAX_NODES  BUFFERIZE_NODES_CAP
#define RU_MAX_AXES   MAX_DIM

#define RU_MAX_ENDING (RU_MAX_NODES * 4u)

// A node's ranges, tinygrad's range_map[x] = (in, out): out_rngs[i] is the
// index expression its consumers read output axis i at, in_rngs[i] the one its
// source is read at (after a movement swizzle or REDUCE axis injection).
// Unfilled entries are 0.
typedef struct {
  Term out_rngs[RU_MAX_AXES];
  Term in_rngs [RU_MAX_AXES];
  u8   out_ndim;
  u8   in_ndim;
  u8   has_ranges;        // 1 once out_rngs has been assigned
} RuRangeMap;

// tinygrad's realize_map[x]: not realized, realized_full (every axis), or
// realized_partial (the axes in axes_mask).
typedef struct {
  u8  realized_full;
  u8  realized_partial;
  u8  axes_mask;          // bit i set iff axis i is realized
  u8  n_realized_axes;
  u8  pinned;             // ru_limit_bufs_node split; remove_bufferize keeps it
} RuRealizeEntry;

// tinygrad's ending_ranges[x]: axis ids of the RANGE leaves an EXPAND in the
// consumer chain ended.  Only emptiness decides; the ids serve the dedup.
#define RU_ENDING_PER_NODE 16
typedef struct {
  u32 axis_ids[RU_ENDING_PER_NODE];
  u8  n;
} RuEndingRanges;

// The fresh REDUCE ranges of a UOP_REDUCE node, which tinygrad's
// convert_reduce_to_reduce_with_ranges puts in src[1:] (indexing.py:107-113).
// An extent-1 reduce axis is CONST(0) and not listed, so n == 0 for a
// non-REDUCE or an all-extent-1 REDUCE.
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

// Consumers before producers: tinygrad's reversed(tsink.toposort())
// (indexing.py:199, 207).  bufferize_walk_rec's DFS pre-order puts a shared
// producer under its first-visited consumer, so it can precede a later
// consumer; ru_compute_topo_order builds a Kahn order instead.
static u32             RU_TOPO_ORDER[RU_MAX_NODES];
static u32             RU_TOPO_ORDER_LEN;
static u32             RU_TOPO_REMAINING[RU_MAX_NODES];  // unprocessed-consumer counter

// Nodes the last pass walked; read by tests (tests/tu_probes.h).
static u32 RU_LAST_NODES_WALKED;

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

// THVM_FUSE_MATMUL_INPUT (default off): inline a single-consumer elementwise
// producer into the matmul that reads it, as tinygrad inlines any
// single-consumer op (indexing.py:242-244).  Otherwise rb_feeds_matmul_reduce
// keeps such a producer realized, since thvm's GEMM paths need buffer operands;
// with the flag the tiled TC emitter computes it at its A-staging load
// (render_uop.c).
fn int ru_fuse_matmul_input_on(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_FUSE_MATMUL_INPUT");
    if (e != NULL && e[0] != '\0') on = (e[0] != '0');
    known = 1;
  }
  return on;
}

// THVM_FUSE_MATMUL_EPILOGUE (default off), the consumer-side sibling: an OPT_TC
// matmul reduce whose only consumer is an elementwise chain is un-realized, so
// its REDUCE inlines into that chain's store value and the codegen epilogue
// (rmu_detect_matmul_tc, rmu_emit_matmul_tc) applies the chain at the matmul's
// C store: one kernel instead of two.  If the fused matmul does not classify as
// a 2-range GEMM (uop_recognise_tc), it runs on the generic parallel
// accumulator: still one kernel, without tensor cores.
fn int ru_fuse_epilogue_on(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_FUSE_MATMUL_EPILOGUE");
    if (e != NULL && e[0] != '\0') on = (e[0] != '0');
    known = 1;
  }
  return on;
}

// Only the Metal tiled matmul (rmu_emit_matmul_tc_tiled) can emit a fused A: it
// rebuilds the producer's (m, k) from the tile origin.  Elsewhere the
// producer's own M/K ranges, behind the matmul's reshape + expand, would leak
// as extra output loops (ru_build_axis_subst binds positionally), and
// rmu_emit_matmul_tc declines a fused A off Metal, so there the producer stays
// a realized operand.
fn int ru_fuse_matmul_input_target_ok(void) {
  return CURRENT_CTX != NULL && CURRENT_CTX->default_device == THVM_DEV_METAL;
}

// The kernel buffer cap, 0 for none.  tinygrad's DEVICE_MAX_BUFS lists only
// WEBGPU (rangeify.py:170): its Metal kernels take one argument-buffer struct
// (renderer/cstyle.py:365, 393-396).  thvm's Metal ABI binds one [[buffer(n)]]
// per buffer (the output at 0, input i at 1+i: cg_render_uop_kernel,
// cg_render_uop_kernel_root, metal_tile_jit_encode_on in backend/metal/_.m),
// and metal_graph_build's ICB allows 31 bindings, so an index past 30 does not
// compile: a correctness cap.  Known gap: metal_tile_jit_encode_on also binds
// the conv-cfg bytes and each symbolic variable after the inputs, which
// ru_limit_bufs_node does not count.  THVM_MAX_KERNEL_BUFFERS > 0 overrides the
// cap on every device, as tinygrad's MAX_KERNEL_BUFFERS does (helpers.py:252).
static u32 ru_device_max_bufs(void) {
  char const *e = getenv("THVM_MAX_KERNEL_BUFFERS");
  if (e != NULL) {
    int v = atoi(e);
    if (v > 0) {
      return (u32)v;
    }
    // tinygrad reads 0 as "use the table", which has no thvm device, so 0 means
    // no cap; only an unset variable keeps the Metal cap.
    return 0;
  }
  if (CURRENT_CTX == NULL) {
    return 0;
  }
  switch (CURRENT_CTX->default_device) {
    case THVM_DEV_METAL: return 31;
    default:             return 0;
  }
}

// Which of bufferize_classify's realize marks seed the walk: the root
// (tinygrad's STORE, indexing.py:49), its REDUCE marks, and the correctness
// realizes below.  The walk derives the rest.
fn int ru_seed_boundary_holds(u32 reasons) {
  if (reasons & BUFFERIZE_REASON_ROOT) return 1;
  // A REDUCE_MAX's input must be a buffer so the forward window max and the
  // backward argmax mask see the same ties (else /count reads RECIP(0)).
  if (reasons & BUFFERIZE_REASON_MAXPOOL_INPUT) return 1;
  // The argmax mask and its normalizer must materialize, or the /count
  // tie-split fuses into the N=1 conv-weight backward reduce, which misreads
  // the count over the size-1 batch axis (a zero grad).
  if (reasons & BUFFERIZE_REASON_MAXPOOL_MASK) return 1;
  // An indexed op's storage operand: only a buffer or a view of a leaf is
  // addressable by the op's computed address (ru_index_leaf).  A SCATTER
  // is always its own read-modify-write kernel (ru_lower_scatter).
  if (reasons & BUFFERIZE_REASON_INDEXED_SRC) return 1;
  if (reasons & BUFFERIZE_REASON_SCATTER) return 1;
  // A REDUCE bufferize_classify marked is a buffer.  tinygrad realizes a reduce
  // only where its ranges end or its consumers diverge (indexing.py:245-276);
  // seeding the marks keeps the conv forward realized once, so the maxpool
  // backward reads its argmax mask from a buffer instead of recomputing the
  // conv inline, and the data-grad lowers as a strided reduce rather than a
  // col2im gather.
  if (reasons & BUFFERIZE_REASON_REDUCE) return 1;
  return 0;
}

// 1 iff `aid` is a REDUCE range, or a range of a realized node, in the last
// pass (RU_FUSE_ALL_REDUCE_AIDS).  materialize.c's bufferize_strand_check_deep
// asks it so a hash-cons-aliased axis from another scope is not stranded, as in
// pm_apply_rangeify's strand check.
fn int rangeify_unified_aid_is_fuse_bound(u32 aid) {
  for (u32 g = 0; g < RU_FUSE_ALL_REDUCE_AIDS_N; g++)
    if (RU_FUSE_ALL_REDUCE_AIDS[g] == aid) return 1;
  return 0;
}

// tinygrad's resolve(s != 1) (uop/ops.py:64) is False only for an s that is 1:
// a symbolic extent is never one.
static int ru_extent_is_one(u32 extent) {
  return kvar_extent_is_var(extent) ? 0 : (kvar_extent_static(extent) == 1);
}

// IndexingContext.new_range (indexing.py:18-21): a fresh RANGE, or CONST(0) for
// an extent-1 axis.
static Term ru_new_range(u32 extent, u32 axistype) {
  // Unlike tinygrad, an extent-1 axis still takes an axis_id, so the ids keep
  // matching output dim positions, which opts (apply_opt_dag_find_range) and
  // the tile/codegen axis lookup rely on.
  u32 axis_id = RU_RANGE_IDX_COUNTER++;
  if (ru_extent_is_one(extent)) {
    // tinygrad's UOp.const(0); a size-1 axis adds nothing to an address.
    return uop_const(DT_INT32, 0);
  }
  return uop_range(axis_id, axistype, extent);
}

// The UOP_RANGE leaf with axis_id `aid` under `t`, or 0 if none.  Does not
// descend into UOP_BUFFER (its dim cells are TAG_NUM leaves), UOP_BUFFERIZE (an
// opaque cross-realize boundary), or, with `stop_kernel`, UOP_KERNEL.
// Iterative and exact, each node expanded once (stamped by heap loc): a false
// "absent" would let ru_reduce_unparented turn a SUM into body * extent.
static u32  *RU_AXIS_SEEN      = NULL;
static u64   RU_AXIS_SEEN_CAP  = 0;
static u32   RU_AXIS_EPOCH     = 0;
static Term *RU_AXIS_STACK     = NULL;
static u64   RU_AXIS_STACK_CAP = 0;

static int ru_axis_stack_push(Term t, u64 *sp) {
  if (*sp == RU_AXIS_STACK_CAP) {
    u64   want  = RU_AXIS_STACK_CAP ? RU_AXIS_STACK_CAP * 2 : 256;
    Term *grown = (Term *)realloc(RU_AXIS_STACK, want * sizeof(Term));
    if (grown == NULL) {
      return 0;
    }
    RU_AXIS_STACK     = grown;
    RU_AXIS_STACK_CAP = want;
  }
  RU_AXIS_STACK[(*sp)++] = t;
  return 1;
}

static Term ru_find_axis_range(Term t, u32 aid, int stop_kernel) {
  u64 want = HEAP_NEXT;
  if (want > RU_AXIS_SEEN_CAP) {
    u32 *grown = (u32 *)realloc(RU_AXIS_SEEN, want * sizeof(u32));
    if (grown != NULL) {
      memset(grown + RU_AXIS_SEEN_CAP, 0, (want - RU_AXIS_SEEN_CAP) * sizeof(u32));
      RU_AXIS_SEEN     = grown;
      RU_AXIS_SEEN_CAP = want;
    }
  }
  if (++RU_AXIS_EPOCH == 0) {
    if (RU_AXIS_SEEN != NULL) {
      memset(RU_AXIS_SEEN, 0, RU_AXIS_SEEN_CAP * sizeof(u32));
    }
    RU_AXIS_EPOCH = 1;
  }
  // A node past the stamped region (the array could not grow) is expanded
  // on every visit: still exact, only slower.
  u64 cap = (RU_AXIS_SEEN != NULL) ? RU_AXIS_SEEN_CAP : 0;
  u64 sp  = 0;
  if (!ru_axis_stack_push(t, &sp)) {
    thvm_fatal("ru_find_axis_range: out of memory");
  }
  while (sp > 0) {
    Term r = term_resolve(RU_AXIS_STACK[--sp]);
    if (term_tag(r) != TAG_UOP) {
      continue;
    }
    u64 loc = term_val(r);
    if (loc < cap) {
      if (RU_AXIS_SEEN[loc] == RU_AXIS_EPOCH) {
        continue;
      }
      RU_AXIS_SEEN[loc] = RU_AXIS_EPOCH;
    }
    u8 op = term_ext(r);
    switch (op) {
      case UOP_RANGE:
        if (uop_range_axis_id(r) == aid) {
          return r;
        }
        continue;
      case UOP_BUFFER: case UOP_BUFFERIZE:
        continue;
      case UOP_KERNEL:
        if (stop_kernel) {
          continue;
        }
        break;
      default:
        break;
    }
    u8 ar = uop_arity(op);
    for (u8 i = 0; i < ar; i++) {
      if (!ru_axis_stack_push(heap_read(loc + i), &sp)) {
        thvm_fatal("ru_find_axis_range: out of memory");
      }
    }
  }
  return 0;
}

// 1 iff `t` reads the range with axis_id `aid`: ru_reduce_unparented asks it of
// a REDUCE body, where a stride-0 broadcast may have collapsed the range.
static int ru_subtree_uses_axis(Term t, u32 aid) {
  return ru_find_axis_range(t, aid, 0) != 0;
}

// The extent of the axis-`aid` RANGE leaf under `t`, or 0 if absent.  The
// THVM_FUSE_MATMUL_INPUT remap matches a leaked producer range to the matmul
// axis (M or K) of equal extent with it.
static u32 ru_subtree_axis_extent(Term t, u32 aid) {
  Term r = ru_find_axis_range(t, aid, 0);
  return (r != 0) ? uop_range_extent(r) : 0;
}

static u32 ru_f32_bits(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

// `v` as a CONST of dtype `dt`: a float CONST payload holds the value's f32
// bits, an int CONST the value (tinygrad's DType.const, dtype.py:79).
static Term ru_const_of(u32 dt, u64 v) {
  if (dtype_is_float(dt)) {
    return uop_const(dt, ru_f32_bits((f32)v));
  }
  return uop_const(dt, (u32)v);
}

// tinygrad's reduce_unparented (codegen/simplify.py:81-91): a reduce range the
// body never reads (a stride-0 broadcast collapsed it to CONST(0)) leaves the
// REDUCE.  A SUM multiplies by its extent (a DEFINE_VAR for a symbolic one); a
// MAX, or a bool SUM (an OR), keeps the body's value.  With no range left the
// REDUCE is its body.
static Term ru_reduce_unparented(Term red, Term const *ranges, u32 n_ranges) {
  if (term_tag(red) != TAG_UOP || term_ext(red) != UOP_REDUCE) {
    return red;
  }
  u32  kind = uop_reduce_kind(red);
  Term body = uop_reduce_src(red);
  u32  parented[MAX_DIM];
  Term unparented[MAX_DIM];
  u32  n_par = 0, n_unpar = 0;
  for (u32 r = 0; r < n_ranges && r < MAX_DIM; r++) {
    Term rng = ranges[r];
    if (term_tag(rng) != TAG_UOP || term_ext(rng) != UOP_RANGE) {
      return red;
    }
    u32 aid = uop_range_axis_id(rng);
    if (ru_subtree_uses_axis(body, aid)) {
      parented[n_par++] = aid;
    } else {
      unparented[n_unpar++] = rng;
    }
  }
  if (n_unpar == 0) {
    return red;
  }
  u32 dt = DT_FP32;
  (void)term_dtype_in(red, 0, &dt);
  Term ret = (n_par > 0) ? uop_reduce_multi(kind, n_par, parented, body) : body;
  if (kind != REDUCE_SUM || dtype_is_bool(dt)) {
    return ret;
  }
  u64 prod = 1;
  for (u32 k = 0; k < n_unpar; k++) {
    u32 ext = uop_range_extent(unparented[k]);
    if (!kvar_extent_is_var(ext)) {
      prod *= (u64)ext;
    }
  }
  if (prod != 1) {
    ret = uop_binary(UOP_MUL, ret, ru_const_of(dt, prod));
  }
  for (u32 k = 0; k < n_unpar; k++) {
    u32 ext = uop_range_extent(unparented[k]);
    if (kvar_extent_is_var(ext)) {
      ret = uop_binary(UOP_MUL, ret, uop_cast(uop_define_var(ext), dt));
    }
  }
  return ret;
}

// Write the axis_ids of the UOP_RANGE leaves of addr expression `t` to
// `out_axes`, at most `cap`, and return how many there are, which can exceed
// `cap`.  A tree walk to depth 32: a shared leaf counts once per path.
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

// Append src's ending ranges to dst, deduplicated to keep the table bounded
// (tinygrad concatenates, indexing.py:217, and only tests for emptiness).
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

// The output shape of the node at (loc, op); 1 on success.
static int ru_node_shape(u64 loc, u8 op, Shape *out) {
  (void)op;
  Term t = term_new(0, TAG_UOP, op, loc);
  return term_shape_in(t, 0, out);
}

// The shape of slot 0, the source of a movement op or REDUCE (tinygrad's
// x.src[0].shape, the pre-swizzle shape).
static int ru_node_src_shape(u64 loc, Shape *out) {
  Term src = term_resolve(heap_read(loc));
  return term_shape_in(src, 0, out);
}

// consumer_map[x] (indexing.py:198-203) as node indices: bufferize_walk_rec
// filled CMAP_LL with the consumer locs; a consumer outside BUFFERIZE_NODES is
// dropped.
#define RU_MAX_CONSUMERS 256
#define RU_CONSRED_BFS_CAP 256
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

// Nodes the walk gives no ranges (indexing.py:208-215).
static int ru_is_skip_op(u8 op) {
  // tinygrad skips CALL, LINEAR, AFTER, MSTACK and MSELECT; thvm has AFTER, and
  // KERNEL is its opaque kernel boundary.
  return op == UOP_AFTER || op == UOP_KERNEL;
}

// One consumer's input ranges for this node: tinygrad's consumer_rngs
// (indexing.py:230).
typedef struct {
  Term rngs[RU_MAX_AXES];
  u8   ndim;
} RuConsumerRangs;

// 1 iff consumer `ci` hands its in_rngs to the node at `loc`.  An indexed op
// builds its storage operands' addresses itself (ru_index_leaf), so a GATHER
// passes its ranges only to its idx slot and a SCATTER only to its base slot; a
// storage operand's views would get ranges shaped for another operand.
static int ru_consumer_passes_rngs(u32 ci, u64 loc) {
  u32 slot;
  switch (BUFFERIZE_NODES[ci].op) {
    case UOP_GATHER:  slot = 1; break;
    case UOP_SCATTER: slot = 0; break;
    default:          return 1;
  }
  Term s = term_resolve(heap_read(BUFFERIZE_NODES[ci].loc + slot));
  return term_tag(s) == TAG_UOP && term_val(s) == loc;
}

static u32 ru_gather_consumer_rngs(u32 node_idx, u8 my_ndim,
                                   RuConsumerRangs *out, u32 cap) {
  u32 consumer_idxs[RU_MAX_CONSUMERS];
  u32 n_c = ru_consumers_for_node(node_idx, consumer_idxs, RU_MAX_CONSUMERS);
  u32 n_out = 0;
  for (u32 i = 0; i < n_c; i++) {
    u32 ci = consumer_idxs[i];
    if (!RU_RANGE_MAP[ci].has_ranges) continue;
    if (!ru_consumer_passes_rngs(ci, BUFFERIZE_NODES[node_idx].loc)) continue;
    // range_map[c][0] has this node's rank: it is c's input for this edge.
    if (n_out >= cap) break;
    RuConsumerRangs *r = &out[n_out++];
    r->ndim = my_ndim;
    for (u8 a = 0; a < my_ndim && a < RU_MAX_AXES; a++) {
      // A consumer with fewer input ranges pads with CONST(0).
      r->rngs[a] = (a < RU_RANGE_MAP[ci].in_ndim)
                 ? RU_RANGE_MAP[ci].in_rngs[a]
                 : uop_const(DT_INT32, 0);
    }
  }
  return n_out;
}

// all_same(local_rngs) on one axis (indexing.py:253).  Hash-consing makes Term
// equality structural; unlike tinygrad (indexing.py:250), the valids take part
// in the comparison.
static int ru_all_same_axis(RuConsumerRangs const *rs, u32 n, u8 axis) {
  if (n <= 1) return 1;
  Term first = rs[0].rngs[axis];
  for (u32 i = 1; i < n; i++) {
    if (rs[i].rngs[axis] != first) return 0;
  }
  return 1;
}

// The ranges a diverging EXPAND shares as a view; each consumer rebinds every
// bare RANGE of them to its own (ru_build_axis_subst).  A broadcast axis reads
// index 0, so consumer 0's range stays.  A passed-through axis whose consumer-0
// range is compound (a PAD valid, a SHRINK offset) gets a fresh range instead,
// or consumer 0's index would leak into the others.  tinygrad realizes the
// EXPAND and remove_bufferize substitutes the consumer ranges back
// (rangeify.py:50-102): the same reads.
static void ru_expand_shared_rngs(u64 loc, RuConsumerRangs const *rs, u32 n,
                                  Shape const *shape, u8 ndim, Term *out_rngs) {
  Shape src;
  int have_src = ru_node_src_shape(loc, &src);
  u32 rank_diff = (have_src && ndim > src.ndim) ? ndim - src.ndim : 0;
  for (u8 a = 0; a < ndim; a++) {
    out_rngs[a] = rs[0].rngs[a];
    if (ru_all_same_axis(rs, n, a)) {
      continue;
    }
    if (have_src) {
      u32 in_dim = (a < rank_diff) ? 1 : src.dims[a - rank_diff];
      if (in_dim != shape->dims[a]) {
        continue;
      }
    }
    Term r = out_rngs[a];
    if (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE) {
      continue;
    }
    out_rngs[a] = ru_new_range(shape->dims[a], KAX_LOOP);
  }
}

// apply_movement_op (indexing.py:171-187, called at :291) through the
// apply_movement_op_* swizzlers of indexing.c.  1 with in_rngs and in_ndim
// filled; 0 when the op has no swizzle here, its source shape is unknown, or
// the RESHAPE swizzle declines.
static int ru_apply_movement(u64 loc, u8 op,
                              Term const *out_rngs, u32 out_ndim,
                              Term *in_rngs, u32 *in_ndim) {
  if (op == UOP_RESHAPE) {
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
    // tinygrad's placeholder round trip (indexing.py:183-185): each consumer
    // RANGE becomes a clean placeholder before the IDIV/IMOD decompose, so a
    // split followed by a re-split (the `_pool` unfold feeding the col2im)
    // folds back to its source range instead of leaking independent axes.
    if (apply_movement_op_reshape_composed(out_ndim, out_dims, src_ndim,
                                           in_dims, out_rngs, in_rngs)) {
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

// RU_TOPO_ORDER, every consumer before its producers, as tinygrad's
// reversed(tsink.toposort()) (indexing.py:199, 207): Kahn's algorithm with
// in-degree consumer_count, seeded with the nodes nothing consumes (the root).
// Nodes it leaves unvisited (a consumer_count that disagrees with the heap
// edges) are appended in table order.
static void ru_compute_topo_order(void) {
  RU_TOPO_ORDER_LEN = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    RU_TOPO_REMAINING[i] = BUFFERIZE_NODES[i].consumer_count;
  }
  // RU_TOPO_ORDER doubles as the FIFO worklist.
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

// A node is a buffer iff bufferize_classify or the walk realized it.
static int rb_node_realized(u32 idx) {
  if (idx == 0xFFFFFFFFu) return 0;
  return BUFFERIZE_NODES[idx].realized
      || RU_REALIZE_MAP[idx].realized_full
      || RU_REALIZE_MAP[idx].realized_partial;
}

// 1 iff `t` holds a REDUCE that is not realized, reached without crossing a
// realized boundary (a buffer, as tinygrad's red_gate stops at AFTER and STAGE,
// rangeify.py:64-79).  Walks the raw heap graph, since an inlined broadcast is
// not in BUFFERIZE_NODES, through elementwise, movement and cast ops only.
static int rb_subtree_has_live_reduce(Term t, u32 depth) {
  if (depth > 48) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 0;
  u8 op = term_ext(r);
  if (op == UOP_KERNEL || op == UOP_BUFFER || op == UOP_BUFFERIZE) return 0;
  if (rb_node_realized(bufferize_info_find(term_val(r)))) return 0;  // buffer: stop
  if (op == UOP_REDUCE) return 1;
  int ew = uop_is_unary_elementwise(op)
        || uop_is_binary_elementwise(op)
        || uop_is_ternary_elementwise(op);
  if (!(ew || uop_is_movement(op) || op == UOP_CAST || op == UOP_BITCAST))
    return 0;
  u64 loc = term_val(r);
  u8 ar = uop_arity(op);
  for (u8 s = 0; s < ar; s++) {
    if (rb_subtree_has_live_reduce(heap_read(loc + s), depth + 1)) return 1;
  }
  return 0;
}

// The distinct buffers `t` reads, tinygrad's red_gate accessed_buffers
// (rangeify.py:61-82): a realized boundary, UOP_BUFFER, KERNEL, BUFFERIZE or
// leaf tensor (PARAM) counts and ends the descent.  Dedups into `locs`
// (capacity `cap`) and returns the running count.
static u32 rb_collect_accessed_buffers(Term t, u64 *locs, u32 n, u32 cap,
                                       u32 depth) {
  if (depth > 48 || n >= cap) return n;
  Term r = term_resolve(t);
  u8 tag = term_tag(r);
  if (tag == TAG_TEN || tag == TAG_VAR) {            // leaf input (PARAM)
    u64 l = term_val(r);
    for (u32 i = 0; i < n; i++) if (locs[i] == l) return n;
    locs[n++] = l;
    return n;
  }
  if (tag != TAG_UOP) return n;
  u8 op = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_BUFFER || op == UOP_KERNEL || op == UOP_BUFFERIZE
      || rb_node_realized(bufferize_info_find(loc))) {
    for (u32 i = 0; i < n; i++) if (locs[i] == loc) return n;  // realized: count, stop
    if (n < cap) locs[n++] = loc;
    return n;
  }
  if (op == UOP_CONST || op == UOP_RANGE) return n;
  u8 ar = uop_arity(op);
  for (u8 s = 0; s < ar; s++)
    n = rb_collect_accessed_buffers(heap_read(loc + s), locs, n, cap, depth + 1);
  return n;
}

// tinygrad keeps a buffer generated from more than 3 buffers (rangeify.py:85):
// recomputing it would re-read them all in every consumer kernel.
static int rb_too_many_buffers(u64 loc) {
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  u64 locs[8];
  u32 n = 0;
  u8 ar = uop_arity(BUFFERIZE_NODES[idx].op);
  for (u8 s = 0; s < ar; s++)
    n = rb_collect_accessed_buffers(heap_read(loc + s), locs, n, 8, 0);
  return n > 3;
}

// The element count of `t`'s shape, or 0 if its shape is unknown.
static u64 rb_term_numel(Term t) {
  Shape s;
  if (!term_shape_in(t, 0, &s)) return 0;
  u64 p = 1;
  for (u32 a = 0; a < s.ndim && a < MAX_DIM; a++) p *= (u64)(s.dims[a] ? s.dims[a] : 1);
  return p;
}

// rb_collect_accessed_buffers recording each buffer's Term, so the caller can
// sum their sizes.
static u32 rb_collect_accessed_buffer_terms(Term t, Term *terms, u32 n, u32 cap,
                                            u32 depth) {
  if (depth > 48 || n >= cap) return n;
  Term r = term_resolve(t);
  u8 tag = term_tag(r);
  if (tag == TAG_TEN || tag == TAG_VAR) {            // leaf input (PARAM)
    for (u32 i = 0; i < n; i++) if (terms[i] == r) return n;
    terms[n++] = r;
    return n;
  }
  if (tag != TAG_UOP) return n;
  u8 op = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_BUFFER || op == UOP_KERNEL || op == UOP_BUFFERIZE
      || rb_node_realized(bufferize_info_find(loc))) {
    for (u32 i = 0; i < n; i++) if (terms[i] == r) return n;  // realized: count, stop
    if (n < cap) terms[n++] = r;
    return n;
  }
  if (op == UOP_CONST || op == UOP_RANGE) return n;
  u8 ar = uop_arity(op);
  for (u8 s = 0; s < ar; s++)
    n = rb_collect_accessed_buffer_terms(heap_read(loc + s), terms, n, cap, depth + 1);
  return n;
}

// tinygrad's buffer_in_reduce (rangeify.py:87-96): a candidate whose value
// holds a live reduce is kept, since recomputing it would re-run the reduce.  A
// realized reduce (the attention scores, the softmax max and sum) is a buffer,
// so an elementwise node over it stays removable.
static int rb_src_has_live_reduce(u64 loc, u32 depth) {
  (void)depth;
  if (loc >= HEAP_NEXT) return 0;
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  u8 ar = uop_arity(BUFFERIZE_NODES[idx].op);
  for (u8 s = 0; s < ar; s++) {
    if (rb_subtree_has_live_reduce(heap_read(loc + s), 0)) return 1;
  }
  return 0;
}

// The number of entries of `rngs` that are a bare UOP_RANGE.
static u8 rb_count_range_axes(Term const *rngs, u8 ndim) {
  u8 n = 0;
  for (u8 a = 0; a < ndim; a++) {
    Term r = rngs[a];
    if (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE) n++;
  }
  return n;
}

// 1 if recomputing the node in its consumers is not exact.  The recompute
// rebinds producer out_rngs[a] to consumer in_rngs[a] positionally
// (ru_build_axis_subst), which holds only without a movement op between: an
// EXPAND reads the value over axes it does not depend on, a RESHAPE or PERMUTE
// re-indexes it.  tinygrad's src.substitute (rangeify.py:102) is exact.  BFS
// the non-realized elementwise consumers: any movement consumer, or one
// iterating more ranges than the producer, makes the region unsafe.
static int rb_unsafe_recompute_region(u32 node_idx) {
  u8 prod_rank = rb_count_range_axes(RU_RANGE_MAP[node_idx].out_rngs,
                                     RU_RANGE_MAP[node_idx].out_ndim);
  u32 queue[RU_CONSRED_BFS_CAP];
  u32 qhead = 0, qtail = 0;
  u32 visited[RU_CONSRED_BFS_CAP];
  u32 n_visited = 0;
  queue[qtail++] = node_idx;
  u32 budget = RU_CONSRED_BFS_CAP * 4;
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
      if (uop_is_movement(cop)) return 1;  // movement re-index: substitution unsafe
      if (rb_count_range_axes(RU_RANGE_MAP[ci].out_rngs,
                              RU_RANGE_MAP[ci].out_ndim) > prod_rank)
        return 1;                          // residual broadcast in the region
      if (cop == UOP_REDUCE) continue;     // reduce: lowers rank, hard boundary
      if (rb_node_realized(ci)) continue;  // realized: its own kernel, stop
      int is_ew = uop_is_unary_elementwise(cop)
               || uop_is_binary_elementwise(cop)
               || uop_is_ternary_elementwise(cop);
      if (is_ew && qtail < RU_CONSRED_BFS_CAP) queue[qtail++] = ci;
    }
  }
  return 0;
}

// 1 if the node feeds a matmul reduce through non-realized movement and
// elementwise hops.  thvm's GEMM paths (cpu/blas.c, the Metal and CUDA tile
// GEMMs) need materialized operands, so such a node is thvm's counterpart of a
// non-removable tinygrad buffer (indexing.py:90, rangeify.py:56): removing it
// would only force a re-realize for the GEMM.
static int rb_feeds_matmul_reduce(u32 node_idx) {
  u32 queue[RU_CONSRED_BFS_CAP];
  u32 qhead = 0, qtail = 0;
  u32 visited[RU_CONSRED_BFS_CAP];
  u32 n_visited = 0;
  queue[qtail++] = node_idx;
  u32 budget = RU_CONSRED_BFS_CAP * 4;
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
      if (cop == UOP_REDUCE) {
        if (BUFFERIZE_NODES[ci].reasons & BUFFERIZE_REASON_MATMUL) return 1;
        continue;                          // non-matmul reduce: hard boundary
      }
      if (rb_node_realized(ci)) continue;  // realized consumer: materializes, stop
      int is_movement = uop_is_movement(cop);
      int is_ew = uop_is_unary_elementwise(cop)
               || uop_is_binary_elementwise(cop)
               || uop_is_ternary_elementwise(cop);
      if ((is_movement || is_ew) && qtail < RU_CONSRED_BFS_CAP)
        queue[qtail++] = ci;
    }
  }
  return 0;
}

// THVM_FUSE_MATMUL_INPUT: 1 if the node reaches a 2-D matmul reduce through a
// chain where every hop has exactly one consumer, each a non-realized movement
// or elementwise op (tinygrad's single-consumer inline, indexing.py:242-244).
// The producer then inlines into exactly one matmul kernel, as its MUL operand.
static int rb_matmul_input_fusable(u32 node_idx) {
  u32 cur = node_idx;
  u32 budget = RU_CONSRED_BFS_CAP;
  while (budget-- > 0) {
    u32 cidxs[RU_MAX_CONSUMERS];
    u32 nc = ru_consumers_for_node(cur, cidxs, RU_MAX_CONSUMERS);
    if (nc != 1) return 0;                  // branch (or sink): not a clean chain
    u32 ci = cidxs[0];
    u8 cop = BUFFERIZE_NODES[ci].op;
    if (cop == UOP_REDUCE) {
      // Only a 2-D {M,N} matmul: the rank-grow remap in pm_apply_rangeify binds
      // M and K only, so a batched one (attention scores and context) keeps its
      // producer realized.
      if (!(BUFFERIZE_NODES[ci].reasons & BUFFERIZE_REASON_MATMUL)) return 0;
      return (RU_RANGE_MAP[ci].out_ndim == 2) ? 1 : 0;
    }
    // A realized hop materializes on its own: no clean inline.
    if (rb_node_realized(ci)) return 0;
    int is_movement = uop_is_movement(cop);
    int is_ew = uop_is_unary_elementwise(cop)
             || uop_is_binary_elementwise(cop)
             || uop_is_ternary_elementwise(cop);
    if (!(is_movement || is_ew)) return 0;  // CAST, STORE etc. end the chain
    cur = ci;
  }
  return 0;
}

// Peel CAST and movement views (a Transpose, a bf16->f32 widening cast, a
// reshape) off a matmul operand, down to its data leaf or first ALU op.
static Term rb_peel_operand_views(Term t, u32 depth) {
  if (depth > 32 || term_tag(t) != TAG_UOP) return t;
  u8 op = term_ext(t);
  if (op == UOP_CAST || op == UOP_BITCAST || uop_is_movement(op))
    return rb_peel_operand_views(heap_read(term_val(t) + 0), depth + 1);
  return t;
}

// 1 if a matmul operand is a stable buffer read: an input or weight BUFFER (or
// a leaf tensor or var) behind cast and movement views.  A computed operand is
// rejected even when realized: a later pass can still drop its realize and
// leave an ALU tree the tiled epilogue cannot stage.
static int rb_operand_is_clean_buffer(Term t) {
  Term leaf = rb_peel_operand_views(t, 0);
  if (term_tag(leaf) != TAG_UOP) return 1;        // TEN / leaf var: a stable buffer
  return term_ext(leaf) == UOP_BUFFER;
}

// 1 if the inlined matmul can stay on the tiled TC path
// (rmu_emit_matmul_tc_tiled): one reduce axis over a MUL whose operands are
// both clean buffer reads.  Otherwise it would drop to a scalar accumulator
// inside the consumer kernel, slower than its own tiled kernel.  The scheduler
// side of rmu_detect_matmul_tc's operand gate.
static int rb_matmul_epilogue_tc_emittable(u32 node_idx) {
  if (BUFFERIZE_NODES[node_idx].op != UOP_REDUCE) return 0;
  Term reduce = term_new(0, TAG_UOP, UOP_REDUCE, BUFFERIZE_NODES[node_idx].loc);
  if (uop_reduce_n_axes(reduce) != 1) return 0;     // single K axis
  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  if (!rb_operand_is_clean_buffer(heap_read(term_val(mul) + 0))) return 0;
  if (!rb_operand_is_clean_buffer(heap_read(term_val(mul) + 1))) return 0;
  return 1;
}

// THVM_FUSE_MATMUL_EPILOGUE: 1 if the node is a matmul reduce the tiled emitter
// keeps (rb_matmul_epilogue_tc_emittable), and it and every hop below the
// realized sink have exactly one consumer, each an elementwise or cast op: no
// branch, no movement, no second reduce.  The fused store then holds
// ELEMENTWISE(...REDUCE...) over the matmul's own (M, N) ranges, which the
// tiled emitter replaces with its tile's (m, n).
static int rb_matmul_epilogue_fusable(u32 node_idx) {
  if (!(BUFFERIZE_NODES[node_idx].reasons & BUFFERIZE_REASON_MATMUL)) return 0;
  if (BUFFERIZE_NODES[node_idx].op != UOP_REDUCE) return 0;
  if (!rb_matmul_epilogue_tc_emittable(node_idx)) return 0;
  u32 cur = node_idx;
  u32 budget = RU_CONSRED_BFS_CAP;
  while (budget-- > 0) {
    u32 cidxs[RU_MAX_CONSUMERS];
    u32 nc = ru_consumers_for_node(cur, cidxs, RU_MAX_CONSUMERS);
    if (nc != 1) return 0;                  // branch (or sink): not a clean chain
    u32 ci = cidxs[0];
    u8 cop = BUFFERIZE_NODES[ci].op;
    // A second reduce leaves no epilogue (rmu_epilogue_walk rejects a nested
    // REDUCE).
    if (cop == UOP_REDUCE) return 0;
    // A movement op re-indexes the value; rmu_epilogue_walk takes only
    // elementwise and cast hops.
    if (uop_is_movement(cop)) return 0;
    int is_ew = uop_is_unary_elementwise(cop)
             || uop_is_binary_elementwise(cop)
             || uop_is_ternary_elementwise(cop)
             || cop == UOP_CAST || cop == UOP_BITCAST;
    if (!is_ew) return 0;                    // STORE/etc: not an elementwise epilogue
    // A realized elementwise hop is the fused store target.
    if (rb_node_realized(ci)) return 1;
    cur = ci;
  }
  return 0;
}

// The distinct input buffers of the kernel rooted at `node_idx`, as tinygrad's
// _limit_bufs visitor counts them (rangeify.py:181-185):
// rb_collect_accessed_buffers over the root's srcs.  The count saturates at
// RU_LIMIT_BUFS_CAP.
#define RU_LIMIT_BUFS_CAP 64
static u32 ru_kernel_input_bufs(u32 node_idx) {
  u64 locs[RU_LIMIT_BUFS_CAP];
  u32 n = 0;
  u64 loc = BUFFERIZE_NODES[node_idx].loc;
  u8  ar  = uop_arity(BUFFERIZE_NODES[node_idx].op);
  for (u8 s = 0; s < ar; s++)
    n = rb_collect_accessed_buffers(heap_read(loc + s), locs,
                                    n, RU_LIMIT_BUFS_CAP, 0);
  return n;
}

// 1 if `src` is a broadcast expander: its numel is at least
// RU_BROADCAST_EXPAND_RATIO times the summed numel of the distinct buffers it
// reads (the attention MUL EXPAND(reshape(a,{B,M,K,1}),{B,M,K,N})).  Realizing
// one materializes a buffer far larger than its inputs, so limit_bufs splits
// elsewhere first.
#define RU_BROADCAST_EXPAND_RATIO 4u
static int ru_src_is_broadcast_expander(Term src) {
  u64 out_numel = rb_term_numel(src);
  if (out_numel == 0) return 0;                  // unknown shape: don't special-case
  Term terms[RU_LIMIT_BUFS_CAP];
  u32 n = 0;
  Term r = term_resolve(src);
  if (term_tag(r) == TAG_UOP) {
    u8  op  = term_ext(r);
    u64 loc = term_val(r);
    u8  ar  = uop_arity(op);
    for (u8 s = 0; s < ar; s++)
      n = rb_collect_accessed_buffer_terms(heap_read(loc + s), terms,
                                           n, RU_LIMIT_BUFS_CAP, 0);
  }
  u64 in_sum = 0;
  for (u32 i = 0; i < n; i++) in_sum += rb_term_numel(terms[i]);
  if (in_sum == 0) return 0;                     // no measurable input: not a clear expander
  return out_numel >= (u64)RU_BROADCAST_EXPAND_RATIO * in_sum;
}

// tinygrad's _limit_bufs (rangeify.py:176-198): a root reading more than
// MAX_BUFS - 1 inputs (one slot is the output) bufferizes its elementwise srcs.
// Here that is a pinned realize mark set during the walk, before the walk
// reaches the src, so the src takes the realized branch like any boundary (a
// mark after the walk would leave its children on inherited ranges).  Runs for
// unary and CAST roots too (tinygrad matches Binary|Ternary).  tinygrad splits
// from the leaves up (graph_rewrite visits srcs first, uop/ops.py:1840-1843);
// this walk starts at the root, so a left-deep chain splits into more kernels.
// Both stay within the cap.
static void ru_limit_bufs_node(u32 root_idx) {
  u32 max_bufs = ru_device_max_bufs();
  if (max_bufs < 2) return;                  // 0 == no limit; <2 underflow-guard
  u32 input_cap = max_bufs - 1;              // -1 reserves the output buffer

  u8 rop = BUFFERIZE_NODES[root_idx].op;
  int root_ew = uop_is_unary_elementwise(rop)
             || uop_is_binary_elementwise(rop)
             || uop_is_ternary_elementwise(rop)
             || rop == UOP_CAST || rop == UOP_BITCAST;
  if (!root_ew) return;

  if (ru_kernel_input_bufs(root_idx) <= input_cap) return;

  // Pass 0 skips broadcast expanders; pass 1 takes them only if the kernel is
  // still over budget, since a split is mandatory under a hard cap.
  u64 rloc = BUFFERIZE_NODES[root_idx].loc;
  u8  rar  = uop_arity(rop);
  for (u32 pass = 0; pass < 2; pass++) {
    if (ru_kernel_input_bufs(root_idx) <= input_cap) return;
    for (u8 s = 0; s < rar; s++) {
      Term src = term_resolve(heap_read(rloc + s));
      if (term_tag(src) != TAG_UOP) continue;            // leaf: not splittable
      u8 sop = term_ext(src);
      int s_ew = uop_is_unary_elementwise(sop)
              || uop_is_binary_elementwise(sop)
              || uop_is_ternary_elementwise(sop)
              || sop == UOP_CAST || sop == UOP_BITCAST;
      if (!s_ew) continue;
      if (pass == 0 && ru_src_is_broadcast_expander(src)) continue;
      u32 sidx = bufferize_info_find(term_val(src));
      if (sidx == 0xFFFFFFFFu) continue;
      if (rb_node_realized(sidx)) continue;
      // tinygrad's s.device check always holds: a realize runs on one device.
      RU_REALIZE_MAP[sidx].realized_full    = 1;
      RU_REALIZE_MAP[sidx].realized_partial = 0;
      RU_REALIZE_MAP[sidx].axes_mask        = 0;
      RU_REALIZE_MAP[sidx].n_realized_axes  = MAX_DIM;
      RU_REALIZE_MAP[sidx].pinned           = 1;  // survive remove_bufferize
    }
  }
}

// run_rangeify (indexing.py:189-320) over BUFFERIZE_NODES, then
// pm_apply_rangeify.  Requires bufferize_classify(root): BUFFERIZE_NODES,
// CMAP_LL and the realized bits.  Fills RU_RANGE_MAP, RU_REALIZE_MAP,
// RU_ENDING_RANGES and RU_REDUCE_RANGES; the realize decision lives in
// RU_REALIZE_MAP, apart from BUFFERIZE_NODES[].realized.
fn void run_rangeify_unified(Term root) {
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
    RU_REALIZE_MAP[i].pinned           = 0;
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

  // indexing.py:195, the realize map: the bufferize_classify realize marks that
  // ru_seed_boundary_holds keeps, the root standing in for tinygrad's STORE.
  // tinygrad's pm_generate_realize_map (indexing.py:45-54) also realizes
  // MSELECT/MSTACK srcs, custom-kernel inputs and a self-reading STORE's src;
  // prepare already turned a bare STAGE or COPY into a STORE
  // (prepare.py:220-230).  The MULTI, MATMUL and FANIN_CAP reasons alone do not
  // seed: the walk derives those realizes, as tinygrad does
  // (indexing.py:245-276).
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (!BUFFERIZE_NODES[i].realized) continue;
    if (!ru_seed_boundary_holds(BUFFERIZE_NODES[i].reasons)) continue;
    RU_REALIZE_MAP[i].realized_full = 1;
    RU_REALIZE_MAP[i].n_realized_axes = MAX_DIM;
  }

  // indexing.py:207: consumers before producers (RU_TOPO_ORDER).
  ru_compute_topo_order();
  for (u32 oi = 0; oi < RU_TOPO_ORDER_LEN; oi++) {
    u32 node_idx = RU_TOPO_ORDER[oi];
    UOpInfo *info = &BUFFERIZE_NODES[node_idx];
    if (ru_is_skip_op(info->op)) continue;
    RU_LAST_NODES_WALKED++;

    Shape shape;
    if (!ru_node_shape(info->loc, info->op, &shape)) {
      // No shape, no ranges.
      continue;
    }
    u8 my_ndim = (u8)shape.ndim;
    if (my_ndim > RU_MAX_AXES) my_ndim = RU_MAX_AXES;

    // indexing.py:217: the consumers' ending ranges.
    {
      u32 cidxs[RU_MAX_CONSUMERS];
      u32 nc = ru_consumers_for_node(node_idx, cidxs, RU_MAX_CONSUMERS);
      for (u32 c = 0; c < nc; c++) {
        ru_ending_append(&RU_ENDING_RANGES[node_idx],
                         &RU_ENDING_RANGES[cidxs[c]]);
      }
    }

    // consumer_rngs (indexing.py:230).
    RuConsumerRangs consumer_rngs[RU_MAX_CONSUMERS];
    u32 n_crngs = ru_gather_consumer_rngs(node_idx, my_ndim,
                                          consumer_rngs, RU_MAX_CONSUMERS);

    Term out_rngs[RU_MAX_AXES] = {0};

    if (RU_REALIZE_MAP[node_idx].realized_full
        || RU_REALIZE_MAP[node_idx].realized_partial) {
      // indexing.py:231-238: a realized node gets fresh output ranges, and
      // every range ends here.
      for (u8 a = 0; a < my_ndim; a++) {
        out_rngs[a] = ru_new_range(shape.dims[a], KAX_LOOP);
      }
      RU_REALIZE_MAP[node_idx].realized_full = 1;
      RU_REALIZE_MAP[node_idx].realized_partial = 0;
      RU_REALIZE_MAP[node_idx].axes_mask = (my_ndim < 8) ? ((1u << my_ndim) - 1u) : 0xFFu;
      RU_REALIZE_MAP[node_idx].n_realized_axes = my_ndim;
      RU_ENDING_RANGES[node_idx].n = 0;
    } else if (n_crngs == 0) {
      // indexing.py:239-241: no consumer has ranges, so neither does this node.
      continue;
    } else if (n_crngs == 1) {
      // indexing.py:242-244: a single consumer's ranges are inherited.
      for (u8 a = 0; a < my_ndim; a++) {
        out_rngs[a] = consumer_rngs[0].rngs[a];
      }
    } else {
      // indexing.py:245-268: two or more consumers.  tinygrad shares the ranges
      // when every axis agrees and otherwise realizes every axis; thvm realizes
      // only the diverging axes (a partial realize), with the exceptions below.
      u8 partial_mask = 0;
      u8 n_realized = 0;
      int all_all_same = 1;
      for (u8 a = 0; a < my_ndim; a++) {
        if (!ru_all_same_axis(consumer_rngs, n_crngs, a)) {
          all_all_same = 0; break;
        }
      }
      // A broadcast EXPAND is shared as a view instead: its diverging consumer
      // ranges are the reduce axes of the consuming REDUCEs, and tinygrad fuses
      // it too (realize, then remove_bufferize).  Every consumer rebinds the
      // shared ranges (ru_expand_shared_rngs).  Any other view of a computed
      // source whose consumers map one producer axis to different axes
      // (q.reshape(M,N,1) vs q.reshape(M,1,N); Gw vs Gw.T) cannot reuse
      // consumer 0's ranges, which would read consumer 1 with consumer 0's
      // stride: it realizes, and each consumer indexes the buffer at its own
      // ranges.
      if (!all_all_same && info->op == UOP_EXPAND) {
        ru_expand_shared_rngs(info->loc, consumer_rngs, n_crngs, &shape,
                              my_ndim, out_rngs);
      } else if (all_all_same) {
        // The OR of the valids (indexing.py:258-261) is consumer 0's range:
        // all_all_same compared the valids too.
        for (u8 a = 0; a < my_ndim; a++) {
          out_rngs[a] = consumer_rngs[0].rngs[a];
        }
      } else {
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
          // A realize over every axis is a full boundary.  So is a realized
          // index-remapping view (RESHAPE, PERMUTE, SHRINK, FLIP): a partial
          // realize over the diverging axis would leave that axis unbound in
          // the buffer and drop the SHRINK offset or permuted stride (c[:,0]
          // and c[:,1] would read the same element).  A REDUCE or elementwise
          // partial realize (a softmax denominator, a layer-norm mean) keeps
          // its fusion.  EXPAND is shared above; PAD keeps its value-side WHERE
          // guard and stays partial.
          int is_mv = (info->op == UOP_RESHAPE || info->op == UOP_PERMUTE
                    || info->op == UOP_SHRINK  || info->op == UOP_FLIP);
          if (n_realized == my_ndim || is_mv) {
            RU_REALIZE_MAP[node_idx].realized_full = 1;
            RU_REALIZE_MAP[node_idx].realized_partial = 0;
            RU_REALIZE_MAP[node_idx].axes_mask =
                (my_ndim < 8) ? (u8)((1u << my_ndim) - 1u) : 0xFFu;
            RU_REALIZE_MAP[node_idx].n_realized_axes = my_ndim;
            // Fresh ranges on the agreeing axes too, so the buffer is iterated
            // over its whole shape.
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

    // indexing.py:270-276: an elementwise op or REDUCE with ended ranges
    // realizes every output axis.  A GATHER counts as elementwise: tinygrad's
    // is a one-hot REDUCE (mixin/op.py:1041), so a gather feeding a matmul
    // realizes and the matmul reads a buffer.
    int is_elementwise = uop_is_unary_elementwise(info->op)
                      || uop_is_binary_elementwise(info->op)
                      || uop_is_ternary_elementwise(info->op)
                      || info->op == UOP_GATHER;
    if (RU_ENDING_RANGES[node_idx].n
        && (is_elementwise || info->op == UOP_REDUCE)) {
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

    // tinygrad's _limit_bufs: may pin this node's elementwise srcs realized
    // before the walk reaches them (ru_limit_bufs_node).
    ru_limit_bufs_node(node_idx);

    // indexing.py:283-301, the input ranges: swizzled by a movement op, fresh
    // on a REDUCE's reduced axes, passed through otherwise.
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
        // The swizzle declined: pass the ranges through.
        for (u8 a = 0; a < my_ndim; a++) in_rngs[a] = out_rngs[a];
      }

      // indexing.py:294-297: an EXPAND ends the ranges of its broadcast axes.
      if (info->op == UOP_EXPAND) {
        Shape src_shape;
        if (ru_node_src_shape(info->loc, &src_shape)) {
          for (u8 a = 0; a < my_ndim; a++) {
            u32 in_dim  = (a < src_shape.ndim) ? src_shape.dims[a] : 1;
            u32 out_dim = (u32)term_val(heap_read(info->loc + 2 + a));
            if (in_dim != out_dim) {
              // Every RANGE in out_rngs[a] ends here.
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
      // indexing.py:300-301: a fresh KAX_REDUCE range on each reduced axis, the
      // output ranges in order on the others (tinygrad's reduced axes are the
      // source's leading arg[1] axes; thvm's stay in place).  RU_REDUCE_RANGES
      // keeps the fresh ranges, tinygrad's REDUCE src[1:] (indexing.py:113).
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
            // Only true RANGEs: an extent-1 axis is CONST(0) (ru_new_range).
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
      for (u8 a = 0; a < my_ndim; a++) in_rngs[a] = out_rngs[a];
    }

    RU_RANGE_MAP[node_idx].out_ndim   = my_ndim;
    RU_RANGE_MAP[node_idx].in_ndim    = (u8)in_ndim;
    RU_RANGE_MAP[node_idx].has_ranges = 1;
    for (u8 a = 0; a < my_ndim; a++) RU_RANGE_MAP[node_idx].out_rngs[a] = out_rngs[a];
    for (u8 a = 0; a < in_ndim; a++) RU_RANGE_MAP[node_idx].in_rngs [a] = in_rngs [a];
  }

  // remove_bufferize (rangeify.py:50-102): un-realize a walk-realized
  // elementwise boundary so each consumer recomputes it instead of reading a
  // shared buffer; pm_apply_rangeify re-indexes it per consumer
  // (ru_build_axis_subst and ru_subtree_rewrite_ranges, tinygrad's
  // src.substitute at rangeify.py:102).  Besides tinygrad's gates (more than 3
  // buffers, rangeify.py:85; a live reduce, rangeify.py:87-96), thvm keeps GEMM
  // operands (rb_feeds_matmul_reduce) and movement regions
  // (rb_unsafe_recompute_region).
  for (u32 oi = 0; oi < RU_TOPO_ORDER_LEN; oi++) {
    u32 i = RU_TOPO_ORDER[oi];
    if (!(RU_REALIZE_MAP[i].realized_full || RU_REALIZE_MAP[i].realized_partial))
      continue;
    // A limit_bufs split: un-realizing it would re-fuse a kernel over the
    // device buffer cap.
    if (RU_REALIZE_MAP[i].pinned) continue;
    // THVM_FUSE_MATMUL_EPILOGUE: un-realize an OPT_TC matmul reduce whose only
    // consumer is an elementwise chain (rb_matmul_epilogue_fusable).  A matmul
    // is a bufferize_classify boundary, so this runs before the keep below.
    // Metal only: the bf16 tiled emitter alone emits the epilogue (render_uop.c
    // checks c_is_bf); elsewhere the matmul stays a realized BLAS operand.
    if (ru_fuse_epilogue_on() && ru_fuse_matmul_input_target_ok()
        && rb_matmul_epilogue_fusable(i)
        && !rb_src_has_live_reduce(BUFFERIZE_NODES[i].loc, 0)) {
      RU_REALIZE_MAP[i].realized_full    = 0;
      RU_REALIZE_MAP[i].realized_partial = 0;
      continue;
    }
    if (BUFFERIZE_NODES[i].realized) continue;          // a classify boundary
    u8 op = BUFFERIZE_NODES[i].op;
    int ew = uop_is_unary_elementwise(op)
          || uop_is_binary_elementwise(op)
          || uop_is_ternary_elementwise(op);
    if (!ew) continue;                                  // only removable elementwise
    // THVM_FUSE_MATMUL_INPUT: a single-consumer elementwise producer of one
    // matmul is un-realized and spliced into its A operand (pm_apply_rangeify's
    // rank-grow remap binds its leaked ranges).  Any other GEMM operand stays
    // realized.
    int fuse_mm = (ru_fuse_matmul_input_on() && ru_fuse_matmul_input_target_ok()
                   && rb_feeds_matmul_reduce(i)
                   && rb_matmul_input_fusable(i));
    if (rb_feeds_matmul_reduce(i) && !fuse_mm)
      continue;                                         // removable=False (GEMM operand)
    // The input fuse skips the next two gates: the matmul lowering's
    // reshape(+unit axis) + expand(over N) is exact here (A's load never reads
    // N), and its producers (a modulation) often read more than 3 buffers.
    // rmu_detect_matmul_tc re-checks the operand and can fall back to the
    // generic accumulator.
    if (!fuse_mm) {
      if (rb_unsafe_recompute_region(i)) continue;      // movement/broadcast re-index unsafe
      // More than 3 buffers (rangeify.py:85).
      if (rb_too_many_buffers(BUFFERIZE_NODES[i].loc))
        continue;
    }
    // A live reduce in the value (rangeify.py:87-96).
    if (rb_src_has_live_reduce(BUFFERIZE_NODES[i].loc, 0)) {
      continue;
    }
    RU_REALIZE_MAP[i].realized_full    = 0;
    RU_REALIZE_MAP[i].realized_partial = 0;
  }

  // indexing.py:317: graph_rewrite(pm_apply_rangeify).
  pm_apply_rangeify(root);
}

// pm_apply_rangeify's tables, indexed by node.  RU_SUBST is what a consumer
// splices in for the node: its rewritten value, or at a realize boundary the
// UOP_BUFFERIZE (RU_BUFFERIZE_TERM) that the consumer indexes at its own
// ranges.

#define RU_SUBST_CAP RU_MAX_NODES
static Term RU_SUBST[RU_SUBST_CAP];
// A realized node's UOP_BUFFERIZE (tinygrad's STAGE, indexing.py:92), or 0.
static Term RU_BUFFERIZE_TERM[RU_SUBST_CAP];
// The realized node's kernel root for kernel_lift.c
// (rangeify_unified_store_root_at): UOP_STORE(buffer of the node's shape,
// output address, store value), or a SCATTER's AFTER chain of stores.  0 when
// there is no boundary or no dtype.
static Term RU_STORE_ROOT[RU_SUBST_CAP];

// The per-axis ranges of each INDEX_E pm_apply_rangeify builds, keyed by its
// heap loc.  tinygrad's INDEX keeps them as srcs (indexing.py:93); thvm's
// INDEX_E has one flat address, and materialize.c's BUFFERIZE inline needs the
// per-axis ranges to substitute closed ranges without re-decomposing that
// address (which fails on IDIV/IMOD swizzles).  Linear scan, capped.
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
  // A hash-consed INDEX_E can be built more than once: the first entry wins.
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

// For materialize.c, which re-registers an INDEX_E it rebuilds (a BUFFERIZE
// promoted to its BUFFER) so the entry survives.
fn void rangeify_unified_index_axes_register(Term index_e_term,
                                             Term const *rngs, u8 ndim) {
  ru_index_axes_register(index_e_term, rngs, ndim);
}

// The node's realized-axes mask (bit i: axis i is closed in its BUFFERIZE).
fn u8 rangeify_unified_axes_mask_at(u32 node_idx) {
  if (node_idx >= RU_MAX_NODES) return 0;
  return RU_REALIZE_MAP[node_idx].axes_mask;
}

// 1 iff the node is fully realized.  materialize.c pairs it with
// rangeify_unified_axes_mask_at and rangeify_unified_out_rng_at to map closed
// ranges to output axes, as ru_collect_closed_ranges does.
fn int rangeify_unified_realized_full_at(u32 node_idx) {
  if (node_idx >= RU_MAX_NODES) return 0;
  return RU_REALIZE_MAP[node_idx].realized_full ? 1 : 0;
}

// The node whose RU_BUFFERIZE_TERM is `buf`, or 0xFFFFFFFFu.  materialize.c
// uses it for a BUFFERIZE whose node did not become a boundary kernel (one not
// in BOUNDARY_BUFFERIZE_TERM).
fn u32 rangeify_unified_node_idx_for_bufferize(Term buf) {
  if (buf == 0) return 0xFFFFFFFFu;
  for (u32 i = 0; i < RU_SUBST_CAP; i++) {
    if (RU_BUFFERIZE_TERM[i] == buf) return i;
  }
  return 0xFFFFFFFFu;
}

// The per-axis ranges registered for the INDEX_E at `index_loc`: the number
// written to out_rngs, 0 if none.
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

// Per tid: 1 when an address here composes the tensor's views itself
// (ru_compose_view_chain, ru_ten_leaf_addr).  materialize.c's input_slot_dedup
// copies it to ke->input_chain_composed, so the backend binds the raw buffer
// instead of first gathering the view, which would apply the views twice.
static u8 RU_TID_CHAIN_COMPOSED[TENS_CAP];

fn int rangeify_unified_tid_chain_composed(u32 tid) {
  if (tid == 0 || tid >= TENS_CAP) return 0;
  return RU_TID_CHAIN_COMPOSED[tid] != 0;
}


// The node's UOP_BUFFERIZE, 0 if it is not a boundary.
fn Term rangeify_unified_bufferize_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_BUFFERIZE_TERM[node_idx];
}

// The node's RU_STORE_ROOT.
fn Term rangeify_unified_store_root_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_STORE_ROOT[node_idx];
}

// The row-major address of per-axis index expressions `rngs` (bare RANGEs or
// IDIV/IMOD swizzles) over `dims`.  With dims NULL each extent is read off a
// bare RANGE, and any other entry counts as extent 1.
static Term ru_build_addr_with_dims(Term const *rngs, u8 ndim,
                                     u32 const *dims) {
  if (ndim == 0) return uop_const(DT_INT32, 0);
  u32 local_dims[RU_MAX_AXES] = {0};
  if (dims == NULL) {
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
    // A symbolic (kvar) dim strides by its upper bound, as shape_numel and
    // view_create lay buffers out; the raw packed extent is not a size.
    strides[a] = strides[a + 1] * kvar_extent_static(dims[a + 1]);
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

// The node's output address, where its store writes: out_rngs over its own
// shape, or over extents read off the ranges when the shape is unknown.
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

// The address the node reads its inputs at: in_rngs (with any swizzle or REDUCE
// range) over the shape of its slot-0 source, whose dims give the strides a
// compound entry cannot.  RU_NO_LOC skips the shape lookup.
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

// A TAG_TEN with prior views (td->nviews > 0) is read at a row-major flat index
// of its public view; composing prior_views[nviews-1] .. [0] into that address
// (coord[d] = (flat / suffix[d]) % dims[d], times strides[d], plus the offset)
// reads the raw buffer directly.  1 if that applies: a contiguous offset-0
// public view, non-negative strides and offsets, no packed dtype.  Otherwise
// the backend gathers the view at dispatch.
static int ru_chain_foldable(u32 tid) {
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  TenDesc const *td = &TENS[tid];
  if (td->nviews == 0 || td->prior_views == NULL) return 0;
  if (dtype_is_packed(td->dtype)) return 0;
  // The flat address indexes prior_views[nviews-1] only from a contiguous,
  // offset-0 public view.
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

// One view's decompose-by-shape of flat index `cur`, with no modulus on the
// leading axis; a broadcast axis (stride 0) adds nothing:
//   offset + sum over d of ((cur / suffix[d]) % dims[d]) * strides[d]
static Term ru_compose_one_view(Term cur, View const *pv) {
  u32 ndim = pv->shape.ndim;
  if (ndim == 0) {
    return pv->offset != 0 ? uop_const(DT_INT32, pv->offset) : uop_const(DT_INT32, 0);
  }
  u32 suffix[MAX_DIM];
  suffix[ndim - 1] = 1;
  for (i32 d = (i32)ndim - 2; d >= 0; d--) {
    // A symbolic dim counts its upper bound (the worst-case layout); the raw
    // packed extent would make every coordinate 0.
    suffix[d] = suffix[d + 1] * kvar_extent_static(pv->shape.dims[d + 1]);
  }
  Term acc = pv->offset != 0 ? uop_const(DT_INT32, pv->offset) : 0;
  for (u32 d = 0; d < ndim; d++) {
    if (pv->strides[d] == 0) continue;          // broadcast: no contribution
    Term coord = cur;
    if (suffix[d] != 1) {
      coord = uop_int_binary(UOP_IDIV, coord, uop_const(DT_INT32, suffix[d]));
    }
    if (d != 0) {
      // The upper bound again, as in the suffix.
      coord = uop_int_binary(UOP_IMOD, coord,
                             uop_const(DT_INT32, kvar_extent_static(pv->shape.dims[d])));
    }
    if (pv->strides[d] != 1) {
      coord = uop_int_binary(UOP_IMUL, coord, uop_const(DT_INT32, pv->strides[d]));
    }
    acc = (acc == 0) ? coord : uop_int_binary(UOP_IADD, acc, coord);
  }
  return acc != 0 ? acc : uop_const(DT_INT32, 0);
}

// 1 if the view maps a flat index to itself: contiguous and offset 0, so each
// stride equals its suffix and the leading axis has no modulus.  Symbolic dims
// are excluded, so the identity never rests on the upper-bound layout.
static int ru_view_composes_as_identity(View const *pv) {
  if (!pv->contiguous || pv->offset != 0) return 0;
  for (u32 d = 0; d < pv->shape.ndim; d++) {
    if (kvar_extent_is_var(pv->shape.dims[d])) return 0;
  }
  return 1;
}

// Compose the tid's prior views into `addr0`, skipping the identity ones, and
// flag the tid in RU_TID_CHAIN_COMPOSED; addr0 is returned unchanged when the
// chain does not fold (ru_chain_foldable).  TENS[tid] is shared, so the views
// are never merged in place.
static Term ru_compose_view_chain(Term addr0, u32 tid) {
  if (addr0 == 0) return addr0;
  if (!ru_chain_foldable(tid)) return addr0;
  TenDesc const *td = &TENS[tid];
  Term cur = addr0;
  for (i32 k = (i32)td->nviews - 1; k >= 0; k--) {
    if (ru_view_composes_as_identity(&td->prior_views[k])) continue;
    cur = ru_compose_one_view(cur, &td->prior_views[k]);
    if (cur == 0) return addr0;
  }
  RU_TID_CHAIN_COMPOSED[tid] = 1;
  return cur;
}

// The UOP_BUFFERIZEs the last pm_apply_rangeify emitted; read by tests
// (tests/tu_probes.h).
static u32 RU_LAST_BUFFERIZES_EMITTED;

// The closed ranges of a realize boundary: out_rngs at its realized axes, every
// axis when full.  tinygrad closes every output range (indexing.py:84); a
// CONST(0) entry (an extent-1 axis) is dropped, as tinygrad's END drops it
// (indexing.py:87).  Returns the count, at most cap.
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

// A producer -> consumer range substitution for splicing a non-realized
// producer's value into a consumer.  The producer's out_rngs need not be the
// consumer's in_rngs (an un-realized boundary minted fresh ranges, a shared
// EXPAND kept consumer 0's), and the consumer kernel iterates only its own.
// Room for one pair per axis plus the leaves a compound entry binds.
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

// The free RANGE leaves of a subtree: those no enclosing UOP_REDUCE binds, so
// the caller's scope must iterate them.  Each set keeps at most
// RU_AXIS_SUBST_CAP entries and drops the rest.
typedef struct {
  u32 bound_aids[RU_AXIS_SUBST_CAP];
  u8  n_bound;
  u32 free_aids[RU_AXIS_SUBST_CAP];
  u8  free_types[RU_AXIS_SUBST_CAP];  // axis_type of each free leaf
  u8  n_free;
} RuFreeAxisSet;

// 1 iff `t` contains the RANGE with axis_id `aid`, without entering BUFFER,
// BUFFERIZE or KERNEL.  A leaf inside a compound out_rngs entry (a RESHAPE
// swizzle such as aid_hi*k + aid_lo) counts: the consumer iterates it and
// substitutes the whole entry on splice.
static int ru_expr_references_aid(Term t, u32 aid) {
  return ru_find_axis_range(t, aid, 1) != 0;
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

// Add the free RANGE leaves of `t` to `s`, not entering BUFFER, BUFFERIZE or
// KERNEL (their ranges are scoped separately).
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
    // A REDUCE binds all its axes in its body, as tinygrad's ended_ranges is
    // its whole src[1:] (uop/ops.py:58, 470-480).
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

// Append the axis ids of node ci's reduce ranges to out_aids, deduplicated.
static void ru_collect_reduce_aids_into(u32 ci, u32 *out_aids, u8 *n_out, u8 cap) {
  for (u8 k = 0; k < RU_REDUCE_RANGES[ci].n; k++) {
    u32 aid = uop_range_axis_id(RU_REDUCE_RANGES[ci].ranges[k]);
    u8 dup = 0;
    for (u8 j = 0; j < *n_out; j++) if (out_aids[j] == aid) { dup = 1; break; }
    if (!dup && *n_out < cap) out_aids[(*n_out)++] = aid;
  }
}

#define RU_CONSRED_AIDS_CAP 64
// The reduce-range axis ids of every REDUCE reachable from `node_idx` through
// movement and elementwise consumers.  The strand check counts a free axis they
// bind as covered: the conv-backward contraction product fans out into several
// scatter chains (the data-grad over cout, the weight-grad over b/h/w), each
// ending in its own REDUCE that binds the product's window axes, so all of them
// must be visited.  A realized REDUCE ends the walk; a fused one does not,
// since bufferize_classify leaves a contraction that feeds a scatter reduce
// unseeded and the combined kernel binds both scopes.
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
        ru_collect_reduce_aids_into(ci, out_aids, n_out, cap);
        int red_realized = RU_REALIZE_MAP[ci].realized_full
                        || RU_REALIZE_MAP[ci].realized_partial;
        if (!red_realized && qtail < RU_CONSRED_BFS_CAP) queue[qtail++] = ci;
        continue;
      }
      int is_movement = (cop == UOP_RESHAPE || cop == UOP_PERMUTE
                      || cop == UOP_EXPAND  || cop == UOP_SHRINK
                      || cop == UOP_PAD     || cop == UOP_FLIP);
      int is_ew = uop_is_unary_elementwise(cop)
               || uop_is_binary_elementwise(cop)
               || uop_is_ternary_elementwise(cop);
      if ((is_movement || is_ew) && qtail < RU_CONSRED_BFS_CAP) queue[qtail++] = ci;
    }
  }
}

// Rebuild `t` with each RANGE leaf whose axis_id `m` maps replaced, not
// entering BUFFERIZE, BUFFER or KERNEL: tinygrad's src.substitute in
// remove_bufferize (rangeify.py:102).
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
  // Carry the INDEX_E's per-axis ranges over to the rebuilt one, substituted,
  // so materialize.c's axis-table inline still finds them.
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

// Map from_aid to to_term unless it is already mapped (the first binding wins).
static void ru_axis_subst_add(RuAxisSubst *m, u32 from_aid, Term to_term) {
  for (u8 k = 0; k < m->n; k++) if (m->from_aid[k] == from_aid) return;
  if (m->n < RU_AXIS_SUBST_CAP) {
    m->from_aid[m->n] = from_aid;
    m->to_term [m->n] = to_term;
    m->n++;
  }
}

// Walk producer out_rng `pr` and consumer in_rng `cr` in parallel and map each
// producer RANGE leaf to the aligned consumer subterm.  A RESHAPE swizzle
// builds the same affine tree (low + high*k) on both sides; where the ops
// differ the walk stops, which only means no extra binding.
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

// The substitution for the edge where `consumer_rm` reads producer `pidx`: each
// producer out_rngs[a] maps to the consumer's in_rngs[a], a bare RANGE directly
// and a compound swizzle leaf by leaf (ru_axis_subst_match_pair).  A leaf left
// unbound would leak into the consumer as a free loop range.
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
    } else {
      ru_axis_subst_match_pair(&m, pr, cr, 0);   // compound out_rng leaf-match
    }
  }
  return m;
}

// The child as consumer `consumer_rm` reads it at `in_addr` (tinygrad's
// create_bufferize_and_index_srcs, indexing.py:73-99):
//   - a realized producer (RU_SUBST a BUFFERIZE): INDEX_E(BUFFERIZE, in_addr),
//     read at the consumer's ranges (indexing.py:93);
//   - a non-realized producer: its rewritten value, its ranges rebound to the
//     consumer's (ru_build_axis_subst);
//   - a leaf tensor or var: INDEX_E(leaf, in_addr);
//   - anything else is kept.
// ru_rewrite_subtree applies it to every slot of a node.
static Term ru_rewrite_child(Term old_child, Term in_addr,
                             RuRangeMap const *consumer_rm) {
  Term new_child = old_child;
  Term resolved  = term_resolve(old_child);
  u8 ctag = term_tag(resolved);
  if (ctag == TAG_UOP) {
    u32 cidx = bufferize_info_find(term_val(resolved));
    if (cidx != 0xFFFFFFFFu && RU_SUBST[cidx] != 0) {
      Term sub = RU_SUBST[cidx];
      if (term_tag(sub) == TAG_UOP && term_ext(sub) == UOP_BUFFERIZE) {
        new_child = uop_index_e(sub, in_addr);
        // Keep the per-axis ranges for materialize.c's axis-table inline.
        if (consumer_rm != NULL) {
          ru_index_axes_register(new_child, consumer_rm->in_rngs,
                                 consumer_rm->in_ndim);
        }
      } else {
        if (consumer_rm != NULL) {
          RuAxisSubst m = ru_build_axis_subst(cidx, consumer_rm);
          new_child = ru_subtree_rewrite_ranges(sub, &m, 0);
        } else {
          new_child = sub;
        }
      }
    }
  } else if (ctag == TAG_TEN || ctag == TAG_VAR) {
    // A TAG_VAR is a symbolic input slot like a TAG_TEN: visit() binds it via
    // input_slot_dedup_var, and materialize.c's unified_rewrite_buffer_for_var
    // swaps in its UOP_BUFFER.  A TAG_TEN that already carries prior views (a
    // chained intermediate reused across realizes) composes them into the
    // address here; chains visit()'s view_resolve mints later are flagged by
    // materialize.c's unified_fold_chain_scan.
    Term addr = in_addr;
    if (ctag == TAG_TEN) {
      u32 tid = (u32)term_val(resolved);
      addr = ru_compose_view_chain(in_addr, tid);
    }
    new_child = uop_index_e(resolved, addr);
  }
  return new_child;
}

static Term ru_rewrite_subtree(Term self, u64 loc, u8 op, Term in_addr,
                                RuRangeMap const *consumer_rm) {
  u8 ar = uop_arity(op);
  if (ar == 0) return self;
  Term srcs[MAX_UOP_SRC] = {0};
  int changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_child = heap_read(loc + i);
    Term new_child = ru_rewrite_child(old_child, in_addr, consumer_rm);
    srcs[i] = new_child;
    if (new_child != old_child) changed = 1;
  }
  if (!changed) return self;
  return uop_graph_rebuild_with_srcs(self, srcs);
}

// Indexed ops: UOP_GATHER and UOP_SCATTER.

// The buffer address of per-axis coordinates into tensor `tid`: the public
// view's strides and offset (row-major when contiguous from 0), then every
// prior view, as tendesc_strided_index (view/strided_index.c) computes it,
// built symbolically.  Such an address reads the raw buffer, so the tid is
// flagged for input_slot_dedup.
static Term ru_ten_leaf_addr(u32 tid, Term const *coords, u32 ndim) {
  TenDesc const *td = &TENS[tid];
  View const    *v  = &td->view;
  int  plain = v->contiguous && v->offset == 0;
  Term addr;
  if (plain) {
    addr = ru_build_addr_with_dims(coords, (u8)ndim, v->shape.dims);
  } else {
    Term acc = (v->offset != 0) ? uop_const(DT_INT32, (u32)v->offset) : 0;
    for (u32 d = 0; d < ndim; d++) {
      if (v->strides[d] == 0) {
        continue;
      }
      Term c = coords[d];
      if (v->strides[d] != 1) {
        c = uop_int_binary(UOP_IMUL, c, uop_const(DT_INT32, (u32)v->strides[d]));
      }
      acc = (acc == 0) ? c : uop_int_binary(UOP_IADD, acc, c);
    }
    addr = (acc != 0) ? acc : uop_const(DT_INT32, 0);
  }
  for (i32 k = (i32)td->nviews - 1; k >= 0; k--) {
    if (!ru_view_composes_as_identity(&td->prior_views[k])) {
      addr = ru_compose_one_view(addr, &td->prior_views[k]);
    }
  }
  if (!plain || td->nviews > 0) {
    RU_TID_CHAIN_COMPOSED[tid] = 1;
  }
  return addr;
}

// Source coordinates of a RESHAPE from its output coordinates: the
// row-major flat index over the output dims, split by the source dims.
// Exact for any reshape of a row-major value, so it never declines.
static void ru_reshape_coords(Term const *oc, Shape const *os, Term *ic,
                              Shape const *is) {
  Term flat   = ru_build_addr_with_dims(oc, (u8)os->ndim, os->dims);
  u32  suffix = 1;
  for (i32 d = (i32)is->ndim - 1; d >= 0; d--) {
    u32  ext = kvar_extent_static(is->dims[d]);
    Term c   = flat;
    if (suffix != 1) {
      c = uop_int_binary(UOP_IDIV, c, uop_const(DT_INT32, suffix));
    }
    if (d != 0) {
      c = uop_int_binary(UOP_IMOD, c, uop_const(DT_INT32, ext));
    }
    ic[d] = c;
    suffix *= ext;
  }
}

__attribute__((noinline, noreturn))
static void ru_index_leaf_fail(u8 op, char const *why) {
  char msg[160];
  snprintf(msg, sizeof msg, "ru_index_leaf: indexed operand op %u %s",
           (u32)op, why);
  thvm_fatal(msg);
}

// The value of indexed operand `t` at per-axis coordinates coords[0..ndim).
// The operand's views are walked structurally (a RESHAPE through its
// row-major flat index, the others through ru_apply_movement) down to
// storage: the buffer of a node bufferize_classify realized
// (BUFFERIZE_REASON_INDEXED_SRC), a TEN's or VAR's storage, a KERNEL's
// output tensor, or a CONST, which is its own value.  An operand that
// reaches none of these was not realized, and no address reads it.  A
// non-UOP term (an unfired grad projection) is returned unlowered; visit()
// declines its kernel as it does elsewhere.
static Term ru_index_leaf(Term t, Term const *coords, u32 ndim) {
  Term c[RU_MAX_AXES];
  for (u32 d = 0; d < ndim; d++) {
    c[d] = coords[d];
  }
  for (;;) {
    Term r   = term_resolve(t);
    u8   tag = term_tag(r);
    if (tag == TAG_TEN) {
      return uop_index_e(r, ru_ten_leaf_addr((u32)term_val(r), c, ndim));
    }
    if (tag == TAG_VAR) {
      Shape vs;
      if (!term_shape_in(r, 0, &vs) || vs.ndim != ndim) {
        return r;
      }
      return uop_index_e(r, ru_build_addr_with_dims(c, (u8)ndim, vs.dims));
    }
    if (tag != TAG_UOP) {
      return r;
    }
    u8  op  = term_ext(r);
    u64 loc = term_val(r);
    if (op == UOP_CONST) {
      return r;
    }
    if (op == UOP_KERNEL) {
      Term out = heap_read(loc);
      if (term_tag(out) != TAG_TEN) {
        ru_index_leaf_fail(op, "has no output tensor");
      }
      return uop_index_e(out, ru_ten_leaf_addr((u32)term_val(out), c, ndim));
    }
    // A realized node is storage, whatever realized it: its kernel binds
    // its buffer (visit_indexed_leaf) and the arena keeps that buffer alive
    // for this read, while the nodes under it may already be recycled.
    u32 idx = bufferize_info_find(loc);
    if (idx != 0xFFFFFFFFu
        && ((BUFFERIZE_NODES[idx].reasons & BUFFERIZE_REASON_INDEXED_SRC)
            || RU_REALIZE_MAP[idx].realized_full)) {
      Term  b = RU_SUBST[idx];
      Shape ns;
      if (b == 0 || term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE
          || !term_shape_in(r, 0, &ns) || ns.ndim != ndim) {
        ru_index_leaf_fail(op, "was realized but has no buffer");
      }
      return uop_index_e(b, ru_build_addr_with_dims(c, (u8)ndim, ns.dims));
    }
    Shape ss;
    if (!ru_node_src_shape(loc, &ss) || ss.ndim > RU_MAX_AXES) {
      ru_index_leaf_fail(op, "has no source shape");
    }
    Term in[RU_MAX_AXES];
    u32  in_ndim = 0;
    switch (op) {
      case UOP_RESHAPE: {
        Shape os;
        if (!ru_node_shape(loc, op, &os) || os.ndim != ndim) {
          ru_index_leaf_fail(op, "has no output shape");
        }
        ru_reshape_coords(c, &os, in, &ss);
        in_ndim = ss.ndim;
        break;
      }
      case UOP_PERMUTE: case UOP_EXPAND: case UOP_SHRINK: case UOP_FLIP:
        if (!ru_apply_movement(loc, op, c, ndim, in, &in_ndim)
            || in_ndim < ss.ndim) {
          ru_index_leaf_fail(op, "has no index swizzle");
        }
        break;
      default:
        ru_index_leaf_fail(op, "is neither storage nor realized");
    }
    // A rank-growing EXPAND aligns its source by suffix: the leading
    // coordinates are the broadcast axes.
    u32 lead = in_ndim - ss.ndim;
    for (u32 d = 0; d < ss.ndim; d++) {
      c[d] = in[lead + d];
    }
    ndim = ss.ndim;
    t    = heap_read(loc);
  }
}

// The value of operand `t` of shape `dims` at the node's own ranges: the
// operand an indexed op reads where its consumer does (GATHER idx, SCATTER
// base).  A structural operand is read through its storage, as the storage
// slots are (ru_index_leaf), so its views compose into the address exactly
// and visit binds that storage (visit_operand_at_ranges).  A computed one
// fuses like an elementwise operand, at the row-major address over its own
// shape (ru_build_input_addr_for would size a GATHER's by slot 0, src).
static Term ru_read_operand_at_ranges(Term t, u32 const *dims,
                                      RuRangeMap const *rm) {
  if (uop_indexed_operand_is_structural(t)) {
    return ru_index_leaf(t, rm->in_rngs, rm->in_ndim);
  }
  Term addr = ru_build_addr_with_dims(rm->in_rngs, rm->in_ndim, dims);
  return ru_rewrite_child(t, addr, rm);
}

// GATHER(src, idx, dim) at this node's ranges: idx is read at the ranges
// (ru_read_operand_at_ranges), src at the ranges with axis `dim` replaced by
// the loaded index clamped to 0, gated INVALID (read as 0) when the index is
// out of range: the collapsed one-hot sum of tinygrad's pm_reduce_load_collapse
// (codegen/simplify.py:124-126).  The gate holds a load, so
// uop_fold_where_closure keeps the clamp, and the INVALID branch keeps the
// materialize broadcast gate off the data-dependent address.  The index
// compares as int32, the width of every kernel address: a 64-bit index outside
// the int32 range wraps (a uint32 -1 still reads as invalid).
static Term ru_lower_gather(Term self, u64 loc, RuRangeMap const *rm) {
  Term  src = heap_read(loc + 0);
  Term  idx = heap_read(loc + 1);
  u32   dim = uop_gather_dim(self);
  Shape is, ss;
  if (!term_shape_in(idx, 0, &is) || !term_shape_in(src, 0, &ss)
      || is.ndim != rm->in_ndim || ss.ndim != is.ndim) {
    ru_index_leaf_fail(UOP_GATHER, "has operand shapes its ranges do not fit");
  }
  Term iv    = uop_cast(ru_read_operand_at_ranges(idx, is.dims, rm), DT_INT32);
  Term valid = uop_int_binary(UOP_IAND,
      uop_int_binary(UOP_ILT, uop_const(DT_INT32, (u32)-1), iv),
      uop_int_binary(UOP_ILT, iv, uop_const(DT_INT32, ss.dims[dim])));
  Term coords[RU_MAX_AXES];
  for (u8 a = 0; a < rm->in_ndim; a++) {
    coords[a] = rm->in_rngs[a];
  }
  coords[dim] = uop_iwhere(valid, iv, uop_const(DT_INT32, 0));
  return uop_iwhere(valid, ru_index_leaf(src, coords, rm->in_ndim), uop_invalid());
}

// acc (op) x for a SUM, MAX or MIN scatter.  MAX is tinygrad's
// (acc < x) ? x : acc, the CPU form of Ops.MAX (codegen/decomp/op.py:84)
// that a reduce folds as acc = MAX(acc, x) (codegen/__init__.py:203); MIN
// is its mirror (x < acc) ? x : acc, which tinygrad's negated (float) or
// inverted (int) MAX computes (mixin/op.py:494, elementwise.py:404-408).
// A NaN x never replaces acc.
static Term ru_scatter_combine(u32 kind, Term acc, Term x) {
  switch (kind) {
    case SCATTER_MAX:
      return uop_where(uop_binary(UOP_CMPLT, acc, x), x, acc);
    case SCATTER_MIN:
      return uop_where(uop_binary(UOP_CMPLT, x, acc), x, acc);
    default:
      return uop_binary(UOP_ADD, acc, x);
  }
}

// SCATTER(base, idx, src, dim, kind, include_self) at node i, always its own
// kernel over its output OUT.  base is read at the node's ranges R
// (ru_read_operand_at_ranges); idx and src through ru_index_leaf at fresh loop
// ranges E over idx.shape.  With t = E but t_dim the loaded index clamped to 0,
// and valid its range check, the accumulate store is
//   OUT[t] = valid ? OUT[t] (op) src[E] : OUT[t]    (src[E] for REPLACE)
// with (op) the kind's ru_scatter_combine.  The src positions that meet at one
// output share t's other coordinates, so every loop order visits them in
// ascending t_dim: tinygrad's reduce order, and REPLACE's last-write-wins.
// With I the kind's identity (scatter_identity_bits) the stores run in AFTER
// order and give tinygrad's out = S (op) T (mixin/op.py:1127-1130):
//   include_self    OUT[R] = I; accumulate; OUT[R] = OUT[R] (op) base[R]
//                   (the last store drops when base is the CONST I);
//   no self         OUT[R] = I (op) base[R] for MAX / MIN, base[R] for
//                   SUM; OUT[t] = valid ? I : OUT[t]; accumulate;
//   REPLACE         OUT[R] = base[R]; accumulate.
// Without include_self a written position holds S and an untouched one I (op)
// base, which is base except that MAX(I, NaN) = I (tinygrad keeps a NaN base
// outside the index's extent for some shapes).  The BUFFERIZE wraps the SCATTER
// itself, so no consumer can inline it.  PROD has no lowering: the node is
// still a boundary, and visit() declines its kernel.
static void ru_lower_scatter(u32 i, RuRangeMap const *rm) {
  u64  loc  = BUFFERIZE_NODES[i].loc;
  Term self = term_new(0, TAG_UOP, UOP_SCATTER, loc);
  Term closed[MAX_DIM];
  u32  n_closed = ru_collect_closed_ranges(i, closed, MAX_DIM);
  Term b = uop_bufferize_new(self, UOP_SCOPE_GLOBAL, 0, n_closed, closed);
  RU_BUFFERIZE_TERM[i] = b;
  RU_SUBST[i]          = b;
  RU_LAST_BUFFERIZES_EMITTED++;
  u32 kind = uop_scatter_kind(self);
  if (kind == SCATTER_PROD) {
    return;
  }
  u32   dim = uop_scatter_dim(self);
  u32   dtype, ident;
  Shape bs, is;
  if (!term_dtype_in(self, 0, &dtype) || !term_shape_in(self, 0, &bs)
      || !term_shape_in(heap_read(loc + 1), 0, &is)
      || bs.ndim != rm->out_ndim || is.ndim != bs.ndim
      || !scatter_identity_bits(kind, dtype, &ident)) {
    ru_index_leaf_fail(UOP_SCATTER, "has operand shapes its ranges do not fit");
  }
  Term out     = uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, bs.ndim, bs.dims, 0);
  Term my_addr = ru_build_index_addr_for(rm, self);
  Term base    = ru_read_operand_at_ranges(heap_read(loc + 0), bs.dims, rm);
  Term e[RU_MAX_AXES];
  for (u32 a = 0; a < is.ndim; a++) {
    e[a] = ru_new_range(is.dims[a], KAX_LOOP);
  }
  Term iv    = uop_cast(ru_index_leaf(heap_read(loc + 1), e, is.ndim), DT_INT32);
  Term sv    = ru_index_leaf(heap_read(loc + 2), e, is.ndim);
  Term valid = uop_int_binary(UOP_IAND,
      uop_int_binary(UOP_ILT, uop_const(DT_INT32, (u32)-1), iv),
      uop_int_binary(UOP_ILT, iv, uop_const(DT_INT32, bs.dims[dim])));
  Term t[RU_MAX_AXES];
  for (u32 a = 0; a < is.ndim; a++) {
    t[a] = e[a];
  }
  t[dim] = uop_iwhere(valid, iv, uop_const(DT_INT32, 0));
  Term t_addr   = ru_build_addr_with_dims(t, (u8)bs.ndim, bs.dims);
  Term old      = uop_index_e(out, t_addr);
  Term upd      = (kind == SCATTER_REPLACE) ? sv : ru_scatter_combine(kind, old, sv);
  Term acc      = uop_store(out, t_addr, uop_where(valid, upd, old));
  Term identity = uop_const(dtype, ident);
  Term root;
  if (kind == SCATTER_REPLACE) {
    root = uop_after(acc, uop_store(out, my_addr, base));
  } else if (uop_scatter_include_self(self)) {
    root = uop_after(acc, uop_store(out, my_addr, identity));
    if (base != identity) {
      Term fin = ru_scatter_combine(kind, uop_index_e(out, my_addr), base);
      root = uop_after(uop_store(out, my_addr, fin), root);
    }
  } else {
    Term init  = (kind == SCATTER_SUM) ? base
                                       : ru_scatter_combine(kind, identity, base);
    Term reset = uop_store(out, t_addr, uop_where(valid, identity, old));
    root = uop_after(acc, uop_after(reset, uop_store(out, my_addr, init)));
  }
  RU_STORE_ROOT[i] = root;
}

// PAD's value-side guard: IWHERE(cond, body, INVALID), cond the AND over the
// padded axes of begin <= out_rng < begin + in_dim; tinygrad's PAD -> WHERE
// (indexing.py:101-105).  An INDEX_E body already carries the swizzler's guard
// in its address, but a CONST, EXPAND or WHERE body (a SHRINK gradient's
// CONST(1) seed) has no address to gate.
static Term ru_pad_wrap_where(u64 loc, RuRangeMap const *rm, Term body) {
  Shape src_shape;
  if (!ru_node_src_shape(loc, &src_shape)) return body;
  u32 ndim = rm->out_ndim;
  if (ndim == 0 || src_shape.ndim != ndim) return body;
  Term cond_acc = 0;
  for (u32 d = 0; d < ndim; d++) {
    u32 begin = (u32)term_val(heap_read(loc + 2 + 2 * d));
    u32 end   = (u32)term_val(heap_read(loc + 2 + 2 * d + 1));
    // An unpadded axis needs no guard, as in apply_movement_op_pad.
    if (begin == 0 && end == 0) continue;
    // A symbolic padded dim bounds by its upper bound, as the strides in
    // ru_build_addr_with_dims do.
    u32 in_dim = kvar_extent_static(src_shape.dims[d]);
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

// tinygrad's pm_apply_rangeify (indexing.py:136-147): a REDUCE gets explicit
// RANGE srcs, a PAD becomes a WHERE, a realized src becomes an INDEX of its
// STAGE, and a movement op drops, its ranges already swizzled.  Here a
// producer-first walk fills RU_SUBST, RU_BUFFERIZE_TERM and RU_STORE_ROOT.
fn void pm_apply_rangeify(Term root) {
  (void)root;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    RU_SUBST[i]           = 0;
    RU_BUFFERIZE_TERM[i]  = 0;
    RU_STORE_ROOT[i]      = 0;
  }
  // The live TENS window is [1, TENS_NEXT).
  for (u32 t = 1; t < TENS_NEXT && t < TENS_CAP; t++) {
    RU_TID_CHAIN_COMPOSED[t] = 0;
  }
  RU_LAST_BUFFERIZES_EMITTED = 0;
  RU_INDEX_AXIS_N = 0;

  // RU_FUSE_ALL_REDUCE_AIDS: every REDUCE range of the pass and every range of
  // a realized node.  A fusing conv-backward product's strided-unfold address
  // can carry hash-consed RANGE leaves of such a scope that its consumer DAG
  // does not reach (another layer's backward); they index correctly once the
  // product splices into its contraction reduce, so the strand checks treat
  // them as covered.  A genuine strand (a stripped closed range) is neither.
  RU_FUSE_ALL_REDUCE_AIDS_N = 0;
  for (u32 n = 0; n < BUFFERIZE_NODES_LEN; n++) {
    for (u8 k = 0; k < RU_REDUCE_RANGES[n].n; k++) {
      u32 aid = uop_range_axis_id(RU_REDUCE_RANGES[n].ranges[k]);
      u8 dup = 0;
      for (u32 j = 0; j < RU_FUSE_ALL_REDUCE_AIDS_N; j++)
        if (RU_FUSE_ALL_REDUCE_AIDS[j] == aid) { dup = 1; break; }
      if (!dup && RU_FUSE_ALL_REDUCE_AIDS_N < RU_FUSE_ALL_REDUCE_AIDS_CAP)
        RU_FUSE_ALL_REDUCE_AIDS[RU_FUSE_ALL_REDUCE_AIDS_N++] = aid;
    }
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

  // Producers first (RU_TOPO_ORDER reversed), so each consumer sees its
  // producers' substitutes.
  for (i64 oi = (i64)RU_TOPO_ORDER_LEN - 1; oi >= 0; oi--) {
    u32 i = RU_TOPO_ORDER[oi];
    UOpInfo const *info = &BUFFERIZE_NODES[i];
    if (ru_is_skip_op(info->op)) continue;
    RuRangeMap const *rm = &RU_RANGE_MAP[i];
    if (!rm->has_ranges) {
      // Not realized and no consumer has ranges: RU_SUBST[i] = 0 leaves the
      // node as it is in its consumers.
      continue;
    }

    int realized = RU_REALIZE_MAP[i].realized_full
                || RU_REALIZE_MAP[i].realized_partial;

    if (info->op == UOP_SCATTER) {
      ru_lower_scatter(i, rm);
      continue;
    }

    Term self = term_new(0, TAG_UOP, info->op, info->loc);

    // The rewritten node is the realized store value or the spliced substitute.
    // A REDUCE then gets its range axes, and the strand checks run.
    Term in_addr   = ru_build_input_addr_for(rm, info->loc);
    Term rewritten = (info->op == UOP_GATHER)
                   ? ru_lower_gather(self, info->loc, rm)
                   : ru_rewrite_subtree(self, info->loc, info->op,
                                        in_addr, rm);
    // Rewire the REDUCE from source-shape axis indices to the fresh ranges'
    // axis_ids (RU_REDUCE_RANGES): uwalk_run_reduce and render_uop match reduce
    // axes by RANGE axis_id.
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
      // Drop the reduce ranges a stride-0 broadcast collapsed out of the
      // body (tinygrad reduce_unparented).
      rewritten = ru_reduce_unparented(rewritten, RU_REDUCE_RANGES[i].ranges,
                                       n_rrng);
      // THVM_FUSE_MATMUL_INPUT rank-grow remap.  A fused A producer splices in
      // through the matmul lowering's RESHAPE({M,K,1}) + EXPAND({M,K,N}), and
      // ru_build_axis_subst's positional map does not compose up that chain, so
      // the producer's own {m,k} ranges stay in the body as free LOOP ranges
      // (fatal below).  tinygrad's src.substitute matches ranges by identity
      // (rangeify.py:102); here each leaked range binds by extent: to an output
      // range of equal extent that the weight B does not read (so M, never N,
      // even when N has the same extent), else to the reduce range when it has
      // K's extent.  Only a 1-axis SUM over a 2-D output qualifies.
      if (ru_fuse_matmul_input_on() && kind == REDUCE_SUM && n_rrng == 1
          && rm->out_ndim == 2
          && term_tag(rewritten) == TAG_UOP && term_ext(rewritten) == UOP_REDUCE) {
        Term rbody = uop_reduce_src(rewritten);
        Term b_idx = 0;   // the weight operand (the clean INDEX_E side of the MUL)
        Term a_side = 0;  // the other operand, which must be a fused producer
        if (term_tag(rbody) == TAG_UOP && term_ext(rbody) == UOP_MUL) {
          Term o0 = term_resolve(heap_read(term_val(rbody) + 0));
          Term o1 = term_resolve(heap_read(term_val(rbody) + 1));
          Term c0 = (term_tag(o0)==TAG_UOP && term_ext(o0)==UOP_CAST)
                  ? term_resolve(heap_read(term_val(o0)+0)) : o0;
          Term c1 = (term_tag(o1)==TAG_UOP && term_ext(o1)==UOP_CAST)
                  ? term_resolve(heap_read(term_val(o1)+0)) : o1;
          if (term_tag(c1)==TAG_UOP && term_ext(c1)==UOP_INDEX_E) { b_idx = c1; a_side = c0; }
          else if (term_tag(c0)==TAG_UOP && term_ext(c0)==UOP_INDEX_E) { b_idx = c0; a_side = c1; }
        }
        // Only a fused elementwise A (an ALU tree over the input), not a bare
        // INDEX_E (a plain operand) or a movement view (a batched attention A,
        // whose free batch axes the 2-D bind would mis-map).
        if (a_side == 0) b_idx = 0;
        else if (term_tag(a_side) != TAG_UOP) b_idx = 0;
        else {
          u8 aop = term_ext(a_side);
          int a_is_ew = uop_is_unary_elementwise(aop)
                     || uop_is_binary_elementwise(aop)
                     || uop_is_ternary_elementwise(aop)
                     || aop == UOP_CAST || aop == UOP_BITCAST;
          if (!a_is_ew) b_idx = 0;   // bare INDEX_E or a movement view: not fused
        }
        Term k_rng = 0;
        for (u32 k = 0; k < n_rrng; k++) {
          Term kr = RU_REDUCE_RANGES[i].ranges[k];
          if (term_tag(kr)==TAG_UOP && term_ext(kr)==UOP_RANGE) { k_rng = kr; break; }
        }
        u32 k_ext = (k_rng != 0) ? uop_range_extent(k_rng) : 0;
        RuFreeAxisSet mg_fs;
        mg_fs.n_bound = 0;
        mg_fs.n_free  = 0;
        ru_collect_free_axes_rec(rbody, &mg_fs, 0);
        RuAxisSubst mg_map;
        mg_map.n = 0;
        for (u8 fi = 0; fi < mg_fs.n_free && b_idx != 0 && k_rng != 0; fi++) {
          if (mg_fs.free_types[fi] != KAX_LOOP) continue;
          u32 faid = mg_fs.free_aids[fi];
          if (faid == uop_range_axis_id(k_rng)) continue; // already the reduce axis
          int is_out = 0;
          for (u8 a = 0; a < rm->out_ndim && !is_out; a++)
            if (ru_expr_references_aid(rm->out_rngs[a], faid)) is_out = 1;
          if (is_out) continue;                           // a genuine output axis
          u32 fext = ru_subtree_axis_extent(rbody, faid);
          if (fext == 0) continue;
          Term bind = 0;
          // M first: an output range of equal extent the weight does not read.
          for (u8 a = 0; a < rm->out_ndim && bind == 0; a++) {
            Term orng = rm->out_rngs[a];
            if (term_tag(orng)==TAG_UOP && term_ext(orng)==UOP_RANGE
                && uop_range_extent(orng) == fext
                && !ru_expr_references_aid(b_idx, uop_range_axis_id(orng)))
              bind = orng;
          }
          // Else K: the reduce range, when the extents match.
          if (bind == 0 && fext == k_ext) bind = k_rng;
          if (bind != 0 && uop_range_axis_id(bind) != faid
              && mg_map.n < RU_AXIS_SUBST_CAP)
            ru_axis_subst_add(&mg_map, faid, bind);
        }
        if (mg_map.n > 0) {
          Term nb = ru_subtree_rewrite_ranges(rbody, &mg_map, 0);
          if (nb != rbody)
            rewritten = uop_reduce_multi(kind, n_rrng, new_axes, nb);
        }
      }
      // A free LOOP range in a reduce body that is neither an output nor a
      // reduce axis would render as a loop around the store.
      // bufferize_reduce_reads_realized_pool realizes the contraction that
      // would produce one (a col2im window read of a realized buffer); meeting
      // one here is fatal.
      if (term_tag(rewritten) == TAG_UOP && term_ext(rewritten) == UOP_REDUCE) {
        RuFreeAxisSet bs;
        bs.n_bound = 0;
        bs.n_free  = 0;
        ru_collect_free_axes_rec(uop_reduce_src(rewritten), &bs, 0);
        for (u8 fi = 0; fi < bs.n_free; fi++) {
          if (bs.free_types[fi] != KAX_LOOP) continue;
          u32 faid = bs.free_aids[fi];
          int is_out = 0;
          for (u8 a = 0; a < rm->out_ndim && !is_out; a++)
            if (ru_expr_references_aid(rm->out_rngs[a], faid)) is_out = 1;
          if (is_out) continue;
          int is_red = 0;
          u32 n_cur = uop_reduce_n_axes(rewritten);
          for (u32 k = 0; k < n_cur && !is_red; k++)
            if (uop_reduce_axis(rewritten, k) == faid) is_red = 1;
          if (!is_red) {
            thvm_fatal("rangeify: a reduce would absorb a stranded window LOOP axis");
          }
        }
      }
    }
    // A REDUCE whose reduce axes all have extent 1 has no range left
    // (ru_new_range made them CONST(0)): it is its body, as the sum over [0, 1)
    // of f(i) is f(0).
    if (info->op == UOP_REDUCE && RU_REDUCE_RANGES[i].n == 0
        && term_tag(rewritten) == TAG_UOP
        && term_ext(rewritten) == UOP_REDUCE) {
      rewritten = heap_read(term_val(rewritten) + 0);
    }

    // The strand check: a free RANGE in the rewritten subtree must be bound by
    // the producer's out_rngs (consumers rebind those on splice) or by a REDUCE
    // inside it.  Any other is a stranded producer-internal range (a
    // partial-realize child's closed range, a reduce axis whose REDUCE was
    // stripped), which a consumer would read at index 0.  Skipped for a scalar
    // producer (no RANGE in out_rngs): its free ranges belong to a deeper
    // producer that stays inlined, so the check would be a false positive.
    int has_nontrivial_out_axis = 0;
    for (u8 a = 0; a < rm->out_ndim; a++) {
      Term pr = rm->out_rngs[a];
      if (term_tag(pr) == TAG_UOP && term_ext(pr) == UOP_RANGE) {
        has_nontrivial_out_axis = 1;
        break;
      }
    }
    // Also skipped for PAD and SHRINK, whose in_rngs shift against out_rngs:
    // ranges leaked through them (the conv-im2col backward's
    // EXPAND-of-broadcast input) are bound at an outer consumer REDUCE.
    int is_shift_op_skip = (info->op == UOP_PAD || info->op == UOP_SHRINK);
    if (!realized && has_nontrivial_out_axis && !is_shift_op_skip) {
      RuFreeAxisSet fs;
      fs.n_bound = 0;
      fs.n_free  = 0;
      ru_collect_free_axes_rec(rewritten, &fs, 0);
      // A free axis a consumer REDUCE binds (the conv-backward contraction
      // product feeding its scatter reduce) is covered.
      u32 cons_red_aids[RU_CONSRED_AIDS_CAP];
      u8  n_cons_red = 0;
      if (fs.n_free > 0) {
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
        // A hash-cons-aliased REDUCE axis of a disjoint scope (another layer's
        // backward) is covered, but only a KAX_REDUCE leaf: a LOOP leaf whose
        // id collides with a reduce id is a real loop axis (a col2im window
        // read of a realized buffer) and still strands.
        if (!covered && fs.free_types[fi] == KAX_REDUCE) {
          for (u32 g = 0; g < RU_FUSE_ALL_REDUCE_AIDS_N && !covered; g++)
            if (RU_FUSE_ALL_REDUCE_AIDS[g] == aid) covered = 1;
        }
        // A leaf anywhere inside one of the producer's out_rngs (a RESHAPE
        // swizzle, aid_hi*k + aid_lo) is covered: the consumer iterates it and
        // substitutes the whole entry on splice.
        for (u8 a = 0; a < rm->out_ndim && !covered; a++) {
          if (ru_expr_references_aid(rm->out_rngs[a], aid)) covered = 1;
        }
        if (!covered) { stranded = 1; break; }
      }
      if (stranded) {
        thvm_fatal("rangeify: a fused producer strands a free range");
      }
    }

    if (realized) {
      // indexing.py:92: STAGE(value, closed_ranges).  The BUFFERIZE wraps the
      // rewritten subtree, so uop_bufferize_value returns the lowered value
      // with INDEX_Es at its realized producers.
      Term my_addr = ru_build_index_addr_for(rm, self);
      // A movement node's rewritten shell wraps an INDEX_E whose address
      // already holds the swizzle.  The store value strips the shell, as the
      // non-realized branch does, since cpu_uop_walk has no value-level
      // movement ops; the BUFFERIZE keeps it.
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
      // A full realize is GLOBAL, a partial one LOCAL.  tinygrad has no partial
      // realize: its STAGE is always GLOBAL (indexing.py:59, 91).
      u32 addrspace = RU_REALIZE_MAP[i].realized_full
                    ? UOP_SCOPE_GLOBAL
                    : UOP_SCOPE_LOCAL;
      // tinygrad's removable (indexing.py:90) excludes ALWAYS_CONTIGUOUS srcs;
      // thvm has no such set, and remove_bufferize has already run.
      u32 removable = 1;
      Term b = uop_bufferize_new(rewritten, addrspace, removable,
                                 n_closed, closed_ranges);
      RU_BUFFERIZE_TERM[i] = b;
      RU_LAST_BUFFERIZES_EMITTED++;
      // The kernel root: STORE(buffer of the node's shape and dtype, my_addr,
      // store_value).  Without a dtype it stays 0 and kernel_lift_to_uop
      // rejects the kernel.  DT_BOOL is 0, so the test is term_dtype_in's
      // result: store_dtype != 0 would drop every cast-to-bool kernel.
      u32 store_dtype = 0;
      if (term_dtype_in(self, 0, &store_dtype)) {
        Shape out_shape = {0};
        if (term_shape_in(self, 0, &out_shape) && out_shape.ndim <= MAX_DIM) {
          Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, store_dtype,
                                         out_shape.ndim, out_shape.dims, 0);
          RU_STORE_ROOT[i] = uop_store(out_buf, my_addr, store_value);
        }
      }
      // RU_SUBST is the bare BUFFERIZE: each consumer indexes it at its own
      // in_rngs (ru_rewrite_child; indexing.py:93).  INDEX_E(b, my_addr) would
      // read at this node's fresh ranges, which no consumer iterates.
      RU_SUBST[i] = b;
    } else {
      // A src not in the realize map stays inline (indexing.py:78): RU_SUBST is
      // the rewritten value, which the consumer's ru_rewrite_child splices in.
      RU_SUBST[i] = rewritten;
    }

    // remove_movement_op_after_rangeify (indexing.py:133-134): a non-realized
    // movement op forwards its producer's substitute.  A realized one keeps its
    // BUFFERIZE, so divergent consumers each index the buffer at their own
    // ranges instead of sharing consumer 0's view.
    int is_movement = (info->op == UOP_RESHAPE || info->op == UOP_PERMUTE
                    || info->op == UOP_EXPAND  || info->op == UOP_PAD
                    || info->op == UOP_SHRINK  || info->op == UOP_FLIP);
    if (is_movement && realized) {
      continue;
    }
    if (is_movement) {
      // Forward the producer's substitute, except a RESHAPE or PERMUTE over a
      // realized producer: its shell's slot 0 holds INDEX_E(buffer, swizzled
      // address), and forwarding the bare BUFFERIZE would drop the swizzle, so
      // the shell is unwrapped below.  Over a realized producer, a PAD, SHRINK,
      // FLIP or EXPAND builds a fresh INDEX_E at its own input address.
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
            // A non-realized PAD forwards under its WHERE guard, so reads
            // outside the kept window are INVALID (0); a BUFFERIZE is first
            // indexed at the PAD's input address, whose in_rngs hold the shift.
            Term fwd = psub;
            if (info->op == UOP_PAD && !realized) {
              if (psub_is_bufferize) {
                Term in_addr = ru_build_input_addr_for(rm, info->loc);
                fwd = uop_index_e(psub, in_addr);
                ru_index_axes_register(fwd, rm->in_rngs, rm->in_ndim);
              }
              fwd = ru_pad_wrap_where(info->loc, rm, fwd);
            } else if (psub_is_bufferize
                       && (info->op == UOP_SHRINK || info->op == UOP_FLIP
                        || info->op == UOP_EXPAND)) {
              // A SHRINK (getitem) or FLIP over a realized producer reads it at
              // its own swizzled address (in_rngs carry the SHRINK begin):
              // forwarding the bare buffer would make c[:,0] and c[:,1] read
              // the same element.  tinygrad indexes a STAGE at the consumer's
              // ranges (indexing.py:93) after the SHRINK swizzle
              // (indexing.py:173).  A broadcast EXPAND likewise: its in_rngs
              // read 0 on broadcast axes, where the consumer's own address
              // would run past the buffer (test_shared_weight_levels).
              Term in_addr = ru_build_input_addr_for(rm, info->loc);
              fwd = uop_index_e(psub, in_addr);
              ru_index_axes_register(fwd, rm->in_rngs, rm->in_ndim);
            }
            RU_SUBST[i] = fwd;
            continue;
          }
        }
      }
      // Unwrap the movement shell to the INDEX_E in its slot 0.
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
