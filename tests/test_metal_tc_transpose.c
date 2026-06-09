// Regression: matmul with a TRANSPOSED-view RHS must compute right on Metal.
//
// out = q{8,64} @ Transpose[kk{8,64}] -> {8,8}, the GPT-2 attention QK^T.
// Lowered (as TMatMul) to REDUCE_SUM(MUL(expand(reshape A), expand(reshape
// permute B))) and realized on Metal via uop_to_device_leaves (the TToDevice
// leaf-insertion path).  M=N=8,K=64 picks the simdgroup_matrix tensor-core
// template, which once hard-coded B's leading dim to N and no transpose --
// correct only for a contiguous {K,N} RHS.  Transpose[kk]'s real buffer (kk
// {8,64}) is laid out transposed (N-axis stride 64, K-axis stride 1), so the
// load read the wrong elements (diverged ~1.0).  Fix: read each operand's
// stride coefficients off the address and emit the leading dim + simdgroup_load
// transpose flag accordingly.  Built -DTHVM_HAS_METAL.
#include "../src/thvm.c"
#include "test.h"
static u32 leaf2(u32 d0,u32 d1,float*data){Shape sh={0};sh.ndim=2;sh.dims[0]=d0;sh.dims[1]=d1;
  u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32); CPU_BACKEND.buf_write(TENS[t].buf_id,data,(u64)d0*d1*4); return t;}
int main(void){ thvm_init();   // default backend = CPU
  int f=0; TEST_BEGIN("metal/tc-transposed-rhs-matmul");
  u32 M=8,K=64,N=8;
  float q[8*64], kk[8*64];
  for(u32 i=0;i<M*K;i++) q[i]  = 0.3f*sinf(0.7f*i) ;
  for(u32 i=0;i<N*K;i++) kk[i] = 0.3f*cosf(0.4f*i+1.f);
  Term qT  = term_new(0,TAG_TEN,DT_FP32,leaf2(M,K,q));
  Term kkT = term_new(0,TAG_TEN,DT_FP32,leaf2(N,K,kk));
  // B = Transpose[kk] : {N,K}->{K,N}
  u32 perm[2]={1,0}; Term B = uop_permute(kkT,2,perm);
  // TMatMul lowering: A{M,K}->{M,K,1}->{M,K,N}; B{K,N}->{1,K,N}->{M,K,N}; MUL; SUM axis1
  u32 a3[3]={M,K,1}, mkn[3]={M,K,N}, b3[3]={1,K,N};
  Term Ae = uop_expand(uop_reshape(qT,3,a3),3,mkn);
  Term Be = uop_expand(uop_reshape(B,3,b3),3,mkn);
  Term out = uop_reduce(REDUCE_SUM,1,uop_binary(UOP_MUL,Ae,Be));   // {M,N}
  Term outM = uop_to_device_leaves(out, THVM_DEV_METAL);           // compute on Metal
  Term r = term_resolve(thvm_realize(outM));
  float got[8*8]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,got,(u64)M*N*4);
  for(u32 i=0;i<M;i++) for(u32 j=0;j<N;j++){
    float want=0; for(u32 k=0;k<K;k++) want += q[i*K+k]*kk[j*K+k];
    if(fabsf(got[i*N+j]-want)>2e-3f) f++;
  }
  printf("  got[0,0]=%.4f got[7,7]=%.4f  %s (%d mismatches)\n", got[0], got[63], f==0?"ok":"FAIL", f);
  return f==0?0:1; }
