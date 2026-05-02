// test_scalar_graph.c -- Phase A scaffolding test for the scalar-UOp
// arena owned by KernelEntry.  Builds a tiny scalar graph by hand
// (no rangeify yet -- that lands in Phase B) and asserts:
//   - slot 0 is the S_NONE sentinel and stays that way
//   - emit assigns monotonically increasing ids starting at 1
//   - reserve grows the arena geometrically and zeros the tail
//   - rangeify_free leaves a clean slate that can be re-emitted into
//   - opname / axisname helpers cover every enum value
//
// The graph we build mirrors the tinygrad shape for `c[i] = a[i] + b[i]`
// over a rank-1 N-element tensor:
//   r0       = S_RANGE  (extent N, axis_type LOOP)
//   pa       = S_DEFINE_PARAM(slot 0, dtype f32)
//   pb       = S_DEFINE_PARAM(slot 1, dtype f32)
//   pc       = S_DEFINE_OUTPUT(dtype f32)
//   ia       = S_INDEX(pa, r0)
//   ib       = S_INDEX(pb, r0)
//   ic       = S_INDEX(pc, r0)
//   la       = S_LOAD(ia)
//   lb       = S_LOAD(ib)
//   sum      = S_ADD(la, lb)
//   sto      = S_STORE(ic, sum)
//   buf      = S_BUFFERIZE(sto, r0)

#include "../src/thvm.c"
#include "test.h"

static void run_jit_f32(KernelEntry *ke, const f32 *input, u32 input_count,
                        f32 *output, u32 output_count) {
  u32 in_buf  = cpu_buf_alloc((u64)input_count * sizeof(f32));
  u32 out_buf = cpu_buf_alloc((u64)output_count * sizeof(f32));
  CHECK_EQ(cpu_buf_write(in_buf, input, (u64)input_count * sizeof(f32)), 0);
  CHECK_EQ(cpu_buf_write(out_buf, output, (u64)output_count * sizeof(f32)), 0);
  u32 in_bufs[1] = {in_buf};
  cpu_jit_cache_reset();
  int jit_hit = 0;
  for (u32 attempt = 0; attempt < 8; attempt++) {
    if (cpu_jit_dispatch_scalar(ke, in_bufs, out_buf)) {
      jit_hit = 1;
    }
  }
  CHECK(jit_hit);
  CHECK_EQ(cpu_buf_read(out_buf, output, (u64)output_count * sizeof(f32)), 0);
}

