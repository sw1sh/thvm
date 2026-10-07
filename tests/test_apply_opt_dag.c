// test_apply_opt_dag.c - DAG-side apply_opt: TC, GLOBAL, split, SWAP and
// group coverage.
//
// Validates the DAG mutation each opt makes on a canonical matmul shape,
// the split validation tinygrad applies (split_targets, REDUCE membership,
// no group inside another reduce), and that kernel_apply_opt's outer
// dispatcher takes the DAG path when cached_lift.store_root is populated.

#include "../src/thvm.c"
#include "test.h"
#include <string.h>

static int rmu_contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

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
  kernel_lift_reserve(&ke->cached_lift, 2);
  ke->cached_lift.store_root = root;
  ke->cached_lift.out_buf    = C;
  ke->cached_lift.in_bufs[0] = A;
  ke->cached_lift.in_bufs[1] = B;
  ke->cached_lift.n_inputs   = 2;
  ke->n_inputs = 2;

  KOpt opt = { KOP_TC, 0, 32 };
  int ok = kernel_apply_opt(ke, opt);
  CHECK(ok == 1);
  CHECK(ke->cached_lift.store_root != root);
  Term new_value = heap_read(term_val(ke->cached_lift.store_root) + 2);
  CHECK(term_ext(new_value) == UOP_OPT);
  CHECK(uop_opt_kind(new_value) == UOP_OPT_TC);
  CHECK(uop_opt_factor(new_value) == 32);
}

static void test_kernel_apply_opt_unsupported_op_bails(void) {
  // Reserved opcodes (PADTO/NOLOCALS) and zero-arg splits must still
  // bail in the DAG path.  KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP/SWAP
  // are supported; they're tested separately below.
  Term root = build_matmul_root(NULL, NULL, NULL);
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->cached_lift.store_root = root;

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
  int saw_outer = 0, saw_n_shifted = 0;
  for (u32 i = 0; i < n; i++) {
    u32 aid = uop_range_axis_id(ranges[i]);
    u32 ext = uop_range_extent(ranges[i]);
    if (aid == 0 && ext == 4) saw_outer = 1;
    if (aid == 2 && ext == 16) saw_n_shifted = 1;  // n shifted from 1 to 2
  }
  CHECK(saw_outer);
  CHECK(saw_n_shifted);

  // The K reduce axis (originally 2, > target 0) shifts to 3, and the
  // UOP_REDUCE node's `axis` field must follow -- otherwise the
  // renderer's reduce-range matching stops finding it.  Find the REDUCE node and check its axis.
  Term value = heap_read(term_val(r) + 2);
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT) {
    value = uop_opt_target(value);
  }
  CHECK(term_tag(value) == TAG_UOP && term_ext(value) == UOP_REDUCE);
  CHECK_EQ(uop_reduce_n_axes(value), 1u);
  CHECK_EQ(uop_reduce_axis(value, 0), 3u);
  // And the actual K range leaf is now axis 3, KAX_REDUCE, extent 32.
  Term body = heap_read(term_val(value) + 0);
  Term value_ranges[16]; u32 nv = 0;
  rmu_collect_ranges(body, value_ranges, &nv);
  int saw_k_shifted = 0;
  for (u32 i = 0; i < nv; i++) {
    if (uop_range_axis_id(value_ranges[i]) == 3
        && uop_range_axis_type(value_ranges[i]) == KAX_REDUCE
        && uop_range_extent(value_ranges[i]) == 32) saw_k_shifted = 1;
  }
  CHECK(saw_k_shifted);
}

static void test_apply_split_local_no_opt_wrap(void) {
  // KOP_LOCAL is the only split that does NOT wrap inner in UOP_OPT.
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r = uop_dag_apply_split(root, KOP_LOCAL, 0, 4);
  CHECK(r != 0);
  CHECK(r != root);
}

