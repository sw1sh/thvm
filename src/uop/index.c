// uop/index.c - constructors for the symbolic INDEX layer.
//
// These give the UOp DAG a representation for symbolic addresses
// (per-axis ranges, integer arithmetic, conditional WHERE, INVALID
// sentinel for PAD masks, and INDEX_E nodes that pair a buffer with
// a symbolic offset expression).
//
// All constructors hash-cons via uop_mov_cache so identical
// subexpressions share heap loc.  Movement-to-INDEX rules build
// large symbolic trees that recur across consumers; sharing keeps
// the DAG compact and lets the symbolic simplifier rewrite once
// per shape.

// === UOP_RANGE: axis-iter leaf ===
//
// Heap layout: [NUM(axis_id), NUM(axis_type), NUM(extent)].
// axis_type uses the KAX_LOOP/REDUCE/UPCAST/UNROLL/LOCAL/GLOBAL/
// GROUP_REDUCE encoding from the KAX_* defines.

fn Term uop_range(u32 axis_id, u32 axis_type, u32 extent) {
  u32 args[3] = { axis_id, axis_type, extent };
  u64 key = uop_mov_hash(UOP_RANGE, 0, args, 3);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, axis_id));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, axis_type));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, extent));
  Term t = term_new(0, TAG_UOP, UOP_RANGE, loc);
  uop_mov_insert(key, t);
  return t;
}

// === UOP_RANGE accessors + axis_type rewriter ===
//
// Groundwork for porting `apply_opt.c`'s KpSchedule mutations onto
// UPatRule[] over UOP_RANGE leaves.  Today KpSchedule.axis_types[]
// stays the primary source of truth and kernel_lift_to_uop replays
// applied_opts onto cur[].axis_type in C control flow before
// emitting UOP_RANGE leaves with the resolved axis_type baked in.
//
// The Phase E port turns each apply_opt mutation into a UPatRule
// over UOP_RANGE.axis_type / UOP_OPT-wrapped UOP_RANGE.  These four
// helpers are the read/write primitives those rules will compose:
//
//   uop_range_axis_id  / uop_range_axis_type  / uop_range_extent
//     -- field accessors that hide the [NUM(axis_id), NUM(axis_type),
//        NUM(extent)] heap layout from rule bodies.
//
//   uop_range_with_axis_type(old, new_axis_type)
//     -- structural rewriter; preserves axis_id/extent and returns the
//        hash-cons-shared Term for the new axis_type.  Equivalent to
//        the `axis_types[i] = ti` assignments in axes_apply_opt.c
//        (KOP_GLOBAL: KAX_LOOP -> KAX_GLOBAL, and KOP_SWAP's pairwise
//        swap of axis_types[]).
//
// E1 deliberately doesn't introduce new opcodes or change existing
// callers -- it adds the read/write seam that subsequent E* wedges
// (KOP_GLOBAL, KOP_SWAP, KOP_UPCAST/UNROLL/LOCAL/GROUP splits, KOP_TC)
// will turn into UPatRule entries.

fn u32 uop_range_axis_id(Term r) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  return (u32)term_val(heap_read(term_val(r) + 0));
}

fn u32 uop_range_axis_type(Term r) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  return (u32)term_val(heap_read(term_val(r) + 1));
}

fn u32 uop_range_extent(Term r) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  return (u32)term_val(heap_read(term_val(r) + 2));
}

// kvar wedge: the low 31 bits of `extent` hold either a literal
// extent or a kvar id when bit 31 (KVAR_FLAG) is set.  These three
// helpers are the canonical accessor surface for "is this range
// symbolic? what var? what's the static upper bound?".
fn int uop_range_extent_is_var(Term r) {
  return kvar_extent_is_var(uop_range_extent(r));
}

fn u32 uop_range_var_id(Term r) {
  return kvar_extent_var_id(uop_range_extent(r));
}

// Static extent: for literal ranges, identity.  For kvar-bound
// ranges, the registered upper bound (kvar_hi).  Use this when
// computing buffer sizes / dispatch shapes -- the worst-case footprint
// must be allocated even when the runtime value is smaller.
fn u32 uop_range_static_extent(Term r) {
  return kvar_extent_static(uop_range_extent(r));
}

fn Term uop_range_with_axis_type(Term r, u32 new_axis_type) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return r;
  u32 axis_id = uop_range_axis_id(r);
  u32 extent  = uop_range_extent(r);
  return uop_range(axis_id, new_axis_type, extent);
}

