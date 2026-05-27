// test_render_ptx.c - exercises src/codegen/render_ptx.c, the port of
// tinygrad/renderer/ptx.py.
//
// Each case builds a small UOp DAG, linearizes it (uop_linearize), then
// renders PTX via cg_render_linearized_ptx and asserts structural
// properties on the emitted assembly text.  We never assert exact line
// layout (register numbering is allocation-order dependent); instead we
// grep for the load-bearing instructions + labels.

#include "../src/thvm.c"
#include "test.h"

#include <string.h>

// Render a linearized kernel to a heap string (caller frees).  Returns
// NULL if the renderer bailed (opcode outside coverage).
static char *render_ptx_str(Term sink, const char *name, int sm) {
  LinKernel lk;
  if (!uop_linearize(sink, &lk)) return NULL;
  static char buf[1 << 18];
  FILE *fp = fmemopen(buf, sizeof(buf) - 1, "w");
  if (fp == NULL) return NULL;
  int ok = cg_render_linearized_ptx(&lk, name, sm, fp);
  long n = ftell(fp);
  fclose(fp);
  if (!ok) return NULL;
  if (n < 0) n = 0;
  buf[n] = 0;
  char *out = (char *)malloc((size_t)n + 1);
  if (out == NULL) return NULL;
  memcpy(out, buf, (size_t)n + 1);
  return out;
}

static int has(const char *hay, const char *needle) {
  return hay != NULL && strstr(hay, needle) != NULL;
}

static int lacks(const char *hay, const char *needle) {
  return hay != NULL && strstr(hay, needle) == NULL;
}

// Case 1: elementwise STORE(out[i], 1.0f + LOAD(in[i])).  The single
// output axis indexes the store, so it is PROMOTED to a parallel thread
// (decoded from the flat thread id) -- not a serial loop.  The canonical
// GPU elementwise shape: thread-id builtins, bounds guard, a global
// load, a float add, a global store.
static int test_case1_elementwise(void) {
  thvm_init();
  TEST_BEGIN("case1 elementwise add-const renders promoted thread PTX");

  u32 dims[1] = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term r       = uop_range(0, KAX_LOOP, 8);
  Term in_idx  = uop_index_e(in_buf, r);
  Term ld      = uop_load(in_idx);
  Term k       = uop_const(DT_FP32, 0x3f800000u);   // 1.0f
  Term add     = uop_binary(UOP_ADD, ld, k);
  Term out_idx = uop_index_e(out_buf, r);
  Term store   = uop_store(out_buf, out_idx, add);

  char *ptx = render_ptx_str(store, "ew_add", 70);
  CHECK(ptx != NULL);
  // Module prologue.
  CHECK(has(ptx, ".version"));
  CHECK(has(ptx, ".target sm_70"));
  CHECK(has(ptx, ".visible .entry ew_add"));
  CHECK(has(ptx, ".param .u64 data0"));
  CHECK(has(ptx, ".param .u64 data1"));
  // Thread-id builtins + bounds guard (promoted parallel axis).
  CHECK(has(ptx, "mov.u32 %tidx_s32_0, %tid.x;"));
  CHECK(has(ptx, "%ctaid.x"));
  CHECK(has(ptx, "%ntid.x"));
  CHECK(has(ptx, "mad.lo.s32"));
  CHECK(has(ptx, "setp.ge.s32"));
  CHECK(has(ptx, ", 8;"));            // guard against total=8
  CHECK(has(ptx, "bra DONE;"));
  CHECK(has(ptx, "DONE:"));
  // n_globals==1: the axis is the flat thread id directly.
  CHECK(has(ptx, "mov.s32 %ridx_s32_0, %gtid_s32_0;"));
  // It must NOT be a serial loop.
  CHECK(lacks(ptx, "LOOP_0:"));
  // Param loads + pointer arithmetic + memory ops.
  CHECK(has(ptx, "ld.param.u64 %dat_u64_0, [data0+0];"));
  CHECK(has(ptx, "mad.wide.s32"));
  CHECK(has(ptx, "ld.global.f32"));
  CHECK(has(ptx, "add.f32"));
  CHECK(has(ptx, "st.global.f32"));
  CHECK(has(ptx, "0f3F800000"));
  CHECK(has(ptx, "ret;"));
  free(ptx);

  thvm_free();
  TEST_REPORT();
}

// Case 2: two output axes (i, j) both index the store -> both promoted
// to a 2-D grid decoded from one flat thread id ((tid/8)%8, tid%8).  The
// index arithmetic (i*8 + j) still exercises mul.lo.s32 + add.s32.
static int test_case2_int_alu(void) {
  thvm_init();
  TEST_BEGIN("case2 2-D promoted grid: div/rem decode + mul.lo/add index");

  u32 dims[2] = { 8, 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims, 1);
  Term ri      = uop_range(0, KAX_LOOP, 8);
  Term rj      = uop_range(1, KAX_LOOP, 8);
  Term stride  = uop_int_binary(UOP_IMUL, ri, uop_const(DT_INT32, 8));
  Term addr    = uop_int_binary(UOP_IADD, stride, rj);
  Term in_idx  = uop_index_e(in_buf, addr);
  Term ld      = uop_load(in_idx);
  Term out_idx = uop_index_e(out_buf, addr);
  Term store   = uop_store(out_buf, out_idx, ld);

  char *ptx = render_ptx_str(store, "copy2d", 70);
  CHECK(ptx != NULL);
  CHECK(has(ptx, "mul.lo.s32"));
  CHECK(has(ptx, "add.s32"));
  // 2-axis grid decode: one axis via div+rem, the other via rem.
  CHECK(has(ptx, "div.s32"));
  CHECK(has(ptx, "rem.s32"));
  CHECK(has(ptx, ", 64;"));           // guard against total=8*8
  CHECK(lacks(ptx, "LOOP_0:"));
  CHECK(lacks(ptx, "LOOP_1:"));
  free(ptx);

  thvm_free();
  TEST_REPORT();
}

