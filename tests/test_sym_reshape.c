// Symbolic reshape {S} -> {1,S} -> reduce over the inner axis -> {1}.
// Verifies view_apply_reshape resolves the symbolic dim for numel (was
// spuriously declining -> broken).  ramp = [0..S-1], sum = S(S-1)/2.
#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("reshape/symbolic-1S");
  u32 hi=16, s=kvar_alloc("s",1,hi);
  Shape rs={0}; rs.ndim=1; rs.dims[0]=hi;
  u32 rid=tensor_alloc(&CPU_BACKEND, rs, DT_FP32);
  float r[16]; for(u32 i=0;i<hi;i++) r[i]=(float)i;
  CPU_BACKEND.buf_write(TENS[rid].buf_id, r, hi*4);
  TENS[rid].view.shape.dims[0]=kvar_pack_extent(s);
  Term ramp = term_new(0,TAG_TEN,DT_FP32,rid);
  u32 P=kvar_pack_extent(s); u32 _1s[2]={1,P};
  Term r2 = uop_reshape(ramp, 2, _1s);             // {1,S}
  Term sum = uop_reduce(REDUCE_SUM, 1, r2);        // {1} = sum 0..S-1
  kvar_set_runtime(s, 4);
  Term res = term_resolve(thvm_realize(sum));
  float o=-1; TENS[(u32)term_val(res)].backend->buf_read(TENS[(u32)term_val(res)].buf_id,&o,4);
  printf("  sum(reshape {S=4}->{1,4}) = %g (want 6 = 0+1+2+3)\n", o);
  if (!(o>5.9f && o<6.1f)) f++;
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
