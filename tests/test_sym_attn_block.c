// Full eager symbolic causal-masked attention block over a symbolic seq S:
//   Q=K=ones{S,d}, V[j]=j ; scores=Q.Kt -> +causalmask -> softmax -> .V.
// out[i] = causal-weighted avg of V[0..i] = (1/(i+1)) sum_{j<=i} j = i/2.
#include "../src/thvm.c"
#include "test.h"
static u32 ten2(u32 d0,u32 d1,float fill){Shape sh={0};sh.ndim=2;sh.dims[0]=d0;sh.dims[1]=d1;u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);u32 n=TENS[t].view.numel;float*b=malloc(n*4);for(u32 i=0;i<n;i++)b[i]=fill;CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4);free(b);return t;}
static Term er(Term t){ return term_new(0,TAG_TEN,DT_FP32,(u32)term_val(term_resolve(thvm_realize(t)))); }
static u32 ramp_xp(u32 hi,u32 s,int inner,u32 d){  // inner axis = ramp; other = broadcast to d (or S)
  Shape rsh={0}; rsh.ndim=1; rsh.dims[0]=hi; u32 rid=tensor_alloc(&CPU_BACKEND,rsh,DT_FP32);
  float rr[16]; for(u32 i=0;i<hi;i++) rr[i]=(float)i; CPU_BACKEND.buf_write(TENS[rid].buf_id,rr,hi*4);
  TENS[rid].view.shape.dims[0]=kvar_pack_extent(s);
  u32 P=kvar_pack_extent(s); u32 out2[2]; out2[inner]=P; out2[1-inner]=d; u32 rs[2]; rs[inner]=P; rs[1-inner]=1;
  return (u32)term_val(term_resolve(thvm_realize(uop_expand(uop_reshape(term_new(0,TAG_TEN,DT_FP32,rid),2,rs),2,out2))));
}
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("attn/full-causal-block-eager");
  u32 hi=16, d=3, s=kvar_alloc("s",1,hi); kvar_set_runtime(s,4);
  u32 P=kvar_pack_extent(s);
  Term Q=term_new(0,TAG_TEN,DT_FP32,ten2(P,d,1.0f)), K=term_new(0,TAG_TEN,DT_FP32,ten2(P,d,1.0f));
  Term V=term_new(0,TAG_TEN,DT_FP32, ramp_xp(hi,s,0,d));        // V[j,k]=j  {S,d}
  // causal mask {S,S} from ramp expands
  Term ri=term_new(0,TAG_TEN,DT_FP32, ramp_xp(hi,s,0,(u32)0x80000000u|s)); // {S,1}->{S,S}: ri[i,j]=i
  Term rj=term_new(0,TAG_TEN,DT_FP32, ramp_xp(hi,s,1,(u32)0x80000000u|s)); // {1,S}->{S,S}: rj[i,j]=j
  Term mask=er(uop_binary(UOP_MUL, uop_binary(UOP_CMPLT, uop_binary(UOP_ADD,ri,uop_unary(UOP_NEG,rj)), uop_const(DT_FP32,ru_f32_bits(0.0f))), uop_const(DT_FP32,ru_f32_bits(-30.0f))));
  // scores = einsum ik,jk->ij  (reduce over d=axis2)
  u32 s1d[3]={P,1,d}, _1sd[3]={1,P,d}, ssd[3]={P,P,d}, ss[2]={P,P}, s1[2]={P,1};
  Term qe=er(uop_expand(uop_reshape(Q,3,s1d),3,ssd)), ke=er(uop_expand(uop_reshape(K,3,_1sd),3,ssd));
  Term scores=er(uop_reduce(REDUCE_SUM,2, er(uop_binary(UOP_MUL,qe,ke))));   // {S,S}=d
  Term masked=er(uop_binary(UOP_ADD,scores,mask));
  // softmax(masked, axis1)
  Term m=er(uop_reduce(REDUCE_MAX,1,masked)), me=er(uop_expand(uop_reshape(m,2,s1),2,ss));
  Term e=er(uop_unary(UOP_EXP2, er(uop_binary(UOP_ADD,masked,uop_unary(UOP_NEG,me)))));
  Term sm=er(uop_reduce(REDUCE_SUM,1,e)), se=er(uop_expand(uop_reshape(sm,2,s1),2,ss));
  Term w=er(uop_binary(UOP_MUL,e,uop_unary(UOP_RECIP,se)));                  // {S,S} causal weights
  // out = einsum ij,jk->ik  (reduce over j=axis1)
  u32 ss1[3]={P,P,1};
  Term we=er(uop_expand(uop_reshape(w,3,ss1),3,ssd)), ve=er(uop_expand(uop_reshape(V,3,_1sd),3,ssd));
  Term out=er(uop_reduce(REDUCE_SUM,1, er(uop_binary(UOP_MUL,we,ve))));      // {S,d}
  Term r=term_resolve(out); float o[256];
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,(u64)4*d*4);
  printf("  out[i,0] (want i/2 = 0 0.5 1 1.5):");
  for(u32 i=0;i<4;i++){ float g=o[i*d]; printf(" %.3f",g); if(fabsf(g-i/2.0f)>0.02f) f++; } printf("\n");
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
