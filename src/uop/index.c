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
// axis_type uses the same S_AXIS_LOOP/REDUCE/UNROLL/GLOBAL/VIRT
// encoding so a downstream lowering can reuse the constants.

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

// === Phase E1: UOP_RANGE accessors + axis_type rewriter ===
//
// Groundwork for porting `apply_opt.c`'s KernelAxes mutations onto
// UPatRule[] over UOP_RANGE leaves.  Today KernelAxes.axis_types[]
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

fn Term uop_range_with_axis_type(Term r, u32 new_axis_type) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return r;
  u32 axis_id = uop_range_axis_id(r);
  u32 extent  = uop_range_extent(r);
  return uop_range(axis_id, new_axis_type, extent);
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
