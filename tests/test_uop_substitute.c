// test_uop_substitute.c -- uop_substitute, tinygrad's UOp.substitute in walk
// mode (uop/ops.py:532): every key replaced at once, topmost match first, a
// replacement value never re-traversed, shared subtrees rewritten once, no
// depth limit, and every source a rewrite can reach (a BUFFERIZE's closed
// ranges, both cells of an AFTER) reached.  An opcode it does not know is fatal.
#include "../src/thvm.c"
#include "test.h"

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static int substitute_stops(Term root, Term from, Term to) {
  jmp_buf jb;
  thvm_heap_exhaust_jmp = &jb;
  if (setjmp(jb) != 0) {
    thvm_heap_exhaust_jmp = NULL;
    return 1;
  }
  (void)uop_substitute(root, &from, &to, 1);
  thvm_heap_exhaust_jmp = NULL;
  return 0;
}

int main(void) {
  thvm_init();
  Term a = ten(1), b = ten(2), c = ten(3);

  TEST_BEGIN("substitute/swap-terminates-topmost-first");
  {
    Term from[2] = { a, b }, to[2] = { b, a };
    CHECK_EQ(uop_substitute(uop_binary(UOP_ADD, a, b), from, to, 2), uop_binary(UOP_ADD, b, a));
    Term grow = uop_binary(UOP_MUL, a, a);
    CHECK_EQ(uop_substitute(a, &a, &grow, 1), grow);
  }

  TEST_BEGIN("substitute/shared-subtree-once");
  {
    Term s    = uop_binary(UOP_MUL, a, c);
    Term s2   = uop_binary(UOP_MUL, b, c);
    Term root = uop_binary(UOP_ADD, s, uop_unary(UOP_NEG, s));
    CHECK_EQ(uop_substitute(root, &a, &b, 1), uop_binary(UOP_ADD, s2, uop_unary(UOP_NEG, s2)));
  }

  TEST_BEGIN("substitute/deep-chain-reaches-the-bottom");
  {
    Term chain = a, want = b;
    for (u32 i = 0; i < 2000; i++) {
      chain = uop_binary(UOP_ADD, chain, c);
      want  = uop_binary(UOP_ADD, want, c);
    }
    CHECK_EQ(uop_substitute(chain, &a, &b, 1), want);
  }

  TEST_BEGIN("substitute/reaches-bufferize-ranges-after-detach-copy");
  {
    Term r0 = uop_range(0, KAX_LOOP, 4), r1 = uop_range(1, KAX_LOOP, 4);
    Term bz = uop_bufferize_new(a, UOP_SCOPE_GLOBAL, 1, 1, &r0);
    CHECK_EQ(uop_substitute(bz, &r0, &r1, 1), uop_bufferize_new(a, UOP_SCOPE_GLOBAL, 1, 1, &r1));
    Term x = uop_binary(UOP_ADD, a, c), y = uop_binary(UOP_MUL, a, c);
    Term x2 = uop_binary(UOP_ADD, b, c), y2 = uop_binary(UOP_MUL, b, c);
    CHECK_EQ(uop_substitute(uop_after(x, y), &a, &b, 1), uop_after(x2, y2));
    Term d = uop_substitute(uop_detach(x), &a, &b, 1);   // a DETACH is not hash-consed
    CHECK_EQ(term_ext(d), UOP_DETACH);
    CHECK_EQ(heap_read(term_val(d)), x2);
    CHECK_EQ(uop_substitute(uop_copy_dev(x, 0), &a, &b, 1), uop_copy_dev(x2, 0));
    CHECK_EQ(uop_substitute(x, &c, &c, 1), x);
  }

  TEST_BEGIN("substitute/unknown-opcode-is-fatal");
  {
    u64 loc = heap_alloc(1);
    heap_set(loc, a);
    CHECK(substitute_stops(term_new(0, TAG_UOP, 46, loc), a, b));
  }

  thvm_free();
  TEST_REPORT();
}
