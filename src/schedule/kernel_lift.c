// Coverage counters for kernel_lift_to_uop.  Read via
// kernel_lift_attempts() / kernel_lift_successes().  Counters are
// global (single-threaded scheduling).  Reset by thvm_init / thvm_free.
static u64 KERNEL_LIFT_ATTEMPTS;
static u64 KERNEL_LIFT_SUCCESSES;

fn u64 kernel_lift_attempts(void)        { return KERNEL_LIFT_ATTEMPTS; }
fn u64 kernel_lift_successes(void)       { return KERNEL_LIFT_SUCCESSES; }

fn void kernel_lift_counters_reset(void) {
  KERNEL_LIFT_ATTEMPTS = 0;
  KERNEL_LIFT_SUCCESSES = 0;
}

// Increment helpers for callers in earlier translation units (codegen/
// render_metal.c is #include'd before this file; they can't reach the
// static globals directly).
fn void kernel_lift_count_attempt (void) { KERNEL_LIFT_ATTEMPTS++; }
fn void kernel_lift_count_success (void) { KERNEL_LIFT_SUCCESSES++; }

// schedule/kernel_lift.c - lift a scheduled kernel's ScalarUop arena
// to a UOp DAG root suitable for cg_render_uop_kernel.
//
// kernel_lift_to_uop takes a fully-scheduled kernel and builds the
// equivalent UOp DAG on the heap, returning a UOP_STORE root +
// UOP_BUFFER terms for output and inputs.
//
// Coverage:
//   S_CONST                  -> UOP_CONST
//   S_LOAD                   -> UOP_INDEX_E(buffer, addr)
//   S_INDEX(buf, ranges...)  -> linearised symbolic addr expr
//   S_ADD / MUL              -> UOP_ADD / UOP_MUL
//   S_NEG / RECIP / EXP2 /
//   LOG2 / SQRT              -> UOP_NEG / UOP_RECIP / etc.
//   S_CMPLT / CMPEQ          -> UOP_CMPLT / UOP_CMPEQ
//   S_CAST                   -> UOP_CAST
//   S_REDUCE_SUM/MAX(body, ranges...)
//                            -> UOP_REDUCE(lifted_body, kind, axis)
//   S_STORE(INDEX, value)    -> UOP_STORE(buf, addr, lifted_value)
//   S_BUFFERIZE(STORE, ranges...) -> kernel root; produces lifter result
//   S_RANGE                  -> UOP_RANGE (via UopRangeMap)
//   S_ICONST / S_I*          -> existing scalar_to_uop path
//
// Returns 0 on unsupported shape; the renderer's caller falls back
// per its own contract.

// KernelUopLift is declared in thvm.h.

// Reject diagnostic forward declaration: when env-gated, prints the
// first ScalarUop the lifter doesn't handle.  Defined below.
static void lift_reject_log(KernelEntry const *ke, u32 sid,
                            const char *where);

// Build per-axis-id maps from BUFFERIZE's range slots.  axis_id is
// the position in BUFFERIZE.src[1..n] (0-based).
typedef struct {
  u32  axis_id;
  u32  scalar_id;     // S_RANGE slot
  Term axis_uop;      // UOP_RANGE term
} LiftRangeMap;

static Term lift_scalar_value(KernelEntry const *ke, u32 sid,
                              LiftRangeMap const *ranges, u32 n_ranges,
                              Term out_buf, Term const *in_bufs,
                              u32 n_inputs);

static Term lift_lookup_range(LiftRangeMap const *ranges, u32 n_ranges,
                              u32 scalar_id) {
  for (u32 i = 0; i < n_ranges; i++) {
    if (ranges[i].scalar_id == scalar_id) return ranges[i].axis_uop;
  }
  return 0;
}

