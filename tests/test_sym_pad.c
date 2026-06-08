// Regression guard: uop_pad over a tensor whose leading axis is symbolic (kvar)
// must preserve the original data region (zero only the pad columns).  It once
// zeroed EVERYTHING: ru_pad_wrap_where built a value-side bound `ILT(RANGE,
// CONST(in_dim))` using the raw kvar-packed extent (0x80000001) for the
// unpadded leading axis; as a negative i32 the ILT simplifier folded it to
// constant FALSE -> the whole IWHERE collapsed to INVALID.  Fixed by skipping
// unpadded axes + resolving the padded dim via kvar_extent_static.
#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("symbolic/pad-over-kvar-axis");
  u32 hi=16, d=3, s=kvar_alloc("s",1,hi);
  Shape sh={0}; sh.ndim=2; sh.dims[0]=kvar_pack_extent(s); sh.dims[1]=d;
  u32 t=tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  u32 n=TENS[t].view.numel; float*b=malloc(n*4); for(u32 i=0;i<n;i++)b[i]=1.0f;
  CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4); free(b);
  u32 widths[4]={0,0, 0,1};                       // pad axis 1 end by 1 -> {S, d+1}
  Term pd = uop_pad(term_new(0,TAG_TEN,DT_FP32,t), 2, widths);
  kvar_set_runtime(s, 4);
  Term r=term_resolve(thvm_realize(pd));
  float o[64]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id, o, (u64)4*(d+1)*4);
  printf("  padded[0,0..3]=%g %g %g %g (want 1 1 1 0)\n", o[0],o[1],o[2],o[3]);
  for(u32 i=0;i<4;i++){ for(u32 j=0;j<d;j++) if(fabsf(o[i*(d+1)+j]-1.0f)>0.01f) f++;
                        if(fabsf(o[i*(d+1)+d]-0.0f)>0.01f) f++; }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
