// uop/expander.c - port of tinygrad codegen/late/expander.py to the
// thvm UOp graph rewrite framework.
//
// This file introduces FOUR structural opcodes and ONE entry point:
//   UOP_VCONST    : vector-typed const literal
//   UOP_UNROLL    : wrapper recording which axes have been unrolled
//   UOP_CONTRACT  : inverse of UNROLL (gather lanes back)
//   UOP_GEP       : vector-element extraction
//   uop_expand_graph(root) -> Term
//
// The entry point runs three sub-passes via uop_graph_rewrite:
//   1. pm_pre_expander (expander.py:147-155)
//      - rewrites every UOP_RANGE with axis_type in {UPCAST, UNROLL}
//        into UNROLL(VCONST(0, 1, ..., F-1)) carrying (axis_id, F).
//      - fix_reduce_unroll (expander.py:116-125): if a REDUCE has any
//        UNROLL'd source range, wrap its src in CONTRACT and drop those
//        ranges from its src[1:].
//      - fix_store_unroll (expander.py:127-130): if a STORE has UNROLL'd
//        srcs in its address subtree, wrap with CONTRACT.
//   2. pm_group_for_reduce (expander.py:157-160): STUBBED.  Handles
//      KAX_GROUP_REDUCE; allocates shared-memory buffer; outside the
//      scope of this port.
//   3. expander (expander.py:94-112): the actual unrolling.
//      - do_expand: any ALU/REDUCE/STORE whose src[] contains an UNROLL
//        broadcasts non-UNROLL srcs to the unified vector width and
//        produces a vector-typed result wrapped in a new UNROLL.
//      - do_contract: CONTRACT-over-UNROLL collapses via GEP swizzle.
//      - double UNROLL: UNROLL(UNROLL(x)) folds args.
//      - empty UNROLL: UNROLL with arg=() unwraps to src.
//
// NOT WIRED INTO THE RENDERER.  The current renderer (render_uop.c)
// consumes the RANGE-leaf representation directly; it has no support
// for vector dtypes / CONTRACT / GEP, which the expander OUTPUT
// requires.  Adopting the expander as a render-time pre-pass requires
// the companion devectorizer port (tinygrad codegen/late/devectorizer.py
// reduce_to_acc / devectorize / pm_render) -- see
// docs/tinygrad_late_passes.md "Architectural alternative" section.
//
// This file exists so the port can land in stages: opcodes + transform
// + tests first; renderer adoption + devectorizer next.

// === Constructors =========================================================

// UOP_VCONST: heap = [NUM(dtype), NUM(n), NUM(b_0), ..., NUM(b_{n-1})]
// Hash-cons via uop_mov_cache keyed on (op, 0, [dtype, n, bits_0, ...]).
fn Term uop_vconst(u32 dtype, u32 n, u32 const *bits) {
  if (n == 0) {
    return 0;
  }
  enum { VCONST_KEY_MAX = 2 + 64 };
  u32 key_buf[VCONST_KEY_MAX];
  if (2u + n > VCONST_KEY_MAX) {
    // Too wide to hash-cons; allocate fresh.
    u64 loc = heap_alloc(2 + n);
    heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, dtype));
    heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, n));
    for (u32 i = 0; i < n; i++) {
      heap_set(loc + 2 + i, term_new(0, TAG_NUM, dtype, bits[i]));
    }
    return term_new(0, TAG_UOP, UOP_VCONST, loc);
  }
  key_buf[0] = dtype;
  key_buf[1] = n;
  for (u32 i = 0; i < n; i++) {
    key_buf[2 + i] = bits[i];
  }
  u64 key = uop_mov_hash(UOP_VCONST, 0, key_buf, 2 + n);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    return hit;
  }
  u64 loc = heap_alloc(2 + n);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, dtype));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, n));
  for (u32 i = 0; i < n; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, dtype, bits[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_VCONST, loc);
  uop_mov_insert(key, t);
  return t;
}

fn u32 uop_vconst_dtype(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_VCONST) return 0;
  return (u32)term_val(heap_read(term_val(t) + 0));
}
fn u32 uop_vconst_n(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_VCONST) return 0;
  return (u32)term_val(heap_read(term_val(t) + 1));
}
fn u32 uop_vconst_bits(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_VCONST) return 0;
  return (u32)term_val(heap_read(term_val(t) + 2 + i));
}

