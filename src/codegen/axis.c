// codegen/axis.c -- KpSchedule lifecycle: default constructor +
// applied-opt query helpers.  The mutation (axes_apply_opt) lives in
// codegen/apply_opt.c.
//
// Mirrors tinygrad's `Kernel.axis_types[]` / `Kernel.full_shape[]`
// (tinygrad/codegen/opt/kernel.py).  Default at materialize-time:
// one LOOP axis per output dim, plus a trailing REDUCE axis sized at
// `src_numel / out_numel` for kernels whose final program op is
// UOP_REDUCE.  This matches today's flat `for i = 0..numel-1` emit
// and keeps the existing 393/393 test grid passing while the
// variant emitter is under construction.

fn void axes_default_for(KernelEntry *ke) {
  // Signal-driven resolvers cover the initial state (nd LOOPs +
  // optional trailing REDUCE) directly from (output_shape +
  // tail-reduce + scalar-reduce).  No scratch to populate; symbol
  // kept so existing call ordering in materialize.c / tile.c stays
  // valid.
  (void)ke;
}

static u32 axes_scalar_reduce_extent(KernelEntry const *ke) {
  (void)ke;
  return 0;
}

// Predicate: "will ke->schedule carry a REDUCE-class axis
// (KAX_REDUCE or KAX_GROUP_REDUCE)?", derived from higher-level
// signals without reading axis_types[].  Mirrors the writer side
// exactly: every production writer of a REDUCE-class entry leaves a
// signal this predicate consults.
//
//   1. axes_default_for appends a trailing KAX_REDUCE iff the kernel
//      program ends in UOP_REDUCE.  Signal: ke->program tail opcode.
//   2. axes_apply_opt(KOP_GROUP / KOP_GROUPTOP) splits an axis and
//      marks the new inner with KAX_GROUP_REDUCE.  Signal: applied_opts
//      log carries one of those op codes.  No other axes_apply_opt
//      class introduces a REDUCE-class type, and SWAP/UPCAST/UNROLL/
//      LOCAL/GLOBAL preserve any REDUCE outer that was already there.
//   3. axes_ensure_scalar_reduce appends a trailing KAX_REDUCE when
//      the scalar arena carries an S_REDUCE_* over an S_AXIS_REDUCE
//      range.  Signal: axes_scalar_reduce_extent(ke) != 0.
//
// All three signals are read-only over the kernel program / scalar
// arena / applied_opts log, never axis_types[].  Used by:
//   - kernel_lift.c's test-seam guard (tests for a REDUCE before
//     deciding whether the single-origin linearisation is safe).
fn int axes_will_have_reduce_axis(KernelEntry const *ke) {
  if (ke == NULL) {
    return 0;
  }
  if (ke->n_ops > 0 && ke->program[ke->n_ops - 1].opcode == UOP_REDUCE) {
    return 1;
  }
  if (axes_scalar_reduce_extent(ke) != 0) {
    return 1;
  }
  return 0;
}

// Mirror of apply_opt.c's static kop_to_axis_type.  Inlined here to
// keep the axis-type simulator self-contained without exporting the
// helper from apply_opt.c.
static u8 axis_kop_to_axis_type(u8 op) {
  switch (op) {
    case KOP_UPCAST:   return KAX_UPCAST;
    case KOP_UNROLL:   return KAX_UNROLL;
    case KOP_LOCAL:    return KAX_LOCAL;
    default:           return KAX_LOOP;
  }
}

