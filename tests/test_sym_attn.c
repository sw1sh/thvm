// Symbolic attention building blocks: (1) matmul contracting a SYMBOLIC dim
// ({M,S}.{S,N}, K=S -- the scores.V matmul); (2) matmul producing {S,S} (both
// output dims symbolic -- the Q.Kt scores).  All ones, so results are exact.
#include "../src/thvm.c"
#include "test.h"
static u32 ten2(u32 d0,u32 d1,float fill){Shape sh={0};sh.ndim=2;sh.dims[0]=d0;sh.dims[1]=d1;u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);u32 n=TENS[t].view.numel;float*b=malloc(n*4);for(u32 i=0;i<n;i++)b[i]=fill;CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4);free(b);return t;}
// matmul A{d0a,d1a}.B{d1b,d2b} via reshape+expand+mul+reduce over the shared axis.
static Term mm(Term A,u32 Ma,u32 Ka, Term B,u32 Kb,u32 Nb){
  // A{M,K}->{M,K,1}->{M,K,N}; B{K,N}->{1,K,N}->{M,K,N}; mul; reduce axis1 ->{M,N}
  u32 mk1[3]={Ma,Ka,1}, mkn[3]={Ma,Ka,Nb}, _kn[3]={1,Kb,Nb};
  Term ae=uop_expand(uop_reshape(A,3,mk1),3,mkn);
  Term be=uop_expand(uop_reshape(B,3,_kn),3,mkn);
  return uop_reduce(REDUCE_SUM,1,uop_binary(UOP_MUL,ae,be));
}
static void rdN(Term r,float*out,u32 n){Term t=term_resolve(thvm_realize(r));TENS[(u32)term_val(t)].backend->buf_read(TENS[(u32)term_val(t)].buf_id,out,(u64)n*4);}
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("attn/matmul-symbolic-K (scores.V)");
  { u32 s=kvar_alloc("k",1,32); u32 M=3,N=4;   // {M,S}.{S,N}=>{M,N}, each=S
    Term A=term_new(0,TAG_TEN,DT_FP32,ten2(M,kvar_pack_extent(s),1.0f));
    Term B=term_new(0,TAG_TEN,DT_FP32,ten2(kvar_pack_extent(s),N,1.0f));
    Term r=mm(A,M,kvar_pack_extent(s), B,kvar_pack_extent(s),N);
    kvar_set_runtime(s,5); float o[12]; rdN(r,o,M*N);
    int bad=0; for(u32 i=0;i<M*N;i++) if(!(o[i]>4.9f&&o[i]<5.1f)) bad++;
    printf("  {3,S=5}.{S=5,4} -> all %g (want 5)  bad=%d\n", o[0], bad); if(bad)f++; }
  TEST_BEGIN("attn/matmul-SxS-output (Q.Kt)");
  { u32 s=kvar_alloc("ss",1,32); u32 d=4;   // {S,d}.{d,S}=>{S,S}, each=d
    Term Q=term_new(0,TAG_TEN,DT_FP32,ten2(kvar_pack_extent(s),d,1.0f));
    Term Kt=term_new(0,TAG_TEN,DT_FP32,ten2(d,kvar_pack_extent(s),1.0f));
    Term r=mm(Q,kvar_pack_extent(s),d, Kt,d,kvar_pack_extent(s));
    kvar_set_runtime(s,6);
    // {S,S} output is symbolic on BOTH axes -> padded layout, row stride hi (32).
    // Read the whole buffer; check only the valid strided region [i*hi+j], i,j<S.
    float o[1024]; rdN(r,o,32*32);
    int bad=0; for(u32 i=0;i<6;i++) for(u32 j=0;j<6;j++) if(!(o[i*32+j]>3.9f&&o[i*32+j]<4.1f)) bad++;
    printf("  {S=6,4}.{4,S=6} -> valid[i*32+j] all %g (want 4)  bad=%d/36\n", o[0], bad); if(bad)f++; }
  TEST_BEGIN("attn/softmax-over-symbolic-axis");
  { u32 s=kvar_alloc("sm",1,32); u32 P=kvar_pack_extent(s);
    // softmax over axis 1 of ones{S,S} -> each entry 1/S (uniform rows).
    Term x=term_new(0,TAG_TEN,DT_FP32,ten2(P,P,1.0f));         // {S,S} ones
    u32 ss[2]={P,P}, s1[2]={P,1};
    Term m  = uop_reduce(REDUCE_MAX,1,x);                       // {S}
    Term me = uop_expand(uop_reshape(m,2,s1),2,ss);             // {S,S}
    Term sub= uop_binary(UOP_ADD,x,uop_unary(UOP_NEG,me));      // x - max = 0
    Term e  = uop_unary(UOP_EXP2,sub);                          // 1
    Term sm = uop_reduce(REDUCE_SUM,1,e);                       // {S} = S
    Term se = uop_expand(uop_reshape(sm,2,s1),2,ss);            // {S,S}
    Term out= uop_binary(UOP_MUL,e,uop_unary(UOP_RECIP,se));    // 1/S
    kvar_set_runtime(s,6); float o[1024]; rdN(out,o,32*32);
    int bad=0; for(u32 i=0;i<6;i++) for(u32 j=0;j<6;j++) if(!(o[i*32+j]>0.16f&&o[i*32+j]<0.174f)) bad++;
    printf("  softmax(ones{S=6,S=6}) -> valid all %g (want 0.1667)  bad=%d/36\n", o[0], bad); if(bad)f++; }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
