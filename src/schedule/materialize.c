// schedule/materialize.c - rewrite a UOp graph into a scheduled DAG of
//                          UOP_KERNEL nodes.
//
// Step 12 v1 strategy: every UOp becomes its own 1-op kernel.  No
// cross-UOp fusion (step 14 will introduce elementwise chains and a
// kernel-cache keyed by signature).  Each kernel has:
//   - N inputs (TenDescs it reads from)
//   - 1 output (a freshly allocated TenDesc)
//   - a 1-entry program[] describing the compute
//
// Children that are themselves UOps are materialized first; their
// kernel's output TenDesc becomes an input to the parent kernel.
// So calling thvm_materialize on the user's root produces a DAG of
// UOP_KERNELs wiring output->input, bottom-up.
//
// The heap layout of an emitted UOP_KERNEL term is:
//   Heap[loc + 0] = output_buf (TAG_TEN, allocated but not filled)
//   Heap[loc + 1] = NUM(kernel_id) pointing at KERNELS[kid]

// --- helpers ---

fn u8 uop_arity(u8 op) {
  switch (op) {
    case UOP_CONST:
      return 0;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
    case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
    case UOP_PAD:     case UOP_SHRINK:  case UOP_FLIP:
    case UOP_REDUCE:  case UOP_LOAD:
      return 1;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
      return 2;
    default:
      return 0;
  }
}

fn u8 uop_is_unary_elementwise(u8 op) {
  return op == UOP_NEG || op == UOP_RECIP || op == UOP_EXP2
      || op == UOP_LOG2 || op == UOP_SQRT;
}
fn u8 uop_is_binary_elementwise(u8 op) {
  return op == UOP_ADD || op == UOP_MUL || op == UOP_CMPLT || op == UOP_CMPEQ;
}

// ---- Compute output shape / dtype for a single op given its input
//      TenDesc ids.  Step 12:
//        elementwise      = broadcast pick non-scalar side,
//        REDUCE           = drop the reduced axis (rank >= 2) or
//                           collapse to {1} for 1-D inputs,
//        movement ops     = inherit for now (step 14 implements).
// ----
fn Shape op_output_shape(u8 op, u32 const *in_tids, u8 n_in, u32 reduce_axis) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = 1;

  if (n_in == 0) {
    // Leaves (CONST): shape {1}.  A CONST carries a scalar; the
    // interpreter broadcasts when combining with larger shapes.
    return s;
  }

  View *v0 = &TENS[in_tids[0]].view;
  s = v0->shape;

  if (uop_is_binary_elementwise(op) && n_in >= 2) {
    View *v1 = &TENS[in_tids[1]].view;
    if (v0->numel == 1 && v1->numel > 1) s = v1->shape;
  }

  if (op == UOP_REDUCE) {
    if (v0->shape.ndim <= 1) {
      s.ndim    = 1;
      s.dims[0] = 1;
    } else {
      // Drop reduce_axis.
      u32 dst = 0;
      for (u32 i = 0; i < v0->shape.ndim; i++) {
        if (i == reduce_axis) continue;
        s.dims[dst++] = v0->shape.dims[i];
      }
      s.ndim = dst;
      for (u32 i = dst; i < MAX_DIM; i++) s.dims[i] = 0;
    }
  }
  return s;
}

// EXPAND output shape: ndim is stored explicitly at expr_loc+1
// (see src/uop/expand.c for the heap layout), so EXPAND can
// change rank -- e.g. broadcasting a scalar/lower-rank cotangent
// to a target's higher-rank shape during backprop.  Dim sizes
// follow at expr_loc+2..expr_loc+1+ndim.
fn Shape expand_output_shape(u64 expr_loc) {
  Shape s = {0};
  u32 ndim = (u32)term_val(heap_read(expr_loc + 1));
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    Term n = heap_read(expr_loc + 2 + i);
    s.dims[i] = (u32)term_val(n);
  }
  return s;
}

fn u32 op_output_dtype(u8 op, u32 const *in_tids, u8 n_in, u32 const_dtype) {
  (void)op;
  if (n_in == 0) return const_dtype;           // CONST carries its own dtype
  return TENS[in_tids[0]].dtype;
}