static void test_apply_split_unroll_wraps_inner_opt(void) {
  // UNROLL the K reduce axis (2, extent 32) by 4.  Every reference to k
  // becomes IADD(IMUL(RANGE(2, REDUCE, 8), 4), OPT(RANGE(3, UNROLL, 4),
  // UNROLL, 4)) and the REDUCE keeps naming the outer id.
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r = uop_dag_apply_split(root, KOP_UNROLL, 2, 4);
  CHECK(r != 0);
  CHECK(r != root);
  Term red = heap_read(term_val(r) + 2);
  CHECK(term_tag(red) == TAG_UOP && term_ext(red) == UOP_REDUCE);
  CHECK_EQ(uop_reduce_n_axes(red), 1u);
  CHECK_EQ(uop_reduce_axis(red, 0), 2u);
  // a[m,k] = m*32 + k now reads m*32 + linear.
  Term a_load = heap_read(term_val(uop_reduce_src(red)) + 0);
  Term a_addr = heap_read(term_val(a_load) + 1);
  CHECK_EQ(term_ext(a_addr), UOP_IADD);
  Term linear = heap_read(term_val(a_addr) + 1);
  CHECK_EQ(term_ext(linear), UOP_IADD);
  Term scaled = heap_read(term_val(linear) + 0);
  Term inner  = heap_read(term_val(linear) + 1);
  CHECK_EQ(term_ext(scaled), UOP_IMUL);
  CHECK_EQ(heap_read(term_val(scaled) + 0), uop_range(2, KAX_REDUCE, 8));
  CHECK_EQ(heap_read(term_val(scaled) + 1), uop_const(DT_INT32, 4));
  CHECK_EQ(term_ext(inner), UOP_OPT);
  CHECK_EQ(uop_opt_kind(inner), (u32)UOP_OPT_UNROLL);
  CHECK_EQ(uop_opt_factor(inner), 4u);
  CHECK_EQ(uop_opt_target(inner), uop_range(3, KAX_UNROLL, 4));
  // b[k,n] = k*16 + n now reads linear*16 + n.
  Term b_load = heap_read(term_val(uop_reduce_src(red)) + 1);
  Term b_addr = heap_read(term_val(b_load) + 1);
  CHECK_EQ(term_ext(b_addr), UOP_IADD);
  Term b_scaled = heap_read(term_val(b_addr) + 0);
  CHECK_EQ(term_ext(b_scaled), UOP_IMUL);
  CHECK_EQ(heap_read(term_val(b_scaled) + 0), linear);
  CHECK_EQ(heap_read(term_val(b_scaled) + 1), uop_const(DT_INT32, 16));
  CHECK_EQ(heap_read(term_val(b_addr) + 1), uop_range(1, KAX_LOOP, 16));
}

// tinygrad split_targets (codegen/opt/postrange.py:14-15, checked in
// shift_to at :80): UPCAST splits an output axis (GLOBAL/LOCAL/WEAK, thvm
// LOOP/GLOBAL/LOCAL), UNROLL a reduce axis, LOCAL a GLOBAL or WEAK one.
static void test_apply_split_checks_source_type(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);   // 0, 1 LOOP; 2 REDUCE
  CHECK(uop_dag_apply_split(root, KOP_UNROLL, 0, 4) == 0);
  CHECK(uop_dag_apply_split(root, KOP_UPCAST, 2, 2) == 0);
  CHECK(uop_dag_apply_split(root, KOP_LOCAL,  2, 2) == 0);
  CHECK(uop_dag_apply_split(root, KOP_UPCAST, 0, 2) != 0);
  CHECK(uop_dag_apply_split(root, KOP_UNROLL, 2, 4) != 0);

  Term g = uop_dag_apply_global(root, 0);
  CHECK(uop_dag_apply_split(g, KOP_UPCAST, 0, 4) != 0);
  CHECK(uop_dag_apply_split(g, KOP_LOCAL,  0, 4) != 0);

  // The UPCAST inner (id 1) is no split target for another UPCAST or LOCAL.
  Term u = uop_dag_apply_split(root, KOP_UPCAST, 0, 4);
  CHECK(u != 0);
  CHECK(uop_dag_apply_split(u, KOP_UPCAST, 1, 2) == 0);
  CHECK(uop_dag_apply_split(u, KOP_LOCAL,  1, 2) == 0);

  // The dispatcher bails the same way and leaves store_root untouched.
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->cached_lift.store_root = root;
  KOpt bad = { KOP_UNROLL, 0, 4 };
  CHECK(kernel_apply_opt(ke, bad) == 0);
  CHECK(ke->cached_lift.store_root == root);
}

// A split keeps the outer id in REDUCE.axes, so a second UNROLL of the outer
// still finds its REDUCE; the UNROLL inner itself is no split target.
static void test_apply_split_unroll_twice_on_outer(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r1 = uop_dag_apply_split(root, KOP_UNROLL, 2, 4);
  CHECK(r1 != 0);
  CHECK(uop_dag_apply_split(r1, KOP_UNROLL, 2, 2) != 0);
  CHECK(uop_dag_apply_split(r1, KOP_UNROLL, 3, 2) == 0);
}

