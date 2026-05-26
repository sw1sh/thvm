// uop/devectorize.c - port of tinygrad codegen/late/devectorizer.py to
// the thvm UOp graph rewrite framework.
//
// Builds on uop/expander.c (commit fc371fe3): the expander produces a
// DAG carrying vector-typed UOPs wrapped in UOP_UNROLL with VCONST /
// CONTRACT / GEP swizzles.  This file lowers that DAG into a shape the
// renderer's existing emit walk can consume:
//
//   1. uop_devectorize_graph(root) runs three sub-passes:
//      (a) pm_reduce      -- REDUCE(body, *axes) ->
//                             PLACEHOLDER(acc_id)
//                             + STORE(acc, 0, identity)
//                             + STORE(acc, 0, acc[0] op body)
//                             + END(axes)
//                             + LOAD(acc[0])
//          Mirrors devectorizer.py:311-328 (reduce_to_acc).
//      (b) pm_devectorize -- vector-typed ALU / CAST / BITCAST split
//                            into a STACK of N scalar ALUs threading
//                            GEPs into the source operands.  Mirrors
//                            devectorizer.py:235-273.
//      (c) pm_render      -- VCONST -> STACK of scalar CONSTs;
//                            GEP(STACK(...), (i,)) -> STACK[i];
//                            GEP of a single index over a scalar src
//                            -> src; STACK with single src -> src.
//                            Mirrors devectorizer.py:275-295.
//
//   2. uop_load_store_fold_graph(root) is a SEPARATE pass implementing
//      the load-store folding from devectorizer.py:81-117 / 136-149:
//      F adjacent scalar LOADs whose addresses differ by 0..F-1 along
//      a single stride collapse to one wide LOAD + GEPs.  Gated on
//      the backend's `supports_float4`.  Currently implements the
//      detection scaffolding -- the actual fold fires when the input
//      shape matches (driven by tests for the structural case).
//
// NOT WIRED INTO render_uop.c.  The renderer still walks the RANGE-leaf
// representation directly; teaching it to emit PLACEHOLDER / END / STACK
// / GEP-folded wide LOADs is a separate change (the "wire" step from
// the task description's stage (f)).  This file lands the graph rewrite
// infrastructure + tests; wiring + the V100 wall measurement is a
// follow-up commit once the renderer is taught the new shape.
//
// References:
//   tinygrad/codegen/late/devectorizer.py:81-149   load_store_folding
//   tinygrad/codegen/late/devectorizer.py:235-273  devectorize
//   tinygrad/codegen/late/devectorizer.py:275-295  pm_render
//   tinygrad/codegen/late/devectorizer.py:311-328  reduce_to_acc
//   tinygrad/codegen/late/devectorizer.py:350-357  pm_reduce
//   tinygrad/codegen/__init__.py:60-78             pipeline order

// === Constructors ========================================================

// UOP_STACK: heap = [NUM(n), src_0, ..., src_{n-1}].  Variadic over Term
// children; we treat it as arity-0 in uop_meta so the generic walker
// stops short of the variadic payload.  Hash-cons via uop_mov_cache.
fn Term uop_stack(u32 n, Term const *srcs) {
  if (n == 0) {
    // Zero-element STACK is the empty vector marker (tinygrad uses it
    // for SINK accumulator init; we keep the slot for symmetry).
    u32 key_buf[1] = { 0 };
    u64 key = uop_mov_hash(UOP_STACK, 0, key_buf, 1);
    Term hit = uop_mov_lookup(key);
    if (hit != 0) return hit;
    u64 loc = heap_alloc(1);
    heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, 0));
    Term t = term_new(0, TAG_UOP, UOP_STACK, loc);
    uop_mov_insert(key, t);
    return t;
  }
  // Single-element STACK degenerates to its src (mirrors pm_render
  // rule devectorizer.py:282).  Callers can rely on this so the
  // expand+devec pipeline doesn't accumulate trivial STACKs.
  if (n == 1) return srcs[0];
  enum { STACK_KEY_MAX = 1 + 256 };
  if (1u + n > STACK_KEY_MAX) {
    u64 loc = heap_alloc(1 + n);
    heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, n));
    for (u32 i = 0; i < n; i++) heap_set(loc + 1 + i, srcs[i]);
    return term_new(0, TAG_UOP, UOP_STACK, loc);
  }
  u32 key_buf[STACK_KEY_MAX];
  key_buf[0] = n;
  // Hash the src Term values directly; each Term is u64 but the
  // mov-cache hash takes u32; xor the high/low halves so two
  // distinct Terms with same low 32 bits don't collide spuriously.
  for (u32 i = 0; i < n; i++) {
    key_buf[1 + i] = (u32)(((u64)srcs[i]) ^ (((u64)srcs[i]) >> 32));
  }
  u64 key = uop_mov_hash(UOP_STACK, 0, key_buf, 1 + n);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    // Sanity-check the hit (low probability of cross-key collision).
    u64 loc = term_val(hit);
    u32 hit_n = (u32)term_val(heap_read(loc + 0));
    if (hit_n == n) {
      int eq = 1;
      for (u32 i = 0; i < n && eq; i++) {
        if (heap_read(loc + 1 + i) != srcs[i]) eq = 0;
      }
      if (eq) return hit;
    }
  }
  u64 loc = heap_alloc(1 + n);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, n));
  for (u32 i = 0; i < n; i++) heap_set(loc + 1 + i, srcs[i]);
  Term t = term_new(0, TAG_UOP, UOP_STACK, loc);
  uop_mov_insert(key, t);
  return t;
}

