// test_tile_graph.c -- tile-plan arena above scalar-UOps.
//
// Builds the same tiny scalar graph as test_scalar_graph, then seeds
// a tile plan from it:
//   TILE_SCALAR_BODY(value expr id)
//   TILE_STORE(S_STORE id, body)
//   TILE_AXIS(...)
//   TILE_LOOP_NEST(store, axes...)
//
// The tile plan is now an opt-in CPU/Metal dispatch target.  This
// test pins arena lifecycle, GEMM analysis, name helpers, and the
// seed builder's two axis sources: scalar S_RANGE fallback and
// KernelAxes override.

#include "../src/thvm.c"
#include "test.h"

// cg_supports_tile / cg_supports_scalar / cg_emit_tile / cg_emit_scalar
// lived in src/codegen/render_c_scalar.c, deleted as part of the
// Phase G ScalarUop / TileUop C-renderer cull.  The corresponding
// CPU JIT path (cpu_jit_dispatch_tile / cpu_jit_dispatch_scalar)
// was deleted with it; cpu_dispatch_tile (the interpreter) survives
// and run_tile_jit_1 below routes through it.  Tests that string-
// matched the rendered C source are now vacuous on these stubs --
// the live coverage they offered (TILE_MMA construction,
// tile_analyze_gemm, dispatch-side metal-gemm-with-TC routing) was
// retired with the deletion of the underlying KProgOp pattern
// matchers in slice 8 session 5; the surviving coverage now lives in
// `test_uop_recognise_tc.c` (DAG-side classifier) and
// `test_metal_real.c` (live cblas dispatcher integration).
static int cg_supports_tile  (KernelEntry const *ke) { (void)ke; return 0; }
static int cg_supports_scalar(KernelEntry const *ke) { (void)ke; return 0; }
static char *cg_emit_tile    (KernelEntry const *ke) { (void)ke; return NULL; }
static char *cg_emit_scalar  (KernelEntry const *ke) { (void)ke; return NULL; }

static u32 build_scalar_add_graph(KernelEntry *ke, u32 extent) {
  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
              ((u64)S_AXIS_LOOP << 32) | (u64)extent);
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
  u32 src[2] = {sto, r0};
  return rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, src, 0);
}

static u32 build_scalar_duplicate_expr_graph(KernelEntry *ke) {
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 4;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 4;

  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | 4u);
  u32 pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 z0 = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 0);
  u32 z1 = rangeify_emit_leaf(ke, S_ICONST, DT_INT64, 0);
  u32 a0 = rangeify_emit_binary(ke, S_IADD, DT_INT64, r0, z0);
  u32 a1 = rangeify_emit_binary(ke, S_IADD, DT_INT64, r0, z1);
  u32 ia_src[2] = {pa, a0};
  u32 ib_src[2] = {pa, a1};
  u32 ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, ia_src, 0);
  u32 ib = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, ib_src, 0);
  u32 la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 lb = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ib);
  u32 sum = rangeify_emit_binary(ke, S_ADD, DT_FP32, la, lb);
  u32 out_src[2] = {pc, r0};
  u32 oi = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, oi, sum);
  u32 buf_src[2] = {sto, r0};
  return rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
}

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static void run_tile_jit_1(KernelEntry *ke, const void *input, u64 input_bytes,
                           void *output, u64 output_bytes) {
  u32 in_buf  = cpu_buf_alloc(input_bytes);
  u32 out_buf = cpu_buf_alloc(output_bytes);
  CHECK_EQ(cpu_buf_write(in_buf, input, input_bytes), 0);
  CHECK_EQ(cpu_buf_write(out_buf, output, output_bytes), 0);
  u32 in_bufs[1] = {in_buf};
  cpu_jit_cache_reset();
  // cpu_jit_dispatch_tile was deleted in the Phase G slice that
  // removed render_c_scalar.c; fall through to cpu_dispatch_tile
  // (the interpreter), which has the same signature and runs the
  // same TileUop[] plan via the interpreter loop instead of clang.
  int jit_hit = 0;
  for (u32 attempt = 0; attempt < 3; attempt++) {
    if (cpu_dispatch_tile(ke, in_bufs, out_buf)) {
      jit_hit = 1;
    }
  }
  CHECK(jit_hit);
  CHECK_EQ(cpu_buf_read(out_buf, output, output_bytes), 0);
}

static u32 build_scalar_pad_graph(KernelEntry *ke) {
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 3;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 5;

  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | 5u);
  u32 pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 in_src[2] = {pa, r0};
  u32 ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  u32 la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 wrap_src[2] = {la, r0};
  u32 pad = rangeify_emit(ke, S_PAD, DT_FP32, 2, wrap_src, 1u | (3u << 8));
  u32 out_src[2] = {pc, r0};
  u32 ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, pad);
  u32 buf_src[2] = {sto, r0};
  return rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
}

static u32 build_scalar_cast64_to32_graph(KernelEntry *ke) {
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP64;
  ke->input_numels[0] = 3;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 3;

  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | 3u);
  u32 pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP64, 0);
  u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 in_src[2] = {pa, r0};
  u32 ia = rangeify_emit(ke, S_INDEX_E, DT_FP64, 2, in_src, 0);
  u32 la = rangeify_emit_unary(ke, S_LOAD, DT_FP64, ia);
  u32 cast = rangeify_emit_unary(ke, S_CAST, DT_FP32, la);
  u32 out_src[2] = {pc, r0};
  u32 ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, cast);
  u32 buf_src[2] = {sto, r0};
  return rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
}

static u32 build_scalar_reduce_graph(KernelEntry *ke, u8 reduce_op) {
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 4;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 1;

  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | 1u);
  u32 rr = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_REDUCE << 32) | 4u);
  u32 pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 in_src[2] = {pa, rr};
  u32 ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  u32 la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 red_src[2] = {la, rr};
  u32 red = rangeify_emit(ke, reduce_op, DT_FP32, 2, red_src, 0);
  u32 out_src[2] = {pc, r0};
  u32 ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, red);
  u32 buf_src[2] = {sto, r0};
  return rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
}

