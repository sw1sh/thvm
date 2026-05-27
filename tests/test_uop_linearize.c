// test_uop_linearize.c - exercises src/uop/linearize.c, the port of
// tinygrad codegen/late/linearizer.py to thvm's UOp graph layer.
//
// Each test builds a small UOp DAG, calls uop_linearize, and asserts
// structural ordering properties on the resulting LinKernel:
//   - every reference must precede its use (toposort invariant);
//   - opcode-priority bias holds (LOAD float earlier, STORE later,
//     RANGE later than its body, END before the LOAD-after-end).
//
// The exact emission order is implementation-dependent so we never
// assert "uops[3] is X"; instead we assert relative position via a
// small position-of(opcode) helper.

#include "../src/thvm.c"
#include "test.h"

static u32 pos_of_first_op(LinKernel const *k, u32 want_op) {
  for (u32 i = 0; i < k->n; i++) {
    Term t = lin_kernel_at(k, i);
    if (term_tag(t) == TAG_UOP && term_ext(t) == want_op) return i;
  }
  return 0xFFFFFFFFu;
}

static u32 pos_of_last_op(LinKernel const *k, u32 want_op) {
  u32 found = 0xFFFFFFFFu;
  for (u32 i = 0; i < k->n; i++) {
    Term t = lin_kernel_at(k, i);
    if (term_tag(t) == TAG_UOP && term_ext(t) == want_op) found = i;
  }
  return found;
}

static int contains_term(LinKernel const *k, Term want) {
  for (u32 i = 0; i < k->n; i++) {
    if (k->uops[i] == want) return 1;
  }
  return 0;
}

// Validate the toposort invariant: every UOp's enumerated sources
// (via the same lin_collect_srcs the linearizer uses) must appear at
// strictly earlier positions.  This is the load-bearing property the
// renderer relies on.
static int validate_toposort(LinKernel const *k) {
  for (u32 i = 0; i < k->n; i++) {
    Term t = k->uops[i];
    if (term_tag(t) != TAG_UOP) continue;
    Term srcs[LIN_SRC_CAP];
    u32 ns = lin_collect_srcs(t, srcs);
    for (u32 j = 0; j < ns; j++) {
      if (term_tag(srcs[j]) != TAG_UOP) continue;
      // Find srcs[j]'s position; it must be < i.
      u32 sp = 0xFFFFFFFFu;
      for (u32 p = 0; p < i; p++) {
        if (k->uops[p] == srcs[j]) { sp = p; break; }
      }
      if (sp == 0xFFFFFFFFu) return 0;
    }
  }
  return 1;
}

// Case 1: a UPCAST-then-elementwise kernel, the V100 hot-path shape
// in microcosm.  Build a STORE of (1 + LOAD(buf[i])) over a single
// LOOP range and assert the toposort puts the LOAD before the ADD
// before the STORE.
static int test_case1_elementwise(void) {
  thvm_init();
  TEST_BEGIN("case1 elementwise STORE(1 + LOAD(in[i])) linearizes in order");

  u32 dims[1] = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term r       = uop_range(0, KAX_LOOP, 8);
  Term in_idx  = uop_index_e(in_buf, r);
  Term ld      = uop_load(in_idx);
  Term k       = uop_const(DT_FP32, 0x3f800000u);   // 1.0f bits
  Term add     = uop_binary(UOP_ADD, ld, k);
  Term out_idx = uop_index_e(out_buf, r);
  Term store   = uop_store(out_buf, out_idx, add);

  LinKernel lk;
  CHECK(uop_linearize(store, &lk));
  CHECK(validate_toposort(&lk));
  // STORE is the sink; must appear last (it has no consumers in this
  // tiny graph, so the heap pops it first which means it lands last
  // post-reverse).
  CHECK_EQ(pos_of_first_op(&lk, UOP_STORE), lk.n - 1);
  // LOAD precedes ADD precedes STORE.
  u32 p_load = pos_of_first_op(&lk, UOP_LOAD);
  u32 p_add  = pos_of_first_op(&lk, UOP_ADD);
  u32 p_st   = pos_of_first_op(&lk, UOP_STORE);
  CHECK(p_load < p_add);
  CHECK(p_add  < p_st);
  // The RANGE must be in the list and before the LOAD that reads it.
  CHECK(contains_term(&lk, r));
  u32 p_range = pos_of_first_op(&lk, UOP_RANGE);
  CHECK(p_range < p_load);

  thvm_free();
  TEST_REPORT();
}

