// KV-cache core compute: a SINGLE new token Q{1,d} attends to a symbolic {t,d}
// K/V cache (t = current cache length, a kvar).  scores=Q.Kt {1,t} -> softmax
// -> .V {1,d}.  K=ones, V[j,k]=j -> out[0,k] = avg_{j<t} j = (t-1)/2.  Eager.
#include "../src/thvm.c"
#include "test.h"
static Term er(Term t){ return term_new(0,TAG_TEN,DT_FP32,(u32)term_val(term_resolve(thvm_realize(t)))); }
static u32 cache(u32 nctx,u32 d,u32 s,int ramp){     // {nctx,d} buf, mark axis0 symbolic->{t,d}
  Shape sh={0}; sh.ndim=2; sh.dims[0]=nctx; sh.dims[1]=d;
  u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);
  float*b=malloc((u64)nctx*d*4); for(u32 j=0;j<nctx;j++)for(u32 k=0;k<d;k++)b[j*d+k]=ramp?(float)j:1.0f;
  CPU_BACKEND.buf_write(TENS[t].buf_id,b,(u64)nctx*d*4); free(b);
  TENS[t].view.shape.dims[0]=kvar_pack_extent(s); return t;
}
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("symbolic/kvcache-single-token-attn");
  u32 nctx=8, d=3, s=kvar_alloc("t",1,nctx); kvar_set_runtime(s,4);
  u32 P=kvar_pack_extent(s);
  Term K=term_new(0,TAG_TEN,DT_FP32, cache(nctx,d,s,0));   // {t,d} ones
  Term V=term_new(0,TAG_TEN,DT_FP32, cache(nctx,d,s,1));   // {t,d} V[j,k]=j
  Shape qs={0}; qs.ndim=2; qs.dims[0]=1; qs.dims[1]=d;
  u32 qid=tensor_alloc(&CPU_BACKEND,qs,DT_FP32); float qb[3]={1,1,1};
  CPU_BACKEND.buf_write(TENS[qid].buf_id,qb,d*4);
  Term Q=term_new(0,TAG_TEN,DT_FP32,qid);                  // {1,d} ones
  // scores = einsum ik,jk->ij : Q{1,d}->{1,1,d}->{1,t,d}; K{t,d}->{1,t,d}; mul; reduce axis2
  u32 _11d[3]={1,1,d}, _1td[3]={1,P,d}, _1t[2]={1,P}, _11[2]={1,1};
  Term qe=er(uop_expand(uop_reshape(Q,3,_11d),3,_1td));
  Term ke=er(uop_expand(uop_reshape(K,3,_1td),3,_1td));    // K already {t,d}; reshape to {1,t,d}
  Term scores=er(uop_reduce(REDUCE_SUM,2, er(uop_binary(UOP_MUL,qe,ke))));  // {1,t}
  // softmax over axis1
  Term m=er(uop_reduce(REDUCE_MAX,1,scores)), me=er(uop_expand(uop_reshape(m,2,_11),2,_1t));
  Term e=er(uop_unary(UOP_EXP2, er(uop_binary(UOP_ADD,scores,uop_unary(UOP_NEG,me)))));
  Term sm=er(uop_reduce(REDUCE_SUM,1,e)), se=er(uop_expand(uop_reshape(sm,2,_11),2,_1t));
  Term w=er(uop_binary(UOP_MUL,e,uop_unary(UOP_RECIP,se)));  // {1,t}
  // out = einsum ij,jk->ik : w{1,t}->{1,t,1}->{1,t,d}; V{t,d}->{1,t,d}; mul; reduce axis1
  u32 _1t1[3]={1,P,1};
  Term we=er(uop_expand(uop_reshape(w,3,_1t1),3,_1td));
  Term ve=er(uop_expand(uop_reshape(V,3,_1td),3,_1td));
  Term out=er(uop_reduce(REDUCE_SUM,1, er(uop_binary(UOP_MUL,we,ve))));  // {1,d}
  Term r=term_resolve(out); float o[8];
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,d*4);
  printf("  t=4 out[0,*]=%.3f %.3f %.3f (want 1.5 = avg of V[0..3]=0,1,2,3)\n",o[0],o[1],o[2]);
  for(u32 k=0;k<d;k++) if(fabsf(o[k]-1.5f)>0.02f) f++;
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
