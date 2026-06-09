// sum(x*x) over K with M%16==0 triggers tinygrad's matvec hand-opt on the GPU:
// GROUP the reduce (8 cooperative threads) + LOCAL the output row.  The
// shared-mem GROUP-reduce template must place the grouped axis as the
// INNERMOST `tt` dim when a LOCAL axis coexists (threadgroup =
// local_total*group_extent); before the fix it set threadgroup=group_extent
// and bound LOCAL to raw `tt`, conflating them -> the GPT-2 LayerNorm variance
// diverged ~1e4 at seq%16==0.  out[m] = sum_k x[m,k]^2, M=16, K=128, on Metal
// via uop_to_device_leaves; vs host oracle.  Built -DTHVM_HAS_METAL.
#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init();
  int f=0; TEST_BEGIN("metal/group-local-matvec");
  u32 M=16, K=128;
  float x[16*128];
  for(u32 i=0;i<M*K;i++) x[i] = 0.2f*sinf(0.3f*i) - 0.1f;
  Shape sh={0}; sh.ndim=2; sh.dims[0]=M; sh.dims[1]=K;
  u32 xt=tensor_alloc(&CPU_BACKEND,sh,DT_FP32); CPU_BACKEND.buf_write(TENS[xt].buf_id,x,(u64)M*K*4);
  Term X=term_new(0,TAG_TEN,DT_FP32,xt);
  Term red = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, X, X));   // {M}
  Term r = term_resolve(thvm_realize(uop_to_device_leaves(red, THVM_DEV_METAL)));
  float got[16]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,got,(u64)M*4);
  for(u32 m=0;m<M;m++){
    float want=0; for(u32 k=0;k<K;k++) want += x[m*K+k]*x[m*K+k];
    if(fabsf(got[m]-want)>2e-3f) f++;
  }
  printf("  got[0]=%.4f want[0]=%.4f got[15]=%.4f  %s (%d mismatches)\n",
         got[0], ({float w=0; for(u32 k=0;k<K;k++) w+=x[k]*x[k]; w;}), got[15], f==0?"ok":"FAIL", f);
  return f==0?0:1; }