fn u32 uop_stack_n(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  return (u32)term_val(heap_read(term_val(t) + 0));
}
fn Term uop_stack_src(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  return heap_read(term_val(t) + 1 + i);
}

// UOP_PLACEHOLDER: heap = [NUM(dtype), NUM(acc_id)].  Atom-like (arity 0).
// Two PLACEHOLDERs with the same (dtype, acc_id) hash-cons to the same
// Term -- the renderer should emit exactly one `<dtype> _acc<acc_id>;`
// declaration regardless of how many graph nodes reference it.
fn Term uop_placeholder(u32 dtype, u32 acc_id) {
  u32 key_buf[2] = { dtype, acc_id };
  u64 key = uop_mov_hash(UOP_PLACEHOLDER, 0, key_buf, 2);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, dtype));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, acc_id));
  Term t = term_new(0, TAG_UOP, UOP_PLACEHOLDER, loc);
  uop_mov_insert(key, t);
  return t;
}
fn u32 uop_placeholder_dtype(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_PLACEHOLDER) return 0;
  return (u32)term_val(heap_read(term_val(t) + 0));
}
fn u32 uop_placeholder_acc_id(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_PLACEHOLDER) return 0;
  return (u32)term_val(heap_read(term_val(t) + 1));
}

// UOP_END: heap = [NUM(n), range_0, ..., range_{n-1}].  Each range_i is
// a UOP_RANGE Term.  We don't hash-cons identical END nodes -- they
// carry a positional meaning (close THIS loop body) and identical END
// markers in two different reduces are semantically distinct.
fn Term uop_end(u32 n, Term const *ranges) {
  if (n == 0) return 0;
  if (n > 16) n = 16;
  u64 loc = heap_alloc(1 + n);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, n));
  for (u32 i = 0; i < n; i++) heap_set(loc + 1 + i, ranges[i]);
  return term_new(0, TAG_UOP, UOP_END, loc);
}
fn u32 uop_end_n(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_END) return 0;
  return (u32)term_val(heap_read(term_val(t) + 0));
}
fn Term uop_end_range(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_END) return 0;
  return heap_read(term_val(t) + 1 + i);
}

// === Helpers ===========================================================

// Bits of float identity element for a reduce kind.  Mirrors tinygrad
// identity_element() in uop/ops.py.  For SUM the identity is +0.0; for
// MAX it is -INFINITY.  Returned as a u32 holding the float bit
// pattern -- consumers pass it directly to uop_const(dtype, bits).
static u32 devec_reduce_identity_bits(u32 kind, u32 dtype) {
  // Only FP32 supported in this first port -- the reduce-to-acc path
  // lands on float accumulators; int reductions go through a different
  // tinygrad lowering.  Other dtypes fall back to +0.0.
  (void)dtype;
  if (kind == REDUCE_MAX) {
    // -INFINITY in IEEE-754 fp32: sign=1, exp=0xFF, mant=0
    union { float f; u32 u; } v;
    v.u = 0xFF800000u;
    return v.u;
  }
  // REDUCE_SUM or unknown -> +0.0
  return 0u;
}