// UOP_UNROLL: heap = [src, NUM(n_args), NUM(axis_0), NUM(F_0), ...]
// Hash-cons by (src, n_args, args[]).
fn Term uop_unroll(Term src, u32 n_args, u32 const *axis_ids, u32 const *factors) {
  enum { UNROLL_KEY_MAX = 1 + 2 * 8 };
  u32 key_buf[UNROLL_KEY_MAX];
  if (1u + 2 * n_args > UNROLL_KEY_MAX) {
    u64 loc = heap_alloc(2 + 2 * n_args);
    heap_set(loc + 0, src);
    heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, n_args));
    for (u32 i = 0; i < n_args; i++) {
      heap_set(loc + 2 + 2 * i + 0, term_new(0, TAG_NUM, DT_INT32, axis_ids[i]));
      heap_set(loc + 2 + 2 * i + 1, term_new(0, TAG_NUM, DT_INT32, factors[i]));
    }
    return term_new(0, TAG_UOP, UOP_UNROLL, loc);
  }
  key_buf[0] = n_args;
  for (u32 i = 0; i < n_args; i++) {
    key_buf[1 + 2 * i + 0] = axis_ids[i];
    key_buf[1 + 2 * i + 1] = factors[i];
  }
  u64 key = uop_mov_hash(UOP_UNROLL, src, key_buf, 1 + 2 * n_args);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    return hit;
  }
  u64 loc = heap_alloc(2 + 2 * n_args);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, n_args));
  for (u32 i = 0; i < n_args; i++) {
    heap_set(loc + 2 + 2 * i + 0, term_new(0, TAG_NUM, DT_INT32, axis_ids[i]));
    heap_set(loc + 2 + 2 * i + 1, term_new(0, TAG_NUM, DT_INT32, factors[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_UNROLL, loc);
  uop_mov_insert(key, t);
  return t;
}

fn u32 uop_unroll_n_args(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_UNROLL) return 0;
  return (u32)term_val(heap_read(term_val(t) + 1));
}
fn u32 uop_unroll_axis_id(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_UNROLL) return 0;
  return (u32)term_val(heap_read(term_val(t) + 2 + 2 * i + 0));
}
fn u32 uop_unroll_factor(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_UNROLL) return 0;
  return (u32)term_val(heap_read(term_val(t) + 2 + 2 * i + 1));
}

// UOP_CONTRACT: same layout as UNROLL.
fn Term uop_contract(Term src, u32 n_args, u32 const *axis_ids, u32 const *factors) {
  enum { CONTRACT_KEY_MAX = 1 + 2 * 8 };
  u32 key_buf[CONTRACT_KEY_MAX];
  if (1u + 2 * n_args > CONTRACT_KEY_MAX) {
    u64 loc = heap_alloc(2 + 2 * n_args);
    heap_set(loc + 0, src);
    heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, n_args));
    for (u32 i = 0; i < n_args; i++) {
      heap_set(loc + 2 + 2 * i + 0, term_new(0, TAG_NUM, DT_INT32, axis_ids[i]));
      heap_set(loc + 2 + 2 * i + 1, term_new(0, TAG_NUM, DT_INT32, factors[i]));
    }
    return term_new(0, TAG_UOP, UOP_CONTRACT, loc);
  }
  key_buf[0] = n_args;
  for (u32 i = 0; i < n_args; i++) {
    key_buf[1 + 2 * i + 0] = axis_ids[i];
    key_buf[1 + 2 * i + 1] = factors[i];
  }
  u64 key = uop_mov_hash(UOP_CONTRACT, src, key_buf, 1 + 2 * n_args);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    return hit;
  }
  u64 loc = heap_alloc(2 + 2 * n_args);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, n_args));
  for (u32 i = 0; i < n_args; i++) {
    heap_set(loc + 2 + 2 * i + 0, term_new(0, TAG_NUM, DT_INT32, axis_ids[i]));
    heap_set(loc + 2 + 2 * i + 1, term_new(0, TAG_NUM, DT_INT32, factors[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_CONTRACT, loc);
  uop_mov_insert(key, t);
  return t;
}

fn u32 uop_contract_n_args(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONTRACT) return 0;
  return (u32)term_val(heap_read(term_val(t) + 1));
}
fn u32 uop_contract_axis_id(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONTRACT) return 0;
  return (u32)term_val(heap_read(term_val(t) + 2 + 2 * i + 0));
}
fn u32 uop_contract_factor(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONTRACT) return 0;
  return (u32)term_val(heap_read(term_val(t) + 2 + 2 * i + 1));
}