// ---- core: materialize a single UOp into a fresh UOP_KERNEL term.
//      Children that are UOps are recursively materialized (bottom-up). ----
fn Term materialize_expr(Term expr) {
  // Lazy outermost-layer resolution (VAR-SUB / ALO chains) first,
  // then a single wnf step ONLY if the result still isn't
  // structurally a UOP / TEN / NUM -- which catches LAM / APP /
  // REF terms that need beta or unfolding before we can see
  // their UOp shape.  wnf is naturally lazy (stops at the first
  // WHNF root), so the call doesn't drag in the rest of the graph.
  expr = term_resolve(expr);
  u8 tag = term_tag(expr);
  if (tag != TAG_UOP && tag != TAG_TEN && tag != TAG_NUM) {
    expr = wnf(expr);
  }

  // TAG_TEN leaves: already concrete, nothing to do.
  if (term_tag(expr) != TAG_UOP) return expr;

  u8 op = term_ext(expr);

  // Already a KERNEL: pass through unchanged.
  if (op == UOP_KERNEL) return expr;

  // GRAD: reduce the chain rule first (the rewrite rule is pure;
  // it produces a UOp graph with no GRAD nodes), then materialize
  // the resulting graph.
  if (op == UOP_GRAD) return materialize_expr(interact_grad(expr));

  u64 expr_loc = term_val(expr);
  u8  arity    = uop_arity(op);

  // 1. Recursively materialize every child.  If the child materializes
  //    to a UOP_KERNEL, extract its output TAG_TEN (the first heap
  //    cell) so parents see a concrete input.
  u32 child_tids[MAX_UOP_SRC];
  for (u8 i = 0; i < arity; i++) {
    Term child = heap_read(expr_loc + i);
    Term mat   = materialize_expr(child);
    if (term_tag(mat) == TAG_UOP && term_ext(mat) == UOP_KERNEL) {
      Term out_buf = heap_read(term_val(mat));
      child_tids[i] = (u32)term_val(out_buf);
    } else if (term_tag(mat) == TAG_TEN) {
      child_tids[i] = (u32)term_val(mat);
    } else {
      // Unsupported leaf (e.g. raw NUM without a CONST wrapper).
      child_tids[i] = 0;
    }
  }

  // Per-op argument extraction from the surrounding heap cells.
  // CONST:  arg = raw bits, dtype from the NUM cell.
  // REDUCE: arg = (kind << 16) | axis, read from the kind/axis NUM cells.
  u32 const_dtype = DT_F32;
  u32 op_arg      = 0;
  if (op == UOP_CONST) {
    Term num = heap_read(expr_loc);
    op_arg      = (u32)term_val(num);
    const_dtype = term_ext(num);
  } else if (op == UOP_REDUCE) {
    u32 kind = (u32)term_val(heap_read(expr_loc + 1));
    u32 axis = (u32)term_val(heap_read(expr_loc + 2));
    op_arg   = (kind << 16) | (axis & 0xFFFF);
  } else if (op == UOP_FLIP) {
    op_arg = (u32)term_val(heap_read(expr_loc + 1));
  }

  // 2. Build a KernelEntry describing this op.
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  // Inputs: the children's TenDesc ids.  (Dedup identical inputs so
  // a + a uses one input slot with two src references.)
  for (u8 i = 0; i < arity; i++) {
    u32 tid = child_tids[i];
    i32 slot = -1;
    for (u32 j = 0; j < ke->n_inputs; j++) {
      if (ke->input_tids[j] == tid) { slot = (i32)j; break; }
    }
    if (slot < 0) {
      slot = (i32)ke->n_inputs++;
      ke->input_tids  [slot] = tid;
      ke->input_dtypes[slot] = TENS[tid].dtype;
      ke->input_numels[slot] = TENS[tid].view.numel;
    }
    // (child order preserved in program op below)
  }

  // Output shape + dtype + TenDesc.  For REDUCE the axis is in the
  // low 16 bits of the packed op_arg we filled above.  EXPAND reads
  // its target shape straight from the heap NUM cells; the default
  // op_output_shape would inherit the source view's shape and miss
  // the broadcast.
  u32   reduce_axis = (op == UOP_REDUCE) ? (op_arg & 0xFFFF) : 0;
  Shape out_shape;
  if (op == UOP_EXPAND) {
    // ndim is stored explicitly in the EXPAND heap (see
    // src/uop/expand.c); EXPAND can legitimately change rank.
    out_shape = expand_output_shape(expr_loc);
  } else if (op == UOP_RESHAPE) {
    // RESHAPE's heap layout (post the ndim-explicit fix) is
    // [src, NUM(ndim), NUM(d0), ...].  ndim is authoritative;
    // op_output_shape's default of "inherit source" is wrong
    // for any rank change.
    u32 ndim = (u32)term_val(heap_read(expr_loc + 1));
    out_shape.ndim = ndim;
    for (u32 i = 0; i < ndim && i < MAX_DIM; i++) {
      out_shape.dims[i] = (u32)term_val(heap_read(expr_loc + 2 + i));
    }
    for (u32 i = ndim; i < MAX_DIM; i++) out_shape.dims[i] = 0;
  } else if (op == UOP_PERMUTE && child_tids[0] != 0) {
    // PERMUTE: out.dim[i] = src.dim[perm[i]].
    Shape s0 = TENS[child_tids[0]].view.shape;
    out_shape.ndim = s0.ndim;
    for (u32 i = 0; i < s0.ndim && i < MAX_DIM; i++) {
      u32 pi = (u32)term_val(heap_read(expr_loc + 1 + i));
      out_shape.dims[i] = s0.dims[pi];
    }
    for (u32 i = s0.ndim; i < MAX_DIM; i++) out_shape.dims[i] = 0;
  } else if (op == UOP_PAD && child_tids[0] != 0) {
    // PAD: out.dim[i] = src.dim[i] + b_i + e_i.  Heap layout
    // [src, NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...]; ndim
    // implicit in the source.
    Shape s0 = TENS[child_tids[0]].view.shape;
    out_shape = s0;
    for (u32 i = 0; i < s0.ndim; i++) {
      u32 b = (u32)term_val(heap_read(expr_loc + 1 + 2 * i));
      u32 e = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
      out_shape.dims[i] = s0.dims[i] + b + e;
    }
  } else if (op == UOP_SHRINK && child_tids[0] != 0) {
    // SHRINK: out.dim[i] = e_i - b_i.
    Shape s0 = TENS[child_tids[0]].view.shape;
    out_shape = s0;
    for (u32 i = 0; i < s0.ndim; i++) {
      u32 b = (u32)term_val(heap_read(expr_loc + 1 + 2 * i));
      u32 e = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
      out_shape.dims[i] = (e > b) ? (e - b) : 0;
    }
  } else {
    out_shape = op_output_shape(op, child_tids, arity, reduce_axis);
  }
  u32   out_dtype   = op_output_dtype(op, child_tids, arity, const_dtype);
  ke->output_shape = out_shape;
  ke->output_dtype = out_dtype;
  u32 out_numel = shape_numel(out_shape);
  ke->output_numel = out_numel;
  ke->output_tid   = tensor_alloc(CURRENT_BACKEND, out_shape, out_dtype);
  TENS[ke->output_tid].producer_kid = kid;

  // 3. Linearize: for v1 every kernel has exactly one program op.
  //    Source slot 0 and slot 1 (if any) reference inputs by index.
  //    Repack REDUCE's op_arg from (kind << 16 | axis) to
  //    (kind << 24 | inner) -- see backend/cpu/op/reduce.c for the
  //    runtime encoding.  inner = product of input dims AFTER the
  //    reduced axis.
  if (op == UOP_REDUCE && arity > 0 && child_tids[0] != 0) {
    u32 kind = (op_arg >> 16) & 0xFFFF;
    Shape sh = TENS[child_tids[0]].view.shape;
    u32 inner = 1;
    for (u32 i = reduce_axis + 1; i < sh.ndim; i++) inner *= sh.dims[i];
    op_arg = ((kind & 0xFF) << 24) | (inner & 0x00FFFFFF);
  }
  // Prepend one LOAD instruction per input slot.  Structural marker
   // (sub-item c of the UOP_LOAD arc); the main op below still
   // references KSRC_AS_INPUT(N) directly, so backends can either
   // run cpu_op_load (memcpy to a scratch buffer; current behavior)
   // or treat LOAD as a no-op (sub-item d).
   for (u32 i = 0; i < ke->n_inputs; i++) {
     KProgOp *l = &ke->program[ke->n_ops++];
     l->opcode    = UOP_LOAD;
     l->dtype     = (u8)ke->input_dtypes[i];
     l->n_src     = 1;
     l->numel     = ke->input_numels[i];
     l->arg       = 0;
     l->src0_ndim = 0;
     l->out_ndim  = 0;
     for (u32 j = 0; j < MAX_DIM; j++) l->src0_dims[j] = 0;
     for (u32 j = 0; j < MAX_DIM; j++) l->out_dims [j] = 0;
     l->src[0]    = KSRC_AS_INPUT(i);
   }
  KProgOp *p = &ke->program[ke->n_ops++];
  p->opcode    = op;
  p->dtype     = (u8)out_dtype;
  p->n_src     = arity;
  p->numel     = out_numel;
  p->arg       = op_arg;                  // CONST bits, REDUCE kind/inner, etc.
  p->src0_ndim = 0;
  p->out_ndim  = 0;
  for (u32 i = 0; i < MAX_DIM; i++) p->src0_dims[i] = 0;
  for (u32 i = 0; i < MAX_DIM; i++) p->out_dims [i] = 0;
  for (u8 i = 0; i < arity; i++) {
    // Find which input slot this child ended up in (after dedup).
    for (u32 j = 0; j < ke->n_inputs; j++) {
      if (ke->input_tids[j] == child_tids[i]) {
        p->src[i] = KSRC_AS_INPUT(j);
        break;
      }
    }
  }

  // Movement ops (currently EXPAND, FLIP) need source slot 0's
  // per-axis shape AND the output's per-axis shape so the kernel
  // can resolve broadcast indexing (EXPAND) or per-axis mirroring
  // (FLIP) without having to re-derive it from in_numel / out_numel
  // alone.
  if ((op == UOP_EXPAND || op == UOP_FLIP || op == UOP_PAD
    || op == UOP_PERMUTE || op == UOP_SHRINK)
   && arity > 0 && child_tids[0] != 0) {
    Shape s0 = TENS[child_tids[0]].view.shape;
    p->src0_ndim = (u8)(s0.ndim & 0xFF);
    for (u32 i = 0; i < s0.ndim && i < MAX_DIM; i++) {
      p->src0_dims[i] = s0.dims[i];
    }
    p->out_ndim = (u8)(out_shape.ndim & 0xFF);
    for (u32 i = 0; i < out_shape.ndim && i < MAX_DIM; i++) {
      p->out_dims[i] = out_shape.dims[i];
    }
  }
  if ((op == UOP_PAD || op == UOP_SHRINK) && arity > 0
   && child_tids[0] != 0) {
    Shape s0 = TENS[child_tids[0]].view.shape;
    for (u32 i = 0; i < s0.ndim && i < MAX_DIM; i++) {
      u32 b = (u32)term_val(heap_read(expr_loc + 1 + 2 * i));
      u32 e = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
      p->pad_widths[2 * i + 0] = (u8)(b & 0xFF);
      p->pad_widths[2 * i + 1] = (u8)(e & 0xFF);
    }
  }
  if (op == UOP_PERMUTE && arity > 0 && child_tids[0] != 0) {
    Shape s0 = TENS[child_tids[0]].view.shape;
    for (u32 i = 0; i < s0.ndim && i < MAX_DIM; i++) {
      u32 pi = (u32)term_val(heap_read(expr_loc + 1 + i));
      p->axis_perm[i] = (u8)(pi & 0xFF);
    }
  }

  // 4. Emit UOP_KERNEL term:  [output_buf, NUM(kernel_id)].
  u64 kloc = heap_alloc(2);
  heap_set(kloc + 0, term_new(0, TAG_TEN, out_dtype, ke->output_tid));
  heap_set(kloc + 1, term_new(0, TAG_NUM, DT_I32,   kid));

  ke->compiled   = NULL;                  // interpreter fallback
  ke->source_uop = expr;                  // for grad walks via the original UOp
  return term_new(0, TAG_UOP, UOP_KERNEL, kloc);
}

fn Term thvm_materialize(Term term) {
  // Heap-walk materializer: scans reachable cells (through LAM / APP /
  // REF / ALO / UOP etc.), propagates shapes through APP-LAM, and in-
  // place rewrites UOP cells to UOP_KERNEL cells.  For UOPs that
  // couldn't be kernelized (e.g. VAR child with no shape binding yet),
  // the walker leaves them alone and materialize_expr picks up the
  // slack on whatever's still reachable at the root afterwards.
  if (term_tag(term) == TAG_TEN) return term;
  if (term_tag(term) == TAG_UOP && term_ext(term) == UOP_KERNEL) return term;
  Term walked = materialize_walk(term);
  if (term_tag(walked) == TAG_TEN) return walked;
  if (term_tag(walked) == TAG_UOP && term_ext(walked) == UOP_KERNEL) return walked;
  return materialize_expr(walked);
}
