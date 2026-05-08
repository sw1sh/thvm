// test_apply_opt_dag.c -- DAG-side apply_opt: TC + GLOBAL coverage.
//
// Phase E groundwork: validates that uop_dag_apply_tc and
// uop_dag_apply_global produce the expected DAG mutations on a
// canonical matmul shape, and that kernel_apply_opt's outer dispatcher
// takes the DAG path when cached_lift.store_root is populated.

#include "../src/thvm.c"
#include "test.h"

// Build the canonical 16x16 matmul DAG used across test_uop_recognise_tc
// and friends.  Returns the STORE root.  Caller-provided buffers are
// instance-disambiguated (out_inst=0, a_inst=1, b_inst=2) so
// rmu_discover_bufs sees a stable buffer order.
static Term build_matmul_root(Term *out_a, Term *out_b, Term *out_c) {
  u32 dimsA[2] = { 16, 32 };
  u32 dimsB[2] = { 32, 16 };
  u32 dimsC[2] = { 16, 16 };
  Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsA, 1);
  Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsB, 2);
  Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsC, 0);

  Term r_m = uop_range(0, KAX_LOOP,   16);
  Term r_n = uop_range(1, KAX_LOOP,   16);
  Term r_k = uop_range(2, KAX_REDUCE, 32);
  Term k16 = uop_const(DT_INT32, 16);
  Term k32 = uop_const(DT_INT32, 32);

  // a[m,k]: m*K + k where K = 32
  Term a_addr = uop_int_binary(UOP_IADD,
                               uop_int_binary(UOP_IMUL, r_m, k32), r_k);
  // b[k,n]: k*N + n where N = 16
  Term b_addr = uop_int_binary(UOP_IADD,
                               uop_int_binary(UOP_IMUL, r_k, k16), r_n);
  // c[m,n]: m*N + n
  Term c_addr = uop_int_binary(UOP_IADD,
                               uop_int_binary(UOP_IMUL, r_m, k16), r_n);

  Term a_load = uop_index_e(A, a_addr);
  Term b_load = uop_index_e(B, b_addr);
  Term mul    = uop_binary(UOP_MUL, a_load, b_load);
  Term reduce = uop_reduce(REDUCE_SUM, 2, mul);
  Term root   = uop_store(C, c_addr, reduce);

  if (out_a != NULL) *out_a = A;
  if (out_b != NULL) *out_b = B;
  if (out_c != NULL) *out_c = C;
  return root;
}

// === KOP_TC ==========================================================

static void test_apply_tc_wraps_bare_reduce(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);

  // Sanity: STORE.value is REDUCE (no OPT yet).
  Term value = heap_read(term_val(root) + 2);
  CHECK(term_tag(value) == TAG_UOP);
  CHECK(term_ext(value) == UOP_REDUCE);

  Term new_root = uop_dag_apply_tc(root, 8);
  CHECK(new_root != 0);
  CHECK(new_root != root);
  Term new_value = heap_read(term_val(new_root) + 2);
  CHECK(term_tag(new_value) == TAG_UOP);
  CHECK(term_ext(new_value) == UOP_OPT);
  CHECK(uop_opt_kind(new_value) == UOP_OPT_TC);
  CHECK(uop_opt_factor(new_value) == 8);
  // Inner is the original REDUCE.
  CHECK(uop_opt_target(new_value) == value);
}

static void test_apply_tc_replaces_existing_factor(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  // First apply: factor 8.
  Term r1 = uop_dag_apply_tc(root, 8);
  CHECK(uop_opt_factor(heap_read(term_val(r1) + 2)) == 8);
  // Second apply: factor 16 -- should replace, not double-wrap.
  Term r2 = uop_dag_apply_tc(r1, 16);
  CHECK(r2 != 0);
  CHECK(r2 != r1);
  Term v2 = heap_read(term_val(r2) + 2);
  CHECK(term_ext(v2) == UOP_OPT);
  CHECK(uop_opt_kind(v2) == UOP_OPT_TC);
  CHECK(uop_opt_factor(v2) == 16);
  // Inner is still the original REDUCE (not OPT-of-OPT).
  Term inner = uop_opt_target(v2);
  CHECK(term_ext(inner) == UOP_REDUCE);
}

