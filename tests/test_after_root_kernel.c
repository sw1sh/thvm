// test_after_root_kernel.c -- multi-store (AFTER-root) kernels.
//
// A kernel whose root is an AFTER chain of STOREs writes its output in
// ordered passes (a scatter's init, accumulate and finish stores), and a
// gather or scatter address holds an INDEX_E of an index buffer.  Built by
// hand from existing ops:
//
//   1. a gather-shaped store with a nested INDEX_E address; invalid ids give
//      0 and unselected -inf/inf/NaN rows never leak;
//   2. AFTER(acc, init) scatter-sum with duplicate and invalid ids;
//   3. a three-store scatter-max with an exact -inf identity;
//   4. every materialize-time pass sees every store of an AFTER root: the
//      unified rewrite leaves no TEN/BUFFERIZE leaf, the arange collapse, the
//      chain-fold scan and the resid/stranded/broadcast gates;
//   5. renumbering sparse axis ids 17/40/41 gives one dense 0..n space;
//   6. kernel_lift_to_uop names the output buffer of an AFTER root;
//   7. uop_dag_has_dynamic_index, and no_opts closing every opt gate.
//
// Kernels 1-3 (and 5 after renumbering) run through kernel_harness.h: the
// rendered C and cpu_uop_walk must equal a host reference byte for byte.
#include "../src/thvm.c"
#include "test.h"
#include "kernel_harness.h"

// 1 iff a TAG_TEN or UOP_BUFFERIZE leaf is reachable from `t`, AFTER
// children included.
static int has_raw_leaf(Term t, u32 depth) {
  if (depth > 256) {
    return 0;
  }
  if (term_tag(t) == TAG_TEN) {
    return 1;
  }
  if (term_tag(t) != TAG_UOP) {
    return 0;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_BUFFERIZE) {
    return 1;
  }
  u8 ar = (op == UOP_AFTER) ? 2 : uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    if (has_raw_leaf(heap_read(loc + i), depth + 1)) {
      return 1;
    }
  }
  return 0;
}

// 1 iff a UOP_REDUCE is reachable from `t`, AFTER children included.
static int has_reduce(Term t, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) {
    return 0;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) {
    return 1;
  }
  u8 ar = (op == UOP_AFTER) ? 2 : uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    if (has_reduce(heap_read(loc + i), depth + 1)) {
      return 1;
    }
  }
  return 0;
}

// Distinct RANGE axis ids reachable from `t` (AFTER children included),
// ascending.  Returns the count.
static u32 range_ids(Term t, u32 *ids, u32 n, u32 cap, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) {
    return n;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 a = uop_range_axis_id(t);
    u32 i = 0;
    while (i < n && ids[i] < a) {
      i++;
    }
    if ((i < n && ids[i] == a) || n >= cap) {
      return n;
    }
    for (u32 j = n; j > i; j--) {
      ids[j] = ids[j - 1];
    }
    ids[i] = a;
    return n + 1;
  }
  u8 ar = (op == UOP_AFTER) ? 2 : uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    n = range_ids(heap_read(loc + i), ids, n, cap, depth + 1);
  }
  return n;
}

// valid(iv) = 0 <= iv < n, the in-range test of a data index.
static Term in_range(Term iv, i32 n) {
  return uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, kh_i32(-1), iv),
                        uop_int_binary(UOP_ILT, iv, kh_i32(n)));
}

static Term lin2(Term a, i32 stride, Term b) {
  return uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, a, kh_i32(stride)), b);
}

// === 1. gather ===
#define GV 5
#define GD 3
#define GN 8
static i32 const GIDX[GN] = { 2, -1, 0, GV, 3, 10982377, 1, -19238 };

// out[n, d] = valid(idx[n]) ? src[idx[n], d] : 0, with the row clamped to 0
// when invalid so the guarded load stays in bounds.
static Term gather_root(KhKernel *k, f32 const *src) {
  Term sb = kh_input(k, DT_FP32, GV * GD, src);
  Term ib = kh_input(k, DT_INT32, GN, GIDX);
  Term n = uop_range(0, KAX_LOOP, GN);
  Term d = uop_range(1, KAX_LOOP, GD);
  Term iv    = uop_index_e(ib, n);
  Term valid = in_range(iv, GV);
  Term row   = uop_iwhere(valid, iv, kh_i32(0));
  Term val   = uop_iwhere(valid, uop_index_e(sb, lin2(row, GD, d)), uop_invalid());
  CHECK_EQ(term_ext(val), UOP_IWHERE);
  return uop_store(k->out, lin2(n, GD, d), val);
}

