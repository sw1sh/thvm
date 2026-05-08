// codegen/axis.c -- KernelAxes lifecycle: default constructor +
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
  // Idempotent: if `ke->axes` already has a non-zero n_axes, another
  // kid sharing this kernel_program_cache slot already populated
  // it.  Per-program-shape sharing means there's nothing to do.
  // E9 session 4: writer-trio reads the private scratch directly.
  if (ke->axes == NULL || ke->axes->_writer.n_axes != 0) {
    return;
  }

  // Phase E migration: route writes through tile_anno_axis_append.
  // The helper bumps version + handles bounds.  Each call appends
  // exactly one axis; LOOP per output dim then optional trailing
  // REDUCE.
  u32 nd = ke->output_shape.ndim;
  if (nd > MAX_AXES - 1) {
    nd = MAX_AXES - 1;
  }
  for (u32 i = 0; i < nd; i++) {
    TileAxisInfo info = { KAX_LOOP, ke->output_shape.dims[i], 0, 0 };
    (void)tile_anno_axis_append(ke, info);
  }
  // Trailing REDUCE: append axis sized at the ratio between the
  // tail-REDUCE op's source numel and the kernel output numel.
  // Mirrors WL Kernel.wl `defaultFullShape`'s redOp.numel/outNumel.
  if (ke->n_ops > 0 && ke->program[ke->n_ops - 1].opcode == UOP_REDUCE) {
    KProgOp const *rd = &ke->program[ke->n_ops - 1];
    u32 src_numel;
    if (KSRC_IS_INPUT(rd->src[0])) {
      src_numel = ke->input_numels[KSRC_INDEX(rd->src[0])];
    } else {
      src_numel = ke->program[KSRC_INDEX(rd->src[0])].numel;
    }
    u32 out_numel = ke->output_numel ? ke->output_numel : 1;
    u32 axis_size = src_numel / out_numel;
    TileAxisInfo info = { KAX_REDUCE, axis_size, 0, 0 };
    (void)tile_anno_axis_append(ke, info);
  }
}

static u32 axes_scalar_reduce_extent(KernelEntry const *ke) {
  if (ke == NULL || ke->scalar_uops == NULL) {
    return 0;
  }
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (u->op != S_REDUCE_SUM && u->op != S_REDUCE_MAX) {
      continue;
    }
    if (u->src_count < 2 || u->src[1] == 0
        || u->src[1] >= ke->n_scalar_uops) {
      return 0;
    }
    ScalarUop const *rng = &ke->scalar_uops[u->src[1]];
    if (rng->op != S_RANGE) {
      return 0;
    }
    u32 axis_type = (u32)(rng->extra >> 32);
    u32 extent    = (u32)(rng->extra & 0xFFFFFFFFu);
    if (axis_type != S_AXIS_REDUCE || extent == 0) {
      return 0;
    }
    return extent;
  }
  return 0;
}

// E9-prep wedge 3: predicate that answers "will ke->axes carry a
// REDUCE-class axis (KAX_REDUCE or KAX_GROUP_REDUCE)?" without reading
// `ke->axes->axis_types[]`.  It mirrors the writer side exactly: every
// production writer of a REDUCE-class entry leaves a higher-level
// signal that this predicate consults.
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
  if (ke->axes != NULL) {
    KOpt const *opts = ke->axes->applied_opts;
    u32 n_applied = (u32)ke->axes->n_applied;
    for (u32 i = 0; i < n_applied; i++) {
      if (opts[i].op == KOP_GROUP || opts[i].op == KOP_GROUPTOP) {
        return 1;
      }
    }
  }
  if (axes_scalar_reduce_extent(ke) != 0) {
    return 1;
  }
  return 0;
}

// Mirror of apply_opt.c's static kop_to_axis_type.  Inlined here to
// keep the wedge-4 simulator self-contained without exporting the
// helper from apply_opt.c.
static u8 axis_kop_to_axis_type(u8 op) {
  switch (op) {
    case KOP_UPCAST:   return KAX_UPCAST;
    case KOP_UNROLL:   return KAX_UNROLL;
    case KOP_LOCAL:    return KAX_LOCAL;
    case KOP_GROUP:    return KAX_GROUP_REDUCE;
    case KOP_GROUPTOP: return KAX_GROUP_REDUCE;
    default:           return KAX_LOOP;
  }
}

