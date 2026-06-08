// Causal mask via GATHERED (realized-contiguous) ramp expands, then contiguous
// binary ops -- sidesteps the fused-broadcast bugs.  mask[i,j]=(j>i)?-1e9:0.
#include "../src/thvm.c"
#include "test.h"
static u32 ramp_expand(u32 hi, u32 s, int inner){  // inner=0 -> {S,1} (ri=i); inner=1 -> {1,S} (rj=j)
  Shape rsh={0}; rsh.ndim=1; rsh.dims[0]=hi;
  u32 rid=tensor_alloc(&CPU_BACKEND, rsh, DT_FP32);
  float rr[16]; for(u32 i=0;i<hi;i++) rr[i]=(float)i;
  CPU_BACKEND.buf_write(TENS[rid].buf_id, rr, hi*4);
  TENS[rid].view.shape.dims[0]=kvar_pack_extent(s);
  u32 P=kvar_pack_extent(s); u32 ss[2]={P,P};
  u32 rsdims[2]; rsdims[inner]=P; rsdims[1-inner]=1;  // {S,1} or {1,S}
  Term e=uop_expand(uop_reshape(term_new(0,TAG_TEN,DT_FP32,rid),2,rsdims),2,ss);
  return (u32)term_val(term_resolve(thvm_realize(e)));   // realize -> contiguous {S,S}
}
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("cmask/via-gathered-expands");
  u32 hi=16, s=kvar_alloc("s",1,hi); kvar_set_runtime(s,4);
  Term ri=term_new(0,TAG_TEN,DT_FP32, ramp_expand(hi,s,0));   // {S,S} ri[i,j]=i
  Term rj=term_new(0,TAG_TEN,DT_FP32, ramp_expand(hi,s,1));   // {S,S} rj[i,j]=j
  Term diff=uop_binary(UOP_ADD, ri, uop_unary(UOP_NEG, rj));
  Term lt  =uop_binary(UOP_CMPLT, diff, uop_const(DT_FP32, ru_f32_bits(0.0f)));
  Term mask=uop_binary(UOP_MUL, lt, uop_const(DT_FP32, ru_f32_bits(-1e9f)));
  Term r=term_resolve(thvm_realize(mask));
  float o[256]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,(u64)hi*hi*4);
  printf("  S=4 mask rows (want 0 on/below diag, -1e9 above):\n");
  for(u32 i=0;i<4;i++){ printf("   ");
    for(u32 j=0;j<4;j++){ float g=o[i*hi+j]; float w=(j>i)?-1e9f:0.0f;
      printf(" %.0e", g); if(fabsf(g-w)>1.0f) f++; } printf("\n"); }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