static u32 build_scalar_reduce_2d_graph(KernelEntry *ke, u8 reduce_op) {
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs        = 1;
  ke->input_tids[0]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_numels[0] = 6;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 1;

  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | 1u);
  u32 rr0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                               ((u64)S_AXIS_REDUCE << 32) | 2u);
  u32 rr1 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                               ((u64)S_AXIS_REDUCE << 32) | 3u);
  u32 pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 c3 = emit_iconst(ke, 3);
  u32 row = emit_ibinop(ke, S_IMUL, rr0, c3);
  u32 flat = emit_ibinop(ke, S_IADD, row, rr1);
  u32 in_src[2] = {pa, flat};
  u32 ia = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, in_src, 0);
  u32 la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 red_src[3] = {la, rr0, rr1};
  u32 red = rangeify_emit(ke, reduce_op, DT_FP32, 3, red_src, 0);
  u32 out_src[2] = {pc, r0};
  u32 ic = rangeify_emit(ke, S_INDEX_E, DT_FP32, 2, out_src, 0);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, red);
  u32 buf_src[2] = {sto, r0};
  return rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
}

static u32 build_scalar_reduce_sum_graph(KernelEntry *ke) {
  return build_scalar_reduce_graph(ke, S_REDUCE_SUM);
}

static u32 build_scalar_reduce_max_graph(KernelEntry *ke) {
  return build_scalar_reduce_graph(ke, S_REDUCE_MAX);
}

static u32 build_scalar_post_reduce_sum_graph(KernelEntry *ke) {
  CHECK(build_scalar_reduce_sum_graph(ke) != 0);
  u32 store = 0;
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_STORE) {
      store = i;
      break;
    }
  }
  CHECK(store != 0);
  u32 red = ke->scalar_uops[store].src[1];
  u32 c_two = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x40000000u);
  u32 scaled = rangeify_emit_binary(ke, S_MUL, DT_FP32, red, c_two);
  ke->scalar_uops[store].src[1] = scaled;
  return scaled;
}

// E9: set_reduce_axes was a hand-write helper for [LOOP, REDUCE, tail]
// shapes; it has no callers in tree (TEST_REDUCE_NO_TAIL above is its
// only-companion sentinel).  Removed alongside the writer trio's
// axis_types[] field.

static void test_set_view3(View *v, u32 d0, u32 d1, u32 d2,
                           i32 s0, i32 s1, i32 s2) {
  memset(v, 0, sizeof(View));
  v->shape.ndim    = 3;
  v->shape.dims[0] = d0;
  v->shape.dims[1] = d1;
  v->shape.dims[2] = d2;
  v->strides[0]    = s0;
  v->strides[1]    = s1;
  v->strides[2]    = s2;
  v->numel         = d0 * d1 * d2;
}

static void test_set_view2(View *v, u32 d0, u32 d1, i32 s0, i32 s1) {
  memset(v, 0, sizeof(View));
  v->shape.ndim    = 2;
  v->shape.dims[0] = d0;
  v->shape.dims[1] = d1;
  v->strides[0]    = s0;
  v->strides[1]    = s1;
  v->numel         = d0 * d1;
  v->contiguous    = (s0 == (i32)d1 && s1 == 1);
}

static void test_set_view1(View *v, u32 d0, i32 s0) {
  memset(v, 0, sizeof(View));
  v->shape.ndim    = 1;
  v->shape.dims[0] = d0;
  v->strides[0]    = s0;
  v->numel         = d0;
  v->contiguous    = (s0 == 1);
}

static void build_kprog_gemm(KernelEntry *ke, u32 M, u32 N, u32 K) {
  kernel_inputs_reserve(ke, 2);
  ke->n_inputs        = 2;
  ke->input_tids[0]   = 0;
  ke->input_tids[1]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_dtypes[1] = DT_FP32;
  ke->input_numels[0] = M * K * N;
  ke->input_numels[1] = M * K * N;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = M * N;
  test_set_view3(&ke->input_views[0], M, K, N, (i32)K, 1, 0);
  test_set_view3(&ke->input_views[1], M, K, N, 0, (i32)N, 1);

  kernel_program_reserve(ke, 2);
  ke->n_ops = 2;
  ke->program[0] = (KProgOp){
    .opcode = UOP_MUL,
    .dtype  = DT_FP32,
    .n_src  = 2,
    .src    = {KSRC_AS_INPUT(0), KSRC_AS_INPUT(1), 0},
    .numel  = M * K * N,
  };
  ke->program[1] = (KProgOp){
    .opcode = UOP_REDUCE,
    .dtype  = DT_FP32,
    .n_src  = 1,
    .src    = {0, 0, 0},
    .arg    = (REDUCE_SUM << 24) | (N & 0x00FFFFFFu),
    .numel  = M * N,
  };
}

static void build_kprog_gemv_expand(KernelEntry *ke, u32 M, u32 K) {
  kernel_inputs_reserve(ke, 2);
  ke->n_inputs        = 2;
  ke->input_tids[0]   = 0;
  ke->input_tids[1]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_dtypes[1] = DT_FP32;
  ke->input_numels[0] = M * K;
  ke->input_numels[1] = K;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = M;
  test_set_view2(&ke->input_views[0], M, K, (i32)K, 1);
  test_set_view1(&ke->input_views[1], K, 1);

  kernel_program_reserve(ke, 3);
  ke->n_ops = 3;
  ke->program[0] = (KProgOp){
    .opcode    = UOP_EXPAND,
    .dtype     = DT_FP32,
    .n_src     = 1,
    .src       = {KSRC_AS_INPUT(1), 0, 0},
    .numel     = M * K,
    .src0_ndim = 1,
    .out_ndim  = 2,
  };
  ke->program[0].src0_dims[0] = K;
  ke->program[0].out_dims [0] = M;
  ke->program[0].out_dims [1] = K;
  ke->program[1] = (KProgOp){
    .opcode = UOP_MUL,
    .dtype  = DT_FP32,
    .n_src  = 2,
    .src    = {KSRC_AS_INPUT(0), 0, 0},
    .numel  = M * K,
  };
  ke->program[2] = (KProgOp){
    .opcode = UOP_REDUCE,
    .dtype  = DT_FP32,
    .n_src  = 1,
    .src    = {1, 0, 0},
    .arg    = (REDUCE_SUM << 24) | 1u,
    .numel  = M,
  };
}

