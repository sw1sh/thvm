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

    // f1d-b2: when the toggle is on, route through the inlined
    // helper.  Realized UOPs (root, REDUCE outputs, multi-consumer)
    // get a single kernel that absorbs un-realized upstream
    // elementwise compute.  Un-realized UOPs return unchanged so
    // walk_cell skips the rewrite -- a downstream realized parent
    // will inline them via materialize_kernel_inlined.  The helper
    // returns 0 if the chain contains a non-elementwise un-realized
    // upstream UOp; in that case fall through to the legacy path.
    if (MATERIALIZE_USE_REALIZE_INFO) {
      if (realize_is_realized(uop)) {
        Term k = materialize_kernel_inlined(uop);
        if (k != 0) return k;
      } else {
        return uop;
      }
    }

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

    // 2. Per-op argument extraction (CONST bits / REDUCE kind+axis /
    //    FLIP axes_bitmask).
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
    // (UOP_PAD widths are extracted later, after we know the source
    //  shape -- the heap stores them but ndim isn't explicit, so we
    //  walk 2 * src0_ndim cells.)

    // 2c. View-only EXPAND (sub-item f3c of the kernel-fusion arc).
    //     Build an aliased TenDesc with shape = target and
    //     strides[axis] = 0 where source.dim == 1 < target.dim
    //     (broadcast).  Other axes keep source's row-major strides.
    //     Returns a TAG_TEN that the walker rewrites the EXPAND
    //     heap cell to.  Consumers of this alias must read through
    //     view_strided_index (cpu_interpret pre-materializes).
    //     If the root of thvm_materialize lands on a non-contig
    //     alias, thvm_materialize post-materializes to a contig buf
    //     so the WL/tests that read the underlying buf flat keep
    //     working.
    if (op == UOP_EXPAND
        && CURRENT_BACKEND && CURRENT_BACKEND->view_aware
        && arity == 1 && child_tids[0] != 0) {
      // Source need not be contiguous (e.g., the source is itself a
      // SHRINK/PERMUTE/FLIP alias).  We inherit the source's strides
      // on non-broadcast axes and set 0 on broadcast axes; the
      // resulting alias is non-contig unless target.numel ==
      // source.numel (degenerate no-op EXPAND).  Without this
      // non-contig fall-through the chain SHRINK -> EXPAND would
      // hit the kernel-emit path on EXPAND and allocate at the
      // FULL target size; LeNet's bias-add chain (Conv1 bias 20 ->
      // expanded to 20*24*24=11520) burned ~7 MiB this way after
      // f3d/e/g landed.
      TenDesc *src = &TENS[child_tids[0]];
      Shape src_shape = src->view.shape;
      u32 t_ndim = (u32)term_val(heap_read(expr_loc + 1));
      if (src_shape.ndim == t_ndim) {
        Shape t_shape = {0};
        t_shape.ndim = t_ndim;
        u32 t_numel = 1;
        u8  ok = 1;
        for (u32 i = 0; i < t_ndim; i++) {
          u32 td = (u32)term_val(heap_read(expr_loc + 2 + i));
          t_shape.dims[i] = td;
          t_numel *= td;
          if (src_shape.dims[i] != td && src_shape.dims[i] != 1) {
            ok = 0;
            break;
          }
        }
        if (ok) {
          View nv = {0};
          nv.shape      = t_shape;
          nv.numel      = t_numel;
          nv.offset     = src->view.offset;
          nv.contiguous = (t_numel == src->view.numel) ? src->view.contiguous : 0;
          // Inherit per-axis strides from source; broadcast axes
          // (src.dim==1 < target.dim) get stride 0.  Works for both
          // contig and non-contig source views since src->view.strides
          // is already authoritative.
          for (u32 i = 0; i < t_ndim; i++) {
            nv.strides[i] = (src_shape.dims[i] == t_shape.dims[i])
                          ? src->view.strides[i] : 0;
          }
          for (u32 i = t_ndim; i < MAX_DIM; i++) nv.strides[i] = 0;
          u32 alias_tid = tensor_view_of(child_tids[0], nv);
          if (alias_tid != 0) {
            return term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
          }
        }
      }
    }

    // 2g. View-only FLIP (sub-item f3g of the kernel-fusion arc).
    //     Flips a set of axes (passed as a bitmask in expr_loc+1)
    //     by negating strides on flipped axes and shifting offset
    //     to the high end: offset += (dims[i] - 1) * strides[i]
    //     per flipped axis.  Shape is unchanged.  contiguous=0
    //     unless no axes are actually flipped (degenerate).
    //     view_strided_index handles negative strides correctly
    //     (offset starts high; (k * neg_stride) walks downward).
    //     materialize_root_alias's max-reachable-index calculation
    //     ignores negative-stride contributions because the offset
    //     already covers the high end of the read range.
    if (op == UOP_FLIP
        && CURRENT_BACKEND && CURRENT_BACKEND->view_aware
        && arity == 1 && child_tids[0] != 0
        && TENS[child_tids[0]].view.contiguous) {
      TenDesc *src = &TENS[child_tids[0]];
      Shape src_shape = src->view.shape;
      u32 mask = (u32)term_val(heap_read(expr_loc + 1));
      View nv = {0};
      nv.shape  = src_shape;
      nv.numel  = src->view.numel;
      nv.offset = src->view.offset;
      u8 any_flip = 0;
      for (u32 i = 0; i < src_shape.ndim; i++) {
        if (mask & (1u << i)) {
          nv.strides[i] = -src->view.strides[i];
          nv.offset += (i32)(src_shape.dims[i] - 1) * src->view.strides[i];
          any_flip = 1;
        } else {
          nv.strides[i] = src->view.strides[i];
        }
      }
      for (u32 i = src_shape.ndim; i < MAX_DIM; i++) nv.strides[i] = 0;
      nv.contiguous = any_flip ? 0 : src->view.contiguous;
      u32 alias_tid = tensor_view_of(child_tids[0], nv);
      if (alias_tid != 0) {
        return term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
      }
    }

    // 2f. View-only PAD (sub-item f3f of the kernel-fusion arc):
    //     INTENTIONALLY NOT IMPLEMENTED.  PAD adds bytes around the
    //     source that must read as the pad_value (zero for our use);
    //     the only way to do that without copying would be to alias
    //     with offset = -starts[i] * strides[i] and trust the
    //     allocator's pre-source memory to be zero.  cpu_buf_alloc
    //     uses calloc, but the bytes BEFORE the buffer's start are
    //     out-of-bounds -- reading them is UB even when the
    //     allocator gave us calloc'd storage at the buffer's
    //     official start.  PAD therefore falls through to the
    //     kernel path (cpu_op_pad memcpy + zero-fill), which is
    //     correct + safe.  See tests/test_view_pad.c for the
    //     documenting test.
    //     Per-conv kernel-count gain: 0 (this sub-item is a no-op
    //     by design); the SHRINK / PERMUTE / FLIP wins land via
    //     f3d / f3e / f3g.

    // 2e. View-only PERMUTE (sub-item f3e of the kernel-fusion arc).
    //     Permuted axis i of the alias maps to source axis perm[i],
    //     so dims[i] = src.dims[perm[i]] and strides[i] =
    //     src.strides[perm[i]].  Offset is unchanged.  contiguous=1
    //     only when perm is identity (rare; most permutes scramble
    //     strides and break row-major ordering).
    if (op == UOP_PERMUTE
        && CURRENT_BACKEND && CURRENT_BACKEND->view_aware
        && arity == 1 && child_tids[0] != 0
        && TENS[child_tids[0]].view.contiguous) {
      TenDesc *src = &TENS[child_tids[0]];
      Shape src_shape = src->view.shape;
      Shape t_shape = {0};
      t_shape.ndim = src_shape.ndim;
      View nv = {0};
      nv.numel  = src->view.numel;
      nv.offset = src->view.offset;
      u8 identity = 1;
      u8 ok = 1;
      u8 used[MAX_DIM] = {0};
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 p = (u32)term_val(heap_read(expr_loc + 1 + i));
        if (p >= src_shape.ndim || used[p]) { ok = 0; break; }
        used[p] = 1;
        t_shape.dims[i] = src_shape.dims[p];
        nv.strides[i]   = src->view.strides[p];
        if (p != i) identity = 0;
      }
      if (ok) {
        nv.shape = t_shape;
        for (u32 i = src_shape.ndim; i < MAX_DIM; i++) nv.strides[i] = 0;
        nv.contiguous = identity ? src->view.contiguous : 0;
        u32 alias_tid = tensor_view_of(child_tids[0], nv);
        if (alias_tid != 0) {
          return term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
        }
      }
    }

    // 2d. View-only SHRINK (sub-item f3d of the kernel-fusion arc).
    //     Build an aliased TenDesc with shape = (e_i - b_i) per axis,
    //     offset += sum(b_i * src_strides[i]), and strides inherited
    //     from the (contiguous) source.  Marks contiguous=0 unless
    //     the slice covers the entire source (degenerate case where
    //     SHRINK is a no-op).  Consumers that need a flat contig
    //     read fall through to view_strided_index via the
    //     materialize_root_alias post-pass at the realize root.
    if (op == UOP_SHRINK
        && CURRENT_BACKEND && CURRENT_BACKEND->view_aware
        && arity == 1 && child_tids[0] != 0
        && TENS[child_tids[0]].view.contiguous) {
      TenDesc *src = &TENS[child_tids[0]];
      Shape src_shape = src->view.shape;
      Shape t_shape = {0};
      t_shape.ndim = src_shape.ndim;
      i32 add_off = 0;
      u32 t_numel = 1;
      u8  ok = 1;
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 b = (u32)term_val(heap_read(expr_loc + 1 + 2 * i));
        u32 e = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
        if (e <= b || e > src_shape.dims[i]) { ok = 0; break; }
        t_shape.dims[i] = e - b;
        t_numel *= (e - b);
        add_off += (i32)b * src->view.strides[i];
      }
      if (ok) {
        View nv = {0};
        nv.shape   = t_shape;
        nv.numel   = t_numel;
        nv.offset  = src->view.offset + add_off;
        for (u32 i = 0; i < src_shape.ndim; i++) {
          nv.strides[i] = src->view.strides[i];
        }
        for (u32 i = src_shape.ndim; i < MAX_DIM; i++) nv.strides[i] = 0;
        nv.contiguous = (t_numel == src->view.numel) ? 1 : 0;
        u32 alias_tid = tensor_view_of(child_tids[0], nv);
        if (alias_tid != 0) {
          return term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
        }
      }
    }

    // 2b. View-only RESHAPE (sub-item f3b of the kernel-fusion arc).
    //     For a contiguous source whose numel matches the target,
    //     RESHAPE doesn't need a memcpy kernel -- alias the source's
    //     buffer with a fresh TenDesc carrying the new shape and
    //     return a TAG_TEN directly.  The walker rewrites the
    //     RESHAPE heap cell to that TAG_TEN; subsequent parents
    //     classify it as CHILD_CONCRETE_TEN.  Falls back to the
    //     cpu_op_reshape memcpy path if the source isn't contiguous
    //     or the numels don't match (rare; usually a user-error
    //     condition).
    if (op == UOP_RESHAPE
        && CURRENT_BACKEND && CURRENT_BACKEND->view_aware
        && arity == 1 && child_tids[0] != 0
        && TENS[child_tids[0]].view.contiguous) {
      // Target shape lives at [src, NUM(ndim), NUM(d0), NUM(d1), ...].
      u32 t_ndim = (u32)term_val(heap_read(expr_loc + 1));
      Shape t_shape = {0};
      t_shape.ndim = t_ndim;
      u32 t_numel = 1;
      for (u32 i = 0; i < t_ndim; i++) {
        u32 d = (u32)term_val(heap_read(expr_loc + 2 + i));
        t_shape.dims[i] = d;
        t_numel *= d;
      }
      if (t_numel == TENS[child_tids[0]].view.numel) {
        View nv = view_create(t_shape);
        u32 alias_tid = tensor_view_of(child_tids[0], nv);
        if (alias_tid != 0) {
          return term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
        }
      }
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
            // ndim is stored explicitly at expr_loc+1; dims at +2..
            u32 ndim = (u32)term_val(heap_read(expr_loc + 1));
            out_shape.ndim = ndim;
            for (u32 i = 0; i < ndim; i++) {
                Term n = heap_read(expr_loc + 2 + i);
                out_shape.dims[i] = (u32)term_val(n);
            }
        }
        if (op == UOP_PAD) {
            // PAD: out.dim[i] = src.dim[i] + b_i + e_i.  Heap layout
            // [src, NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...]; ndim
            // implicit in the source.
            out_shape = child_shapes[0];
            for (u32 i = 0; i < child_shapes[0].ndim; i++) {
                u32 b = (u32)term_val(heap_read(expr_loc + 1 + 2 * i));
                u32 e = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
                out_shape.dims[i] = child_shapes[0].dims[i] + b + e;
            }
        }
        if (op == UOP_SHRINK) {
            // SHRINK: out.dim[i] = e_i - b_i.  Same heap layout as
            // PAD but b/e are kept-slice boundaries (inclusive begin,
            // exclusive end) rather than pad widths.
            out_shape = child_shapes[0];
            for (u32 i = 0; i < child_shapes[0].ndim; i++) {
                u32 b = (u32)term_val(heap_read(expr_loc + 1 + 2 * i));
                u32 e = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
                out_shape.dims[i] = (e > b) ? (e - b) : 0;
            }
        }
        if (op == UOP_PERMUTE) {
            // PERMUTE: out.dim[i] = src.dim[perm[i]].  Heap layout
            // [src, NUM(p0), NUM(p1), ..., NUM(p_{ndim-1})];
            // ndim implicit in the source.
            out_shape = child_shapes[0];
            for (u32 i = 0; i < child_shapes[0].ndim; i++) {
                u32 p = (u32)term_val(heap_read(expr_loc + 1 + i));
                out_shape.dims[i] = child_shapes[0].dims[p];
            }
        }
        if (op == UOP_RESHAPE) {
            // Layout: heap[expr_loc] = src; heap[expr_loc + 1] =
            // NUM(ndim); heap[expr_loc + 2 + i] = NUM(d_i).  ndim is
            // authoritative -- the previous numel-based termination
            // hack broke on shapes whose prefix products hit numel
            // early (e.g. {1, 4} on a numel-4 source).
            u32 ndim = (u32)term_val(heap_read(expr_loc + 1));
            out_shape.ndim = ndim;
            for (u32 i = 0; i < ndim; i++) {
                Term n = heap_read(expr_loc + 2 + i);
                out_shape.dims[i] = (u32)term_val(n);
            }
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
    // Prepend a LOAD per input slot (sub-item c of the UOP_LOAD arc) --
    // structural marker; backends can run cpu_op_load (memcpy) or
    // treat LOAD as no-op (sub-item d).  Main op below still
    // references KSRC_AS_INPUT(N).
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
    p->arg       = op_arg;
    p->src0_ndim = 0;
    p->out_ndim  = 0;
    for (u32 i = 0; i < MAX_DIM; i++) p->src0_dims[i] = 0;
    for (u32 i = 0; i < MAX_DIM; i++) p->out_dims [i] = 0;
    for (u8 i = 0; i < arity; i++) p->src[i] = KSRC_AS_INPUT(src_slot[i]);

    // Movement ops (currently EXPAND, FLIP -- RESHAPE/PERMUTE
    // could reuse this in future) need source slot 0's per-axis
    // shape AND the output's per-axis shape so the kernel can
    // distinguish leading- from trailing-axis broadcasts (EXPAND)
    // or mirror axes (FLIP).  out_numel + in_numel alone aren't
    // enough.
    if ((op == UOP_EXPAND || op == UOP_FLIP || op == UOP_PAD
      || op == UOP_PERMUTE || op == UOP_SHRINK) && arity > 0) {
      Shape s0 = child_shapes[0];
      p->src0_ndim = (u8)(s0.ndim & 0xFF);
      for (u32 i = 0; i < s0.ndim && i < MAX_DIM; i++) {
        p->src0_dims[i] = s0.dims[i];
      }
      p->out_ndim = (u8)(out_shape.ndim & 0xFF);
      for (u32 i = 0; i < out_shape.ndim && i < MAX_DIM; i++) {
        p->out_dims[i] = out_shape.dims[i];
      }
    }
    // PAD/SHRINK widths (begin/end interleaved) share the same
    // storage; semantics differ at the kernel level.  u8 caps each
    // at 255 -- plenty for transposed-conv kh/kw - 1 (PAD) or for
    // kept-slice boundaries on axes <= 255 in extent (SHRINK).
    if ((op == UOP_PAD || op == UOP_SHRINK) && arity > 0) {
      for (u32 i = 0; i < child_shapes[0].ndim && i < MAX_DIM; i++) {
        u32 b = (u32)term_val(heap_read(expr_loc + 1 + 2 * i));
        u32 e = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
        p->pad_widths[2 * i + 0] = (u8)(b & 0xFF);
        p->pad_widths[2 * i + 1] = (u8)(e & 0xFF);
      }
    }
    // PERMUTE per-axis source-axis mapping.
    if (op == UOP_PERMUTE && arity > 0) {
      for (u32 i = 0; i < child_shapes[0].ndim && i < MAX_DIM; i++) {
        u32 pi = (u32)term_val(heap_read(expr_loc + 1 + i));
        p->axis_perm[i] = (u8)(pi & 0xFF);
      }
    }

    ke->compiled   = NULL;
    ke->source_uop = uop;     // remember the original UOP for grad walks

    // 6. Emit UOP_KERNEL term.
    u64 kloc = heap_alloc(2);
    heap_set(kloc + 0, term_new(0, TAG_TEN, out_dtype, ke->output_tid));
    heap_set(kloc + 1, term_new(0, TAG_NUM, DT_I32, kid));
    return term_new(0, TAG_UOP, UOP_KERNEL, kloc);
}