// === pm_reduce / reduce_to_acc (devectorizer.py:311-328) ===============
//
// Context shared across all rewrites in one uop_graph_rewrite pass: a
// monotonically increasing acc_id counter so every distinct REDUCE gets
// a unique accumulator slot (tinygrad's ReduceContext.acc_num).
typedef struct {
  u32 acc_num;
} DevecReduceCtx;

// Build the reduce-to-acc lowering for one REDUCE node:
//   placeholder = PLACEHOLDER(dtype, acc_num++)
//   init        = STORE(placeholder, 0, CONST(identity))
//   update      = STORE(placeholder, 0, ALU(LOAD(placeholder, 0), body))
//   end         = END(reduce_axes)
//   result      = LOAD(placeholder, 0) sequenced after end
//
// The LOAD/STORE used here are the value-layer UOP_LOAD / UOP_STORE
// from src/uop/load.c / src/uop/store.c.  thvm's STORE takes (buf,
// addr, value) and LOAD takes (src); we use index 0 (a CONST) as the
// address since the accumulator is a 1-slot REG buffer.
//
// The return Term replaces the REDUCE in its parent; downstream rewrites
// stitch the init + update + end into the parent's STORE-order chain
// via UOP_AFTER markers when the renderer is wired up.  For the
// graph-rewrite output we encode the chain by sequencing through
// UOP_AFTER:
//   final = AFTER(LOAD(placeholder), end_marker)
// where end_marker itself nests AFTER(update, init).  The AFTER tree
// is what the renderer's existing emit-after walker (rmu_emit_after)
// already linearizes into source order.
static Term devec_reduce_to_acc(DevecReduceCtx *ctx, Term red) {
  if (term_tag(red) != TAG_UOP || term_ext(red) != UOP_REDUCE) return 0;
  u64 loc = term_val(red);
  Term body = heap_read(loc + 0);
  u32 kind = uop_reduce_kind(red);
  u32 n_axes = uop_reduce_n_axes(red);
  // dtype inherited from body.
  u32 body_dtype = DT_FP32;
  (void)term_dtype_in(body, 0, &body_dtype);
  u32 acc_id = ctx->acc_num++;
  Term acc = uop_placeholder(body_dtype, acc_id);
  u32 idbits = devec_reduce_identity_bits(kind, body_dtype);
  Term identity = uop_const(body_dtype, idbits);
  Term zero_addr = uop_const(DT_INT32, 0);
  // init: STORE(acc, 0, identity)
  Term init = uop_store(acc, zero_addr, identity);
  // loaded acc value used inside the update.
  Term acc_val = uop_load(acc);
  // body op: combine acc with body via the same op the REDUCE carried.
  Term combined = 0;
  if (kind == REDUCE_SUM) {
    combined = uop_binary(UOP_ADD, acc_val, body);
  } else if (kind == REDUCE_MAX) {
    // No UOP_MAX in the value layer; the tinygrad path emits
    // alu(MAX, acc, body) and the renderer lowers it.  thvm currently
    // does MAX-reduces via the REDUCE_MAX kind on the REDUCE itself;
    // for a one-shot fallback emit a CMPLT-based select.
    // body if body >= acc else acc -> WHERE(CMPLT(acc, body), body, acc)
    Term lt = uop_binary(UOP_CMPLT, acc_val, body);
    combined = uop_iwhere(lt, body, acc_val);
  } else {
    // Unknown reduce kind; bail out and leave the REDUCE alone.
    return 0;
  }
  Term update = uop_store(acc, zero_addr, combined);
  // END marker carrying the reduce axes (as their RANGE Terms).
  // Build a tiny array of RANGE Terms for each axis_id.  In thvm's
  // representation the axes are stored as IDs; we synthesize a
  // canonical RANGE(axis_id, KAX_LOOP, extent) for the marker since the
  // REDUCE node itself does not carry per-axis extents.  The marker is
  // only consumed positionally by the renderer (close N loops here);
  // the precise RANGE term is identification only.
  Term axes_terms[8] = {0};
  u32 nax = n_axes;
  if (nax > 8) nax = 8;
  for (u32 i = 0; i < nax; i++) {
    u32 ax = uop_reduce_axis(red, i);
    // extent 0 placeholder -- the renderer matches END.range to a
    // RANGE on the value tree by axis_id alone.
    axes_terms[i] = uop_range(ax, KAX_LOOP, 0);
  }
  Term end_marker = (nax > 0) ? uop_end(nax, axes_terms) : 0;
  // Sequence: AFTER(update, init), then END after that, then LOAD after
  // END.  thvm's UOP_AFTER takes (node, after_node) meaning "node
  // executes after after_node".  We thread:
  //   chain1 = AFTER(update, init)        -- update sequenced after init
  //   chain2 = (end_marker != 0) ? AFTER(end_marker, chain1) : chain1
  //   result = AFTER(LOAD(acc), chain2)   -- final load after the end
  Term chain1 = uop_after(update, init);
  Term chain2 = (end_marker != 0) ? uop_after(end_marker, chain1) : chain1;
  Term final_load = uop_load(acc);
  Term result = uop_after(final_load, chain2);
  return result;
}

