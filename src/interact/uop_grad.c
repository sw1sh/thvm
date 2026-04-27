// interact/uop_grad.c - dup-like chain-rule rewrite for UOP_GRAD.
//
// UOP_GRAD and UOP_FWD form a 3-port combinator: one principal port
// (the cell's `y`) and two aux ports (FWD = forward projection,
// BWD = backward projection a.k.a. UOP_GRAD).  Each lazy chain-rule
// fire steps through y's outermost UOp:
//
//   GRAD(ADD(a, b))   -> { ADD(fw_a, fw_b),    ADD(bw_a, bw_b) }
//   GRAD(MUL(a, b))   -> { MUL(fw_a, fw_b),    ADD(MUL(fw_b, bw_a),
//                                                   MUL(fw_a, bw_b)) }
//   GRAD(NEG(a))      -> { NEG(fw_a),          NEG(bw_a) }
//   GRAD(SUM(a, ax))  -> { SUM(fw_a, ax),      EXPAND(bw_a, src.shape) }
//   GRAD(EXPAND(a,s)) -> { EXPAND(fw_a, s),    REDUCE_SUM(bw_a, ax_set) }
//   GRAD(RESHAPE(a,s))-> { RESHAPE(fw_a, s),   RESHAPE(bw_a, a.shape) }
//   GRAD(TEN_t)       -> { TEN_t,              SUP^{t.tid}(0, 1) }
//
// where {fw_a, bw_a} = GRAD(a), built fresh per child via a new cell
// holding [a].  Sibling forwards reach via UOP_FWD projections;
// `bw` references reach via UOP_GRAD projections of the same cells.
//
// The fire is ATOMIC over the dup-like pair: it computes both new_fw
// and new_bw, then heap_replaces UOP_FWD(orig_cell) -> new_fw and
// returns new_bw to the caller (which heap_replaces UOP_GRAD(orig_cell)
// -> new_bw).  Whichever projection was forced first triggers the
// rewrite; the other is rewritten as a side-effect.
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

// Atomically rewrite the FWD projection paired with this BWD cell.
// The caller (redex_fire) heap_replaces UOP_GRAD(cell_orig) with our
// returned new_bw; we additionally heap_replace UOP_FWD(cell_orig)
// with new_fw so the dup-like pair stays in sync.
fn void grad_replace_fwd(u64 cell_orig, Term new_fw) {
  Term old_fwd = term_new(0, TAG_UOP, UOP_FWD, cell_orig);
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    if (heap_read(i) == old_fwd) heap_set(i, new_fw);
  }
}

