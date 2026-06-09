// Full 2-head symbolic causal MHA, STRUCTURED inputs (uniform masks the bug),
// EAGER reference vs LAZY with a per-stage barrier bisect.  BIS env names the
// LAST stage that stays realized when lazy: split<scores<soft<out<cat.
// Q[i,c]=0.1(i+1)+0.01c, K=0.1(i+1)-0.01c, V[i,c]=i+0.5c; causal; compare to
// the EAGER run (stored as reference). Reports head0 (cols0,1) row1 + fail count.
#include "../src/thvm.c"
#include "test.h"
static int LVL;  // 0 fully lazy .. 5 fully eager
static Term Rz(Term t){return term_new(0,TAG_TEN,DT_FP32,(u32)term_val(term_resolve(thvm_realize(t))));}
static Term B(Term t,int lvl){return (LVL>=lvl)?Rz(t):t;}   // barrier kept if LVL>=lvl
static u32 tenf(u32 hi,u32 s,u32 d,int kind){Shape sh={0};sh.ndim=2;sh.dims[0]=kvar_pack_extent(s);sh.dims[1]=d;u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);float*b=malloc((u64)hi*d*4);for(u32 i=0;i<hi;i++)for(u32 c=0;c<d;c++){float v=kind==0?0.1f*(i+1)+0.01f*c:kind==1?0.1f*(i+1)-0.01f*c:(float)i+0.5f*c;b[i*d+c]=v;}CPU_BACKEND.buf_write(TENS[t].buf_id,b,(u64)hi*d*4);free(b);return t;}
static u32 ramp_xp(u32 hi,u32 s,int inner,u32 d){Shape rsh={0};rsh.ndim=1;rsh.dims[0]=hi;u32 rid=tensor_alloc(&CPU_BACKEND,rsh,DT_FP32);float rr[16];for(u32 i=0;i<hi;i++)rr[i]=(float)i;CPU_BACKEND.buf_write(TENS[rid].buf_id,rr,hi*4);TENS[rid].view.shape.dims[0]=kvar_pack_extent(s);u32 P=kvar_pack_extent(s);u32 o2[2];o2[inner]=P;o2[1-inner]=d;u32 rs[2];rs[inner]=P;rs[1-inner]=1;return (u32)term_val(term_resolve(thvm_realize(uop_expand(uop_reshape(term_new(0,TAG_TEN,DT_FP32,rid),2,rs),2,o2))));}
static float REF[16];
int main(void){ thvm_init(); int f=0;
 for(int lv=5; lv>=0; lv--){ LVL=lv;
  u32 hi=8,nH=2,dH=2,dim=nH*dH,s=kvar_alloc("s",1,hi); kvar_set_runtime(s,4); u32 P=kvar_pack_extent(s);
  float scale=0.7071067811865475f;
  Term Q=term_new(0,TAG_TEN,DT_FP32,tenf(hi,s,dim,0)),K=term_new(0,TAG_TEN,DT_FP32,tenf(hi,s,dim,1)),V=term_new(0,TAG_TEN,DT_FP32,tenf(hi,s,dim,2));
  Term ri=term_new(0,TAG_TEN,DT_FP32,ramp_xp(hi,s,0,(u32)0x80000000u|s)),rj=term_new(0,TAG_TEN,DT_FP32,ramp_xp(hi,s,1,(u32)0x80000000u|s));
  Term mask=B(uop_binary(UOP_MUL,uop_binary(UOP_CMPLT,uop_binary(UOP_ADD,ri,uop_unary(UOP_NEG,rj)),uop_const(DT_FP32,ru_f32_bits(0.f))),uop_const(DT_FP32,ru_f32_bits(-30.f))),5);
  u32 snh[3]={P,nH,dH},perm[3]={1,0,2},sd[2]={P,dH},s1d[3]={P,1,dH},_1sd[3]={1,P,dH},ssd[3]={P,P,dH},ss[2]={P,P},s1[2]={P,1},ss1[3]={P,P,1};
  Term acc=0;
  for(u32 h=0;h<nH;h++){u32 bg[6]={h,h+1,0,P,0,dH};
    Term qh=B(uop_reshape(uop_shrink(uop_permute(uop_reshape(Q,3,snh),3,perm),3,bg),2,sd),4);
    Term kh=B(uop_reshape(uop_shrink(uop_permute(uop_reshape(K,3,snh),3,perm),3,bg),2,sd),4);
    Term vh=B(uop_reshape(uop_shrink(uop_permute(uop_reshape(V,3,snh),3,perm),3,bg),2,sd),4);
    Term qe=B(uop_expand(uop_reshape(qh,3,s1d),3,ssd),3),ke=B(uop_expand(uop_reshape(kh,3,_1sd),3,ssd),3);
    Term sc=B(uop_binary(UOP_ADD,B(uop_binary(UOP_MUL,B(uop_reduce(REDUCE_SUM,2,B(uop_binary(UOP_MUL,qe,ke),3)),3),uop_const(DT_FP32,ru_f32_bits(scale))),3),mask),3);
    Term m=B(uop_reduce(REDUCE_MAX,1,sc),2),me=B(uop_expand(uop_reshape(m,2,s1),2,ss),2);
    Term e=B(uop_unary(UOP_EXP2,B(uop_binary(UOP_MUL,B(uop_binary(UOP_ADD,sc,uop_unary(UOP_NEG,me)),2),uop_const(DT_FP32,ru_f32_bits(1.4426950408889634f))),2)),2);
    Term sm=B(uop_reduce(REDUCE_SUM,1,e),2),se=B(uop_expand(uop_reshape(sm,2,s1),2,ss),2);
    Term w=B(uop_binary(UOP_MUL,e,uop_unary(UOP_RECIP,se)),2);
    Term we=B(uop_expand(uop_reshape(w,3,ss1),3,ssd),1),ve=B(uop_expand(uop_reshape(vh,3,_1sd),3,ssd),1);
    Term out=B(uop_reduce(REDUCE_SUM,1,B(uop_binary(UOP_MUL,we,ve),1)),1);   // {S,dH}
    u32 pw[4]={0,0,h*dH,(nH-1-h)*dH}; Term prod=B(uop_pad(out,2,pw),1);
    acc=(acc==0)?prod:B(uop_binary(UOP_ADD,acc,prod),1);
  }
  Term r=term_resolve(thvm_realize(acc)); float o[64];
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,(u64)4*dim*4);
  if(lv==5){for(int i=0;i<16;i++)REF[i]=o[i];}
  int bad=0; for(int i=0;i<4*dim;i++) if(fabsf(o[i]-REF[i])>1e-3f) bad++;
  printf("  LVL=%d (%s) row1=%.4f %.4f %.4f %.4f  bad=%d\n",lv,
    lv==5?"eager":lv==4?"+split":lv==3?"+scores":lv==2?"+softmax":lv==1?"+out/cat":"fully-lazy",
    o[dim],o[dim+1],o[dim+2],o[dim+3],bad);
  if(lv<5&&bad)f=1;
 }
 return f; }
