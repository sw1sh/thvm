// interact/uop_grad.c - dup-like chain-rule rewrite for UOP_GRAD.
//
// UOP_GRAD and UOP_FWD form a 3-port combinator: one principal port
// (the cell's `y`) and two aux ports (FWD = forward projection,
// BWD = backward projection a.k.a. UOP_GRAD).  Each lazy chain-rule
// fire walks one structural step on y's outermost UOp:
//
//   GRAD(ADD(a, b))   -> { ADD(fw_a, fw_b),    ADD(bw_a, bw_b) }
//   GRAD(MUL(a, b))   -> { MUL(fw_a, fw_b),    ADD(MUL(fw_b, bw_a),
//                                                   MUL(fw_a, bw_b)) }
//   GRAD(NEG(a))      -> { NEG(fw_a),          NEG(bw_a) }
//   GRAD(LOG2(a))     -> { LOG2(fw_a),         MUL(bw_a, MUL(RECIP(fw_a),
//                                                            CONST(1/ln2))) }
//   GRAD(EXP2(a))     -> { EXP2(fw_a),         MUL(bw_a, MUL(EXP2(fw_a),
//                                                            CONST(ln2))) }
//   GRAD(RECIP(a))    -> { RECIP(fw_a),        MUL(bw_a, NEG(MUL(RECIP(fw_a),
//                                                                RECIP(fw_a)))) }
//   GRAD(SQRT(a))     -> { SQRT(fw_a),         MUL(bw_a, MUL(CONST(0.5),
//                                                            RECIP(SQRT(fw_a)))) }
//   GRAD(SUM(a, ax))  -> { SUM(fw_a, ax),      EXPAND(bw_a, a.shape) }
//   GRAD(MAX(a, ax))  -> { MAX(fw_a, ax),      MUL(MASK(a, MAX(a, ax)),
//                                                  EXPAND(bw_a, a.shape)) }
//   GRAD(EXPAND(a))   -> { EXPAND(fw_a, ...),  REDUCE_SUM(bw_a, expanded_ax) }
//   GRAD(RESHAPE(a))  -> { RESHAPE(fw_a, ...), RESHAPE(bw_a, a.shape) }
//   GRAD(PAD(a))      -> { PAD(fw_a, ...),     SHRINK(bw_a, b, b+a.shape) }
//   GRAD(SHRINK(a))   -> { SHRINK(fw_a, ...),  PAD(bw_a, b, src_dim - e) }
//   GRAD(FLIP(a))     -> { FLIP(fw_a, mask),   FLIP(bw_a, mask) }
//   GRAD(PERMUTE(a))  -> { PERMUTE(fw_a, p),   PERMUTE(bw_a, inv_p) }
//   GRAD(TEN_t)       -> { TEN_t,              SUP^{t.tid}(zero, one) }
//
// where {fw_a, bw_a} = GRAD(a), built fresh per child via a new cell
// holding [a].  Sibling forwards reach via UOP_FWD projections;
// `bw` references reach via UOP_GRAD projections of the same cells.
//
// The fire is ATOMIC over the dup-like pair: it computes both new_fw
// and new_bw, then heap_replaces UOP_FWD(orig_cell) -> new_fw and
// returns new_bw to the caller (which heap_replaces UOP_GRAD(orig_cell)
// -> new_bw).
//
// No gy threading, no target stored.  Target identification happens
// at the leaf via SUP^{leaf_tid}(zero, one); the WL surface DUPs the
// result by the requested target's tid to project the gradient.

// Lift `src` to a tensor's shape via UOP_EXPAND.  No-op when src
// already has that shape OR target_term isn't a TAG_TEN with rank.
fn Term expand_to_target(Term src, Term target_term) {
  if (term_tag(target_term) != TAG_TEN) return src;
  u32 tid = term_val(target_term);
  TenDesc *desc = &TENS[tid];
  if (desc->view.shape.ndim == 0) return src;
  return uop_expand(src, desc->view.shape.ndim, desc->view.shape.dims);
}

fn Term grad_zero_at(Term y) {
  return expand_to_target(uop_const(DT_F32, 0), y);
}

fn Term grad_one_at(Term y) {
  // 1.0f in IEEE-754 = 0x3F800000.
  return expand_to_target(uop_const(DT_F32, 0x3F800000u), y);
}

// Allocate a fresh GRAD cell for `child` and return its FWD/BWD
// projection terms.  Both projections share the cell.
typedef struct { Term fwd; Term bwd; } GradPair;
static GradPair grad_pair(Term child) {
  u64 c = uop_grad_cell(child);
  GradPair p;
  p.fwd = term_new(0, TAG_UOP, UOP_FWD,  c);
  p.bwd = term_new(0, TAG_UOP, UOP_GRAD, c);
  return p;
}