static void test_gather_store(void) {
  f32 src[GV * GD];
  for (u32 q = 0; q < GV * GD; q++) {
    src[q] = 0.25f * (f32)q - 1.5f;
  }
  src[4 * GD + 0] = -INFINITY;     // row 4 is never selected
  src[4 * GD + 1] = INFINITY;
  src[4 * GD + 2] = NAN;
  KhKernel k;
  kh_init(&k, DT_FP32, GN * GD);
  Term root = gather_root(&k, src);
  f32 want[GN * GD];
  for (u32 r = 0; r < GN; r++) {
    for (u32 c = 0; c < GD; c++) {
      int ok = GIDX[r] >= 0 && GIDX[r] < GV;
      want[r * GD + c] = ok ? src[GIDX[r] * GD + c] : 0.0f;
    }
  }
  kh_check(root, &k, want, "gather");
}

// === 2. scatter-sum ===
#define SV 4
#define SD 2
#define SE 6
static i32 const SIDX[SE] = { 1, 3, 1, -1, SV, 0 };
static f32 const SSRC[SE * SD] = { 0.5f, 1.25f, -2.0f, 0.75f, 0.125f, 3.5f,
                                   9.0f, 9.0f, 7.0f, 7.0f, -0.25f, 1.5f };

// OUT = 0, then OUT[idx[e], d] += src[e, d] for each valid e, in e order.
// The RANGE axis ids come from `aid` (init v, init d, acc e, acc d).
static Term scatter_sum_root(KhKernel *k, u32 const *aid) {
  Term ib = kh_input(k, DT_INT32, SE, SIDX);
  Term sb = kh_input(k, DT_FP32, SE * SD, SSRC);
  Term v  = uop_range(aid[0], KAX_LOOP, SV);
  Term vd = uop_range(aid[1], KAX_LOOP, SD);
  Term e  = uop_range(aid[2], KAX_LOOP, SE);
  Term ed = uop_range(aid[3], KAX_LOOP, SD);
  Term init   = uop_store(k->out, lin2(v, SD, vd), kh_f32(0.0f));
  Term iv     = uop_index_e(ib, e);
  Term valid  = in_range(iv, SV);
  Term t_addr = lin2(uop_iwhere(valid, iv, kh_i32(0)), SD, ed);
  Term old    = uop_index_e(k->out, t_addr);
  Term sv     = uop_index_e(sb, lin2(e, SD, ed));
  Term upd    = uop_iwhere(valid, uop_binary(UOP_ADD, old, sv), old);
  return uop_after(uop_store(k->out, t_addr, upd), init);
}

static void scatter_sum_want(f32 *want) {
  for (u32 q = 0; q < SV * SD; q++) {
    want[q] = 0.0f;
  }
  for (u32 q = 0; q < SE; q++) {
    if (SIDX[q] < 0 || SIDX[q] >= SV) {
      continue;
    }
    for (u32 c = 0; c < SD; c++) {
      want[SIDX[q] * SD + c] += SSRC[q * SD + c];
    }
  }
}

static void test_scatter_sum(void) {
  static u32 const aid[4] = { 0, 1, 2, 3 };
  KhKernel k;
  kh_init(&k, DT_FP32, SV * SD);
  Term root = scatter_sum_root(&k, aid);
  CHECK_EQ(term_ext(root), UOP_AFTER);
  f32 want[SV * SD];
  scatter_sum_want(want);
  kh_check(root, &k, want, "scatter-sum");
}

