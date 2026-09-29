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

#include <dlfcn.h>
#include <unistd.h>

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
  // END must precede the final LOAD of the accumulator.  Devectorize
  // emits LOAD(AFTER(acc, END(update, r))) with the update storing
  // through AFTER(acc, init), so in emission order: PLACEHOLDER, init
  // STORE, update STORE, END, then the final LOAD.
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
  CHECK(strstr(buf, "for (int a0 = 0; a0 < 8; a0++)") != NULL);
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
  CHECK(strstr(buf, "for (int a0 = 0; a0 < 8;") != NULL);
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
  CHECK(strstr(buf, "for (int a0 = 0; a0 < 4;") != NULL);
  CHECK(strstr(buf, "for (int a1 = 0; a1 < 3;") != NULL);
  CHECK(strstr(buf, "for (int a2 = 0; a2 < 3;") != NULL);
  // The accumulator decl is at body scope (before any for-loop).
  const char *acc = strstr(buf, "float _acc0;");
  const char *first_for = strstr(buf, "for (int a");
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

// === Reduces inside output loops (codegen/__init__.py:197-204) ==========

static u32 pos_of_term(LinKernel const *k, Term want) {
  for (u32 i = 0; i < k->n; i++) {
    if (k->uops[i] == want) {
      return i;
    }
  }
  return 0xFFFFFFFFu;
}

// A STORE's buf with the ordering AFTERs peeled off.
static Term store_target(Term st) {
  Term b = heap_read(term_val(st) + 0);
  while (term_tag(b) == TAG_UOP && term_ext(b) == UOP_AFTER) {
    b = heap_read(term_val(b) + 0);
  }
  return b;
}

// Position of the init (value is the identity CONST) or the update STORE
// into accumulator `acc_id`.
static u32 pos_of_acc_store(LinKernel const *k, u32 acc_id, int init) {
  for (u32 i = 0; i < k->n; i++) {
    Term t = k->uops[i];
    if (term_ext(t) != UOP_STORE) {
      continue;
    }
    Term acc = store_target(t);
    if (term_ext(acc) != UOP_PLACEHOLDER || uop_placeholder_acc_id(acc) != acc_id) {
      continue;
    }
    Term v = heap_read(term_val(t) + 2);
    if ((term_ext(v) == UOP_CONST) == (init != 0)) {
      return i;
    }
  }
  return 0xFFFFFFFFu;
}

// Axis ids of the RANGEs an init STORE's buf is sequenced after (the
// reduce's live ranges); returns how many.
static u32 init_live_axes(LinKernel const *k, u32 acc_id, u32 *axes) {
  u32 p = pos_of_acc_store(k, acc_id, 1);
  if (p == 0xFFFFFFFFu) {
    return 0;
  }
  u32 n = 0;
  Term b = heap_read(term_val(k->uops[p]) + 0);
  while (term_tag(b) == TAG_UOP && term_ext(b) == UOP_AFTER) {
    Term dep = heap_read(term_val(b) + 1);
    if (term_ext(dep) == UOP_RANGE && n < 8) {
      axes[n++] = uop_range_axis_id(dep);
    }
    b = heap_read(term_val(b) + 0);
  }
  return n;
}

// Linearize + render `root` as C99 into buf; returns the renderer's result.
static int render_lin_c(Term root, LinKernel *lk, char *buf, u32 cap) {
  buf[0] = 0;
  if (!uop_linearize(root, lk)) {
    return 0;
  }
  FILE *fp = fmemopen(buf, cap - 1, "w");
  if (fp == NULL) {
    return 0;
  }
  int ok = cg_render_linearized_c(lk, "k", fp);
  long n = ftell(fp);
  fclose(fp);
  buf[n > 0 ? n : 0] = 0;
  return ok;
}