// Case 3: a cast f32 -> s32 must emit cvt.rzi.s32.f32 (truncate toward
// zero).  The axis is promoted (thread-decoded).
static int test_case3_cast(void) {
  thvm_init();
  TEST_BEGIN("case3 float->int cast renders cvt.rzi");

  u32 dims[1] = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_INT32, 1, dims, 0);
  Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term r       = uop_range(0, KAX_LOOP, 8);
  Term in_idx  = uop_index_e(in_buf, r);
  Term ld      = uop_load(in_idx);
  Term cast    = uop_cast(ld, DT_INT32);
  Term out_idx = uop_index_e(out_buf, r);
  Term store   = uop_store(out_buf, out_idx, cast);

  char *ptx = render_ptx_str(store, "f2i", 70);
  CHECK(ptx != NULL);
  CHECK(has(ptx, "cvt.rzi.s32.f32"));
  CHECK(has(ptx, "ld.global.f32"));
  CHECK(has(ptx, "st.global.s32"));
  CHECK(has(ptx, "mad.lo.s32"));      // promoted thread geometry
  free(ptx);

  thvm_free();
  TEST_REPORT();
}

// Case 4: a REDUCE in the kernel is outside milestone-1 coverage; the
// renderer must bail (return 0 -> render_ptx_str NULL) so the caller
// falls back to the C-source CUDA emit.
static int test_case4_reduce_bails(void) {
  thvm_init();
  TEST_BEGIN("case4 REDUCE kernel bails out of the PTX renderer");

  u32 dims_c[1] = { 8 };
  u32 dims_a[2] = { 8, 8 };
  Term cbuf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_c, 0);
  Term abuf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_a, 1);
  Term ri   = uop_range(0, KAX_LOOP, 8);
  Term rk   = uop_range(1, KAX_REDUCE, 8);
  Term stride = uop_int_binary(UOP_IMUL, ri, uop_const(DT_INT32, 8));
  Term a_addr = uop_int_binary(UOP_IADD, stride, rk);
  Term a_idx  = uop_index_e(abuf, a_addr);
  Term a_ld   = uop_load(a_idx);
  Term red    = uop_reduce(REDUCE_SUM, 1, a_ld);
  Term c_idx  = uop_index_e(cbuf, ri);
  Term store  = uop_store(cbuf, c_idx, red);

  char *ptx = render_ptx_str(store, "rowsum", 70);
  CHECK(ptx == NULL);    // bailed; caller falls back to legacy emit

  thvm_free();
  TEST_REPORT();
}

// Case 5: a devectorized reduce (row-sum) renders a PLACEHOLDER
// accumulator + serial reduce loop with UNIQUE labels (the linearized
// list can carry two RANGE nodes sharing an axis id; labels keyed on
// axis id would collide into invalid PTX).  The accumulator is a
// persistent register: mov-init, add+mov update, read-back.
static int test_case5_reduce_accumulator(void) {
  thvm_init();
  TEST_BEGIN("case5 devectorized reduce: acc register + unique loop labels");

  u32 dc[1] = { 3 };
  u32 da[2] = { 3, 4 };
  Term C  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dc, 0);
  Term A  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, da, 1);
  Term ri = uop_range(0, KAX_LOOP, 3);
  Term rk = uop_range(1, KAX_REDUCE, 4);
  Term addr = uop_int_binary(UOP_IADD,
                uop_int_binary(UOP_IMUL, ri, uop_const(DT_INT32, 4)), rk);
  Term ld  = uop_load(uop_index_e(A, addr));
  Term red = uop_reduce(REDUCE_SUM, 1, ld);
  Term st  = uop_store(C, uop_index_e(C, ri), red);
  Term devec = uop_devectorize_graph(st);

  char *ptx = render_ptx_str(devec, "rowsum", 70);
  CHECK(ptx != NULL);
  // Accumulator register present + initialised + read.
  CHECK(has(ptx, ".reg .f32 %acc_f32_"));
  CHECK(has(ptx, "mov.b32 %acc_f32_0,"));   // init / update reassign
  CHECK(has(ptx, "add.f32 %alu_f32_0, %acc_f32_0,"));  // acc + x
  // Serial reduce loop present (the real one tests against extent 4).
  CHECK(has(ptx, "setp.lt.s32"));
  CHECK(has(ptx, ", 4;"));
  // Labels must be unique: LOOP_1 exists (the second loop occurrence),
  // proving labels are keyed on occurrence, not the repeated axis id.
  CHECK(has(ptx, "LOOP_1:"));
  CHECK(has(ptx, "END_1:"));
  free(ptx);

  thvm_free();
  TEST_REPORT();
}

int main(void) {
  int rc = 0;
  rc |= test_case1_elementwise();
  rc |= test_case2_int_alu();
  rc |= test_case3_cast();
  rc |= test_case4_reduce_bails();
  rc |= test_case5_reduce_accumulator();
  return rc;
}
