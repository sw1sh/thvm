// test_render_uop_cuda.c - Stage 1: CUDA render path.
//
// Mirrors test_render_uop_metal.c but for the CUDA target.  Builds the
// canonical matmul / softmax-tail / reduce UOp DAGs, renders each via
// cg_render_uop_kernel_cuda_root, and string-checks the output
// structure: the `extern "C" __global__` signature, the blockIdx/
// blockDim/threadIdx thread-builtin prologue, and the CUDA-specific
// OPT lowerings (WMMA fragments, __shfl_down_sync, __exp2f, float4).
//
// This is render-only.  There is no CUDA toolchain on the dev Mac, so
// the emitted `.cu` is never compiled or run -- the rendered string is
// the assertion surface, exactly as test_render_uop.c's MSL grep
// fixtures.  Stage 2 (the src/backend/cuda/ runtime, pod-dependent)
// adds nvrtc compilation + dispatch.

#include "../src/thvm.c"
#include "test.h"

static int contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

// Render `root` through the structural CUDA entry point into a heap
// buffer.  Caller frees.
static char *render_cuda(Term root, const char *name) {
  char *buf = NULL;
  size_t sz = 0;
  FILE *fp = open_memstream(&buf, &sz);
  if (fp == NULL) return NULL;
  cg_render_uop_kernel_cuda_root(root, name, fp);
  fclose(fp);
  return buf;
}

