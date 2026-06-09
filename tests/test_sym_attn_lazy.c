// LAZY symbolic causal attention block: same compute as test_sym_attn_block.c
// but the er() barriers are REMOVED (er = identity) so the whole graph fuses
// and realizes ONCE.  If the fused symbolic lowering is correct this matches
// out[i,0]=i/2; if the fused-broadcast bug is real it diverges.  LAZY_LEVEL env
// controls how many barriers we keep (bisect the first diverging fusion).
#include "../src/thvm.c"
#include "test.h"
static int LZ; // 0=fully lazy, higher=more eager barriers kept
static u32 ten2(u32 d0,u32 d1,float fill){Shape sh={0};sh.ndim=2;sh.dims[0]=d0;sh.dims[1]=d1;u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);u32 n=TENS[t].view.numel;float*b=malloc(n*4);for(u32 i=0;i<n;i++)b[i]=fill;CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4);free(b);return t;}
static Term R(Term t){ return term_new(0,TAG_TEN,DT_FP32,(u32)term_val(term_resolve(thvm_realize(t)))); }
// er barrier kept only if level>=lvl
static Term erL(Term t,int lvl){ return (LZ>=lvl)?R(t):t; }
static u32 ramp_xp(u32 hi,u32 s,int inner,u32 d){
  Shape rsh={0}; rsh.ndim=1; rsh.dims[0]=hi; u32 rid=tensor_alloc(&CPU_BACKEND,rsh,DT_FP32);
  float rr[16]; for(u32 i=0;i<hi;i++) rr[i]=(float)i; CPU_BACKEND.buf_write(TENS[rid].buf_id,rr,hi*4);
  TENS[rid].view.shape.dims[0]=kvar_pack_extent(s);
  u32 P=kvar_pack_extent(s); u32 out2[2]; out2[inner]=P; out2[1-inner]=d; u32 rs[2]; rs[inner]=P; rs[1-inner]=1;
  return (u32)term_val(term_resolve(thvm_realize(uop_expand(uop_reshape(term_new(0,TAG_TEN,DT_FP32,rid),2,rs),2,out2))));
}
int main(void){ thvm_init(); LZ=getenv("LZ")?atoi(getenv("LZ")):0; int f=0;
  u32 hi=16, d=3, s=kvar_alloc("s",1,hi); kvar_set_runtime(s,4);
  u32 P=kvar_pack_extent(s);
  Term Q=term_new(0,TAG_TEN,DT_FP32,ten2(P,d,1.0f)), K=term_new(0,TAG_TEN,DT_FP32,ten2(P,d,1.0f));
  Term V=term_new(0,TAG_TEN,DT_FP32, ramp_xp(hi,s,0,d));
  Term ri=term_new(0,TAG_TEN,DT_FP32, ramp_xp(hi,s,0,(u32)0x80000000u|s));
  Term rj=term_new(0,TAG_TEN,DT_FP32, ramp_xp(hi,s,1,(u32)0x80000000u|s));
  Term mask=erL(uop_binary(UOP_MUL, uop_binary(UOP_CMPLT, uop_binary(UOP_ADD,ri,uop_unary(UOP_NEG,rj)), uop_const(DT_FP32,ru_f32_bits(0.0f))), uop_const(DT_FP32,ru_f32_bits(-30.0f))),5);
  u32 s1d[3]={P,1,d}, _1sd[3]={1,P,d}, ssd[3]={P,P,d}, ss[2]={P,P}, s1[2]={P,1};
  Term qe=erL(uop_expand(uop_reshape(Q,3,s1d),3,ssd),4), ke=erL(uop_expand(uop_reshape(K,3,_1sd),3,ssd),4);
  Term scores=erL(uop_reduce(REDUCE_SUM,2, erL(uop_binary(UOP_MUL,qe,ke),4)),3);
  Term masked=erL(uop_binary(UOP_ADD,scores,mask),3);
  Term m=erL(uop_reduce(REDUCE_MAX,1,masked),2), me=erL(uop_expand(uop_reshape(m,2,s1),2,ss),2);
  Term e=erL(uop_unary(UOP_EXP2, erL(uop_binary(UOP_ADD,masked,uop_unary(UOP_NEG,me)),2)),2);
  Term sm=erL(uop_reduce(REDUCE_SUM,1,e),2), se=erL(uop_expand(uop_reshape(sm,2,s1),2,ss),2);
  Term w=erL(uop_binary(UOP_MUL,e,uop_unary(UOP_RECIP,se)),1);
  u32 ss1[3]={P,P,1};
  Term we=erL(uop_expand(uop_reshape(w,3,ss1),3,ssd),1), ve=erL(uop_expand(uop_reshape(V,3,_1sd),3,ssd),1);
  Term out=uop_reduce(REDUCE_SUM,1, uop_binary(UOP_MUL,we,ve));
  Term r=term_resolve(thvm_realize(out)); float o[256];
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,(u64)4*d*4);
  printf("  LZ=%d out[i,0] (want 0 0.5 1 1.5):",LZ);
  for(u32 i=0;i<4;i++){ float g=o[i*d]; printf(" %.3f",g); if(fabsf(g-i/2.0f)>0.02f) f++; } printf("  fails=%d\n",f);
  return f==0?0:1; }