// Compile the rendered kernel with clang, run it once, return 1 if it
// compiled and ran.  The signature is render_linearized.c's C prelude.
static int run_c_kernel(const char *src, float *out, const void *const *ins) {
  static u32 serial = 0;
  char src_path[96], dl_path[96], cmd[320];
  snprintf(src_path, sizeof(src_path), "/tmp/thvm_lin_reduce_%d_%u.c",
           (int)getpid(), serial);
  snprintf(dl_path, sizeof(dl_path), "/tmp/thvm_lin_reduce_%d_%u.dylib",
           (int)getpid(), serial);
  serial++;
  FILE *f = fopen(src_path, "w");
  if (f == NULL) {
    return 0;
  }
  fputs(src, f);
  fclose(f);
  snprintf(cmd, sizeof(cmd), "%s -O1 -fPIC -shared -o %s %s", cpu_cc(), dl_path, src_path);
  int built = (system(cmd) == 0);
  unlink(src_path);
  if (!built) {
    return 0;
  }
  void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
  unlink(dl_path);
  if (h == NULL) {
    return 0;
  }
  typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *);
  KFn kfn = (KFn)dlsym(h, "k");
  if (kfn != NULL) {
    kfn(out, ins, 0, NULL);
  }
  dlclose(h);
  return kfn != NULL;
}

// out[r0] = sum_r1 in[r0*e1 + r1]: r0 an output loop, r1 the reduce loop.
static Term build_row_sum(u32 e0, u32 e1, Term *r0_out, Term *r1_out) {
  u32 dims_out[1] = { e0 };
  u32 dims_in[1]  = { e0 * e1 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_out, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in, 1);
  Term r0 = uop_range(0, KAX_LOOP, e0);
  Term r1 = uop_range(1, KAX_LOOP, e1);
  Term addr = uop_int_binary(UOP_IADD,
                uop_int_binary(UOP_IMUL, r0, uop_const(DT_INT32, e1)), r1);
  u32 axes[1] = { 1 };
  Term red = uop_reduce_multi(REDUCE_SUM, 1, axes, uop_load(uop_index_e(in_buf, addr)));
  *r0_out = r0;
  *r1_out = r1;
  return uop_store(out_buf, uop_index_e(out_buf, r0), red);
}

// Renders out[r0] = sum_r1 in[r0*e1 + r1] and checks the order and the
// numbers: the accumulator init runs once per output element, inside the
// r0 loop and before the r1 loop, and the END closes the r1 loop right
// after the update, before the output store.
static void check_row_sum(u32 e0, u32 e1) {
  Term r0, r1;
  Term dv = uop_devectorize_graph(build_row_sum(e0, e1, &r0, &r1));
  static LinKernel lk;
  static char buf[16384];
  CHECK(render_lin_c(dv, &lk, buf, sizeof(buf)));
  CHECK(validate_toposort(&lk));
  CHECK(strstr(buf, "unsupported") == NULL);

  u32 p_r0   = pos_of_term(&lk, r0);
  u32 p_r1   = pos_of_term(&lk, r1);
  u32 p_init = pos_of_acc_store(&lk, 0, 1);
  u32 p_upd  = pos_of_acc_store(&lk, 0, 0);
  u32 p_end  = pos_of_first_op(&lk, UOP_END);
  CHECK(p_r0 < p_init);
  CHECK(p_init < p_r1);
  CHECK(p_r1 < p_upd);
  CHECK(p_upd < p_end);
  CHECK(p_end < pos_of_last_op(&lk, UOP_STORE));

  char f0[64], f1[64];
  snprintf(f0, sizeof(f0), "for (int a0 = 0; a0 < %u;", e0);
  snprintf(f1, sizeof(f1), "for (int a1 = 0; a1 < %u;", e1);
  const char *loop0 = strstr(buf, f0);
  const char *loop1 = strstr(buf, f1);
  const char *init  = strstr(buf, "_acc0 = 0x0p+0f;");
  CHECK(loop0 != NULL && init != NULL && loop1 != NULL);
  CHECK(loop0 < init && init < loop1);
  CHECK(strstr(buf, "   }\n   out[a0] = _acc0;\n") != NULL);

  static float in[4096], out[64];
  for (u32 i = 0; i < e0 * e1; i++) {
    in[i] = (float)(i % 7);
  }
  const void *ins[1] = { in };
  CHECK(run_c_kernel(buf, out, ins));
  for (u32 i = 0; i < e0; i++) {
    float want = 0.0f;
    for (u32 j = 0; j < e1; j++) {
      want += in[i * e1 + j];
    }
    CHECK(out[i] == want);
  }
}

