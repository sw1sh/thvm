// Metal device-COPY (TToDevice) divergence repro -- FAILING TEST (open bug).
//
// pad(softmax(xa)) + pad(softmax(xb)) -- two STRUCTURALLY-IDENTICAL softmax
// reduce+broadcast chains, each padded into a complementary column slice, summed.
// Realized via uop_copy_dev(.., THVM_DEV_METAL) (the WL TToDevice path) it
// DIVERGES from the host oracle; a plain CPU realize of the SAME graph, and
// GLOBAL Metal (DEV=metal default), both compute it CORRECTLY.
//
// Diagnosis (fully instrumented -- it is NOT a Metal bug):
//  - The wrong result is already in the src TEN ON CPU before the cross-backend
//    copy runs; interact_assign's memcpy and Metal compute are both fine.
//  - A=softmax(xa) and B=softmax(xb) are structurally identical, so their per-op
//    kernels share a slot-based store_root (correct CSE).  But B's exp-chain
//    kernel mis-binds ONE input slot: the "view-of-x" slot lists A's view-alias
//    TenDesc (a view over xa's buffer) instead of B's view over xb -- so in that
//    slot B reads xa.  (Confirmed: A's and B's exp kernels share store_root and
//    BOTH carry A's view-alias tid in the same input slot; tensor_view_of never
//    caches, so the collapse is already in the value tree the COPY materialize
//    hands the lift, not in view_resolve.)
//  - Plain CPU realize of the SAME graph, and GLOBAL Metal, are both correct --
//    only wrapping it in COPY -> ASSIGN(metal_dst, src) triggers the value-tree
//    substitution that collapses B's view-of-xb onto A's view-of-xa alias.
// So: a CPU-side hash-cons / simplify collision between two structurally-
// identical branches, EXPOSED by the device-COPY (TToDevice) materialize order.
// Root of the GPT-2 multi-head-attention divergence through TToDevice (the 12
// heads are identical; headStitch sums PADs of their softmax.V outputs).  Fix
// lives in schedule/materialize.c (the COPY-path value-tree materialize /
// bufferize boundary-input resolution) -- left FAILING for whoever fixes it.
//
// Built -DTHVM_HAS_METAL so backend_metal.o owns METAL_BACKEND.
#include "../src/thvm.c"
#include "test.h"
static u32 leaf(Backend *be,u32 d0,u32 d1,float*data){Shape sh={0};sh.ndim=2;sh.dims[0]=d0;sh.dims[1]=d1;u32 t=tensor_alloc(be,sh,DT_FP32);be->buf_write(TENS[t].buf_id,data,(u64)d0*d1*4);return t;}
static Term softmax1(Term x,u32 R,u32 C){
  u32 r1[2]={R,1}, rc[2]={R,C};
  Term m = uop_expand(uop_reshape(uop_reduce(REDUCE_MAX,1,x),2,r1),2,rc);
  Term e = uop_unary(UOP_EXP2, uop_binary(UOP_MUL, uop_binary(UOP_ADD,x,uop_unary(UOP_NEG,m)), uop_const(DT_FP32,ru_f32_bits(1.4426950408889634f))));
  Term s = uop_expand(uop_reshape(uop_reduce(REDUCE_SUM,1,e),2,r1),2,rc);
  return uop_binary(UOP_MUL, e, uop_unary(UOP_RECIP,s));
}
// build pad(softmax(xa)) + pad(softmax(xb)) on the CPU default backend
static Term build(u32 R,u32 C,float*xa,float*xb){
  Term A=softmax1(term_new(0,TAG_TEN,DT_FP32,leaf(&CPU_BACKEND,R,C,xa)),R,C);
  Term B=softmax1(term_new(0,TAG_TEN,DT_FP32,leaf(&CPU_BACKEND,R,C,xb)),R,C);
  u32 wEnd[4]={0,0,0,C}, wBeg[4]={0,0,C,0};
  return uop_binary(UOP_ADD, uop_pad(A,2,wEnd), uop_pad(B,2,wBeg));   // {R,2C}
}
int main(void){ thvm_init();   // default backend = CPU
  int f=0; TEST_BEGIN("metal/tod-pad-softmax-sum");
  u32 R=4,C=4,N=R*2*C; float xa[16],xb[16];
  for(int i=0;i<16;i++){xa[i]=0.3f*((i*7)%5)-0.6f; xb[i]=0.2f*((i*3)%4)-0.3f;}
  // (a) host oracle (no thvm realize -- isolates the device-COPY path)
  float cpu[32];
  for(int pass=0;pass<2;pass++){float*src=pass?xb:xa;
    for(u32 rr=0;rr<R;rr++){float mx=-1e30f,sm=0; for(u32 cc=0;cc<C;cc++) if(src[rr*C+cc]>mx)mx=src[rr*C+cc];
      float e[4]; for(u32 cc=0;cc<C;cc++){e[cc]=exp2f((src[rr*C+cc]-mx)*1.4426950408889634f); sm+=e[cc];}
      for(u32 cc=0;cc<C;cc++) cpu[rr*2*C+pass*C+cc]=e[cc]/sm;}}
  // (b) device-COPY to metal (the ONLY thvm realize)
  Term rm = term_resolve(thvm_realize(uop_copy_dev(build(R,C,xa,xb), THVM_DEV_METAL)));
  float met[32]; TENS[(u32)term_val(rm)].backend->buf_read(TENS[(u32)term_val(rm)].buf_id,met,(u64)N*4);
  for(u32 i=0;i<N;i++) if(fabsf(cpu[i]-met[i])>1e-4f) f++;
  printf("  cpu  row0: %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f\n",cpu[0],cpu[1],cpu[2],cpu[3],cpu[4],cpu[5],cpu[6],cpu[7]);
  printf("  metal row0: %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f\n",met[0],met[1],met[2],met[3],met[4],met[5],met[6],met[7]);
  printf("  %s (%d mismatches vs CPU)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