// Build the symbolic address expression for an S_INDEX(buf, ranges...)
// node.  Linearises: addr = sum(rng_i * stride_i) where stride_i is
// the row-major stride based on the surrounding buffer shape.  For
// row-major contiguous buffers this matches the expected layout.
//
// Returns the addr Term; *out_buf_term is filled with the lifted
// UOP_BUFFER for the source buffer.
static Term lift_scalar_index(KernelEntry const *ke, u32 sid,
                              LiftRangeMap const *ranges, u32 n_ranges,
                              Term out_buf, Term const *in_bufs,
                              u32 n_inputs, Term *out_buf_term) {
  if (sid == 0 || sid >= ke->n_scalar_uops) {
    lift_reject_log(ke, sid, "index/sid-oor");
    return 0;
  }
  ScalarUop const *u = &ke->scalar_uops[sid];
  if ((u->op != S_INDEX && u->op != S_INDEX_E) || u->src_count < 1) {
    lift_reject_log(ke, sid, "index/not-S_INDEX-or-INDEX_E");
    return 0;
  }
  u32 buf_sid = u->src[0];
  ScalarUop const *bu = &ke->scalar_uops[buf_sid];
  Term buf = 0;
  if (bu->op == S_DEFINE_PARAM) {
    u32 slot = (u32)bu->extra;
    if (slot >= n_inputs) { lift_reject_log(ke, buf_sid, "index/slot-oor"); return 0; }
    buf = in_bufs[slot];
  } else if (bu->op == S_DEFINE_OUTPUT) {
    buf = out_buf;
  } else if (bu->op == S_INDEX && bu->src_count >= 1) {
    // Buf-of-INDEX: the outer S_INDEX wraps an inner S_INDEX that
    // references the same underlying tensor.  Rangeify produces
    // this shape as pad/shrink/permute residue -- two layered
    // S_INDEX nodes addressing the same DEFINE_*  buffer with
    // different iteration ranges.  See-through: use the inner's
    // DEFINE_*  as the underlying buffer; the outer's ranges (this
    // node's src[1..]) drive the address linearisation.
    u32 inner_buf_sid = bu->src[0];
    if (inner_buf_sid == 0 || inner_buf_sid >= ke->n_scalar_uops) {
      lift_reject_log(ke, buf_sid, "index/buf-of-INDEX/inner-oor");
      return 0;
    }
    ScalarUop const *ibu = &ke->scalar_uops[inner_buf_sid];
    if (ibu->op == S_DEFINE_PARAM) {
      u32 slot = (u32)ibu->extra;
      if (slot >= n_inputs) {
        lift_reject_log(ke, inner_buf_sid, "index/buf-of-INDEX/slot-oor");
        return 0;
      }
      buf = in_bufs[slot];
    } else if (ibu->op == S_DEFINE_OUTPUT) {
      buf = out_buf;
    } else {
      lift_reject_log(ke, inner_buf_sid, "index/buf-of-INDEX/nested");
      return 0;
    }
  } else {
    lift_reject_log(ke, buf_sid, "index/buf-not-DEFINE");
    // Extra context for the buf-of-INDEX pattern: print the inner
    // ScalarUop's src list so we can see what the layered access
    // looks like.  Gated by the same env knob as the main reject
    // log.
    char const *e = getenv("THVM_DUMP_LIFT_REJECT");
    if (e != NULL && e[0] == '1' && bu->src_count > 0) {
      fprintf(stderr, "  inner srcs:");
      for (u32 i = 0; i < bu->src_count && i < 8; i++) {
        u32 isid = bu->src[i];
        if (isid > 0 && isid < ke->n_scalar_uops) {
          fprintf(stderr, " [%u]=%s",
                  i, scalar_op_name(ke->scalar_uops[isid].op));
        } else {
          fprintf(stderr, " [%u]=oor", i);
        }
      }
      fputc('\n', stderr);
    }
    return 0;
  }
  if (out_buf_term != NULL) *out_buf_term = buf;

  // S_INDEX_E carries the addr expression directly in src[1] -- no
  // need to linearise from per-axis ranges.  Lift via scalar_to_uop
  // (handles S_IADD / S_IMUL / S_RANGE / etc.) and return.
  if (u->op == S_INDEX_E) {
    if (u->src_count < 2) {
      lift_reject_log(ke, sid, "index_e/no-addr");
      return 0;
    }
    UopRangeMap srm[MAX_DIM];
    u32 n = (n_ranges < MAX_DIM) ? n_ranges : MAX_DIM;
    for (u32 i = 0; i < n; i++) {
      srm[i].scalar_id = ranges[i].scalar_id;
      srm[i].axis_uop  = ranges[i].axis_uop;
    }
    Term addr = scalar_to_uop((KernelEntry *)ke, u->src[1], srm, n);
    if (addr == 0) lift_reject_log(ke, u->src[1], "index_e/addr-lift-fail");
    return addr;
  }

  // Walk the RANGE srcs and emit `sum(r_i * stride_i)`.  Strides come
  // from the buffer's shape (row-major).  For now we use uop_buffer
  // dims as the shape -- this matches the lifter's UOP_BUFFER setup.
  u32 ndim = uop_buffer_ndim(buf);
  u32 outer_rank = (u32)u->src_count - 1;
  // Flat-index special case: outer S_INDEX has a single range whose
  // extent equals the buffer's total numel (flat row-major linear
  // offset).  This handles the rangeify pad/shrink residue feeding
  // through a buf-of-INDEX where the outer access flattens the
  // buffer to rank-1.  Reject if the range extent doesn't match
  // total numel -- that's a partial-axis SHRINK or non-contiguous
  // access we can't lift safely (tested via lenet-mnist/bench-
  // train.wls; mismatched extents triggered SIGABRT on the
  // backward path).
  // Axis-append composition was attempted but produced wrong
  // numerics on LeNet forward (sample 3 NaN'd).  The simple
  // row-major-stride composition over inner+outer ranges doesn't
  // hold when the inner / outer ranges have extents that don't
  // span their corresponding buffer dims (rangeify produces partial
  // ranges as movement-op residue).  Reverted; needs a more
  // careful design that takes range extents and movement semantics
  // into account.  See nn_profiling_loop.md axis-append-revisit
  // task.
  if (outer_rank == 1 && ndim > 1) {
    u32 r_sid = u->src[1];
    if (r_sid == 0 || r_sid >= ke->n_scalar_uops) {
      lift_reject_log(ke, r_sid, "index/flat-range-oor");
      return 0;
    }
    ScalarUop const *ru = &ke->scalar_uops[r_sid];
    if (ru->op != S_RANGE) {
      lift_reject_log(ke, r_sid, "index/flat-range-not-RANGE");
      return 0;
    }
    u32 range_extent = (u32)(ru->extra & 0xFFFFFFFFu);
    u32 total_numel = 1;
    for (u32 d = 0; d < ndim; d++) total_numel *= uop_buffer_dim(buf, d);
    if (range_extent != total_numel) {
      lift_reject_log(ke, r_sid, "index/flat-range-extent-mismatch");
      char const *e = getenv("THVM_DUMP_LIFT_REJECT");
      if (e != NULL && e[0] == '1') {
        fprintf(stderr, "  flat-extent: range=%u numel=%u dims=[",
                range_extent, total_numel);
        for (u32 d = 0; d < ndim; d++) {
          fprintf(stderr, "%s%u", d ? "," : "", uop_buffer_dim(buf, d));
        }
        fputs("]\n", stderr);
      }
      return 0;
    }
    Term r_uop = lift_lookup_range(ranges, n_ranges, r_sid);
    if (r_uop == 0) {
      lift_reject_log(ke, r_sid, "index/flat-range-not-mapped");
      return 0;
    }
    return r_uop;
  }
  // Singleton-broadcast fast path: a 1-d buffer of size 1 indexed
  // with no per-dim range refs (src_count=1).  The offset is
  // unambiguously 0 -- this is the softmax-tail / reduce-broadcast
  // pattern (max/sum reduce produces shape [1] which is then
  // broadcast across an outer feature loop).  Diagnosed via MLP2's
  // mlp2_lift_reject.txt (singleton-broadcast at outer_rank>=0).
  if (u->src_count == 1 && ndim == 1 && uop_buffer_dim(buf, 0) == 1) {
    return uop_const(DT_INT32, 0);
  }
  // Broadcast-over-outer-iter fast path: a buf with ndim < outer_rank
  // and src refs src[1..ndim] aligned with the buf's dims (rangeify
  // supplied src[ndim+1..outer_rank] as extra outer iters that this
  // buf doesn't index -- they're broadcast axes).  Safe iff each
  // src[1+d] is an S_RANGE whose extent matches dim[d]; that aligns
  // with the rangeify ordering invariant the L199-207 axis-append
  // composition violated.  Diagnosed via LayerNorm's src_count=3
  // reject (Level 18d / Level 21 prep): buf=[seq_len=32],
  // outer_rank=2 ([seq, features]).  When the alignment holds, fall
  // through to the existing offset-computation loop using only
  // src[1..ndim]; the extras are dropped.
  if (ndim >= 1 && ndim < outer_rank) {
    int extents_align = 1;
    for (u32 d = 0; d < ndim; d++) {
      u32 r_sid = u->src[1 + d];
      if (r_sid == 0 || r_sid >= ke->n_scalar_uops) {
        extents_align = 0; break;
      }
      ScalarUop const *ru = &ke->scalar_uops[r_sid];
      if (ru->op != S_RANGE) { extents_align = 0; break; }
      u32 range_extent = (u32)(ru->extra & 0xFFFFFFFFu);
      if (range_extent != uop_buffer_dim(buf, d)) {
        extents_align = 0; break;
      }
    }
    if (extents_align) {
      // Fall through to the offset-computation loop below.
      goto compute_offset;
    }
    // Otherwise reject below as the strict ndim-mismatch case.
  }
  // Broadcast-over-leading-iter fast path: ndim > outer_rank where
  // the LAST outer_rank buf dims equal the range extents.  The
  // leading (ndim - outer_rank) buf dims are missing from the iter
  // context -- treat them as 0 (broadcast page 0 of the leading
  // axes, sum offset over only the trailing ranges).  Diagnosed via
  // lenet-mnist bench-train (16 hits per training step, all
  // dims=[16,4,2] range_extents=[4,2]).  Default-on after A/B
  // confirmed identical loss (2.3026, log 10) with the path
  // enabled; THVM_LIFT_NDIM_BROADCAST=0 reverts to the reject
  // for bisection.  The earlier axis-append attempt broke numerics
  // for cases where ranges DIDN'T match buf dims; here they MUST
  // match exactly (extents_align check), so the offset composition
  // is unambiguous.
  if (ndim > outer_rank && outer_rank >= 1) {
    char const *_ge = getenv("THVM_LIFT_NDIM_BROADCAST");
    int _on = (_ge == NULL) ? 1 : (_ge[0] != '0');
    if (_on) {
      int extents_align = 1;
      u32 lead = ndim - outer_rank;
      for (u32 d = 0; d < outer_rank; d++) {
        u32 r_sid = u->src[1 + d];
        if (r_sid == 0 || r_sid >= ke->n_scalar_uops) {
          extents_align = 0; break;
        }
        ScalarUop const *ru = &ke->scalar_uops[r_sid];
        if (ru->op != S_RANGE) { extents_align = 0; break; }
        u32 range_extent = (u32)(ru->extra & 0xFFFFFFFFu);
        if (range_extent != uop_buffer_dim(buf, lead + d)) {
          extents_align = 0; break;
        }
      }
      if (extents_align) {
        // Offset = sum(range_iter[d] * stride_d) where stride_d is
        // the product of buf dims AFTER position (lead + d).
        Term acc = 0;
        for (u32 d = 0; d < outer_rank; d++) {
          u32 r_sid = u->src[1 + d];
          Term r_uop = lift_lookup_range(ranges, n_ranges, r_sid);
          if (r_uop == 0) {
            acc = 0; break;
          }
          u32 stride = 1;
          for (u32 e = lead + d + 1; e < ndim; e++) stride *= uop_buffer_dim(buf, e);
          Term term = (stride == 1) ? r_uop
                    : uop_int_binary(UOP_IMUL, r_uop,
                                     uop_const(DT_INT32, stride));
          acc = (acc == 0) ? term
              : uop_int_binary(UOP_IADD, acc, term);
        }
        if (acc != 0) return acc;
      }
    }
  }
  if (ndim == 0 || ndim != outer_rank) {
    // Match the env-gating on every other lift_reject_log site --
    // the unconditional one-line stderr print used to fire even
    // when the bench wasn't asking for diagnostics.
    char const *e = getenv("THVM_DUMP_LIFT_REJECT");
    int dump_on = (e != NULL && e[0] == '1');
    if (dump_on) {
      fprintf(stderr,
              "lift reject: index/ndim-mismatch buf_ndim=%u src_count=%u\n",
              ndim, u->src_count);
      fprintf(stderr, "  ndim-mismatch: outer_rank=%u dims=[", outer_rank);
      for (u32 d = 0; d < ndim; d++) {
        fprintf(stderr, "%s%u", d ? "," : "", uop_buffer_dim(buf, d));
      }
      fputs("] range_extents=[", stderr);
      // Dump per-axis-ref scalar source: extent for S_RANGE leaves,
      // op-name for expressions.  This is what the earlier reverted
      // axis-append attempt needed to design against -- knowing whether
      // the ranges collectively span the buffer's numel and in what
      // order is the prerequisite for a safe lift.
      for (u32 d = 0; d < outer_rank; d++) {
        u32 r_sid = u->src[1 + d];
        if (d) fputc(',', stderr);
        if (r_sid == 0 || r_sid >= ke->n_scalar_uops) {
          fputs("?", stderr);
          continue;
        }
        ScalarUop const *ru = &ke->scalar_uops[r_sid];
        if (ru->op == S_RANGE) {
          fprintf(stderr, "%u",
                  (u32)(ru->extra & 0xFFFFFFFFu));
        } else {
          fprintf(stderr, "expr(op=%u)", (unsigned)ru->op);
        }
      }
      fputs("]\n", stderr);
    }
    return 0;
  }
compute_offset:;
  // Strides: prefer the View's actual strides (which encode broadcast
  // as stride=0 on EXPANDED dims and reflect non-contiguous storage)
  // over reconstructed dim-product strides. Falling back to dim-product
  // if the buffer isn't an input we can resolve to a View.
  // F3.5 fix: previously this computed `stride[d] = product(dims[d+1..])`
  // which is correct for contiguous materialised buffers but wrong for
  // virtual EXPAND views (e.g. matmul A reshape->[M,K,1] expand->[M,K,N]
  // is shape=[M,K,N] strides=[K,1,0] over a 256-element underlying
  // buffer). The dim-product strides [256,16,1] would address a
  // 4096-element materialised buffer, but the schedule keeps EXPAND
  // virtual -- so reads at offsets >= numel are out-of-bounds.
  i64 view_strides[MAX_DIM];
  int have_view_strides = 0;
  if (ke->input_views != NULL) {
    u32 inst = uop_buffer_inst_get(buf);
    if (inst >= 1 && inst - 1 < ke->n_inputs) {
      u32 slot = inst - 1;
      View const *v = &ke->input_views[slot];
      if (v->shape.ndim == ndim) {
        for (u32 d = 0; d < ndim; d++) view_strides[d] = v->strides[d];
        have_view_strides = 1;
      }
    }
  }
  Term acc = 0;
  for (u32 d = 0; d < ndim; d++) {
    u32 r_sid = u->src[1 + d];
    Term r_uop = lift_lookup_range(ranges, n_ranges, r_sid);
    if (r_uop == 0) return 0;
    i64 stride;
    if (have_view_strides) {
      stride = view_strides[d];
      if (stride == 0) continue;       // broadcast dim: don't contribute
    } else {
      stride = 1;
      for (u32 e = d + 1; e < ndim; e++) stride *= uop_buffer_dim(buf, e);
    }
    Term term = (stride == 1) ? r_uop
              : uop_int_binary(UOP_IMUL, r_uop,
                               uop_const(DT_INT32, (u32)stride));
    acc = (acc == 0) ? term
        : uop_int_binary(UOP_IADD, acc, term);
  }
  if (acc == 0) acc = uop_const(DT_INT32, 0);
  return acc;
}