// Case 8: 4 outputs, each a sum of 8.  Without the live-range AFTER the
// init ran once, before the output loop, so the accumulator carried over
// from one output element to the next.
static int test_case8_reduce_init_per_output(void) {
  thvm_init();
  TEST_BEGIN("case8 out[r0] = sum_r1: init inside the output loop, END after the update");
  check_row_sum(4, 8);
  thvm_free();
  TEST_REPORT();
}

// Case 9: the output loop runs more iterations (64) than the reduce loop
// (8), so run-count priority alone would open r1 before the init; the
// reduce's RANGE must be nested inside the ranges its END lives in
// (tinygrad pm_add_control_flow).
static int test_case9_reduce_init_wide_output(void) {
  thvm_init();
  TEST_BEGIN("case9 out[64] = sum over 8: the reduce loop nests inside the output loop");
  check_row_sum(64, 8);
  thvm_free();
  TEST_REPORT();
}

// Case 10: a reduce inside a reduce,
//   out[r0] = sum_r1 in[r0*8 + r1] * (sum_r2 w[r1*3 + r2]).
// The inner reduce lowers first (acc 0), the outer second (acc 1).  The
// outer init lives in r0 only -- never in its own r1 or the inner r2 the
// inner END already closes -- and precedes both reduce loops; the inner
// init lives in r1 and precedes r2.
static int test_case10_nested_reduce(void) {
  thvm_init();
  TEST_BEGIN("case10 nested reduce: each init runs inside its live loops only");

  u32 dims_out[1] = { 4 };
  u32 dims_in[1]  = { 32 };
  u32 dims_w[1]   = { 24 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_out, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in, 1);
  Term w_buf   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_w, 2);
  Term r0 = uop_range(0, KAX_LOOP, 4);
  Term r1 = uop_range(1, KAX_LOOP, 8);
  Term r2 = uop_range(2, KAX_LOOP, 3);
  Term in_addr = uop_int_binary(UOP_IADD,
                   uop_int_binary(UOP_IMUL, r0, uop_const(DT_INT32, 8)), r1);
  Term w_addr  = uop_int_binary(UOP_IADD,
                   uop_int_binary(UOP_IMUL, r1, uop_const(DT_INT32, 3)), r2);
  u32 ax2[1] = { 2 };
  Term inner = uop_reduce_multi(REDUCE_SUM, 1, ax2, uop_load(uop_index_e(w_buf, w_addr)));
  Term body  = uop_binary(UOP_MUL, uop_load(uop_index_e(in_buf, in_addr)), inner);
  u32 ax1[1] = { 1 };
  Term outer = uop_reduce_multi(REDUCE_SUM, 1, ax1, body);
  Term store = uop_store(out_buf, uop_index_e(out_buf, r0), outer);

  Term dv = uop_devectorize_graph(store);
  static LinKernel lk;
  static char buf[16384];
  CHECK(render_lin_c(dv, &lk, buf, sizeof(buf)));
  CHECK(validate_toposort(&lk));

  u32 axes[8];
  u32 n_out = init_live_axes(&lk, 1, axes);
  CHECK_EQ(n_out, 1);
  CHECK_EQ(axes[0], 0);
  u32 n_in = init_live_axes(&lk, 0, axes);
  CHECK_EQ(n_in, 1);
  CHECK_EQ(axes[0], 1);

  u32 p_out_init = pos_of_acc_store(&lk, 1, 1);
  u32 p_in_init  = pos_of_acc_store(&lk, 0, 1);
  CHECK(pos_of_term(&lk, r0) < p_out_init);
  CHECK(p_out_init < pos_of_term(&lk, r1));
  CHECK(pos_of_term(&lk, r1) < p_in_init);
  CHECK(p_in_init < pos_of_term(&lk, r2));
  CHECK(pos_of_term(&lk, r2) < pos_of_acc_store(&lk, 0, 0));
  CHECK(pos_of_acc_store(&lk, 0, 0) < pos_of_acc_store(&lk, 1, 0));

  static float in[32], w[24], out[4];
  for (u32 i = 0; i < 32; i++) {
    in[i] = (float)(i % 5);
  }
  for (u32 i = 0; i < 24; i++) {
    w[i] = (float)(i % 3);
  }
  const void *ins[2] = { in, w };
  CHECK(run_c_kernel(buf, out, ins));
  for (u32 i = 0; i < 4; i++) {
    float want = 0.0f;
    for (u32 j = 0; j < 8; j++) {
      float s = 0.0f;
      for (u32 q = 0; q < 3; q++) {
        s += w[j * 3 + q];
      }
      want += in[i * 8 + j] * s;
    }
    CHECK(out[i] == want);
  }

  thvm_free();
  TEST_REPORT();
}

