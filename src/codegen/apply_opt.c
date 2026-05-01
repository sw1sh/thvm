// codegen/apply_opt.c -- mutate a KernelAxes by applying one TOpt.
//
// Owns the axis rewrite previously done by WL TKernelApplyOpt; WL
// is a thin LibraryLink wrapper now.
//
// Each opt class:
//
//   UPCAST / UNROLL / LOCAL / GROUP / GROUPTOP
//     Split full_shape[axis] into outer (= old/arg) + inner (= arg).
//     Insert a new axis after `axis` with the opt's KAX_ type and
//     size = arg.  Outer keeps the original type (LOOP / REDUCE / ...).
//     Validation: arg must divide full_shape[axis].
//
//   GLOBAL
//     Mark the selected LOOP axis as GLOBAL.  Validation: arg must
//     equal the current axis size.  This pairs naturally with LOCAL:
//     LOCAL splits N into LOOP(N/L) + LOCAL(L), then GLOBAL marks
//     that remaining LOOP(N/L) axis for threadgroup/grid binding.
//
//   SWAP
//     Swap full_shape[axis] <-> full_shape[arg] and the matching
//     axis_types entries.  No new axis.
//
//   PADTO / NOLOCALS / TC
//     Reserved for future tensor-core / padding passes.  Rejected
//     until a renderer consumes them; no-op opts must not mutate the
//     JIT cache key or pretend to be an applied variant.
//
// Returns 1 on success, 0 on validation failure (axis out of range,
// arg doesn't divide, applied_opts full, MAX_AXES exceeded).

// Map a KOP_ class to the KAX_ type to mark the new inner axis.
static u8 kop_to_axis_type(u8 op) {
  switch (op) {
    case KOP_UPCAST:   return KAX_UPCAST;
    case KOP_UNROLL:   return KAX_UNROLL;
    case KOP_LOCAL:    return KAX_LOCAL;
    case KOP_GROUP:    return KAX_GROUP_REDUCE;
    case KOP_GROUPTOP: return KAX_GROUP_REDUCE;
    default:           return KAX_LOOP;
  }
}

// True if this opt class splits an axis (vs SWAP / no-op opts).
static int kop_splits_axis(u8 op) {
  return op == KOP_UPCAST || op == KOP_UNROLL || op == KOP_LOCAL
      || op == KOP_GROUP  || op == KOP_GROUPTOP;
}

fn int axes_apply_opt(KernelAxes *ax, KOpt opt) {
  if (ax == NULL) {
    return 0;
  }
  if (ax->n_applied >= MAX_OPTS) {
    return 0;
  }
  if (opt.axis >= ax->n_axes) {
    return 0;
  }

  if (kop_splits_axis(opt.op)) {
    if (opt.arg == 0) {
      return 0;
    }
    u32 axis_size = ax->full_shape[opt.axis];
    if (axis_size % opt.arg != 0) {
      return 0;
    }
    if (ax->n_axes >= MAX_AXES) {
      return 0;
    }

    // Shift axes after opt.axis right by one to make room for the
    // new inner axis.
    for (i32 i = (i32)ax->n_axes; i > (i32)opt.axis + 1; i--) {
      ax->axis_types[i] = ax->axis_types[i - 1];
      ax->full_shape[i] = ax->full_shape[i - 1];
    }
    // Outer keeps its original type, but its size shrinks by the
    // split factor.
    ax->full_shape[opt.axis]      = axis_size / opt.arg;
    // Inner takes the opt's type, size = split factor.
    ax->axis_types[opt.axis + 1]  = kop_to_axis_type(opt.op);
    ax->full_shape[opt.axis + 1]  = opt.arg;
    ax->n_axes++;
  } else if (opt.op == KOP_GLOBAL) {
    u32 axis_size = ax->full_shape[opt.axis];
    if (opt.arg != axis_size || ax->axis_types[opt.axis] != KAX_LOOP) {
      return 0;
    }
    ax->axis_types[opt.axis] = KAX_GLOBAL;
  } else if (opt.op == KOP_SWAP) {
    if ((u8)opt.arg >= ax->n_axes) {
      return 0;
    }
    u8  ti = ax->axis_types[opt.axis];
    u8  tj = ax->axis_types[opt.arg];
    u32 si = ax->full_shape[opt.axis];
    u32 sj = ax->full_shape[opt.arg];
    ax->axis_types[opt.axis] = tj;
    ax->axis_types[opt.arg]  = ti;
    ax->full_shape[opt.axis] = sj;
    ax->full_shape[opt.arg]  = si;
  } else {
    return 0;
  }

  ax->applied_opts[ax->n_applied++] = opt;
  ax->version++;
  if (ax->version == 0) {
    ax->version = 1;
  }
  return 1;
}
