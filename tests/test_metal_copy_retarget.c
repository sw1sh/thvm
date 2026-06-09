// A GENERIC UOP_COPY (device -1, the deferred upload a TFromNet weight uses via
// TUOpCopy[TTensorCreateHost[...]]) must be RE-TARGETED to the device by
// uop_to_device_leaves -- otherwise it resolves its target to CURRENT_BACKEND
// (CPU during a routed realize), so the weight stays on CPU while leaf-inserted
// inputs go to Metal, and the mixed-device kernel computes garbage.
//
// out = matmul(A{4,16}, copy(W{16,8})) + copy(P{4,8}), BOTH generic copies,
// realized on Metal via uop_to_device_leaves; compared to the host oracle
// A.W + P.  Two generic copies in one fused matmul+epilogue kernel was the
// GPT-2 embedding (onehot . tokT + posT) divergence.  Built -DTHVM_HAS_METAL.
#include "../src/thvm.c"
#include "test.h"
static u32 leaf2(u32 d0,u32 d1,float*data){Shape sh={0};sh.ndim=2;sh.dims[0]=d0;sh.dims[1]=d1;
  u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32); CPU_BACKEND.buf_write(TENS[t].buf_id,data,(u64)d0*d1*4); return t;}
int main(void){ thvm_init();
  int f=0; TEST_BEGIN("metal/generic-copy-retarget");
  u32 M=4,K=16,N=8;
  float A[4*16]={0}, W[16*8], P[4*8];
  u32 ids[4]={3,11,0,7};
  for(u32 i=0;i<M;i++) A[i*K+ids[i]]=1.0f;                 // one-hot rows
  for(u32 i=0;i<K*N;i++) W[i]=0.3f*sinf(0.7f*i);
  for(u32 i=0;i<M*N;i++) P[i]=cosf(0.4f*i+1.f);
  Term At = term_new(0,TAG_TEN,DT_FP32,leaf2(M,K,A));
  Term Wt = uop_copy(term_new(0,TAG_TEN,DT_FP32,leaf2(K,N,W)));   // generic copy
  Term Pt = uop_copy(term_new(0,TAG_TEN,DT_FP32,leaf2(M,N,P)));   // generic copy
  u32 a3[3]={M,K,1}, mkn[3]={M,K,N}, b3[3]={1,K,N};
  Term Ae = uop_expand(uop_reshape(At,3,a3),3,mkn);
  Term Be = uop_expand(uop_reshape(Wt,3,b3),3,mkn);
  Term mm = uop_reduce(REDUCE_SUM,1,uop_binary(UOP_MUL,Ae,Be));   // {M,N}
  Term out = uop_binary(UOP_ADD, mm, Pt);                          // + epilogue
  Term r = term_resolve(thvm_realize(uop_to_device_leaves(out, THVM_DEV_METAL)));
  float got[4*8]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,got,(u64)M*N*4);
  for(u32 i=0;i<M;i++) for(u32 j=0;j<N;j++){
    float want=P[i*N+j]; for(u32 k=0;k<K;k++) want += A[i*K+k]*W[k*N+j];
    if(fabsf(got[i*N+j]-want)>2e-3f) f++;
  }
  printf("  got[0,0]=%.4f want[0,0]=%.4f  %s (%d mismatches)\n",
         got[0], P[0]+W[ids[0]*N], f==0?"ok":"FAIL", f);
  return f==0?0:1; }