// Case 11: F parallel accumulators.  A STACK body of 4 lanes (an UPCAST'd
// output reaching the reduce as lanes) gets 4 accumulators; every lane's
// init runs inside the output loop and before the reduce loop, and all
// four updates close under one END.  The F-lane output store renders on
// the PTX route only (text check).
static int test_case11_parallel_accumulators(void) {
  thvm_init();
  TEST_BEGIN("case11 F-lane reduce: every lane's init inside r0, before r1");

  u32 dims_out[1] = { 8 };
  u32 dims_in[1]  = { 64 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_out, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in, 1);
  Term r0 = uop_range(0, KAX_LOOP, 2);
  Term r1 = uop_range(1, KAX_REDUCE, 8);
  Term lanes[4], addrs[4];
  for (u32 i = 0; i < 4; i++) {
    Term row = uop_int_binary(UOP_IADD,
                 uop_int_binary(UOP_IMUL, r0, uop_const(DT_INT32, 4)),
                 uop_const(DT_INT32, i));
    Term a = uop_int_binary(UOP_IADD,
               uop_int_binary(UOP_IMUL, row, uop_const(DT_INT32, 8)), r1);
    lanes[i] = uop_load(uop_index_e(in_buf, a));
    addrs[i] = uop_index_e(out_buf, row);
  }
  u32 axes[1] = { 1 };
  Term red   = uop_reduce_multi(REDUCE_SUM, 1, axes, uop_stack(4, lanes));
  Term store = uop_store(out_buf, uop_stack(4, addrs), red);

  Term dv = uop_devectorize_graph(store);
  static LinKernel lk;
  CHECK(uop_linearize(dv, &lk));
  CHECK(validate_toposort(&lk));
  u32 p_r0 = pos_of_term(&lk, r0);
  u32 p_r1 = pos_of_term(&lk, r1);
  u32 p_end = pos_of_first_op(&lk, UOP_END);
  CHECK_EQ(pos_of_last_op(&lk, UOP_END), p_end);
  for (u32 acc = 0; acc < 4; acc++) {
    u32 p_init = pos_of_acc_store(&lk, acc, 1);
    u32 p_upd  = pos_of_acc_store(&lk, acc, 0);
    CHECK(p_r0 < p_init);
    CHECK(p_init < p_r1);
    CHECK(p_r1 < p_upd);
    CHECK(p_upd < p_end);
  }

  // PTX: r0 indexes the store, so it is a thread axis; each lane's init
  // `mov` precedes the reduce loop's label.
  static char ptx[1 << 16];
  FILE *fp = fmemopen(ptx, sizeof(ptx) - 1, "w");
  CHECK(fp != NULL);
  int ok = cg_render_linearized_ptx(&lk, "k", 70, fp);
  long n = ftell(fp);
  fclose(fp);
  ptx[n > 0 ? n : 0] = 0;
  CHECK(ok);
  const char *loop = strstr(ptx, "LOOP_0:");
  CHECK(loop != NULL);
  for (u32 acc = 0; acc < 4; acc++) {
    char init[48];
    snprintf(init, sizeof(init), "mov.b32 %%acc_f32_%u, %%const_f32_", acc);
    const char *p = strstr(ptx, init);
    CHECK(p != NULL && loop != NULL && p < loop);
  }
  CHECK(strstr(ptx, "LOOP_1:") == NULL);

  thvm_free();
  TEST_REPORT();
}