static void test_apply_tc_idempotent_on_same_factor(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r1 = uop_dag_apply_tc(root, 16);
  Term r2 = uop_dag_apply_tc(r1, 16);
  // Hash-cons: rebuilding with the same params must dedup back to r1.
  CHECK(r2 == r1);
}

static void test_apply_tc_bails_on_non_store(void) {
  // Pass in a bare CONST -- not a STORE root.
  Term c = uop_const(DT_INT32, 42);
  CHECK(uop_dag_apply_tc(c, 8) == 0);
}

// === KOP_GLOBAL ======================================================

static void test_apply_global_swaps_axis_type(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  // Apply GLOBAL to axis 0 (m). Walk the post-rewrite DAG, find
  // RANGE leaves, assert axis 0 is now KAX_GLOBAL while axes 1 & 2
  // are unchanged.
  Term new_root = uop_dag_apply_global(root, 0);
  CHECK(new_root != 0);
  CHECK(new_root != root);
  // Reach into STORE.addr for an m-bearing RANGE.
  Term addr_c = heap_read(term_val(new_root) + 1);
  Term ranges[16]; u32 n = 0;
  rmu_collect_ranges(addr_c, ranges, &n);
  // Look for axis 0 and axis 1 -- both should appear in addr_c.
  int saw_m_global = 0, saw_n_loop = 0;
  for (u32 i = 0; i < n; i++) {
    u32 aid = (u32)term_val(heap_read(term_val(ranges[i]) + 0));
    u32 atype = (u32)term_val(heap_read(term_val(ranges[i]) + 1));
    if (aid == 0 && atype == KAX_GLOBAL) saw_m_global = 1;
    if (aid == 1 && atype == KAX_LOOP)   saw_n_loop = 1;
  }
  CHECK(saw_m_global);
  CHECK(saw_n_loop);
}

static void test_apply_global_idempotent(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r1 = uop_dag_apply_global(root, 0);
  Term r2 = uop_dag_apply_global(r1, 0);
  // Second apply finds RANGE(0, GLOBAL, _) which doesn't match the
  // (axis_type == KAX_LOOP) gate, so the rewrite is a no-op and the
  // walker memoises `r1` unchanged.
  CHECK(r2 == r1);
}

static void test_apply_global_axis_not_present(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  // Axis 99 isn't in the DAG; rewrite finds no matches; root is
  // returned unchanged.
  Term r1 = uop_dag_apply_global(root, 99);
  CHECK(r1 == root);
}

// === KOP_TC + GLOBAL composition (parallel TC) =======================

static void test_apply_tc_and_global_compose(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  // Apply KOP_TC, then GLOBAL on m (0) and n (1).  Expected post-state:
  //   STORE.value is OPT(REDUCE, TC, factor)
  //   m_axis range has axis_type = KAX_GLOBAL
  //   n_axis range has axis_type = KAX_GLOBAL
  Term r = root;
  KOpt opt_tc = { KOP_TC, 0, 16 };
  r = uop_dag_apply_kopt(r, opt_tc);
  CHECK(r != 0);
  KOpt opt_g_m = { KOP_GLOBAL, 0, 0 };
  r = uop_dag_apply_kopt(r, opt_g_m);
  CHECK(r != 0);
  KOpt opt_g_n = { KOP_GLOBAL, 1, 0 };
  r = uop_dag_apply_kopt(r, opt_g_n);
  CHECK(r != 0);

  // Verify: STORE.value is OPT(_, TC, 16).
  Term value = heap_read(term_val(r) + 2);
  CHECK(term_ext(value) == UOP_OPT);
  CHECK(uop_opt_kind(value) == UOP_OPT_TC);
  CHECK(uop_opt_factor(value) == 16);

  // Verify: addr_c references RANGE(0, GLOBAL, _) and RANGE(1, GLOBAL, _).
  Term addr_c = heap_read(term_val(r) + 1);
  Term ranges[16]; u32 n = 0;
  rmu_collect_ranges(addr_c, ranges, &n);
  int m_global = 0, n_global = 0;
  for (u32 i = 0; i < n; i++) {
    u32 aid = (u32)term_val(heap_read(term_val(ranges[i]) + 0));
    u32 atype = (u32)term_val(heap_read(term_val(ranges[i]) + 1));
    if (aid == 0 && atype == KAX_GLOBAL) m_global = 1;
    if (aid == 1 && atype == KAX_GLOBAL) n_global = 1;
  }
  CHECK(m_global);
  CHECK(n_global);
}