static u32 lift_scalar_unary_op(u8 sop) {
  switch (sop) {
    case S_NEG:   return UOP_NEG;
    case S_RECIP: return UOP_RECIP;
    case S_EXP2:  return UOP_EXP2;
    case S_LOG2:  return UOP_LOG2;
    case S_SQRT:  return UOP_SQRT;
    default:      return 0;
  }
}

static u32 lift_scalar_binary_op(u8 sop) {
  switch (sop) {
    case S_ADD:   return UOP_ADD;
    case S_MUL:   return UOP_MUL;
    case S_CMPLT: return UOP_CMPLT;
    case S_CMPEQ: return UOP_CMPEQ;
    default:      return 0;
  }
}

static Term lift_scalar_value(KernelEntry const *ke, u32 sid,
                              LiftRangeMap const *ranges, u32 n_ranges,
                              Term out_buf, Term const *in_bufs,
                              u32 n_inputs) {
  if (sid == 0 || sid >= ke->n_scalar_uops) return 0;
  ScalarUop const *u = &ke->scalar_uops[sid];
  switch (u->op) {
    case S_CONST: {
      u32 bits = (u32)u->extra;
      return uop_const(u->dtype, bits);
    }
    case S_LOAD: case S_LOAD_RAW: {
      if (u->src_count != 1) return 0;
      Term buf = 0;
      Term addr = lift_scalar_index(ke, u->src[0], ranges, n_ranges,
                                    out_buf, in_bufs, n_inputs, &buf);
      if (addr == 0 || buf == 0) return 0;
      return uop_index_e(buf, addr);
    }
    case S_NEG: case S_RECIP: case S_EXP2:
    case S_LOG2: case S_SQRT: {
      if (u->src_count != 1) return 0;
      u32 op = lift_scalar_unary_op(u->op);
      Term src = lift_scalar_value(ke, u->src[0], ranges, n_ranges,
                                   out_buf, in_bufs, n_inputs);
      if (src == 0) return 0;
      return uop_unary(op, src);
    }
    case S_ADD: case S_MUL: case S_CMPLT: case S_CMPEQ: {
      if (u->src_count != 2) return 0;
      u32 op = lift_scalar_binary_op(u->op);
      Term a = lift_scalar_value(ke, u->src[0], ranges, n_ranges,
                                 out_buf, in_bufs, n_inputs);
      Term b = lift_scalar_value(ke, u->src[1], ranges, n_ranges,
                                 out_buf, in_bufs, n_inputs);
      if (a == 0 || b == 0) return 0;
      return uop_binary(op, a, b);
    }
    case S_REDUCE_SUM: case S_REDUCE_MAX: {
      if (u->src_count < 1) return 0;
      Term body = lift_scalar_value(ke, u->src[0], ranges, n_ranges,
                                    out_buf, in_bufs, n_inputs);
      if (body == 0) return 0;
      // The reduce axis_id == the first range's axis_id (RANGE
      // ordering at BUFFERIZE matches semantic axis order).  For
      // multi-axis reductions we'd need to nest; F1e renderer only
      // handles single-axis, so we match that.
      if (u->src_count < 2) return 0;
      Term r_uop = lift_lookup_range(ranges, n_ranges, u->src[1]);
      if (r_uop == 0) return 0;
      // See-through UOP_OPT(target=range, ...) to find the inner
      // UOP_RANGE: per-USE ranges may be wrapped in
      // UOP_OPT_GROUP_REDUCE for autotune-driven cooperative
      // reduction.  The renderer reads the OPT annotation while
      // the UOP_REDUCE references the underlying axis_id.
      Term inner = r_uop;
      if (term_tag(inner) == TAG_UOP && term_ext(inner) == UOP_OPT) {
        inner = uop_opt_target(inner);
      }
      if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_RANGE) {
        return 0;
      }
      u32 axis_id = term_val(heap_read(term_val(inner) + 0));
      u32 kind = (u->op == S_REDUCE_SUM) ? REDUCE_SUM : REDUCE_MAX;
      return uop_reduce(kind, axis_id, body);
    }
    case S_CAST: {
      if (u->src_count != 1) return 0;
      Term src = lift_scalar_value(ke, u->src[0], ranges, n_ranges,
                                   out_buf, in_bufs, n_inputs);
      if (src == 0) return 0;
      return uop_cast(src, u->dtype);
    }
    case S_RANGE: {
      // Bare RANGE in a value position would be unusual but well-
      // defined: emit the corresponding UOP_RANGE.  Used for
      // index-like scalar values feeding into another expression.
      Term r = lift_lookup_range(ranges, n_ranges, sid);
      if (r == 0) lift_reject_log(ke, sid, "value/range-not-mapped");
      return r;
    }
    case S_SHRINK: {
      // src[0] = body, src[1..n] = ranges; extra packs per-axis u16
      // begin offsets (4 axes at 16 bits each).  Lifter rewrites the
      // matching axis_uop entries to UOP_IADD(original, begin) so
      // body LOADs read the shifted source.
      if (u->src_count < 2) return 0;
      u32 ndim = (u32)u->src_count - 1;
      if (ndim > 4) return 0;
      LiftRangeMap shifted[MAX_DIM];
      u32 n_sh = n_ranges;
      for (u32 i = 0; i < n_ranges; i++) shifted[i] = ranges[i];
      for (u32 d = 0; d < ndim; d++) {
        u32 begin = (u32)((u->extra >> (16 * d)) & 0xFFFFu);
        if (begin == 0) continue;
        u32 r_sid = u->src[1 + d];
        for (u32 i = 0; i < n_sh; i++) {
          if (shifted[i].scalar_id != r_sid) continue;
          Term r = shifted[i].axis_uop;
          shifted[i].axis_uop = uop_int_binary(UOP_IADD, r,
                                               uop_const(DT_INT32, begin));
          break;
        }
      }
      return lift_scalar_value(ke, u->src[0], shifted, n_sh,
                               out_buf, in_bufs, n_inputs);
    }
    case S_PAD: {
      // src[0] = body, src[1..n] = ranges; extra packs (begin u8,
      // src_dim u8) per axis (4 axes).  Lifter rewrites axis_uop to
      // UOP_ISUB(original, begin) so body LOADs read the shifted
      // source, AND wraps the body's value in UOP_IWHERE guarded
      // against shifted_iter ∈ [0, src_dim).  Out-of-range reads
      // yield UOP_INVALID (renderer emits 0 / reduce identity).
      if (u->src_count < 2) return 0;
      u32 ndim = (u32)u->src_count - 1;
      if (ndim > 4) return 0;
      LiftRangeMap shifted[MAX_DIM];
      u32 n_sh = n_ranges;
      for (u32 i = 0; i < n_ranges; i++) shifted[i] = ranges[i];
      Term cond_acc = 0;
      for (u32 d = 0; d < ndim; d++) {
        u32 begin   = (u32)((u->extra >> (16 * d)) & 0xFFu);
        u32 src_dim = (u32)((u->extra >> (16 * d + 8)) & 0xFFu);
        u32 r_sid = u->src[1 + d];
        for (u32 i = 0; i < n_sh; i++) {
          if (shifted[i].scalar_id != r_sid) continue;
          Term r = shifted[i].axis_uop;
          Term shifted_iter = (begin == 0) ? r
                            : uop_int_binary(UOP_ISUB, r,
                                             uop_const(DT_INT32, begin));
          shifted[i].axis_uop = shifted_iter;
          // (shifted >= 0): for non-negative iters (our RANGE leaves
          // are bounded [0, ext)), this is equivalent to (iter >= begin).
          // We encode as (shifted >= 0) which equals (begin <= iter)
          // -> simplify to (iter >= begin) -> we use UOP_ILT inverted.
          // For simplicity emit (shifted < src_dim) AND (begin <= iter).
          Term lo = uop_int_binary(UOP_ILT,
                                   uop_const(DT_INT32, begin - 1),
                                   r);  // begin-1 < iter == iter >= begin
          if (begin == 0) lo = uop_const(DT_INT32, 1);
          Term hi = uop_int_binary(UOP_ILT, shifted_iter,
                                   uop_const(DT_INT32, src_dim));
          Term axis_in = uop_int_binary(UOP_IAND, lo, hi);
          cond_acc = (cond_acc == 0) ? axis_in
                                     : uop_int_binary(UOP_IAND,
                                                      cond_acc, axis_in);
          break;
        }
      }
      Term body = lift_scalar_value(ke, u->src[0], shifted, n_sh,
                                    out_buf, in_bufs, n_inputs);
      if (body == 0) return 0;
      if (cond_acc == 0) return body;
      return uop_iwhere(cond_acc, body, uop_invalid());
    }
    case S_RESHAPE_V: {
      // src[0]=body; extra[0..7]=N_out; src[1..1+N_out]=output iter
      // refs; src[1+N_out..]=input range refs.  Body evaluates with
      // input iters set to (flat_idx / in_stride[d]) % in_extent[d]
      // where flat_idx = sum(out_iter[d] * out_stride[d]) and strides
      // are row-major over the respective extents.
      if (u->src_count < 2) return 0;
      u32 n_out = (u32)(u->extra & 0xFFu);
      u32 n_in  = (u32)u->src_count - 1 - n_out;
      if (n_out == 0 || n_in == 0 || n_out > MAX_DIM || n_in > MAX_DIM)
        return 0;
      // Read output iters (lift each via the existing range/expr path)
      // and their extents.
      Term out_iter[MAX_DIM];
      u32  out_ext [MAX_DIM];
      for (u32 d = 0; d < n_out; d++) {
        u32 r_sid = u->src[1 + d];
        if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
        ScalarUop const *ru = &ke->scalar_uops[r_sid];
        if (ru->op == S_RANGE) {
          out_iter[d] = lift_lookup_range(ranges, n_ranges, r_sid);
          out_ext [d] = (u32)(ru->extra & 0xFFFFFFFFu);
        } else {
          // expression-as-iter (e.g. S_IMOD); lift via scalar_to_uop
          out_iter[d] = lift_scalar_value(ke, r_sid, ranges, n_ranges,
                                          out_buf, in_bufs, n_inputs);
          out_ext [d] = 0;  // unknown extent for expressions
        }
        if (out_iter[d] == 0) return 0;
      }
      // Compute flat_idx = sum(out_iter[d] * out_stride[d]).
      Term flat = 0;
      for (u32 d = 0; d < n_out; d++) {
        u32 stride = 1;
        for (u32 e = d + 1; e < n_out; e++) stride *= out_ext[e];
        Term term = (stride == 1) ? out_iter[d]
                  : uop_int_binary(UOP_IMUL, out_iter[d],
                                   uop_const(DT_INT32, stride));
        flat = (flat == 0) ? term : uop_int_binary(UOP_IADD, flat, term);
      }
      if (flat == 0) flat = uop_const(DT_INT32, 0);
      // Decompose flat_idx into input iters and substitute into the
      // range map.
      u32 in_ext[MAX_DIM];
      u32 in_sid[MAX_DIM];
      for (u32 d = 0; d < n_in; d++) {
        u32 r_sid = u->src[1 + n_out + d];
        if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
        ScalarUop const *ru = &ke->scalar_uops[r_sid];
        if (ru->op != S_RANGE) return 0;
        in_sid[d] = r_sid;
        in_ext[d] = (u32)(ru->extra & 0xFFFFFFFFu);
        if (in_ext[d] == 0) return 0;
      }
      LiftRangeMap reshaped[MAX_DIM];
      u32 n_re = n_ranges;
      for (u32 i = 0; i < n_ranges; i++) reshaped[i] = ranges[i];
      // Append/override the n_in input ranges with derived iters.
      for (u32 d = 0; d < n_in; d++) {
        u32 stride = 1;
        for (u32 e = d + 1; e < n_in; e++) stride *= in_ext[e];
        Term iter_d = (stride == 1) ? flat
                    : uop_int_binary(UOP_IDIV, flat,
                                     uop_const(DT_INT32, stride));
        iter_d = uop_int_binary(UOP_IMOD, iter_d,
                                uop_const(DT_INT32, in_ext[d]));
        // Find the range entry to override.
        int found = 0;
        for (u32 i = 0; i < n_re; i++) {
          if (reshaped[i].scalar_id == in_sid[d]) {
            reshaped[i].axis_uop = iter_d;
            found = 1;
            break;
          }
        }
        if (!found && n_re < MAX_DIM) {
          reshaped[n_re].axis_id   = n_re;
          reshaped[n_re].scalar_id = in_sid[d];
          reshaped[n_re].axis_uop  = iter_d;
          n_re++;
        }
      }
      return lift_scalar_value(ke, u->src[0], reshaped, n_re,
                               out_buf, in_bufs, n_inputs);
    }
    case S_FLIP: {
      // src[0] = body, src[1..n] = LOOP ranges to potentially flip;
      // extra is a u8 bitmask, bit d set => replace iter with
      // (extent - 1 - iter) for axis d when evaluating the body.
      // Implemented by rebuilding the LiftRangeMap for the body so
      // matching range lookups return the flipped UOp expression.
      if (u->src_count < 2) return 0;
      u32 mask = (u32)u->extra;
      u32 ndim = (u32)u->src_count - 1;
      if (ndim > MAX_DIM) return 0;
      LiftRangeMap flipped[MAX_DIM];
      u32 n_flipped = 0;
      // Copy + override ranges that are in the flip bitmask.
      for (u32 i = 0; i < n_ranges; i++) {
        flipped[n_flipped++] = ranges[i];
      }
      for (u32 d = 0; d < ndim; d++) {
        if ((mask & (1u << d)) == 0) continue;
        u32 r_sid = u->src[1 + d];
        // Find the range in `flipped[]` and rewrite its axis_uop to
        // (extent - 1 - axis_uop).
        for (u32 i = 0; i < n_flipped; i++) {
          if (flipped[i].scalar_id != r_sid) continue;
          Term r = flipped[i].axis_uop;
          u32 ext = term_val(heap_read(term_val(r) + 2));
          if (ext == 0) return 0;
          Term ext_m1 = uop_const(DT_INT32, ext - 1);
          flipped[i].axis_uop = uop_int_binary(UOP_ISUB, ext_m1, r);
          break;
        }
      }
      return lift_scalar_value(ke, u->src[0], flipped, n_flipped,
                               out_buf, in_bufs, n_inputs);
    }
    case S_RESHAPE: {
      // Legacy shared-LOOP-refs RESHAPE: src[1..nrng) are LOOP
      // ranges used as both input and output via in-place iter
      // shift.  extra packs out_dims (low 32, 4xu8) and in_dims
      // (high 32, 4xu8).  Body sees iter shifted: in_iter[d] is
      // derived from a flat index built over out_dims, then split
      // by in_dims.  Mirrors S_RESHAPE_V's algorithm but with
      // shared range refs (input and output iters share the same
      // S_RANGE slots, just remapped via the LiftRangeMap override).
      // Without this case, lenet-mnist bench-train falls back to
      // the legacy ScalarUop renderer for ~128 kernels per step
      // (lift_reject "value/unknown-op op=S_?(25)").
      if (u->src_count < 2) return 0;
      u32 nrng = (u32)u->src_count - 1;
      if (nrng > 4 || nrng > MAX_DIM) return 0;
      u32 lo = (u32)(u->extra & 0xFFFFFFFFu);
      u32 hi = (u32)((u->extra >> 32) & 0xFFFFFFFFu);
      u32 out_dims[MAX_DIM] = {0};
      u32 in_dims [MAX_DIM] = {0};
      for (u32 d = 0; d < nrng; d++) {
        out_dims[d] = (lo >> (8 * d)) & 0xFFu;
        in_dims [d] = (hi >> (8 * d)) & 0xFFu;
        if (out_dims[d] == 0 || in_dims[d] == 0) return 0;
      }
      // Out and in must have same numel for the reshape to compose.
      u32 out_numel = 1, in_numel = 1;
      for (u32 d = 0; d < nrng; d++) {
        out_numel *= out_dims[d];
        in_numel  *= in_dims[d];
      }
      if (out_numel != in_numel) return 0;
      // Build out_iter from the shared ranges (each src[1+d] is a
      // shared S_RANGE; the existing range map gives its UOP iter).
      Term out_iter[MAX_DIM];
      for (u32 d = 0; d < nrng; d++) {
        u32 r_sid = u->src[1 + d];
        if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
        ScalarUop const *ru = &ke->scalar_uops[r_sid];
        if (ru->op != S_RANGE) return 0;
        out_iter[d] = lift_lookup_range(ranges, n_ranges, r_sid);
        if (out_iter[d] == 0) return 0;
      }
      // flat_idx = sum(out_iter[d] * out_stride[d]) row-major.
      Term flat = 0;
      for (u32 d = 0; d < nrng; d++) {
        u32 stride = 1;
        for (u32 e = d + 1; e < nrng; e++) stride *= out_dims[e];
        Term term = (stride == 1) ? out_iter[d]
                  : uop_int_binary(UOP_IMUL, out_iter[d],
                                   uop_const(DT_INT32, stride));
        flat = (flat == 0) ? term
             : uop_int_binary(UOP_IADD, flat, term);
      }
      if (flat == 0) flat = uop_const(DT_INT32, 0);
      // Decompose flat into per-dim in_iters and override the shared
      // range mapping so the body sees in_iter[d] for the same
      // scalar_id slot.
      LiftRangeMap reshaped[MAX_DIM];
      u32 n_re = n_ranges;
      for (u32 i = 0; i < n_ranges; i++) reshaped[i] = ranges[i];
      for (u32 d = 0; d < nrng; d++) {
        u32 stride = 1;
        for (u32 e = d + 1; e < nrng; e++) stride *= in_dims[e];
        Term iter_d = (stride == 1) ? flat
                    : uop_int_binary(UOP_IDIV, flat,
                                     uop_const(DT_INT32, stride));
        iter_d = uop_int_binary(UOP_IMOD, iter_d,
                                uop_const(DT_INT32, in_dims[d]));
        u32 sid_d = u->src[1 + d];
        int found = 0;
        for (u32 i = 0; i < n_re; i++) {
          if (reshaped[i].scalar_id == sid_d) {
            reshaped[i].axis_uop = iter_d;
            found = 1;
            break;
          }
        }
        if (!found) {
          if (n_re >= MAX_DIM) return 0;
          reshaped[n_re].scalar_id = sid_d;
          reshaped[n_re].axis_uop  = iter_d;
          n_re++;
        }
      }
      return lift_scalar_value(ke, u->src[0], reshaped, n_re,
                               out_buf, in_bufs, n_inputs);
    }
    case S_ICONST: case S_IADD: case S_ISUB: case S_IMUL:
    case S_IDIV: case S_IMOD: case S_ILT: case S_IAND:
    case S_IWHERE:
      // Integer-side scalar ops -- handled by scalar_to_uop with a
      // UopRangeMap built from the same ranges.
      {
        UopRangeMap srm[MAX_DIM];
        u32 n = (n_ranges < MAX_DIM) ? n_ranges : MAX_DIM;
        for (u32 i = 0; i < n; i++) {
          srm[i].scalar_id = ranges[i].scalar_id;
          srm[i].axis_uop  = ranges[i].axis_uop;
        }
        return scalar_to_uop(ke, sid, srm, n);
      }
    default:
      lift_reject_log(ke, sid, "value/unknown-op");
      return 0;
  }
}

