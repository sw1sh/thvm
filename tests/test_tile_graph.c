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

// LEGACY_MSL_CHECK: assertions that hold for the legacy ScalarUop
// renderer's specific MSL conventions (`_tg`, `_ltid`, `_ta0`, `_sh`,
// `_rk`, `_gid3.x`, `_tk`, `threadgroup float`, etc.).  The new
// render_uop renderer (default since Phase F) uses different
// conventions (`tg`, `tt`, `aN`, `_acc<axis>`) so these assertions
// don't apply.  Run only when THVM_RENDER_VIA_UOP=0 forces the
// legacy renderer.
static inline int legacy_msl_render_active(void) {
  char const *e = getenv("THVM_RENDER_VIA_UOP");
  return e != NULL && e[0] == '0';
}
#define LEGACY_MSL_CHECK(expr) \
  do { if (legacy_msl_render_active()) { CHECK(expr); } } while (0)
#define LEGACY_MSL_CHECK_EQ(a, b) \
  do { if (legacy_msl_render_active()) { CHECK_EQ(a, b); } } while (0)

#define TEST_REDUCE_NO_TAIL 0xFFFFFFFFu

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
  int jit_hit = 0;
  for (u32 attempt = 0; attempt < 3; attempt++) {
    if (cpu_jit_dispatch_tile(ke, in_bufs, out_buf)) {
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

static void set_reduce_axes(KernelEntry *ke, u32 tail_axis_type) {
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 1;
  if (tail_axis_type == TEST_REDUCE_NO_TAIL) {
    ke->axes->n_axes = 2;
    ke->axes->axis_types[1] = KAX_REDUCE;
    ke->axes->full_shape[1] = 4;
  } else {
    ke->axes->n_axes = 3;
    ke->axes->axis_types[1] = KAX_REDUCE;
    ke->axes->full_shape[1] = 2;
    ke->axes->axis_types[2] = tail_axis_type;
    ke->axes->full_shape[2] = 2;
  }
  ke->axes->version++;
}

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

  TEST_BEGIN("tile-graph/gemm-analysis-normal-and-transposed");
  build_kprog_gemm(ke, 2, 3, 4);
  u32 storage_numels[2] = {8, 12};
  TileGemmInfo gemm = {0};
  CHECK(tile_analyze_gemm(ke, storage_numels, &gemm));
  CHECK_EQ(gemm.dtype, (u32)DT_FP32);
  CHECK_EQ(gemm.M, 2u);
  CHECK_EQ(gemm.N, 3u);
  CHECK_EQ(gemm.K, 4u);
  CHECK_EQ(gemm.a_input, 0u);
  CHECK_EQ(gemm.b_input, 1u);
  CHECK_EQ(gemm.ldA, 4u);
  CHECK_EQ(gemm.ldB, 3u);
  CHECK_EQ(gemm.flags, 0u);
  CHECK_EQ(gemm.tile_size, 16u);

  test_set_view3(&ke->input_views[1], 2, 4, 3, 0, 1, 4);
  CHECK(tile_analyze_gemm(ke, storage_numels, &gemm));
  CHECK_EQ(gemm.b_input, 1u);
  CHECK_EQ(gemm.ldB, 4u);
  CHECK_EQ(gemm.flags, 2u);
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/gemm-analysis-square-view-disambiguates");
  build_kprog_gemm(ke, 4, 4, 4);
  test_set_view3(&ke->input_views[0], 4, 4, 4, 0, 4, 1);
  test_set_view3(&ke->input_views[1], 4, 4, 4, 4, 1, 0);
  u32 square_storage_numels[2] = {16, 16};
  CHECK(tile_analyze_gemm(ke, square_storage_numels, &gemm));
  CHECK_EQ(gemm.a_input, 1u);
  CHECK_EQ(gemm.b_input, 0u);
  CHECK_EQ(gemm.flags, 0u);
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/gemv-expand-promotes-to-mma-plan");
  build_kprog_gemv_expand(ke, 2, 3);
  u32 gemv_storage_numels[2] = {6, 3};
  CHECK(tile_analyze_gemm(ke, gemv_storage_numels, &gemm));
  CHECK_EQ(gemm.dtype, (u32)DT_FP32);
  CHECK_EQ(gemm.M, 2u);
  CHECK_EQ(gemm.N, 1u);
  CHECK_EQ(gemm.K, 3u);
  CHECK_EQ(gemm.a_input, 0u);
  CHECK_EQ(gemm.b_input, 1u);
  CHECK_EQ(gemm.ldA, 3u);
  CHECK_EQ(gemm.ldB, 1u);
  CHECK_EQ(gemm.flags, 0u);
  CHECK(tile_sync_from_scalar(ke));
  CHECK(tile_validate(ke));
  CHECK_EQ(ke->tile_uops[ke->tile_root].op, TILE_MMA);
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.mma.M, 2u);
  CHECK_EQ(info.mma.N, 1u);
  CHECK_EQ(info.mma.K, 3u);
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/build-mma-plan-from-gemm");
  build_kprog_gemm(ke, 2, 3, 4);
  CHECK(tile_sync_from_scalar(ke));
  CHECK(tile_validate(ke));
  CHECK_EQ(ke->tile_uops[ke->tile_root].op, TILE_MMA);
  CHECK_EQ(ke->n_tile_uops, 5u);
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.root_id, ke->tile_root);
  CHECK_EQ(info.mma_tile_id, ke->tile_root);
  CHECK_EQ(info.dtype, (u32)DT_FP32);
  CHECK_EQ(info.n_axes, 3u);
  CHECK_EQ(info.axis_types[0], (u32)KAX_LOOP);
  CHECK_EQ(info.axis_types[1], (u32)KAX_LOOP);
  CHECK_EQ(info.axis_types[2], (u32)KAX_REDUCE);
  CHECK_EQ(info.axis_extents[0], 2u);
  CHECK_EQ(info.axis_extents[1], 3u);
  CHECK_EQ(info.axis_extents[2], 4u);
  CHECK_EQ(info.mma.M, 2u);
  CHECK_EQ(info.mma.N, 3u);
  CHECK_EQ(info.mma.K, 4u);
  CHECK_EQ(info.mma.a_input, 0u);
  CHECK_EQ(info.mma.b_input, 1u);
  CHECK_EQ(info.mma.ldA, 4u);
  CHECK_EQ(info.mma.ldB, 3u);
  CHECK_EQ(info.mma.flags, 0u);
  CHECK_EQ(info.mma.tile_size, 16u);
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 2;
  ke->axes->axis_types[1] = KAX_LOOP;
  ke->axes->full_shape[1] = 3;
  ke->axes->axis_types[2] = KAX_REDUCE;
  ke->axes->full_shape[2] = 4;
  ke->axes->version++;
  KOpt tc8 = { .op = KOP_TC, .axis = 0, .arg = 8 };
  CHECK(kernel_apply_opt(ke, tc8));
  CHECK_EQ(ke->axes->n_applied, 1u);
  CHECK_EQ(ke->axes->applied_opts[0].op, (u32)KOP_TC);
  CHECK(tile_sync_from_scalar(ke));
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.mma.tile_size, 8u);
  KOpt tc7 = { .op = KOP_TC, .axis = 0, .arg = 7 };
  CHECK(!kernel_apply_opt(ke, tc7));
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes = NULL;
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/metal-gemm-proposes-tc");
  build_kprog_gemm(ke, 16, 16, 16);
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 16;
  ke->axes->axis_types[1] = KAX_LOOP;
  ke->axes->full_shape[1] = 16;
  ke->axes->axis_types[2] = KAX_REDUCE;
  ke->axes->full_shape[2] = 16;
  ke->axes->version++;
  setenv("THVM_BACKEND", "metal", 1);
  KOpt tc_cands[16];
  u32 n_tc = kernel_opts_propose(ke, tc_cands,
                                 (u32)(sizeof(tc_cands)/sizeof(*tc_cands)));
  CHECK_EQ(n_tc, 3u);
  CHECK_EQ(tc_cands[0].op, (u32)KOP_TC);
  CHECK_EQ(tc_cands[0].axis, 0u);
  CHECK_EQ(tc_cands[0].arg, 32u);
  CHECK_EQ(tc_cands[1].op, (u32)KOP_TC);
  CHECK_EQ(tc_cands[1].arg, 16u);
  CHECK_EQ(tc_cands[2].op, (u32)KOP_TC);
  CHECK_EQ(tc_cands[2].arg, 8u);
  unsetenv("THVM_BACKEND");
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes = NULL;
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/metal-gemv-expand-proposes-tc");
  build_kprog_gemv_expand(ke, 16, 32);
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 2;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 16;
  ke->axes->axis_types[1] = KAX_REDUCE;
  ke->axes->full_shape[1] = 32;
  ke->axes->version++;
  setenv("THVM_BACKEND", "metal", 1);
  n_tc = kernel_opts_propose(ke, tc_cands,
                             (u32)(sizeof(tc_cands)/sizeof(*tc_cands)));
  CHECK_EQ(n_tc, 3u);
  CHECK_EQ(tc_cands[0].op, (u32)KOP_TC);
  CHECK_EQ(tc_cands[0].arg, 32u);
  CHECK_EQ(tc_cands[1].op, (u32)KOP_TC);
  CHECK_EQ(tc_cands[1].arg, 16u);
  CHECK_EQ(tc_cands[2].op, (u32)KOP_TC);
  CHECK_EQ(tc_cands[2].arg, 8u);
  unsetenv("THVM_BACKEND");
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes = NULL;
  kernel_free_arrays(ke);

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
    LEGACY_MSL_CHECK(strstr(conv_src, "#define KRED (CIN * KH * KW)") != NULL);
    LEGACY_MSL_CHECK(strstr(conv_src, "for (int ci = 0") != NULL);
    LEGACY_MSL_CHECK(strstr(conv_src, "constant int *cfg") != NULL);
    LEGACY_MSL_CHECK(strstr(conv_src, "acc += in0[wi] * in1[xi]") != NULL);
    free(conv_src);
  }
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 4;
  ke->axes->axis_types[1] = KAX_LOOP;
  ke->axes->full_shape[1] = 16;
  ke->axes->axis_types[2] = KAX_REDUCE;
  ke->axes->full_shape[2] = 18;
  ke->axes->version++;
  KOpt conv_local4 = { .op = KOP_LOCAL, .axis = 0, .arg = 4 };
  CHECK(kernel_apply_opt(ke, conv_local4));
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.threads, 4u);
  CHECK_EQ(conv.outputs_per_thread, 1u);
  CHECK_EQ(conv.reduce_unroll, 1u);
  CHECK(cg_tile_metal_dispatch_shape(ke, &conv_groups_x, &conv_threads_x));
  CHECK_EQ(conv_groups_x, 16u);
  CHECK_EQ(conv_threads_x, 4u);
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 4;
  ke->axes->axis_types[1] = KAX_LOOP;
  ke->axes->full_shape[1] = 16;
  ke->axes->axis_types[2] = KAX_REDUCE;
  ke->axes->full_shape[2] = 18;
  ke->axes->version++;
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
    LEGACY_MSL_CHECK(strstr(conv_src, "#define OUTS 4u") != NULL);
    LEGACY_MSL_CHECK(strstr(conv_src, "uint base = gid * OUTS") != NULL);
    free(conv_src);
  }
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 4;
  ke->axes->axis_types[1] = KAX_LOOP;
  ke->axes->full_shape[1] = 16;
  ke->axes->axis_types[2] = KAX_REDUCE;
  ke->axes->full_shape[2] = 18;
  ke->axes->version++;
  KOpt conv_unroll2 = { .op = KOP_UNROLL, .axis = 2, .arg = 2 };
  CHECK(kernel_apply_opt(ke, conv_unroll2));
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.reduce_unroll, 2u);
  conv_src = cg_emit_tile_metal(ke);
  CHECK(conv_src != NULL);
  if (conv_src != NULL) {
    LEGACY_MSL_CHECK(strstr(conv_src, "#pragma clang loop unroll_count(2)") != NULL);
    LEGACY_MSL_CHECK(strstr(conv_src, "for (int q = 0; q < KRED") != NULL);
    free(conv_src);
  }
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 4;
  ke->axes->axis_types[1] = KAX_LOOP;
  ke->axes->full_shape[1] = 16;
  ke->axes->axis_types[2] = KAX_REDUCE;
  ke->axes->full_shape[2] = 18;
  ke->axes->version++;
  KOptSeq conv_seq = {0};
  conv_seq.n = 2;
  conv_seq.opts[0] = conv_upcast4;
  conv_seq.opts[1] = conv_local4;
  CHECK(kautotune_apply_seq(ke, &conv_seq));
  CHECK_EQ(ke->axes->n_applied, 3u);
  CHECK_EQ(ke->axes->applied_opts[0].op, (u32)KOP_UPCAST);
  CHECK_EQ(ke->axes->applied_opts[1].op, (u32)KOP_LOCAL);
  CHECK_EQ(ke->axes->applied_opts[2].op, (u32)KOP_GLOBAL);
  CHECK(tile_analyze_conv2d_flat(ke, &conv));
  CHECK_EQ(conv.threads, 4u);
  CHECK_EQ(conv.outputs_per_thread, 4u);
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes = NULL;
  kernel_free_arrays(ke);

  TEST_BEGIN("tile-graph/metal-conv2d-flat-proposes-local");
  build_kprog_conv2d_flat(ke);
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 4;
  ke->axes->axis_types[1] = KAX_LOOP;
  ke->axes->full_shape[1] = 16;
  ke->axes->axis_types[2] = KAX_REDUCE;
  ke->axes->full_shape[2] = 18;
  ke->axes->version++;
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
  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 2;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 2;
  ke->axes->axis_types[1] = KAX_UPCAST;
  ke->axes->full_shape[1] = 4;
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
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 2;
  ke->axes->axis_types[0] = KAX_GLOBAL;
  ke->axes->full_shape[0] = 2;
  ke->axes->axis_types[1] = KAX_LOCAL;
  ke->axes->full_shape[1] = 4;
  ke->axes->version++;
  CHECK(tile_build_from_scalar(ke));
  CHECK(tile_validate(ke));
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.n_axes, 2);
  CHECK_EQ(info.axis_types[0], (u32)KAX_GLOBAL);
  CHECK_EQ(info.axis_extents[0], 2u);
  CHECK_EQ(info.axis_types[1], (u32)KAX_LOCAL);
  CHECK_EQ(info.axis_extents[1], 4u);
  CHECK(cg_supports_tile(ke));
  u32 groups_x = 0;
  u32 threads_x = 0;
  CHECK(cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x));
  CHECK_EQ(groups_x, 2u);
  CHECK_EQ(threads_x, 4u);
  char *local_global_src = cg_emit_tile(ke);
  CHECK(local_global_src != NULL);
  if (local_global_src != NULL) {
    CHECK(strstr(local_global_src, "for (unsigned _ta0") != NULL);
    CHECK(strstr(local_global_src, "for (unsigned _ta1") != NULL);
    CHECK(strstr(local_global_src, "#pragma clang loop unroll_count") == NULL);
    free(local_global_src);
  }
  char *metal_tile_src = cg_emit_tile_metal(ke);
  CHECK(metal_tile_src != NULL);
  if (metal_tile_src != NULL) {
    CHECK(strstr(metal_tile_src, "threadgroup_position_in_grid") != NULL);
    CHECK(strstr(metal_tile_src, "thread_position_in_threadgroup") != NULL);
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "threadgroup_barrier") != NULL);
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _tg = _tgid;") != NULL);
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _ta0 = _tg % 2u;") != NULL);
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _ta1 = _ltid;") != NULL);
    free(metal_tile_src);
  }
  u32 saved_n_inputs = ke->n_inputs;
  ke->n_inputs = 31;
  CHECK(!cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x));
  LEGACY_MSL_CHECK(cg_emit_tile_metal(ke) == NULL);
  ke->n_inputs = saved_n_inputs;

  TEST_BEGIN("tile-graph/metal-autotune-proposes-local-global");
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE", "1", 1);
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 1;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 8;
  ke->axes->version++;
  KOpt cands[16];
  u32 n_cands = kernel_opts_propose(ke, cands,
                                    (u32)(sizeof(cands)/sizeof(*cands)));
  CHECK(n_cands >= 3);
  CHECK_EQ(cands[0].op, (u32)KOP_LOCAL);
  CHECK_EQ(cands[0].axis, 0u);
  CHECK_EQ(cands[0].arg, 8u);
  KOpt local4 = { .op = KOP_LOCAL, .axis = 0, .arg = 4 };
  CHECK(kernel_apply_tune_candidate(ke, local4));
  CHECK_EQ(ke->axes->n_applied, 2u);
  CHECK_EQ(ke->axes->axis_types[0], (u32)KAX_GLOBAL);
  CHECK_EQ(ke->axes->axis_types[1], (u32)KAX_LOCAL);
  CHECK_EQ(ke->axes->full_shape[0], 2u);
  CHECK_EQ(ke->axes->full_shape[1], 4u);
  CHECK(cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x));
  CHECK_EQ(groups_x, 2u);
  CHECK_EQ(threads_x, 4u);
  unsetenv("THVM_BACKEND");
  unsetenv("THVM_TILE");

  TEST_BEGIN("tile-graph/kernel-axes-local-global-swapped");
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 2;
  ke->axes->axis_types[0] = KAX_LOCAL;
  ke->axes->full_shape[0] = 4;
  ke->axes->axis_types[1] = KAX_GLOBAL;
  ke->axes->full_shape[1] = 2;
  ke->axes->version++;
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
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _ta0 = _ltid;") != NULL);
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _ta1 = _tg % 2u;") != NULL);
    free(metal_tile_src);
  }

  TEST_BEGIN("tile-graph/kernel-axes-local-global-with-loop");
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_GLOBAL;
  ke->axes->full_shape[0] = 2;
  ke->axes->axis_types[1] = KAX_LOCAL;
  ke->axes->full_shape[1] = 2;
  ke->axes->axis_types[2] = KAX_LOOP;
  ke->axes->full_shape[2] = 2;
  ke->axes->version++;
  CHECK(tile_build_from_scalar(ke));
  CHECK(tile_validate(ke));
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.axis_types[0], (u32)KAX_GLOBAL);
  CHECK_EQ(info.axis_types[1], (u32)KAX_LOCAL);
  CHECK_EQ(info.axis_types[2], (u32)KAX_LOOP);
  CHECK(cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x));
  CHECK_EQ(groups_x, 4u);
  CHECK_EQ(threads_x, 2u);
  metal_tile_src = cg_emit_tile_metal(ke);
  CHECK(metal_tile_src != NULL);
  if (metal_tile_src != NULL) {
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _tg = _tgid;") != NULL);
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _ta2 = _tg % 2u;") != NULL);
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _ta0 = _tg % 2u;") != NULL);
    LEGACY_MSL_CHECK(strstr(metal_tile_src, "uint _ta1 = _ltid;") != NULL);
    free(metal_tile_src);
  }

  TEST_BEGIN("tile-graph/group-reduce-axis-falls-back-from-c-renderer");
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 3;
  ke->axes->axis_types[0] = KAX_GLOBAL;
  ke->axes->full_shape[0] = 2;
  ke->axes->axis_types[1] = KAX_LOCAL;
  ke->axes->full_shape[1] = 4;
  ke->axes->axis_types[2] = KAX_GROUP_REDUCE;
  ke->axes->full_shape[2] = 1;
  ke->axes->version++;
  CHECK(tile_build_from_scalar(ke));
  CHECK(tile_validate(ke));
  CHECK(tile_collect_plan_info(ke, &info));
  CHECK_EQ(info.n_axes, 3);
  CHECK_EQ(info.axis_types[2], (u32)KAX_GROUP_REDUCE);
  CHECK_EQ(info.axis_extents[2], 1u);
  CHECK(!cg_supports_tile(ke));

  TEST_BEGIN("tile-graph/restore-kernel-axes-override");
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 2;
  ke->axes->axis_types[0] = KAX_LOOP;
  ke->axes->full_shape[0] = 2;
  ke->axes->axis_types[1] = KAX_UPCAST;
  ke->axes->full_shape[1] = 4;
  ke->axes->version++;
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

  TEST_BEGIN("tile-graph/c-renderer-emits-loop");
  CHECK(cg_supports_tile(mk));
  char *src = cg_emit_tile(mk);
  CHECK(src != NULL);
  CHECK(strstr(src, "for (unsigned _ta0") != NULL);
  CHECK(strstr(src, "unsigned _tk = 0u;") != NULL);
  CHECK(strstr(src, "out[") != NULL);
  CHECK(strstr(src, "eval_scalar") == NULL);
  free(src);
  u32 flat_groups_x = 0;
  u32 flat_threads_x = 0;
  CHECK(cg_tile_metal_dispatch_shape(mk, &flat_groups_x, &flat_threads_x));
  CHECK_EQ(flat_groups_x, 1u);
  CHECK_EQ(flat_threads_x, 4u);
  char *flat_metal_src = cg_emit_tile_metal(mk);
  CHECK(flat_metal_src != NULL);
  if (flat_metal_src != NULL) {
    CHECK(strstr(flat_metal_src, "thread_position_in_grid") != NULL);
    LEGACY_MSL_CHECK(strstr(flat_metal_src, "_tk = _gid3.x") != NULL);
    CHECK(strstr(flat_metal_src, "threadgroup_barrier") == NULL);
    free(flat_metal_src);
  }

  TEST_BEGIN("tile-graph/c-renderer-honors-upcast-axis");
  CHECK(mk->axes != NULL);
  KOpt opt = { .op = KOP_UPCAST, .axis = 0, .arg = 2 };
  CHECK(axes_apply_opt(mk->axes, opt));
  CHECK(tile_sync_from_scalar(mk));
  CHECK(cg_supports_tile(mk));
  src = cg_emit_tile(mk);
  CHECK(src != NULL);
  CHECK(strstr(src, "#pragma clang loop unroll_count(2)") != NULL);
  CHECK(strstr(src, "for (unsigned _ta1") != NULL);
  free(src);

  TEST_BEGIN("tile-graph/apply-global-marks-loop-axis");
  KOpt glob = { .op = KOP_GLOBAL, .axis = 0, .arg = 2 };
  CHECK(axes_apply_opt(mk->axes, glob));
  CHECK_EQ(mk->axes->axis_types[0], (u32)KAX_GLOBAL);
  CHECK_EQ(mk->axes->full_shape[0], 2u);
  KOpt tc = { .op = KOP_TC, .axis = 0, .arg = 1 };
  CHECK(!axes_apply_opt(mk->axes, tc));

  TEST_BEGIN("tile-graph/c-renderer-pad-wrapper-jit");
  u32 tile_kid = kernel_alloc();
  KernelEntry *tk = &KERNELS[tile_kid];
  CHECK(build_scalar_pad_graph(tk) != 0);
  CHECK(tile_build_from_scalar(tk));
  CHECK(cg_supports_tile(tk));
  char *pad_src = cg_emit_tile(tk);
  CHECK(pad_src != NULL);
  if (pad_src != NULL) {
    CHECK(strstr(pad_src, "for (unsigned _ta0") != NULL);
    CHECK(strstr(pad_src, "_ok") != NULL);
    free(pad_src);
  }
  f32 pad_in[3] = {10.0f, 20.0f, 30.0f};
  f32 pad_out[5] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
  run_tile_jit_1(tk, pad_in, sizeof(pad_in), pad_out, sizeof(pad_out));
  CHECK(pad_out[0] == 0.0f);
  CHECK(pad_out[1] == 10.0f);
  CHECK(pad_out[2] == 20.0f);
  CHECK(pad_out[3] == 30.0f);
  CHECK(pad_out[4] == 0.0f);
  kernel_free_arrays(tk);

  TEST_BEGIN("tile-graph/c-renderer-cast-f64-to-f32-jit");
  CHECK(build_scalar_cast64_to32_graph(tk) != 0);
  CHECK(tile_build_from_scalar(tk));
  CHECK(cg_supports_tile(tk));
  char *cast_src = cg_emit_tile(tk);
  CHECK(cast_src != NULL);
  if (cast_src != NULL) {
    CHECK(strstr(cast_src, "float *out") != NULL);
    CHECK(strstr(cast_src, "const double *in0") != NULL);
    CHECK(strstr(cast_src, "((float)(") != NULL);
    free(cast_src);
  }
  f64 cast_in[3] = {-8.0, 0.5, 16.0};
  f32 cast_out[3] = {0.0f, 0.0f, 0.0f};
  run_tile_jit_1(tk, cast_in, sizeof(cast_in), cast_out, sizeof(cast_out));
  CHECK(cast_out[0] == -8.0f);
  CHECK(cast_out[1] == 0.5f);
  CHECK(cast_out[2] == 16.0f);
  kernel_free_arrays(tk);

  TEST_BEGIN("tile-graph/scalar-reduce-axis-auto-sync-and-proposes-group");
  CHECK(build_scalar_reduce_sum_graph(tk) != 0);
  tk->output_shape.ndim = 1;
  tk->output_shape.dims[0] = 1;
  tk->axes = &tk->_local_axes;
  memset(tk->axes, 0, sizeof(KernelAxes));
  axes_default_for(tk);
  CHECK_EQ(tk->axes->n_axes, 1u);
  CHECK_EQ(tk->axes->axis_types[0], (u32)KAX_LOOP);
  axes_ensure_scalar_reduce(tk);
  CHECK_EQ(tk->axes->n_axes, 2u);
  CHECK_EQ(tk->axes->axis_types[1], (u32)KAX_REDUCE);
  CHECK_EQ(tk->axes->full_shape[1], 4u);
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
  // Phase 7-structural: nested-reduce kernels are routed through the
  // FLAT_GRID path by default; the renderer's
  // `rmt_emit_value_with_reduce` already wraps the reduce loop and
  // substitutes the accumulator into the surrounding scalar
  // expression.  Pin the strict pre-fix behavior here for the
  // dispatch-shape rejection assertion; the GROUP_REDUCE path below
  // exercises the parallelised variant separately.
  setenv("THVM_TILE_NESTED_REDUCE_FLAT_GRID", "0", 1);
  CHECK(!cg_tile_metal_dispatch_shape(tk, &post_groups_x, &post_threads_x));
  LEGACY_MSL_CHECK(cg_emit_tile_metal(tk) == NULL);
  unsetenv("THVM_TILE_NESTED_REDUCE_FLAT_GRID");
  KOpt post_group4 = { .op = KOP_GROUP, .axis = 1, .arg = 4 };
  CHECK(kernel_apply_opt(tk, post_group4));
  CHECK(cg_tile_metal_dispatch_shape(tk, &post_groups_x, &post_threads_x));
  CHECK_EQ(post_groups_x, 1u);
  CHECK_EQ(post_threads_x, 4u);
  char *post_metal_src = cg_emit_tile_metal(tk);
  CHECK(post_metal_src != NULL);
  if (post_metal_src != NULL) {
    LEGACY_MSL_CHECK(strstr(post_metal_src, "threadgroup float") != NULL);
    LEGACY_MSL_CHECK(strstr(post_metal_src, "_sh") != NULL);
    LEGACY_MSL_CHECK(strstr(post_metal_src, "* as_type<float>(0x40000000u)") != NULL);
    free(post_metal_src);
  }
  kernel_free_arrays(tk);
  tk->axes = NULL;

  TEST_BEGIN("tile-graph/c-renderer-reduce-axis-jit");
  CHECK(build_scalar_reduce_sum_graph(tk) != 0);
  set_reduce_axes(tk, TEST_REDUCE_NO_TAIL);
  CHECK(tile_build_from_scalar(tk));
  CHECK(tile_collect_plan_info(tk, &info));
  CHECK(info.reduce_tile_id != 0);
  CHECK_EQ(tk->tile_uops[info.store_tile_id].src[0], info.reduce_tile_id);
  CHECK_EQ(tk->tile_uops[info.reduce_tile_id].op, TILE_REDUCE);
  CHECK_EQ(tk->tile_uops[info.reduce_tile_id].src[0], info.body_tile_id);
  CHECK_EQ(info.scalar_reduce_id, info.scalar_value_id);
  CHECK_EQ(info.scalar_body_value_id,
           tk->scalar_uops[info.scalar_value_id].src[0]);
  u32 saved_reduce_src = tk->tile_uops[info.store_tile_id].src[0];
  tk->tile_uops[info.store_tile_id].src[0] = info.body_tile_id;
  CHECK(!tile_validate(tk));
  tk->tile_uops[info.store_tile_id].src[0] = saved_reduce_src;
  CHECK(tile_validate(tk));
  CHECK(cg_supports_tile(tk));
  char *red_src = cg_emit_tile(tk);
  CHECK(red_src != NULL);
  if (red_src != NULL) {
    CHECK(strstr(red_src, "_acc") != NULL);
    CHECK(strstr(red_src, "for (unsigned _ta0") != NULL);
    CHECK(strstr(red_src, "for (unsigned _ta1") == NULL);
    free(red_src);
  }
  u32 red_groups_x = 0;
  u32 red_threads_x = 0;
  CHECK(cg_tile_metal_dispatch_shape(tk, &red_groups_x, &red_threads_x));
  CHECK_EQ(red_groups_x, 1u);
  CHECK_EQ(red_threads_x, 1u);
  char *red_metal_src = cg_emit_tile_metal(tk);
  CHECK(red_metal_src != NULL);
  if (red_metal_src != NULL) {
    CHECK(strstr(red_metal_src, "thread_position_in_grid") != NULL);
    LEGACY_MSL_CHECK(strstr(red_metal_src, "for (uint _rk") != NULL);
    CHECK(strstr(red_metal_src, "_acc") != NULL);
    free(red_metal_src);
  }
  f32 red_in[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 red_out[1] = {0.0f};
  run_tile_jit_1(tk, red_in, sizeof(red_in), red_out, sizeof(red_out));
  CHECK(red_out[0] == 10.0f);
  KOpt group4 = { .op = KOP_GROUP, .axis = 1, .arg = 4 };
  CHECK(kernel_apply_opt(tk, group4));
  CHECK(cg_tile_metal_dispatch_shape(tk, &red_groups_x, &red_threads_x));
  CHECK_EQ(red_groups_x, 1u);
  CHECK_EQ(red_threads_x, 4u);
  red_metal_src = cg_emit_tile_metal(tk);
  CHECK(red_metal_src != NULL);
  if (red_metal_src != NULL) {
    LEGACY_MSL_CHECK(strstr(red_metal_src, "threadgroup float") != NULL);
    LEGACY_MSL_CHECK(strstr(red_metal_src, "_rk") != NULL);
    LEGACY_MSL_CHECK(strstr(red_metal_src, "_ltid") != NULL);
    LEGACY_MSL_CHECK(strstr(red_metal_src, "threadgroup_barrier") != NULL);
    LEGACY_MSL_CHECK(strstr(red_metal_src, "if (_ltid == 0u)") != NULL);
    free(red_metal_src);
  }
  kernel_free_arrays(tk);
  tk->axes = NULL;

  TEST_BEGIN("tile-graph/multi-range-reduce-axis-jit");
  CHECK(build_scalar_reduce_2d_graph(tk, S_REDUCE_SUM) != 0);
  CHECK(tile_build_from_scalar(tk));
  CHECK(tile_collect_plan_info(tk, &info));
  CHECK(info.reduce_tile_id != 0);
  CHECK_EQ(tk->scalar_uops[info.scalar_reduce_id].src_count, 3u);
  CHECK(cg_supports_tile(tk));
  red_src = cg_emit_tile(tk);
  CHECK(red_src != NULL);
  if (red_src != NULL) {
    CHECK(strstr(red_src, "_acc") != NULL);
    free(red_src);
  }
  CHECK(cg_tile_metal_dispatch_shape(tk, &red_groups_x, &red_threads_x));
  CHECK_EQ(red_groups_x, 1u);
  CHECK_EQ(red_threads_x, 1u);
  red_metal_src = cg_emit_tile_metal(tk);
  CHECK(red_metal_src != NULL);
  if (red_metal_src != NULL) {
    LEGACY_MSL_CHECK(strstr(red_metal_src, "< 2u") != NULL);
    LEGACY_MSL_CHECK(strstr(red_metal_src, "< 3u") != NULL);
    LEGACY_MSL_CHECK(strstr(red_metal_src, "_rk") != NULL);
    free(red_metal_src);
  }
  f32 red2_in[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  f32 red2_out[1] = {0.0f};
  run_tile_jit_1(tk, red2_in, sizeof(red2_in), red2_out, sizeof(red2_out));
  CHECK(red2_out[0] == 21.0f);
  kernel_free_arrays(tk);
  tk->axes = NULL;

  TEST_BEGIN("tile-graph/c-renderer-reduce-max-axis-jit");
  CHECK(build_scalar_reduce_max_graph(tk) != 0);
  set_reduce_axes(tk, TEST_REDUCE_NO_TAIL);
  CHECK(tile_build_from_scalar(tk));
  CHECK(tile_collect_plan_info(tk, &info));
  CHECK(info.reduce_tile_id != 0);
  CHECK_EQ(tk->scalar_uops[info.scalar_reduce_id].op, S_REDUCE_MAX);
  CHECK(cg_supports_tile(tk));
  red_src = cg_emit_tile(tk);
  CHECK(red_src != NULL);
  if (red_src != NULL) {
    CHECK(strstr(red_src, "-INFINITY") != NULL);
    CHECK(strstr(red_src, "if (_rv") != NULL);
    free(red_src);
  }
  f32 red_max_in[4] = {-3.0f, 7.0f, 2.0f, 5.0f};
  red_out[0] = 0.0f;
  run_tile_jit_1(tk, red_max_in, sizeof(red_max_in), red_out, sizeof(red_out));
  CHECK(red_out[0] == 7.0f);
  kernel_free_arrays(tk);
  tk->axes = NULL;

  TEST_BEGIN("tile-graph/c-renderer-reduce-unroll-axis-jit");
  CHECK(build_scalar_reduce_sum_graph(tk) != 0);
  set_reduce_axes(tk, KAX_UNROLL);
  CHECK(tile_build_from_scalar(tk));
  CHECK(cg_supports_tile(tk));
  red_src = cg_emit_tile(tk);
  CHECK(red_src != NULL);
  if (red_src != NULL) {
    CHECK(strstr(red_src, "_acc") != NULL);
    CHECK(strstr(red_src, "for (unsigned _ta0") != NULL);
    CHECK(strstr(red_src, "for (unsigned _ta1") == NULL);
    CHECK(strstr(red_src, "for (unsigned _ta2") == NULL);
    free(red_src);
  }
  red_out[0] = 0.0f;
  run_tile_jit_1(tk, red_in, sizeof(red_in), red_out, sizeof(red_out));
  CHECK(red_out[0] == 10.0f);
  kernel_free_arrays(tk);
  tk->axes = NULL;

  TEST_BEGIN("tile-graph/c-renderer-reduce-group-axis-jit");
  CHECK(build_scalar_reduce_sum_graph(tk) != 0);
  set_reduce_axes(tk, KAX_GROUP_REDUCE);
  CHECK(tile_build_from_scalar(tk));
  CHECK(cg_supports_tile(tk));
  red_src = cg_emit_tile(tk);
  CHECK(red_src != NULL);
  if (red_src != NULL) {
    CHECK(strstr(red_src, "_acc") != NULL);
    CHECK(strstr(red_src, "for (unsigned _ta0") != NULL);
    CHECK(strstr(red_src, "for (unsigned _ta1") == NULL);
    CHECK(strstr(red_src, "for (unsigned _ta2") == NULL);
    free(red_src);
  }
  red_out[0] = 0.0f;
  run_tile_jit_1(tk, red_in, sizeof(red_in), red_out, sizeof(red_out));
  CHECK(red_out[0] == 10.0f);
  kernel_free_arrays(tk);

  thvm_free();
  TEST_REPORT();
}
