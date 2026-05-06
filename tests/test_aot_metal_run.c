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

  // Iter JJ: dtype preservation regression test.  Inputs carry
  // DT_INT32 (ext=5); iter HH made the kernel preserve LHS dtype
  // on result, so the output should be NUM(7, ext=DT_INT32) -- not
  // NUM(7, ext=DT_BOOL) like before the fix.
  TEST_BEGIN("dtype preservation: NUM(i32, i32) -> NUM(i32)");
  Term i32_args[2] = {
    term_new(0, TAG_NUM, DT_INT32, 3),
    term_new(0, TAG_NUM, DT_INT32, 4),
  };
  result = thvm_aot_metal_compile_and_run(
      "add2", def_id, i32_args, 2,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)7);
  CHECK_EQ(term_ext(result), (u64)DT_INT32);   // dtype preserved

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

  // Iter G: REF inlining.  Build double_add(x, y) = add2(x, y) * 2,
  // where the inner App(App(REF[add2], x), y) is a cross-def call.
  // The Metal emitter inlines add2's body at the call site.
  TEST_BEGIN("REF inlining: double_add(3,4) -> 14 on GPU");
  // double_add: TLam[x, TLam[y, TOp2(MUL, App(App(REF[add2], x), y), NUM(2))]]
  // The first def_id assigned by def_register was 0 for add2 -- we need its id.
  // Look it up by walking DEFS.
  u32 add2_id = 0;  // matches build_add2 / def_register("add2", ...) call earlier
  Term ref_add2     = term_new(0, TAG_REF, add2_id, 0);
  u64  app_inner_loc = book_alloc(2);   // [REF[add2], TVar(x)]
  u64  app_outer_loc = book_alloc(2);   // [App(REF, x), TVar(y)]
  u64  op2_dd_loc    = book_alloc(2);   // [App(App(REF,x),y), NUM(2)]
  u64  dd_lam_y_loc  = book_alloc(1);
  u64  dd_lam_x_loc  = book_alloc(1);
  book_set(app_inner_loc + 0, ref_add2);
  book_set(app_inner_loc + 1, term_new(0, TAG_VAR, 0, dd_lam_x_loc));
  Term app_inner = term_new(0, TAG_APP, 0, app_inner_loc);
  book_set(app_outer_loc + 0, app_inner);
  book_set(app_outer_loc + 1, term_new(0, TAG_VAR, 0, dd_lam_y_loc));
  Term app_outer = term_new(0, TAG_APP, 0, app_outer_loc);
  book_set(op2_dd_loc + 0, app_outer);
  book_set(op2_dd_loc + 1, term_new(0, TAG_NUM, 0, 2));
  Term op2_dd = term_new(0, TAG_OP2, OP_MUL, op2_dd_loc);
  book_set(dd_lam_y_loc, op2_dd);
  Term dd_lam_y = term_new(0, TAG_LAM, 0, dd_lam_y_loc);
  book_set(dd_lam_x_loc, dd_lam_y);
  Term double_add = term_new(0, TAG_LAM, 0, dd_lam_x_loc);
  u32 def_id4 = def_register("double_add", double_add);

  Term args_dd[2] = {
    term_new(0, TAG_NUM, 0, 3),
    term_new(0, TAG_NUM, 0, 4),
  };
  result = thvm_aot_metal_compile_and_run(
      "double_add", def_id4, args_dd, 2,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)14);  // (3+4) * 2

  // Recursion: a self-referential def -- emit must fail (return 0).
  TEST_BEGIN("REF self-recursion is refused (compile_and_run -> 0)");
  // recur(x) = recur(x)  -- never terminates, but emit alone should bail.
  u64  rec_app_loc = book_alloc(2);
  u64  rec_lam_loc = book_alloc(1);
  // Reserve def_id5 first so the REF can target it.
  u32  def_id5 = (u32)-1;
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] == 0) { def_id5 = i; break; }
  }
  book_set(rec_app_loc + 0, term_new(0, TAG_REF, def_id5, 0));
  book_set(rec_app_loc + 1, term_new(0, TAG_VAR, 0, rec_lam_loc));
  Term rec_app = term_new(0, TAG_APP, 0, rec_app_loc);
  book_set(rec_lam_loc, rec_app);
  Term recur = term_new(0, TAG_LAM, 0, rec_lam_loc);
  DEFS[def_id5] = recur;

  Term args_r[1] = { term_new(0, TAG_NUM, 0, 0) };
  result = thvm_aot_metal_compile_and_run(
      "recur", def_id5, args_r, 1,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  // 0 = compile_and_run failure sentinel (emit returned NULL).
  CHECK_EQ(result, (Term)0);

  // Iter H: CTR construction.  wrap(x) = TCtr[1, x] -- a single-child
  // CTR cell built on the GPU via aot_book_alloc + heap_set.
  TEST_BEGIN("CTR build: wrap(x) = TCtr[1, x] returns TAG_CTR");
  // Body shape: TCtr label=1 with 1 child = TVar(wrap_x_loc).
  // Heap: ctr_loc = [NUM(1), TVar(wrap_x_loc)]; root = TAG_CTR.
  // Build the def: TLam[x, TCtr[1, x]].
  u64 wrap_x_loc = book_alloc(1);
  u64 wrap_ctr_loc = book_alloc(2);
  book_set(wrap_ctr_loc + 0, term_new(0, TAG_NUM, DT_INT32, 1));
  book_set(wrap_ctr_loc + 1, term_new(0, TAG_VAR, 0, wrap_x_loc));
  Term wrap_ctr = term_new(0, TAG_CTR, /*label=*/1, wrap_ctr_loc);
  book_set(wrap_x_loc, wrap_ctr);
  Term wrap = term_new(0, TAG_LAM, 0, wrap_x_loc);
  u32  def_id6 = (u32)-1;
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] == 0) { def_id6 = i; break; }
  }
  DEFS[def_id6] = wrap;

  Term args_w[1] = { term_new(0, TAG_NUM, 0, 42) };
  u64 next_before_wrap = book_next_state;
  result = thvm_aot_metal_compile_and_run(
      "wrap", def_id6, args_w, 1,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_CTR);
  CHECK_EQ(term_ext(result), (u64)1);  // label
  // GPU allocated 2 cells (1 + 1 child).
  CHECK_EQ(book_next_state, next_before_wrap + 2);
  u64 r_loc = term_val(result);
  Term n_cell = BOOK_HEAP[r_loc + 0];
  Term c0     = BOOK_HEAP[r_loc + 1];
  CHECK_EQ(term_tag(n_cell), (u64)TAG_NUM);
  CHECK_EQ(term_val(n_cell), (u64)1);     // arity
  CHECK_EQ(term_tag(c0), (u64)TAG_NUM);
  CHECK_EQ(term_val(c0), (u64)42);         // the bound x

  // Pair builder: pair(x, y) = TCtr[2, x, y] -- arity 2.
  TEST_BEGIN("CTR build arity 2: pair(x, y) = TCtr[2, x, y]");
  u64 pair_x_loc = book_alloc(1);
  u64 pair_y_loc = book_alloc(1);
  u64 pair_ctr_loc = book_alloc(3);
  book_set(pair_ctr_loc + 0, term_new(0, TAG_NUM, DT_INT32, 2));
  book_set(pair_ctr_loc + 1, term_new(0, TAG_VAR, 0, pair_x_loc));
  book_set(pair_ctr_loc + 2, term_new(0, TAG_VAR, 0, pair_y_loc));
  Term pair_ctr = term_new(0, TAG_CTR, /*label=*/2, pair_ctr_loc);
  book_set(pair_y_loc, pair_ctr);
  Term pair_lam_y = term_new(0, TAG_LAM, 0, pair_y_loc);
  book_set(pair_x_loc, pair_lam_y);
  Term pair = term_new(0, TAG_LAM, 0, pair_x_loc);
  u32  def_id7 = (u32)-1;
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] == 0) { def_id7 = i; break; }
  }
  DEFS[def_id7] = pair;

  Term args_p[2] = {
    term_new(0, TAG_NUM, 0, 100),
    term_new(0, TAG_NUM, 0, 200),
  };
  u64 next_before_pair = book_next_state;
  result = thvm_aot_metal_compile_and_run(
      "pair", def_id7, args_p, 2,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_CTR);
  CHECK_EQ(term_ext(result), (u64)2);
  CHECK_EQ(book_next_state, next_before_pair + 3);
  u64 p_loc = term_val(result);
  Term p_n = BOOK_HEAP[p_loc + 0];
  Term p_a = BOOK_HEAP[p_loc + 1];
  Term p_b = BOOK_HEAP[p_loc + 2];
  CHECK_EQ(term_val(p_n), (u64)2);
  CHECK_EQ(term_val(p_a), (u64)100);
  CHECK_EQ(term_val(p_b), (u64)200);

  // Iter K: multi-arg MAT-chain.  select(idx, a, b) =
  //   match idx { 0 -> a; 1 -> b; _ -> 0 }
  // Body shape: App(MAT[0, [TVar(a), MAT[1, [TVar(b), TNum(0)]]]],
  //                   TVar(idx)).
  TEST_BEGIN("multi-arg MAT-chain: select(idx, a, b)");
  u64 sel_idx_loc = book_alloc(1);
  u64 sel_a_loc   = book_alloc(1);
  u64 sel_b_loc   = book_alloc(1);
  u64 mat1_kc = book_alloc(2);
  book_set(mat1_kc + 0, term_new(0, TAG_VAR, 0, sel_b_loc));   // handler 1 -> b
  book_set(mat1_kc + 1, term_new(0, TAG_NUM, 0, 0));            // default -> 0
  Term mat1_kk = term_new(0, TAG_MAT, 1, mat1_kc);
  u64 mat0_kc = book_alloc(2);
  book_set(mat0_kc + 0, term_new(0, TAG_VAR, 0, sel_a_loc));   // handler 0 -> a
  book_set(mat0_kc + 1, mat1_kk);                              // fallback -> mat1
  Term mat0_kk = term_new(0, TAG_MAT, 0, mat0_kc);
  u64 sel_app_loc = book_alloc(2);
  book_set(sel_app_loc + 0, mat0_kk);
  book_set(sel_app_loc + 1, term_new(0, TAG_VAR, 0, sel_idx_loc));
  Term sel_app = term_new(0, TAG_APP, 0, sel_app_loc);
  // Body wraps in 3 LAMs (idx, a, b).
  book_set(sel_b_loc, sel_app);
  Term sel_lam_b = term_new(0, TAG_LAM, 0, sel_b_loc);
  book_set(sel_a_loc, sel_lam_b);
  Term sel_lam_a = term_new(0, TAG_LAM, 0, sel_a_loc);
  book_set(sel_idx_loc, sel_lam_a);
  Term sel = term_new(0, TAG_LAM, 0, sel_idx_loc);
  u32 def_id8 = (u32)-1;
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] == 0) { def_id8 = i; break; }
  }
  DEFS[def_id8] = sel;

  // select(0, 11, 22) -> 11
  Term sa1[3] = {
    term_new(0, TAG_NUM, 0, 0),
    term_new(0, TAG_NUM, 0, 11),
    term_new(0, TAG_NUM, 0, 22),
  };
  result = thvm_aot_metal_compile_and_run(
      "select", def_id8, sa1, 3,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)11);

  // select(1, 11, 22) -> 22
  Term sa2[3] = {
    term_new(0, TAG_NUM, 0, 1),
    term_new(0, TAG_NUM, 0, 11),
    term_new(0, TAG_NUM, 0, 22),
  };
  result = thvm_aot_metal_compile_and_run(
      "select", def_id8, sa2, 3,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)22);

  // select(99, 11, 22) -> 0 (default arm)
  Term sa3[3] = {
    term_new(0, TAG_NUM, 0, 99),
    term_new(0, TAG_NUM, 0, 11),
    term_new(0, TAG_NUM, 0, 22),
  };
  result = thvm_aot_metal_compile_and_run(
      "select", def_id8, sa3, 3,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)0);

  // Iter L: CTR destructure in MAT arms.  pair_sum(p) =
  //   match p { #2 x y -> x + y; _ -> 0 }
  // Body: App(MAT[2, [Lam[x, Lam[y, x+y]], NUM(0)]], TVar(p)).
  TEST_BEGIN("CTR destructure in MAT: pair_sum(#2 x y) = x + y");
  // Build the TLam[x, TLam[y, OP2(ADD, x, y)]] handler.
  u64 ps_x_loc = book_alloc(1);
  u64 ps_y_loc = book_alloc(1);
  u64 ps_op_loc = book_alloc(2);
  book_set(ps_op_loc + 0, term_new(0, TAG_VAR, 0, ps_x_loc));
  book_set(ps_op_loc + 1, term_new(0, TAG_VAR, 0, ps_y_loc));
  Term ps_op = term_new(0, TAG_OP2, OP_ADD, ps_op_loc);
  book_set(ps_y_loc, ps_op);
  Term ps_lam_y = term_new(0, TAG_LAM, 0, ps_y_loc);
  book_set(ps_x_loc, ps_lam_y);
  Term ps_handler = term_new(0, TAG_LAM, 0, ps_x_loc);

  // Build the MAT chain: MAT[2, [ps_handler, NUM(0)]]
  u64 ps_mat_loc = book_alloc(2);
  book_set(ps_mat_loc + 0, ps_handler);
  book_set(ps_mat_loc + 1, term_new(0, TAG_NUM, 0, 0));
  Term ps_mat = term_new(0, TAG_MAT, 2, ps_mat_loc);

  u64 ps_p_loc = book_alloc(1);
  u64 ps_app_loc = book_alloc(2);
  book_set(ps_app_loc + 0, ps_mat);
  book_set(ps_app_loc + 1, term_new(0, TAG_VAR, 0, ps_p_loc));
  Term ps_app = term_new(0, TAG_APP, 0, ps_app_loc);
  book_set(ps_p_loc, ps_app);
  Term pair_sum = term_new(0, TAG_LAM, 0, ps_p_loc);
  u32 def_id9 = (u32)-1;
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] == 0) { def_id9 = i; break; }
  }
  DEFS[def_id9] = pair_sum;

  // Build a CTR{label=2, [NUM(7), NUM(35)]} on the heap and pass as input.
  u64 in_ctr_loc = book_alloc(3);
  book_set(in_ctr_loc + 0, term_new(0, TAG_NUM, DT_INT32, 2));
  book_set(in_ctr_loc + 1, term_new(0, TAG_NUM, 0, 7));
  book_set(in_ctr_loc + 2, term_new(0, TAG_NUM, 0, 35));
  Term in_ctr = term_new(0, TAG_CTR, 2, in_ctr_loc);

  Term ps_args[1] = { in_ctr };
  result = thvm_aot_metal_compile_and_run(
      "pair_sum", def_id9, ps_args, 1,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)42);   // 7 + 35

  // Default arm fires when label doesn't match.  Build CTR{99, [NUM(1)]}.
  u64 in_ctr2_loc = book_alloc(2);
  book_set(in_ctr2_loc + 0, term_new(0, TAG_NUM, DT_INT32, 1));
  book_set(in_ctr2_loc + 1, term_new(0, TAG_NUM, 0, 1));
  Term in_ctr2 = term_new(0, TAG_CTR, 99, in_ctr2_loc);

  Term ps_args2[1] = { in_ctr2 };
  result = thvm_aot_metal_compile_and_run(
      "pair_sum", def_id9, ps_args2, 1,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)0);

  // Iter V: DUP support.  square(x) = x * x -- after auto_dup, the
  // body uses DP0/DP1 of a shared dup cell whose body is the
  // original TVar(x).  The Metal emit memos dup_loc -> uint var so
  // both projections reuse one computation.
  TEST_BEGIN("DUP: square(x) = x * x via DP0/DP1");
  u64 sq_x_loc   = book_alloc(1);
  u64 sq_dup_loc = book_alloc(1);
  book_set(sq_dup_loc, term_new(0, TAG_VAR, 0, sq_x_loc));   // dup body = TVar(x)
  u64 sq_op_loc  = book_alloc(2);
  book_set(sq_op_loc + 0, term_new(0, TAG_DP0, 0, sq_dup_loc));
  book_set(sq_op_loc + 1, term_new(0, TAG_DP1, 0, sq_dup_loc));
  Term sq_op = term_new(0, TAG_OP2, OP_MUL, sq_op_loc);
  book_set(sq_x_loc, sq_op);
  Term square = term_new(0, TAG_LAM, 0, sq_x_loc);
  u32 def_id_sq = (u32)-1;
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] == 0) { def_id_sq = i; break; }
  }
  DEFS[def_id_sq] = square;

  Term sq_args[1] = { term_new(0, TAG_NUM, 0, 5) };
  result = thvm_aot_metal_compile_and_run(
      "square", def_id_sq, sq_args, 1,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)25);

  // square(13) = 169
  Term sq_args2[1] = { term_new(0, TAG_NUM, 0, 13) };
  result = thvm_aot_metal_compile_and_run(
      "square", def_id_sq, sq_args2, 1,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)169);

  // Iter Z: hot-path microbench.  After PSO + metallib disk caches
  // are warm, a single add2(a,b) Metal dispatch should be on the
  // order of a few hundred microseconds (kernel launch + readback
  // dominate; the actual fold is ~5 ns).  CHECK ensures we don't
  // silently regress to seconds (which would mean the cache broke
  // and we're recompiling per call).
  TEST_BEGIN("Metal hot-path: 100 add2 calls under 200 ms");
  Term bargs[2] = {
    term_new(0, TAG_NUM, 0, 7),
    term_new(0, TAG_NUM, 0, 11),
  };
  // Warm-up (already JIT-compiled earlier in this test, but make
  // sure the PSO is in cache for the timed loop).
  (void)thvm_aot_metal_compile_and_run(
      "add2", 0, bargs, 2,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  u64 t0 = cg_now_us();
  for (u32 i = 0; i < 100; i++) {
    Term r = thvm_aot_metal_compile_and_run(
        "add2", 0, bargs, 2,
        BOOK_HEAP, BOOK_CAP, &book_next_state);
    (void)r;
  }
  u64 dt = cg_now_us() - t0;
  fprintf(stderr, "  bench: 100 add2 Metal calls in %llu us"
          " (avg %llu us/call)\n",
          (unsigned long long)dt, (unsigned long long)(dt / 100));
  CHECK(dt < 200000);   // 200 ms cap; typical run is ~50 ms total

  // Iter GG: CPU baseline -- same OP2(NUM, NUM) fold via wnf, no
  // kernel launch.  Demonstrates the GPU-dispatch overhead vs the
  // intrinsic cost of the actual computation.  No CHECK, just
  // informational; numbers shift across machines.
  TEST_BEGIN("CPU baseline: 100 OP2(NUM,NUM) folds via wnf");
  u64 t1 = cg_now_us();
  for (u32 i = 0; i < 100; i++) {
    u64 op_loc = heap_alloc(2);
    heap_set(op_loc + 0, term_new(0, TAG_NUM, 0, 7));
    heap_set(op_loc + 1, term_new(0, TAG_NUM, 0, 11));
    Term op = term_new(0, TAG_OP2, OP_ADD, op_loc);
    Term r  = wnf(op);
    (void)r;
  }
  u64 dt_cpu = cg_now_us() - t1;
  fprintf(stderr,
    "  bench: 100 OP2 folds via wnf in %llu us (avg %llu us/call); "
    "Metal/CPU ratio ~%llux\n",
    (unsigned long long)dt_cpu,
    (unsigned long long)(dt_cpu / 100),
    (unsigned long long)(dt_cpu == 0 ? 0 : dt / dt_cpu));

  thvm_free();
  TEST_REPORT();
}