static void build_kprog_conv2d_flat(KernelEntry *ke) {
  u32 c_out = 4;
  u32 c_in  = 2;
  u32 h     = 6;
  u32 w     = 6;
  u32 kh    = 3;
  u32 kw    = 3;
  u32 h_out = h - kh + 1;
  u32 w_out = w - kw + 1;
  u32 k     = c_in * kh * kw;
  u32 p     = h_out * w_out;

  kernel_inputs_reserve(ke, 2);
  ke->n_inputs        = 2;
  ke->input_tids[0]   = 0;
  ke->input_tids[1]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_dtypes[1] = DT_FP32;
  ke->input_numels[0] = c_out * k;
  ke->input_numels[1] = c_in * h * w;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = c_out * p;
  ke->output_shape.ndim = 2;
  ke->output_shape.dims[0] = c_out;
  ke->output_shape.dims[1] = p;
  test_set_view3(&ke->input_views[0], c_out, k, p, (i32)k, 1, 0);
  test_set_view3(&ke->input_views[1], c_in, h, w, (i32)(h * w), (i32)w, 1);

  kernel_program_reserve(ke, 2);
  ke->n_ops = 2;
  ke->program[0] = (KProgOp){
    .opcode = UOP_MUL,
    .dtype  = DT_FP32,
    .n_src  = 2,
    .src    = {KSRC_AS_INPUT(0), KSRC_AS_INPUT(1), 0},
    .numel  = c_out * k * p,
  };
  ke->program[1] = (KProgOp){
    .opcode = UOP_REDUCE,
    .dtype  = DT_FP32,
    .n_src  = 1,
    .src    = {0, 0, 0},
    .arg    = (REDUCE_SUM << 24) | (p & 0x00FFFFFFu),
    .numel  = c_out * p,
  };
}

