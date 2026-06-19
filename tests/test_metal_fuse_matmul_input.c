// THVM_FUSE_MATMUL_INPUT: an elementwise producer (X*A+B) feeding a matmul.
//
// out = (X*A + B) @ Transpose[W], shapes {M,K} @ {K,N} -> {M,N}, bf16 TC.
// With the flag ON the modulation producer (X*A+B) is un-realized: its value
// subtree splices into the matmul MUL operand A, and rmu_emit_matmul_tc_tiled
// computes it INLINE at the simdgroup A-staging load (keeping the bf16 tensor
// cores) instead of realizing a separate buffer + falling to the scalar
// accumulator.  Mirrors tinygrad schedule/rangeify.py:40-46 pm_syntactic_sugar
// (push INDEX through elementwise) + schedule/indexing.py:198-200 (single-
// consumer fuse).  The weight W is a realized bf16 buffer (the clean B read).
//
// Run THVM_FUSE_MATMUL_INPUT=1 ./bin/test_metal_fuse_matmul_input to exercise
// the fused path; without the env it runs the default (separate-kernel) path
// and the assertion is just numeric correctness.  THVM_DUMP_TILE_JIT_SRC=2
// shows the A-staging contains the `(in0*in1)+in2` producer + `TC tiled`.
//
// Asserts: ON numeric == host reference to bf16/TC tolerance.
#include "../src/thvm.c"
#include "test.h"

static u32 leaf2(u32 d0,u32 d1,float *data){Shape sh={0};sh.ndim=2;sh.dims[0]=d0;sh.dims[1]=d1;
  u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32); CPU_BACKEND.buf_write(TENS[t].buf_id,data,(u64)d0*d1*4); return t;}

// round-trip f32 -> bf16 -> f32 (truncate-to-even mantissa) for a faithful
// reference of the bf16 tensor-core matmul.
static float bf16rt(float x){ u32 b; memcpy(&b,&x,4); u32 r=(b>>16)&1; b+=0x7fff+r; b&=0xffff0000u; float o; memcpy(&o,&b,4); return o; }

int main(void){ thvm_init();
  u32 M=64,K=128,N=64;
  float *X=malloc(M*K*4),*A=malloc(M*K*4),*B=malloc(M*K*4),*W=malloc(N*K*4);
  for(u32 i=0;i<M*K;i++){X[i]=0.20f*sinf(0.7f*i); A[i]=0.5f+0.10f*cosf(0.3f*i); B[i]=0.05f*sinf(0.2f*i+1.f);}
  for(u32 i=0;i<N*K;i++) W[i]=0.15f*cosf(0.4f*i+0.5f);

  Term Xt=term_new(0,TAG_TEN,DT_FP32,leaf2(M,K,X));
  Term At=term_new(0,TAG_TEN,DT_FP32,leaf2(M,K,A));
  Term Bt=term_new(0,TAG_TEN,DT_FP32,leaf2(M,K,B));
  Term Wt=term_new(0,TAG_TEN,DT_FP32,leaf2(N,K,W));
  Term prod = uop_binary(UOP_ADD, uop_binary(UOP_MUL,Xt,At), Bt);   // X*A+B {M,K}, FUSED
  Term pbf  = uop_cast(prod, DT_BF16);
  // CONTROL: realize the modulation to bf16 (non-fused) so the SAME shape goes
  // through the clean-buffer TC tiled path -- isolates a pre-existing tiled bug
  // from the fused-A staging.  Enable with THVM_FUSE_CONTROL=1.
  if (getenv("THVM_FUSE_CONTROL")) {
    pbf = term_resolve(thvm_realize(uop_to_device_leaves(pbf, THVM_DEV_METAL)));
    pbf = uop_to_device_leaves(pbf, THVM_DEV_METAL);
  }
  // Realized bf16 weight -> clean B = cast(INDEX_E, f32).
  Term wbf  = term_resolve(thvm_realize(
                uop_to_device_leaves(uop_cast(Wt, DT_BF16), THVM_DEV_METAL)));
  wbf       = uop_to_device_leaves(wbf, THVM_DEV_METAL);
  u32 perm[2]={1,0}; Term Bperm = uop_permute(wbf,2,perm);          // {N,K}->{K,N}
  u32 a3[3]={M,K,1}, mkn[3]={M,K,N}, b3[3]={1,K,N};
  Term Ae=uop_cast(uop_expand(uop_reshape(pbf,3,a3),3,mkn), DT_FP32);
  Term Be=uop_cast(uop_expand(uop_reshape(Bperm,3,b3),3,mkn), DT_FP32);
  Term outf=uop_reduce(REDUCE_SUM,1,uop_binary(UOP_MUL,Ae,Be));     // {M,N} f32
  Term outM=uop_to_device_leaves(outf, THVM_DEV_METAL);
  Term r=term_resolve(thvm_realize(outM));
  float *got=malloc((u64)M*N*4);
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,got,(u64)M*N*4);

  // bf16-faithful host reference: producer + weight rounded to bf16, product
  // accumulated in f32 (matching the simdgroup bfloat-operand / f32-accumulate).
  float *ref=malloc((u64)M*N*4);
  for(u32 m=0;m<M;m++)for(u32 n=0;n<N;n++){ float s=0;
    for(u32 k=0;k<K;k++){ float pr=bf16rt(X[m*K+k]*A[m*K+k]+B[m*K+k]);
                          float w =bf16rt(W[n*K+k]); s+=pr*w; }
    ref[m*N+n]=s; }

  TEST_BEGIN("metal/fuse-matmul-input");
  int f=0, bad=0; float maxd=0;
  for(u32 i=0;i<M*N;i++){ float d=fabsf(got[i]-ref[i]); if(d>maxd)maxd=d; if(d>0.05f)bad++; }
  printf("  flag=%s  maxAbsDiff=%.5f  bad=%d/%u\n",
         getenv("THVM_FUSE_MATMUL_INPUT")?getenv("THVM_FUSE_MATMUL_INPUT"):"off",
         maxd, bad, M*N);
  if (bad) { printf("  numeric mismatch vs bf16 reference -- FUSION INCORRECT\n"); f++; }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f);
  return f==0?0:1;
}