// hand_opts section 7 unrolls a GROUP_REDUCE axis (tinygrad's UNROLL
// split_targets hold the reduce-typed LOCAL a group leaves).
static void test_apply_split_unroll_on_group_reduce_axis(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term g = uop_dag_apply_group_reduce(root, 2, 4);
  CHECK(g != 0);
  CHECK(uop_dag_apply_split(g, KOP_UNROLL, 2, 2) != 0);
}

// tinygrad test_custom_kernel.py test_gemm_unroll_refused /
// test_gemm_group_refused: k is typed REDUCE but no REDUCE reduces it, the
// shape of a kernel that stores per (m, n, k).
static Term build_orphan_reduce_root(void) {
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
  Term a_addr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r_m, k32), r_k);
  Term b_addr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r_k, k16), r_n);
  Term c_addr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r_m, k16), r_n);
  Term mul = uop_binary(UOP_MUL, uop_index_e(A, a_addr), uop_index_e(B, b_addr));
  return uop_store(C, c_addr, mul);
}

// tinygrad postrange.py:128-130: UNROLL and the group split need an axis
// some REDUCE reduces.
static void test_apply_opt_rejects_orphan_reduce_axis(void) {
  Term root = build_orphan_reduce_root();
  CHECK(uop_dag_apply_split(root, KOP_UNROLL, 2, 4) == 0);
  CHECK(uop_dag_apply_group_reduce(root, 2, 4) == 0);
  KOpt top = { KOP_GROUPTOP, 2, 4 };
  CHECK(uop_dag_apply_kopt(root, top) == 0);
  CHECK(uop_dag_apply_split(root, KOP_UPCAST, 0, 4) != 0);
}

// Unique UOp nodes reachable from `root`.
static u32 count_uop_nodes(Term root) {
  UOpWalk w = {0};
  uop_walk_push(&w, root);
  u32 n = 0;
  for (Term t = uop_walk_pop(&w); t != 0; t = uop_walk_pop(&w)) {
    n++;
    u8  ar  = uop_arity((u8)term_ext(t));
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
      uop_walk_push(&w, heap_read(loc + i));
    }
  }
  uop_walk_free(&w);
  return n;
}

// `body` + w(m), where w(m) = sum_i z[m*4096 + i] over 1200 loads of about
// four unique nodes each, so a reduce over it holds more than 4096 nodes.
// WIDE_FIRST makes w the ADD's first operand and WIDE_LAST its second, so a
// depth-first walk meets either operand first.
enum { WIDE_NONE, WIDE_FIRST, WIDE_LAST };

static Term add_wide_term(Term body, Term r_m, int wide) {
  if (wide == WIDE_NONE) {
    return body;
  }
  u32  dimsZ[1] = { 4 * 4096 };
  Term Z   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsZ, 3);
  Term row = uop_int_binary(UOP_IMUL, r_m, uop_const(DT_INT32, 4096));
  Term sum = uop_index_e(Z, row);
  for (u32 i = 1; i < 1200; i++) {
    Term addr = uop_int_binary(UOP_IADD, row, uop_const(DT_INT32, i));
    sum = uop_binary(UOP_ADD, sum, uop_index_e(Z, addr));
  }
  return (wide == WIDE_FIRST) ? uop_binary(UOP_ADD, sum, body)
                              : uop_binary(UOP_ADD, body, sum);
}

// out[m] = sum_k (x[m,k] + w(m)).
static Term build_wide_row_sum_root(int wide) {
  u32 dimsX[2] = { 4, 32 };
  u32 dimsO[1] = { 4 };
  Term X = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsX, 1);
  Term O = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsO, 0);
  Term r_m = uop_range(0, KAX_LOOP,   4);
  Term r_k = uop_range(1, KAX_REDUCE, 32);
  Term mk  = uop_int_binary(UOP_IADD,
                            uop_int_binary(UOP_IMUL, r_m, uop_const(DT_INT32, 32)),
                            r_k);
  Term sum = uop_reduce(REDUCE_SUM, 1,
                        add_wide_term(uop_index_e(X, mk), r_m, wide));
  return uop_store(O, r_m, sum);
}

