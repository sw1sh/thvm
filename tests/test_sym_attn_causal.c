// Causal-masked softmax (the causal attention weights) over a SYMBOLIC seq,
// executed FULLY EAGER (each reduce/binary-op/expand realized to contiguous).
// This sidesteps the (separate, documented) fused-broadcast bugs: symbolic
// attention computes correctly eager; fusion is a perf optimization, not a
// correctness blocker.  row i attends uniformly to j<=i (1/(i+1)), 0 to j>i.
#include "../src/thvm.c"
#include "test.h"
static u32 ramp_expand(u32 hi, u32 s, int inner){
  Shape rsh={0}; rsh.ndim=1; rsh.dims[0]=hi;
  u32 rid=tensor_alloc(&CPU_BACKEND, rsh, DT_FP32);
  float rr[16]; for(u32 i=0;i<hi;i++) rr[i]=(float)i;
  CPU_BACKEND.buf_write(TENS[rid].buf_id, rr, hi*4);
  TENS[rid].view.shape.dims[0]=kvar_pack_extent(s);
  u32 P=kvar_pack_extent(s); u32 ss[2]={P,P}; u32 rsd[2]; rsd[inner]=P; rsd[1-inner]=1;
  return (u32)term_val(term_resolve(thvm_realize(uop_expand(uop_reshape(term_new(0,TAG_TEN,DT_FP32,rid),2,rsd),2,ss))));
}
static Term er(Term t){ return term_new(0,TAG_TEN,DT_FP32,(u32)term_val(term_resolve(thvm_realize(t)))); }
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("attn/causal-masked-softmax-eager");
  u32 hi=16, s=kvar_alloc("s",1,hi); kvar_set_runtime(s,4);
  Term ri=term_new(0,TAG_TEN,DT_FP32, ramp_expand(hi,s,0));
  Term rj=term_new(0,TAG_TEN,DT_FP32, ramp_expand(hi,s,1));
  Term lt =uop_binary(UOP_CMPLT, uop_binary(UOP_ADD, ri, uop_unary(UOP_NEG, rj)), uop_const(DT_FP32, ru_f32_bits(0.0f)));
  Term mask=er(uop_binary(UOP_MUL, lt, uop_const(DT_FP32, ru_f32_bits(-30.0f))));
  u32 P=kvar_pack_extent(s); u32 ss[2]={P,P}, s1[2]={P,1};
  Term m  = er(uop_reduce(REDUCE_MAX,1,mask));
  Term me = er(uop_expand(uop_reshape(m,2,s1),2,ss));
  Term e  = er(uop_unary(UOP_EXP2, er(uop_binary(UOP_ADD, mask, uop_unary(UOP_NEG, me)))));
  Term sm = er(uop_reduce(REDUCE_SUM,1,e));
  Term se = er(uop_expand(uop_reshape(sm,2,s1),2,ss));
  Term w  = er(uop_binary(UOP_MUL, e, uop_unary(UOP_RECIP, se)));
  Term r=term_resolve(w); float o[256];
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,(u64)hi*hi*4);
  printf("  S=4 causal attn weights:\n");
  for(u32 i=0;i<4;i++){ printf("   ");
    for(u32 j=0;j<4;j++){ float g=o[i*hi+j]; float wv=(j<=i)?1.0f/(i+1):0.0f;
      printf(" %.3f", g); if(fabsf(g-wv)>0.02f) f++; } printf("\n"); }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