fn Term interact_grad(Term grad_term) {
  u64  cell_orig = term_val(grad_term);
  Term y         = term_resolve(heap_read(cell_orig));

  u8 y_tag = term_tag(y);

  // === LEAF: y is a TEN ===
  // Emit SUP^{y.tid}(zero, one) shape-lifted to y's shape; the WL DUP^t
  // projects the match side iff t == y.tid.  The companion FWD
  // projection just resolves to y itself.
  if (y_tag == TAG_TEN) {
    u32 tid = (u32)term_val(y);
    Term zero = grad_zero_at(y);
    Term one  = grad_one_at(y);
    u64  sloc = heap_alloc(2);
    heap_set(sloc + 0, zero);
    heap_set(sloc + 1, one);
    Term sup = term_new(0, TAG_SUP, tid, sloc);
    return sup;
  }

  // NUM as a constant -- no gradient anywhere; emit zero on bw.
  if (y_tag == TAG_NUM) {
    return uop_const(DT_F32, 0);
  }

  // y not a UOP we can pattern-match -- leave the GRAD as WHNF.
  if (y_tag != TAG_UOP) return grad_term;

  u8  y_op  = term_ext(y);
  u64 y_loc = term_val(y);

  switch (y_op) {

    // === Elementwise binary ===
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ: {
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      GradPair ga = grad_pair(a);
      GradPair gb = grad_pair(b);

      Term new_fw = uop_binary(y_op, ga.fwd, gb.fwd);
      Term new_bw;
      if (y_op == UOP_ADD) {
        new_bw = uop_binary(UOP_ADD, ga.bwd, gb.bwd);
      } else if (y_op == UOP_MUL) {
        Term l = uop_binary(UOP_MUL, gb.fwd, ga.bwd);
        Term r = uop_binary(UOP_MUL, ga.fwd, gb.bwd);
        new_bw = uop_binary(UOP_ADD, l, r);
      } else {
        // CMPLT / CMPEQ are non-differentiable -- bw is zero.
        new_bw = grad_zero_at(y);
      }
      return new_bw;
    }

    // === Elementwise unary ===
    case UOP_NEG: {
      Term a = heap_read(y_loc + 0);
      GradPair ga = grad_pair(a);
      Term new_fw = uop_unary(UOP_NEG, ga.fwd);
      Term new_bw = uop_unary(UOP_NEG, ga.bwd);
      return new_bw;
    }

    case UOP_LOG2: {
      // d(log2 a)/dx = (1/(a*ln 2)) * da/dx
      f32 inv_ln2 = 1.4426950408889634f;
      u32 inv_bits;
      memcpy(&inv_bits, &inv_ln2, sizeof inv_bits);
      Term a = heap_read(y_loc + 0);
      GradPair ga = grad_pair(a);
      Term new_fw = uop_unary(UOP_LOG2, ga.fwd);
      Term ra     = uop_unary(UOP_RECIP, ga.fwd);
      Term k      = uop_const(DT_F32, inv_bits);
      Term factor = uop_binary(UOP_MUL, ra, k);
      Term new_bw = uop_binary(UOP_MUL, ga.bwd, factor);
      return new_bw;
    }

    case UOP_EXP2: {
      // d(2^a)/dx = (2^a * ln 2) * da/dx
      f32 ln2 = 0.6931471805599453f;
      u32 ln2_bits;
      memcpy(&ln2_bits, &ln2, sizeof ln2_bits);
      Term a = heap_read(y_loc + 0);
      GradPair ga = grad_pair(a);
      Term new_fw = uop_unary(UOP_EXP2, ga.fwd);
      Term k      = uop_const(DT_F32, ln2_bits);
      Term factor = uop_binary(UOP_MUL, new_fw, k);
      Term new_bw = uop_binary(UOP_MUL, ga.bwd, factor);
      return new_bw;
    }

    case UOP_RECIP: {
      // d(1/a)/dx = (-1/a^2) * da/dx
      Term a = heap_read(y_loc + 0);
      GradPair ga = grad_pair(a);
      Term new_fw  = uop_unary(UOP_RECIP, ga.fwd);
      // Two independent RECIP nodes so materialize doesn't think
      // they're a shared computation (we want the same computed value
      // re-read, but structurally separate so the diagram is clean).
      Term ra1     = uop_unary(UOP_RECIP, ga.fwd);
      Term ra2     = uop_unary(UOP_RECIP, ga.fwd);
      Term sq      = uop_binary(UOP_MUL, ra1, ra2);
      Term nsq     = uop_unary(UOP_NEG, sq);
      Term new_bw  = uop_binary(UOP_MUL, ga.bwd, nsq);
      return new_bw;
    }

    case UOP_SQRT: {
      // d(sqrt a)/dx = (1/(2*sqrt a)) * da/dx
      Term a = heap_read(y_loc + 0);
      GradPair ga = grad_pair(a);
      Term sa     = uop_unary(UOP_SQRT, ga.fwd);
      Term new_fw = sa;
      Term inv_sa = uop_unary(UOP_RECIP, sa);
      f32 half = 0.5f;
      u32 half_bits;
      memcpy(&half_bits, &half, sizeof half_bits);
      Term k      = uop_const(DT_F32, half_bits);
      Term factor = uop_binary(UOP_MUL, inv_sa, k);
      Term new_bw = uop_binary(UOP_MUL, ga.bwd, factor);
      return new_bw;
    }

    // === REDUCE ===
    case UOP_REDUCE: {
      Term a    = heap_read(y_loc + 0);
      u32  kind = (u32)term_val(heap_read(y_loc + 1));
      u32  axis = (u32)term_val(heap_read(y_loc + 2));
      GradPair ga = grad_pair(a);
      Term new_fw = uop_reduce(kind, axis, ga.fwd);

      Shape a_shape;
      Term new_bw;
      if (kind == REDUCE_SUM && term_shape_in(a, 0, &a_shape) && a_shape.ndim > 0) {
        // bw_outer = EXPAND bw_a back along the reduce axis.
        new_bw = uop_expand(ga.bwd, a_shape.ndim, a_shape.dims);
      } else if (kind == REDUCE_MAX && term_shape_in(a, 0, &a_shape) && a_shape.ndim > 0) {
        // bw_outer = mask(a == max) * EXPAND(bw_a, a.shape).
        u32 keep_dim_shape[MAX_DIM] = {0};
        for (u32 i = 0; i < a_shape.ndim; i++)
          keep_dim_shape[i] = (i == axis) ? 1u : a_shape.dims[i];
        Term mx        = uop_reduce(REDUCE_MAX, axis, ga.fwd);
        Term mx_keep   = uop_reshape(mx, a_shape.ndim, keep_dim_shape);
        Term mx_lifted = uop_expand(mx_keep, a_shape.ndim, a_shape.dims);
        Term mask      = uop_binary(UOP_CMPEQ, ga.fwd, mx_lifted);
        Term gy_lifted = uop_expand(ga.bwd, a_shape.ndim, a_shape.dims);
        new_bw         = uop_binary(UOP_MUL, gy_lifted, mask);
      } else {
        new_bw = ga.bwd;   // best effort -- passthrough
      }
      return new_bw;
    }

    // === Movement ops ===
    case UOP_RESHAPE: {
      Term a = heap_read(y_loc + 0);
      u32  ndim = (u32)term_val(heap_read(y_loc + 1));
      u32  out_dims[MAX_DIM];
      for (u32 i = 0; i < ndim; i++)
        out_dims[i] = (u32)term_val(heap_read(y_loc + 2 + i));
      GradPair ga = grad_pair(a);
      Term new_fw = uop_reshape(ga.fwd, ndim, out_dims);
      // bw_outer = RESHAPE(bw_a, a.shape).  bw_a has whatever shape
      // GRAD(a) gave; if a is a TEN target leaf, bw_a has a.shape
      // already and RESHAPE is identity-ish.  Otherwise we try to
      // infer a's shape via term_shape_in.
      Shape a_shape;
      Term new_bw;
      if (term_shape_in(a, 0, &a_shape) && a_shape.ndim > 0) {
        new_bw = uop_reshape(ga.bwd, a_shape.ndim, a_shape.dims);
      } else {
        new_bw = ga.bwd;   // passthrough
      }
      return new_bw;
    }

    case UOP_EXPAND: {
      Term a = heap_read(y_loc + 0);
      u32  ndim = (u32)term_val(heap_read(y_loc + 1));
      u32  out_dims[MAX_DIM];
      for (u32 i = 0; i < ndim; i++)
        out_dims[i] = (u32)term_val(heap_read(y_loc + 2 + i));
      GradPair ga = grad_pair(a);
      Term new_fw = uop_expand(ga.fwd, ndim, out_dims);

      // bw_outer: REDUCE_SUM along axes that were expanded (where
      // src.dim == 1 < out.dim).  bw_a is at a's shape.
      Shape src_shape;
      Term new_bw = ga.bwd;
      if (term_shape_in(a, 0, &src_shape) && src_shape.ndim > 0) {
        // First lift bw_a to expand-output shape so the per-axis
        // REDUCE_SUMs see a well-defined source extent.
        new_bw = uop_expand(new_bw, ndim, out_dims);
        for (i32 axis = (i32)src_shape.ndim - 1; axis >= 0; axis--) {
          if (src_shape.dims[axis] == 1 && out_dims[axis] > 1) {
            new_bw = uop_reduce(REDUCE_SUM, (u32)axis, new_bw);
          }
        }
      }
      return new_bw;
    }

    case UOP_FLIP: {
      Term a = heap_read(y_loc + 0);
      u32 mask = (u32)term_val(heap_read(y_loc + 1));
      GradPair ga = grad_pair(a);
      Term new_fw = uop_flip(ga.fwd, mask);
      Term new_bw = uop_flip(ga.bwd, mask);
      return new_bw;
    }

    case UOP_PERMUTE: {
      // ndim is encoded in ext's high bits per the comment; here we
      // can't easily decode that without a dedicated helper.  Fall
      // back to passthrough on bw and rely on shape-inference at
      // materialize time.  TODO: handle properly.
      Term a = heap_read(y_loc + 0);
      GradPair ga = grad_pair(a);
      // We don't know permutation rank here without decoding ext;
      // bail (return WHNF) for now.
      (void)ga;
      return grad_term;
    }

    case UOP_PAD: case UOP_SHRINK: {
      // PAD/SHRINK have the same heap layout: [src, NUM(b0), NUM(e0),
      // ..., NUM(b_{ndim-1}), NUM(e_{ndim-1})].  ndim isn't directly
      // stored -- we'd need to derive it from a's shape.
      Term a = heap_read(y_loc + 0);
      Shape a_shape;
      if (!term_shape_in(a, 0, &a_shape) || a_shape.ndim == 0) {
        return grad_term;   // can't determine ndim -- bail
      }
      GradPair ga = grad_pair(a);

      u32 ndim = a_shape.ndim;
      u32 ranges[2 * MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        u32 b = (u32)term_val(heap_read(y_loc + 1 + 2 * i));
        u32 e = (u32)term_val(heap_read(y_loc + 2 + 2 * i));
        ranges[2 * i + 0] = b;
        ranges[2 * i + 1] = e;
      }

      Term new_fw, new_bw;
      if (y_op == UOP_PAD) {
        // PAD output dims = src.dim + b + e
        u32 out_dims[MAX_DIM];
        for (u32 i = 0; i < ndim; i++)
          out_dims[i] = a_shape.dims[i] + ranges[2*i] + ranges[2*i + 1];
        new_fw = uop_pad(ga.fwd, ndim, ranges);
        // bw_outer: SHRINK bw_a to the un-padded extent [b, b+a.dim).
        u32 sranges[2 * MAX_DIM];
        for (u32 i = 0; i < ndim; i++) {
          sranges[2*i + 0] = ranges[2*i];
          sranges[2*i + 1] = ranges[2*i] + a_shape.dims[i];
        }
        // First lift bw_a to PAD's output shape.
        Term lifted = uop_expand(ga.bwd, ndim, out_dims);
        new_bw      = uop_shrink(lifted, ndim, sranges);
      } else {
        // SHRINK extracts [b, e); output dim = e - b.
        u32 out_dims[MAX_DIM];
        for (u32 i = 0; i < ndim; i++)
          out_dims[i] = (ranges[2*i + 1] > ranges[2*i])
                           ? (ranges[2*i + 1] - ranges[2*i]) : 0;
        new_fw = uop_shrink(ga.fwd, ndim, ranges);
        // bw_outer: PAD bw_a back to source extent with widths
        // [b_i, src.dim_i - e_i].
        u32 widths[2 * MAX_DIM];
        for (u32 i = 0; i < ndim; i++) {
          widths[2*i + 0] = ranges[2*i];
          widths[2*i + 1] = (a_shape.dims[i] > ranges[2*i + 1])
                              ? (a_shape.dims[i] - ranges[2*i + 1]) : 0;
        }
        Term lifted = uop_expand(ga.bwd, ndim, out_dims);
        new_bw      = uop_pad(lifted, ndim, widths);
      }
      return new_bw;
    }

    case UOP_KERNEL: {
      // KERNEL: chain rule should run on the pre-kernelize source UOp,
      // recovered from KernelEntry.source_uop.  The walker mutates
      // parent cells to UOP_KERNEL but leaves the source UOP intact
      // (orphaned) on the heap.
      u32 kid = (u32)term_val(heap_read(y_loc + 1));
      if (kid == 0 || kid >= KERNELS_NEXT) {
        return grad_zero_at(y);
      }
      Term src = KERNELS[kid].source_uop;
      if (src == 0) {
        return grad_zero_at(y);
      }
      GradPair ga = grad_pair(src);
      return ga.bwd;
    }

    case UOP_CONST:
    case UOP_LOAD:
    case UOP_ASSIGN:
      // Not differentiable -- bw is zero.
      return grad_zero_at(y);

    default:
      return grad_term;
  }
}