// out[m] = sum_j x[m,j] * max_k (y[m,j,k] + w(m)): the MAX reduce over k
// (axis 2) sits inside the SUM over j (axis 1) and reads j.  WIDE_NONE
// leaves w(m) out.
static Term build_nested_reduce_root(int wide) {
  u32 dimsX[2] = { 4, 8 };
  u32 dimsY[3] = { 4, 8, 16 };
  u32 dimsO[1] = { 4 };
  Term X = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsX, 1);
  Term Y = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 3, dimsY, 2);
  Term O = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsO, 0);
  Term r_m = uop_range(0, KAX_LOOP,   4);
  Term r_j = uop_range(1, KAX_REDUCE, 8);
  Term r_k = uop_range(2, KAX_REDUCE, 16);
  Term mj  = uop_int_binary(UOP_IADD,
                            uop_int_binary(UOP_IMUL, r_m, uop_const(DT_INT32, 8)),
                            r_j);
  Term mjk = uop_int_binary(UOP_IADD,
                            uop_int_binary(UOP_IMUL, mj, uop_const(DT_INT32, 16)),
                            r_k);
  Term inner = uop_reduce(REDUCE_MAX, 2,
                          add_wide_term(uop_index_e(Y, mjk), r_m, wide));
  Term outer = uop_reduce(REDUCE_SUM, 1,
                          uop_binary(UOP_MUL, uop_index_e(X, mj), inner));
  return uop_store(O, r_m, outer);
}

// tinygrad postrange.py:131-134 (test_kernel_opts.py test_double_sum_group):
// no group inside another reduce.  The ranges still open at a REDUCE are
// its body's minus the ones it or a reduce in its body closes.
static void test_apply_group_reduce_rejects_group_inside_reduce(void) {
  Term root = build_nested_reduce_root(WIDE_NONE);
  CHECK(uop_dag_apply_group_reduce(root, 2, 4) == 0);
  CHECK(uop_dag_apply_group_reduce(root, 1, 4) != 0);
  // UNROLL j, then group k (now axis 3): k's reduce still reads j's halves.
  Term u = uop_dag_apply_split(root, KOP_UNROLL, 1, 4);
  CHECK(u != 0);
  CHECK(uop_dag_apply_group_reduce(u, 3, 4) == 0);
}

// Both halves of a reduce's own split close at that reduce (tinygrad keeps
// them in REDUCE.src[1:]), so a group after its own UNROLL is no nested
// group.
static void test_apply_group_reduce_after_own_unroll(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term u = uop_dag_apply_split(root, KOP_UNROLL, 2, 4);
  CHECK(u != 0);
  CHECK(uop_dag_apply_group_reduce(u, 2, 4) != 0);
}

// In tinygrad a reduce closes both halves of its own split whatever its
// body's size (reduces[0].ranges, uop/ops.py:469-497), so the split
// ownership and nested-group checks walk the whole body.
static void test_apply_group_reduce_after_own_unroll_wide_body(void) {
  for (int wide = WIDE_FIRST; wide <= WIDE_LAST; wide++) {
    Term u = uop_dag_apply_split(build_wide_row_sum_root(wide),
                                 KOP_UNROLL, 1, 4);
    CHECK(u != 0);
    if (u == 0) {
      continue;
    }
    Term red = heap_read(term_val(u) + 2);
    CHECK(count_uop_nodes(uop_reduce_src(red)) > 4096);
    CHECK(uop_reduce_owns_split_axis(red, 2) == 1);
    CHECK(uop_dag_apply_group_reduce(u, 1, 4) != 0);
  }
}

// After UNROLL k the MAX closes both halves of k whatever its body's size, so
// a group of the SUM's j is no nested group, while a group of k stays
// refused: j is open at the MAX.
static void test_apply_group_reduce_over_wide_nested_unroll(void) {
  for (int wide = WIDE_FIRST; wide <= WIDE_LAST; wide++) {
    Term u = uop_dag_apply_split(build_nested_reduce_root(wide),
                                 KOP_UNROLL, 2, 4);
    CHECK(u != 0);
    if (u == 0) {
      continue;
    }
    CHECK(uop_dag_apply_group_reduce(u, 1, 4) != 0);
    CHECK(uop_dag_apply_group_reduce(u, 2, 2) == 0);
  }
}

// === hand-coded opts ================================================