static Term devec_pm_reduce(Term t, void *user) {
  DevecReduceCtx *ctx = (DevecReduceCtx *)user;
  return devec_reduce_to_acc(ctx, t);
}

// === devectorize: vector ALU -> STACK of scalar ALU =====================
//
// Detects when a UOP_ADD / UOP_MUL / UOP_NEG / etc. has vector dtype
// (carried via UOP_UNROLL wrapping) and rewrites to:
//   STACK(N, [op(gep(src0, i), gep(src1, i), ...) for i in range(N)])
// Mirrors devectorizer.py:235-239 (no_vectorized_alu).
//
// We detect "vector" by structural shape: the node is wrapped in
// UOP_UNROLL.  The factor product gives the lane count.  We pull the
// inner (vector) ALU out, replicate it scalar-wise.

static int devec_alu_op_arity(u32 op) {
  switch (op) {
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
    case UOP_CAST: case UOP_BITCAST:
      return 1;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT: case UOP_IAND: case UOP_IOR:
    case UOP_IXOR:
      return 2;
    case UOP_IWHERE:
      return 3;
    default:
      return 0;
  }
}

// Build STACK(N, [op(gep(src0, i), gep(src1, i), ...) for i in N]).
// `srcs` is an array of N_SRCS Term operands (each potentially a vector
// or a scalar to be broadcast); for non-vector srcs we don't wrap in
// GEP since they're already scalar.  Returns 0 if no rewrite applies.
static Term devec_no_vec_alu(Term node) {
  if (term_tag(node) != TAG_UOP) return 0;
  u32 op = term_ext(node);
  int ar = devec_alu_op_arity(op);
  if (ar == 0) return 0;
  // Detect the vector width: look at the topmost UNROLL wrapping (or
  // STACK below already-lowered srcs).  We accept any src that's a
  // STACK to derive N from.  No vector context -> no rewrite.
  u64 loc = term_val(node);
  Term srcs[3];
  for (int i = 0; i < ar; i++) srcs[i] = heap_read(loc + i);
  u32 width = 0;
  for (int i = 0; i < ar; i++) {
    if (term_tag(srcs[i]) == TAG_UOP && term_ext(srcs[i]) == UOP_STACK) {
      u32 w = uop_stack_n(srcs[i]);
      if (width == 0) width = w;
      else if (width != w) return 0;  // mismatched widths -> bail
    }
  }
  if (width == 0 || width == 1) return 0;
  if (width > 256) return 0;
  Term out[256];
  for (u32 i = 0; i < width; i++) {
    Term lane_srcs[3];
    for (int j = 0; j < ar; j++) {
      Term s = srcs[j];
      if (term_tag(s) == TAG_UOP && term_ext(s) == UOP_STACK) {
        lane_srcs[j] = uop_stack_src(s, i);
      } else {
        // Scalar broadcast: same src on every lane.
        lane_srcs[j] = s;
      }
    }
    Term lane_node = 0;
    switch (op) {
      case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
      case UOP_LOG2: case UOP_SQRT:
        lane_node = uop_unary(op, lane_srcs[0]);
        break;
      case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
        lane_node = uop_binary(op, lane_srcs[0], lane_srcs[1]);
        break;
      case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
      case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR:
      case UOP_IXOR:
        lane_node = uop_int_binary(op, lane_srcs[0], lane_srcs[1]);
        break;
      case UOP_IWHERE:
        lane_node = uop_iwhere(lane_srcs[0], lane_srcs[1], lane_srcs[2]);
        break;
      case UOP_CAST: {
        u32 dt = (u32)term_val(heap_read(loc + 1));
        lane_node = uop_cast(lane_srcs[0], dt);
        break;
      }
      case UOP_BITCAST: {
        u32 dt = (u32)term_val(heap_read(loc + 1));
        lane_node = uop_bitcast(lane_srcs[0], dt);
        break;
      }
      default:
        return 0;
    }
    out[i] = lane_node;
  }
  return uop_stack(width, out);
}