int main(void) {
  thvm_init();

  // Allocate a real KernelEntry to host the arena (so we exercise
  // the kernel_alloc -> rangeify_emit path and confirm
  // kernel_free_arrays cleans it up via rangeify_free).
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  TEST_BEGIN("scalar-graph/initial-state");
  CHECK_EQ((unsigned long long)ke->scalar_uops, 0);
  CHECK_EQ(ke->n_scalar_uops, 0);
  CHECK_EQ(ke->scalar_uops_cap, 0);

  TEST_BEGIN("scalar-graph/first-emit-reserves-sentinel");
  // First emit: should allocate scalar_uops, install S_NONE at slot 0,
  // and place the new op at slot 1.
  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
              ((u64)S_AXIS_LOOP << 32) | (u64)128u);
  CHECK_EQ(r0, 1);
  CHECK_EQ(ke->n_scalar_uops, 2);            // sentinel + r0
  CHECK(ke->scalar_uops_cap >= 2);
  CHECK_EQ(ke->scalar_uops[0].op, S_NONE);
  CHECK_EQ(ke->scalar_uops[0].src_count, 0);
  CHECK_EQ(ke->scalar_uops[1].op, S_RANGE);
  CHECK_EQ(ke->scalar_uops[1].src_count, 0);
  CHECK_EQ(ke->scalar_uops[1].dtype, DT_INT32);
  CHECK_EQ((u32)(ke->scalar_uops[1].extra & 0xFFFFFFFFu), 128u);
  CHECK_EQ((u32)(ke->scalar_uops[1].extra >> 32),         (u32)S_AXIS_LOOP);

  TEST_BEGIN("scalar-graph/build-add-graph-ids-monotone");
  u32 pa  = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pb  = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 1);
  u32 pc  = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 ia  = rangeify_emit_binary(ke, S_INDEX, DT_FP32, pa, r0);
  u32 ib  = rangeify_emit_binary(ke, S_INDEX, DT_FP32, pb, r0);
  u32 ic  = rangeify_emit_binary(ke, S_INDEX, DT_FP32, pc, r0);
  u32 la  = rangeify_emit_unary (ke, S_LOAD,  DT_FP32, ia);
  u32 lb  = rangeify_emit_unary (ke, S_LOAD,  DT_FP32, ib);
  u32 sum = rangeify_emit_binary(ke, S_ADD,   DT_FP32, la, lb);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, sum);
  // BUFFERIZE: 2 sources (body, range).
  u32 buf_src[3] = {sto, r0, 0};
  u32 buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK_EQ(pa,  2);   CHECK_EQ(pb,  3);   CHECK_EQ(pc,  4);
  CHECK_EQ(ia,  5);   CHECK_EQ(ib,  6);   CHECK_EQ(ic,  7);
  CHECK_EQ(la,  8);   CHECK_EQ(lb,  9);   CHECK_EQ(sum, 10);
  CHECK_EQ(sto, 11);  CHECK_EQ(buf, 12);
  CHECK_EQ(ke->n_scalar_uops, 13);

  TEST_BEGIN("scalar-graph/structure-roundtrips");
  // Spot-check the structural fields land in scalar_uops[].
  CHECK_EQ(ke->scalar_uops[ia].op,        S_INDEX);
  CHECK_EQ(ke->scalar_uops[ia].src_count, 2);
  CHECK_EQ(ke->scalar_uops[ia].src[0],    pa);
  CHECK_EQ(ke->scalar_uops[ia].src[1],    r0);
  CHECK_EQ(ke->scalar_uops[ia].src[2],    0);  // unused, must be zero
  CHECK_EQ(ke->scalar_uops[buf].op,       S_BUFFERIZE);
  CHECK_EQ(ke->scalar_uops[buf].src_count,2);
  CHECK_EQ(ke->scalar_uops[buf].src[0],   sto);
  CHECK_EQ(ke->scalar_uops[buf].src[1],   r0);

  TEST_BEGIN("scalar-graph/dce-drops-unreachable-nodes");
  rangeify_free(ke);
  u32 d_r  = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
              ((u64)S_AXIS_LOOP << 32) | (u64)4u);
  u32 d_pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 dead = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x3F800000u);
  u32 dead_src[2] = {dead, d_r};
  rangeify_emit(ke, S_SHRINK, DT_FP32, 2, dead_src, 0);
  u32 live = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x40000000u);
  u32 d_idx = rangeify_emit_binary(ke, S_INDEX, DT_FP32, d_pc, d_r);
  u32 d_sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, d_idx, live);
  u32 d_buf_src[2] = {d_sto, d_r};
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, d_buf_src, 0);
  u32 before_dce = ke->n_scalar_uops;
  CHECK_EQ(rangeify_dce(ke), 2);
  CHECK_EQ(ke->n_scalar_uops, before_dce - 2);
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    CHECK(ke->scalar_uops[i].op != S_SHRINK);
  }

  TEST_BEGIN("scalar-graph/free-then-reemit");
  // rangeify_free wipes pointer + counts; subsequent emit should
  // start fresh at slot 1 again.
  rangeify_free(ke);
  CHECK_EQ((unsigned long long)ke->scalar_uops, 0);
  CHECK_EQ(ke->n_scalar_uops, 0);
  CHECK_EQ(ke->scalar_uops_cap, 0);
  u32 fresh = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x40400000u); // 3.0f bits
  CHECK_EQ(fresh, 1);
  CHECK_EQ(ke->scalar_uops[0].op, S_NONE);
  CHECK_EQ(ke->scalar_uops[1].op, S_CONST);
  CHECK_EQ((u32)ke->scalar_uops[1].extra, 0x40400000u);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/valid-mask-helpers-fold");
  u32 mi_zero = emit_iconst(ke, 0);
  u32 mi_one  = emit_iconst(ke, 1);
  u32 mi_mask = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                                   ((u64)S_AXIS_LOOP << 32) | 4u);
  u32 mi_yes  = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x3F800000u);
  u32 mi_no   = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x00000000u);
  CHECK_EQ(emit_ibinop(ke, S_IAND, mi_mask, mi_mask), mi_mask);
  CHECK_EQ(emit_ibinop(ke, S_IAND, mi_one,  mi_mask), mi_mask);
  CHECK_EQ(emit_ibinop(ke, S_IAND, mi_mask, mi_one),  mi_mask);
  CHECK_EQ(emit_ibinop(ke, S_IAND, mi_zero, mi_mask), mi_zero);
  CHECK_EQ(emit_ibinop(ke, S_IAND, mi_mask, mi_zero), mi_zero);
  CHECK_EQ(emit_iwhere(ke, DT_FP32, 0,       mi_yes, mi_no), mi_yes);
  CHECK_EQ(emit_iwhere(ke, DT_FP32, mi_one,  mi_yes, mi_no), mi_yes);
  CHECK_EQ(emit_iwhere(ke, DT_FP32, mi_zero, mi_yes, mi_no), mi_no);
  CHECK_EQ(emit_iwhere(ke, DT_FP32, mi_mask, mi_yes, mi_yes), mi_yes);
  u32 mi_where = emit_iwhere(ke, DT_FP32, mi_mask, mi_yes, mi_no);
  CHECK_EQ(ke->scalar_uops[mi_where].op, S_IWHERE);
  CHECK_EQ(ke->scalar_uops[mi_where].src[0], mi_mask);
  CHECK_EQ(ke->scalar_uops[mi_where].src[1], mi_yes);
  CHECK_EQ(ke->scalar_uops[mi_where].src[2], mi_no);
  u32 mi_mask2 = rangeify_emit_binary(ke, S_ILT, DT_INT64, mi_mask, mi_one);
  u32 mi_nested = emit_iwhere(ke, DT_FP32, mi_mask, mi_yes, mi_no);
  u32 mi_flat = emit_iwhere(ke, DT_FP32, mi_mask2, mi_nested, mi_no);
  CHECK_EQ(ke->scalar_uops[mi_flat].op, S_IWHERE);
  CHECK_EQ(ke->scalar_uops[ke->scalar_uops[mi_flat].src[0]].op, S_IAND);
  CHECK_EQ(ke->scalar_uops[mi_flat].src[1], mi_yes);
  CHECK_EQ(ke->scalar_uops[mi_flat].src[2], mi_no);
  u32 mi_hi_only = emit_pad_bounds_mask(ke, mi_mask, 0, 4);
  CHECK_EQ(ke->scalar_uops[mi_hi_only].op, S_ILT);
  CHECK_EQ(ke->scalar_uops[mi_hi_only].src[0], mi_mask);
  u32 mi_bounded = emit_pad_bounds_mask(ke, mi_mask, 1, 3);
  CHECK_EQ(ke->scalar_uops[mi_bounded].op, S_IAND);
  CHECK_EQ(ke->scalar_uops[ke->scalar_uops[mi_bounded].src[0]].op, S_ILT);
  CHECK_EQ(ke->scalar_uops[ke->scalar_uops[mi_bounded].src[1]].op, S_ISUB);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/integer-expression-folds");
  u32 si_r = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                                ((u64)S_AXIS_LOOP << 32) | 8u);
  u32 si_zero = emit_iconst(ke, 0);
  u32 si_one  = emit_iconst(ke, 1);
  u32 si_two  = emit_iconst(ke, 2);
  u32 si_six  = emit_iconst(ke, 6);
  CHECK_EQ(emit_ibinop(ke, S_IADD, si_r, si_zero), si_r);
  CHECK_EQ(emit_ibinop(ke, S_IADD, si_zero, si_r), si_r);
  CHECK_EQ(emit_ibinop(ke, S_ISUB, si_r, si_zero), si_r);
  CHECK_EQ(emit_ibinop(ke, S_IMUL, si_r, si_one), si_r);
  CHECK_EQ(emit_ibinop(ke, S_IMUL, si_one, si_r), si_r);
  CHECK_EQ(emit_ibinop(ke, S_IMUL, si_zero, si_r), si_zero);
  CHECK_EQ(emit_ibinop(ke, S_IDIV, si_r, si_one), si_r);
  u32 si_mod_one = emit_ibinop(ke, S_IMOD, si_r, si_one);
  i64 si_val = -1;
  CHECK(scalar_iconst_value(ke, si_mod_one, &si_val));
  CHECK_EQ((u64)si_val, 0);
  u32 si_const_add = emit_ibinop(ke, S_IADD, si_two, si_six);
  CHECK(scalar_iconst_value(ke, si_const_add, &si_val));
  CHECK_EQ((u64)si_val, 8);
  u32 si_const_lt = emit_ibinop(ke, S_ILT, si_two, si_six);
  CHECK(scalar_iconst_value(ke, si_const_lt, &si_val));
  CHECK_EQ((u64)si_val, 1);
  u32 si_self_lt = emit_ibinop(ke, S_ILT, si_r, si_r);
  CHECK(scalar_iconst_value(ke, si_self_lt, &si_val));
  CHECK_EQ((u64)si_val, 0);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/opname-helpers-cover-enum");
  // Every enum value should have a non-"?" name.  Belt-and-braces
  // against future additions where someone forgets to extend the
  // switch in scalar_op_name.
  for (u8 op = S_NONE; op < S__COUNT; op++) {
    const char *nm = scalar_op_name(op);
    CHECK(nm != NULL);
    CHECK(nm[0] == 'S' && nm[1] == '_');
  }
  CHECK_EQ((u64)scalar_axis_name(S_AXIS_LOOP)[0],   (u64)'L');
  CHECK_EQ((u64)scalar_axis_name(S_AXIS_REDUCE)[0], (u64)'R');
  CHECK_EQ((u64)scalar_axis_name(S_AXIS_UNROLL)[0], (u64)'U');
  CHECK_EQ((u64)scalar_axis_name(S_AXIS_GLOBAL)[0], (u64)'G');

  TEST_BEGIN("scalar-graph/grow-many-ops");
  // Push enough emits to exercise multiple realloc rounds.
  // SUOP_INIT_CAP = 16, so 100 emits should hit at least 4 grows.
  for (u32 i = 0; i < 100; i++) {
    u32 id = rangeify_emit_leaf(ke, S_CONST, DT_FP32, i);
    CHECK_EQ(id, i + 1);
  }
  CHECK_EQ(ke->n_scalar_uops, 101);
  CHECK(ke->scalar_uops_cap >= 101);
  // Spot-check the tail wasn't trampled.
  CHECK_EQ((u32)ke->scalar_uops[100].extra, 99u);
  CHECK_EQ(ke->scalar_uops[100].op, S_CONST);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-index-e-jit");
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 8;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 4;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 4u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 c_two   = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 2);
  u32 c_one   = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 1);
  u32 c_three = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 3);
  u32 mul2    = rangeify_emit_binary(ke, S_IMUL, DT_INT64, r0, c_two);
  u32 addr    = rangeify_emit_binary(ke, S_IADD, DT_INT64, mul2, c_one);
  u32 in_src[2] = {pa, addr};
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 cond = rangeify_emit_binary(ke, S_ILT, DT_INT64, r0, c_three);
  u32 zero = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 0);
  u32 where_src[3] = {cond, la, zero};
  u32 guarded = rangeify_emit(ke, S_IWHERE, DT_FP32, 3, where_src, 0);
  u32 out_src[2] = {pc, r0};
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, guarded);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  char *src = cg_emit_scalar(ke);
  CHECK(src != NULL);
  CHECK(strstr(src, "in0[") != NULL);
  CHECK(strstr(src, "int64_t") != NULL);
  CHECK(strstr(src, " ? ") != NULL);
  free(src);

  f32 src_vals[8] = {10.0f, 20.0f, 30.0f, 40.0f,
                     50.0f, 60.0f, 70.0f, 80.0f};
  f32 dst_vals[4] = {0};
  u32 in_buf = cpu_buf_alloc(sizeof(src_vals));
  u32 out_buf = cpu_buf_alloc(sizeof(dst_vals));
  CHECK_EQ(cpu_buf_write(in_buf, src_vals, sizeof(src_vals)), 0);
  CHECK_EQ(cpu_buf_write(out_buf, dst_vals, sizeof(dst_vals)), 0);
  u32 in_bufs[1] = {in_buf};
  cpu_jit_cache_reset();
  int jit_hit = 0;
  for (u32 attempt = 0; attempt < 8; attempt++) {
    if (cpu_jit_dispatch_scalar(ke, in_bufs, out_buf)) {
      jit_hit = 1;
    }
  }
  CHECK(jit_hit);
  CHECK_EQ(cpu_buf_read(out_buf, dst_vals, sizeof(dst_vals)), 0);
  CHECK(dst_vals[0] == 20.0f);
  CHECK(dst_vals[1] == 40.0f);
  CHECK(dst_vals[2] == 60.0f);
  CHECK(dst_vals[3] == 0.0f);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-reduce-sum-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 4;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 1;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 1u);
  u32 rr = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_REDUCE << 32) | 4u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 red_in_src[2] = {pa, rr};
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, red_in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 red_src[2] = {la, rr};
  u32 red = rangeify_emit(ke, S_REDUCE_SUM, DT_FP32, 2, red_src, 0);
  out_src[0] = pc;
  out_src[1] = r0;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, red);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  src = cg_emit_scalar(ke);
  CHECK(src != NULL);
  CHECK(strstr(src, "_acc") != NULL);
  CHECK(strstr(src, "for (unsigned _v") != NULL);
  free(src);

  f32 red_vals[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 red_out[1] = {0.0f};
  in_buf = cpu_buf_alloc(sizeof(red_vals));
  out_buf = cpu_buf_alloc(sizeof(red_out));
  CHECK_EQ(cpu_buf_write(in_buf, red_vals, sizeof(red_vals)), 0);
  CHECK_EQ(cpu_buf_write(out_buf, red_out, sizeof(red_out)), 0);
  in_bufs[0] = in_buf;
  cpu_jit_cache_reset();
  jit_hit = 0;
  for (u32 attempt = 0; attempt < 8; attempt++) {
    if (cpu_jit_dispatch_scalar(ke, in_bufs, out_buf)) {
      jit_hit = 1;
    }
  }
  CHECK(jit_hit);
  CHECK_EQ(cpu_buf_read(out_buf, red_out, sizeof(red_out)), 0);
  CHECK(red_out[0] == 10.0f);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-reduce-max-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 4;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 1;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 1u);
  rr = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_REDUCE << 32) | 4u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  red_in_src[0] = pa;
  red_in_src[1] = rr;
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, red_in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  red_src[0] = la;
  red_src[1] = rr;
  red = rangeify_emit(ke, S_REDUCE_MAX, DT_FP32, 2, red_src, 0);
  out_src[0] = pc;
  out_src[1] = r0;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, red);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  src = cg_emit_scalar(ke);
  CHECK(src != NULL);
  CHECK(strstr(src, "-INFINITY") != NULL);
  CHECK(strstr(src, "> _acc") != NULL);
  free(src);

  f32 max_vals[4] = {-3.0f, 7.0f, 5.0f, 2.0f};
  f32 max_out[1] = {0.0f};
  in_buf = cpu_buf_alloc(sizeof(max_vals));
  out_buf = cpu_buf_alloc(sizeof(max_out));
  CHECK_EQ(cpu_buf_write(in_buf, max_vals, sizeof(max_vals)), 0);
  CHECK_EQ(cpu_buf_write(out_buf, max_out, sizeof(max_out)), 0);
  in_bufs[0] = in_buf;
  cpu_jit_cache_reset();
  jit_hit = 0;
  for (u32 attempt = 0; attempt < 8; attempt++) {
    if (cpu_jit_dispatch_scalar(ke, in_bufs, out_buf)) {
      jit_hit = 1;
    }
  }
  CHECK(jit_hit);
  CHECK_EQ(cpu_buf_read(out_buf, max_out, sizeof(max_out)), 0);
  CHECK(max_out[0] == 7.0f);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-cast-f32-to-f64-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 3;
  ke->output_dtype    = DT_FP64;
  ke->output_numel    = 3;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 3u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP64, 0);
  in_src[0] = pa;
  in_src[1] = r0;
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 cast = rangeify_emit_unary(ke, S_CAST, DT_FP64, la);
  out_src[0] = pc;
  out_src[1] = r0;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP64, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP64, ic, cast);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP64, 2, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  src = cg_emit_scalar(ke);
  CHECK(src != NULL);
  CHECK(strstr(src, "double *out") != NULL);
  CHECK(strstr(src, "const float *in0") != NULL);
  CHECK(strstr(src, "((double)(") != NULL);
  free(src);

  f32 cast32_vals[3] = {1.25f, -2.5f, 3.75f};
  f64 cast64_out[3] = {0.0, 0.0, 0.0};
  in_buf = cpu_buf_alloc(sizeof(cast32_vals));
  out_buf = cpu_buf_alloc(sizeof(cast64_out));
  CHECK_EQ(cpu_buf_write(in_buf, cast32_vals, sizeof(cast32_vals)), 0);
  CHECK_EQ(cpu_buf_write(out_buf, cast64_out, sizeof(cast64_out)), 0);
  in_bufs[0] = in_buf;
  cpu_jit_cache_reset();
  jit_hit = 0;
  for (u32 attempt = 0; attempt < 8; attempt++) {
    if (cpu_jit_dispatch_scalar(ke, in_bufs, out_buf)) {
      jit_hit = 1;
    }
  }
  CHECK(jit_hit);
  CHECK_EQ(cpu_buf_read(out_buf, cast64_out, sizeof(cast64_out)), 0);
  CHECK(cast64_out[0] == 1.25);
  CHECK(cast64_out[1] == -2.5);
  CHECK(cast64_out[2] == 3.75);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-cast-f64-to-f32-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP64;
  ke->input_numels[0] = 3;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 3;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 3u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP64, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  in_src[0] = pa;
  in_src[1] = r0;
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP64, 2, in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP64, ia);
  cast = rangeify_emit_unary(ke, S_CAST, DT_FP32, la);
  out_src[0] = pc;
  out_src[1] = r0;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, cast);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  src = cg_emit_scalar(ke);
  CHECK(src != NULL);
  CHECK(strstr(src, "float *out") != NULL);
  CHECK(strstr(src, "const double *in0") != NULL);
  CHECK(strstr(src, "((float)(") != NULL);
  free(src);

  f64 cast64_vals[3] = {-8.0, 0.5, 16.0};
  f32 cast32_out[3] = {0.0f, 0.0f, 0.0f};
  in_buf = cpu_buf_alloc(sizeof(cast64_vals));
  out_buf = cpu_buf_alloc(sizeof(cast32_out));
  CHECK_EQ(cpu_buf_write(in_buf, cast64_vals, sizeof(cast64_vals)), 0);
  CHECK_EQ(cpu_buf_write(out_buf, cast32_out, sizeof(cast32_out)), 0);
  in_bufs[0] = in_buf;
  cpu_jit_cache_reset();
  jit_hit = 0;
  for (u32 attempt = 0; attempt < 8; attempt++) {
    if (cpu_jit_dispatch_scalar(ke, in_bufs, out_buf)) {
      jit_hit = 1;
    }
  }
  CHECK(jit_hit);
  CHECK_EQ(cpu_buf_read(out_buf, cast32_out, sizeof(cast32_out)), 0);
  CHECK(cast32_out[0] == -8.0f);
  CHECK(cast32_out[1] == 0.5f);
  CHECK(cast32_out[2] == 16.0f);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-shrink-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 5;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 3;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 3u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  in_src[0] = pa;
  in_src[1] = r0;
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 wrap_src[3] = {la, r0, 0};
  u32 shrink = rangeify_emit(ke, S_SHRINK, DT_FP32, 2, wrap_src, 1u);
  out_src[0] = pc;
  out_src[1] = r0;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, shrink);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  f32 move_in5[5] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
  f32 move_out3[3] = {0.0f, 0.0f, 0.0f};
  run_jit_f32(ke, move_in5, 5, move_out3, 3);
  CHECK(move_out3[0] == 20.0f);
  CHECK(move_out3[1] == 30.0f);
  CHECK(move_out3[2] == 40.0f);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-pad-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 3;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 5;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 5u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  in_src[0] = pa;
  in_src[1] = r0;
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  wrap_src[0] = la;
  wrap_src[1] = r0;
  u32 pad = rangeify_emit(ke, S_PAD, DT_FP32, 2, wrap_src,
                          1u | (3u << 8));
  out_src[0] = pc;
  out_src[1] = r0;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, pad);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  f32 pad_in[3] = {10.0f, 20.0f, 30.0f};
  f32 pad_out[5] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
  run_jit_f32(ke, pad_in, 3, pad_out, 5);
  CHECK(pad_out[0] == 0.0f);
  CHECK(pad_out[1] == 10.0f);
  CHECK(pad_out[2] == 20.0f);
  CHECK(pad_out[3] == 30.0f);
  CHECK(pad_out[4] == 0.0f);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-flip-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 4;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 4;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 4u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  in_src[0] = pa;
  in_src[1] = r0;
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  wrap_src[0] = la;
  wrap_src[1] = r0;
  u32 flip = rangeify_emit(ke, S_FLIP, DT_FP32, 2, wrap_src, 1u);
  out_src[0] = pc;
  out_src[1] = r0;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, flip);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  f32 flip_in[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 flip_out[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  run_jit_f32(ke, flip_in, 4, flip_out, 4);
  CHECK(flip_out[0] == 4.0f);
  CHECK(flip_out[1] == 3.0f);
  CHECK(flip_out[2] == 2.0f);
  CHECK(flip_out[3] == 1.0f);
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-reshape-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 6;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 6;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 3u);
  u32 r1 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | 2u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 c_three_i = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 3);
  u32 c_two_i   = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 2);
  u32 in_mul = rangeify_emit_binary(ke, S_IMUL, DT_INT64, r0, c_three_i);
  u32 in_addr = rangeify_emit_binary(ke, S_IADD, DT_INT64, in_mul, r1);
  in_src[0] = pa;
  in_src[1] = in_addr;
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  wrap_src[0] = la;
  wrap_src[1] = r0;
  wrap_src[2] = r1;
  u64 reshape_extra = 3ULL | (2ULL << 8) | (2ULL << 32) | (3ULL << 40);
  u32 reshape = rangeify_emit(ke, S_RESHAPE, DT_FP32, 3, wrap_src,
                              reshape_extra);
  u32 out_mul = rangeify_emit_binary(ke, S_IMUL, DT_INT64, r0, c_two_i);
  u32 out_addr = rangeify_emit_binary(ke, S_IADD, DT_INT64, out_mul, r1);
  out_src[0] = pc;
  out_src[1] = out_addr;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, reshape);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf_src[2] = r1;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 3, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  f32 reshape_in[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  f32 reshape_out[6] = {0};
  run_jit_f32(ke, reshape_in, 6, reshape_out, 6);
  for (u32 i = 0; i < 6; i++) {
    CHECK(reshape_out[i] == reshape_in[i]);
  }
  rangeify_free(ke);

  TEST_BEGIN("scalar-graph/c-renderer-reshape-v-jit");
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 6;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 6;
  r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 2u);
  r1 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                          ((u64)S_AXIS_LOOP << 32) | 3u);
  u32 rv = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_VIRT << 32) | 6u);
  pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  in_src[0] = pa;
  in_src[1] = rv;
  ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 rv_src[4] = {la, r0, r1, rv};
  reshape = rangeify_emit(ke, S_RESHAPE_V, DT_FP32, 4, rv_src, 2u);
  c_three_i = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 3);
  out_mul = rangeify_emit_binary(ke, S_IMUL, DT_INT64, r0, c_three_i);
  out_addr = rangeify_emit_binary(ke, S_IADD, DT_INT64, out_mul, r1);
  out_src[0] = pc;
  out_src[1] = out_addr;
  ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, reshape);
  buf_src[0] = sto;
  buf_src[1] = r0;
  buf_src[2] = r1;
  buf = rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 3, buf_src, 0);
  CHECK(buf != 0);
  CHECK(cg_supports_scalar(ke));
  f32 reshape_v_out[6] = {0};
  run_jit_f32(ke, reshape_in, 6, reshape_v_out, 6);
  for (u32 i = 0; i < 6; i++) {
    CHECK(reshape_v_out[i] == reshape_in[i]);
  }
  rangeify_free(ke);

  thvm_free();
  TEST_REPORT();
}
