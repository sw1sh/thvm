// inner-symbolic REALIZE: a {A, S} tensor (S inner symbolic) reduced over the
// inner axis -> {A}, each row = S; and a {S, S} (both axes symbolic) reduced
// over the inner axis -> {S}, each = S.  Exercises ru_build_addr_with_dims
// sizing the addr stride coefficient at the kvar upper bound.
#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("inner/reduce-over-inner-symbolic");
  {
    u32 s = kvar_alloc("s",1,16); u32 A=3;
    Shape sh={0}; sh.ndim=2; sh.dims[0]=A; sh.dims[1]=kvar_pack_extent(s);
    u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
    u32 n=TENS[t].view.numel; float*b=malloc(n*4); for(u32 i=0;i<n;i++)b[i]=1.0f;
    CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4); free(b);
    Term red = uop_reduce(REDUCE_SUM, 1, term_new(0,TAG_TEN,DT_FP32,t)); // {A}
    kvar_set_runtime(s, 5);
    Term r = term_resolve(thvm_realize(red));
    float out[3]={-1,-1,-1};
    TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id, out, 12);
    printf("  {3,S=5} rowsums -> %g %g %g (want 5 5 5)\n", out[0],out[1],out[2]);
    for(int i=0;i<3;i++) if(!(out[i]>4.9f&&out[i]<5.1f)) f++;
  }
  TEST_BEGIN("inner/SxS-reduce-inner");
  {
    u32 s = kvar_alloc("ss",1,16);
    Shape sh={0}; sh.ndim=2; sh.dims[0]=kvar_pack_extent(s); sh.dims[1]=kvar_pack_extent(s);
    u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
    u32 n=TENS[t].view.numel; float*b=malloc(n*4); for(u32 i=0;i<n;i++)b[i]=1.0f;
    CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4); free(b);
    Term red = uop_reduce(REDUCE_SUM, 1, term_new(0,TAG_TEN,DT_FP32,t)); // {S}, each=S
    kvar_set_runtime(s, 4);
    Term r = term_resolve(thvm_realize(red));
    float out[4]={-1,-1,-1,-1};
    TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id, out, 16);
    printf("  {S=4,S=4} rowsums -> %g %g %g %g (want 4 4 4 4)\n", out[0],out[1],out[2],out[3]);
    for(int i=0;i<4;i++) if(!(out[i]>3.9f&&out[i]<4.1f)) f++;
  }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