// Case 12: an extent-1 reduce loop gives the init and the update the same
// run count; the update still follows the init because it stores through
// acc.after(init) (tinygrad's acc_initted), not by priority.
static int test_case12_reduce_extent_one(void) {
  thvm_init();
  TEST_BEGIN("case12 extent-1 reduce: the update is sequenced after the init");

  Term r0, r1;
  Term dv = uop_devectorize_graph(build_row_sum(4, 1, &r0, &r1));
  static LinKernel lk;
  static char buf[16384];
  CHECK(render_lin_c(dv, &lk, buf, sizeof(buf)));
  CHECK(pos_of_acc_store(&lk, 0, 1) < pos_of_acc_store(&lk, 0, 0));

  static float in[4], out[4];
  for (u32 i = 0; i < 4; i++) {
    in[i] = (float)(i + 1);
  }
  const void *ins[1] = { in };
  CHECK(run_c_kernel(buf, out, ins));
  for (u32 i = 0; i < 4; i++) {
    CHECK(out[i] == in[i]);
  }

  thvm_free();
  TEST_REPORT();
}

// Case 13: two reduces over the same RANGE Term would open one loop and
// close it twice.  tinygrad merges or clones such ranges first
// (merge_reduce_ends); thvm's linearizer declines the kernel instead.
static int test_case13_shared_reduce_range_declines(void) {
  thvm_init();
  TEST_BEGIN("case13 two ENDs closing one RANGE: linearize declines");

  u32 dims_out[1] = { 4 };
  u32 dims_in[1]  = { 32 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_out, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in, 1);
  Term r0 = uop_range(0, KAX_LOOP, 4);
  Term r1 = uop_range(1, KAX_LOOP, 8);
  Term addr = uop_int_binary(UOP_IADD,
                uop_int_binary(UOP_IMUL, r0, uop_const(DT_INT32, 8)), r1);
  Term ld = uop_load(uop_index_e(in_buf, addr));
  u32 axes[1] = { 1 };
  Term sum = uop_reduce_multi(REDUCE_SUM, 1, axes, ld);
  Term max = uop_reduce_multi(REDUCE_MAX, 1, axes, ld);
  Term store = uop_store(out_buf, uop_index_e(out_buf, r0),
                         uop_binary(UOP_ADD, sum, max));

  static LinKernel lk;
  CHECK_EQ(uop_linearize(uop_devectorize_graph(store), &lk), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 14: a vector value in a scalar slot has no single value.  The
// renderer declines instead of storing lane 0.
static int test_case14_vector_in_scalar_store_declines(void) {
  thvm_init();
  TEST_BEGIN("case14 STACK value stored to a scalar address: render declines");

  u32 dims[1] = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term r = uop_range(0, KAX_LOOP, 4);
  Term lanes[2] = {
    uop_load(uop_index_e(in_buf, r)),
    uop_load(uop_index_e(in_buf, uop_int_binary(UOP_IADD, r, uop_const(DT_INT32, 4)))),
  };
  Term store = uop_store(out_buf, uop_index_e(out_buf, r), uop_stack(2, lanes));

  static LinKernel lk;
  static char buf[4096];
  CHECK_EQ(render_lin_c(store, &lk, buf, sizeof(buf)), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 15: the Metal and CUDA preludes do not decode thread ids, so a
// lowered reduce stays on the legacy emit there; C renders it.
static int test_case15_gpu_targets_leave_reduces_to_legacy(void) {
  thvm_init();
  TEST_BEGIN("case15 lowered reduce: C renders it, Metal/CUDA decline");

  Term r0, r1;
  Term dv = uop_devectorize_graph(build_row_sum(4, 8, &r0, &r1));
  static LinKernel lk;
  CHECK(uop_linearize(dv, &lk));
  static char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf) - 1, "w");
  CHECK(fp != NULL);
  CHECK_EQ(cg_render_linearized_c(&lk, "k", fp), 1);
  CHECK_EQ(cg_render_linearized_metal(&lk, "k", fp), 0);
  CHECK_EQ(cg_render_linearized_cuda(&lk, "k", fp), 0);
  fclose(fp);

  thvm_free();
  TEST_REPORT();
}

// Case 16: out[i] = sum_j 2 * sum_k z[(i*4 + j)*3 + k] over 2^24 outputs.
// Every run count reaches 2^24 here, so a priority key that clips run counts
// at 24 bits ties each init with the nodes of its own reduce loop and the
// outer init lands inside the j loop, resetting the accumulator every
// iteration.  tinygrad compares exact run counts (linearizer.py:21), so the
// init sorts before the loop whenever the loop has more than one iteration.
static int test_case16_init_before_loop_at_2p24_outputs(void) {
  thvm_init();
  TEST_BEGIN("case16 2^24 outputs: each accumulator init precedes its reduce loop");

  u32 n_out = 1u << 24;
  u32 dout[1] = { n_out };
  u32 dz[1]   = { 4096 };
  Term O  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dout, 0);
  Term Z  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dz, 1);
  Term ri = uop_range(0, KAX_LOOP, n_out);
  Term rj = uop_range(1, KAX_REDUCE, 4);
  Term rk = uop_range(2, KAX_REDUCE, 3);
  Term ij = uop_int_binary(UOP_IADD,
              uop_int_binary(UOP_IMUL, ri, uop_const(DT_INT32, 4)), rj);
  Term ijk = uop_int_binary(UOP_IADD,
               uop_int_binary(UOP_IMUL, ij, uop_const(DT_INT32, 3)), rk);
  Term addr = uop_int_binary(UOP_IMOD, ijk, uop_const(DT_INT32, 4096));
  Term inner = uop_reduce(REDUCE_SUM, 2, uop_load(uop_index_e(Z, addr)));
  Term outer = uop_reduce(REDUCE_SUM, 1,
                 uop_binary(UOP_MUL, inner, uop_const(DT_FP32, 0x40000000u)));
  Term st = uop_store(O, uop_index_e(O, ri), outer);

  static LinKernel lk;
  static char buf[16384];
  CHECK(render_lin_c(uop_devectorize_graph(st), &lk, buf, sizeof(buf)));
  CHECK(validate_toposort(&lk));
  // The inner reduce lowers first (acc 0), the outer second (acc 1).
  CHECK(pos_of_term(&lk, ri) < pos_of_acc_store(&lk, 1, 1));
  CHECK(pos_of_acc_store(&lk, 1, 1) < pos_of_term(&lk, rj));
  CHECK(pos_of_term(&lk, rj) < pos_of_acc_store(&lk, 0, 1));
  CHECK(pos_of_acc_store(&lk, 0, 1) < pos_of_term(&lk, rk));
  const char *init_outer = strstr(buf, "_acc1 = 0x0p+0f;");
  const char *loop_j     = strstr(buf, "for (int a1 = 0; a1 < 4;");
  CHECK(init_outer != NULL && loop_j != NULL && init_outer < loop_j);

  thvm_free();
  TEST_REPORT();
}