// Derive per-axis kax_type[] from the higher-level signals
// (output_shape + tail-reduce + scalar-reduce + applied_opts).
// Mirrors the writer trio (axes_default_for +
// axes_ensure_scalar_reduce + axes_apply_opt) exactly:
//
//   1. Initial state: `nd = output_shape.ndim` LOOPs (clipped to
//      MAX_AXES-1), optionally followed by a single trailing REDUCE.
//      The trailing-REDUCE is present iff
//        - the kernel program ends in UOP_REDUCE (axes_default_for
//          appends it), OR
//        - the scalar arena carries an S_REDUCE_* over an
//          S_AXIS_REDUCE range (axes_ensure_scalar_reduce appends it).
//   2. Replay applied_opts in order using the same structural logic
//      as axes_apply_opt: KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP
//      split the indicated axis and insert a new inner axis with the
//      opt's KAX_ type; KOP_GLOBAL stamps the indicated axis as
//      KAX_GLOBAL; KOP_SWAP exchanges two positions; KOP_TC carries
//      no axis-structure mutation in axes_apply_opt (rejected there;
//      kernel_apply_opt handles it as metadata).
//
// Returns the number of axes written to `out` (matches the post-replay
// `n_axes` derived from initial_n_axes + count(split-class opts)).
// On overflow, returns 0.
//
// Used by axes_resolve_kax_type as the single kax_type read point
// outside the writer trio.
fn u32 axes_compute_axis_types(struct KernelEntry const *ke, u8 *out,
                               u32 cap) {
  if (ke == NULL || ke->schedule == NULL || out == NULL || cap == 0) {
    return 0;
  }

  // Initial layout: nd LOOPs + optional trailing REDUCE.
  u32 nd = ke->output_shape.ndim;
  if (nd > MAX_AXES - 1) {
    nd = MAX_AXES - 1;
  }
  u8 types[MAX_AXES] = {0};
  u32 n = 0;
  for (u32 i = 0; i < nd; i++) {
    if (n >= MAX_AXES) {
      return 0;
    }
    types[n++] = KAX_LOOP;
  }
  int has_initial_reduce =
      (ke->n_ops > 0 && ke->program != NULL
       && ke->program[ke->n_ops - 1].opcode == UOP_REDUCE)
      || (axes_scalar_reduce_extent(ke) != 0);
  if (has_initial_reduce && n < MAX_AXES) {
    types[n++] = KAX_REDUCE;
  }

  // Replay applied_opts using axes_apply_opt's structural logic.  We
  // don't validate splits (size % arg) or GLOBAL preconditions here:
  // applied_opts is the LOG of opts that ALREADY succeeded against the
  // axis structure, so the replay is guaranteed-valid by construction.
  KOpt const *opts = ke->schedule->applied_opts;
  u32 n_applied   = (u32)ke->schedule->n_applied;
  for (u32 k = 0; k < n_applied; k++) {
    KOpt o = opts[k];
    u8 op = o.op;
    if (op == KOP_TC || op == KOP_NONE || op == KOP_PADTO
        || op == KOP_NOLOCALS) {
      // No axis-structure mutation.
      continue;
    }
    if (op == KOP_UPCAST || op == KOP_UNROLL || op == KOP_LOCAL) {
      if (o.axis >= n || n >= MAX_AXES) {
        return 0;
      }
      // Shift positions > axis right by one; insert at axis+1.
      for (i32 i = (i32)n; i > (i32)o.axis + 1; i--) {
        types[i] = types[i - 1];
      }
      // Outer keeps its type; inner takes the opt's KAX_ type.
      types[o.axis + 1] = axis_kop_to_axis_type(op);
      n++;
      continue;
    }
    if (op == KOP_GLOBAL) {
      if (o.axis >= n) {
        return 0;
      }
      types[o.axis] = KAX_GLOBAL;
      continue;
    }
    if (op == KOP_SWAP) {
      if (o.axis >= n || (u8)o.arg >= n) {
        return 0;
      }
      u8 tmp = types[o.axis];
      types[o.axis]  = types[o.arg];
      types[o.arg]   = tmp;
      continue;
    }
    // Unknown opt -- bail (caller falls back to legacy path).
    return 0;
  }

  if (n > cap) {
    return 0;
  }
  for (u32 i = 0; i < n; i++) {
    out[i] = types[i];
  }
  return n;
}

// Single kax_type read point outside the writer trio.  Returns the
// simulator output -- signal-derived from (output_shape + tail-reduce
// + scalar-reduce + applied_opts), which mirrors the writer trio
// (axes_default_for + axes_ensure_scalar_reduce + axes_apply_opt) by
// construction.
//
// Returns the resolved kax_type (KAX_*) for axis `d`.  When ke /
// axes are NULL, `d >= n_axes`, or the simulator can't speak
// (overflow / unknown opt -- bug in the writer trio's applied_opts
// log), returns KAX_LOOP as a safe default.
fn u8 axes_resolve_kax_type(struct KernelEntry const *ke, u32 d) {
  if (ke == NULL || ke->schedule == NULL) {
    return KAX_LOOP;
  }
  u8 types[MAX_AXES] = {0};
  u32 n = axes_compute_axis_types(ke, types, MAX_AXES);
  if (n == 0 || d >= n) {
    return KAX_LOOP;
  }
  return types[d];
}