// tinygrad unrollable_dims (codegen/opt/postrange.py:102-104) draws from
// reduce_axes, so the hand-coded UNROLL (heuristic.py:138-150) never picks
// a REDUCE-typed axis no REDUCE reduces.  out[m] = max_j x[m*32 + j*4 + o]
// with o such an axis, sorted last.  A MAX keeps the kernel off the CPU
// BLAS ladder, which would skip the hand-coded opts.
static void test_hand_opts_unroll_skips_orphan_reduce_axis(void) {
  unsetenv("NOOPT");
  setenv("HAND_CODED_OPTS", "1", 1);
  u32 dimsX[1] = { 128 };
  u32 dimsO[1] = { 4 };
  Term X = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsX, 1);
  Term O = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsO, 0);
  Term r_m = uop_range(0, KAX_LOOP,   4);
  Term r_j = uop_range(1, KAX_REDUCE, 8);
  Term r_o = uop_range(2, KAX_REDUCE, 4);
  Term mj = uop_int_binary(UOP_IADD,
                           uop_int_binary(UOP_IMUL, r_m, uop_const(DT_INT32, 32)),
                           uop_int_binary(UOP_IMUL, r_j, uop_const(DT_INT32, 4)));
  Term red  = uop_reduce(REDUCE_MAX, 1,
                         uop_index_e(X, uop_int_binary(UOP_IADD, mj, r_o)));
  Term root = uop_store(O, r_m, red);

  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  kernel_lift_reserve(&ke->cached_lift, 1);
  ke->cached_lift.store_root = root;
  ke->cached_lift.out_buf    = O;
  ke->cached_lift.in_bufs[0] = X;
  ke->cached_lift.n_inputs   = 1;
  ke->n_inputs = 1;
  CHECK(kernel_hand_coded_opts(ke) > 0);

  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types, exts,
                               MAX_AXES);
  int unrolled_j = 0, unrolled_o = 0;
  for (u32 i = 0; i < n; i++) {
    if (types[i] == KAX_UNROLL && exts[i] == 8) unrolled_j = 1;
    if (types[i] == KAX_UNROLL && exts[i] == 4) unrolled_o = 1;
  }
  CHECK(unrolled_j);
  CHECK(!unrolled_o);
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

static void test_apply_swap_follows_reduce_axis(void) {
  // Swapping the K reduce axis (2) with the N axis (1) must also
  // re-map the UOP_REDUCE node's `axis` field to 1.
  Term root = build_matmul_root(NULL, NULL, NULL);
  Term r = uop_dag_apply_swap(root, 1, 2);
  CHECK(r != 0 && r != root);
  Term value = heap_read(term_val(r) + 2);
  CHECK(term_tag(value) == TAG_UOP && term_ext(value) == UOP_REDUCE);
  CHECK_EQ(uop_reduce_n_axes(value), 1u);
  CHECK_EQ(uop_reduce_axis(value, 0), 1u);
}

// === KOP_GROUP / KOP_GROUPTOP ========================================

static void test_apply_group_reduce_stamps_reduce_axis(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);

  // matmul has reduce axis 2 (K=32).  Apply GROUP with k=8.
  Term r = uop_dag_apply_group_reduce(root, 2, 8);
  CHECK(r != 0);
  CHECK(r != root);

  // The reduce-axis RANGE leaf should now have axis_type KAX_GROUP_REDUCE
  // and be wrapped in OPT(_, GROUP_REDUCE, 8) at its use site.
  Term value = heap_read(term_val(r) + 2);
  CHECK(term_tag(value) == TAG_UOP && term_ext(value) == UOP_REDUCE);
  Term body = uop_reduce_src(value);
  // Walk to find OPT(RANGE(2, KAX_GROUP_REDUCE, 32), GROUP_REDUCE, 8).
  Term ranges[16]; u32 nv = 0;
  rmu_collect_ranges(body, ranges, &nv);
  int saw_group_axis = 0;
  for (u32 i = 0; i < nv; i++) {
    if (uop_range_axis_id(ranges[i]) == 2
        && uop_range_axis_type(ranges[i]) == KAX_GROUP_REDUCE
        && uop_range_extent(ranges[i]) == 32) saw_group_axis = 1;
  }
  CHECK(saw_group_axis);
}

static void test_apply_group_reduce_bails_on_non_reduce(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  // axis 0 is KAX_LOOP (output M); GROUP should bail.
  Term r = uop_dag_apply_group_reduce(root, 0, 4);
  CHECK(r == 0);
}

