// schedule/materialize_in_env.c - kernelize one UOP with shape env.
//
// Companion to materialize_expr that knows how to handle a child
// term resolving to a free TAG_VAR -- it stores the VAR as a
// symbolic input slot (KernelEntry.input_terms[i]) and looks the
// shape/dtype up via the active shape env for output planning.
// kernel_fire_by_id later resolves each symbolic slot via
// term_resolve to find the now-substituted TAG_TEN.
//
// Returns the UOP_KERNEL term on success, or the original UOP
// term unchanged when the children's shapes can't be derived
// even with env help (the walker leaves it alone for a later
// pass to retry).

// Classify a child for kernelization purposes.
typedef enum {
    CHILD_CONCRETE_TEN,    // input_tids[i] = ten id
    CHILD_KERNEL_OUT,      // child is UOP_KERNEL; pull its output tid
    CHILD_SYMBOLIC_VAR,    // input_terms[i] = VAR term, env gives shape
    CHILD_UNKNOWN          // can't classify -- fail
} ChildKind;

static ChildKind classify_child(Term child, u32 env_id, u32 *out_tid,
                                Term *out_term, Shape *out_shape, u32 *out_dtype) {
    Term r = term_resolve(child);
    u8 tag = term_tag(r);
    if (tag == TAG_TEN) {
        u32 tid = (u32)term_val(r);
        if (tid == 0 || tid >= TENS_NEXT) return CHILD_UNKNOWN;
        *out_tid   = tid;
        *out_shape = TENS[tid].view.shape;
        *out_dtype = TENS[tid].dtype;
        return CHILD_CONCRETE_TEN;
    }
    if (tag == TAG_UOP && term_ext(r) == UOP_KERNEL) {
        Term outbuf = heap_read(term_val(r));
        if (term_tag(outbuf) != TAG_TEN) return CHILD_UNKNOWN;
        u32 tid = (u32)term_val(outbuf);
        if (tid == 0 || tid >= TENS_NEXT) return CHILD_UNKNOWN;
        *out_tid   = tid;
        *out_shape = TENS[tid].view.shape;
        *out_dtype = TENS[tid].dtype;
        return CHILD_KERNEL_OUT;
    }
    if (tag == TAG_VAR) {
        Shape s;
        if (!shape_env_lookup(env_id, term_val(r), &s)) return CHILD_UNKNOWN;
        *out_term  = r;
        *out_shape = s;
        *out_dtype = DT_F32;     // assume f32 until fire-time tells us otherwise
        return CHILD_SYMBOLIC_VAR;
    }
    return CHILD_UNKNOWN;
}