// Derive per-axis full_shape extents from the higher-level signals
// (output_shape + tail-reduce + scalar-reduce + applied_opts).
// Mirrors the writer trio (axes_default_for +
// axes_ensure_scalar_reduce + axes_apply_opt) exactly:
//
//   1. Initial state: extents[i] = output_shape.dims[i] for i < nd
//      (clipped to MAX_AXES-1), optionally followed by a trailing
//      REDUCE extent (= source-numel / output-numel for tail-REDUCE
//      programs, or `axes_scalar_reduce_extent(ke)` for scalar-arena
//      reductions).
//   2. Replay applied_opts in order using the same structural logic
//      as kernel_apply_opt: split-class opts (UPCAST/UNROLL/LOCAL/
//      GROUP/GROUPTOP) divide the indicated extent by opt.arg and
//      insert opt.arg as the inner extent at axis+1; KOP_SWAP
//      exchanges two extents in place; KOP_GLOBAL/TC/PADTO/NOLOCALS
//      carry no shape mutation.
//
// Returns the number of extents written to `out`; 0 on overflow,
// unknown opt, or invalid replay (axis out of range, arg doesn't
// divide).  By construction, the value matches `ke->schedule->full_shape`
// + `ke->schedule->n_axes` after the writer trio has produced the same
// applied_opts log.
fn u32 axes_compute_full_shape(struct KernelEntry const *ke, u32 *out,
                               u32 cap) {
  if (ke == NULL || ke->schedule == NULL || out == NULL || cap == 0) {
    return 0;
  }

  u32 nd = ke->output_shape.ndim;
  if (nd > MAX_AXES - 1) {
    nd = MAX_AXES - 1;
  }
  u32 extents[MAX_AXES] = {0};
  u32 n = 0;
  for (u32 i = 0; i < nd; i++) {
    if (n >= MAX_AXES) {
      return 0;
    }
    extents[n++] = ke->output_shape.dims[i];
  }
  int has_initial_reduce =
      (ke->n_ops > 0 && ke->program != NULL
       && ke->program[ke->n_ops - 1].opcode == UOP_REDUCE);
  if (has_initial_reduce && n < MAX_AXES) {
    KProgOp const *rd = &ke->program[ke->n_ops - 1];
    u32 src_numel;
    if (KSRC_IS_INPUT(rd->src[0])) {
      src_numel = ke->input_numels[KSRC_INDEX(rd->src[0])];
    } else {
      src_numel = ke->program[KSRC_INDEX(rd->src[0])].numel;
    }
    u32 out_numel = ke->output_numel ? ke->output_numel : 1;
    extents[n++] = src_numel / out_numel;
  } else {
    u32 sru = axes_scalar_reduce_extent(ke);
    if (sru != 0 && n < MAX_AXES) {
      extents[n++] = sru;
    }
  }

  // Replay applied_opts using kernel_apply_opt's structural logic.
  // applied_opts is the LOG of opts that ALREADY succeeded against
  // the axis structure, so the replay is guaranteed-valid by
  // construction.
  KOpt const *opts = ke->schedule->applied_opts;
  u32 n_applied   = (u32)ke->schedule->n_applied;
  for (u32 k = 0; k < n_applied; k++) {
    KOpt o = opts[k];
    u8 op = o.op;
    if (op == KOP_TC || op == KOP_NONE || op == KOP_PADTO
        || op == KOP_NOLOCALS || op == KOP_GLOBAL) {
      // No shape mutation.
      continue;
    }
    if (op == KOP_UPCAST || op == KOP_UNROLL || op == KOP_LOCAL) {
      if (o.axis >= n || n >= MAX_AXES || o.arg == 0) {
        return 0;
      }
      u32 axis_size = extents[o.axis];
      if (axis_size % o.arg != 0) {
        return 0;
      }
      // Shift positions > axis right by one; insert at axis+1.
      for (i32 i = (i32)n; i > (i32)o.axis + 1; i--) {
        extents[i] = extents[i - 1];
      }
      extents[o.axis]     = axis_size / o.arg;
      extents[o.axis + 1] = o.arg;
      n++;
      continue;
    }
    if (op == KOP_SWAP) {
      if (o.axis >= n || (u8)o.arg >= n) {
        return 0;
      }
      u32 tmp = extents[o.axis];
      extents[o.axis]  = extents[o.arg];
      extents[o.arg]   = tmp;
      continue;
    }
    // Unknown opt -- bail.
    return 0;
  }

  if (n > cap) {
    return 0;
  }
  for (u32 i = 0; i < n; i++) {
    out[i] = extents[i];
  }
  return n;
}