// === 3. three-store scatter-max: S = max over hits (identity -inf), then
// OUT = max(S, base) ===
#define MV 5
#define ME 8
static void test_scatter_max_three_stores(void) {
  static i32 const idx[ME] = { 0, 2, 0, 3, -1, 2, MV, 0 };
  static f32 const src[ME] = { 1.25f, -INFINITY, 1.75f, -0.5f, 9.0f, -INFINITY,
                               8.0f, 1.5f };
  static f32 const base[MV] = { 1.5f, -3.0f, -1.0f, -0.75f, -INFINITY };
  KhKernel k;
  kh_init(&k, DT_FP32, MV);
  Term ib = kh_input(&k, DT_INT32, ME, idx);
  Term sb = kh_input(&k, DT_FP32, ME, src);
  Term bb = kh_input(&k, DT_FP32, MV, base);
  Term v  = uop_range(0, KAX_LOOP, MV);
  Term e  = uop_range(1, KAX_LOOP, ME);
  Term w  = uop_range(2, KAX_LOOP, MV);
  Term ninf = uop_const(DT_FP32, 0xFF800000u);
  Term init  = uop_store(k.out, v, ninf);
  Term iv    = uop_index_e(ib, e);
  Term valid = in_range(iv, MV);
  Term vsafe = uop_iwhere(valid, iv, kh_i32(0));
  Term old   = uop_index_e(k.out, vsafe);
  Term sv    = uop_index_e(sb, e);
  Term acc   = uop_store(k.out, vsafe,
                         uop_iwhere(valid, uop_iwhere(uop_binary(UOP_CMPLT, old, sv), sv, old),
                                    old));
  Term s     = uop_index_e(k.out, w);
  Term bw    = uop_index_e(bb, w);
  Term fin   = uop_store(k.out, w, uop_iwhere(uop_binary(UOP_CMPLT, s, bw), bw, s));
  Term root  = uop_after(fin, uop_after(acc, init));
  f32 want[MV];
  for (u32 j = 0; j < MV; j++) {
    want[j] = -INFINITY;
  }
  for (u32 q = 0; q < ME; q++) {
    if (idx[q] >= 0 && idx[q] < MV && want[idx[q]] < src[q]) {
      want[idx[q]] = src[q];
    }
  }
  for (u32 j = 0; j < MV; j++) {
    want[j] = (want[j] < base[j]) ? base[j] : want[j];
  }
  u32 bits;
  memcpy(&bits, &want[4], sizeof bits);
  CHECK_EQ(bits, 0xFF800000u);
  kh_check(root, &k, want, "scatter-max");
}

// === 4. every materialize-time pass sees every store ===
static Term ten_of(u32 tid) {
  return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
}

static void test_unified_rewrite_every_store(void) {
  u32 idx_tid = kh_tensor(DT_INT32, SE, SIDX);
  u32 src_tid = kh_tensor(DT_FP32, SE * SD, SSRC);
  u32 base_tid = kh_tensor(DT_FP32, SV * SD, NULL);
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  u32 odims[1] = { SV * SD };
  Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims, 0);
  Term v  = uop_range(0, KAX_LOOP, SV * SD);
  Term e  = uop_range(1, KAX_LOOP, SE);
  Term ed = uop_range(2, KAX_LOOP, SD);
  Term init   = uop_store(out, v, uop_index_e(ten_of(base_tid), v));
  Term iv     = uop_index_e(ten_of(idx_tid), e);
  Term valid  = in_range(iv, SV);
  Term t_addr = lin2(uop_iwhere(valid, iv, kh_i32(0)), SD, ed);
  Term old    = uop_index_e(out, t_addr);
  Term two    = uop_index_e(uop_bufferize_new(kh_f32(2.0f), UOP_SCOPE_GLOBAL, 1, 0, NULL),
                            kh_i32(0));
  Term sv     = uop_binary(UOP_MUL, uop_index_e(ten_of(src_tid), lin2(e, SD, ed)), two);
  Term root   = uop_after(uop_store(out, t_addr,
                                    uop_iwhere(valid, uop_binary(UOP_ADD, old, sv), old)),
                          init);
  CHECK(has_raw_leaf(root, 0));
  Term rw = unified_store_root_for_walker(ke, root);
  CHECK_EQ(term_ext(rw), UOP_AFTER);
  CHECK(!has_raw_leaf(rw, 0));
  CHECK_EQ(ke->n_inputs, 3u);
  kernel_dealloc_last(kid);
}

