#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("gather/expand-S1-direct-realize");
  u32 hi=16, s=kvar_alloc("s",1,hi);
  Shape rsh={0}; rsh.ndim=1; rsh.dims[0]=hi;
  u32 rid=tensor_alloc(&CPU_BACKEND, rsh, DT_FP32);
  float rr[16]; for(u32 i=0;i<hi;i++) rr[i]=(float)i;
  CPU_BACKEND.buf_write(TENS[rid].buf_id, rr, hi*4);
  TENS[rid].view.shape.dims[0]=kvar_pack_extent(s);
  u32 P=kvar_pack_extent(s); u32 s1[2]={P,1}, ss[2]={P,P};
  Term ri=uop_expand(uop_reshape(term_new(0,TAG_TEN,DT_FP32,rid),2,s1),2,ss); // ri[i,j]=i
  kvar_set_runtime(s,4);
  Term r=term_resolve(thvm_realize(ri));
  float o[256]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,(u64)hi*hi*4);
  for(u32 i=0;i<4;i++) for(u32 j=0;j<4;j++){ if(fabsf(o[i*hi+j]-(float)i)>0.01f) f++; }
  printf("  {S,1}->{S,S} direct: row0[%g %g] row1[%g %g] (want 0 0 / 1 1)\n",o[0],o[1],o[16],o[17]);
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
