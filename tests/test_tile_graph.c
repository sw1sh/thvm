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
// KpSchedule override.

#include "../src/thvm.c"
#include "test.h"

// cg_supports_tile / cg_supports_scalar / cg_emit_tile / cg_emit_scalar
// lived in src/codegen/render_c_scalar.c, deleted as part of the
// ScalarUop / TileUop C-renderer cull.  The corresponding CPU JIT
// paths (cpu_jit_dispatch_tile / cpu_jit_dispatch_scalar) and the
// CPU tile interpreter (cpu_dispatch_tile) have all been deleted.
// Tests that string-matched the rendered C source are now vacuous
// on these stubs -- the live coverage they offered (matmul tile
// construction, tile_analyze_gemm, dispatch-side metal-gemm-with-TC
// routing) now lives in test_uop_recognise_tc.c (DAG-side
// classifier) and test_metal_real.c (live cblas dispatcher
// integration).
static int cg_supports_tile  (KernelEntry const *ke) { (void)ke; return 0; }
static int cg_supports_scalar(KernelEntry const *ke) { (void)ke; return 0; }

// Local emit helpers (previously static-internal to rangeify.c, now
// deleted with the legacy lowering pass).  Keep the test-only graph
// builders self-contained.
static u32 emit_iconst(KernelEntry *ke, i64 v) {
  return rangeify_emit_leaf(ke, S_ICONST, DT_INT64, (u64)v);
}
static u32 emit_ibinop(KernelEntry *ke, u8 op, u32 a, u32 b) {
  return rangeify_emit_binary(ke, op, DT_INT64, a, b);
}
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

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
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

  // rangeify_cse was retired with the legacy ScalarUop lowering pass;
  // the unified-rangeify pass owns CSE at the UOp-DAG level now.
  kernel_free_arrays(ke);

  // Matmul shape-recognition coverage lives in
  // test_uop_recognise_tc.c (DAG-side uop_dag_classify_matmul_shape)
  // and the live `metal-real/{gemm,dot,gemv}-cpu-routes-through-cblas`
  // checks in test_metal_real.c.

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
  ke->schedule = &ke->_local_schedule;
  memset(ke->schedule, 0, sizeof(KpSchedule));
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
  memset(ke->schedule, 0, sizeof(KpSchedule));
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
  memset(ke->schedule, 0, sizeof(KpSchedule));
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
  memset(ke->schedule, 0, sizeof(KpSchedule));
  axes_default_for(ke);
  KOptSeq conv_seq = {0};
  conv_seq.n = 2;
  conv_seq.opts[0] = conv_upcast4;
  conv_seq.opts[1] = conv_local4;
  CHECK(kautotune_apply_seq(ke, &conv_seq));
  // Read applied_opts via the tile_anno facade so the eventual
  // ownership move is a single-file change.
  CHECK_EQ(tile_anno_applied_opts_count(ke), 3u);
  CHECK_EQ(tile_anno_applied_opts(ke)[0].op, (u32)KOP_UPCAST);
  CHECK_EQ(tile_anno_applied_opts(ke)[1].op, (u32)KOP_LOCAL);
  CHECK_EQ(tile_anno_applied_opts(ke)[2].op, (u32)KOP_GLOBAL);
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.threads, 4u);
  CHECK_EQ(conv.outputs_per_thread, 4u);
  memset(ke->schedule, 0, sizeof(KpSchedule));
  ke->schedule = NULL;
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/metal-conv2d-flat-proposes-local");
  build_kprog_conv2d_flat(ke);
  // E9: drive [LOOP=4, LOOP=16, REDUCE=18] via the writer trio.
  ke->schedule = &ke->_local_schedule;
  memset(ke->schedule, 0, sizeof(KpSchedule));
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
  memset(ke->schedule, 0, sizeof(KpSchedule));
  ke->schedule = NULL;
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/conv2d-flat-counters-legacy-fixture");
  // The hand-built KProgOp fixture above has `cached_lift.store_root == 0`
  // (no lifter was run), so tile_analyze_conv2d_flat takes the LEGACY
  // program[]-side branch.  Counter probe asserts at least one LEGACY
  // dispatch fired and zero DAG dispatches.  Mirrors slice 8 session
  // 2's BLAS_GEMM_DISPATCH_DAG counter probe pattern.
  tile_conv2d_flat_counters_reset();
  CHECK_EQ(tile_conv2d_flat_dag_count(),    0ull);
  CHECK_EQ(tile_conv2d_flat_legacy_count(), 0ull);
  build_kprog_conv2d_flat(ke);
  TileConv2DInfo legacy_conv = {0};
  CHECK(tile_analyze_conv2d_flat(ke, &legacy_conv));
  CHECK_EQ(tile_conv2d_flat_dag_count(),    0ull);
  CHECK_EQ(tile_conv2d_flat_legacy_count(), 1ull);
  kernel_free_arrays(ke);
  tile_conv2d_flat_counters_reset();

  TEST_BEGIN("tile-graph/conv2d-flat-counters-dag-fixture");
  // Build a synthetic STORE(C, addr, REDUCE_SUM(...)) and pin it on
  // ke->cached_lift.store_root, with input_views[] populated so the
  // shape extraction succeeds.  The DAG-side classifier accepts and
  // tile_analyze_conv2d_flat reports the same fields as the LEGACY
  // path -- structural shape comes from input_views/output_shape, not
  // from program[].
  {
    KernelEntry dke = {0};
    u32 dc_out = 4, dc_in = 2, dh = 6, dw = 6, dkh = 3, dkw = 3;
    u32 dh_out = dh - dkh + 1, dw_out = dw - dkw + 1;
    u32 dk = dc_in * dkh * dkw, dp = dh_out * dw_out;
    kernel_inputs_reserve(&dke, 2);
    dke.n_inputs = 2;
    dke.input_dtypes[0] = DT_FP32;
    dke.input_dtypes[1] = DT_FP32;
    dke.input_numels[0] = dc_out * dk;
    dke.input_numels[1] = dc_in * dh * dw;
    dke.output_dtype    = DT_FP32;
    dke.output_numel    = dc_out * dp;
    dke.output_shape.ndim = 2;
    dke.output_shape.dims[0] = dc_out;
    dke.output_shape.dims[1] = dp;
    test_set_view3(&dke.input_views[0], dc_out, dk, dp, (i32)dk, 1, 0);
    test_set_view3(&dke.input_views[1], dc_in, dh, dw, (i32)(dh * dw),
                   (i32)dw, 1);
    // Build a minimal lifted-conv-shape DAG.  Only the STORE-of-
    // REDUCE_SUM structure matters for the classifier; the addresses
    // can be any valid UOp terms.
    u32 ddims_w[2] = {dc_out, dk};
    u32 ddims_x[1] = {1024};
    u32 ddims_o[1] = {dc_out * dp};
    Term Wd = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, ddims_w, 1);
    Term Xd = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, ddims_x, 2);
    Term Cd = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, ddims_o, 0);
    Term r_out_d = uop_range(0, 0, dc_out * dp);
    Term r_q_d   = uop_range(1, 1 /*REDUCE*/, dk);
    Term ldW_d   = uop_index_e(Wd, r_out_d);
    Term ldX_d   = uop_index_e(Xd, r_q_d);
    Term mul_d   = uop_binary(UOP_MUL, ldW_d, ldX_d);
    Term red_d   = uop_reduce(REDUCE_SUM, /*axis=*/1, mul_d);
    Term store_d = uop_store(Cd, r_out_d, red_d);
    dke.cached_lift.store_root = store_d;

    TileConv2DInfo dag_conv = {0};
    CHECK(tile_analyze_conv2d_flat(&dke, &dag_conv));
    CHECK_EQ(dag_conv.dtype, (u32)DT_FP32);
    CHECK_EQ(dag_conv.c_out, dc_out);
    CHECK_EQ(dag_conv.c_in,  dc_in);
    CHECK_EQ(dag_conv.kh,    dkh);
    CHECK_EQ(dag_conv.kw,    dkw);
    CHECK_EQ(dag_conv.h_out, dh_out);
    CHECK_EQ(dag_conv.w_out, dw_out);
    CHECK_EQ(tile_conv2d_flat_dag_count(),    1ull);
    CHECK_EQ(tile_conv2d_flat_legacy_count(), 0ull);

    TEST_BEGIN("tile-graph/conv2d-flat-counters-dag-rejects-non-reduce");
    // STORE(C, addr, CONST 0) -- not a REDUCE.  Classifier rejects.
    Term zero_v   = uop_const(DT_FP32, 0);
    Term bad_store= uop_store(Cd, r_out_d, zero_v);
    dke.cached_lift.store_root = bad_store;
    TileConv2DInfo bad_conv = {0};
    CHECK(!tile_analyze_conv2d_flat(&dke, &bad_conv));
    // No counter increments on classifier reject.
    CHECK_EQ(tile_conv2d_flat_dag_count(),    1ull);
    CHECK_EQ(tile_conv2d_flat_legacy_count(), 0ull);

    TEST_BEGIN("tile-graph/conv2d-flat-counters-dag-rejects-max-reduce");
    // REDUCE_MAX is not REDUCE_SUM -- classifier rejects.
    Term red_max = uop_reduce(REDUCE_MAX, /*axis=*/1, mul_d);
    Term store_max = uop_store(Cd, r_out_d, red_max);
    dke.cached_lift.store_root = store_max;
    TileConv2DInfo max_conv = {0};
    CHECK(!tile_analyze_conv2d_flat(&dke, &max_conv));
    CHECK_EQ(tile_conv2d_flat_dag_count(),    1ull);
    CHECK_EQ(tile_conv2d_flat_legacy_count(), 0ull);

    TEST_BEGIN("tile-graph/conv2d-flat-counters-dag-accepts-opt-conv");
    // The F4 recogniser may have wrapped REDUCE in OPT(_, CONV, 0).
    // The classifier peels the wrapper and still accepts.
    Term opt_conv  = uop_opt(red_d, UOP_OPT_CONV, 0);
    Term store_opt = uop_store(Cd, r_out_d, opt_conv);
    dke.cached_lift.store_root = store_opt;
    TileConv2DInfo opt_conv_info = {0};
    CHECK(tile_analyze_conv2d_flat(&dke, &opt_conv_info));
    CHECK_EQ(opt_conv_info.c_out, dc_out);
    CHECK_EQ(tile_conv2d_flat_dag_count(),    2ull);
    CHECK_EQ(tile_conv2d_flat_legacy_count(), 0ull);

    kernel_free_arrays(&dke);
  }
  tile_conv2d_flat_counters_reset();

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

  TEST_REPORT();
}