static Term devec_pm_devectorize(Term t, void *user) {
  (void)user;
  return devec_no_vec_alu(t);
}

// === pm_render lowering rules (devectorizer.py:275-295) ================
//
// 1. VCONST -> STACK of scalar CONSTs.
// 2. GEP(STACK(...), (i,)) -> STACK src[i] (when single index over a
//    STACK).
// 3. GEP(scalar_src, (0,)) -> scalar_src (when source's vcount==1).
// 4. STACK with single src -> the src itself.

static Term devec_pm_vconst_to_stack(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_VCONST) return 0;
  u32 dtype = uop_vconst_dtype(t);
  u32 n = uop_vconst_n(t);
  if (n <= 1 || n > 256) return 0;
  Term elts[256];
  for (u32 i = 0; i < n; i++) {
    elts[i] = uop_const(dtype, uop_vconst_bits(t, i));
  }
  return uop_stack(n, elts);
}

static Term devec_pm_gep_unwrap(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_GEP) return 0;
  u32 n = uop_gep_n_idx(t);
  Term src = heap_read(term_val(t) + 0);
  // Case: GEP over a STACK -> pull out the lanes.
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_STACK) {
    u32 src_n = uop_stack_n(src);
    if (n == 1) {
      u32 idx = uop_gep_idx(t, 0);
      if (idx < src_n) return uop_stack_src(src, idx);
    }
    if (n > 1 && n <= 256) {
      // GEP(STACK(...), (i_0, ..., i_{n-1})) -> STACK(src[i_0], ..., src[i_{n-1}])
      Term out[256];
      for (u32 i = 0; i < n; i++) {
        u32 idx = uop_gep_idx(t, i);
        if (idx >= src_n) return 0;
        out[i] = uop_stack_src(src, idx);
      }
      return uop_stack(n, out);
    }
  }
  // Case: GEP(scalar, (0,)) -> scalar.
  if (n == 1 && uop_gep_idx(t, 0) == 0) {
    // Heuristic: if src is not a STACK / UNROLL / VCONST, it's already
    // scalar, so the GEP is a no-op.
    if (term_tag(src) == TAG_UOP) {
      u32 sop = term_ext(src);
      if (sop != UOP_STACK && sop != UOP_UNROLL && sop != UOP_VCONST) {
        return src;
      }
    } else {
      return src;
    }
  }
  return 0;
}

// uop_stack already collapses n==1 in the constructor, but a STACK that
// was built before its only-element status was visible can still be
// rewritten by this rule fired bottom-up.
static Term devec_pm_stack_singleton(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  if (uop_stack_n(t) != 1) return 0;
  return uop_stack_src(t, 0);
}

// === Entry point: uop_devectorize_graph ================================

