// uop/opt.c - construct UOP_OPT annotations (Phase D'4).
//
// UOP_OPT attaches an optimisation directive (UNROLL, UPCAST, TC,
// LOCAL, GROUP_REDUCE) to a target node.  Mirrors TileLang's OptOp
// system: T.unroll = OPT(range, UNROLL, factor); T.gemm composes a
// matmul shape + OPT(_, TC); T.Parallel = OPT(range, LOCAL, 0).
//
// The renderer (F0+) pattern-matches on (UOp shape, OPT kind) to fire
// specialised templates instead of emitting one new opcode per
// specialisation (no UOP_GEMM / UOP_CONV2D needed).
//
// Hash-cons via uop_mov_cache: identical (target, kind, factor)
// tuples share heap loc.  No consumer in this commit -- F0 is the
// first reader.

fn Term uop_opt(Term target, u32 kind, u32 factor) {
  u32 args[4] = { (u32)target, (u32)(target >> 32), kind, factor };
  u64 key = uop_mov_hash(UOP_OPT, 0, args, 4);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, target);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, kind));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, factor));
  Term t = term_new(0, TAG_UOP, UOP_OPT, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_opt_target(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_OPT) return 0;
  return heap_read(term_val(t) + 0);
}

fn u32 uop_opt_kind(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_OPT) return 0;
  return term_val(heap_read(term_val(t) + 1));
}

fn u32 uop_opt_factor(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_OPT) return 0;
  return term_val(heap_read(term_val(t) + 2));
}
