// test_uop_range_axis_type.c -- Phase E8 graph-rebuild extension.
//
// Phase E1-E6 stamps `axis_type` on UOP_RANGE leaves via UPatRules
// applied through uop_pattern_rewrite. The fundamental limitation:
// uop_pattern_rewrite descends only through opcodes enumerated in
// uop_arity (schedule/uop_meta.c) AND rebuildable by
// uop_graph_rebuild_with_srcs (uop/graph_rewrite.c). Before E8,
// these covered float arithmetic + movement ops + REDUCE/LOAD/CAST/
// BITCAST -- but NOT the symbolic INDEX layer (UOP_INDEX_E + the
// IADD/IMUL/IDIV/IMOD/ILT/IAND chain + UOP_IWHERE + UOP_INVALID +
// UOP_OPT + UOP_RANGE + UOP_STORE).
//
// In production lifter output, UOP_RANGE leaves are nested inside
// UOP_INDEX_E.addr (which is an IADD/IMUL chain), so a UPatRule on
// UOP_RANGE could only fire on a bare UOP_RANGE root before E8.
//
// E8 extends uop_arity to return correct child counts for those
// opcodes and uop_graph_rebuild_with_srcs to reconstruct each from
// rewritten children. This test demonstrates that, after E8,
// uop_pattern_rewrite reaches a UOP_RANGE leaf nested several levels
// deep (UOP_STORE -> UOP_INDEX_E.addr -> UOP_IADD -> UOP_IMUL ->
// UOP_RANGE).

#include "../src/thvm.c"
#include "test.h"

// ----------------------------------------------------------------------
// Rule: stamp axis_type=KAX_GLOBAL on UOP_RANGE leaves whose axis_id
// matches the user-provided slot. Mirrors the shape an E2/E3 axis
// rewriter would take.

typedef struct {
  u32 axis_id;
  u32 new_axis_type;
  u32 fire_count;
} StampCtx;

static Term rw_stamp_range(Term const *bindings, void *user) {
  (void)bindings;
  StampCtx *ctx = (StampCtx *)user;
  Term t = bindings[0];
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  u64 loc = term_val(t);
  u32 axis_id  = (u32)term_val(heap_read(loc + 0));
  u32 axis_typ = (u32)term_val(heap_read(loc + 1));
  u32 extent   = (u32)term_val(heap_read(loc + 2));
  if (axis_id != ctx->axis_id) return 0;
  if (axis_typ == ctx->new_axis_type) return 0;  // already stamped
  ctx->fire_count++;
  return uop_range(axis_id, ctx->new_axis_type, extent);
}