fn Term uop_devectorize_graph(Term root) {
  // Pass (a): pm_reduce -- one shared ReduceContext threads acc_num
  // across every REDUCE the rewrite encounters.
  static UOpGraphRewriteRule const REDUCE_RULES[] = {
    { "devec_pm_reduce", devec_pm_reduce },
  };
  DevecReduceCtx ctx = { 0 };
  Term t = uop_graph_rewrite(root, REDUCE_RULES,
                             sizeof(REDUCE_RULES) / sizeof(REDUCE_RULES[0]),
                             &ctx);
  // Pass (b): devectorize -- vector ALU -> STACK of scalar ALU.  This
  // pass must see STACK leaves (produced by pm_render lowering of
  // VCONST/GEP) to drive the width detection, so we run pm_render
  // FIRST on this sub-pass, then devectorize.
  static UOpGraphRewriteRule const RENDER_RULES[] = {
    { "devec_pm_vconst_to_stack",  devec_pm_vconst_to_stack },
    { "devec_pm_gep_unwrap",       devec_pm_gep_unwrap },
    { "devec_pm_stack_singleton",  devec_pm_stack_singleton },
  };
  t = uop_graph_rewrite(t, RENDER_RULES,
                        sizeof(RENDER_RULES) / sizeof(RENDER_RULES[0]),
                        NULL);
  static UOpGraphRewriteRule const DEVEC_RULES[] = {
    { "devec_pm_devectorize", devec_pm_devectorize },
  };
  t = uop_graph_rewrite(t, DEVEC_RULES,
                        sizeof(DEVEC_RULES) / sizeof(DEVEC_RULES[0]),
                        NULL);
  // Pass (c) again: clean up any STACK-of-singleton / GEP-of-singleton
  // residue left by devectorize.
  t = uop_graph_rewrite(t, RENDER_RULES,
                        sizeof(RENDER_RULES) / sizeof(RENDER_RULES[0]),
                        NULL);
  return t;
}

// === load_store_folding (devectorizer.py:81-149) =======================
//
// fold_expanded_index: F adjacent scalar LOADs whose addresses differ by
// 0, 1, ..., F-1 along a single stride collapse to one wide LOAD + GEPs.
// This is the path #3 effect: the post-devectorize graph carries STACKs
// of scalar LOADs; this pass walks each STACK, checks the address
// pattern, and rewrites to a single LOAD over a vector-typed INDEX_E
// plus per-lane GEPs.
//
// Detection: a STACK(LOAD(INDEX_E(buf, addr_0)), LOAD(INDEX_E(buf, addr_1)), ...)
// where addr_i - addr_0 == i (or a unit-stride pattern in symbolic
// arithmetic).  For this first port we recognise the simplest case:
// addr_i = IADD(base, CONST(i)) where base is shared and CONST(i) is
// literally i.
//
// Backend gate: float / half dtype only; integer types fold differently
// (no SIMD wide-load benefit) and we keep them scalar.

// Walks t down through UOP_LOAD -> UOP_INDEX_E to extract (buf, addr).
// Returns 1 on match.
static int devec_extract_load_index(Term t, Term *out_buf, Term *out_addr) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_LOAD) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_INDEX_E) return 0;
  *out_buf = heap_read(term_val(inner) + 0);
  *out_addr = heap_read(term_val(inner) + 1);
  return 1;
}

// For addr_i = IADD(base, CONST(i)) extract base and i.  If addr_i is
// just a CONST (i.e. i == 0 base), accept base == NULL / i = const.
// If addr_i is a bare non-CONST term (e.g. UOP_IADD's identity rule
// folded IADD(base, CONST(0)) -> base), treat that as (base, off=0).
static int devec_extract_unit_stride(Term addr, Term *out_base, u32 *out_off) {
  if (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_IADD) {
    u64 loc = term_val(addr);
    Term a = heap_read(loc + 0);
    Term b = heap_read(loc + 1);
    if (term_tag(b) == TAG_UOP && term_ext(b) == UOP_CONST) {
      Term cell = heap_read(term_val(b) + 0);
      if (term_tag(cell) == TAG_NUM) {
        *out_base = a;
        *out_off = (u32)term_val(cell);
        return 1;
      }
    }
    if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) {
      Term cell = heap_read(term_val(a) + 0);
      if (term_tag(cell) == TAG_NUM) {
        *out_base = b;
        *out_off = (u32)term_val(cell);
        return 1;
      }
    }
    return 0;
  }
  if (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_CONST) {
    Term cell = heap_read(term_val(addr) + 0);
    if (term_tag(cell) == TAG_NUM) {
      *out_base = 0;
      *out_off = (u32)term_val(cell);
      return 1;
    }
  }
  // Bare non-CONST term: identity simplifier collapsed addr = base + 0
  // to just `base`.  Treat it as (base, off=0) so the F=4 lane scan
  // succeeds when the lane-0 address is the shared base.
  *out_base = addr;
  *out_off = 0;
  return 1;
}