// UOP_GEP: heap = [src, NUM(n_idx), NUM(idx_0), ..., NUM(idx_{n_idx-1})]
fn Term uop_gep(Term src, u32 n_idx, u32 const *indices) {
  enum { GEP_KEY_MAX = 1 + 32 };
  u32 key_buf[GEP_KEY_MAX];
  if (1u + n_idx > GEP_KEY_MAX) {
    u64 loc = heap_alloc(2 + n_idx);
    heap_set(loc + 0, src);
    heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, n_idx));
    for (u32 i = 0; i < n_idx; i++) {
      heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_INT32, indices[i]));
    }
    return term_new(0, TAG_UOP, UOP_GEP, loc);
  }
  key_buf[0] = n_idx;
  for (u32 i = 0; i < n_idx; i++) {
    key_buf[1 + i] = indices[i];
  }
  u64 key = uop_mov_hash(UOP_GEP, src, key_buf, 1 + n_idx);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    return hit;
  }
  u64 loc = heap_alloc(2 + n_idx);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, n_idx));
  for (u32 i = 0; i < n_idx; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_INT32, indices[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_GEP, loc);
  uop_mov_insert(key, t);
  return t;
}

fn u32 uop_gep_n_idx(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_GEP) return 0;
  return (u32)term_val(heap_read(term_val(t) + 1));
}
fn u32 uop_gep_idx(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_GEP) return 0;
  return (u32)term_val(heap_read(term_val(t) + 2 + i));
}

// === Helpers ==============================================================

// Deduplicated, sorted union of (axis, factor) tuples from a list of
// UNROLLs.  out must hold at least sum_of_n_args entries.  Returns
// out_count.  Sort order: ascending axis_id; we assume no axis appears
// with two different factors (would be a graph invariant violation).
static u32 expander_collect_args(Term const *unrolls, u32 n,
                                 u32 *out_axes, u32 *out_factors) {
  u32 count = 0;
  for (u32 i = 0; i < n; i++) {
    u32 na = uop_unroll_n_args(unrolls[i]);
    for (u32 j = 0; j < na; j++) {
      u32 ax = uop_unroll_axis_id(unrolls[i], j);
      u32 fa = uop_unroll_factor(unrolls[i], j);
      // dedup: skip if (ax, fa) already in out.
      int dup = 0;
      for (u32 k = 0; k < count; k++) {
        if (out_axes[k] == ax && out_factors[k] == fa) {
          dup = 1;
          break;
        }
      }
      if (dup) continue;
      // insertion sort by axis_id
      u32 ins = count;
      while (ins > 0 && out_axes[ins - 1] > ax) {
        out_axes[ins]    = out_axes[ins - 1];
        out_factors[ins] = out_factors[ins - 1];
        ins--;
      }
      out_axes[ins]    = ax;
      out_factors[ins] = fa;
      count++;
    }
  }
  return count;
}

// Compute the offset of an (axis -> value) choice into a flattened
// vector indexed by `args` (row-major over args, like tinygrad's
// _expand_arg_to_idx at expander.py:8).  `vals` is a parallel array
// to `args`: vals[i] is the chosen index for axis args[i].
static u32 expander_arg_to_idx(u32 const *factors, u32 n_args,
                               u32 const *vals) {
  u32 idx = 0, mul = 1;
  for (u32 ri = 0; ri < n_args; ri++) {
    u32 i = n_args - 1 - ri;
    idx += vals[i] * mul;
    mul *= factors[i];
  }
  return idx;
}