// Build a kernel for a single UOP node.  Children must already be
// shape-known (concrete or VAR-with-env-binding).
fn Term materialize_uop_in_env(Term uop, u32 env_id) {
    if (term_tag(uop) != TAG_UOP) return uop;
    u32 op = term_ext(uop);
    // Meta ops the walker shouldn't kernelize:
    //   - UOP_KERNEL : already-kernelized form.
    //   - UOP_GRAD   : pure rewrite rule; let interact_grad fire
    //                  lazily in wnf and re-walk the unrolled chain.
    if (op == UOP_KERNEL || op == UOP_GRAD) return uop;

    u8 arity = uop_arity(op);
    u64 expr_loc = term_val(uop);

    // 1. Classify children.
    u32   child_tids  [MAX_UOP_SRC];
    Term  child_terms [MAX_UOP_SRC];
    Shape child_shapes[MAX_UOP_SRC];
    u32   child_dtypes[MAX_UOP_SRC];
    u8    child_kinds [MAX_UOP_SRC];
    for (u8 i = 0; i < arity; i++) {
        Term child = heap_read(expr_loc + i);
        child_tids[i]  = 0;
        child_terms[i] = 0;
        ChildKind k = classify_child(child, env_id,
                                     &child_tids[i], &child_terms[i],
                                     &child_shapes[i], &child_dtypes[i]);
        if (k == CHILD_UNKNOWN) return uop;        // can't kernelize yet
        child_kinds[i] = (u8)k;
    }

    // 2. Per-op argument extraction (CONST bits / REDUCE kind+axis).
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

    // 3. Allocate KernelEntry and fill input slots, deduping equal
    //    inputs (same tid, or same symbolic term).
    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];

    u8 src_slot[MAX_UOP_SRC];
    for (u8 i = 0; i < arity; i++) {
        i32 slot = -1;
        if (child_kinds[i] == CHILD_SYMBOLIC_VAR) {
            for (u32 j = 0; j < ke->n_inputs; j++) {
                if (ke->input_tids[j] == 0 && ke->input_terms[j] == child_terms[i]) {
                    slot = (i32)j; break;
                }
            }
        } else {
            for (u32 j = 0; j < ke->n_inputs; j++) {
                if (ke->input_tids[j] == child_tids[i]) { slot = (i32)j; break; }
            }
        }
        if (slot < 0) {
            slot = (i32)ke->n_inputs++;
            ke->input_tids  [slot] = child_tids[i];
            ke->input_terms [slot] = child_terms[i];
            ke->input_dtypes[slot] = child_dtypes[i];
            ke->input_numels[slot] = shape_numel(child_shapes[i]);
        }
        src_slot[i] = (u8)slot;
    }

    // 4. Output shape -- mirror the rules in op_output_shape but
    //    using the child_shapes table so VAR inputs work.
    Shape out_shape = (Shape){0};
    out_shape.ndim    = 1;
    out_shape.dims[0] = 1;
    if (arity == 0) {
        out_shape.ndim = 1;
        out_shape.dims[0] = 1;
    } else {
        out_shape = child_shapes[0];
        if (uop_is_binary_elementwise(op) && arity >= 2) {
            u32 n0 = shape_numel(child_shapes[0]);
            u32 n1 = shape_numel(child_shapes[1]);
            if (n0 == 1 && n1 > 1) out_shape = child_shapes[1];
        }
        if (op == UOP_REDUCE) {
            u32 axis = op_arg & 0xFFFF;
            if (out_shape.ndim <= 1) {
                out_shape.ndim    = 1;
                out_shape.dims[0] = 1;
            } else {
                u32 dst = 0;
                for (u32 i = 0; i < child_shapes[0].ndim; i++) {
                    if (i == axis) continue;
                    out_shape.dims[dst++] = child_shapes[0].dims[i];
                }
                out_shape.ndim = dst;
                for (u32 i = dst; i < MAX_DIM; i++) out_shape.dims[i] = 0;
            }
        }
        if (op == UOP_EXPAND) {
            u32 ndim = child_shapes[0].ndim;
            out_shape.ndim = ndim;
            for (u32 i = 0; i < ndim; i++) {
                Term n = heap_read(expr_loc + 1 + i);
                out_shape.dims[i] = (u32)term_val(n);
            }
        }
        if (op == UOP_CONV2D) {
            // input  child_shapes[0] = {C_in, H, W}
            // weight child_shapes[1] = {C_out, C_in, kh, kw}
            // bias   child_shapes[2] = {C_out}     (unused for shape calc)
            // Output {C_out, H - kh + 1, W - kw + 1} (valid conv, stride 1).
            u32 c_out = child_shapes[1].dims[0];
            u32 kh    = child_shapes[1].dims[2];
            u32 kw    = child_shapes[1].dims[3];
            u32 h     = child_shapes[0].dims[1];
            u32 w     = child_shapes[0].dims[2];
            out_shape.ndim    = 3;
            out_shape.dims[0] = c_out;
            out_shape.dims[1] = h - kh + 1;
            out_shape.dims[2] = w - kw + 1;
            for (u32 i = 3; i < MAX_DIM; i++) out_shape.dims[i] = 0;
        }
        if (op == UOP_RESHAPE) {
            // Layout: heap[expr_loc] = src; heap[expr_loc + 1 + i] =
            // NUM(d_i).  Recover ndim by reading dim cells until the
            // running product equals the input numel -- RESHAPE
            // preserves total element count, so this terminates
            // exactly at the last real dim.  MAX_DIM caps the loop
            // so a misshapen reshape can't run off the heap.
            u32 in_numel = shape_numel(child_shapes[0]);
            u32 ndim = 0;
            u32 prod = 1;
            for (u32 i = 0; i < MAX_DIM; i++) {
                Term n = heap_read(expr_loc + 1 + i);
                if (term_tag(n) != TAG_NUM) break;
                u32 d = (u32)term_val(n);
                out_shape.dims[ndim++] = d;
                prod *= d;
                if (prod == in_numel) break;
            }
            out_shape.ndim = ndim;
            for (u32 i = ndim; i < MAX_DIM; i++) out_shape.dims[i] = 0;
        }
    }
    u32 out_dtype = (arity == 0) ? const_dtype : child_dtypes[0];
    ke->output_shape = out_shape;
    ke->output_dtype = out_dtype;
    u32 out_numel = shape_numel(out_shape);
    ke->output_numel = out_numel;
    ke->output_tid   = tensor_alloc(CURRENT_BACKEND, out_shape, out_dtype);
    TENS[ke->output_tid].producer_kid = kid;

    // 5. Linearize program (single op).  For REDUCE, repack op_arg
    //    from (kind << 16 | axis) -- which the shape calc above
    //    needed -- to (kind << 24 | inner) where inner = product
    //    of input dims AFTER the reduced axis.  cpu_op_reduce
    //    needs `inner` (not `axis`) to stride correctly over a
    //    non-innermost axis.
    if (op == UOP_REDUCE && arity > 0) {
        u32 kind = (op_arg >> 16) & 0xFFFF;
        u32 axis =  op_arg        & 0xFFFF;
        u32 inner = 1;
        for (u32 i = axis + 1; i < child_shapes[0].ndim; i++) {
            inner *= child_shapes[0].dims[i];
        }
        op_arg = ((kind & 0xFF) << 24) | (inner & 0x00FFFFFF);
    }
    KProgOp *p = &ke->program[ke->n_ops++];
    p->opcode = op;
    p->dtype  = (u8)out_dtype;
    p->n_src  = arity;
    p->numel  = out_numel;
    p->arg    = op_arg;
    for (u8 i = 0; i < arity; i++) p->src[i] = KSRC_AS_INPUT(src_slot[i]);

    ke->compiled   = NULL;
    ke->source_uop = uop;     // remember the original UOP for grad walks

    // 6. Emit UOP_KERNEL term.
    u64 kloc = heap_alloc(2);
    heap_set(kloc + 0, term_new(0, TAG_TEN, out_dtype, ke->output_tid));
    heap_set(kloc + 1, term_new(0, TAG_NUM, DT_I32, kid));
    return term_new(0, TAG_UOP, UOP_KERNEL, kloc);
}