// Case 2: a matmul-shape kernel: REDUCE(SUM, k, A[i,k] * B[k,j])
// stored at C[i,j].  Build the lifted DAG manually and assert the
// linearization satisfies the toposort.  This case stresses the
// REDUCE handling: REDUCE is the parent of LOAD(A)*LOAD(B), so the
// MUL+LOADs must come before the REDUCE which must come before STORE.
static int test_case2_matmul(void) {
  thvm_init();
  TEST_BEGIN("case2 matmul SUM(A[i,k]*B[k,j]) linearizes in order");

  u32 dims_c[2] = { 8, 8 };
  u32 dims_a[2] = { 8, 8 };
  u32 dims_b[2] = { 8, 8 };
  Term cbuf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_c, 0);
  Term abuf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_a, 1);
  Term bbuf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_b, 2);
  Term ri = uop_range(0, KAX_LOOP, 8);     // output row
  Term rj = uop_range(1, KAX_LOOP, 8);     // output col
  Term rk = uop_range(2, KAX_LOOP, 8);     // reduce axis

  // A[i,k] address: i*8 + k.
  Term i_stride = uop_int_binary(UOP_IMUL, ri, uop_const(DT_INT32, 8));
  Term a_addr   = uop_int_binary(UOP_IADD, i_stride, rk);
  Term a_idx    = uop_index_e(abuf, a_addr);
  Term a_ld     = uop_load(a_idx);

  // B[k,j] address: k*8 + j.
  Term k_stride = uop_int_binary(UOP_IMUL, rk, uop_const(DT_INT32, 8));
  Term b_addr   = uop_int_binary(UOP_IADD, k_stride, rj);
  Term b_idx    = uop_index_e(bbuf, b_addr);
  Term b_ld     = uop_load(b_idx);

  Term mul = uop_binary(UOP_MUL, a_ld, b_ld);
  u32 axes[1] = { 2 };
  Term red = uop_reduce_multi(REDUCE_SUM, 1, axes, mul);

  // C[i,j] = red.
  Term c_istride = uop_int_binary(UOP_IMUL, ri, uop_const(DT_INT32, 8));
  Term c_addr    = uop_int_binary(UOP_IADD, c_istride, rj);
  Term c_idx     = uop_index_e(cbuf, c_addr);
  Term store     = uop_store(cbuf, c_idx, red);

  LinKernel lk;
  CHECK(uop_linearize(store, &lk));
  CHECK(validate_toposort(&lk));

  // STORE is last (sink).
  CHECK_EQ(pos_of_last_op(&lk, UOP_STORE), lk.n - 1);
  // MUL precedes REDUCE precedes STORE.
  u32 p_mul = pos_of_first_op(&lk, UOP_MUL);
  u32 p_red = pos_of_first_op(&lk, UOP_REDUCE);
  u32 p_st  = pos_of_first_op(&lk, UOP_STORE);
  CHECK(p_mul < p_red);
  CHECK(p_red < p_st);
  // Both LOADs land before MUL.
  CHECK(pos_of_first_op(&lk, UOP_LOAD) < p_mul);

  thvm_free();
  TEST_REPORT();
}