// Build the GEP-swizzle indices that translate from cargs's lane order
// into eargs's lane order.  Mirrors _swizzle_args at expander.py:18-20.
// Returns the count of indices written (= prod of cargs factors).
// out_indices must hold at least that many entries.
static u32 expander_swizzle(u32 const *c_axes, u32 const *c_factors, u32 c_n,
                            u32 const *e_axes, u32 const *e_factors, u32 e_n,
                            u32 *out_indices) {
  // Enumerate every choice over cargs in row-major order.
  // Find each c_axis's position in e_axes.
  u32 c_pos_in_e[8];
  for (u32 i = 0; i < c_n; i++) {
    c_pos_in_e[i] = 0xFFFFFFFFu;
    for (u32 j = 0; j < e_n; j++) {
      if (e_axes[j] == c_axes[i]) {
        c_pos_in_e[i] = j;
        break;
      }
    }
  }
  // Total choices = prod of c_factors.
  u32 total = 1;
  for (u32 i = 0; i < c_n; i++) total *= c_factors[i];
  for (u32 choice = 0; choice < total; choice++) {
    u32 vals_in_e[8] = {0};
    u32 rem = choice;
    // Row-major decode of choice into c_axes.
    for (u32 ri = 0; ri < c_n; ri++) {
      u32 i = c_n - 1 - ri;
      u32 v = rem % c_factors[i];
      rem /= c_factors[i];
      u32 pos = c_pos_in_e[i];
      if (pos != 0xFFFFFFFFu) {
        vals_in_e[pos] = v;
      }
    }
    out_indices[choice] = expander_arg_to_idx(e_factors, e_n, vals_in_e);
  }
  return total;
}

static int args_equal(u32 const *a_ax, u32 const *a_f, u32 a_n,
                      u32 const *b_ax, u32 const *b_f, u32 b_n) {
  if (a_n != b_n) return 0;
  for (u32 i = 0; i < a_n; i++) {
    if (a_ax[i] != b_ax[i] || a_f[i] != b_f[i]) return 0;
  }
  return 1;
}

// === pm_pre_expander: RANGE -> UNROLL(VCONST) =============================
// Mirrors expander.py:147-151.

static Term expander_pm_range(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  u32 axis_type = uop_range_axis_type(t);
  if (axis_type != KAX_UPCAST && axis_type != KAX_UNROLL) return 0;
  u32 axis_id = uop_range_axis_id(t);
  u32 extent  = uop_range_extent(t);
  if (extent == 0 || extent > 64) return 0;
  // Build VCONST(0, 1, ..., extent-1).  DT_INT32 is the natural choice
  // for the range-leaf substitute (the range was an integer index).
  u32 bits[64];
  for (u32 i = 0; i < extent; i++) bits[i] = i;
  Term vc = uop_vconst(DT_INT32, extent, bits);
  return uop_unroll(vc, 1, &axis_id, &extent);
}

// === fix_reduce_unroll (expander.py:116-125) ==============================
// REDUCE whose axes contain UNROLL'd ranges: pull those axes into a
// CONTRACT on the src, leave the remaining (loop) ranges in the REDUCE.
//
// thvm's UOP_REDUCE differs from tinygrad's: in thvm the axes are stored
// as a NUM list (axis IDs) and the value's RANGE leaves are matched by
// axis_id at lowering time.  For the expander port we treat the REDUCE
// as multi-axis and re-emit it with the non-UNROLL'd axes.  Detecting
// "this axis was just rewritten to an UNROLL" requires walking the src
// subtree.  As a simpler correct approximation: after pm_pre_expander
// rewrites the RANGE leaves, the REDUCE src tree no longer references
// those axes as RANGE leaves -- they're now UNROLL(VCONST) nodes.  The
// REDUCE node still LISTS those axis IDs in its axes[] array, though.
// For each axis ID in the REDUCE's axes[], if any UNROLL in the src
// subtree carries that axis, the axis was unrolled -- drop it from
// REDUCE.axes[] and add it to a CONTRACT around src.
//
// Recursive walk to find UNROLL'd axes nested in src.