static void test_gates_see_every_store(void) {
  u32 odims[1] = { 8 };
  u32 idims[1] = { 8 };
  u32 sdims[1] = { 2 };
  Term out   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims, 0);
  Term in8   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, idims, 1);
  Term in2   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, sdims, 1);
  Term a0    = uop_range(0, KAX_LOOP, 8);
  Term a5    = uop_range(5, KAX_LOOP, 8);
  Term first = uop_store(out, a0, kh_f32(0.0f));
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  // A value RANGE that neither the address nor a REDUCE binds.
  Term stranded = uop_after(uop_store(out, a0, uop_index_e(in8, a5)), first);
  CHECK(uop_subtree_has_stranded_range(stranded));
  Term clean = uop_after(uop_store(out, a0, uop_index_e(in8, a0)), first);
  CHECK(!uop_subtree_has_stranded_range(clean));
  // A BUFFERIZE the rewrite could not resolve.
  Term bz    = uop_bufferize_new(uop_index_e(in8, a0), UOP_SCOPE_GLOBAL, 1, 1, &a0);
  Term resid = uop_after(uop_store(out, a0, uop_index_e(bz, a0)), first);
  CHECK(uop_subtree_has_residual_bufferize(resid));
  CHECK(!uop_subtree_has_residual_bufferize(clean));
  // A read past a 2-element input.
  Term bcast = uop_after(uop_store(out, a0, uop_index_e(in2, a0)), first);
  CHECK(uop_subtree_has_broadcast_input(ke, bcast));
  CHECK(!uop_subtree_has_broadcast_input(ke, clean));
  // The chain-fold scan flags a strided input slot read by the second store.
  f32 data[16] = {0};
  u32 tid = kh_tensor(DT_FP32, 16, data);
  TENS[tid].view.offset = 2;
  u32 slot = input_slot_dedup(ke, tid, ten_of(tid));
  CHECK_EQ(slot, 0u);
  ChainFoldMarks marks;
  unified_fold_chain(ke, clean, &marks);
  CHECK(marks.n_slots >= 1 && marks.flagged[0] == 1);
  unified_fold_chain_commit_flags(ke, &marks);
  CHECK_EQ(ke->input_chain_composed[0], 1);
  kernel_dealloc_last(kid);
  // The arange collapse folds a triangular-mask SUM in the later store.
  Term r0  = uop_range(0, KAX_LOOP, 8);
  Term r1  = uop_range(1, KAX_LOOP, 8);
  Term tri = uop_reduce(REDUCE_SUM, 1,
                        uop_iwhere(uop_int_binary(UOP_ILT, r1, r0), kh_i32(1), kh_i32(0)));
  Term ar  = uop_after(uop_store(out, r0, tri), uop_store(out, r0, kh_i32(0)));
  CHECK(has_reduce(ar, 0));
  CHECK(!has_reduce(uop_reduce_arange_collapse(ar), 0));
}

// === 5. dense axis ids across stores ===
static void test_renumber_every_store(void) {
  static u32 const aid[4] = { 17, 40, 41, 42 };
  KhKernel k;
  kh_init(&k, DT_FP32, SV * SD);
  Term root = scatter_sum_root(&k, aid);
  Term dense = uop_dag_renumber_axes(root);
  u32 ids[8];
  u32 n = range_ids(dense, ids, 0, 8, 0);
  CHECK_EQ(n, 4u);
  for (u32 i = 0; i < n; i++) {
    CHECK_EQ(ids[i], i);
  }
  f32 want[SV * SD];
  scatter_sum_want(want);
  kh_check(dense, &k, want, "scatter-sum-renumbered");
}