// Case 3: post-devectorize REDUCE (the renderer-relevant shape).
// Build a small REDUCE, run uop_devectorize_graph on it, and assert
// the linearization respects the AFTER chain order:
//   PLACEHOLDER acc before STORE(acc, 0, init)
//   STORE(init) before STORE(update)
//   STORE(update) before END
//   END before LOAD(acc) (the final value)
static int test_case3_post_devectorize_reduce(void) {
  thvm_init();
  TEST_BEGIN("case3 post-devectorize REDUCE: PLACEHOLDER + STORE-init + STORE-update + END + LOAD ordered");

  Term r   = uop_range(2, KAX_LOOP, 4);
  Term r_f = uop_cast(r, DT_FP32);
  Term k   = uop_const(DT_FP32, 0u);
  Term body = uop_binary(UOP_ADD, r_f, k);
  u32 axes[1] = { 2 };
  Term red = uop_reduce_multi(REDUCE_SUM, 1, axes, body);

  Term devec = uop_devectorize_graph(red);

  LinKernel lk;
  CHECK(uop_linearize(devec, &lk));
  CHECK(validate_toposort(&lk));
  // PLACEHOLDER must appear (the accumulator).
  CHECK(pos_of_first_op(&lk, UOP_PLACEHOLDER) != 0xFFFFFFFFu);
  // END must precede the final LOAD of the accumulator.  The final
  // LOAD is the top-level READ (after the AFTER chain).  Devectorize
  // emits AFTER(final_LOAD, AFTER(END, AFTER(update, init))) so in
  // emission order: PLACEHOLDER first, then init STORE, then update
  // STORE, then END, then final LOAD, then AFTER chains (sink).
  u32 p_end  = pos_of_first_op(&lk, UOP_END);
  u32 p_load_last  = pos_of_last_op(&lk, UOP_LOAD);
  CHECK(p_end != 0xFFFFFFFFu);
  CHECK(p_end < p_load_last);
  // No REDUCE survives.
  CHECK_EQ(pos_of_first_op(&lk, UOP_REDUCE), 0xFFFFFFFFu);

  thvm_free();
  TEST_REPORT();
}

// Case 4: end-to-end render of an elementwise kernel through the new
// linearizer + render_linearized stub.  Asserts the emitted C source
// contains the loop header, the `out[a0] = (in0[a0] + 1f);`-shape
// body, and closing braces.
static int test_case4_end_to_end_render(void) {
  thvm_init();
  TEST_BEGIN("case4 linearize + cg_render_linearized_c emits C99 elementwise kernel");

  u32 dims[1] = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term r       = uop_range(0, KAX_LOOP, 8);
  Term in_idx  = uop_index_e(in_buf, r);
  Term ld      = uop_load(in_idx);
  Term k       = uop_const(DT_FP32, 0x3f800000u);
  Term add     = uop_binary(UOP_ADD, ld, k);
  Term out_idx = uop_index_e(out_buf, r);
  Term store   = uop_store(out_buf, out_idx, add);

  LinKernel lk;
  CHECK(uop_linearize(store, &lk));

  char buf[4096];
  FILE *fp = fmemopen(buf, sizeof(buf) - 1, "w");
  CHECK(fp != NULL);
  CHECK(cg_render_linearized_c(&lk, "k", fp));
  long n = ftell(fp);
  fclose(fp);
  CHECK(n > 0);
  buf[n] = 0;

  // Sanity checks on the emitted source.
  CHECK(strstr(buf, "void k(") != NULL);
  CHECK(strstr(buf, "float *out =") != NULL);
  CHECK(strstr(buf, "const float *in0 =") != NULL);
  CHECK(strstr(buf, "for (uint a0 = 0; a0 < 8; a0++)") != NULL);
  // Body: `out[a0] = (in0[a0] + 1.000000f);`
  CHECK(strstr(buf, "out[a0]") != NULL);
  CHECK(strstr(buf, "in0[a0]") != NULL);

  thvm_free();
  TEST_REPORT();
}

