// Repeated FRESH-kvar eager causal MASK realize crashes ~4th (kvar-packed stride leak).
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
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("symbolic/repeated-fresh-kvar-mask");
  u32 hi=8;
  for(u32 iter=0; iter<8; iter++){
    u32 s = kvar_alloc("s", 1, hi); kvar_set_runtime(s, 4);   // FRESH vid each iter
    Term ri=term_new(0,TAG_TEN,DT_FP32, ramp_expand(hi,s,0));
    Term rj=term_new(0,TAG_TEN,DT_FP32, ramp_expand(hi,s,1));
    Term lt=uop_binary(UOP_CMPLT, uop_binary(UOP_ADD,ri,uop_unary(UOP_NEG,rj)), uop_const(DT_FP32,ru_f32_bits(0.0f)));
    Term mask=uop_binary(UOP_MUL, lt, uop_const(DT_FP32, ru_f32_bits(-30.0f)));
    Term r=term_resolve(thvm_realize(mask));
    float o[64]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,(u64)hi*hi*4);
    fprintf(stderr,"  iter %u vid=%u -> row0[%.0f %.0f] row1[%.0f %.0f] (want 0 -30 / 0 0)\n", iter, s, o[0],o[1],o[hi],o[hi+1]);
    if(!(o[0]==0.0f && o[1]<-1.0f && o[hi]==0.0f && o[hi+1]==0.0f)) f++;
  }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