// Walk the scalar arena to find an S_INDEX(slot, ranges...) usage
// for the given DEFINE_PARAM slot id, and read the per-axis extents
// from the referenced S_RANGE leaves.  Used as a fallback when the
// kernel's input_tids[slot] doesn't have a TenDesc (synthetic test
// kernels, or paths where TenDesc isn't yet wired).  Returns 1 on
// success, filling `*ndim_out` and `dims_out[]`.
static int infer_input_shape_from_usage(KernelEntry const *ke, u32 slot,
                                        u32 *ndim_out, u32 *dims_out) {
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (u->op != S_INDEX || u->src_count < 1) continue;
    u32 buf_sid = u->src[0];
    if (buf_sid == 0 || buf_sid >= ke->n_scalar_uops) continue;
    ScalarUop const *bu = &ke->scalar_uops[buf_sid];
    if (bu->op != S_DEFINE_PARAM || (u32)bu->extra != slot) continue;
    u32 ndim = (u32)u->src_count - 1;
    if (ndim == 0 || ndim > MAX_DIM) continue;
    for (u32 d = 0; d < ndim; d++) {
      u32 r_sid = u->src[1 + d];
      if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
      ScalarUop const *ru = &ke->scalar_uops[r_sid];
      if (ru->op != S_RANGE) return 0;
      dims_out[d] = (u32)(ru->extra & 0xFFFFFFFFu);
    }
    *ndim_out = ndim;
    return 1;
  }
  return 0;
}

