#include "../src/thvm.c"
#include "test.h"
static u32 sym_tensor(u32 var_id, u32 ndim, u32 d1, float fill) {
  Shape sh = {0}; sh.ndim = ndim; sh.dims[0] = kvar_pack_extent(var_id);
  if (ndim == 2) sh.dims[1] = d1;
  u32 tid = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  u32 n = TENS[tid].view.numel; float *buf = malloc(n*4);
  for (u32 i=0;i<n;i++) buf[i]=fill;
  CPU_BACKEND.buf_write(TENS[tid].buf_id, buf, n*4); free(buf);
  return tid;
}
static float rd(Term r){ Term t=term_resolve(r); float o=-1; TENS[(u32)term_val(t)].backend->buf_read(TENS[(u32)term_val(t)].buf_id,&o,4); return o; }
int main(void){ thvm_init(); int f=0;
  // elementwise: {S} 1 + {S} 1 = {S} 2, sum = 2S
  TEST_BEGIN("m2/elementwise-symbolic");
  { u32 s=kvar_alloc("s",1,16); kvar_set_runtime(s,5);
    Term a=term_new(0,TAG_TEN,DT_FP32,sym_tensor(s,1,0,1.0f));
    Term b=term_new(0,TAG_TEN,DT_FP32,sym_tensor(s,1,0,1.0f));
    Term sum=uop_reduce(REDUCE_SUM,0,uop_binary(UOP_ADD,a,b));
    float o=rd(thvm_realize(sum)); printf("  sum({S=5} of 2) -> %g (want 10)\n",o);
    if(!(o>9.99f&&o<10.01f)) f++; }
  // 2-D symbolic OUTER: {S,3} ones, sum-all = 3S
  TEST_BEGIN("m2/2d-symbolic-outer");
  { u32 s=kvar_alloc("s2",1,16); kvar_set_runtime(s,4);
    Term a=term_new(0,TAG_TEN,DT_FP32,sym_tensor(s,2,3,1.0f));
    u32 ax[2]={0,1}; Term sum=uop_reduce_multi(REDUCE_SUM,2,ax,a);
    float o=rd(thvm_realize(sum)); printf("  sum({S=4,3} of 1) -> %g (want 12)\n",o);
    if(!(o>11.99f&&o<12.01f)) f++; }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