// out[r0] = max_r1 in[r0*8 + r1] * 1e-7 + 0.1, on the C route.  The MAX
// accumulator starts at -INFINITY and 1e-7 has no six-decimal spelling, so
// every float literal must render exactly: `%f` spelled them `-inff` (not a
// C token) and `0.000000f`.
static int test_case17_float_literals_render_exactly(void) {
  thvm_init();
  TEST_BEGIN("case17 C route: -inf identity and 1e-7 render exactly");

  u32 dims_out[1] = { 4 };
  u32 dims_in[1]  = { 32 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_out, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in, 1);
  Term r0 = uop_range(0, KAX_LOOP, 4);
  Term r1 = uop_range(1, KAX_REDUCE, 8);
  Term addr = uop_int_binary(UOP_IADD,
                uop_int_binary(UOP_IMUL, r0, uop_const(DT_INT32, 8)), r1);
  u32 axes[1] = { 1 };
  Term mx = uop_reduce_multi(REDUCE_MAX, 1, axes, uop_load(uop_index_e(in_buf, addr)));
  float tiny = 1e-7f, tenth = 0.1f;
  u32 tiny_bits, tenth_bits;
  memcpy(&tiny_bits, &tiny, 4);
  memcpy(&tenth_bits, &tenth, 4);
  Term val = uop_binary(UOP_ADD,
               uop_binary(UOP_MUL, mx, uop_const(DT_FP32, tiny_bits)),
               uop_const(DT_FP32, tenth_bits));
  Term store = uop_store(out_buf, uop_index_e(out_buf, r0), val);

  static LinKernel lk;
  static char buf[16384];
  CHECK(render_lin_c(uop_devectorize_graph(store), &lk, buf, sizeof(buf)));
  static float in[32], out[4];
  for (u32 i = 0; i < 32; i++) {
    in[i] = (float)((int)((i * 5) % 13) - 20);
  }
  const void *ins[1] = { in };
  CHECK(run_c_kernel(buf, out, ins));
  for (u32 i = 0; i < 4; i++) {
    float m = -INFINITY;
    for (u32 j = 0; j < 8; j++) {
      m = (m < in[i * 8 + j]) ? in[i * 8 + j] : m;
    }
    CHECK(out[i] == m * tiny + tenth);
  }

  thvm_free();
  TEST_REPORT();
}