// Build a UOP_BUFFER for kernel input slot `slot`.  Prefers the
// TenDesc shape when present; falls back to inferring from the
// S_INDEX usage in the scalar arena (test kernels, synthetic shapes).
//
// The `instance` field on UOP_BUFFER ensures distinct slots get
// distinct Terms even when shape collides; we use slot+1 (since
// instance=0 is the output's reserved value).
static Term lift_input_buffer(KernelEntry const *ke, u32 slot) {
  if (slot >= ke->n_inputs) return 0;
  u32 dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
  u32 tid = (ke->input_tids != NULL) ? ke->input_tids[slot] : 0;
  u32 inst = slot + 1;
  if (tid != 0 && tid < TENS_NEXT) {
    TenDesc const *td = &TENS[tid];
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                           td->view.shape.ndim, td->view.shape.dims, inst);
  }
  u32 ndim = 0;
  u32 dims[MAX_DIM] = {0};
  if (infer_input_shape_from_usage(ke, slot, &ndim, dims)) {
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, ndim, dims, inst);
  }
  // No usage found -- 1D dummy buffer.
  u32 dummy[1] = { 1 };
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, 1, dummy, inst);
}

// Walk the scalar arena for an S_INDEX rooted at S_DEFINE_OUTPUT and
// read per-axis extents from the referenced ranges.  Returns 1 on
// success.  Mirrors infer_input_shape_from_usage but for the output.
static int infer_output_shape_from_usage(KernelEntry const *ke,
                                         u32 *ndim_out, u32 *dims_out) {
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (u->op != S_INDEX || u->src_count < 1) continue;
    u32 buf_sid = u->src[0];
    if (buf_sid == 0 || buf_sid >= ke->n_scalar_uops) continue;
    ScalarUop const *bu = &ke->scalar_uops[buf_sid];
    if (bu->op != S_DEFINE_OUTPUT) continue;
    u32 ndim = (u32)u->src_count - 1;
    if (ndim == 0 || ndim > MAX_DIM) continue;
    for (u32 d = 0; d < ndim; d++) {
      u32 r_sid = u->src[1 + d];
      if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
      ScalarUop const *ru = &ke->scalar_uops[r_sid];
      if (ru->op != S_RANGE) return 0;
      dims_out[d] = (u32)(ru->extra & 0xFFFFFFFFu);
    }
    *ndim_out = ndim;
    return 1;
  }
  return 0;
}

// Build a UOP_BUFFER for the kernel's output, using the actual output
// INDEX expression to determine ndim+dims (not BUFFERIZE.src[1..],
// which counts ALL ranges including reduce axes that don't appear in
// the output index).
static Term lift_output_buffer(KernelEntry const *ke,
                               LiftRangeMap const *ranges, u32 n_ranges) {
  u32 ndim = 0;
  u32 dims[MAX_DIM] = {0};
  if (infer_output_shape_from_usage(ke, &ndim, dims)) {
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, ke->output_dtype, ndim, dims, 0);
  }
  // Fallback: use all ranges' extents.  Conservative; may produce
  // higher-rank output than the actual store but the renderer will
  // still index it.
  u32 n = (n_ranges < MAX_DIM) ? n_ranges : MAX_DIM;
  for (u32 i = 0; i < n; i++) {
    Term r = ranges[i].axis_uop;
    dims[i] = term_val(heap_read(term_val(r) + 2));
  }
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, ke->output_dtype, n, dims, 0);
}

// Reject diagnostic: when env-gated, print the first ScalarUop the
// lifter doesn't handle.  Helps prioritise which shapes B3-finish
// targets next.
static void lift_reject_log(KernelEntry const *ke, u32 sid,
                            const char *where) {
  static int reject_log_inited = 0;
  static int reject_log_on     = 0;
  if (!reject_log_inited) {
    char const *e = getenv("THVM_DUMP_LIFT_REJECT");
    reject_log_on    = (e != NULL && e[0] == '1');
    reject_log_inited = 1;
  }
  if (!reject_log_on) return;
  if (sid == 0 || sid >= ke->n_scalar_uops) {
    fprintf(stderr, "lift reject: %s sid=%u (out of range)\n", where, sid);
    return;
  }
  ScalarUop const *u = &ke->scalar_uops[sid];
  fprintf(stderr,
          "lift reject: %s op=%s(%u) src_count=%u dtype=%u\n",
          where, scalar_op_name(u->op), u->op, u->src_count, u->dtype);
}

// Find the BUFFERIZE root in scalar_uops and lift the whole kernel
// program to a UOp DAG.  Fills `out` with the rendered-ready
// store_root + buffer terms.
// Synthesize the canonical matmul UOp DAG from a TileGemmInfo.
// Mirrors what F2b's rmu_detect_matmul_tc pattern recognises:
//   STORE(C, m*N+n, OPT(REDUCE(MUL(A[m*K+k], B[k*N+n]), SUM, k), TC, _))
// Used when the scheduling pipeline goes through the dedicated GEMM
// path (no scalar_uops arena) so the lifter can still produce a valid
// UOp DAG for the renderer.
static int kernel_lift_from_gemm(KernelEntry const *ke, KernelUopLift *out) {
  TileGemmInfo gemm = {0};
  if (!tile_collect_mma_plan((KernelEntry *)ke, &gemm)) return 0;
  if (gemm.M == 0 || gemm.N == 0 || gemm.K == 0) return 0;
  if (gemm.a_input >= ke->n_inputs || gemm.b_input >= ke->n_inputs) return 0;

  // Three buffers with distinct shapes (no hash-cons collision).
  u32 dims_a[2] = { gemm.M, gemm.K };
  u32 dims_b[2] = { gemm.K, gemm.N };
  u32 dims_c[2] = { gemm.M, gemm.N };
  u32 a_dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[gemm.a_input] : DT_FP32;
  u32 b_dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[gemm.b_input] : DT_FP32;
  Term A   = uop_buffer_inst(UOP_SCOPE_GLOBAL, a_dtype, 2, dims_a,
                             gemm.a_input + 1);
  Term B   = uop_buffer_inst(UOP_SCOPE_GLOBAL, b_dtype, 2, dims_b,
                             gemm.b_input + 1);
  Term C   = uop_buffer_inst(UOP_SCOPE_GLOBAL, ke->output_dtype, 2,
                             dims_c, 0);

  // Range axes: M (output 0), N (output 1), K (reduce axis 2).
  Term r_m = uop_range(0, 0 /*LOOP*/,   gemm.M);
  Term r_n = uop_range(1, 0 /*LOOP*/,   gemm.N);
  Term r_k = uop_range(2, 1 /*REDUCE*/, gemm.K);

  // A[m * K + k]  (or [m, k]; row-major linearisation).
  Term mK    = uop_int_binary(UOP_IMUL, r_m, uop_const(DT_INT32, gemm.K));
  Term addrA = uop_int_binary(UOP_IADD, mK, r_k);
  Term ldA   = uop_index_e(A, addrA);
  // B[k * N + n]
  Term kN    = uop_int_binary(UOP_IMUL, r_k, uop_const(DT_INT32, gemm.N));
  Term addrB = uop_int_binary(UOP_IADD, kN, r_n);
  Term ldB   = uop_index_e(B, addrB);
  // OPT(REDUCE(MUL(A,B), SUM, k_axis=2), TC, 0)
  Term mul   = uop_binary(UOP_MUL, ldA, ldB);
  Term red   = uop_reduce(REDUCE_SUM, /*axis=*/2, mul);
  Term tc_v  = uop_opt(red, UOP_OPT_TC, 0);
  // C[m * N + n]
  Term mN    = uop_int_binary(UOP_IMUL, r_m, uop_const(DT_INT32, gemm.N));
  Term addrC = uop_int_binary(UOP_IADD, mN, r_n);
  Term store = uop_store(C, addrC, tc_v);

  // Populate KernelUopLift.  Inputs go in slot order even when
  // a_input / b_input aren't 0/1 (the renderer's buffer-name map
  // honours the order we register).
  out->n_inputs = ke->n_inputs;
  if (out->n_inputs > KERNEL_LIFT_MAX_INPUT) return 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (i == gemm.a_input)      out->in_bufs[i] = A;
    else if (i == gemm.b_input) out->in_bufs[i] = B;
    else                        out->in_bufs[i] = lift_input_buffer(ke, i);
  }
  out->out_buf    = C;
  out->store_root = store;
  return 1;
}

