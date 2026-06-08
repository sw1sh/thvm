// KV-cache DECODE STEP: append the new token's V_t at a kvar row offset, then
// attend over the APPENDED {t,d} cache.  Combines the two landed mechanisms.
// Cache holds V[0..2]=0,1,2 (3 past tokens); append V_3={3,3,3} at row 3;
// attend (K=ones -> uniform) over V[0..3] -> avg(0,1,2,3)=1.5.  If the append
// were dropped, V[3]=0 -> avg=0.75; this distinguishes.
#include "../src/thvm.c"
#include "test.h"
static Term er(Term t){ return term_new(0,TAG_TEN,DT_FP32,(u32)term_val(term_resolve(thvm_realize(t)))); }
static Term mk_assign(Term dst, Term src){ u64 loc=heap_alloc(2); heap_set(loc+0,dst); heap_set(loc+1,src); return term_new(0,TAG_UOP,UOP_ASSIGN,loc); }
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("symbolic/kvcache-decode-step");
  u32 nctx=8, d=3;
  // K cache: all ones (uniform attention)
  Shape ksh={0}; ksh.ndim=2; ksh.dims[0]=nctx; ksh.dims[1]=d;
  u32 Kc=tensor_alloc(&CPU_BACKEND,ksh,DT_FP32);
  float kb[24]; for(u32 i=0;i<24;i++)kb[i]=1.0f; CPU_BACKEND.buf_write(TENS[Kc].buf_id,kb,24*4);
  // V cache: pre-fill V[j,k]=j for j<3, rows 3..7 = 0
  u32 Vc=tensor_alloc(&CPU_BACKEND,ksh,DT_FP32);
  float vb[24]; for(u32 j=0;j<nctx;j++)for(u32 k=0;k<d;k++)vb[j*d+k]=(j<3)?(float)j:0.0f;
  CPU_BACKEND.buf_write(TENS[Vc].buf_id,vb,24*4);
  // APPEND V_3={3,3,3} at row 3 (kvar offset pos=3)
  u32 pos=kvar_alloc("pos",1,nctx); kvar_set_runtime(pos,3);
  Shape vts={0}; vts.ndim=2; vts.dims[0]=1; vts.dims[1]=d;
  u32 vtid=tensor_alloc(&CPU_BACKEND,vts,DT_FP32); float vt[3]={3,3,3};
  CPU_BACKEND.buf_write(TENS[vtid].buf_id,vt,d*4);
  u32 be[4]={kvar_pack_extent(pos),kvar_pack_extent(pos)+1, 0,d};
  Term app=mk_assign(uop_shrink(term_new(0,TAG_TEN,DT_FP32,Vc),2,be), term_new(0,TAG_TEN,DT_FP32,vtid));
  term_resolve(thvm_realize(app));   // realize the append -> V[3]={3,3,3}
  // ATTEND Q{1,d}=ones over K[0..t], V[0..t] with t=4 (kvar len)
  u32 len=kvar_alloc("t",1,nctx); kvar_set_runtime(len,4); u32 P=kvar_pack_extent(len);
  TENS[Kc].view.shape.dims[0]=P; TENS[Vc].view.shape.dims[0]=P;   // mark {t,d}
  Term K=term_new(0,TAG_TEN,DT_FP32,Kc), V=term_new(0,TAG_TEN,DT_FP32,Vc);
  Shape qs={0}; qs.ndim=2; qs.dims[0]=1; qs.dims[1]=d; u32 qid=tensor_alloc(&CPU_BACKEND,qs,DT_FP32);
  float qb[3]={1,1,1}; CPU_BACKEND.buf_write(TENS[qid].buf_id,qb,d*4); Term Q=term_new(0,TAG_TEN,DT_FP32,qid);
  u32 _11d[3]={1,1,d}, _1td[3]={1,P,d}, _1t[2]={1,P}, _11[2]={1,1}, _1t1[3]={1,P,1};
  Term qe=er(uop_expand(uop_reshape(Q,3,_11d),3,_1td)), ke=er(uop_expand(uop_reshape(K,3,_1td),3,_1td));
  Term scores=er(uop_reduce(REDUCE_SUM,2, er(uop_binary(UOP_MUL,qe,ke))));
  Term m=er(uop_reduce(REDUCE_MAX,1,scores)), me=er(uop_expand(uop_reshape(m,2,_11),2,_1t));
  Term e=er(uop_unary(UOP_EXP2, er(uop_binary(UOP_ADD,scores,uop_unary(UOP_NEG,me)))));
  Term sm=er(uop_reduce(REDUCE_SUM,1,e)), se=er(uop_expand(uop_reshape(sm,2,_11),2,_1t));
  Term w=er(uop_binary(UOP_MUL,e,uop_unary(UOP_RECIP,se)));
  Term we=er(uop_expand(uop_reshape(w,3,_1t1),3,_1td)), ve=er(uop_expand(uop_reshape(V,3,_1td),3,_1td));
  Term out=er(uop_reduce(REDUCE_SUM,1, er(uop_binary(UOP_MUL,we,ve))));
  Term r=term_resolve(out); float o[8]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,d*4);
  printf("  decode out[0,*]=%.3f %.3f %.3f (want 1.5 = avg V[0..3]=0,1,2,3 incl appended V_3)\n",o[0],o[1],o[2]);
  for(u32 k=0;k<d;k++) if(fabsf(o[k]-1.5f)>0.02f) f++;
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