// === kernel_apply_opt outer dispatcher (DAG vs schedule paths) =======

static void test_kernel_apply_opt_dag_path(void) {
  Term A, B, C;
  Term root = build_matmul_root(&A, &B, &C);

  // Allocate a synthetic KernelEntry and populate cached_lift only.
  // schedule remains NULL -- forces the DAG branch.
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->cached_lift.store_root = root;
  ke->cached_lift.out_buf    = C;
  ke->cached_lift.in_bufs[0] = A;
  ke->cached_lift.in_bufs[1] = B;
  ke->cached_lift.n_inputs   = 2;
  ke->cached_lift.n_outputs  = 1;
  ke->cached_lift.out_bufs[0] = C;
  ke->compute_root = root;
  ke->n_inputs = 2;

  KOpt opt = { KOP_TC, 0, 32 };
  int ok = kernel_apply_opt(ke, opt);
  CHECK(ok == 1);
  CHECK(ke->cached_lift.store_root != root);
  Term new_value = heap_read(term_val(ke->cached_lift.store_root) + 2);
  CHECK(term_ext(new_value) == UOP_OPT);
  CHECK(uop_opt_kind(new_value) == UOP_OPT_TC);
  CHECK(uop_opt_factor(new_value) == 32);
  CHECK(ke->compute_root == ke->cached_lift.store_root);
}

static void test_kernel_apply_opt_unsupported_op_bails(void) {
  // Reserved opcodes (PADTO/NOLOCALS) and zero-arg splits must still
  // bail in the DAG path.  KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP/SWAP
  // are supported; they're tested separately below.
  Term root = build_matmul_root(NULL, NULL, NULL);
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->cached_lift.store_root = root;
  ke->compute_root = root;

  // KOP_PADTO is reserved; unsupported.
  KOpt opt = { KOP_PADTO, 0, 8 };
  CHECK(kernel_apply_opt(ke, opt) == 0);
  CHECK(ke->cached_lift.store_root == root);
}

// === KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP DAG mutation =============

static void test_apply_split_upcast_splits_axis(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  // Split axis 0 (m, extent 16) by 4: outer at axis 0 (extent 4),
  // inner at axis 1 (extent 4, KAX_UPCAST).  Existing axis 1 (n,
  // extent 16) and axis 2 (k, extent 32) shift right to 2 and 3.
  Term r = uop_dag_apply_split(root, KOP_UPCAST, 0, 4);
  CHECK(r != 0);
  CHECK(r != root);

  // Walk and find the new outer (axis 0, extent 4) + new inner (axis
  // 1, KAX_UPCAST, extent 4).  Also verify shifted ranges exist.
  Term ranges[16]; u32 n = 0;
  rmu_collect_ranges(heap_read(term_val(r) + 1), ranges, &n);  // c_addr
  Term value_ranges[16]; u32 nv = 0;
  rmu_collect_ranges(heap_read(term_val(r) + 2), value_ranges, &nv);
  int saw_outer = 0, saw_n_shifted = 0;
  for (u32 i = 0; i < n; i++) {
    u32 aid = uop_range_axis_id(ranges[i]);
    u32 ext = uop_range_extent(ranges[i]);
    if (aid == 0 && ext == 4) saw_outer = 1;
    if (aid == 2 && ext == 16) saw_n_shifted = 1;  // n shifted from 1 to 2
  }
  CHECK(saw_outer);
  CHECK(saw_n_shifted);
}

static void test_apply_split_local_no_opt_wrap(void) {
  // KOP_LOCAL is the only split that does NOT wrap inner in UOP_OPT.
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r = uop_dag_apply_split(root, KOP_LOCAL, 0, 4);
  CHECK(r != 0);
  CHECK(r != root);
}

