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
// So reducing a UOP_MATERIALIZE on the user's root produces a DAG
// of UOP_KERNELs wiring output->input, bottom-up.
//
// The heap layout of an emitted UOP_KERNEL term is:
//   Heap[loc + 0] = output_buf (TAG_TEN, allocated but not filled)
//   Heap[loc + 1] = NUM(kernel_id) pointing at KERNELS[kid]

// --- helpers ---

fn u8 uop_is_leaf(u8 op) {
  return op == UOP_CONST;
}

fn u8 uop_arity(u8 op) {
  switch (op) {
    case UOP_CONST:
      return 0;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
    case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
    case UOP_PAD:     case UOP_SHRINK:  case UOP_FLIP:
    case UOP_REDUCE:
      return 1;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT:
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
  return op == UOP_ADD || op == UOP_MUL || op == UOP_CMPLT;
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

fn u32 op_output_dtype(u8 op, u32 const *in_tids, u8 n_in, u32 const_dtype) {
  (void)op;
  if (n_in == 0) return const_dtype;           // CONST carries its own dtype
  return TENS[in_tids[0]].dtype;
}

// ---- core: materialize a single UOp into a fresh UOP_KERNEL term.
//      Children that are UOps are recursively materialized (bottom-up). ----
fn Term materialize_expr(Term expr) {
  // TAG_TEN leaves: already concrete, nothing to do.
  if (term_tag(expr) != TAG_UOP) return expr;

  u8 op = term_ext(expr);

  // Nested UOP_MATERIALIZE: recurse through.
  if (op == UOP_MATERIALIZE) {
    Term inner = heap_read(term_val(expr));
    return materialize_expr(inner);
  }

  // Already a KERNEL: pass through unchanged.
  if (op == UOP_KERNEL) return expr;

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
  // low 16 bits of the packed op_arg we filled above.
  u32   reduce_axis = (op == UOP_REDUCE) ? (op_arg & 0xFFFF) : 0;
  Shape out_shape   = op_output_shape(op, child_tids, arity, reduce_axis);
  u32   out_dtype   = op_output_dtype(op, child_tids, arity, const_dtype);
  ke->output_shape = out_shape;
  ke->output_dtype = out_dtype;
  u32 out_numel = 1;
  for (u32 i = 0; i < out_shape.ndim; i++) out_numel *= out_shape.dims[i];
  ke->output_numel = out_numel;
  ke->output_tid   = tensor_alloc(CURRENT_BACKEND, out_shape, out_dtype);
  TENS[ke->output_tid].producer_kid = kid;

  // 3. Linearize: for v1 every kernel has exactly one program op.
  //    Source slot 0 and slot 1 (if any) reference inputs by index.
  KProgOp *p = &ke->program[ke->n_ops++];
  p->opcode = op;
  p->dtype  = (u8)out_dtype;
  p->n_src  = arity;
  p->numel  = out_numel;
  p->arg    = op_arg;                     // CONST bits, REDUCE kind/axis, etc.
  for (u8 i = 0; i < arity; i++) {
    // Find which input slot this child ended up in (after dedup).
    for (u32 j = 0; j < ke->n_inputs; j++) {
      if (ke->input_tids[j] == child_tids[i]) {
        p->src[i] = KSRC_AS_INPUT(j);
        break;
      }
    }
  }

  // 4. Emit UOP_KERNEL term:  [output_buf, NUM(kernel_id)].
  u64 kloc = heap_alloc(2);
  heap_set(kloc + 0, term_new(0, TAG_TEN, out_dtype, ke->output_tid));
  heap_set(kloc + 1, term_new(0, TAG_NUM, DT_I32,   kid));

  ke->compiled = NULL;                    // interpreter fallback
  return term_new(0, TAG_UOP, UOP_KERNEL, kloc);
}

fn Term thvm_materialize(Term term) {
  // If it's a UOP_MATERIALIZE wrapper, unwrap and materialize its payload.
  // Otherwise materialize the term directly (useful for programmatic use).
  if (term_tag(term) == TAG_UOP && term_ext(term) == UOP_MATERIALIZE) {
    return materialize_expr(heap_read(term_val(term)));
  }
  return materialize_expr(term);
}
