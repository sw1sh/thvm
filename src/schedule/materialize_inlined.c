// schedule/materialize_inlined.c - emit ONE kernel for a
//                                   realized UOp that inlines
//                                   all elementwise upstream
//                                   un-realized UOPs into the
//                                   same program.
//
// Used by f1d-b2 from materialize_uop_in_env when the toggle is
// on AND realize_is_realized(uop) is 1.  Children classified
// either as inputs (TEN or realized UOP_KERNEL output) or
// inlined upstream UOPs are emitted in topo order via DFS;
// inputs are deduped into the kernel's input table; inlined
// upstream UOPs land as program ops referencing earlier
// program-indices or input slots.
//
// MVP scope: ONLY elementwise + UOP_CONST get inlined.  If the
// helper meets a non-elementwise un-realized upstream UOP
// (e.g., REDUCE / EXPAND / RESHAPE), it bails (returns 0) so
// the caller falls back to the legacy per-UOp kernel emit.
// f1d-c can broaden coverage as needed.

typedef struct {
  KernelEntry *ke;
  Term emitted_terms[KPROG_MAX_OPS];
  u32  emitted_slots[KPROG_MAX_OPS];
  u32  n_emitted;
} InlineCtx;

fn u8 inline_is_inlinable(u8 op) {
  return op == UOP_ADD || op == UOP_MUL || op == UOP_NEG
      || op == UOP_RECIP || op == UOP_EXP2 || op == UOP_LOG2
      || op == UOP_SQRT  || op == UOP_CMPLT || op == UOP_CMPEQ
      || op == UOP_CONST;
}

static i32 inline_find_or_add_input(KernelEntry *ke, u32 tid,
                                    u32 dtype, u32 numel) {
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_tids[i] == tid) return (i32)i;
  }
  if (ke->n_inputs >= KERNEL_MAX_INPUT) return -1;
  u32 slot = ke->n_inputs++;
  ke->input_tids   [slot] = tid;
  ke->input_dtypes [slot] = dtype;
  ke->input_numels [slot] = numel;
  ke->input_terms  [slot] = 0;
  return (i32)slot;
}

// Returns KSRC_AS_INPUT(slot), a bare program-index, or
// 0xFFFFFFFFu on failure.
static u32 inline_emit(InlineCtx *ctx, Term t) {
  Term r = term_resolve(t);
  u8 tag = term_tag(r);

  if (tag == TAG_TEN) {
    u32 tid = (u32)term_val(r);
    if (tid == 0 || tid >= TENS_NEXT) return 0xFFFFFFFFu;
    i32 slot = inline_find_or_add_input(ctx->ke, tid,
        TENS[tid].dtype, TENS[tid].view.numel);
    if (slot < 0) return 0xFFFFFFFFu;
    return KSRC_AS_INPUT((u32)slot);
  }

  if (tag == TAG_UOP && term_ext(r) == UOP_KERNEL) {
    Term out_buf = heap_read(term_val(r));
    if (term_tag(out_buf) != TAG_TEN) return 0xFFFFFFFFu;
    u32 tid = (u32)term_val(out_buf);
    if (tid == 0 || tid >= TENS_NEXT) return 0xFFFFFFFFu;
    i32 slot = inline_find_or_add_input(ctx->ke, tid,
        TENS[tid].dtype, TENS[tid].view.numel);
    if (slot < 0) return 0xFFFFFFFFu;
    return KSRC_AS_INPUT((u32)slot);
  }

  if (tag != TAG_UOP) return 0xFFFFFFFFu;
  u8 op = term_ext(r);
  if (realize_is_realized(r)) return 0xFFFFFFFFu;   // would need its own kernel
  if (!inline_is_inlinable(op))   return 0xFFFFFFFFu;

  // Memo: same UOp instance may appear via shared subexpression.
  for (u32 i = 0; i < ctx->n_emitted; i++) {
    if (ctx->emitted_terms[i] == r) return ctx->emitted_slots[i];
  }

  u8  arity = uop_arity(op);
  u32 child_srcs  [MAX_UOP_SRC] = {0};
  u32 child_numels[MAX_UOP_SRC] = {1, 1, 1};
  for (u8 i = 0; i < arity; i++) {
    Term child = heap_read(term_val(r) + i);
    u32 src_enc = inline_emit(ctx, child);
    if (src_enc == 0xFFFFFFFFu) return 0xFFFFFFFFu;
    child_srcs[i] = src_enc;
    if (KSRC_IS_INPUT(src_enc)) {
      child_numels[i] = ctx->ke->input_numels[KSRC_INDEX(src_enc)];
    } else {
      child_numels[i] = ctx->ke->program[src_enc].numel;
    }
  }

  if (ctx->ke->n_ops  >= KPROG_MAX_OPS) return 0xFFFFFFFFu;
  if (ctx->n_emitted  >= KPROG_MAX_OPS) return 0xFFFFFFFFu;
  u32 slot = ctx->ke->n_ops++;
  KProgOp *p = &ctx->ke->program[slot];
  memset(p, 0, sizeof(*p));
  p->opcode = op;
  p->n_src  = arity;
  for (u8 i = 0; i < arity; i++) p->src[i] = child_srcs[i];

  if (op == UOP_CONST) {
    Term num = heap_read(term_val(r));
    p->arg   = (u32)term_val(num);
    p->dtype = (u8)term_ext(num);
  } else {
    p->dtype = (u8)(ctx->ke->n_inputs > 0 ? ctx->ke->input_dtypes[0] : DT_F32);
  }

  // Output numel: max of children (broadcast).  arity 0 (CONST) -> 1.
  u32 out_numel = arity > 0 ? child_numels[0] : 1;
  for (u8 i = 1; i < arity; i++) {
    if (child_numels[i] > out_numel) out_numel = child_numels[i];
  }
  p->numel = out_numel;

  ctx->emitted_terms[ctx->n_emitted] = r;
  ctx->emitted_slots[ctx->n_emitted] = slot;
  ctx->n_emitted++;
  return slot;
}