fn Term interact_grad(Term grad_term) {
  u64  cell_orig = term_val(grad_term);
  Term y         = term_resolve(heap_read(cell_orig));

  u8 y_tag = term_tag(y);

  // === LEAF: y is a TEN ===
  // Emit SUP^{y.tid}(0, 1) shape-lifted to y's shape; the WL DUP^t
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
    grad_replace_fwd(cell_orig, y);
    return sup;
  }

  // NUM as a constant -- no gradient anywhere; emit zero on bw.
  if (y_tag == TAG_NUM) {
    grad_replace_fwd(cell_orig, y);
    return uop_const(DT_F32, 0);
  }

  // y not a UOP we can pattern-match -- leave the GRAD as WHNF.
  if (y_tag != TAG_UOP) return grad_term;

  u8  y_op  = term_ext(y);
  u64 y_loc = term_val(y);

  // Helper to allocate a fresh grad cell for a child term and return
  // its FWD/BWD projection terms.  Both projections share the cell.
  // (We inline rather than defining a static helper because uop_grad.c
  // already exposes uop_grad_cell.)

  switch (y_op) {

    // === Elementwise binary ===
    case UOP_ADD: case UOP_MUL:
    case UOP_CMPLT: case UOP_CMPEQ: {
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      u64  ca = uop_grad_cell(a);
      u64  cb = uop_grad_cell(b);
      Term fa = term_new(0, TAG_UOP, UOP_FWD,  ca);
      Term fb = term_new(0, TAG_UOP, UOP_FWD,  cb);
      Term ba = term_new(0, TAG_UOP, UOP_GRAD, ca);
      Term bb = term_new(0, TAG_UOP, UOP_GRAD, cb);

      Term new_fw = uop_binary(y_op, fa, fb);
      Term new_bw;
      if (y_op == UOP_ADD) {
        new_bw = uop_binary(UOP_ADD, ba, bb);
      } else if (y_op == UOP_MUL) {
        // d(a*b)/dt = b*da/dt + a*db/dt -- via FWD projections of
        // the same cells so the forward subgraph is shared.
        Term l = uop_binary(UOP_MUL, fb, ba);
        Term r = uop_binary(UOP_MUL, fa, bb);
        new_bw = uop_binary(UOP_ADD, l, r);
      } else {
        // CMPLT / CMPEQ are non-differentiable.
        new_bw = grad_zero_at(y);
      }
      grad_replace_fwd(cell_orig, new_fw);
      return new_bw;
    }

    // === Elementwise unary ===
    case UOP_NEG: {
      Term a  = heap_read(y_loc + 0);
      u64  ca = uop_grad_cell(a);
      Term fa = term_new(0, TAG_UOP, UOP_FWD,  ca);
      Term ba = term_new(0, TAG_UOP, UOP_GRAD, ca);
      Term new_fw = uop_unary(UOP_NEG, fa);
      Term new_bw = uop_unary(UOP_NEG, ba);
      grad_replace_fwd(cell_orig, new_fw);
      return new_bw;
    }

    // RECIP / EXP2 / LOG2 / SQRT chain rules deferred (TODO -- need
    // rewrites with local-Jacobian shapes via FWD projections).
    case UOP_RECIP: case UOP_EXP2: case UOP_LOG2: case UOP_SQRT:
      return grad_term;   // stuck

    // === REDUCE ===
    case UOP_REDUCE: {
      Term a    = heap_read(y_loc + 0);
      u32  kind = (u32)term_val(heap_read(y_loc + 1));
      u32  axis = (u32)term_val(heap_read(y_loc + 2));
      u64  ca   = uop_grad_cell(a);
      Term fa   = term_new(0, TAG_UOP, UOP_FWD,  ca);
      Term ba   = term_new(0, TAG_UOP, UOP_GRAD, ca);
      Term new_fw = uop_reduce(kind, axis, fa);
      // bw of REDUCE (SUM): broadcast back along the reduce axis.
      // Use term_shape_in to discover a's shape; if unavailable,
      // leave as expand_to_target(bw_a, a) which is a no-op for
      // non-TEN a.
      Shape a_shape;
      Term new_bw;
      if (kind == REDUCE_SUM && term_shape_in(a, 0, &a_shape) && a_shape.ndim > 0) {
        // EXPAND bw_a back to a's full shape (size-1 at the reduced
        // axis would be implicit -- we pass the full a.dims).
        new_bw = uop_expand(ba, a_shape.ndim, a_shape.dims);
      } else {
        new_bw = ba;   // best effort -- passthrough
      }
      grad_replace_fwd(cell_orig, new_fw);
      return new_bw;
    }

    // === Movement ops (passthrough fw, mirror bw) ===
    case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
    case UOP_PAD:     case UOP_SHRINK:  case UOP_FLIP:
      // Deferred -- need to emit the inverse movement in bw.  Leave
      // as WHNF for now; consumer can re-fire later.
      return grad_term;

    case UOP_KERNEL:
    case UOP_CONST:
    case UOP_LOAD:
    case UOP_ASSIGN:
      // KERNEL: post-materialize -- chain rule should happen on the
      // pre-kernelize source UOp (KernelEntry.source_uop).  Deferred.
      // CONST/LOAD/ASSIGN: not differentiable in this context.
      grad_replace_fwd(cell_orig, y);
      return uop_const(DT_F32, 0);

    default:
      return grad_term;
  }
}
