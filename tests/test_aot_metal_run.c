// test_aot_metal_run.c -- Phase 7 iter D: TAOTRun-equivalent on Metal.
//
// End-to-end demo of the goal pipeline:
//   1. TDef("add2", TLam(a, TLam(b, TOp2(ADD, a, b))))
//   2. thvm_aot_metal_compile_and_run("add2", def_id, [3, 4], 2)
//   3. Verify result == NUM(7), all the work done on GPU.
//
// Coverage: TLam-peel + TVar + TNum + TOp2.  No MAT, REF, CTR, DUP yet
// (later iters).

#include "../src/thvm.c"
#include "test.h"

extern Term thvm_aot_metal_compile_and_run(
    const char *name, u32 def_id,
    Term *args, u32 n_args,
    Term *book_heap, u64 book_cells,
    u64 *book_next_inout);

static u32 def_register(const char *name, Term body) {
  // Allocate a slot in DEFS, copy body into book heap, return slot id.
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] == 0) {
      DEFS[i] = body;
      // Park name -> slot via NAME_TO_DEF_ID if available; not needed
      // for this test since we pass name directly to the emit/run path.
      (void)name;
      return i;
    }
  }
  return (u32)-1;
}

// Build TLam(x, TLam(y, TOp2(ADD, x, y))) in book_heap.
static Term build_add2(void) {
  // Outer lam: lam_a_loc points at body  (= inner lam term)
  // Inner lam: lam_b_loc points at body  (= TOp2 term)
  // TOp2: op_loc points at [TVar(lam_a_loc), TVar(lam_b_loc)]
  u64 op_loc = book_alloc(2);
  u64 lam_b_loc = book_alloc(1);
  u64 lam_a_loc = book_alloc(1);
  book_set(op_loc + 0, term_new(0, TAG_VAR, 0, lam_a_loc));
  book_set(op_loc + 1, term_new(0, TAG_VAR, 0, lam_b_loc));
  Term op2 = term_new(0, TAG_OP2, OP_ADD, op_loc);
  book_set(lam_b_loc, op2);
  Term inner_lam = term_new(0, TAG_LAM, 0, lam_b_loc);
  book_set(lam_a_loc, inner_lam);
  return term_new(0, TAG_LAM, 0, lam_a_loc);
}

int main(void) {
  thvm_init();

  Term add2 = build_add2();
  u32  def_id = def_register("add2", add2);

  TEST_BEGIN("add2(3,4) on GPU returns NUM(7)");
  Term args[2] = {
    term_new(0, TAG_NUM, 0, 3),
    term_new(0, TAG_NUM, 0, 4),
  };
  u64 book_next_state = BOOK_NEXT;
  Term result = thvm_aot_metal_compile_and_run(
      "add2", def_id, args, 2,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)7);

  TEST_BEGIN("cache hit: second add2 call uses cached PSO");
  Term args2[2] = {
    term_new(0, TAG_NUM, 0, 100),
    term_new(0, TAG_NUM, 0, 200),
  };
  result = thvm_aot_metal_compile_and_run(
      "add2", def_id, args2, 2,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)300);

  // Compile + run a different def shape: nested OP2.
  // mul_then_add(a, b, c) = (a * b) + c
  TEST_BEGIN("(a*b)+c on GPU returns 5*6+7 = 37");
  u64 inner_op_loc = book_alloc(2);   // [TVar(a), TVar(b)]
  u64 outer_op_loc = book_alloc(2);   // [TOp2(MUL, a, b), TVar(c)]
  u64 lam_c_loc = book_alloc(1);
  u64 lam_b_loc = book_alloc(1);
  u64 lam_a_loc = book_alloc(1);
  book_set(inner_op_loc + 0, term_new(0, TAG_VAR, 0, lam_a_loc));
  book_set(inner_op_loc + 1, term_new(0, TAG_VAR, 0, lam_b_loc));
  Term inner_op = term_new(0, TAG_OP2, OP_MUL, inner_op_loc);
  book_set(outer_op_loc + 0, inner_op);
  book_set(outer_op_loc + 1, term_new(0, TAG_VAR, 0, lam_c_loc));
  Term outer_op = term_new(0, TAG_OP2, OP_ADD, outer_op_loc);
  book_set(lam_c_loc, outer_op);
  Term lam_c = term_new(0, TAG_LAM, 0, lam_c_loc);
  book_set(lam_b_loc, lam_c);
  Term lam_b = term_new(0, TAG_LAM, 0, lam_b_loc);
  book_set(lam_a_loc, lam_b);
  Term mul_add = term_new(0, TAG_LAM, 0, lam_a_loc);
  u32  def_id2 = def_register("mul_add", mul_add);

  Term args3[3] = {
    term_new(0, TAG_NUM, 0, 5),
    term_new(0, TAG_NUM, 0, 6),
    term_new(0, TAG_NUM, 0, 7),
  };
  result = thvm_aot_metal_compile_and_run(
      "mul_add", def_id2, args3, 3,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)37);

  // Iter F: MAT-chain on Metal.
  // classify(n) = match n { 0 -> 42; 1 -> 99; 2 -> 7; default -> 0 }
  // Build manually: TLam[n, App(MAT[0, [42, MAT[1, [99, MAT[2, [7, 0]]]]]], n)]
  TEST_BEGIN("MAT-chain on GPU: classify(n)");
  u64 mat2_loc = book_alloc(2);
  book_set(mat2_loc + 0, term_new(0, TAG_NUM, 0, 7));   // handler for 2
  book_set(mat2_loc + 1, term_new(0, TAG_NUM, 0, 0));   // default
  Term mat2 = term_new(0, TAG_MAT, 2, mat2_loc);
  u64 mat1_loc = book_alloc(2);
  book_set(mat1_loc + 0, term_new(0, TAG_NUM, 0, 99));  // handler for 1
  book_set(mat1_loc + 1, mat2);                          // fallback = mat2
  Term mat1 = term_new(0, TAG_MAT, 1, mat1_loc);
  u64 mat0_loc = book_alloc(2);
  book_set(mat0_loc + 0, term_new(0, TAG_NUM, 0, 42));  // handler for 0
  book_set(mat0_loc + 1, mat1);                          // fallback = mat1
  Term mat0 = term_new(0, TAG_MAT, 0, mat0_loc);

  u64 lam_n_loc = book_alloc(1);
  u64 app_loc   = book_alloc(2);
  book_set(app_loc + 0, mat0);
  book_set(app_loc + 1, term_new(0, TAG_VAR, 0, lam_n_loc));
  Term app = term_new(0, TAG_APP, 0, app_loc);
  book_set(lam_n_loc, app);
  Term classify = term_new(0, TAG_LAM, 0, lam_n_loc);
  u32  def_id3 = def_register("classify", classify);

  static const u32 inputs[] = { 0, 1, 2, 7 };
  static const u32 expected[] = { 42, 99, 7, 0 };
  for (u32 i = 0; i < 4; i++) {
    Term args1[1] = { term_new(0, TAG_NUM, 0, inputs[i]) };
    result = thvm_aot_metal_compile_and_run(
        "classify", def_id3, args1, 1,
        BOOK_HEAP, BOOK_CAP, &book_next_state);
    CHECK_EQ(term_tag(result), (u64)TAG_NUM);
    CHECK_EQ(term_val(result), (u64)expected[i]);
  }

  thvm_free();
  TEST_REPORT();
}