// Backend gate for wide-load folding.  In the absence of a renderer
// flag plumbed all the way through, allow folding on float / half /
// bfloat dtypes -- the dtype gate matches tinygrad's supports_float4
// check (which is True on Metal + CUDA, False on cpu).  Callers may
// further restrict via env var THVM_DISABLE_LOAD_FOLD.
static int devec_load_fold_dtype_ok(u32 dtype) {
  if (dtype == DT_FP32) return 1;
  // dtypes for f16 / bf16 are gated off until the renderer can emit
  // half-width vector loads.
  return 0;
}

static Term devec_fold_load_stack(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  u32 n = uop_stack_n(t);
  if (n < 2 || n > 16) return 0;
  // F = 4 / 8 / 16 supported; F=2 also legal (float2).  Restrict to
  // powers of two between 2 and 8 -- the tinygrad backend default.
  if (!(n == 2 || n == 4 || n == 8)) return 0;
  Term shared_buf = 0;
  Term shared_base = 0;
  u32  base_off = 0;
  for (u32 i = 0; i < n; i++) {
    Term lane = uop_stack_src(t, i);
    Term buf, addr;
    if (!devec_extract_load_index(lane, &buf, &addr)) return 0;
    Term base;
    u32 off;
    if (!devec_extract_unit_stride(addr, &base, &off)) return 0;
    if (i == 0) {
      shared_buf = buf;
      shared_base = base;
      base_off = off;
    } else {
      if (buf != shared_buf || base != shared_base) return 0;
      if (off != base_off + i) return 0;
    }
  }
  // Dtype gate.
  u32 dt = DT_FP32;
  if (!term_dtype_in(shared_buf, 0, &dt) || !devec_load_fold_dtype_ok(dt)) {
    return 0;
  }
  // Build one wide LOAD whose INDEX_E address is (base + base_off) and
  // whose result is a vector of width n.  Since thvm's LOAD currently
  // is scalar-valued, we represent the wide load by a single LOAD over
  // INDEX_E(buf, base_addr) wrapped in UNROLL carrying a synthetic
  // (axis=0, n) so the renderer (when wired) can lower it to a
  // float<n> reinterpret_cast.  GEPs per lane recover the original
  // STACK shape; the constructor's STACK(n, [GEP(load, i)]) is the
  // bottom-up rewrite output the renderer consumes.
  Term base_addr;
  if (shared_base == 0) {
    base_addr = uop_const(DT_INT32, base_off);
  } else if (base_off == 0) {
    base_addr = shared_base;
  } else {
    Term off_const = uop_const(DT_INT32, base_off);
    base_addr = uop_int_binary(UOP_IADD, shared_base, off_const);
  }
  Term wide_idx = uop_index_e(shared_buf, base_addr);
  Term wide_load = uop_load(wide_idx);
  u32 axis_id = 0xFEu;          // synthetic "vector-lane" axis id
  u32 factor = n;
  Term wide_vector = uop_unroll(wide_load, 1, &axis_id, &factor);
  Term out[16];
  for (u32 i = 0; i < n; i++) {
    u32 idx = i;
    out[i] = uop_gep(wide_vector, 1, &idx);
  }
  return uop_stack(n, out);
}

fn Term uop_load_store_fold_graph(Term root) {
  // Pre-condition: caller has already run uop_devectorize_graph so
  // adjacent LOADs are present as STACK children.  The fold pass
  // walks STACKs bottom-up; rewriting a STACK of LOADs in place is
  // safe because the GEPs we emit point into the new wide_load --
  // pm_render's GEP-on-STACK rule would re-expand them if run on the
  // result, but we don't run pm_render after fold (the renderer
  // consumes the GEP form to emit per-lane reads).
  static UOpGraphRewriteRule const FOLD_RULES[] = {
    { "devec_fold_load_stack", devec_fold_load_stack },
  };
  return uop_graph_rewrite(root, FOLD_RULES,
                           sizeof(FOLD_RULES) / sizeof(FOLD_RULES[0]),
                           NULL);
}