// === uop_range_split primitive ========================================
//
// Replaces a single UOP_RANGE leaf with a (outer, inner) pair that
// represents the same iteration space, plus a `linear_index` Term that
// the caller substitutes for any consumer that referenced the original
// leaf.  Mirrors kernel_lift.c's structural-replay "split" block
// (lines ~1561-1604) at the UOp DAG level: that block walks the
// SplitAxis cur[] vector, shifts entries right at o.axis+1, halves the
// outer extent, and stamps inner with the opt's KAX_ type.  The
// consumer's INDEX_E.addr arithmetic is implicitly rebuilt by the
// `factor` field of the SplitAxis vector (outer's factor = old factor *
// inner_extent).
//
// uop_range_split returns the same three values declaratively so a
// UPatRule (rw_range_split below) can replace one UOP_RANGE leaf with
// the equivalent (outer * k + inner) sub-expression and let
// uop_pattern_rewrite's bottom-up rebuild thread the new linear_index
// through every IADD/IMUL chain that consumed the old leaf.
//
// Heap layout returned (no new opcode -- composition of existing
// UOP_RANGE + UOP_IMUL + UOP_IADD + UOP_CONST):
//
//   outer        : UOP_RANGE(axis=N,   axis_type=LOOP,           extent=E/k)
//   inner        : UOP_RANGE(axis=N+1, axis_type=inner_axis_type, extent=k)
//   linear_index : uop_int_binary(UOP_IADD,
//                                 uop_int_binary(UOP_IMUL, outer, k_const),
//                                 inner)
//
// Pre-conditions:
//   - old_range is a UOP_RANGE term (returns zero-init on tag mismatch).
//   - k > 0 and old_range.extent % k == 0 (returns zero-init otherwise).
//   - inner_axis_type is one of KAX_UPCAST/UNROLL/LOCAL/GROUP_REDUCE
//     (the rule body / caller validates this; the primitive accepts
//     any u32 and trusts the caller, mirroring uop_range's own
//     unchecked axis_type).
//
// Hash-cons: outer/inner/linear_index all flow through the canonical
// uop_range / uop_int_binary / uop_const constructors, so re-running
// uop_range_split with the same inputs returns hash-cons-identical
// Terms.  This is the property a UPatRule that descends through
// IADD/IMUL chains relies on: the rewriter's memo table dedups equal
// subgraphs across calls.

fn UopRangeSplit uop_range_split(Term old_range, u32 k, u32 inner_axis_type) {
  UopRangeSplit out = {0, 0, 0};
  if (term_tag(old_range) != TAG_UOP || term_ext(old_range) != UOP_RANGE) {
    return out;
  }
  if (k == 0) return out;
  u32 axis_id = uop_range_axis_id(old_range);
  u32 extent  = uop_range_extent(old_range);
  if (extent % k != 0) return out;
  u32 outer_axis_type = uop_range_axis_type(old_range);

  out.outer = uop_range(axis_id,     outer_axis_type, extent / k);
  out.inner = uop_range(axis_id + 1, inner_axis_type, k);
  Term k_const = uop_const(DT_INT32, k);
  Term scaled  = uop_int_binary(UOP_IMUL, out.outer, k_const);
  out.linear_index = uop_int_binary(UOP_IADD, scaled, out.inner);
  return out;
}

// === UOP_INDEX_E: symbolic INDEX expression ===
//
// Heap layout: [buffer_src, addr_expr].

fn Term uop_index_e(Term buffer, Term addr) {
  u32 args[4] = { (u32)buffer, (u32)(buffer >> 32),
                  (u32)addr,   (u32)(addr >> 32) };
  u64 key = uop_mov_hash(UOP_INDEX_E, 0, args, 4);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, buffer);
  heap_set(loc + 1, addr);
  Term t = term_new(0, TAG_UOP, UOP_INDEX_E, loc);
  uop_mov_insert(key, t);
  return t;
}

// === UOP_I{ADD,SUB,MUL,DIV,MOD,LT,AND}: integer binary ===
//
// `uop_simplify_int_binary` runs first to fold identity /
// annihilator / constant cases.  Hash-cons applies to the unfolded
// form so distinct shape-different chains still dedup at construction.

fn Term uop_int_binary(u32 opcode, Term a, Term b) {
  Term folded = uop_simplify_int_binary(opcode, a, b);
  if (folded != 0) return folded;
  u32 args[4] = { (u32)a, (u32)(a >> 32), (u32)b, (u32)(b >> 32) };
  u64 key = uop_mov_hash(opcode, 0, args, 4);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  Term t = term_new(0, TAG_UOP, opcode, loc);
  uop_mov_insert(key, t);
  return t;
}