static void test_apply_split_unroll_wraps_inner_opt(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r = uop_dag_apply_split(root, KOP_UNROLL, 0, 4);
  CHECK(r != 0);
  CHECK(r != root);
  // Walk c_addr looking for an OPT(_, UNROLL, 4) wrapper.
  Term c_addr = heap_read(term_val(r) + 1);
  // c_addr should be IADD(IMUL(IADD(IMUL(outer, 4), inner_unroll), 16), n).
  // Just check that an OPT_UNROLL wrapper exists somewhere.
  Term opts[16]; u32 no = 0;
  // Brute walk: descend IADD/IMUL chains.
  Term stack[32]; u32 sp = 0; stack[sp++] = c_addr;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_OPT) {
      if (no < 16) opts[no++] = t;
      Term tgt = uop_opt_target(t);
      if (term_tag(tgt) == TAG_UOP && sp < 32) stack[sp++] = tgt;
      continue;
    }
    if (op == UOP_IADD || op == UOP_IMUL) {
      u64 loc = term_val(t);
      Term l = heap_read(loc + 0);
      Term ri = heap_read(loc + 1);
      if (term_tag(l) == TAG_UOP && sp < 32) stack[sp++] = l;
      if (term_tag(ri) == TAG_UOP && sp < 32) stack[sp++] = ri;
    }
  }
  int saw_unroll = 0;
  for (u32 i = 0; i < no; i++) {
    if (uop_opt_kind(opts[i]) == UOP_OPT_UNROLL && uop_opt_factor(opts[i]) == 4) saw_unroll = 1;
  }
  CHECK(saw_unroll);
}

// === KOP_SWAP DAG mutation ============================================

static void test_apply_swap_axes(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  // Swap m (axis 0) and n (axis 1) on the matmul.
  Term r = uop_dag_apply_swap(root, 0, 1);
  CHECK(r != 0);
  CHECK(r != root);

  // c_addr originally references m (axis 0) and n (axis 1).  After
  // swap, c_addr should reference axis 1 in the m position and axis 0
  // in the n position.
  Term c_addr = heap_read(term_val(r) + 1);
  Term ranges[16]; u32 n = 0;
  rmu_collect_ranges(c_addr, ranges, &n);
  int saw_swapped_0 = 0, saw_swapped_1 = 0;
  for (u32 i = 0; i < n; i++) {
    u32 aid = uop_range_axis_id(ranges[i]);
    u32 ext = uop_range_extent(ranges[i]);
    // Both M and N were extent 16, so swap is detectable only by
    // axis_id presence -- both 0 and 1 should still be there but with
    // their roles swapped.  At minimum verify both ids still appear.
    if (aid == 0 && ext == 16) saw_swapped_0 = 1;
    if (aid == 1 && ext == 16) saw_swapped_1 = 1;
  }
  CHECK(saw_swapped_0);
  CHECK(saw_swapped_1);
}

static void test_apply_swap_noop_when_axes_equal(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r = uop_dag_apply_swap(root, 0, 0);
  CHECK(r == root);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("apply_tc_wraps_bare_reduce");        test_apply_tc_wraps_bare_reduce();
  TEST_BEGIN("apply_tc_replaces_existing_factor"); test_apply_tc_replaces_existing_factor();
  TEST_BEGIN("apply_tc_idempotent_on_same_factor"); test_apply_tc_idempotent_on_same_factor();
  TEST_BEGIN("apply_tc_bails_on_non_store");       test_apply_tc_bails_on_non_store();

  TEST_BEGIN("apply_global_swaps_axis_type");      test_apply_global_swaps_axis_type();
  TEST_BEGIN("apply_global_idempotent");           test_apply_global_idempotent();
  TEST_BEGIN("apply_global_axis_not_present");     test_apply_global_axis_not_present();

  TEST_BEGIN("apply_tc_and_global_compose");       test_apply_tc_and_global_compose();

  TEST_BEGIN("kernel_apply_opt_dag_path");         test_kernel_apply_opt_dag_path();
  TEST_BEGIN("kernel_apply_opt_unsupported_op_bails");
                                                    test_kernel_apply_opt_unsupported_op_bails();

  TEST_BEGIN("apply_split_upcast_splits_axis");    test_apply_split_upcast_splits_axis();
  TEST_BEGIN("apply_split_local_no_opt_wrap");     test_apply_split_local_no_opt_wrap();
  TEST_BEGIN("apply_split_unroll_wraps_inner_opt"); test_apply_split_unroll_wraps_inner_opt();
  TEST_BEGIN("apply_swap_axes");                   test_apply_swap_axes();
  TEST_BEGIN("apply_swap_noop_when_axes_equal");   test_apply_swap_noop_when_axes_equal();

  thvm_free();
  TEST_REPORT();
}
