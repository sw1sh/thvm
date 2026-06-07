// M2: a symbolic-M matmul ({S,K}.{K,N} = {S,N}) dispatched through GEMM at a
// runtime-bound S.  Exercises blas_try_gemm resolving the output leading dim
// M via kvar_extent_runtime: the buffer is sized at the kvar upper bound,
// GEMM writes the first S rows.
#include "../src/thvm.c"
#include "test.h"
static u32 ten(u32 d0, u32 d1, float fill) {
  Shape sh = {0}; sh.ndim = 2; sh.dims[0] = d0; sh.dims[1] = d1;
  u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  u32 n = TENS[t].view.numel; float *b = malloc(n*4);
  for (u32 i=0;i<n;i++) b[i]=fill; CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4); free(b);
  return t;
}
// realize {S,N} matmul of ones at S=bound; returns (#mismatches, gemm fired).
static int check(u32 s, u32 bound, u32 K, u32 N, u64 *gemm) {
  u32 S = kvar_pack_extent(s);
  Term A = term_new(0,TAG_TEN,DT_FP32, ten(S, K, 1.0f));
  Term B = term_new(0,TAG_TEN,DT_FP32, ten(K, N, 1.0f));
  u32 sk1[3]={S,K,1}, skn[3]={S,K,N}, _kn[3]={1,K,N};
  Term ae = uop_expand(uop_reshape(A,3,sk1), 3, skn);
  Term be = uop_expand(uop_reshape(B,3,_kn), 3, skn);
  Term mm = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, ae, be));  // {S,N}
  kvar_set_runtime(s, bound);
  u64 g0 = cpu_blas_gemm_dispatch_dag_count();
  Term r = term_resolve(thvm_realize(mm));
  *gemm = cpu_blas_gemm_dispatch_dag_count() - g0;
  float *out = malloc((u64)bound*N*4);
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id, out, (u64)bound*N*4);
  int bad = 0;
  for (u32 i=0;i<bound*N;i++) if (!(out[i] > K-0.1f && out[i] < K+0.1f)) bad++;
  free(out); return bad;
}
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("m2/matmul-symbolic-M-gemm");
  u64 g5, g2;
  int bad5 = check(kvar_alloc("s1",1,128), 40, 64, 64, &g5);   // S=40
  int bad2 = check(kvar_alloc("s2",1,128), 96, 64, 64, &g2);   // S=96
  printf("  S=40: %d mismatches gemm=%llu   S=96: %d mismatches gemm=%llu\n",
         bad5, (unsigned long long)g5, bad2, (unsigned long long)g2);
  if (bad5 || bad2) f++;
  if (g5 == 0 || g2 == 0) { printf("  (note: GEMM did not fire -- fix unexercised)\n"); f++; }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