// === 6. the lift names an AFTER root's output buffer ===
static void test_lift_out_buf(void) {
  static u32 const aid[4] = { 0, 1, 2, 3 };
  KhKernel k;
  kh_init(&k, DT_FP32, SV * SD);
  Term root = scatter_sum_root(&k, aid);
  CHECK_EQ(uop_root_store_buf(root), k.out);
  Term src_uop = uop_unary(UOP_NEG, ten_of(kh_tensor(DT_FP32, SV * SD, NULL)));
  bufferize_info_clear();
  u32 idx = bufferize_node_get_or_add(term_val(src_uop), UOP_NEG);
  CHECK(idx != 0xFFFFFFFFu);
  RU_STORE_ROOT[idx] = root;
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->source_uop = src_uop;
  KernelUopLift lift;
  memset(&lift, 0, sizeof lift);
  CHECK(kernel_lift_to_uop(ke, &lift));
  CHECK_EQ(lift.store_root, root);
  CHECK_EQ(lift.out_buf, k.out);
  kernel_dealloc_last(kid);
  RU_STORE_ROOT[idx] = 0;
  bufferize_info_clear();
}

// === 7. dynamic addresses and the no_opts gate ===
static void test_dynamic_index(void) {
  f32 src[GV * GD] = {0};
  KhKernel g;
  kh_init(&g, DT_FP32, GN * GD);
  CHECK(uop_dag_has_dynamic_index(gather_root(&g, src)));
  static u32 const aid[4] = { 0, 1, 2, 3 };
  KhKernel sk;
  kh_init(&sk, DT_FP32, SV * SD);
  Term scatter = scatter_sum_root(&sk, aid);
  CHECK(uop_dag_has_dynamic_index(scatter));
  // Only the accumulate store has a data-dependent address.
  CHECK(uop_dag_has_dynamic_index(uop_after_node(scatter)));
  CHECK(!uop_dag_has_dynamic_index(uop_after_after_node(scatter)));
  // Loads in the value at affine addresses are not dynamic.
  KhKernel ek;
  kh_init(&ek, DT_FP32, SE);
  Term a  = kh_input(&ek, DT_FP32, SE, SSRC);
  Term b  = kh_input(&ek, DT_FP32, SE, SSRC);
  Term i  = uop_range(0, KAX_LOOP, SE);
  Term ew = uop_store(ek.out, i, uop_binary(UOP_MUL, uop_index_e(a, i), uop_index_e(b, i)));
  CHECK(!uop_dag_has_dynamic_index(ew));
}

static Term ten_add(u32 n) {
  f32 data[64];
  for (u32 q = 0; q < n; q++) {
    data[q] = (f32)q;
  }
  return uop_binary(UOP_ADD, ten_of(kh_tensor(DT_FP32, n, data)),
                    ten_of(kh_tensor(DT_FP32, n, data)));
}

static void test_no_opts_closes_every_gate(void) {
  thvm_materialize(ten_add(64));
  KernelEntry *ke = &KERNELS[KERNELS_NEXT - 1];
  CHECK(ke->cached_lift.store_root != 0);
  CHECK_EQ(ke->no_opts, 0);
  CHECK(kernel_should_hand_code_opts(ke));
  setenv("BEAM", "1", 1);
  CHECK(kernel_should_autotune(ke));
  ke->no_opts = 1;
  CHECK(!kernel_should_hand_code_opts(ke));
  CHECK(!kernel_should_autotune(ke));
  unsetenv("BEAM");
  Term before = ke->cached_lift.store_root;
  KOpt up = { KOP_UPCAST, 0, 4 };
  CHECK(!kernel_apply_opt(ke, up));
  CHECK_EQ(ke->cached_lift.store_root, before);
  ke->no_opts = 0;
  CHECK(kernel_apply_opt(ke, up));
}

int main(void) {
  thvm_init();
  TEST_BEGIN("after-root/gather-store");
  test_gather_store();
  TEST_BEGIN("after-root/scatter-sum");
  test_scatter_sum();
  TEST_BEGIN("after-root/scatter-max-three-stores");
  test_scatter_max_three_stores();
  TEST_BEGIN("after-root/unified-rewrite-every-store");
  test_unified_rewrite_every_store();
  TEST_BEGIN("after-root/gates-see-every-store");
  test_gates_see_every_store();
  TEST_BEGIN("after-root/renumber-every-store");
  test_renumber_every_store();
  TEST_BEGIN("after-root/lift-out-buf");
  test_lift_out_buf();
  TEST_BEGIN("after-root/dynamic-index");
  test_dynamic_index();
  TEST_BEGIN("after-root/no-opts-closes-every-gate");
  test_no_opts_closes_every_gate();
  thvm_free();
  TEST_REPORT();
}