// Synthesize the conv2d_flat UOp DAG from a TileConv2DInfo.  Mirrors
// what rmt_emit_conv2d_flat produces but as UOp DAG so the renderer
// rewrite path can take over.  Layout:
//
//   for r_out in [0, c_out * patches):
//     co     = r_out / patches
//     patch  = r_out % patches
//     bi     = patch / spatial_patches
//     sp     = patch % spatial_patches
//     ow     = sp % w_out
//     oh     = sp / w_out
//     acc    = 0
//     for q in [0, KRED) where KRED = c_in * kh * kw:
//       kw_v = q % kw
//       qk   = q / kw
//       kh_v = qk % kh
//       ci   = qk / kh
//       wi   = w_offset + co * w_stride0 + q * w_stride1
//       xi   = x_offset + bi * x_stride_b + ci * x_stride2
//                       + (oh + kh_v) * x_stride0
//                       + (ow + kw_v) * x_stride1
//       acc += W[wi] * X[xi]
//     out[r_out] = acc
//
// Multi-input "patch_input_count" cases (im2col split into multiple
// input buffers via a switch on q) are handled below with a nested
// UOP_IWHERE chain selecting which input slot to read based on q.
static int kernel_lift_from_conv2d(KernelEntry const *ke,
                                   KernelUopLift *out) {
  TileConv2DInfo conv;
  if (!tile_analyze_conv2d_flat(ke, &conv)) return 0;
  if (conv.batch == 0 || conv.h_out == 0 || conv.w_out == 0) return 0;
  if (conv.spatial_patches == 0) return 0;
  if (conv.w_input >= ke->n_inputs) return 0;
  if (conv.patch_input_count == 0 && conv.x_input >= ke->n_inputs) return 0;
  if (conv.patch_input_count != 0
      && (u64)conv.patch_input_base + conv.patch_input_count > ke->n_inputs) return 0;
  if (ke->n_inputs > KERNEL_LIFT_MAX_INPUT) return 0;

  // Buffers.  Conv2d output is [c_out, patches].
  u32 dims_w[2] = { conv.c_out, conv.c_in * conv.kh * conv.kw };
  u32 dims_x[1] = { 1024 };  // shape unused; lifter walks affine addr
  u32 dims_o[1] = { conv.c_out * conv.patches };
  u32 w_dt = (ke->input_dtypes != NULL) ? ke->input_dtypes[conv.w_input] : DT_FP32;
  u32 x_dt = (conv.x_input < ke->n_inputs && ke->input_dtypes != NULL)
             ? ke->input_dtypes[conv.x_input] : DT_FP32;
  Term W = uop_buffer_inst(UOP_SCOPE_GLOBAL, w_dt, 2, dims_w, conv.w_input + 1);
  Term X = (conv.patch_input_count == 0)
           ? uop_buffer_inst(UOP_SCOPE_GLOBAL, x_dt, 1, dims_x, conv.x_input + 1)
           : 0;  // multi-input path doesn't use a single X buffer
  Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, ke->output_dtype, 1, dims_o, 0);

  u32 patches = conv.patches;
  u32 KRED    = conv.c_in * conv.kh * conv.kw;
  if (KRED == 0) return 0;

  // Range axes.
  Term r_out = uop_range(0, 0 /*LOOP*/,   conv.c_out * patches);
  Term r_q   = uop_range(1, 1 /*REDUCE*/, KRED);

  // Decompose r_out -> (co, patch) and patch -> (bi, oh, ow).
  Term k_patches = uop_const(DT_INT32, patches);
  Term k_spatial = uop_const(DT_INT32, conv.spatial_patches);
  Term k_w_out   = uop_const(DT_INT32, conv.w_out);
  Term co     = uop_int_binary(UOP_IDIV, r_out, k_patches);
  Term patch  = uop_int_binary(UOP_IMOD, r_out, k_patches);
  Term bi     = uop_int_binary(UOP_IDIV, patch, k_spatial);
  Term sp     = uop_int_binary(UOP_IMOD, patch, k_spatial);
  Term ow     = uop_int_binary(UOP_IMOD, sp, k_w_out);
  Term oh     = uop_int_binary(UOP_IDIV, sp, k_w_out);

  // Decompose r_q -> (ci, kh_v, kw_v).
  Term k_kw   = uop_const(DT_INT32, conv.kw);
  Term k_kh   = uop_const(DT_INT32, conv.kh);
  Term kw_v   = uop_int_binary(UOP_IMOD, r_q, k_kw);
  Term qk     = uop_int_binary(UOP_IDIV, r_q, k_kw);
  Term kh_v   = uop_int_binary(UOP_IMOD, qk, k_kh);
  Term ci     = uop_int_binary(UOP_IDIV, qk, k_kh);

  // wi = w_offset + co * w_stride0 + q * w_stride1
  Term k_w_off = uop_const(DT_INT32, (u32)conv.w_offset);
  Term k_ws0   = uop_const(DT_INT32, (u32)conv.w_stride0);
  Term k_ws1   = uop_const(DT_INT32, (u32)conv.w_stride1);
  Term wi_co   = uop_int_binary(UOP_IMUL, co, k_ws0);
  Term wi_q    = uop_int_binary(UOP_IMUL, r_q, k_ws1);
  Term wi_sum  = uop_int_binary(UOP_IADD, wi_co, wi_q);
  Term wi      = uop_int_binary(UOP_IADD, k_w_off, wi_sum);
  Term ldW     = uop_index_e(W, wi);

  Term ldX = 0;
  if (conv.patch_input_count == 0) {
    // Single X buffer: xi = x_offset + bi * x_stride_b + ci * x_stride2
    //                          + (oh + kh_v) * x_stride0
    //                          + (ow + kw_v) * x_stride1
    Term k_x_off = uop_const(DT_INT32, (u32)conv.x_offset);
    Term k_xsb   = uop_const(DT_INT32, (u32)conv.x_stride_b);
    Term k_xs0   = uop_const(DT_INT32, (u32)conv.x_stride0);
    Term k_xs1   = uop_const(DT_INT32, (u32)conv.x_stride1);
    Term k_xs2   = uop_const(DT_INT32, (u32)conv.x_stride2);
    Term xi_b    = uop_int_binary(UOP_IMUL, bi, k_xsb);
    Term xi_ci   = uop_int_binary(UOP_IMUL, ci, k_xs2);
    Term oh_kh   = uop_int_binary(UOP_IADD, oh, kh_v);
    Term xi_h    = uop_int_binary(UOP_IMUL, oh_kh, k_xs0);
    Term ow_kw   = uop_int_binary(UOP_IADD, ow, kw_v);
    Term xi_w    = uop_int_binary(UOP_IMUL, ow_kw, k_xs1);
    Term xi_sum1 = uop_int_binary(UOP_IADD, xi_b, xi_ci);
    Term xi_sum2 = uop_int_binary(UOP_IADD, xi_h, xi_w);
    Term xi_sum  = uop_int_binary(UOP_IADD, xi_sum1, xi_sum2);
    Term xi      = uop_int_binary(UOP_IADD, k_x_off, xi_sum);
    ldX          = uop_index_e(X, xi);
  } else {
    // Multi-input X via switch on q: xv = (q < 1) ? load_pi0 :
    //                                     (q < 2) ? load_pi1 : ...
    // Each pi has its own input slot + per-axis strides; address is
    // `pv->offset + bi * psb + oh * psh + ow * psw` (no ci/kh/kw --
    // those are encoded statically by which pi the q lands in).
    if (ke->input_views == NULL) return 0;
    // Build bottom-up so the chain is right-leaning.
    ldX = uop_const(x_dt, 0);  // default for q out of range
    for (u32 pi = conv.patch_input_count; pi > 0; pi--) {
      u32 idx = pi - 1;
      u32 slot = conv.patch_input_base + idx;
      View const *pv = &ke->input_views[slot];
      i32 psb = 0, psh = 0, psw = 0;
      if (pv->shape.ndim == 3) { psh = pv->strides[1]; psw = pv->strides[2]; }
      else                     { psb = pv->strides[1]; psh = pv->strides[2];
                                 psw = pv->strides[3]; }
      u32 dt_pi = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
      u32 dims_pi[1] = { 1024 };
      Term Xpi  = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt_pi, 1, dims_pi,
                                  slot + 1);
      Term k_off = uop_const(DT_INT32, (u32)pv->offset);
      Term k_psb = uop_const(DT_INT32, (u32)psb);
      Term k_psh = uop_const(DT_INT32, (u32)psh);
      Term k_psw = uop_const(DT_INT32, (u32)psw);
      Term a_b = uop_int_binary(UOP_IMUL, bi, k_psb);
      Term a_h = uop_int_binary(UOP_IMUL, oh, k_psh);
      Term a_w = uop_int_binary(UOP_IMUL, ow, k_psw);
      Term a_bh = uop_int_binary(UOP_IADD, a_b, a_h);
      Term a_bhw = uop_int_binary(UOP_IADD, a_bh, a_w);
      Term addr_pi = uop_int_binary(UOP_IADD, k_off, a_bhw);
      Term ld_pi   = uop_index_e(Xpi, addr_pi);
      // cond: q < (idx + 1)  (i.e. q matches case `idx` in the
      // bottom-up chain we're building).
      Term k_idxp1 = uop_const(DT_INT32, idx + 1);
      Term cond    = uop_int_binary(UOP_ILT, r_q, k_idxp1);
      ldX = uop_iwhere(cond, ld_pi, ldX);
    }
  }

  // MUL + REDUCE_SUM_q(W * X)
  Term mul   = uop_binary(UOP_MUL, ldW, ldX);
  Term red   = uop_reduce(REDUCE_SUM, /*axis=*/1, mul);

  Term store = uop_store(C, r_out, red);

  out->n_inputs = ke->n_inputs;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (i == conv.w_input) {
      out->in_bufs[i] = W;
    } else if (conv.patch_input_count == 0 && i == conv.x_input) {
      out->in_bufs[i] = X;
    } else if (conv.patch_input_count != 0
               && i >= conv.patch_input_base
               && i < conv.patch_input_base + conv.patch_input_count) {
      // Re-construct the same Xpi the load chain referenced, so
      // rmu_buf_name's lookup hits the correct entry.
      View const *pv = (ke->input_views != NULL) ? &ke->input_views[i] : NULL;
      u32 dt_pi = (ke->input_dtypes != NULL) ? ke->input_dtypes[i] : DT_FP32;
      u32 dims_pi[1] = { (pv != NULL) ? (pv->numel ? pv->numel : 1024u) : 1024u };
      out->in_bufs[i] = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt_pi, 1,
                                        dims_pi, i + 1);
    } else {
      out->in_bufs[i] = lift_input_buffer(ke, i);
    }
  }
  out->out_buf    = C;
  out->store_root = store;
  return 1;
}