static int term_subtree_has_unroll_axis(Term t, u32 axis_id, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_UNROLL) {
    u32 na = uop_unroll_n_args(t);
    for (u32 i = 0; i < na; i++) {
      if (uop_unroll_axis_id(t, i) == axis_id) return 1;
    }
    // Don't descend through UNROLL -- its inner has already been
    // expanded, won't reference the raw range axis_id.
    return 0;
  }
  u8 ar = uop_arity((u8)op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    if (term_subtree_has_unroll_axis(heap_read(loc + i), axis_id, depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static Term expander_fix_reduce_unroll(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_REDUCE) return 0;
  u32 n_axes = uop_reduce_n_axes(t);
  if (n_axes == 0) return 0;
  Term src = heap_read(term_val(t) + 0);
  // Partition: kept axes vs unrolled axes.
  u32 kept_axes[MAX_DIM];
  u32 n_kept = 0;
  u32 ur_axes[MAX_DIM];
  u32 ur_factors[MAX_DIM];
  u32 n_ur = 0;
  for (u32 i = 0; i < n_axes; i++) {
    u32 ax = uop_reduce_axis(t, i);
    // Find this axis's UNROLL factor by walking src.
    // Use a search helper that returns the factor when found.
    // (term_subtree_has_unroll_axis just bool; we need factor too.)
    // Inline a tiny walker: BFS over src subtree.
    u32 factor = 0;
    Term stack[64];
    u32 sp = 0;
    stack[sp++] = src;
    while (sp > 0 && factor == 0) {
      Term cur = stack[--sp];
      if (term_tag(cur) != TAG_UOP) continue;
      u32 cop = term_ext(cur);
      if (cop == UOP_UNROLL) {
        u32 na = uop_unroll_n_args(cur);
        for (u32 j = 0; j < na; j++) {
          if (uop_unroll_axis_id(cur, j) == ax) {
            factor = uop_unroll_factor(cur, j);
            break;
          }
        }
        // Don't descend.
        continue;
      }
      u8 ar = uop_arity((u8)cop);
      u64 cloc = term_val(cur);
      for (u8 j = 0; j < ar && j < MAX_UOP_SRC && sp < 64; j++) {
        stack[sp++] = heap_read(cloc + j);
      }
    }
    if (factor != 0) {
      ur_axes[n_ur] = ax;
      ur_factors[n_ur] = factor;
      n_ur++;
    } else {
      kept_axes[n_kept++] = ax;
    }
  }
  if (n_ur == 0) return 0;  // no UNROLL'd axes; leave the REDUCE alone.
  // Wrap src in CONTRACT(unrolled_axes), rebuild REDUCE with only the kept axes.
  u32 kind = uop_reduce_kind(t);
  Term new_src = uop_contract(src, n_ur, ur_axes, ur_factors);
  if (n_kept == 0) {
    // All axes were UNROLL'd -- the REDUCE becomes the CONTRACT's
    // horizontal reduction.  We still need a REDUCE wrapper to carry
    // the kind; emit a 0-axis REDUCE.  thvm's uop_reduce_multi handles
    // n_axes==0 (mirroring tinygrad's pure-horizontal REDUCE).
    return uop_reduce_multi(kind, 0, NULL, new_src);
  }
  return uop_reduce_multi(kind, n_kept, kept_axes, new_src);
}

// (fix_store_unroll removed: STORE is expanded by do_expand now, like
// tinygrad.  The old CONTRACT-wrap re-matched its own output under
// graph_rewrite's re-recursion and nested ~129 deep before the depth
// cap, which made BOTH the C-source and PTX linearized renderers bail
// on every conv kernel.)

// === do_expand (expander.py:22-75) ========================================
// Any UOp whose src[] contains an UNROLL: collect all UNROLL args
// across src children, deduplicated + sorted; broadcast non-UNROLL srcs
// to vector width = prod(factors); produce a vectorized root wrapped
// in a new outer UNROLL carrying the unified args.
//
// Eligible ops: ADD/MUL/NEG/RECIP/EXP2/LOG2/SQRT/CMPLT/CMPEQ
// + CAST/BITCAST + IADD/IMUL/... (the integer arithmetic the address
// trees use) + INDEX_E.

static int op_is_expandable(u32 op) {
  switch (op) {
    case UOP_ADD: case UOP_MUL: case UOP_NEG:
    case UOP_RECIP: case UOP_EXP2: case UOP_LOG2: case UOP_SQRT:
    case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_CAST: case UOP_BITCAST:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT: case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_IWHERE:
    case UOP_INDEX_E:
    // STORE is expandable (tinygrad expander.py:130 do_expand eligible
    // set).  An UPCAST'd output store has an UNROLL'd address (and value)
    // -- do_expand consumes the UNROLLs into a vectorized store wrapped
    // in one outer UNROLL, which the devectorizer lowers to F scalar
    // stores.  This REPLACES the old fix_store_unroll CONTRACT-wrap,
    // which re-matched its own output (graph_rewrite re-recurses) and
    // nested ~129 deep before the depth cap.
    case UOP_STORE:
      return 1;
    default:
      return 0;
  }
}

// Construct a broadcast of a scalar src to the unified vector width
// via UNROLL+VCONST when src is itself a VCONST/CONST scalar, or a
// CONTRACT wrapper (mirror of UOp.broadcast).  For arbitrary scalar
// srcs we wrap in an UNROLL(src) with the unified args -- the
// downstream renderer (when wired) needs to recognize "scalar
// broadcast UNROLL" but for now this is the structural correct shape.
//
// Tinygrad uses VCAT for non-scalar (count>1) srcs; we don't have VCAT,
// so we emit a CONTRACT wrapping the src for now, matching expander.py:54
// "VCAT(src.dtype.scalar().vec(...), (src,)*expand_sz)".
//
// For the simplest case (scalar src), build VCONST when it's a literal;
// otherwise wrap with UNROLL whose args are the unified ones and whose
// src is just `src` repeated -- we model this as UNROLL with a special
// "broadcast" semantics where its src is the scalar to repeat.  Since
// we can't carry that without a new opcode, we use CONTRACT(src, args)
// which by tinygrad expander.py:80 semantics: "CONTRACT without UNROLL
// repeats the element VECTORIZED" -- yes, this matches.

static Term expander_broadcast(Term src, u32 n_args, u32 const *ax,
                               u32 const *fa) {
  // CONTRACT without UNROLL underneath -> repeats element vectorized
  // (expander.py:80).
  return uop_contract(src, n_args, ax, fa);
}

static Term expander_do_expand(Term root) {
  if (term_tag(root) != TAG_UOP) return 0;
  u32 op = term_ext(root);
  if (!op_is_expandable(op)) return 0;
  u8 ar = uop_arity((u8)op);
  if (ar == 0) return 0;
  u64 loc = term_val(root);
  Term srcs[MAX_UOP_SRC];
  int any_unroll = 0;
  for (u8 i = 0; i < ar; i++) {
    srcs[i] = heap_read(loc + i);
    if (term_tag(srcs[i]) == TAG_UOP && term_ext(srcs[i]) == UOP_UNROLL) {
      any_unroll = 1;
    }
  }
  if (!any_unroll) return 0;
  // Collect args across all UNROLL srcs.
  Term unrolls[MAX_UOP_SRC];
  u32  n_unr = 0;
  for (u8 i = 0; i < ar; i++) {
    if (term_tag(srcs[i]) == TAG_UOP && term_ext(srcs[i]) == UOP_UNROLL) {
      unrolls[n_unr++] = srcs[i];
    }
  }
  u32 u_axes[MAX_DIM];
  u32 u_factors[MAX_DIM];
  u32 u_n = expander_collect_args(unrolls, n_unr, u_axes, u_factors);
  if (u_n == 0) return 0;
  // Build new srcs.
  Term new_srcs[MAX_UOP_SRC];
  for (u8 i = 0; i < ar; i++) {
    Term s = srcs[i];
    // STORE slot 0 is the buffer pointer -- it is shared across all F
    // lanes, never broadcast (mirrors tinygrad's "pass through range/
    // pointer args").  Pass it through unchanged.
    if (op == UOP_STORE && i == 0) {
      new_srcs[0] = s;
      continue;
    }
    if (term_tag(s) == TAG_UOP && term_ext(s) == UOP_UNROLL) {
      // Check if this UNROLL's args match the unified set exactly.
      u32 sa = uop_unroll_n_args(s);
      u32 s_axes[MAX_DIM];
      u32 s_factors[MAX_DIM];
      for (u32 j = 0; j < sa; j++) {
        s_axes[j]    = uop_unroll_axis_id(s, j);
        s_factors[j] = uop_unroll_factor(s, j);
      }
      if (args_equal(s_axes, s_factors, sa, u_axes, u_factors, u_n)) {
        // Unwrap.
        new_srcs[i] = heap_read(term_val(s) + 0);
      } else {
        // Need a GEP swizzle.
        u32 idx_buf[64];
        u32 nidx = expander_swizzle(u_axes, u_factors, u_n,
                                    s_axes, s_factors, sa, idx_buf);
        Term inner = heap_read(term_val(s) + 0);
        new_srcs[i] = uop_gep(inner, nidx, idx_buf);
      }
    } else {
      // Broadcast.
      new_srcs[i] = expander_broadcast(s, u_n, u_axes, u_factors);
    }
  }
  // Reconstruct the root op with new srcs.  We need to rebuild via
  // the standard constructors so dtype inference inherits naturally.
  Term rebuilt = 0;
  switch (op) {
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2: case UOP_LOG2:
    case UOP_SQRT:
      rebuilt = uop_unary(op, new_srcs[0]);
      break;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
      rebuilt = uop_binary(op, new_srcs[0], new_srcs[1]);
      break;
    case UOP_CAST: {
      u32 dt = (u32)term_val(heap_read(loc + 1));
      rebuilt = uop_cast(new_srcs[0], dt);
      break;
    }
    case UOP_BITCAST: {
      u32 dt = (u32)term_val(heap_read(loc + 1));
      rebuilt = uop_bitcast(new_srcs[0], dt);
      break;
    }
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT: case UOP_IAND: case UOP_IOR: case UOP_IXOR:
      rebuilt = uop_int_binary(op, new_srcs[0], new_srcs[1]);
      break;
    case UOP_IWHERE:
      rebuilt = uop_iwhere(new_srcs[0], new_srcs[1], new_srcs[2]);
      break;
    case UOP_INDEX_E:
      rebuilt = uop_index_e(new_srcs[0], new_srcs[1]);
      break;
    case UOP_STORE:
      // new_srcs[0]=buf (passthrough), [1]=expanded addr, [2]=expanded
      // value.  The vectorized store is wrapped in the outer UNROLL
      // below; devectorize lowers it to F scalar stores.
      rebuilt = uop_store(new_srcs[0], new_srcs[1], new_srcs[2]);
      break;
    default:
      return 0;
  }
  if (rebuilt == 0) return 0;
  return uop_unroll(rebuilt, u_n, u_axes, u_factors);
}

static Term expander_pm_do_expand(Term t, void *user) {
  (void)user;
  return expander_do_expand(t);
}

// === do_contract (expander.py:77-86) ======================================
// CONTRACT(UNROLL(x, eargs), cargs) -> UNROLL(GEP(x, swizzle), eargs \ cargs)
// CONTRACT(non-UNROLL) -> noop (broadcast semantics handled in
// expander_broadcast).

static Term expander_pm_do_contract(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONTRACT) return 0;
  Term src = heap_read(term_val(t) + 0);
  if (term_tag(src) != TAG_UOP || term_ext(src) != UOP_UNROLL) return 0;
  u32 c_n = uop_contract_n_args(t);
  u32 c_axes[MAX_DIM];
  u32 c_factors[MAX_DIM];
  for (u32 i = 0; i < c_n; i++) {
    c_axes[i]    = uop_contract_axis_id(t, i);
    c_factors[i] = uop_contract_factor(t, i);
  }
  u32 e_n = uop_unroll_n_args(src);
  u32 e_axes[MAX_DIM];
  u32 e_factors[MAX_DIM];
  for (u32 i = 0; i < e_n; i++) {
    e_axes[i]    = uop_unroll_axis_id(src, i);
    e_factors[i] = uop_unroll_factor(src, i);
  }
  // Compute eargs \ cargs (axes that remain after contraction).
  u32 r_axes[MAX_DIM];
  u32 r_factors[MAX_DIM];
  u32 r_n = 0;
  for (u32 i = 0; i < e_n; i++) {
    int in_c = 0;
    for (u32 j = 0; j < c_n; j++) {
      if (e_axes[i] == c_axes[j]) { in_c = 1; break; }
    }
    if (!in_c) {
      r_axes[r_n]    = e_axes[i];
      r_factors[r_n] = e_factors[i];
      r_n++;
    }
  }
  // Build the index list per expander.py:83-86: outer-loop over
  // remaining axes (r), inner over contracted axes (c), index =
  // _expand_arg_to_idx(eargs, {**r, **c}).
  Term inner = heap_read(term_val(src) + 0);
  u32 total_r = 1, total_c = 1;
  for (u32 i = 0; i < r_n; i++) total_r *= r_factors[i];
  for (u32 i = 0; i < c_n; i++) total_c *= c_factors[i];
  u32 total = total_r * total_c;
  if (total > 256) {
    // Cap; this is a structural limit on the GEP width we'll emit.
    return 0;
  }
  u32 idx_buf[256];
  u32 written = 0;
  for (u32 ri = 0; ri < total_r; ri++) {
    u32 r_vals[MAX_DIM] = {0};
    {
      u32 rem = ri;
      for (u32 rj = 0; rj < r_n; rj++) {
        u32 k = r_n - 1 - rj;
        r_vals[k] = rem % r_factors[k];
        rem /= r_factors[k];
      }
    }
    for (u32 ci = 0; ci < total_c; ci++) {
      u32 c_vals[MAX_DIM] = {0};
      {
        u32 rem = ci;
        for (u32 cj = 0; cj < c_n; cj++) {
          u32 k = c_n - 1 - cj;
          c_vals[k] = rem % c_factors[k];
          rem /= c_factors[k];
        }
      }
      // Compose vals in e_axes order: walk e_axes, find each axis in
      // r or c, take the value.
      u32 e_vals[MAX_DIM] = {0};
      for (u32 ei = 0; ei < e_n; ei++) {
        u32 ax = e_axes[ei];
        int found = 0;
        for (u32 j = 0; j < r_n; j++) {
          if (r_axes[j] == ax) { e_vals[ei] = r_vals[j]; found = 1; break; }
        }
        if (!found) {
          for (u32 j = 0; j < c_n; j++) {
            if (c_axes[j] == ax) { e_vals[ei] = c_vals[j]; break; }
          }
        }
      }
      idx_buf[written++] = expander_arg_to_idx(e_factors, e_n, e_vals);
    }
  }
  Term gepped = uop_gep(inner, written, idx_buf);
  if (r_n == 0) {
    return gepped;  // fully contracted -- no outer UNROLL needed.
  }
  return uop_unroll(gepped, r_n, r_axes, r_factors);
}

// === double UNROLL + empty UNROLL (expander.py:104-111) ==================

static Term expander_pm_double_unroll(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_UNROLL) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_UNROLL) return 0;
  u32 inner_n = uop_unroll_n_args(inner);
  u32 outer_n = uop_unroll_n_args(t);
  if (inner_n + outer_n > 8) return 0;
  u32 axes[MAX_DIM];
  u32 factors[MAX_DIM];
  for (u32 i = 0; i < inner_n; i++) {
    axes[i]    = uop_unroll_axis_id(inner, i);
    factors[i] = uop_unroll_factor(inner, i);
  }
  for (u32 i = 0; i < outer_n; i++) {
    axes[inner_n + i]    = uop_unroll_axis_id(t, i);
    factors[inner_n + i] = uop_unroll_factor(t, i);
  }
  Term inner_src = heap_read(term_val(inner) + 0);
  return uop_unroll(inner_src, inner_n + outer_n, axes, factors);
}