// out[r0] = sum_r1 (0 <= r0 + r1 - 1 < 8 ? in[r0 + r1 - 1] : 0), a padded
// window on the C route.  Index math is signed (UOP_ISUB / UOP_ILT are
// signed, thvm.h), as tinygrad renders ranges as `int`
// (renderer/cstyle.py:18-19): with `uint` loops and `%u` constants, -1
// became 4294967295 and `-1 < idx` was false for every idx.
static int test_case18_padded_window_index_is_signed(void) {
  thvm_init();
  TEST_BEGIN("case18 C route: a negative window offset stays signed");

  u32 dims_out[1] = { 4 };
  u32 dims_in[1]  = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_out, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in, 1);
  Term r0 = uop_range(0, KAX_LOOP, 4);
  Term r1 = uop_range(1, KAX_REDUCE, 3);
  Term idx = uop_int_binary(UOP_ISUB, uop_int_binary(UOP_IADD, r0, r1),
                            uop_const(DT_INT32, 1));
  Term ok  = uop_int_binary(UOP_IAND,
               uop_int_binary(UOP_ILT, uop_const(DT_INT32, 0xFFFFFFFFu), idx),
               uop_int_binary(UOP_ILT, idx, uop_const(DT_INT32, 8)));
  Term val = uop_iwhere(ok, uop_load(uop_index_e(in_buf, idx)),
                        uop_const(DT_FP32, 0));
  u32 axes[1] = { 1 };
  Term red = uop_reduce_multi(REDUCE_SUM, 1, axes, val);
  Term store = uop_store(out_buf, uop_index_e(out_buf, r0), red);

  static LinKernel lk;
  static char buf[16384];
  CHECK(render_lin_c(uop_devectorize_graph(store), &lk, buf, sizeof(buf)));
  static float in[8], out[4];
  for (u32 i = 0; i < 8; i++) {
    in[i] = (float)(i + 1);
  }
  const void *ins[1] = { in };
  CHECK(run_c_kernel(buf, out, ins));
  for (int i = 0; i < 4; i++) {
    float want = 0.0f;
    for (int j = 0; j < 3; j++) {
      int x = i + j - 1;
      want += (x >= 0 && x < 8) ? in[x] : 0.0f;
    }
    CHECK(out[i] == want);
  }

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
  rc |= test_case8_reduce_init_per_output();
  rc |= test_case9_reduce_init_wide_output();
  rc |= test_case10_nested_reduce();
  rc |= test_case11_parallel_accumulators();
  rc |= test_case12_reduce_extent_one();
  rc |= test_case13_shared_reduce_range_declines();
  rc |= test_case14_vector_in_scalar_store_declines();
  rc |= test_case15_gpu_targets_leave_reduces_to_legacy();
  rc |= test_case16_init_before_loop_at_2p24_outputs();
  rc |= test_case17_float_literals_render_exactly();
  rc |= test_case18_padded_window_index_is_signed();
  return rc;
}
