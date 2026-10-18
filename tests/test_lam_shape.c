// test_lam_shape.c - a lambda body materialized BEFORE its binder is
// substituted: the shape-annotated VAR (the lam_shape side table) becomes a
// kernel input slot, and APP-LAM then substitutes a TEN whose buffer the
// kernel reads at fire time.  The C port of lam_shape.wlt's end-to-end
// materialize-body-then-apply case.
#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("lam-shape/materialize-body-then-apply");
  {
    Shape s = {0};
    s.ndim    = 1;
    s.dims[0] = 3;
    u64 lam_loc = heap_alloc(1);
    heap_set(lam_loc, 0);
    lam_shape_set(lam_loc, &s);
    Term var  = term_new(0, TAG_VAR, 0, lam_loc);
    u32  k0   = KERNELS_NEXT;
    Term body = thvm_materialize(uop_binary(UOP_ADD, var, var));
    heap_set(lam_loc, body);
    Term lam  = term_new(0, TAG_LAM, 0, lam_loc);

    u32   tid   = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
    float in[3] = {1.0f, 2.0f, 3.0f};
    CPU_BACKEND.buf_write(TENS[tid].buf_id, in, sizeof in);
    u64 app = heap_alloc(2);
    heap_set(app + 0, lam);
    heap_set(app + 1, term_new(0, TAG_TEN, DT_FP32, tid));
    Term r = term_resolve(wnf(term_new(0, TAG_APP, 0, app)));
    CHECK_EQ(term_tag(r), TAG_TEN);
    if (term_tag(r) == TAG_TEN) {
      float out[3] = {0};
      CPU_BACKEND.buf_read(TENS[(u32)term_val(r)].buf_id, out, sizeof out);
      for (u32 i = 0; i < 3; i++) {
        CHECK(out[i] == 2.0f * in[i]);
      }
    }
    // One kernel, emitted by the materialize; the APP only fires it.
    CHECK_EQ(KERNELS_NEXT - k0, 1u);
  }

  thvm_free();
  TEST_REPORT();
}