static Term expander_pm_empty_unroll(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_UNROLL) return 0;
  if (uop_unroll_n_args(t) != 0) return 0;
  return heap_read(term_val(t) + 0);
}

// === Entry point ==========================================================

// Run pm_pre_expander (pass 1), then expander (pass 3).
// pm_group_for_reduce (pass 2) is stubbed.  Mirrors codegen/__init__.py:54.
fn Term uop_expand_graph(Term root) {
  // Pass 1: pm_pre_expander (RANGE rewrite + fix_reduce_unroll + fix_store_unroll).
  static UOpGraphRewriteRule const PRE_RULES[] = {
    { "exp_range_to_unroll",   expander_pm_range },
    { "exp_fix_reduce_unroll", expander_fix_reduce_unroll },
    // fix_store_unroll removed: STORE is now expanded by do_expand
    // (op_is_expandable) like tinygrad, consuming the UNROLL'd address
    // rather than wrapping the whole store in a self-re-matching
    // CONTRACT (which nested ~129 deep).
  };
  Term t = uop_graph_rewrite(root, PRE_RULES,
                             sizeof(PRE_RULES) / sizeof(PRE_RULES[0]),
                             NULL);
  // Pass 3: do_expand + do_contract + double-UNROLL fold + empty UNROLL.
  static UOpGraphRewriteRule const EXP_RULES[] = {
    { "exp_double_unroll", expander_pm_double_unroll },
    { "exp_empty_unroll",  expander_pm_empty_unroll },
    { "exp_do_contract",   expander_pm_do_contract },
    { "exp_do_expand",     expander_pm_do_expand },
  };
  t = uop_graph_rewrite(t, EXP_RULES,
                        sizeof(EXP_RULES) / sizeof(EXP_RULES[0]),
                        NULL);
  return t;
}
