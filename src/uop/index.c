// uop/index.c - constructors for the symbolic INDEX layer (Phase B0).
//
// These give the UOp DAG a representation for symbolic addresses
// (per-axis ranges, integer arithmetic, conditional WHERE, INVALID
// sentinel for PAD masks, and INDEX_E nodes that pair a buffer with
// a symbolic offset expression).  The opcodes mirror the ScalarUop
// S_* layer one-for-one so rangeify (Phase B3) can consume the UOp
// graph directly and emit the matching scalar form without translating
// across IR boundaries.
//
// All constructors hash-cons via uop_mov_cache so identical
// subexpressions share heap loc.  Movement-to-INDEX rules (Phase B1)
// build large symbolic trees that recur across consumers; sharing
// keeps the DAG compact and lets the symbolic simplifier (Phase B2)
// rewrite once per shape.

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
// Hash-cons-only constructor that does NOT call uop_rewrite_binary
// (whose rules are float-specific).  Phase B2's index_simplify.c
// owns integer-side folds.

fn Term uop_int_binary(u32 opcode, Term a, Term b) {
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