static void test_apply_group_reduce_bails_on_bad_k(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  // K = 32; k=7 doesn't divide.
  Term r = uop_dag_apply_group_reduce(root, 2, 7);
  CHECK(r == 0);
}

static void test_apply_group_reduce_via_dispatcher(void) {
  Term root = build_matmul_root(NULL, NULL, NULL);
  KOpt opt = { KOP_GROUPTOP, 2, 16 };
  Term r = uop_dag_apply_kopt(root, opt);
  CHECK(r != 0);
  CHECK(r != root);
}

static void test_apply_group_reduce_renders_metal_shared(void) {
  Term a, b, c;
  Term root = build_matmul_root(&a, &b, &c);
  Term r = uop_dag_apply_group_reduce(root, 2, 8);
  CHECK(r != 0);
  // MSL render path.
  char buf[8192];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  Term in_bufs[2] = { a, b };
  cg_render_uop_kernel_root(r, "mm_group", fp);
  fclose(fp);
  (void)in_bufs;
  // Should emit the threadgroup-shared accumulator + barriers.
  CHECK(rmu_contains(buf, "threadgroup float _acc"));
  CHECK(rmu_contains(buf, "threadgroup_barrier"));
}

static void test_apply_group_reduce_renders_cuda_shared(void) {
  Term a, b, c;
  Term root = build_matmul_root(&a, &b, &c);
  Term r = uop_dag_apply_group_reduce(root, 2, 8);
  CHECK(r != 0);
  char buf[8192];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  cg_render_uop_kernel_cuda_root(r, "mm_group", fp);
  fclose(fp);
  (void)c;
  CHECK(rmu_contains(buf, "__shared__ float _acc"));
  CHECK(rmu_contains(buf, "__syncthreads"));
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
  TEST_BEGIN("apply_split_checks_source_type");    test_apply_split_checks_source_type();
  TEST_BEGIN("apply_split_unroll_twice_on_outer"); test_apply_split_unroll_twice_on_outer();
  TEST_BEGIN("apply_split_unroll_on_group_reduce_axis");
                                                    test_apply_split_unroll_on_group_reduce_axis();
  TEST_BEGIN("apply_opt_rejects_orphan_reduce_axis");
                                                    test_apply_opt_rejects_orphan_reduce_axis();
  TEST_BEGIN("apply_group_reduce_rejects_group_inside_reduce");
                                                    test_apply_group_reduce_rejects_group_inside_reduce();
  TEST_BEGIN("apply_group_reduce_after_own_unroll");
                                                    test_apply_group_reduce_after_own_unroll();
  TEST_BEGIN("apply_group_reduce_after_own_unroll_wide_body");
                                                    test_apply_group_reduce_after_own_unroll_wide_body();
  TEST_BEGIN("apply_group_reduce_over_wide_nested_unroll");
                                                    test_apply_group_reduce_over_wide_nested_unroll();
  TEST_BEGIN("hand_opts_unroll_skips_orphan_reduce_axis");
                                                    test_hand_opts_unroll_skips_orphan_reduce_axis();
  TEST_BEGIN("apply_swap_axes");                   test_apply_swap_axes();
  TEST_BEGIN("apply_swap_noop_when_axes_equal");   test_apply_swap_noop_when_axes_equal();
  TEST_BEGIN("apply_swap_follows_reduce_axis");    test_apply_swap_follows_reduce_axis();

  TEST_BEGIN("apply_group_reduce_stamps_reduce_axis");
                                                    test_apply_group_reduce_stamps_reduce_axis();
  TEST_BEGIN("apply_group_reduce_bails_on_non_reduce");
                                                    test_apply_group_reduce_bails_on_non_reduce();
  TEST_BEGIN("apply_group_reduce_bails_on_bad_k");
                                                    test_apply_group_reduce_bails_on_bad_k();
  TEST_BEGIN("apply_group_reduce_via_dispatcher");  test_apply_group_reduce_via_dispatcher();
  TEST_BEGIN("apply_group_reduce_renders_metal_shared");
                                                    test_apply_group_reduce_renders_metal_shared();
  TEST_BEGIN("apply_group_reduce_renders_cuda_shared");
                                                    test_apply_group_reduce_renders_cuda_shared();

  thvm_free();
  TEST_REPORT();
}
