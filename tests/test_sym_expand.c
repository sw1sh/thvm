// Symbolic EXPAND/broadcast to {S,S}.  {1,S}->{S,S} (broadcast the OUTER axis)
// works and is asserted as a regression guard.  {S,1}->{S,S} (broadcast the
// INNER axis, real stride on the outer kvar axis) is the OPEN M3 bug -- it
// reads element 0 for every row; printed as a diagnostic, NOT asserted (see
// docs/plans/decode_roadmap.md).  ramp{S}=[0..S-1].
#include "../src/thvm.c"
#include "test.h"
static u32 ramp_tid(u32 hi, u32 s){
  Shape rs={0}; rs.ndim=1; rs.dims[0]=hi;
  u32 t=tensor_alloc(&CPU_BACKEND, rs, DT_FP32);
  float r[64]; for(u32 i=0;i<hi;i++) r[i]=(float)i;
  CPU_BACKEND.buf_write(TENS[t].buf_id, r, hi*4);
  TENS[t].view.shape.dims[0]=kvar_pack_extent(s);
  return t;
}
static float sumall(Term t, u32 s, u32 bound){
  u32 ax[2]={0,1}; Term sum=uop_reduce_multi(REDUCE_SUM,2,ax,t);
  kvar_set_runtime(s, bound);
  Term r=term_resolve(thvm_realize(sum)); float o=-2;
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,&o,4); return o;
}
int main(void){ thvm_init(); int f=0; u32 hi=16;
  TEST_BEGIN("expand/1S-to-SS-broadcast-outer");      // rj[i,j]=j ; sum = S*S(S-1)/2 = 24
  { u32 s=kvar_alloc("a",1,hi); u32 P=kvar_pack_extent(s); u32 _1s[2]={1,P}, ss[2]={P,P};
    Term rj=uop_expand(uop_reshape(term_new(0,TAG_TEN,DT_FP32,ramp_tid(hi,s)),2,_1s),2,ss);
    float o=sumall(rj,s,4); printf("  expand{1,S}->{S,S} sum=%g (want 24)\n",o); if(!(o>23.9f&&o<24.1f))f++; }
  // DIAGNOSTIC (open bug, not asserted): {S,1}->{S,S} broadcast-inner.
  { u32 s=kvar_alloc("b",1,hi); u32 P=kvar_pack_extent(s); u32 s1[2]={P,1}, ss[2]={P,P};
    Term ri=uop_expand(uop_reshape(term_new(0,TAG_TEN,DT_FP32,ramp_tid(hi,s)),2,s1),2,ss);
    float o=sumall(ri,s,4); printf("  [open] expand{S,1}->{S,S} sum=%g (want 24; reads elt0 -> OPEN M3 bug)\n",o); }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