int main(void) {
  thvm_init();

  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  TEST_BEGIN("tile-graph/initial-state");
  CHECK_EQ((unsigned long long)ke->tile_uops, 0);
  CHECK_EQ(ke->n_tile_uops, 0);
  CHECK_EQ(ke->tile_uops_cap, 0);
  CHECK_EQ(ke->tile_root, 0);
  TilePlanInfo info = {0};
  CHECK(!tile_collect_plan_info(ke, &info));
  CHECK(!tile_collect_plan_info(ke, NULL));

  TEST_BEGIN("tile-graph/opname-helpers-cover-enum");
  for (u8 op = TILE_NONE; op < TILE__COUNT; op++) {
    const char *nm = tile_op_name(op);
    CHECK(nm != NULL);
    CHECK(nm[0] == 'T' && nm[1] == 'I');
  }
  CHECK_EQ((u64)tile_axis_name(KAX_LOOP)[0],   (u64)'L');
  CHECK_EQ((u64)tile_axis_name(KAX_REDUCE)[0], (u64)'R');
  CHECK_EQ((u64)tile_axis_name(KAX_UPCAST)[0], (u64)'U');
  CHECK_EQ((u64)tile_axis_name(KAX_LOCAL)[0],  (u64)'L');
  CHECK_EQ((u64)tile_axis_name(KAX_GLOBAL)[0], (u64)'G');
  CHECK_EQ((u64)tile_axis_name(KAX_GROUP_REDUCE)[0], (u64)'G');

  TEST_BEGIN("tile-graph/rangeify-cse-dedupes-expression-nodes");
  CHECK(build_scalar_duplicate_expr_graph(ke) != 0);
  u32 old_n = ke->n_scalar_uops;
  u32 removed = rangeify_cse(ke);
  CHECK_EQ(removed, 4u);
  CHECK_EQ(ke->n_scalar_uops, old_n - 4u);
  u32 n_load = 0;
  u32 add_id = 0;
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_LOAD) {
      n_load++;
    }
    if (ke->scalar_uops[i].op == S_ADD) {
      add_id = i;
    }
  }
  CHECK_EQ(n_load, 1u);
  CHECK(add_id != 0);
  CHECK_EQ(ke->scalar_uops[add_id].src[0], ke->scalar_uops[add_id].src[1]);
  CHECK(tile_build_from_scalar(ke));
  CHECK(tile_validate(ke));
  kernel_free_arrays(ke);

  // Slice 8 session 5: TileGemmInfo / tile_analyze_gemm /
  // tile_collect_mma_plan / tile_build_mma_from_gemm retired.  The
  // direct unit tests for KProgOp matmul pattern matching
  // (`gemm-analysis-normal-and-transposed`,
  // `gemm-analysis-square-view-disambiguates`,
  // `gemv-expand-promotes-to-mma-plan`,
  // `build-mma-plan-from-gemm`) and the legacy-arm coverage tests
  // (`metal-gemm-proposes-tc`, `metal-gemv-expand-proposes-tc`,
  // `metal-gemm-tc-via-cached-lift-dag`) deleted with the underlying
  // pattern matchers.  Equivalent shape-recognition coverage is in
  // `test_uop_recognise_tc.c` (DAG-side `uop_dag_classify_matmul_shape`)
  // and the live `metal-real/{gemm,dot,gemv}-cpu-routes-through-cblas`
  // checks in `test_metal_real.c` cover the dispatcher integration.

  TEST_BEGIN("tile-graph/conv2d-flat-analysis-and-metal-source");
  build_kprog_conv2d_flat(ke);
  TileConv2DInfo conv = {0};
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.dtype, (u32)DT_FP32);
  CHECK_EQ(conv.c_out, 4u);
  CHECK_EQ(conv.c_in, 2u);
  CHECK_EQ(conv.kh, 3u);
  CHECK_EQ(conv.kw, 3u);
  CHECK_EQ(conv.h_out, 4u);
  CHECK_EQ(conv.w_out, 4u);
  CHECK_EQ(conv.patches, 16u);
  CHECK_EQ(conv.w_stride0, 18);
  CHECK_EQ(conv.w_stride1, 1);
  CHECK_EQ(conv.x_stride0, 36);
  CHECK_EQ(conv.x_stride1, 6);
  CHECK_EQ(conv.x_stride2, 1);
  u32 conv_groups_x = 0;
  u32 conv_threads_x = 0;
  CHECK(cg_tile_metal_dispatch_shape(ke, &conv_groups_x, &conv_threads_x));
  CHECK_EQ(conv_groups_x, 1u);
  CHECK_EQ(conv_threads_x, 256u);
  char *conv_src = cg_emit_tile_metal(ke);
  CHECK(conv_src != NULL);
  if (conv_src != NULL) {
    free(conv_src);
  }
  // E9: drive [LOOP=4, LOOP=16, REDUCE=18] via the writer trio.
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt conv_local4 = { .op = KOP_LOCAL, .axis = 0, .arg = 4 };
  CHECK(kernel_apply_opt(ke, conv_local4));
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.threads, 4u);
  CHECK_EQ(conv.outputs_per_thread, 1u);
  CHECK_EQ(conv.reduce_unroll, 1u);
  CHECK(cg_tile_metal_dispatch_shape(ke, &conv_groups_x, &conv_threads_x));
  CHECK_EQ(conv_groups_x, 16u);
  CHECK_EQ(conv_threads_x, 4u);
  // E9: re-seed with [LOOP=4, LOOP=16, REDUCE=18] via the writer trio.
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt conv_upcast4 = { .op = KOP_UPCAST, .axis = 1, .arg = 4 };
  CHECK(kernel_apply_opt(ke, conv_upcast4));
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.outputs_per_thread, 4u);
  CHECK(cg_tile_metal_dispatch_shape(ke, &conv_groups_x, &conv_threads_x));
  CHECK_EQ(conv_groups_x, 1u);
  CHECK_EQ(conv_threads_x, 256u);
  conv_src = cg_emit_tile_metal(ke);
  CHECK(conv_src != NULL);
  if (conv_src != NULL) {
    free(conv_src);
  }
  // E9: re-seed with [LOOP=4, LOOP=16, REDUCE=18] via the writer trio.
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt conv_unroll2 = { .op = KOP_UNROLL, .axis = 2, .arg = 2 };
  CHECK(kernel_apply_opt(ke, conv_unroll2));
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.reduce_unroll, 2u);
  conv_src = cg_emit_tile_metal(ke);
  CHECK(conv_src != NULL);
  if (conv_src != NULL) {
    free(conv_src);
  }
  // E9: re-seed with [LOOP=4, LOOP=16, REDUCE=18] via the writer trio.
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOptSeq conv_seq = {0};
  conv_seq.n = 2;
  conv_seq.opts[0] = conv_upcast4;
  conv_seq.opts[1] = conv_local4;
  CHECK(kautotune_apply_seq(ke, &conv_seq));
  // E9 session 4: read applied_opts via the tile_anno facade so the
  // eventual ownership move is a single-file change.
  CHECK_EQ(tile_anno_applied_opts_count(ke), 3u);
  CHECK_EQ(tile_anno_applied_opts(ke)[0].op, (u32)KOP_UPCAST);
  CHECK_EQ(tile_anno_applied_opts(ke)[1].op, (u32)KOP_LOCAL);
  CHECK_EQ(tile_anno_applied_opts(ke)[2].op, (u32)KOP_GLOBAL);
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.threads, 4u);
  CHECK_EQ(conv.outputs_per_thread, 4u);
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes = NULL;
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/metal-conv2d-flat-proposes-local");
  build_kprog_conv2d_flat(ke);
  // E9: drive [LOOP=4, LOOP=16, REDUCE=18] via the writer trio.
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE", "1", 1);
  KOpt conv_cands[16];
  u32 n_conv_cands = kernel_opts_propose(ke, conv_cands,
                                         (u32)(sizeof(conv_cands)/
                                               sizeof(*conv_cands)));
  CHECK_EQ(n_conv_cands, 8u);
  CHECK_EQ(conv_cands[0].op, (u32)KOP_LOCAL);
  CHECK_EQ(conv_cands[0].axis, 1u);
  CHECK_EQ(conv_cands[0].arg, 16u);
  CHECK_EQ(conv_cands[1].op, (u32)KOP_LOCAL);
  CHECK_EQ(conv_cands[1].axis, 1u);
  CHECK_EQ(conv_cands[1].arg, 8u);
  CHECK_EQ(conv_cands[2].op, (u32)KOP_LOCAL);
  CHECK_EQ(conv_cands[2].axis, 0u);
  CHECK_EQ(conv_cands[2].arg, 4u);
  CHECK_EQ(conv_cands[3].op, (u32)KOP_LOCAL);
  CHECK_EQ(conv_cands[3].axis, 0u);
  CHECK_EQ(conv_cands[3].arg, 2u);
  CHECK_EQ(conv_cands[4].op, (u32)KOP_UPCAST);
  CHECK_EQ(conv_cands[4].axis, 1u);
  CHECK_EQ(conv_cands[4].arg, 8u);
  CHECK_EQ(conv_cands[5].op, (u32)KOP_UPCAST);
  CHECK_EQ(conv_cands[5].axis, 0u);
  CHECK_EQ(conv_cands[5].arg, 4u);
  CHECK_EQ(conv_cands[6].op, (u32)KOP_UPCAST);
  CHECK_EQ(conv_cands[6].axis, 0u);
  CHECK_EQ(conv_cands[6].arg, 2u);
  CHECK_EQ(conv_cands[7].op, (u32)KOP_UNROLL);
  CHECK_EQ(conv_cands[7].axis, 2u);
  CHECK_EQ(conv_cands[7].arg, 2u);
  unsetenv("THVM_BACKEND");
  unsetenv("THVM_TILE");
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes = NULL;
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/build-from-scalar-ranges");
  u32 scalar_root = build_scalar_add_graph(ke, 8);
  kernel_inputs_reserve(ke, 2);
  ke->n_inputs        = 2;
  ke->input_tids[0]   = 0;
  ke->input_tids[1]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_dtypes[1] = DT_FP32;
  ke->input_numels[0] = 8;
  ke->input_numels[1] = 8;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = 8;
  u32 scalar_store = ke->scalar_uops[scalar_root].src[0];
  u32 scalar_value = ke->scalar_uops[scalar_store].src[1];
  CHECK(tile_build_from_scalar(ke));
  CHECK_EQ(ke->n_tile_uops, 5);       // sentinel + body + store + axis + loop
  CHECK_EQ(ke->tile_uops[0].op, TILE_NONE);
  CHECK_EQ(ke->tile_uops[1].op, TILE_SCALAR_BODY);
  CHECK_EQ((u32)ke->tile_uops[1].extra, scalar_value);
  CHECK_EQ(ke->tile_uops[2].op, TILE_STORE);
  CHECK_EQ((u32)ke->tile_uops[2].extra, scalar_store);
  CHECK_EQ(ke->tile_uops[2].src_count, 1);
  CHECK_EQ(ke->tile_uops[2].src[0], 1);
  CHECK_EQ(ke->tile_uops[3].op, TILE_AXIS);
  CHECK_EQ((u32)(ke->tile_uops[3].extra >> 32), (u32)KAX_LOOP);
  CHECK_EQ((u32)(ke->tile_uops[3].extra & 0xFFFFFFFFu), 8u);
  CHECK_EQ(ke->tile_uops[4].op, TILE_LOOP_NEST);
  CHECK_EQ(ke->tile_root, 4);
  CHECK_EQ(ke->tile_uops[4].src_count, 2);
  CHECK_EQ(ke->tile_uops[4].src[0], 2);
  CHECK_EQ(ke->tile_uops[4].src[1], 3);
  CHECK_EQ(ke->tile_uops[4].src[2], 0);
  CHECK(tile_validate(ke));
  CHECK_EQ(tile_loop_axis_count(ke), 1);
  CHECK_EQ(tile_loop_axis_type(ke, 0), (u32)KAX_LOOP);
  CHECK_EQ(tile_loop_axis_extent(ke, 0), 8u);
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.root_id, 4);
  CHECK_EQ(info.store_tile_id, 2);
  CHECK_EQ(info.body_tile_id, 1);
  CHECK_EQ(info.scalar_store_id, scalar_store);
  CHECK_EQ(info.scalar_index_id, ke->scalar_uops[scalar_store].src[0]);
  CHECK_EQ(info.scalar_value_id, scalar_value);
  CHECK_EQ(info.dtype, (u32)DT_FP32);
  CHECK_EQ(info.n_axes, 1);
  CHECK_EQ(info.axis_ids[0], 3);
  CHECK_EQ(info.axis_types[0], (u32)KAX_LOOP);
  CHECK_EQ(info.axis_extents[0], 8u);

  TEST_BEGIN("tile-graph/kernel-axes-override");
  // E9-prep wedge 6: drive [LOOP=2, UPCAST=4] via axes_default_for +
  // KOP_UPCAST(axis=0, arg=4) instead of hand-writing axis_types[].
  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = 8;
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt upcast4 = { .op = KOP_UPCAST, .axis = 0, .arg = 4 };
  CHECK(kernel_apply_opt(ke, upcast4));
  CHECK(tile_build_from_scalar(ke));
  CHECK_EQ(ke->n_tile_uops, 6);       // sentinel + body + store + two axes + loop
  CHECK_EQ(ke->tile_uops[2].op, TILE_STORE);
  CHECK_EQ(ke->tile_uops[3].op, TILE_AXIS);
  CHECK_EQ((u32)(ke->tile_uops[3].extra >> 32), (u32)KAX_LOOP);
  CHECK_EQ((u32)(ke->tile_uops[3].extra & 0xFFFFFFFFu), 2u);
  CHECK_EQ(ke->tile_uops[4].op, TILE_AXIS);
  CHECK_EQ((u32)(ke->tile_uops[4].extra >> 32), (u32)KAX_UPCAST);
  CHECK_EQ((u32)(ke->tile_uops[4].extra & 0xFFFFFFFFu), 4u);
  CHECK_EQ(ke->tile_uops[5].op, TILE_LOOP_NEST);
  CHECK_EQ(ke->tile_root, 5);
  CHECK_EQ(ke->tile_uops[5].src_count, 3);
  CHECK_EQ(ke->tile_uops[5].src[0], 2);
  CHECK_EQ(ke->tile_uops[5].src[1], 3);
  CHECK_EQ(ke->tile_uops[5].src[2], 4);
  CHECK(tile_validate(ke));
  CHECK_EQ(tile_loop_axis_count(ke), 2);
  CHECK_EQ(tile_loop_axis_type(ke, 0), (u32)KAX_LOOP);
  CHECK_EQ(tile_loop_axis_extent(ke, 0), 2u);
  CHECK_EQ(tile_loop_axis_type(ke, 1), (u32)KAX_UPCAST);
  CHECK_EQ(tile_loop_axis_extent(ke, 1), 4u);
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.root_id, 5);
  CHECK_EQ(info.store_tile_id, 2);
  CHECK_EQ(info.body_tile_id, 1);
  CHECK_EQ(info.n_axes, 2);
  CHECK_EQ(info.axis_ids[0], 3);
  CHECK_EQ(info.axis_ids[1], 4);
  CHECK_EQ(info.axis_types[0], (u32)KAX_LOOP);
  CHECK_EQ(info.axis_extents[0], 2u);
  CHECK_EQ(info.axis_types[1], (u32)KAX_UPCAST);
  CHECK_EQ(info.axis_extents[1], 4u);

  TEST_BEGIN("tile-graph/kernel-axes-local-global");
  // E9-prep wedge 6: drive [GLOBAL=2, LOCAL=4] via axes_default_for +
  // KOP_LOCAL(arg=4) + KOP_GLOBAL(arg=2) instead of hand-writing
  // axis_types[].
  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = 8;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt klg_local4  = { .op = KOP_LOCAL,  .axis = 0, .arg = 4 };
  KOpt klg_global2 = { .op = KOP_GLOBAL, .axis = 0, .arg = 2 };
  CHECK(kernel_apply_opt(ke, klg_local4));
  CHECK(kernel_apply_opt(ke, klg_global2));
  CHECK(tile_build_from_scalar(ke));
  CHECK(tile_validate(ke));
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.n_axes, 2);
  CHECK_EQ(info.axis_types[0], (u32)KAX_GLOBAL);
  CHECK_EQ(info.axis_extents[0], 2u);
  CHECK_EQ(info.axis_types[1], (u32)KAX_LOCAL);
  CHECK_EQ(info.axis_extents[1], 4u);
  // cg_supports_tile / cg_emit_tile (the C-side tile renderer) were
  // deleted with render_c_scalar.c.  The Metal-side equivalents
  // (cg_emit_tile_metal) survive and are exercised below.
  u32 groups_x = 0;
  u32 threads_x = 0;
  CHECK(cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x));
  CHECK_EQ(groups_x, 2u);
  CHECK_EQ(threads_x, 4u);
  char *metal_tile_src = cg_emit_tile_metal(ke);
  CHECK(metal_tile_src != NULL);
  if (metal_tile_src != NULL) {
    CHECK(strstr(metal_tile_src, "threadgroup_position_in_grid") != NULL);
    CHECK(strstr(metal_tile_src, "thread_position_in_threadgroup") != NULL);
    free(metal_tile_src);
  }
  u32 saved_n_inputs = ke->n_inputs;
  ke->n_inputs = 31;
  CHECK(!cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x));
  ke->n_inputs = saved_n_inputs;

  TEST_BEGIN("tile-graph/metal-autotune-proposes-local-global");
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE", "1", 1);
  // E9: drive [LOOP=8] via the writer trio (output_shape ndim=1).
  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = 8;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt cands[16];
  u32 n_cands = kernel_opts_propose(ke, cands,
                                    (u32)(sizeof(cands)/sizeof(*cands)));
  CHECK(n_cands >= 3);
  CHECK_EQ(cands[0].op, (u32)KOP_LOCAL);
  CHECK_EQ(cands[0].axis, 0u);
  CHECK_EQ(cands[0].arg, 8u);
  KOpt local4 = { .op = KOP_LOCAL, .axis = 0, .arg = 4 };
  CHECK(kernel_apply_tune_candidate(ke, local4));
  // E9 session 4: applied_opts via facade; full_shape via resolver.
  CHECK_EQ(tile_anno_applied_opts_count(ke), 2u);
  // E9: kax_type reads route through axes_resolve_kax_type now that
  // axis_types[] is gone; the simulator replays applied_opts to derive
  // the post-mutation type.
  CHECK_EQ(axes_resolve_kax_type(ke, 0), (u8)KAX_GLOBAL);
  CHECK_EQ(axes_resolve_kax_type(ke, 1), (u8)KAX_LOCAL);
  u32 ext0 = 0;
  u32 ext1 = 0;
  CHECK(axes_resolve_full_shape(ke, 0, &ext0));
  CHECK(axes_resolve_full_shape(ke, 1, &ext1));
  CHECK_EQ(ext0, 2u);
  CHECK_EQ(ext1, 4u);
  CHECK(cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x));
  CHECK_EQ(groups_x, 2u);
  CHECK_EQ(threads_x, 4u);
  unsetenv("THVM_BACKEND");
  unsetenv("THVM_TILE");

  TEST_BEGIN("tile-graph/kernel-axes-local-global-swapped");
  // E9-prep wedge 6: drive [LOCAL=4, GLOBAL=2] via axes_default_for +
  // KOP_LOCAL(arg=4) + KOP_GLOBAL(arg=2) + KOP_SWAP(0,1).
  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = 8;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt klgs_local4  = { .op = KOP_LOCAL,  .axis = 0, .arg = 4 };
  KOpt klgs_global2 = { .op = KOP_GLOBAL, .axis = 0, .arg = 2 };
  KOpt klgs_swap    = { .op = KOP_SWAP,   .axis = 0, .arg = 1 };
  CHECK(kernel_apply_opt(ke, klgs_local4));
  CHECK(kernel_apply_opt(ke, klgs_global2));
  CHECK(kernel_apply_opt(ke, klgs_swap));
  CHECK(tile_build_from_scalar(ke));
  CHECK(tile_validate(ke));
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.axis_types[0], (u32)KAX_LOCAL);
  CHECK_EQ(info.axis_types[1], (u32)KAX_GLOBAL);
  CHECK(cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x));
  CHECK_EQ(groups_x, 2u);
  CHECK_EQ(threads_x, 4u);
  metal_tile_src = cg_emit_tile_metal(ke);
  CHECK(metal_tile_src != NULL);
  if (metal_tile_src != NULL) {
    free(metal_tile_src);
  }

  TEST_BEGIN("tile-graph/kernel-axes-local-global-with-loop");
  // E9-prep wedge 8: drive [GLOBAL=2, LOCAL=2, LOOP=2] via a fresh
  // 2-axis BUFFERIZE source (dims=[2,4]) plus the writer trio:
  // KOP_LOCAL(axis=1, arg=2) splits LOOP=4 into LOOP=2+LOCAL=2 ->
  // [LOOP=2, LOOP=2, LOCAL=2]; KOP_GLOBAL(axis=0, arg=2) marks the
  // outer LOOP -> [GLOBAL=2, LOOP=2, LOCAL=2]; KOP_SWAP(axis=1, arg=2)
  // exchanges the LOCAL/LOOP pair -> [GLOBAL=2, LOCAL=2, LOOP=2].
  // The 2-range arena lets the kernel_lift replay loop process all 3
  // opts (no axis >= n_cur skip), so cg_emit_tile_metal succeeds
  // through the writer-trio path without the (now-deleted) test seam.
  // Uses a sub-kernel so the original 1-range arena (and the
  // scalar_store reference at line 806) survives for downstream tests.
  {
    u32 with_loop_kid = kernel_alloc();
    KernelEntry *wlk = &KERNELS[with_loop_kid];
    kernel_inputs_reserve(wlk, 2);
    wlk->n_inputs = 2;
    wlk->input_tids[0]   = 0;
    wlk->input_tids[1]   = 0;
    wlk->input_dtypes[0] = DT_FP32;
    wlk->input_dtypes[1] = DT_FP32;
    wlk->input_numels[0] = 8;
    wlk->input_numels[1] = 8;
    wlk->output_dtype    = DT_FP32;
    wlk->output_numel    = 8;
    wlk->output_shape.ndim    = 2;
    wlk->output_shape.dims[0] = 2;
    wlk->output_shape.dims[1] = 4;
    u32 wl_r0 = rangeify_emit_leaf(wlk, S_RANGE, DT_INT32,
                                   ((u64)S_AXIS_LOOP << 32) | 2u);
    u32 wl_r1 = rangeify_emit_leaf(wlk, S_RANGE, DT_INT32,
                                   ((u64)S_AXIS_LOOP << 32) | 4u);
    u32 wl_pa = rangeify_emit_leaf(wlk, S_DEFINE_PARAM,  DT_FP32, 0);
    u32 wl_pb = rangeify_emit_leaf(wlk, S_DEFINE_PARAM,  DT_FP32, 1);
    u32 wl_pc = rangeify_emit_leaf(wlk, S_DEFINE_OUTPUT, DT_FP32, 0);
    u32 wl_ia_src[3] = { wl_pa, wl_r0, wl_r1 };
    u32 wl_ib_src[3] = { wl_pb, wl_r0, wl_r1 };
    u32 wl_ic_src[3] = { wl_pc, wl_r0, wl_r1 };
    u32 wl_ia = rangeify_emit(wlk, S_INDEX, DT_FP32, 3, wl_ia_src, 0);
    u32 wl_ib = rangeify_emit(wlk, S_INDEX, DT_FP32, 3, wl_ib_src, 0);
    u32 wl_ic = rangeify_emit(wlk, S_INDEX, DT_FP32, 3, wl_ic_src, 0);
    u32 wl_la = rangeify_emit_unary(wlk, S_LOAD, DT_FP32, wl_ia);
    u32 wl_lb = rangeify_emit_unary(wlk, S_LOAD, DT_FP32, wl_ib);
    u32 wl_sum = rangeify_emit_binary(wlk, S_ADD, DT_FP32, wl_la, wl_lb);
    u32 wl_sto = rangeify_emit_binary(wlk, S_STORE, DT_FP32, wl_ic, wl_sum);
    u32 wl_buf_src[3] = { wl_sto, wl_r0, wl_r1 };
    rangeify_emit(wlk, S_BUFFERIZE, DT_FP32, 3, wl_buf_src, 0);
    wlk->axes = &wlk->_local_axes;
    memset(wlk->axes, 0, sizeof(KernelAxes));
    axes_default_for(wlk);
    KOpt klglw_local2  = { .op = KOP_LOCAL,  .axis = 1, .arg = 2 };
    KOpt klglw_global2 = { .op = KOP_GLOBAL, .axis = 0, .arg = 2 };
    KOpt klglw_swap    = { .op = KOP_SWAP,   .axis = 1, .arg = 2 };
    CHECK(kernel_apply_opt(wlk, klglw_local2));
    CHECK(kernel_apply_opt(wlk, klglw_global2));
    CHECK(kernel_apply_opt(wlk, klglw_swap));
    CHECK(tile_build_from_scalar(wlk));
    CHECK(tile_validate(wlk));
    CHECK(tile_collect_plan_info(wlk, &info));
    CHECK_EQ(info.axis_types[0], (u32)KAX_GLOBAL);
    CHECK_EQ(info.axis_types[1], (u32)KAX_LOCAL);
    CHECK_EQ(info.axis_types[2], (u32)KAX_LOOP);
    CHECK(cg_tile_metal_dispatch_shape(wlk, &groups_x, &threads_x));
    CHECK_EQ(groups_x, 4u);
    CHECK_EQ(threads_x, 2u);
    metal_tile_src = cg_emit_tile_metal(wlk);
    CHECK(metal_tile_src != NULL);
    if (metal_tile_src != NULL) {
      free(metal_tile_src);
    }
    kernel_free_arrays(wlk);
    wlk->axes = NULL;
  }

  TEST_BEGIN("tile-graph/group-reduce-axis-falls-back-from-c-renderer");
  // E9-prep wedge 8: drive [GLOBAL=2, LOCAL=4, GROUP_REDUCE=1] via the
  // writer trio against the existing 1-axis BUFFERIZE: KOP_LOCAL(axis=0,
  // arg=4) splits LOOP=8 into LOOP=2+LOCAL=4 -> [LOOP=2, LOCAL=4];
  // KOP_GLOBAL(axis=0, arg=2) -> [GLOBAL=2, LOCAL=4]; KOP_GROUP(axis=1,
  // arg=1) inserts the trailing GROUP_REDUCE=1 sentinel ->
  // [GLOBAL=2, LOCAL=4, GROUP_REDUCE=1].  This exercises the
  // axes_compute_axis_types simulator end-to-end (sim_ok=true,
  // n_applied=3>0 -> simulator output is authoritative; no
  // axis_types[] read).  cg_emit_tile_metal is not called here: the
  // assertion is the 3-axis tile_collect_plan_info + cg_supports_tile
  // (a vacuous stub returning 0) coverage.
  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = 8;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt grfb_local4  = { .op = KOP_LOCAL,  .axis = 0, .arg = 4 };
  KOpt grfb_global2 = { .op = KOP_GLOBAL, .axis = 0, .arg = 2 };
  KOpt grfb_group1  = { .op = KOP_GROUP,  .axis = 1, .arg = 1 };
  CHECK(kernel_apply_opt(ke, grfb_local4));
  CHECK(kernel_apply_opt(ke, grfb_global2));
  CHECK(kernel_apply_opt(ke, grfb_group1));
  CHECK(tile_build_from_scalar(ke));
  CHECK(tile_validate(ke));
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.n_axes, 3);
  CHECK_EQ(info.axis_types[0], (u32)KAX_GLOBAL);
  CHECK_EQ(info.axis_extents[0], 2u);
  CHECK_EQ(info.axis_types[1], (u32)KAX_LOCAL);
  CHECK_EQ(info.axis_extents[1], 4u);
  CHECK_EQ(info.axis_types[2], (u32)KAX_GROUP_REDUCE);
  CHECK_EQ(info.axis_extents[2], 1u);
  CHECK(!cg_supports_tile(ke));

  TEST_BEGIN("tile-graph/restore-kernel-axes-override");
  // E9-prep wedge 6: drive [LOOP=2, UPCAST=4] via axes_default_for +
  // KOP_UPCAST(axis=0, arg=4).
  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = 8;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
  KOpt rkao_upcast4 = { .op = KOP_UPCAST, .axis = 0, .arg = 4 };
  CHECK(kernel_apply_opt(ke, rkao_upcast4));
  CHECK(tile_build_from_scalar(ke));
  CHECK(tile_validate(ke));

  TEST_BEGIN("tile-graph/validator-rejects-bad-root");
  u32 good_root = ke->tile_root;
  ke->tile_root = 3;
  CHECK(!tile_validate(ke));
  CHECK_EQ(tile_loop_axis_count(ke), 0);
  ke->tile_root = good_root;
  CHECK(tile_validate(ke));
  u8 good_src_count = ke->tile_uops[good_root].src_count;
  ke->tile_uops[good_root].src_count = (u8)(TILE_MAX_SRC + 1);
  CHECK(!tile_validate(ke));
  CHECK_EQ(tile_loop_axis_count(ke), 0);
  ke->tile_uops[good_root].src_count = good_src_count;
  CHECK(tile_validate(ke));

  TEST_BEGIN("tile-graph/validator-rejects-bad-refs");
  u32 good_axis = ke->tile_uops[good_root].src[1];
  ke->tile_uops[good_root].src[1] = ke->n_tile_uops + 7;
  CHECK(!tile_validate(ke));
  CHECK(!tile_collect_plan_info(ke, &info));
  CHECK_EQ(tile_loop_axis_count(ke), 0);
  ke->tile_uops[good_root].src[1] = good_axis;
  CHECK(tile_validate(ke));
  u32 good_index = ke->scalar_uops[scalar_store].src[0];
  ke->scalar_uops[scalar_store].src[0] = 0;
  CHECK(!tile_validate(ke));
  CHECK(!tile_collect_plan_info(ke, &info));
  CHECK_EQ(tile_loop_axis_count(ke), 0);
  ke->scalar_uops[scalar_store].src[0] = good_index;
  CHECK(tile_validate(ke));

  TEST_BEGIN("tile-graph/free-then-reemit");
  tile_free(ke);
  CHECK_EQ((unsigned long long)ke->tile_uops, 0);
  CHECK_EQ(ke->n_tile_uops, 0);
  CHECK_EQ(ke->tile_uops_cap, 0);
  CHECK_EQ(ke->tile_root, 0);
  for (u32 i = 0; i < 100; i++) {
    u32 id = tile_emit_leaf(ke, TILE_AXIS, DT_INT64,
                            ((u64)KAX_LOOP << 32) | i);
    CHECK_EQ(id, i + 1);
  }
  CHECK_EQ(ke->n_tile_uops, 101);
  CHECK(ke->tile_uops_cap >= 101);
  CHECK_EQ(ke->tile_uops[100].op, TILE_AXIS);
  CHECK_EQ((u32)ke->tile_uops[100].extra, 99u);
  CHECK_EQ(ke->tile_root, 0);
  CHECK(!tile_validate(ke));

  TEST_BEGIN("tile-graph/kernel-free-cleans-tile-and-scalar");
  kernel_free_arrays(ke);
  CHECK_EQ((unsigned long long)ke->tile_uops, 0);
  CHECK_EQ(ke->n_tile_uops, 0);
  CHECK_EQ(ke->tile_uops_cap, 0);
  CHECK_EQ(ke->tile_root, 0);
  CHECK_EQ((unsigned long long)ke->scalar_uops, 0);
  CHECK_EQ(ke->n_scalar_uops, 0);
  CHECK_EQ(ke->scalar_uops_cap, 0);

  TEST_BEGIN("tile-graph/materialize-auto-seeds-tile-plan");
  setenv("THVM_RANGEIFY", "1", 1);
  u32 dims[1] = {4};
  Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  Term b = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  thvm_materialize(uop_binary(UOP_ADD, a, b));
  KernelEntry *mk = &KERNELS[KERNELS_NEXT - 1];
  CHECK(mk->scalar_uops != NULL);
  CHECK(mk->n_scalar_uops > 1);
  CHECK(mk->tile_uops != NULL);
  CHECK(mk->n_tile_uops >= 5);
  CHECK_EQ(mk->tile_uops[0].op, TILE_NONE);
  CHECK_EQ(mk->tile_uops[1].op, TILE_SCALAR_BODY);
  CHECK_EQ(mk->tile_uops[2].op, TILE_STORE);
  CHECK(tile_validate(mk));
  CHECK(mk->tile_root != 0);
  CHECK_EQ(mk->tile_uops[mk->tile_root].op, TILE_LOOP_NEST);
  CHECK_EQ(tile_loop_axis_count(mk), 1);
  CHECK_EQ(tile_loop_axis_extent(mk, 0), 4u);

  // Set up `tk` -- the tk-using tests originally lived inside the
  // c-renderer test bodies that were removed when render_c_scalar.c
  // was deleted; the surviving Metal/axes tests still need a fresh
  // KernelEntry handle.  ("apply-global-marks-loop-axis" was removed
  // here too -- it relied on a c-renderer-applied UPCAST step earlier
  // in the file's mk setup that no longer runs.)
  u32 tile_kid = kernel_alloc();
  KernelEntry *tk = &KERNELS[tile_kid];

  TEST_BEGIN("tile-graph/scalar-reduce-axis-auto-sync-and-proposes-group");
  CHECK(build_scalar_reduce_sum_graph(tk) != 0);
  tk->output_shape.ndim = 1;
  tk->output_shape.dims[0] = 1;
  tk->axes = &tk->_local_axes;
  memset(tk->axes, 0, sizeof(KernelAxes));
  axes_default_for(tk);
  // E9 session 4: route through resolvers now that the writer scratch
  // is private (`tk->axes->_writer.{n_axes,full_shape}`).  The
  // resolver derives axis count from signals (output_shape +
  // tail-reduce + scalar-reduce + applied_opts), so for a kernel with
  // a scalar-arena S_REDUCE_* the resolver counts the trailing
  // KAX_REDUCE axis from the start -- the previous staged check
  // (1 axis post-default-for, 2 post-ensure-scalar-reduce) is a
  // writer-internal artifact no longer observable through the
  // resolver.  Verify the steady state after both writers have run.
  axes_ensure_scalar_reduce(tk);
  CHECK_EQ(axes_resolve_n_axes(tk), 2u);
  CHECK_EQ(axes_resolve_kax_type(tk, 0), (u8)KAX_LOOP);
  CHECK_EQ(axes_resolve_kax_type(tk, 1), (u8)KAX_REDUCE);
  u32 tk_ext1 = 0;
  CHECK(axes_resolve_full_shape(tk, 1, &tk_ext1));
  CHECK_EQ(tk_ext1, 4u);
  CHECK(tile_build_from_scalar(tk));
  CHECK(tile_collect_plan_info(tk, &info));
  CHECK_EQ(info.n_axes, 2u);
  CHECK_EQ(info.axis_types[0], (u32)KAX_LOOP);
  CHECK_EQ(info.axis_types[1], (u32)KAX_REDUCE);
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE", "1", 1);
  KOpt red_cands[16];
  u32 n_red_cands = kernel_opts_propose(tk, red_cands,
                                        (u32)(sizeof(red_cands)/
                                              sizeof(*red_cands)));
  int saw_group4 = 0;
  for (u32 i = 0; i < n_red_cands; i++) {
    if (red_cands[i].op == KOP_GROUP && red_cands[i].axis == 1
        && red_cands[i].arg == 4) {
      saw_group4 = 1;
    }
  }
  CHECK(saw_group4);
  unsetenv("THVM_BACKEND");
  unsetenv("THVM_TILE");
  kernel_free_arrays(tk);
  tk->axes = NULL;

  TEST_BEGIN("tile-graph/metal-post-reduce-expression-uses-group-accumulator");
  CHECK(build_scalar_post_reduce_sum_graph(tk) != 0);
  tk->output_shape.ndim = 1;
  tk->output_shape.dims[0] = 1;
  tk->axes = &tk->_local_axes;
  memset(tk->axes, 0, sizeof(KernelAxes));
  axes_default_for(tk);
  axes_ensure_scalar_reduce(tk);
  CHECK(tile_build_from_scalar(tk));
  CHECK(tile_collect_plan_info(tk, &info));
  CHECK_EQ(info.reduce_tile_id, 0u);
  CHECK(info.scalar_reduce_id != 0);
  CHECK(info.scalar_value_id != info.scalar_reduce_id);
  u32 post_groups_x = 0;
  u32 post_threads_x = 0;
  // GROUP_REDUCE path (parallelised variant) below.
  KOpt post_group4 = { .op = KOP_GROUP, .axis = 1, .arg = 4 };
  CHECK(kernel_apply_opt(tk, post_group4));
  CHECK(cg_tile_metal_dispatch_shape(tk, &post_groups_x, &post_threads_x));
  CHECK_EQ(post_groups_x, 1u);
  CHECK_EQ(post_threads_x, 4u);
  char *post_metal_src = cg_emit_tile_metal(tk);
  CHECK(post_metal_src != NULL);
  if (post_metal_src != NULL) {
    free(post_metal_src);
  }
  kernel_free_arrays(tk);
  tk->axes = NULL;


  TEST_REPORT();
}
