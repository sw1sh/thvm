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
static char *render_lin_ptx(LinKernel const *lk, const char *name, int sm) {
  static char buf[1 << 18];
  FILE *fp = fmemopen(buf, sizeof(buf) - 1, "w");
  if (fp == NULL) return NULL;
  int ok = cg_render_linearized_ptx(lk, name, sm, fp);
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

static char *render_ptx_str(Term sink, const char *name, int sm) {
  LinKernel lk;
  if (!uop_linearize(sink, &lk)) return NULL;
  return render_lin_ptx(&lk, name, sm);
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
  // Exactly ONE serial reduce loop (the real Cin loop, extent 4), and it
  // closes at its END, right after the accumulate.
  CHECK(has(ptx, "setp.lt.s32"));
  CHECK(has(ptx, ", 4;"));
  CHECK(has(ptx, "LOOP_0:"));
  CHECK(has(ptx, "END_0:"));
  CHECK(lacks(ptx, "LOOP_1:"));   // no spurious second loop
  free(ptx);

  thvm_free();
  TEST_REPORT();
}

// Render STORE(out[i], CONST) for an `dt` output buffer.
static char *render_const_fill(u32 dt, u32 bits, const char *name) {
  u32 dims[1] = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, dims, 0);
  Term r       = uop_range(0, KAX_LOOP, 8);
  Term store   = uop_store(out_buf, uop_index_e(out_buf, r), uop_const(dt, bits));
  return render_ptx_str(store, name, 70);
}

// Case 6: float CONST immediates follow tinygrad render_val
// (renderer/ptx.py:11-16).  The payload holds the value's f32 bits, so
// f16 renders its half encoding, f64 its double encoding, and every
// other float (bf16 included) the f32 bits.
static int test_case6_float_const_immediates(void) {
  thvm_init();
  TEST_BEGIN("case6 float CONST immediates follow render_val");

  char *h = render_const_fill(DT_FP16, 0x3F800000u, "fill_f16");
  CHECK(h != NULL);
  CHECK(has(h, "0x3C00"));
  free(h);

  char *b = render_const_fill(DT_BF16, 0x3F800000u, "fill_bf16");
  CHECK(b != NULL);
  CHECK(has(b, "0f3F800000"));
  CHECK(lacks(b, "1065353216"));
  free(b);

  char *d = render_const_fill(DT_FP64, 0x3F800000u, "fill_f64");
  CHECK(d != NULL);
  CHECK(has(d, "0d3FF0000000000000"));
  free(d);

  // A CAST(CONST) fold feeds the same immediate.
  u32 dims[1] = { 8 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP16, 1, dims, 0);
  Term r       = uop_range(0, KAX_LOOP, 8);
  Term val     = uop_cast(uop_const(DT_FP32, 0x3F800000u), DT_FP16);
  char *c = render_ptx_str(uop_store(out_buf, uop_index_e(out_buf, r), val),
                           "fill_cast_f16", 70);
  CHECK(c != NULL);
  CHECK(has(c, "0x3C00"));
  free(c);

  thvm_free();
  TEST_REPORT();
}

static Term imad_const(Term a, u32 c, Term b) {
  return uop_int_binary(UOP_IADD,
           uop_int_binary(UOP_IMUL, a, uop_const(DT_INT32, c)), b);
}