// === UOP_IWHERE: ternary select ===
//
// Heap layout: [cond, then_v, else_v].

fn Term uop_iwhere(Term cond, Term then_v, Term else_v) {
  Term folded = uop_simplify_iwhere(cond, then_v, else_v);
  if (folded != 0) return folded;
  u32 args[6] = { (u32)cond,   (u32)(cond   >> 32),
                  (u32)then_v, (u32)(then_v >> 32),
                  (u32)else_v, (u32)(else_v >> 32) };
  u64 key = uop_mov_hash(UOP_IWHERE, 0, args, 6);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, cond);
  heap_set(loc + 1, then_v);
  heap_set(loc + 2, else_v);
  Term t = term_new(0, TAG_UOP, UOP_IWHERE, loc);
  uop_mov_insert(key, t);
  return t;
}

// === UOP_INVALID: PAD-mask sentinel ===
//
// Heap layout: [NUM(0)] (placeholder).  Singleton via mov_cache.

fn Term uop_invalid(void) {
  u64 key = uop_mov_hash(UOP_INVALID, 0, NULL, 0);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(1);
  heap_set(loc, term_new(0, TAG_NUM, DT_INT32, 0));
  Term t = term_new(0, TAG_UOP, UOP_INVALID, loc);
  uop_mov_insert(key, t);
  return t;
}

// === UOP_BUFFERIZE: realize-boundary main-heap node ===
//
// Heap layout: [value, NUM(addrspace), NUM(removable), NUM(n_ranges),
//               range_0, ..., range_{n_ranges-1}].
//
// Mirror source: tinygrad/schedule/indexing.py:77 (`UOp(Ops.BUFFERIZE,
// s.dtype, src=(new_src,)+closed_ranges, arg=opts)` inside
// `create_bufferize_and_index_based_on_ranges`).
//
// Hash-cons via uop_mov_cache so reusing the same (value, addrspace,
// removable, ranges) tuple returns the same Term.  The unified pass
// calls this once per realize boundary the run_rangeify walk decided;
// identical boundaries dedup naturally.

fn Term uop_bufferize_new(Term value, u32 addrspace, u32 removable,
                          u32 n_ranges, Term const *ranges) {
  // Bound the range list to MAX_DIM (the same per-axis limit
  // RU_RANGE_MAP uses).  Tinygrad bounds via its UPat semantics; we
  // bound here to keep the heap layout deterministic.
  if (n_ranges > MAX_DIM) n_ranges = MAX_DIM;

  // Build hash key over (op, value, addrspace, removable, n_ranges, ranges...).
  // 2 u32 per Term + 3 u32 for the header = 2 + 3 + 2*n_ranges.
  u32 args[3 + 2 + 2 * MAX_DIM];
  args[0] = (u32)value;
  args[1] = (u32)(value >> 32);
  args[2] = addrspace;
  args[3] = removable;
  args[4] = n_ranges;
  for (u32 i = 0; i < n_ranges; i++) {
    args[5 + 2 * i + 0] = (u32)ranges[i];
    args[5 + 2 * i + 1] = (u32)(ranges[i] >> 32);
  }
  u64 key = uop_mov_hash(UOP_BUFFERIZE, 0, args, 5 + 2 * n_ranges);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;

  // Heap layout: [value, NUM(addrspace), NUM(removable), NUM(n_ranges),
  //               range_0, ..., range_{n-1}].
  u64 loc = heap_alloc(4 + n_ranges);
  heap_set(loc + 0, value);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, addrspace));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, removable));
  heap_set(loc + 3, term_new(0, TAG_NUM, DT_INT32, n_ranges));
  for (u32 i = 0; i < n_ranges; i++) {
    heap_set(loc + 4 + i, ranges[i]);
  }
  Term t = term_new(0, TAG_UOP, UOP_BUFFERIZE, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_bufferize_value(Term b) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  return heap_read(term_val(b) + 0);
}

fn u32 uop_bufferize_addrspace(Term b) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  return (u32)term_val(heap_read(term_val(b) + 1));
}

fn u32 uop_bufferize_removable(Term b) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  return (u32)term_val(heap_read(term_val(b) + 2));
}

fn u32 uop_bufferize_n_ranges(Term b) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  return (u32)term_val(heap_read(term_val(b) + 3));
}

fn Term uop_bufferize_range_at(Term b, u32 i) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  u32 n = uop_bufferize_n_ranges(b);
  if (i >= n) return 0;
  return heap_read(term_val(b) + 4 + i);
}