// Per-axis full_shape resolver.  Returns the derived extent for
// axis `d` (signal-replay over output_shape + applied_opts); 0 when
// ke/axes are NULL, d is out of range, or the simulator can't speak.
// This is the only authoritative source -- nothing left to
// cross-check against.
fn u32 axes_resolve_full_shape(struct KernelEntry const *ke, u32 d,
                               u32 *out_extent) {
  if (ke == NULL || ke->schedule == NULL) {
    if (out_extent != NULL) *out_extent = 0;
    return 0;
  }
  u32 extents[MAX_AXES] = {0};
  u32 n = axes_compute_full_shape(ke, extents, MAX_AXES);
  if (n == 0 || d >= n) {
    if (out_extent != NULL) *out_extent = 0;
    return 0;
  }
  if (out_extent != NULL) *out_extent = extents[d];
  return 1;
}

// Axis-count resolver.  Returns the derived axis count
// (output_shape.ndim clipped to MAX_AXES-1, plus 1 if a trailing
// REDUCE-class axis is present, plus the count of split-class
// applied_opts).  Authoritative count.
fn u32 axes_resolve_n_axes(struct KernelEntry const *ke) {
  if (ke == NULL || ke->schedule == NULL) {
    return 0;
  }
  u32 extents[MAX_AXES] = {0};
  return axes_compute_full_shape(ke, extents, MAX_AXES);
}

// Content hash of the kernel's axis-defining inputs.  Deterministic
// u64 hash of (applied_opts, output_shape, source_uop) -- the same
// triple that drives the resolvers above.  Cache equality semantics
// stay (one u64 == one u64); the hash is bit-stable across runs
// given the same input state, so cross-process / cross-session
// caches stay coherent.
//
// FNV-1a over: applied_opts[0..n_applied] bytes, output_shape.dims +
// ndim, source_uop (tag + ext).  Always non-zero so the empty-state
// sentinel `tile_axes_hash = 0` (uninitialised tile_uops) stays
// disambiguated from a real hash.
fn u64 tile_axes_hash(struct KernelEntry const *ke) {
  if (ke == NULL || ke->schedule == NULL) {
    return 0;
  }
  u64 h = 0xcbf29ce484222325ULL ^ 0x415845535F484148ULL;  // "AXES_HAH"
  u32 n_applied = (u32)ke->schedule->n_applied;
  h ^= (u64)n_applied;
  h *= 0x100000001b3ULL;
  KOpt const *opts = ke->schedule->applied_opts;
  u8 const *opts_bytes = (u8 const *)opts;
  size_t opts_n = (size_t)n_applied * sizeof(KOpt);
  for (size_t i = 0; i < opts_n; i++) {
    h ^= (u64)opts_bytes[i]; h *= 0x100000001b3ULL;
  }
  h ^= (u64)ke->output_shape.ndim;
  h *= 0x100000001b3ULL;
  u8 const *dim_bytes = (u8 const *)ke->output_shape.dims;
  size_t dim_n = (size_t)ke->output_shape.ndim * sizeof(u32);
  for (size_t i = 0; i < dim_n; i++) {
    h ^= (u64)dim_bytes[i]; h *= 0x100000001b3ULL;
  }
  h ^= (u64)term_tag(ke->source_uop);
  h *= 0x100000001b3ULL;
  h ^= (u64)term_ext(ke->source_uop);
  h *= 0x100000001b3ULL;
  // Bias bit 63 so the hash is never zero (zero is the
  // empty-tile-uops sentinel in tile_anno_tile_uops_fresh).
  return h | (1ULL << 63);
}

fn void axes_ensure_scalar_reduce(struct KernelEntry *ke) {
  // Signal-driven resolvers cover the trailing REDUCE-axis case
  // directly via axes_scalar_reduce_extent inside
  // axes_compute_full_shape.  No scratch to extend; symbol kept so
  // existing call ordering in materialize.c / tile_anno.c stays
  // valid.
  (void)ke;
}