// Case 7: a nested reduce whose outer body reaches its own axis only
// through the inner reduce, out[i] = sum_j 2 * sum_k z[i*6 + j*3 + k].
// Each loop closes at the END that names it (tinygrad renderer/ptx.py:
// 126-130), so the outer update runs once per j: inside the j loop,
// after the k loop.  The last node that reads j is the inner load, so a
// j loop closed after its last use would leave the outer update after
// the loop, adding only the last j's inner sum.
static int test_case7_nested_reduce_closes_at_end(void) {
  thvm_init();
  TEST_BEGIN("case7 nested reduce: the outer update runs inside its own loop");

  u32 dout[1] = { 4 };
  u32 dz[1]   = { 24 };
  Term O  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dout, 0);
  Term Z  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dz, 1);
  Term ri = uop_range(0, KAX_LOOP, 4);
  Term rj = uop_range(1, KAX_REDUCE, 2);
  Term rk = uop_range(2, KAX_REDUCE, 3);
  Term addr  = imad_const(ri, 6, imad_const(rj, 3, rk));
  Term inner = uop_reduce(REDUCE_SUM, 2, uop_load(uop_index_e(Z, addr)));
  Term outer = uop_reduce(REDUCE_SUM, 1,
                 uop_binary(UOP_MUL, inner, uop_const(DT_FP32, 0x40000000u)));
  Term st    = uop_store(O, uop_index_e(O, ri), outer);

  char *ptx = render_ptx_str(uop_devectorize_graph(st), "nested", 70);
  CHECK(ptx != NULL);
  if (ptx == NULL) {
    thvm_free();
    TEST_REPORT();
  }
  // i indexes the store (a thread axis); j and k are serial loops, k
  // nested in j.
  const char *loop_j = strstr(ptx, "LOOP_0:");
  const char *loop_k = strstr(ptx, "LOOP_1:");
  const char *back_k = strstr(ptx, "bra LOOP_1;");
  const char *end_j  = strstr(ptx, "END_0:");
  const char *back_j = strstr(ptx, "bra LOOP_0;");
  CHECK(loop_j != NULL && loop_k != NULL && back_k != NULL
        && end_j != NULL && back_j != NULL);
  CHECK(lacks(ptx, "LOOP_2:"));
  CHECK(loop_j < loop_k && loop_k < back_k && back_k < end_j && end_j < back_j);
  // The accumulator initialised before the j loop is the outer one; the
  // inner one is initialised inside the j loop, before the k loop.
  u32 outer_acc = 0;
  const char *init_outer = strstr(ptx, "mov.b32 %acc_f32_");
  CHECK(init_outer != NULL && init_outer < loop_j);
  CHECK(init_outer != NULL
        && sscanf(init_outer, "mov.b32 %%acc_f32_%u", &outer_acc) == 1);
  char want[48];
  snprintf(want, sizeof(want), "mov.b32 %%acc_f32_%u, %%const_f32_", outer_acc ^ 1u);
  const char *init_inner = strstr(ptx, want);
  CHECK(init_inner != NULL && loop_j < init_inner && init_inner < loop_k);
  snprintf(want, sizeof(want), "mov.b32 %%acc_f32_%u, %%alu_f32_", outer_acc);
  const char *upd_outer = strstr(ptx, want);
  CHECK(upd_outer != NULL && back_k < upd_outer && upd_outer < end_j);
  const char *store = strstr(ptx, "st.global.f32");
  CHECK(store != NULL && back_j < store);
  free(ptx);

  thvm_free();
  TEST_REPORT();
}

// Case 8: sibling reduces, out[i] = sum_j a[i*4 + j] + sum_k b[i*3 + k].
// An END closes the loops it names, so they must be the innermost open
// loops.  thvm's linearizer does not add tinygrad's sibling-loop edges
// (linearizer.py:72-81), so no dependency keeps the k loop from opening
// right after the j loop; the j END would then close j around the open
// k loop and run the k reduce once per j.  The renderer declines that
// order, as the C renderer does.
static int test_case8_misnested_end_declines(void) {
  thvm_init();
  TEST_BEGIN("case8 an END that does not close the innermost loop declines");

  u32 dout[1] = { 4 };
  u32 da[1]   = { 16 };
  u32 db[1]   = { 12 };
  Term O  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dout, 0);
  Term A  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, da, 1);
  Term B  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, db, 2);
  Term ri = uop_range(0, KAX_LOOP, 4);
  Term rj = uop_range(1, KAX_REDUCE, 4);
  Term rk = uop_range(2, KAX_REDUCE, 3);
  Term sj = uop_reduce(REDUCE_SUM, 1, uop_load(uop_index_e(A, imad_const(ri, 4, rj))));
  Term sk = uop_reduce(REDUCE_SUM, 2, uop_load(uop_index_e(B, imad_const(ri, 3, rk))));
  Term st = uop_store(O, uop_index_e(O, ri), uop_binary(UOP_ADD, sj, sk));

  static LinKernel lk;
  CHECK(uop_linearize(uop_devectorize_graph(st), &lk));
  char *ptx = render_lin_ptx(&lk, "siblings", 70);
  CHECK(ptx != NULL);
  free(ptx);

  u32 p_first  = 0xFFFFFFFFu;
  u32 p_second = 0xFFFFFFFFu;
  u32 p_end    = 0xFFFFFFFFu;
  for (u32 i = 0; i < lk.n; i++) {
    Term t = lk.uops[i];
    if (t == rj || t == rk) {
      if (p_first == 0xFFFFFFFFu) {
        p_first = i;
      } else {
        p_second = i;
      }
    }
    if (term_ext(t) == UOP_END && p_end == 0xFFFFFFFFu) {
      p_end = i;
    }
  }
  CHECK(p_first < p_end && p_end < p_second && p_second < lk.n);
  if (p_second >= lk.n) {
    thvm_free();
    TEST_REPORT();
  }
  Term second = lk.uops[p_second];
  memmove(&lk.uops[p_first + 2], &lk.uops[p_first + 1],
          (size_t)(p_second - p_first - 1) * sizeof(Term));
  lk.uops[p_first + 1] = second;
  CHECK(render_lin_ptx(&lk, "siblings", 70) == NULL);

  static char cbuf[16384];
  FILE *fp = fmemopen(cbuf, sizeof(cbuf) - 1, "w");
  CHECK(fp != NULL);
  if (fp != NULL) {
    CHECK_EQ(cg_render_linearized_c(&lk, "siblings", fp), 0);
    fclose(fp);
  }

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
  rc |= test_case6_float_const_immediates();
  rc |= test_case7_nested_reduce_closes_at_end();
  rc |= test_case8_misnested_end_declines();
  return rc;
}