int main(void) {
  thvm_init();
  // CONV-BWD REDUCE TILING knob ON for the whole binary.  The renderer
  // memoises it on first read; set it up front.  The multi-axis
  // warp-stripe it enables is narrowly gated (SIMD_REDUCE-wrapped AND
  // >= 2 reduce axes AND outer extent % 32 == 0), so no other test's
  // reduce shape is perturbed -- the single-axis SIMD test and the
  // non-SIMD nested-reduce test both fall outside the gate.
  setenv("THVM_CONV_BWD_REDUCE_TILING", "1", 1);

  // === Signature + prologue ==========================================
  TEST_BEGIN("render-uop-cuda/elementwise-signature-and-prologue");
  // STORE(out[r], ADD(MUL(in0[r], in1[r]), 1.0f)) -- 1-output, 2-input.
  {
    u32 dimsOut[1] = { 256 };
    u32 dimsIn0[1] = { 256 };
    u32 dimsIn1[1] = { 256 };
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsOut, 0);
    Term in0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsIn0, 1);
    Term in1 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsIn1, 2);
    Term r0  = uop_range(0, KAX_LOOP, 256);
    Term lda = uop_index_e(in0, r0);
    Term ldb = uop_index_e(in1, r0);
    Term mul = uop_binary(UOP_MUL, lda, ldb);
    Term one = uop_const(DT_FP32, 0x3F800000u);
    Term add = uop_binary(UOP_ADD, mul, one);
    Term st  = uop_store(out, r0, add);
    char *cu = render_cuda(st, "k_elw");
    CHECK(cu != NULL);
    // extern "C" __global__ signature, plain pointer args, no Metal
    // address-space qualifiers or [[ buffer(N) ]] attributes.
    CHECK(contains(cu, "extern \"C\" __global__ void k_elw"));
    CHECK(contains(cu, "float *out"));
    CHECK(contains(cu, "const float *in0"));
    CHECK(contains(cu, "const float *in1"));
    CHECK(!contains(cu, "[[ buffer"));
    CHECK(!contains(cu, "metal_stdlib"));
    CHECK(!contains(cu, "device "));
    // Thread-builtin prologue from CUDA launch builtins.
    CHECK(contains(cu, "blockIdx.x * blockDim.x + threadIdx.x"));
    CHECK(contains(cu, "uint tg  = blockIdx.x;"));
    CHECK(contains(cu, "uint tt  = threadIdx.x;"));
    CHECK(contains(cu, "uint sgi = threadIdx.x / 32u;"));
    free(cu);
  }

  // === Empty kernel ==================================================
  TEST_BEGIN("render-uop-cuda/empty-root");
  {
    char *cu = render_cuda(/*root=*/0, "k_empty");
    CHECK(cu != NULL);
    CHECK(contains(cu, "extern \"C\" __global__ void k_empty"));
    CHECK(contains(cu, "/* empty kernel */"));
    free(cu);
  }

  // === Reduce (vector sum) ===========================================
  TEST_BEGIN("render-uop-cuda/reduce-sum-scalar-accumulator");
  {
    // STORE(out[0], REDUCE(SUM, in[k], k)).
    u32 dimsIn[1]  = { 64 };
    u32 dimsOut[1] = { 1 };
    Term in0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsIn,  1);
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsOut, 0);
    Term r_k = uop_range(0, KAX_REDUCE, 64);
    Term ld  = uop_index_e(in0, r_k);
    Term red = uop_reduce(REDUCE_SUM, /*axis=*/0, ld);
    Term zero = uop_const(DT_INT32, 0);
    Term st  = uop_store(out, zero, red);
    char *cu = render_cuda(st, "k_red");
    CHECK(cu != NULL);
    CHECK(contains(cu, "extern \"C\" __global__ void k_red"));
    // Scalar accumulator path: declared accumulator + reduce loop.
    CHECK(contains(cu, "float _acc0 = "));
    free(cu);
  }

  // === signed integer subtraction =================================
  TEST_BEGIN("render-uop-cuda/isub-is-signed");
  {
    // STORE(out[i], IWHERE(ILT(-1, i-1), 1.0f, 0.0f)).  UOP_ISUB is a
    // SIGNED subtract; RANGE vars are `uint`, so `a0 - 1` at a0==0
    // wraps to ~UINT_MAX and the guard `-1 < (a0-1)` (with -1 promoted
    // to UINT_MAX) is always false on the CUDA target -- nvrtc applies
    // the same unsigned-promotion rule.  The renderer casts each
    // UOP_I* operand to `int` so the comparison is signed-correct.
    u32 dimsOut[1] = { 8 };
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsOut, 0);
    Term r0  = uop_range(0, KAX_LOOP, 8);
    Term sub = uop_int_binary(UOP_ISUB, r0, uop_const(DT_INT32, 1));
    Term grd = uop_int_binary(UOP_ILT, uop_const(DT_INT32, 0xFFFFFFFFu),
                              sub);
    Term sel = uop_iwhere(grd, uop_const(DT_FP32, 0x3F800000u),
                          uop_const(DT_FP32, 0u));
    Term st  = uop_store(out, r0, sel);
    char *cu = render_cuda(st, "k_isub");
    CHECK(cu != NULL);
    CHECK(contains(cu, "(int)(a0) - (int)(1)"));
    CHECK(contains(cu, "(int)(-1) < "));
    CHECK(!contains(cu, "(a0 - 1)"));
    free(cu);
  }

  // === nested reduce: inner reduce uses outer reduce's axis var =====
  TEST_BEGIN("render-uop-cuda/nested-reduce-axis-dep-nests-loops");
  {
    // out[i] = sum_j(in[i] * sum_k m[j,k]).  The inner REDUCE over k is
    // wrapped in an elementwise MUL inside the outer REDUCE over j; its
    // body m[j,k] indexes by the OUTER reduce's axis var `a1` (j).  The
    // inner reduce's accumulator loop must emit INSIDE the outer loop --
    // a flat hoist references `a1` before its declaration, which nvrtc
    // rejects with `identifier "a1" is undefined`.  This shape underlies
    // scaled-dot-product attention (softmax-over-scores).
    u32 dN_i[1] = { 4 };
    u32 dN_m[2] = { 3, 5 };
    Term out_n = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dN_i, 0);
    Term in_n  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dN_i, 1);
    Term m_n   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dN_m, 2);
    Term ri_n = uop_range(0, KAX_LOOP,   4);
    Term rj_n = uop_range(1, KAX_REDUCE, 3);
    Term rk_n = uop_range(2, KAX_REDUCE, 5);
    Term jk_n = uop_int_binary(
        UOP_IADD,
        uop_int_binary(UOP_IMUL, rj_n, uop_const(DT_INT32, 5)), rk_n);
    Term inner_n = uop_reduce(REDUCE_SUM, /*axis=*/2,
                              uop_index_e(m_n, jk_n));
    Term body_n  = uop_binary(UOP_MUL, uop_index_e(in_n, ri_n), inner_n);
    Term outer_n = uop_reduce(REDUCE_SUM, /*axis=*/1, body_n);
    Term st_n    = uop_store(out_n, ri_n, outer_n);
    char *cu = render_cuda(st_n, "k_nested_red");
    CHECK(cu != NULL);
    char *p_a1  = strstr(cu, "for (uint a1 = 0");
    char *p_a2  = strstr(cu, "for (uint a2 = 0");
    char *p_ac2 = strstr(cu, "float _acc2");
    CHECK(p_a1 != NULL);
    CHECK(p_a2 != NULL);
    CHECK(p_ac2 != NULL);
    // Outer loop opens before the inner accumulator is declared, and
    // the inner accumulator is declared before its own loop opens --
    // i.e. the inner reduce is nested inside the outer reduce's loop.
    CHECK(p_a1 < p_ac2);
    CHECK(p_ac2 < p_a2);
    // The inner reduce body references the outer reduce's axis var `a1`,
    // in scope only because the `a2` loop nests inside the `a1` loop.
    CHECK(contains(cu, "_acc2 = _acc2 + in1["));
    char *p_inner_body = strstr(cu, "_acc2 = _acc2 + in1[");
    CHECK(p_inner_body != NULL && strstr(p_inner_body, "a1") != NULL
          && strstr(p_inner_body, "a1") < strstr(p_inner_body, ";"));
    free(cu);
  }

  // === SIMD_REDUCE -> __shfl_xor_sync warp all-reduce ================
  TEST_BEGIN("render-uop-cuda/simd-reduce-emits-shfl-xor-sync");
  {
    // Same vector-sum DAG, wrapped in OPT(_, SIMD_REDUCE, _).
    u32 dimsIn[1]  = { 64 };
    u32 dimsOut[1] = { 1 };
    Term in0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsIn,  1);
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsOut, 0);
    Term r_k = uop_range(0, KAX_REDUCE, 64);
    Term ld  = uop_index_e(in0, r_k);
    Term red = uop_reduce(REDUCE_SUM, /*axis=*/0, ld);
    Term zero = uop_const(DT_INT32, 0);
    Term st  = uop_store(out, zero, red);
    Term st_simd = uop_dag_apply_simd_reduce(st);
    CHECK(st_simd != 0);
    CHECK(st_simd != st);
    char *cu = render_cuda(st_simd, "k_simd");
    CHECK(cu != NULL);
    // Per-lane strided loop: lane index = threadIdx.x % 32, stride 32.
    CHECK(contains(cu, "(threadIdx.x % 32u)"));
    CHECK(contains(cu, "+= 32u"));
    // 5-step warp butterfly: __shfl_xor_sync at offsets 16..1.  xor
    // (not down) is an all-reduce -- every lane ends with the full
    // result, matching Metal simd_sum's broadcast, so the unguarded
    // per-lane store of the reduced value is race-free.
    CHECK(contains(cu, "__shfl_xor_sync(0xffffffffu, _acc0, 16u)"));
    CHECK(contains(cu, "__shfl_xor_sync(0xffffffffu, _acc0, 8u)"));
    CHECK(contains(cu, "__shfl_xor_sync(0xffffffffu, _acc0, 4u)"));
    CHECK(contains(cu, "__shfl_xor_sync(0xffffffffu, _acc0, 2u)"));
    CHECK(contains(cu, "__shfl_xor_sync(0xffffffffu, _acc0, 1u)"));
    // A down-shuffle would leave the result in lane 0 only.
    CHECK(!contains(cu, "__shfl_down_sync"));
    // No Metal simd_sum on the CUDA path.
    CHECK(!contains(cu, "simd_sum"));
    CHECK(!contains(cu, "thread_index_in_simdgroup"));
    free(cu);
  }

  // === FAST_MATH -> __exp2f intrinsic ================================
  TEST_BEGIN("render-uop-cuda/fast-math-emits-exp2f");
  {
    // STORE(out[r], OPT(EXP2(in[r]), FAST_MATH, 0)) -- the softmax tell.
    u32 dims[1] = { 16 };
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
    Term in0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
    Term r0  = uop_range(0, KAX_LOOP, 16);
    Term ld  = uop_index_e(in0, r0);
    Term ex  = uop_unary(UOP_EXP2, ld);
    Term st  = uop_store(out, r0, ex);
    // Pre-rewrite: bare exp2(, no fast intrinsic.
    char *cu0 = render_cuda(st, "k_pre");
    CHECK(cu0 != NULL);
    CHECK(contains(cu0, "exp2("));
    CHECK(!contains(cu0, "__exp2f("));
    free(cu0);
    KOpt opt = { KOP_FAST_MATH, 0, 0 };
    Term st_fm = uop_dag_apply_kopt(st, opt);
    CHECK(st_fm != 0);
    CHECK(st_fm != st);
    char *cu = render_cuda(st_fm, "k_fm");
    CHECK(cu != NULL);
    // CUDA fast-math intrinsic spelling, not Metal's fast::exp2.
    CHECK(contains(cu, "__exp2f("));
    CHECK(!contains(cu, "fast::exp2"));
    free(cu);
  }

  // === VEC_LOAD -> float4 reinterpret ================================
  TEST_BEGIN("render-uop-cuda/vec-load-emits-float4-cast");
  {
    // STORE(out[m*16+n], OPT(in0[m*16+n], VEC_LOAD, 4)).
    u32 dims[2] = { 16, 16 };
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims, 0);
    Term in0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims, 1);
    Term r_m = uop_range(0, KAX_LOOP, 16);
    Term r_n = uop_range(1, KAX_LOOP, 16);
    Term k16 = uop_const(DT_INT32, 16);
    Term addr = uop_int_binary(UOP_IADD,
                               uop_int_binary(UOP_IMUL, r_m, k16), r_n);
    Term ld  = uop_index_e(in0, addr);
    Term st  = uop_store(out, addr, ld);
    Term st_vl = uop_dag_apply_vec_load(st, 4);
    CHECK(st_vl != 0);
    CHECK(st_vl != st);
    char *cu = render_cuda(st_vl, "k_vl");
    CHECK(cu != NULL);
    // float4 reinterpret cast WITHOUT the Metal `device ` qualifier.
    CHECK(contains(cu, "(const float4*)"));
    CHECK(!contains(cu, "(device const float4*)"));
    free(cu);
  }

  // === OPT_TC -> WMMA fragments (conforming 16x16x16 matmul) =========
  TEST_BEGIN("render-uop-cuda/tc-matmul-emits-wmma");
  {
    // STORE(C[m*N+n], OPT(REDUCE(MUL(A[m*K+k], B[k*N+n]), SUM, k), TC, 0))
    // M=16, N=16, K=32 -- all multiples of 16 -> WMMA fires.
    // A/B are fp16: on the Volta V100 target WMMA matrix fragments are
    // fp16-only (no tf32), so the tensor-core template is gated to
    // fp16-typed A/B buffers; C accumulates in fp32 (the accumulator
    // fragment dtype, correct on Volta).  An fp32 A/B matmul takes the
    // scalar fallback -- see the next test.
    u32 dimsC[2] = { 16, 16 };
    u32 dimsA[2] = { 16, 32 };
    u32 dimsB[2] = { 32, 16 };
    Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsC, 0);
    Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP16, 2, dimsA, 1);
    Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP16, 2, dimsB, 2);
    Term r_m = uop_range(0, KAX_LOOP, 16);
    Term r_n = uop_range(1, KAX_LOOP, 16);
    Term r_k = uop_range(2, KAX_REDUCE, 32);
    Term k16 = uop_const(DT_INT32, 16);
    Term k32 = uop_const(DT_INT32, 32);
    Term addrA = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, k32), r_k);
    Term ldA   = uop_index_e(A, addrA);
    Term addrB = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_k, k16), r_n);
    Term ldB   = uop_index_e(B, addrB);
    Term mul   = uop_binary(UOP_MUL, ldA, ldB);
    Term red   = uop_reduce(REDUCE_SUM, /*axis=*/2, mul);
    // BARE matmul (no pre-installed OPT(_, TC, 0)): mirrors the real
    // lifted store_root.  cg_render_uop_kernel_cuda_root applies
    // uop_recognise_tc_parallel itself (stamping M/N GLOBAL + wrapping
    // the REDUCE), which a pre-wrapped root would suppress (the
    // already-TC value is not re-classified as a bare matmul).
    Term addrC = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, k16), r_n);
    Term st    = uop_store(C, addrC, red);
    char *cu = render_cuda(st, "k_gemm");
    CHECK(cu != NULL);
    // WMMA headers emitted because the DAG carries a TC annotation.
    CHECK(contains(cu, "#include <mma.h>"));
    CHECK(contains(cu, "#include <cuda_fp16.h>"));
    CHECK(contains(cu, "using namespace nvcuda;"));
    // The CUDA renderer applies uop_recognise_tc_parallel, stamping M/N
    // GLOBAL, so the shared-staged register-blocked tiled WMMA emit fires
    // (the 16x16x16 minimum tile here: lm=ln=rm=rn=1, KB=32).
    CHECK(contains(cu, "/* TC WMMA matmul (nvcuda::wmma 16x16x16) -- tiled */"));
    CHECK(contains(cu, "/* TC WMMA tiled matmul"));
    CHECK(contains(cu, "wmma::fragment<wmma::matrix_a, 16, 16, 16, half, wmma::row_major>"));
    CHECK(contains(cu, "wmma::fragment<wmma::matrix_b, 16, 16, 16, half, wmma::row_major>"));
    CHECK(contains(cu, "wmma::fragment<wmma::accumulator, 16, 16, 16, float> _acc"));
    CHECK(contains(cu, "wmma::fill_fragment(_acc"));
    CHECK(contains(cu, "wmma::load_matrix_sync(_af[0]"));
    CHECK(contains(cu, "wmma::load_matrix_sync(_bf[0]"));
    CHECK(contains(cu, "wmma::mma_sync(_acc[0], _af[0], _bf[0], _acc[0])"));
    // f32 output -> store_matrix_sync directly to C.
    CHECK(contains(cu, "wmma::store_matrix_sync(&out"));
    // Shared staging tiles + barriers; tile decoded from the block index.
    CHECK(contains(cu, "__shared__ half _Asm"));
    CHECK(contains(cu, "__shared__ half _Bsm"));
    CHECK(contains(cu, "__syncthreads();"));
    CHECK(contains(cu, "uint _tm = (tg / "));
    // K-block loop steps by KB=32 (the staged K-block).
    CHECK(contains(cu, "_k0 += 32u"));
    // Inner K-subtile loop steps by 16 (the WMMA K-tile).
    CHECK(contains(cu, "_kk += 16u"));
    // Not the Metal simdgroup_matrix template.
    CHECK(!contains(cu, "simdgroup_matrix"));
    free(cu);
  }

  // === OPT_TC fallback -> scalar accumulator (non-16 shape) ==========
  TEST_BEGIN("render-uop-cuda/tc-non-16-multiple-falls-back-to-scalar");
  {
    // K=24 is a multiple of 8 (so rmu_detect_matmul_tc fires) but NOT
    // of 16 -> WMMA declines, scalar accumulator path emits instead.
    u32 dimsC[2] = { 16, 16 };
    u32 dimsA[2] = { 16, 24 };
    u32 dimsB[2] = { 24, 16 };
    Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsC, 0);
    Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsA, 1);
    Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsB, 2);
    Term r_m = uop_range(0, KAX_LOOP, 16);
    Term r_n = uop_range(1, KAX_LOOP, 16);
    Term r_k = uop_range(2, KAX_REDUCE, 24);
    Term k16 = uop_const(DT_INT32, 16);
    Term k24 = uop_const(DT_INT32, 24);
    Term addrA = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, k24), r_k);
    Term ldA   = uop_index_e(A, addrA);
    Term addrB = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_k, k16), r_n);
    Term ldB   = uop_index_e(B, addrB);
    Term mul   = uop_binary(UOP_MUL, ldA, ldB);
    Term red   = uop_reduce(REDUCE_SUM, /*axis=*/2, mul);
    Term addrC = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, k16), r_n);
    Term st    = uop_store(C, addrC, red);  /* bare matmul */
    char *cu = render_cuda(st, "k_gemm_fb");
    CHECK(cu != NULL);
    // WMMA declined -> the TC-tile-mismatch fallback marker + scalar
    // accumulator, exactly as the Metal path falls back from
    // simdgroup_matrix on a non-conforming shape.
    CHECK(contains(cu, "/* TC tile mismatch"));
    CHECK(!contains(cu, "wmma::"));
    CHECK(contains(cu, "float _acc2 = "));
    free(cu);
  }

  // === OPT_TC fp32 buffers -> scalar fallback (Volta WMMA is fp16) ===
  TEST_BEGIN("render-uop-cuda/tc-fp32-buffers-fall-back-to-scalar");
  {
    // Conforming 16x16x16 shape, but A/B are fp32.  On the Volta V100
    // target WMMA matrix fragments are fp16-only -- so even a clean
    // 16-multiple shape declines the tensor-core template when the
    // source buffers are fp32, and the scalar accumulator emits.
    u32 dimsC[2] = { 16, 16 };
    u32 dimsA[2] = { 16, 32 };
    u32 dimsB[2] = { 32, 16 };
    Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsC, 0);
    Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsA, 1);
    Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsB, 2);
    Term r_m = uop_range(0, KAX_LOOP, 16);
    Term r_n = uop_range(1, KAX_LOOP, 16);
    Term r_k = uop_range(2, KAX_REDUCE, 32);
    Term k16 = uop_const(DT_INT32, 16);
    Term k32 = uop_const(DT_INT32, 32);
    Term addrA = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, k32), r_k);
    Term ldA   = uop_index_e(A, addrA);
    Term addrB = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_k, k16), r_n);
    Term ldB   = uop_index_e(B, addrB);
    Term mul   = uop_binary(UOP_MUL, ldA, ldB);
    Term red   = uop_reduce(REDUCE_SUM, /*axis=*/2, mul);
    Term addrC = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, k16), r_n);
    Term st    = uop_store(C, addrC, red);  /* bare matmul */
    char *cu = render_cuda(st, "k_gemm_fp32");
    CHECK(cu != NULL);
    CHECK(contains(cu, "/* TC tile mismatch"));
    CHECK(!contains(cu, "wmma::"));
    CHECK(contains(cu, "float _acc2 = "));
    free(cu);
  }

  // === bitcast -> __uint_as_float (CUDA, not Metal as_type) ==========
  TEST_BEGIN("render-uop-cuda/fp32-const-emits-uint-as-float");
  {
    // STORE(out[r], ADD(in0[r], 1.0f)) -- the 1.0f const renders as a
    // bit-exact bitcast.  CUDA spells it __uint_as_float (Metal uses
    // as_type<float>, nvrtc has neither as_type nor that spelling).
    u32 dims[1] = { 8 };
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
    Term in0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
    Term r0  = uop_range(0, KAX_LOOP, 8);
    Term ld  = uop_index_e(in0, r0);
    Term one = uop_const(DT_FP32, 0x3F800000u);
    Term add = uop_binary(UOP_ADD, ld, one);
    Term st  = uop_store(out, r0, add);
    char *cu = render_cuda(st, "k_bc");
    CHECK(cu != NULL);
    CHECK(contains(cu, "__uint_as_float(0x3f800000u)"));
    CHECK(!contains(cu, "as_type<float>"));
    free(cu);
  }

  // === CONV-BWD REDUCE TILING: multi-axis SIMD reduce warp-stripe =====
  TEST_BEGIN("render-uop-cuda/conv-bwd-reduce-tiling-warp-stripes-outer-axis");
  {
    // out[c] = sum_{b,k}(in[b,k] indexed by (b*64+k)) -- a 2-axis reduce
    // over (b=64, k=4): the conv-weight-grad shape in miniature (reduce
    // over batch + spatial).  Wrapped in OPT(_, SIMD_REDUCE, _).  With
    // THVM_CONV_BWD_REDUCE_TILING=1 the OUTER reduce axis (b, extent 64,
    // %32==0) is striped across the 32 warp lanes; the inner axis (k)
    // stays a serial loop; the per-lane partials fold via __shfl_xor_sync.
    u32 dimsOut[1] = { 1 };
    u32 dimsIn[2]  = { 64, 4 };
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsOut, 0);
    Term in0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsIn,  1);
    Term r_b = uop_range(0, KAX_REDUCE, 64);
    Term r_k = uop_range(1, KAX_REDUCE, 4);
    Term bk  = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, r_b, uop_const(DT_INT32, 4)),
                              r_k);
    Term ld  = uop_index_e(in0, bk);
    u32 axes[2] = { 0, 1 };
    Term red = uop_reduce_multi(REDUCE_SUM, 2, axes, ld);
    Term zero = uop_const(DT_INT32, 0);
    Term st   = uop_store(out, zero, red);
    Term st_simd = uop_dag_apply_simd_reduce(st);
    CHECK(st_simd != 0);
    CHECK(st_simd != st);
    char *cu = render_cuda(st_simd, "k_cbt");
    CHECK(cu != NULL);
    // Outer axis (a0) is the warp stripe: lane index + stride 32, extent 64.
    CHECK(contains(cu, "for (uint a0 = (threadIdx.x % 32u); a0 < 64; a0 += 32u)"));
    // Inner axis (a1) is a plain serial loop, extent 4, nested inside.
    CHECK(contains(cu, "for (uint a1 = 0; a1 < 4"));
    char *p_a0 = strstr(cu, "for (uint a0 = (threadIdx.x % 32u)");
    char *p_a1 = strstr(cu, "for (uint a1 = 0; a1 < 4");
    CHECK(p_a0 != NULL && p_a1 != NULL && p_a0 < p_a1);   // a1 nests in a0
    // 5-step warp butterfly folds the lane partials (same as single-axis).
    CHECK(contains(cu, "__shfl_xor_sync(0xffffffffu, _acc0, 16u)"));
    CHECK(contains(cu, "__shfl_xor_sync(0xffffffffu, _acc0, 1u)"));
    // Not the Metal collective; not a down-shuffle.
    CHECK(!contains(cu, "simd_sum"));
    CHECK(!contains(cu, "__shfl_down_sync"));
    free(cu);
  }

  thvm_free();
  TEST_REPORT();
}