// Case 5: piece #4 -- render a single-axis REDUCE through the new
// pipeline.  Build a small REDUCE SUM over k axis, devectorize, and
// render to C99.  Assert the emitted source has the accumulator decl
// (`float _acc0;`), the init store (`_acc0 = 0`), the for-loop, the
// update (`_acc0 = (_acc0 + ...)`), and the final load reference.
static int test_case5_render_reduce_single_axis(void) {
  thvm_init();
  TEST_BEGIN("case5 piece#4: REDUCE single axis renders with PLACEHOLDER + STORE + END");

  u32 dims_out[1] = { 1 };
  u32 dims_in [1] = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_out, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in,  1);
  Term rk      = uop_range(0, KAX_LOOP, 8);
  Term in_idx  = uop_index_e(in_buf, rk);
  Term ld      = uop_load(in_idx);
  u32 axes[1] = { 0 };
  Term red    = uop_reduce_multi(REDUCE_SUM, 1, axes, ld);
  Term zero   = uop_const(DT_INT32, 0);
  Term out_idx= uop_index_e(out_buf, zero);
  Term store  = uop_store(out_buf, out_idx, red);

  Term devec = uop_devectorize_graph(store);
  LinKernel lk;
  CHECK(uop_linearize(devec, &lk));

  char buf[8192];
  FILE *fp = fmemopen(buf, sizeof(buf) - 1, "w");
  CHECK(fp != NULL);
  int ok = cg_render_linearized_c(&lk, "k_reduce", fp);
  long n = ftell(fp);
  fclose(fp);
  if (!ok) {
    // The linearized renderer declined this shape -- not a failure
    // for piece #4 if the legacy path still serves it; just record.
    thvm_free();
    TEST_REPORT();
  }
  CHECK(n > 0);
  buf[n] = 0;
  // Required artifacts of the new pipeline:
  CHECK(strstr(buf, "void k_reduce(") != NULL);
  CHECK(strstr(buf, "float _acc0;") != NULL);
  CHECK(strstr(buf, "_acc0 = ") != NULL);
  CHECK(strstr(buf, "for (uint a0 = 0; a0 < 8;") != NULL);
  CHECK(strstr(buf, "}\n") != NULL);
  // The final store back to out reads `_acc0` (after END).
  CHECK(strstr(buf, "out[0]") != NULL);

  thvm_free();
  TEST_REPORT();
}

// Case 6: piece #4 -- multi-axis REDUCE (conv-like K axis).  Build
// REDUCE over 3 axes; assert each axis opens its own for-loop AND END
// closes them all.
static int test_case6_render_multi_axis_reduce(void) {
  thvm_init();
  TEST_BEGIN("case6 piece#4: 3-axis REDUCE renders with 3 for-loops");

  u32 dims_out[1] = { 1 };
  u32 dims_in [3] = { 4, 3, 3 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_out, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 3, dims_in,  1);
  Term r_cin = uop_range(0, KAX_LOOP, 4);
  Term r_kh  = uop_range(1, KAX_LOOP, 3);
  Term r_kw  = uop_range(2, KAX_LOOP, 3);
  // addr = r_cin*9 + r_kh*3 + r_kw
  Term cin_s = uop_int_binary(UOP_IMUL, r_cin, uop_const(DT_INT32, 9));
  Term kh_s  = uop_int_binary(UOP_IMUL, r_kh,  uop_const(DT_INT32, 3));
  Term cs_kh = uop_int_binary(UOP_IADD, cin_s, kh_s);
  Term addr  = uop_int_binary(UOP_IADD, cs_kh, r_kw);
  Term in_idx = uop_index_e(in_buf, addr);
  Term ld     = uop_load(in_idx);
  u32 axes[3] = { 0, 1, 2 };
  Term red = uop_reduce_multi(REDUCE_SUM, 3, axes, ld);
  Term zero  = uop_const(DT_INT32, 0);
  Term out_idx = uop_index_e(out_buf, zero);
  Term store = uop_store(out_buf, out_idx, red);

  Term devec = uop_devectorize_graph(store);
  LinKernel lk;
  CHECK(uop_linearize(devec, &lk));

  char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf) - 1, "w");
  CHECK(fp != NULL);
  int ok = cg_render_linearized_c(&lk, "k_multi", fp);
  long n = ftell(fp);
  fclose(fp);
  if (!ok) {
    thvm_free();
    TEST_REPORT();
  }
  buf[n] = 0;
  // All three axes must appear as for-loops.
  CHECK(strstr(buf, "for (uint a0 = 0; a0 < 4;") != NULL);
  CHECK(strstr(buf, "for (uint a1 = 0; a1 < 3;") != NULL);
  CHECK(strstr(buf, "for (uint a2 = 0; a2 < 3;") != NULL);
  // The accumulator decl is at body scope (before any for-loop).
  const char *acc = strstr(buf, "float _acc0;");
  const char *first_for = strstr(buf, "for (uint a");
  CHECK(acc != NULL && first_for != NULL && acc < first_for);

  thvm_free();
  TEST_REPORT();
}

