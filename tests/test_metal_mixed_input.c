// Mixed-device kernel input: a kernel routed to Metal (output on Metal) that
// reads ONE Metal input (an uploaded leaf) and ONE CPU input (a raw leaf, e.g.
// a net weight not moved to the device).  out = copy(A,metal) + B_cpu must equal
// A + B.  Currently interact_kernel passes B's CPU buf-id straight to the Metal
// dispatch (no cross-backend upload), so Metal reads its own buffer pool at that
// id -> wrong.  This is what makes a net with CPU weights diverge when its input
// is TToDevice'd to Metal.  Built -DTHVM_HAS_METAL.
#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init();   // default backend = CPU
  int f=0; TEST_BEGIN("metal/mixed-device-kernel-input");
  u32 N=8; Shape sh={0}; sh.ndim=1; sh.dims[0]=N;
  float a[8],b[8]; for(u32 i=0;i<N;i++){a[i]=0.5f*i-1.0f; b[i]=2.0f-0.3f*i;}
  u32 ta=tensor_alloc(&CPU_BACKEND,sh,DT_FP32); CPU_BACKEND.buf_write(TENS[ta].buf_id,a,N*4);
  u32 tb=tensor_alloc(&CPU_BACKEND,sh,DT_FP32); CPU_BACKEND.buf_write(TENS[tb].buf_id,b,N*4);
  // out = copy(A, metal) + B(cpu)  -- routes to metal (the COPY'd operand)
  Term out = uop_binary(UOP_ADD,
      uop_copy_dev(term_new(0,TAG_TEN,DT_FP32,ta), THVM_DEV_METAL),
      term_new(0,TAG_TEN,DT_FP32,tb));
  Term r = term_resolve(thvm_realize(out));
  float o[8]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,N*4);
  printf("  out: "); for(u32 i=0;i<N;i++)printf("%.2f ",o[i]);
  printf("\n  want:"); for(u32 i=0;i<N;i++)printf("%.2f ",a[i]+b[i]);
  printf("\n");
  for(u32 i=0;i<N;i++) if(fabsf(o[i]-(a[i]+b[i]))>1e-4f) f++;
  printf("  %s (%d mismatches)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