fn Term materialize_kernel_inlined(Term realized_uop_term) {
  Term r = term_resolve(realized_uop_term);
  if (term_tag(r) != TAG_UOP) return 0;
  u8 root_op = term_ext(r);
  if (root_op == UOP_KERNEL)        return r;
  if (!inline_is_inlinable(root_op)) return 0;

  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  InlineCtx ctx = {0};
  ctx.ke = ke;

  // Process root's children first via inline_emit (which honors
  // the realized check on each child), then emit the root's
  // main op explicitly so we don't trip the "realized -> bail"
  // guard against the root itself.
  u8  arity = uop_arity(root_op);
  u32 child_srcs  [MAX_UOP_SRC] = {0};
  u32 child_numels[MAX_UOP_SRC] = {1, 1, 1};
  for (u8 i = 0; i < arity; i++) {
    Term child = heap_read(term_val(r) + i);
    u32 src_enc = inline_emit(&ctx, child);
    if (src_enc == 0xFFFFFFFFu) { kernel_dealloc_last(kid); return 0; }
    child_srcs[i] = src_enc;
    if (KSRC_IS_INPUT(src_enc)) {
      child_numels[i] = ctx.ke->input_numels[KSRC_INDEX(src_enc)];
    } else {
      child_numels[i] = ctx.ke->program[src_enc].numel;
    }
  }
  if (ke->n_ops >= KPROG_MAX_OPS) { kernel_dealloc_last(kid); return 0; }
  u32 last = ke->n_ops++;
  KProgOp *root_p = &ke->program[last];
  memset(root_p, 0, sizeof(*root_p));
  root_p->opcode = root_op;
  root_p->n_src  = arity;
  for (u8 i = 0; i < arity; i++) root_p->src[i] = child_srcs[i];
  if (root_op == UOP_CONST) {
    Term num = heap_read(term_val(r));
    root_p->arg   = (u32)term_val(num);
    root_p->dtype = (u8)term_ext(num);
  } else {
    root_p->dtype = (u8)(ke->n_inputs > 0 ? ke->input_dtypes[0] : DT_F32);
  }
  u32 out_numel = arity > 0 ? child_numels[0] : 1;
  for (u8 i = 1; i < arity; i++) {
    if (child_numels[i] > out_numel) out_numel = child_numels[i];
  }
  root_p->numel = out_numel;
  ke->output_dtype = root_p->dtype;
  ke->output_numel = root_p->numel;
  // Output shape: pick from the input matching the root numel.
  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = root_p->numel;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid != 0 && tid < TENS_NEXT
        && TENS[tid].view.numel == root_p->numel) {
      ke->output_shape = TENS[tid].view.shape;
      break;
    }
  }
  ke->output_tid = tensor_alloc(CURRENT_BACKEND,
                                ke->output_shape, ke->output_dtype);
  TENS[ke->output_tid].producer_kid = kid;
  ke->source_uop = realized_uop_term;
  ke->compiled   = NULL;

  u64 kloc = heap_alloc(2);
  heap_set(kloc + 0, term_new(0, TAG_TEN, ke->output_dtype, ke->output_tid));
  heap_set(kloc + 1, term_new(0, TAG_NUM, 0, kid));
  return term_new(0, TAG_UOP, UOP_KERNEL, kloc);
}
