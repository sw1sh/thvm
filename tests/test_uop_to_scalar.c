// test_uop_to_scalar.c - Phase B3 wedge: UOp INDEX expression ->
// ScalarUop slot id translator.
//
// `uop_to_scalar` walks a UOp index tree built by Phase B1/B2 (e.g.
// from `uop_resolve_movement_chain`) and emits the equivalent
// ScalarUop nodes in the target kernel's arena.  This is the bridge
// the eventual rangeify rerouting will use to consume B3's outputs
// without rewriting per-USE address logic from scratch.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("uop-to-scalar/range-via-map");
  // Build a UOP_RANGE and a corresponding S_RANGE; verify the
  // translator returns the mapped scalar id rather than emitting a
  // fresh leaf.
  KernelEntry ke = {0};
  u32 sr = rangeify_emit_leaf(&ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | 32);
  Term ur = uop_range(0, S_AXIS_LOOP, 32);
  UopRangeMap ranges[1] = {{ur, sr}};
  u32 r = uop_to_scalar(&ke, ur, ranges, 1);
  CHECK_EQ(r, sr);

  TEST_BEGIN("uop-to-scalar/iconst-emits-s_iconst");
  Term c = uop_const(DT_INT32, 7);
  u32 sc = uop_to_scalar(&ke, c, ranges, 1);
  CHECK(sc > 0);
  ScalarUop const *ss = &ke.scalar_uops[sc];
  CHECK_EQ(ss->op, S_ICONST);
  CHECK_EQ((i64)ss->extra, 7);

  TEST_BEGIN("uop-to-scalar/binary-tree-emits-binary-scalar-tree");
  // Build (range + const) at UOp layer.  Translator emits S_IADD
  // wrapping the mapped S_RANGE and a fresh S_ICONST.
  Term sum = uop_int_binary(UOP_IADD, ur, c);
  u32 ssum = uop_to_scalar(&ke, sum, ranges, 1);
  CHECK(ssum > 0);
  ScalarUop const *sadd = &ke.scalar_uops[ssum];
  CHECK_EQ(sadd->op, S_IADD);
  CHECK_EQ(sadd->src[0], sr);
  CHECK_EQ(ke.scalar_uops[sadd->src[1]].op, S_ICONST);

  TEST_BEGIN("uop-to-scalar/iwhere-emits-s_iwhere");
  // Build IWHERE(range < c, range, c) at UOp layer.
  Term cond = uop_int_binary(UOP_ILT, ur, c);
  Term w = uop_iwhere(cond, ur, c);
  u32 sw = uop_to_scalar(&ke, w, ranges, 1);
  CHECK(sw > 0);
  CHECK_EQ(ke.scalar_uops[sw].op, S_IWHERE);

  TEST_BEGIN("uop-to-scalar/missing-range-returns-zero");
  // A RANGE not in the map fails translation.
  Term ur_missing = uop_range(99, S_AXIS_LOOP, 16);
  u32 fail = uop_to_scalar(&ke, ur_missing, ranges, 1);
  CHECK_EQ(fail, 0);

  TEST_BEGIN("uop-to-scalar/invalid-returns-zero");
  // UOP_INVALID is intentionally unsupported -- caller substitutes
  // an identity (0 / reduce sentinel) at the LOAD site.
  Term inv = uop_invalid();
  u32 fail2 = uop_to_scalar(&ke, inv, ranges, 1);
  CHECK_EQ(fail2, 0);

  TEST_BEGIN("uop-to-scalar/non-index-uop-returns-zero");
  // A non-INDEX UOp (e.g. UOP_ADD on tensors, UOP_RESHAPE) is
  // outside the translator's scope.
  Term tensor = term_new(0, TAG_TEN, DT_FP32, 1);
  Term tens_add = uop_binary(UOP_ADD, tensor, tensor);
  u32 fail3 = uop_to_scalar(&ke, tens_add, ranges, 1);
  CHECK_EQ(fail3, 0);

  TEST_BEGIN("uop-to-scalar/end-to-end-pad-style-mask");
  // Build the canonical PAD bounds-check shape at UOp layer:
  //   IWHERE(IAND(ILT(r, hi), ISUB(1, ILT(r, lo))), r-lo, INVALID)
  // and translate.  The resolver's simplifier folded the trivial
  // pieces; whatever survives must round-trip to ScalarUop.
  Term lo = uop_const(DT_INT32, 1);
  Term hi = uop_const(DT_INT32, 30);
  Term lt_hi = uop_int_binary(UOP_ILT, ur, hi);            // 32 > 30 so non-trivial
  Term lt_lo = uop_int_binary(UOP_ILT, ur, lo);            // not folded by simplifier
  Term ge_lo = uop_int_binary(UOP_ISUB, uop_const(DT_INT32, 1), lt_lo);
  Term mask = uop_int_binary(UOP_IAND, lt_hi, ge_lo);
  Term shifted = uop_int_binary(UOP_ISUB, ur, lo);
  Term load_addr = uop_iwhere(mask, shifted, uop_invalid());
  // INVALID branch can't translate; so this returns 0.  That's the
  // expected contract -- the caller owns substitution of INVALID
  // before calling the translator.
  u32 sla = uop_to_scalar(&ke, load_addr, ranges, 1);
  CHECK_EQ(sla, 0);

  // Translate the mask alone (no INVALID) -- it should succeed.
  u32 smask = uop_to_scalar(&ke, mask, ranges, 1);
  CHECK(smask > 0);
  CHECK_EQ(ke.scalar_uops[smask].op, S_IAND);

  rangeify_free(&ke);
  thvm_free();
  TEST_REPORT();
}