int main(void) {
  thvm_init();

  // ------------------------------------------------------------------
  // (1) Bare RANGE root -- pre-E8 also worked.  Sanity check.
  TEST_BEGIN("e8/bare-range-root-stamps");
  Term r0 = uop_range(/*axis_id=*/0, KAX_LOOP, /*extent=*/16);
  CHECK_EQ(term_tag(r0), TAG_UOP);
  CHECK_EQ(term_ext(r0), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(r0) + 1)), KAX_LOOP);

  // RANGE has uop_arity=0; pattern matches with nsrc=0.  bind=0
  // captures the matched RANGE term itself for the rewrite fn.
  static UPat const pat_range = {UOP_RANGE, 0, 0, 0, NULL, NULL};

  StampCtx ctx0 = { .axis_id = 0, .new_axis_type = KAX_GLOBAL };
  UPatRule rules[] = {{&pat_range, rw_stamp_range}};
  Term r0_out = uop_pattern_rewrite(r0, rules, 1, &ctx0);
  CHECK_EQ(term_tag(r0_out), TAG_UOP);
  CHECK_EQ(term_ext(r0_out), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(r0_out) + 0)), 0);
  CHECK_EQ((u32)term_val(heap_read(term_val(r0_out) + 1)), KAX_GLOBAL);
  CHECK_EQ((u32)term_val(heap_read(term_val(r0_out) + 2)), 16);
  CHECK_EQ(ctx0.fire_count, 1);

  // ------------------------------------------------------------------
  // (2) Production lifter shape: UOP_STORE(buf, INDEX_E(buf,
  //     IADD(IMUL(RANGE(axis=0), const), RANGE(axis=1))), value).
  //     The UOP_RANGE at axis=0 is FOUR levels deep beneath the root:
  //     STORE -> INDEX_E -> IADD -> IMUL -> RANGE.  Without E8's
  //     extensions to uop_arity / uop_graph_rebuild_with_srcs, the
  //     rewriter would stop at the STORE node (arity 0) without
  //     descending into INDEX_E.addr's address arithmetic, and the
  //     stamp rule would never reach the RANGE leaf.
  TEST_BEGIN("e8/nested-range-under-store-index-e-iadd-imul-stamps");
  u32 buf_dims[2] = { 4, 16 };
  Term buf = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, buf_dims);

  Term r_axis0 = uop_range(/*axis_id=*/0, KAX_LOOP, /*extent=*/4);
  Term r_axis1 = uop_range(/*axis_id=*/1, KAX_LOOP, /*extent=*/16);

  // Stride for the row axis = 16 (the inner-axis extent). Avoid 0/1
  // values that would trigger the int-binary identity simplifier.
  Term stride = uop_const(DT_INT32, 16);
  Term row    = uop_int_binary(UOP_IMUL, r_axis0, stride);
  Term addr   = uop_int_binary(UOP_IADD, row, r_axis1);
  Term ie     = uop_index_e(buf, addr);

  // Value being stored: any non-buffer expression. A UOP_LOAD of the
  // INDEX_E mirrors the trivial copy-store shape T.copy lowers to.
  Term value  = uop_load(ie);
  Term root   = uop_store(buf, addr, value);

  CHECK_EQ(term_tag(root), TAG_UOP);
  CHECK_EQ(term_ext(root), UOP_STORE);

  // Apply the rule asking for axis_id=0 -> KAX_GLOBAL.
  StampCtx ctx1 = { .axis_id = 0, .new_axis_type = KAX_GLOBAL };
  Term out = uop_pattern_rewrite(root, rules, 1, &ctx1);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_STORE);

  // Walk down: STORE.addr -> IADD -> IMUL.lhs should be RANGE(axis=0,
  // axis_type=KAX_GLOBAL, extent=4).
  Term out_addr = heap_read(term_val(out) + 1);
  CHECK_EQ(term_tag(out_addr), TAG_UOP);
  CHECK_EQ(term_ext(out_addr), UOP_IADD);
  Term out_imul = heap_read(term_val(out_addr) + 0);
  CHECK_EQ(term_tag(out_imul), TAG_UOP);
  CHECK_EQ(term_ext(out_imul), UOP_IMUL);
  Term out_range0 = heap_read(term_val(out_imul) + 0);
  CHECK_EQ(term_tag(out_range0), TAG_UOP);
  CHECK_EQ(term_ext(out_range0), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range0) + 0)), 0);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range0) + 1)), KAX_GLOBAL);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range0) + 2)), 4);

  // The RANGE at axis_id=1 should remain at KAX_LOOP -- the rule
  // guards on axis_id and only stamps axis 0.
  Term out_range1 = heap_read(term_val(out_addr) + 1);
  CHECK_EQ(term_tag(out_range1), TAG_UOP);
  CHECK_EQ(term_ext(out_range1), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range1) + 0)), 1);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range1) + 1)), KAX_LOOP);

  // The rule fires once on the heap-cons'd RANGE node despite it
  // appearing twice in the DAG (once inside STORE.addr, once inside
  // INDEX_E.addr inside the LOAD value): the rewriter memoizes.
  CHECK_EQ(ctx1.fire_count, 1);

  // Same INDEX_E node beneath the value's LOAD also has its
  // RANGE(axis=0) updated -- the IMUL child now points at the same
  // KAX_GLOBAL range because uop_int_binary is hash-cons'd.
  Term out_value = heap_read(term_val(out) + 2);
  CHECK_EQ(term_tag(out_value), TAG_UOP);
  CHECK_EQ(term_ext(out_value), UOP_LOAD);
  Term out_value_ie = heap_read(term_val(out_value) + 0);
  CHECK_EQ(term_tag(out_value_ie), TAG_UOP);
  CHECK_EQ(term_ext(out_value_ie), UOP_INDEX_E);
  Term out_value_addr = heap_read(term_val(out_value_ie) + 1);
  CHECK_EQ(out_value_addr, out_addr);

  // ------------------------------------------------------------------
  // (3) Coverage: uop_arity now returns the documented child counts
  //     for every newly enumerated opcode. The rewriter / view / GC
  //     mark walks all key on this table; making the values explicit
  //     in a test pins the contract.
  TEST_BEGIN("e8/uop-arity-symbolic-index-layer");
  CHECK_EQ(uop_arity(UOP_RANGE),    0);
  CHECK_EQ(uop_arity(UOP_INVALID),  0);
  CHECK_EQ(uop_arity(UOP_OPT),      1);
  CHECK_EQ(uop_arity(UOP_IADD),     2);
  CHECK_EQ(uop_arity(UOP_ISUB),     2);
  CHECK_EQ(uop_arity(UOP_IMUL),     2);
  CHECK_EQ(uop_arity(UOP_IDIV),     2);
  CHECK_EQ(uop_arity(UOP_IMOD),     2);
  CHECK_EQ(uop_arity(UOP_ILT),      2);
  CHECK_EQ(uop_arity(UOP_IAND),     2);
  CHECK_EQ(uop_arity(UOP_INDEX_E),  2);
  CHECK_EQ(uop_arity(UOP_IWHERE),   3);
  CHECK_EQ(uop_arity(UOP_STORE),    3);

  // ------------------------------------------------------------------
  // (4) uop_graph_rebuild_with_srcs reaches each new opcode through
  //     the bottom-up rewrite path. We exercise it with an identity
  //     rewrite (rule never fires) so every node in the DAG is
  //     visited via uop_graph_rewrite_rec; if rebuild bombed (e.g.
  //     by returning `t` for UOP_INDEX_E and dropping a child
  //     rewrite), the resulting graph would diverge.
  TEST_BEGIN("e8/identity-rewrite-survives-symbolic-layer");
  // No-op rule -- never returns a replacement.
  static UPat const pat_never = {UOP_NEG, 1, 0, -1, NULL, NULL};
  UPatRule no_rules[] = {{&pat_never, rw_stamp_range}};
  StampCtx idle = { .axis_id = 99, .new_axis_type = KAX_GLOBAL };
  Term root2 = uop_pattern_rewrite(root, no_rules, 1, &idle);
  // root contains no UOP_NEG; the rule never fires; the graph is
  // structurally identical (all nodes hash-cons back to themselves).
  CHECK_EQ(root2, root);
  CHECK_EQ(idle.fire_count, 0);

  // ------------------------------------------------------------------
  // (5) IWHERE descent: build IWHERE(ILT(RANGE0, c), RANGE1, INVALID)
  //     and rewrite RANGE1's axis_type. Verifies that the rebuilder
  //     handles the 3-arity IWHERE shape and that arity-0 INVALID is
  //     also reached without stalling the walker.
  TEST_BEGIN("e8/iwhere-with-invalid-leaf-stamps-then");
  Term lim   = uop_const(DT_INT32, 3);
  Term cond  = uop_int_binary(UOP_ILT, r_axis0, lim);
  Term inv   = uop_invalid();
  Term iw    = uop_iwhere(cond, r_axis1, inv);
  StampCtx ctx2 = { .axis_id = 1, .new_axis_type = KAX_LOCAL };
  Term iw_out = uop_pattern_rewrite(iw, rules, 1, &ctx2);
  CHECK_EQ(term_tag(iw_out), TAG_UOP);
  CHECK_EQ(term_ext(iw_out), UOP_IWHERE);
  Term iw_then = heap_read(term_val(iw_out) + 1);
  CHECK_EQ(term_tag(iw_then), TAG_UOP);
  CHECK_EQ(term_ext(iw_then), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(iw_then) + 0)), 1);
  CHECK_EQ((u32)term_val(heap_read(term_val(iw_then) + 1)), KAX_LOCAL);
  // INVALID arm stays the singleton.
  Term iw_else = heap_read(term_val(iw_out) + 2);
  CHECK_EQ(term_tag(iw_else), TAG_UOP);
  CHECK_EQ(term_ext(iw_else), UOP_INVALID);
  CHECK_EQ(ctx2.fire_count, 1);

  // ------------------------------------------------------------------
  // (6) UOP_OPT descent: wrap a RANGE in UOP_OPT(target, kind=UNROLL,
  //     factor=4) and verify the rewrite reaches and replaces the
  //     target while the kind/factor scalar metadata is preserved.
  TEST_BEGIN("e8/opt-wraps-range-rewrite-replaces-target");
  Term opt_node = uop_opt(r_axis0, UOP_OPT_UNROLL, 4);
  CHECK_EQ(term_tag(opt_node), TAG_UOP);
  CHECK_EQ(term_ext(opt_node), UOP_OPT);
  StampCtx ctx3 = { .axis_id = 0, .new_axis_type = KAX_UNROLL };
  Term opt_out = uop_pattern_rewrite(opt_node, rules, 1, &ctx3);
  CHECK_EQ(term_tag(opt_out), TAG_UOP);
  CHECK_EQ(term_ext(opt_out), UOP_OPT);
  // Verify the kind / factor scalar slots round-tripped intact.
  CHECK_EQ(uop_opt_kind(opt_out),   UOP_OPT_UNROLL);
  CHECK_EQ(uop_opt_factor(opt_out), 4);
  Term opt_target = uop_opt_target(opt_out);
  CHECK_EQ(term_tag(opt_target), TAG_UOP);
  CHECK_EQ(term_ext(opt_target), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(opt_target) + 1)), KAX_UNROLL);
  CHECK_EQ(ctx3.fire_count, 1);

  thvm_free();
  TEST_REPORT();
}