fn int kernel_lift_to_uop(KernelEntry const *ke, KernelUopLift *out) {
  if (ke == NULL || out == NULL) return 0;
  // Empty / GEMM-only kernel: try the gemm-shape lifter.  The dedicated
  // GEMM path bypasses rangeify, so scalar_uops is NULL and the
  // ScalarUop walker below would reject.
  if (ke->scalar_uops == NULL) {
    if (kernel_lift_from_gemm(ke, out)) return 1;
    if (kernel_lift_from_conv2d(ke, out)) return 1;
    static int reject_log_inited = 0;
    static int reject_log_on     = 0;
    if (!reject_log_inited) {
      char const *e = getenv("THVM_DUMP_LIFT_REJECT");
      reject_log_on    = (e != NULL && e[0] == '1');
      reject_log_inited = 1;
    }
    if (reject_log_on) {
      fprintf(stderr,
              "lift reject: entry/no-scalar-arena n_inputs=%u "
              "n_ops=%u n_tile_uops=%u\n",
              ke->n_inputs, ke->n_ops, ke->n_tile_uops);
      // Probe: source_uop populated on each KProgOp?  If yes, the
      // future kernel_lift_from_kprog can use it as the lift root.
      if (ke->n_ops > 0 && ke->program != NULL) {
        u32 set = 0;
        for (u32 i = 0; i < ke->n_ops; i++) {
          if (ke->program[i].source_uop != 0) set++;
        }
        Term last = ke->program[ke->n_ops - 1].source_uop;
        fprintf(stderr,
                "  source_uop coverage: %u/%u set; last.tag=%u last.ext=%u\n",
                set, ke->n_ops,
                (unsigned)term_tag(last), (unsigned)term_ext(last));
      }
      // Layout dump for the conv2d-flat rejecting case so the
      // operator can see what shape kernel_lift_from_conv2d
      // doesn't yet handle.  Iterate over input_views (if any)
      // to print rank + dims for each, then summarise the
      // KProgOp opcode histogram.
      if (ke->input_views != NULL) {
        for (u32 i = 0; i < ke->n_inputs && i < 32; i++) {
          View const *v = &ke->input_views[i];
          fprintf(stderr, "  in[%u] ndim=%u dims=[", i, v->shape.ndim);
          for (u32 d = 0; d < v->shape.ndim && d < MAX_DIM; d++) {
            fprintf(stderr, "%s%u", d ? "," : "", v->shape.dims[d]);
          }
          fprintf(stderr, "] strides=[");
          for (u32 d = 0; d < v->shape.ndim && d < MAX_DIM; d++) {
            fprintf(stderr, "%s%d", d ? "," : "", v->strides[d]);
          }
          fprintf(stderr, "]\n");
        }
      }
      if (ke->program != NULL && ke->n_ops > 0) {
        u32 op_hist[256] = {0};
        for (u32 i = 0; i < ke->n_ops; i++) {
          u32 op = ke->program[i].opcode;
          if (op < 256) op_hist[op]++;
        }
        fprintf(stderr, "  prog_ops:");
        for (u32 op = 0; op < 256; op++) {
          if (op_hist[op] > 0) {
            fprintf(stderr, " op%u=%u", op, op_hist[op]);
          }
        }
        fputc('\n', stderr);
      }
    }
    return 0;
  }
  // Find the BUFFERIZE root.
  u32 buf_sid = 0;
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_BUFFERIZE) { buf_sid = i; break; }
  }
  if (buf_sid == 0) { lift_reject_log(ke, 0, "entry/no-bufferize-root"); return 0; }
  ScalarUop const *bu = &ke->scalar_uops[buf_sid];
  if (bu->src_count < 1) return 0;

  // Build LiftRangeMap from BUFFERIZE.src[1..n] (axis-id ordering)
  // PLUS every other S_RANGE leaf in the arena (per-USE ranges that
  // appear inside S_INDEX_E without being on the BUFFERIZE boundary).
  // The first n_buf entries get axis_ids matching BUFFERIZE order; the
  // rest get axis_ids continuing past n_buf.
  //
  // When ke->axes carries autotune-applied opts (UPCAST/UNROLL/LOCAL/
  // GROUP/GROUPTOP/GLOBAL/SWAP), each BUFFERIZE range's axis_uop is
  // replaced by a linearised expression over the post-mutation axes.
  // Splitting axis a with arg L into (outer of size N/L, inner of size L)
  // produces axis_uop_a = outer * L + inner where outer/inner are new
  // UOP_RANGE leaves with the appropriate axis_types.  The renderer
  // walks the addr/value tree and emits one for-loop / thread bind per
  // discovered UOP_RANGE leaf, so split semantics flow through to MSL.
  u32 n_buf = (u32)bu->src_count - 1;
  if (n_buf > MAX_DIM) return 0;
  LiftRangeMap ranges[MAX_DIM * 2];
  u32 n_ranges = 0;
  // Working list of "current axes" after applied-opt replay.  Each
  // entry contributes `axis_uop * factor` to its origin BUFFERIZE
  // range's linearised expression.  Initially: one entry per
  // BUFFERIZE range with factor=1 and axis_type/extent from S_RANGE.
  typedef struct {
    u32 axis_type;
    u32 extent;
    u32 origin;       // index into BUFFERIZE.src[1..n_buf]
    u32 factor;       // multiplier in linearisation
    u32 opt_kind;     // UOP_OPT_* or 0xFFu (no opt wrap)
    u32 opt_factor;   // factor argument carried into UOP_OPT
  } SplitAxis;
  #define SPLIT_AXIS_NO_OPT 0xFFu
  SplitAxis cur[MAX_AXES];
  u32 n_cur = 0;
  u32 r_sids[MAX_DIM];
  for (u32 i = 0; i < n_buf; i++) {
    u32 r_sid = bu->src[1 + i];
    if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
    ScalarUop const *ru = &ke->scalar_uops[r_sid];
    if (ru->op != S_RANGE) return 0;
    r_sids[i] = r_sid;
    cur[n_cur].axis_type  = (u32)(ru->extra >> 32) & 0xFFu;
    cur[n_cur].extent     = (u32)(ru->extra & 0xFFFFFFFFu);
    cur[n_cur].origin     = i;
    cur[n_cur].factor     = 1;
    cur[n_cur].opt_kind   = SPLIT_AXIS_NO_OPT;
    cur[n_cur].opt_factor = 0;
    n_cur++;
  }
  // Replay applied_opts when present.  Pre-mutated axes (test seam:
  // direct assignment to ke->axes->axis_types/full_shape with no
  // applied_opts log) are honoured when n_buf == 1: distribute axes
  // across the single origin in declaration order.
  //
  // Skip replay for reduce kernels: REDUCE / GROUP_REDUCE axes target
  // per-USE ranges that aren't in BUFFERIZE.src, and the S_REDUCE_SUM
  // lifter requires a single UOP_RANGE for the reduce axis -- splitting
  // would produce a compound expression it can't decode.  Reduce-
  // dimension autotune effects flow through a different path.
  KernelAxes const *kax = ke->axes;
  u32 n_applied = (kax != NULL) ? (u32)kax->n_applied : 0;
  // The previous `has_reduce_axis` short-circuit zeroed both
  // n_applied and kax for any kernel touching a reduce dimension --
  // overly conservative.  Now per-USE handling is split across:
  //
  //   1. The replay loop below: skips opts targeting per-USE axes
  //      via `if (o.axis >= n_cur) continue;`.  Per-USE axes don't
  //      live in cur[] (which only tracks BUFFERIZE-derived axes
  //      and their splits), so any opt with axis >= n_cur is by
  //      construction a per-USE opt deferred to step 2.
  //
  //   2. The per-USE auxiliary scan further down: walks
  //      ke->scalar_uops, finds each per-USE S_RANGE, and wraps it
  //      in UOP_OPT_GROUP_REDUCE when applied_opts has a matching
  //      KOP_GROUP/KOP_GROUPTOP.
  //
  //   3. Test-seam (n_axes > n_buf, n_buf == 1) still bails on
  //      reduce-tail axes because the linearisation across LOOP +
  //      REDUCE into a single origin would be wrong.  Detected via
  //      the axis_types[] scan below.
  int has_reduce_axis = 0;
  if (kax != NULL) {
    for (u32 a = 0; a < kax->n_axes; a++) {
      if (kax->axis_types[a] == KAX_REDUCE
          || kax->axis_types[a] == KAX_GROUP_REDUCE) {
        has_reduce_axis = 1;
        break;
      }
    }
  }
  // Test-seam guard: only kill kax for the test-seam branch when
  // reduce axes are present (and only when the test-seam would
  // otherwise fire).  The replay loop and per-USE scan stay live.
  KernelAxes const *kax_for_test_seam =
      (has_reduce_axis) ? NULL : kax;
  if (n_applied > 0) {
    KOpt const *opts = kax->applied_opts;
    for (u32 oi = 0; oi < n_applied; oi++) {
      KOpt o = opts[oi];
      if (o.axis >= n_cur) continue;  // per-USE; handled below
      u8 op = o.op;
      if (op == KOP_UPCAST || op == KOP_UNROLL || op == KOP_LOCAL
          || op == KOP_GROUP || op == KOP_GROUPTOP) {
        if (o.arg == 0 || cur[o.axis].extent % o.arg != 0) return 0;
        if (n_cur >= MAX_AXES) return 0;
        // Shift right.
        for (u32 j = n_cur; j > (u32)o.axis + 1; j--) cur[j] = cur[j - 1];
        u32 outer_extent = cur[o.axis].extent / o.arg;
        u32 inner_extent = o.arg;
        u8  inner_kax    = (op == KOP_UPCAST)   ? KAX_UPCAST
                         : (op == KOP_UNROLL)   ? KAX_UNROLL
                         : (op == KOP_LOCAL)    ? KAX_LOCAL
                         : KAX_GROUP_REDUCE;
        u32 origin = cur[o.axis].origin;
        u32 factor = cur[o.axis].factor;
        // Outer keeps original axis_type, inner takes the opt's type.
        cur[o.axis].extent = outer_extent;
        cur[o.axis].factor = factor * inner_extent;
        cur[o.axis + 1].axis_type = inner_kax;
        cur[o.axis + 1].extent    = inner_extent;
        cur[o.axis + 1].origin    = origin;
        cur[o.axis + 1].factor    = factor;
        // OPT-annotation stamping for the inner range:
        //   UPCAST / UNROLL  -> renderer fires `#pragma unroll(N)` above
        //                       the inner for-loop.
        //   GROUP / GROUPTOP -> renderer recognises a threadgroup-
        //                       shared-accumulator pattern (Phase 3
        //                       follow-on) and emits the cooperative
        //                       reduce shape.  Stamping the OPT here
        //                       is harmless until the renderer learns
        //                       to consume it; the existing scalar
        //                       accumulator path ignores unfamiliar
        //                       opt kinds.
        //   LOCAL            -> axis_type=KAX_LOCAL alone drives the
        //                       `tt`/`tg` thread bind in the renderer.
        cur[o.axis + 1].opt_kind   =
            (op == KOP_UPCAST)                          ? UOP_OPT_UPCAST
          : (op == KOP_UNROLL)                          ? UOP_OPT_UNROLL
          : (op == KOP_GROUP || op == KOP_GROUPTOP)     ? UOP_OPT_GROUP_REDUCE
          :                                               SPLIT_AXIS_NO_OPT;
        cur[o.axis + 1].opt_factor =
            (op == KOP_UPCAST || op == KOP_UNROLL
             || op == KOP_GROUP || op == KOP_GROUPTOP)  ? inner_extent
          :                                               0;
        n_cur++;
      } else if (op == KOP_GLOBAL) {
        if (cur[o.axis].axis_type != KAX_LOOP || o.arg != cur[o.axis].extent) {
          return 0;
        }
        cur[o.axis].axis_type = KAX_GLOBAL;
      } else if (op == KOP_SWAP) {
        if ((u32)o.arg >= n_cur) return 0;
        SplitAxis t = cur[o.axis];
        cur[o.axis] = cur[o.arg];
        cur[o.arg]  = t;
      } else if (op == KOP_TC) {
        // Tensor-core opt is metadata-only; pattern-matched in render.
      } else {
        return 0;
      }
    }
  } else if (kax_for_test_seam != NULL
             && kax_for_test_seam->n_axes > n_buf && n_buf == 1) {
    kax = kax_for_test_seam;
    // Test seam: ke->axes was hand-mutated without going through
    // axes_apply_opt.  Honour the assigned axis structure when there's
    // a single origin to map to.  Replace cur[0] with kax->n_axes
    // entries, all origin=0, factors picked so the linearisation
    // matches row-major across the assigned axes.
    if (kax->n_axes > MAX_AXES) return 0;
    u32 product = 1;
    for (u32 a = 0; a < kax->n_axes; a++) product *= kax->full_shape[a];
    if (product != cur[0].extent) return 0;
    n_cur = 0;
    u32 inner_prod = 1;
    for (u32 a = kax->n_axes; a > 0; a--) {
      u32 idx = a - 1;
      cur[idx].axis_type  = kax->axis_types[idx];
      cur[idx].extent     = kax->full_shape[idx];
      cur[idx].origin     = 0;
      cur[idx].factor     = inner_prod;
      cur[idx].opt_kind   = SPLIT_AXIS_NO_OPT;
      cur[idx].opt_factor = 0;
      inner_prod *= kax->full_shape[idx];
    }
    n_cur = kax->n_axes;
  }
  // Allocate UOP_RANGE leaves for each post-replay axis and compose
  // per-origin linearised expressions in row-major order (left-to-right
  // axes are outermost; accumulating from the left).
  Term origin_expr[MAX_DIM] = {0};
  for (u32 a = 0; a < n_cur; a++) {
    Term r = uop_range(a, cur[a].axis_type, cur[a].extent);
    if (cur[a].opt_kind != SPLIT_AXIS_NO_OPT) {
      r = uop_opt(r, cur[a].opt_kind, cur[a].opt_factor);
    }
    Term term = (cur[a].factor == 1)
              ? r
              : uop_int_binary(UOP_IMUL, r,
                               uop_const(DT_INT32, cur[a].factor));
    u32 origin = cur[a].origin;
    origin_expr[origin] = (origin_expr[origin] == 0)
        ? term
        : uop_int_binary(UOP_IADD, origin_expr[origin], term);
  }
  for (u32 i = 0; i < n_buf; i++) {
    if (origin_expr[i] == 0) origin_expr[i] = uop_const(DT_INT32, 0);
    ranges[n_ranges].axis_id   = n_ranges;
    ranges[n_ranges].scalar_id = r_sids[i];
    ranges[n_ranges].axis_uop  = origin_expr[i];
    n_ranges++;
  }
  // Per-USE auxiliary S_RANGE leaves (not in BUFFERIZE) -- find by
  // sweeping the arena.  Skip dups.
  //
  // When `ke->axes` has applied opts targeting per-USE axes
  // (typically KOP_GROUP / KOP_GROUPTOP on a REDUCE axis), wrap
  // the corresponding UOP_RANGE in `UOP_OPT(_, GROUP_REDUCE,
  // arg)` so the renderer (rmu_emit_store_reduce) emits the
  // threadgroup-shared cooperative reduce shape.  Per-USE ranges
  // appear in declaration order in the scalar arena and are
  // assumed to map 1:1 to ke->axes axis_types[n_buf..n_axes) in
  // the same order -- this matches how rangeify emits them.
  KernelAxes const *orig_kax = ke->axes;
  u32 per_use_idx = 0;
  for (u32 i = 1; i < ke->n_scalar_uops && n_ranges < MAX_DIM * 2; i++) {
    if (ke->scalar_uops[i].op != S_RANGE) continue;
    int seen = 0;
    for (u32 j = 0; j < n_ranges; j++) {
      if (ranges[j].scalar_id == i) { seen = 1; break; }
    }
    if (seen) continue;
    u32 axis_type = (u32)(ke->scalar_uops[i].extra >> 32) & 0xFFu;
    u32 extent    = (u32)(ke->scalar_uops[i].extra & 0xFFFFFFFFu);
    Term r = uop_range(n_ranges, axis_type, extent);
    // Map this per-USE range to its position in ke->axes:
    // axes index = n_buf + per_use_idx (per-USE entries follow
    // BUFFERIZE entries in axes_default_for / autotune layout).
    u32 axes_idx = n_buf + per_use_idx;
    per_use_idx++;
    if (orig_kax != NULL && axes_idx < orig_kax->n_axes) {
      for (u32 oi = 0; oi < (u32)orig_kax->n_applied; oi++) {
        KOpt const *o = &orig_kax->applied_opts[oi];
        if (o->axis == (u8)axes_idx
            && (o->op == KOP_GROUP || o->op == KOP_GROUPTOP)
            && o->arg > 0
            && extent % o->arg == 0) {
          r = uop_opt(r, UOP_OPT_GROUP_REDUCE, o->arg);
          break;
        }
      }
    }
    ranges[n_ranges].axis_id   = n_ranges;
    ranges[n_ranges].scalar_id = i;
    ranges[n_ranges].axis_uop  = r;
    n_ranges++;
  }

  // Build buffers.
  if (ke->n_inputs > KERNEL_LIFT_MAX_INPUT) return 0;
  out->n_inputs = ke->n_inputs;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    Term in_buf = lift_input_buffer(ke, i);
    if (in_buf == 0) return 0;
    out->in_bufs[i] = in_buf;
  }
  out->out_buf = lift_output_buffer(ke, ranges, n_ranges);
  if (out->out_buf == 0) return 0;

  // Lift the body STORE.
  u32 store_sid = bu->src[0];
  if (store_sid == 0 || store_sid >= ke->n_scalar_uops) return 0;
  ScalarUop const *su = &ke->scalar_uops[store_sid];
  if (su->op != S_STORE || su->src_count != 2) return 0;
  Term ignored;
  Term addr = lift_scalar_index(ke, su->src[0], ranges, n_ranges,
                                out->out_buf, out->in_bufs,
                                out->n_inputs, &ignored);
  if (addr == 0) return 0;
  Term value = lift_scalar_value(ke, su->src[1], ranges, n_ranges,
                                 out->out_buf, out->in_bufs,
                                 out->n_inputs);
  if (value == 0) return 0;
  out->store_root = uop_store(out->out_buf, addr, value);
  return 1;
}
