// test_realize_abort.c - a thvm_fatal inside a realize, single-root or
// bundled, longjmps to the armed host (the WL entries' THVM_WL_RECOVER), and
// thvm_realize_ceiling_cleanup puts the runtime back where the realize
// entered: pool watermark, loc -> tid scope and fire scope.  The next
// realize then computes the right values.
#include "../src/thvm.c"
#include "test.h"

static Term ten4(float v) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = 4;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  float data[4] = {v, v, v, v};
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, sizeof data);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static float first(Term t) {
  float data[4] = {0};
  CPU_BACKEND.buf_read(TENS[(u32)term_val(term_resolve(t))].buf_id, data, sizeof data);
  return data[0];
}

// No kernel computes an i4 buffer, so classify stops a realize of this.
static Term int4_add(void) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = 6;
  Term p = term_new(0, TAG_TEN, DT_INT4, tensor_alloc(CURRENT_BACKEND, s, DT_INT4));
  Term q = term_new(0, TAG_TEN, DT_INT4, tensor_alloc(CURRENT_BACKEND, s, DT_INT4));
  return uop_binary(UOP_ADD, p, q);
}

// Realize `t` with the fatal hook armed, as a WL entry does; 1 when it
// stopped with a fatal.
static int realize_stops(Term t) {
  jmp_buf jb;
  thvm_heap_exhaust_jmp = &jb;
  thvm_heap_exhausted   = 0;
  if (setjmp(jb) != 0) {
    thvm_realize_ceiling_cleanup();
    thvm_heap_exhaust_jmp = NULL;
    return 1;
  }
  (void)thvm_realize(t);
  thvm_heap_exhaust_jmp = NULL;
  return 0;
}

int main(void) {
  thvm_init();

  for (int bundle = 0; bundle < 2; bundle++) {
    TEST_BEGIN(bundle ? "realize-abort/bundle-rolls-back" : "realize-abort/single-rolls-back");
    Term x       = ten4(1.0f);
    Term kids[2] = {uop_binary(UOP_MUL, x, ten4(3.0f)), int4_add()};
    Term t       = bundle ? term_new_ctr(0, kids, 2) : kids[1];
    u32  bufs    = (u32)CPU_BUFS_NEXT;
    CHECK_EQ(realize_stops(t), 1);
    CHECK_EQ((u32)CPU_BUFS_NEXT, bufs);
    CHECK_EQ(materialized_loc_scope_depth(), 0);
    CHECK_EQ(KERNEL_FIRE_SCOPE_DEPTH, 0);

    Term y       = uop_binary(UOP_ADD, x, ten4(2.0f));
    Term okay[2] = {y, uop_binary(UOP_MUL, y, ten4(4.0f))};
    Term r       = thvm_realize(term_new_ctr(0, okay, 2));
    CHECK(r != 0);
    CHECK(term_tag(r) == TAG_CTR);
    CHECK(first(term_ctr_at(r, 0)) == 3.0f);
    CHECK(first(term_ctr_at(r, 1)) == 12.0f);
    CHECK(thvm_realize_error()[0] == '\0');
  }

  thvm_free();
  TEST_REPORT();
}