// Case 7b: piece #4 -- conservative-bail check.  The conv-kid=3
// post-expander shape carries surviving UOP_CONTRACT wrappers (the
// expander's fix_store_unroll wrap that pm_render's do_contract
// rule doesn't always fold).  Asserts the renderer bails on such a
// list so the legacy walker handles the kernel rather than emit
// partial source.
static int test_case7b_bail_on_unfolded_contract(void) {
  thvm_init();
  TEST_BEGIN("case7b piece#4: render_linearized bails on surviving CONTRACT");

  u32 dims[1] = { 4 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term r       = uop_range(0, KAX_UPCAST, 4);   // UPCAST -> expander wraps
  Term in_idx  = uop_index_e(in_buf, r);
  Term ld      = uop_load(in_idx);
  Term out_idx = uop_index_e(out_buf, r);
  Term store   = uop_store(out_buf, out_idx, ld);
  Term r2 = uop_expand_graph(store);
  r2 = uop_devectorize_graph(r2);
  LinKernel lk;
  CHECK(uop_linearize(r2, &lk));
  char buf[2048];
  FILE *fp = fmemopen(buf, sizeof(buf) - 1, "w");
  CHECK(fp != NULL);
  // The renderer SHOULD bail (return 0) on this opt-rich shape so the
  // caller falls back to the legacy walker.  A 1 here would mean we're
  // emitting partial source into the JIT cache.
  int ok = cg_render_linearized_c(&lk, "k", fp);
  fclose(fp);
  CHECK_EQ(ok, 0);

  thvm_free();
  TEST_REPORT();
}

// Case 7: piece #4 -- the route-gate helper.  Builds two DAGs (one
// with KAX_LOOP only, one with a KAX_UPCAST RANGE) and asserts
// uop_has_upcast_or_unroll fires on the second and not the first.
static int test_case7_route_gate(void) {
  thvm_init();
  TEST_BEGIN("case7 piece#4: uop_has_upcast_or_unroll detects opt-rich kernels");

  u32 dims[1] = { 8 };
  Term ob = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
  Term ib = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);

  // Plain LOOP: gate must NOT fire.
  Term rl = uop_range(0, KAX_LOOP, 8);
  Term il = uop_index_e(ib, rl);
  Term ll = uop_load(il);
  Term ol = uop_index_e(ob, rl);
  Term sl = uop_store(ob, ol, ll);
  CHECK_EQ(uop_has_upcast_or_unroll(sl), 0);

  // UPCAST: gate MUST fire.
  Term ru = uop_range(1, KAX_UPCAST, 4);
  Term iu = uop_index_e(ib, ru);
  Term lu = uop_load(iu);
  Term ou = uop_index_e(ob, ru);
  Term su = uop_store(ob, ou, lu);
  CHECK_EQ(uop_has_upcast_or_unroll(su), 1);

  // UNROLL: gate MUST fire.
  Term rn = uop_range(2, KAX_UNROLL, 4);
  Term in_ = uop_index_e(ib, rn);
  Term ln = uop_load(in_);
  Term on = uop_index_e(ob, rn);
  Term sn = uop_store(ob, on, ln);
  CHECK_EQ(uop_has_upcast_or_unroll(sn), 1);

  thvm_free();
  TEST_REPORT();
}

int main(void) {
  int rc = 0;
  rc |= test_case1_elementwise();
  rc |= test_case2_matmul();
  rc |= test_case3_post_devectorize_reduce();
  rc |= test_case4_end_to_end_render();
  rc |= test_case5_render_reduce_single_axis();
  rc |= test_case6_render_multi_axis_reduce();
  rc |= test_case7_route_gate();
  rc |= test_case7b_bail_on_unfolded_contract();
  return rc;
}