// E9-prep wedge 4: derive per-axis kax_type[] from the higher-level
// signals (output_shape + tail-reduce + scalar-reduce + applied_opts)
// instead of reading `ke->axes->axis_types[]`.  Mirrors the writer
// trio (axes_default_for + axes_ensure_scalar_reduce + axes_apply_opt)
// exactly:
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
// Used by tile_emit_axes_from_kernel_signals as the source of TILE_AXIS
// leaf kax_type values, and by axes_resolve_kax_type as the single
// read point.  Wedge 8 retired the previous legacy-fallback /
// THVM_E9_VALIDATE=1 cross-check shape: this is now the only kax_type
// read path outside the writer trio.
fn u32 axes_compute_axis_types(struct KernelEntry const *ke, u8 *out,
                               u32 cap) {
  if (ke == NULL || ke->axes == NULL || out == NULL || cap == 0) {
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
  KOpt const *opts = ke->axes->applied_opts;
  u32 n_applied   = (u32)ke->axes->n_applied;
  for (u32 k = 0; k < n_applied; k++) {
    KOpt o = opts[k];
    u8 op = o.op;
    if (op == KOP_TC || op == KOP_NONE || op == KOP_PADTO
        || op == KOP_NOLOCALS) {
      // No axis-structure mutation.
      continue;
    }
    if (op == KOP_UPCAST || op == KOP_UNROLL || op == KOP_LOCAL
        || op == KOP_GROUP  || op == KOP_GROUPTOP) {
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

// E9-prep wedge 7+8: single resolve point for kax_type reads outside
// the writer trio.  Returns the wedge-4 simulator output -- signal-
// derived from output_shape + tail-reduce + scalar-reduce + applied_opts
// -- which mirrors the writer trio (axes_default_for +
// axes_ensure_scalar_reduce + axes_apply_opt) by construction.
//
// Wedge 8 retired the legacy `ke->axes->axis_types[d]` fallback: the 2
// remaining hand-write tests in test_tile_graph (wedge-6 residuals)
// were rebuilt against the writer trio, so every read site now has
// either applied_opts > 0 (simulator authoritative) or a signal-only
// state that the simulator can derive (default_for: nd LOOPs +
// optional trailing REDUCE).
//
// Returns the resolved kax_type (KAX_*) for axis `d`.  When ke / axes
// are NULL, `d >= n_axes`, or the simulator can't speak (overflow /
// unknown opt -- bug in the writer trio's applied_opts log), returns
// KAX_LOOP as a safe default.
fn u8 axes_resolve_kax_type(struct KernelEntry const *ke, u32 d) {
  // E9 session 4: bound check uses writer-private n_axes (validator
  // role; this function lives in the writer-trio scope).  External
  // callers see only the resolver result.
  if (ke == NULL || ke->axes == NULL || d >= ke->axes->_writer.n_axes) {
    return KAX_LOOP;
  }
  u8 types[MAX_AXES] = {0};
  u32 n = axes_compute_axis_types(ke, types, MAX_AXES);
  if (n == 0 || d >= n) {
    return KAX_LOOP;
  }
  return types[d];
}

// E9 session 2: derive per-axis full_shape extents from the higher-
// level signals (output_shape + tail-reduce + scalar-reduce +
// applied_opts) instead of reading `ke->axes->_writer.full_shape[]`
// directly (writer-trio scratch as of session 4).
// Mirrors the writer trio (axes_default_for + axes_ensure_scalar_reduce
// + axes_apply_opt) exactly:
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
// divide).  By construction, the value matches `ke->axes->full_shape`
// + `ke->axes->n_axes` after the writer trio has produced the same
// applied_opts log.
fn u32 axes_compute_full_shape(struct KernelEntry const *ke, u32 *out,
                               u32 cap) {
  if (ke == NULL || ke->axes == NULL || out == NULL || cap == 0) {
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
  KOpt const *opts = ke->axes->applied_opts;
  u32 n_applied   = (u32)ke->axes->n_applied;
  for (u32 k = 0; k < n_applied; k++) {
    KOpt o = opts[k];
    u8 op = o.op;
    if (op == KOP_TC || op == KOP_NONE || op == KOP_PADTO
        || op == KOP_NOLOCALS || op == KOP_GLOBAL) {
      // No shape mutation.
      continue;
    }
    if (op == KOP_UPCAST || op == KOP_UNROLL || op == KOP_LOCAL
        || op == KOP_GROUP  || op == KOP_GROUPTOP) {
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

// E9 session 2: cross-check resolver output against the writer's
// `ke->axes->_writer.full_shape[]` / `ke->axes->_writer.n_axes` state.
// Enabled by setting `THVM_E9_VALIDATE=1`.  Aborts on divergence -- this
// catches drift between the writer trio's mutations and the resolver's
// signal-replay logic.  Mirrors the wedge 1 / E9-prep validation
// pattern.
static int axes_e9_validate_enabled(void) {
  static int cached = -1;
  if (cached < 0) {
    char const *env = getenv("THVM_E9_VALIDATE");
    cached = (env != NULL && env[0] == '1' && env[1] == 0) ? 1 : 0;
  }
  return cached;
}

static void axes_e9_validate_full_shape(struct KernelEntry const *ke,
                                        u32 const *derived, u32 n_derived) {
  if (!axes_e9_validate_enabled() || ke == NULL || ke->axes == NULL) {
    return;
  }
  if (n_derived != ke->axes->_writer.n_axes) {
    fprintf(stderr,
            "[THVM_E9_VALIDATE] axes_resolve_full_shape: n_axes mismatch "
            "(derived=%u writer=%u)\n",
            n_derived, ke->axes->_writer.n_axes);
    abort();
  }
  for (u32 i = 0; i < n_derived; i++) {
    if (derived[i] != ke->axes->_writer.full_shape[i]) {
      fprintf(stderr,
              "[THVM_E9_VALIDATE] axes_resolve_full_shape: extent[%u] "
              "mismatch (derived=%u writer=%u)\n",
              i, derived[i], ke->axes->_writer.full_shape[i]);
      abort();
    }
  }
}

// E9 session 2: per-axis full_shape resolver.  Returns the derived
// extent for axis `d` (signal-replay over output_shape + applied_opts);
// 0 when ke/axes are NULL, d is out of range, or the simulator can't
// speak.  Under `THVM_E9_VALIDATE=1`, cross-checks against the writer's
// `ke->axes->_writer.full_shape[d]` and aborts on divergence.
fn u32 axes_resolve_full_shape(struct KernelEntry const *ke, u32 d,
                               u32 *out_extent) {
  if (ke == NULL || ke->axes == NULL || d >= ke->axes->_writer.n_axes) {
    if (out_extent != NULL) *out_extent = 0;
    return 0;
  }
  u32 extents[MAX_AXES] = {0};
  u32 n = axes_compute_full_shape(ke, extents, MAX_AXES);
  if (n == 0 || d >= n) {
    if (out_extent != NULL) *out_extent = 0;
    return 0;
  }
  axes_e9_validate_full_shape(ke, extents, n);
  if (out_extent != NULL) *out_extent = extents[d];
  return 1;
}

// E9 session 2: axis-count resolver.  Returns the derived axis count
// (output_shape.ndim clipped to MAX_AXES-1, plus 1 if a trailing
// REDUCE-class axis is present, plus the count of split-class
// applied_opts).  Under `THVM_E9_VALIDATE=1`, cross-checks against
// `ke->axes->_writer.n_axes` and aborts on divergence.
fn u32 axes_resolve_n_axes(struct KernelEntry const *ke) {
  if (ke == NULL || ke->axes == NULL) {
    return 0;
  }
  u32 extents[MAX_AXES] = {0};
  u32 n = axes_compute_full_shape(ke, extents, MAX_AXES);
  if (axes_e9_validate_enabled() && ke->axes != NULL
      && n != ke->axes->_writer.n_axes) {
    fprintf(stderr,
            "[THVM_E9_VALIDATE] axes_resolve_n_axes: n_axes mismatch "
            "(derived=%u writer=%u)\n",
            n, ke->axes->_writer.n_axes);
    abort();
  }
  return n;
}

// E9 session 2: content hash of the kernel's axis-defining inputs.
// Replaces the `ke->axes->version` u32 freshness counter with a
// deterministic u64 hash of (applied_opts, output_shape, source_uop)
// -- the same triple that drives the resolvers above.  Cache equality
// semantics stay (one u64 == one u64); the hash is bit-stable across
// runs given the same input state, so cross-process / cross-session
// caches stay coherent.
//
// FNV-1a over: applied_opts[0..n_applied] bytes, output_shape.dims +
// ndim, source_uop (tag + ext).  Always non-zero so the empty-state
// sentinel `tile_axes_hash = 0` (uninitialised tile_uops) stays
// disambiguated from a real hash.
fn u64 tile_axes_hash(struct KernelEntry const *ke) {
  if (ke == NULL || ke->axes == NULL) {
    return 0;
  }
  u64 h = 0xcbf29ce484222325ULL ^ 0x415845535F484148ULL;  // "AXES_HAH"
  u32 n_applied = (u32)ke->axes->n_applied;
  h ^= (u64)n_applied;
  h *= 0x100000001b3ULL;
  KOpt const *opts = ke->axes->applied_opts;
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
  if (ke == NULL || ke->axes == NULL) {
    return;
  }
  // E9 session 4: writer-trio reads the private scratch directly.
  if (ke->axes->_writer.n_axes == 0) {
    axes_default_for(ke);
  }
  // E9-prep wedge 3: skip-if-already-have-REDUCE without reading
  // axis_types[].  At this callsite (post axes_default_for, before
  // autotune) n_applied == 0 and the only axis-structure writer that
  // has run is axes_default_for itself (one LOOP per output dim plus
  // an optional trailing REDUCE) or a previous axes_ensure_scalar_reduce
  // (one trailing REDUCE).  In both cases the trailing-REDUCE signal
  // is `n_axes > nd_output_clipped`: default-for adds nd_output LOOPs
  // and at most one trailing axis, so any axis past the LOOP block is
  // exactly the REDUCE we'd otherwise duplicate.
  u32 nd = ke->output_shape.ndim;
  if (nd > MAX_AXES - 1) {
    nd = MAX_AXES - 1;
  }
  if (ke->axes->_writer.n_axes > nd) {
    return;
  }
  u32 extent = axes_scalar_reduce_extent(ke);
  if (extent == 0 || ke->axes->_writer.n_axes >= MAX_AXES) {
    return;
  }
  // Phase E writer migration: route through tile_anno_axis_append.
  // Today the helper writes ke->axes; when Phase F flips, it writes
  // TILE_AXIS directly and freshness propagates via tile_axes_hash.
  TileAxisInfo info = { KAX_REDUCE, extent, 0, 0 };
  (void)tile_anno_axis_append(ke, info);
}
