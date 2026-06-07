// view_create sizes a SYMBOLIC (kvar) dim's strides at its UPPER BOUND, so a
// tensor with a symbolic INNER axis ({A, S}) has a sane row width (hi) rather
// than a kvar-packed extent.  This is the foundation for {S,S} attention.
//
// NOTE: full inner-symbolic *realize* additionally needs the addr-coefficient
// layer (the lift bakes a tensor's outer stride = product of inner RANGE
// extents straight from the raw kvar) to be kvar-aware -- see
// docs/plans/decode_roadmap.md M2.5.  The OUTER-symbolic case ({S, dim}, all
// of GPT-2 so far) already realizes (M1 / M2-GEMM); only {S,S} is blocked.
#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init(); int f=0;
  u32 s = kvar_alloc("s",1,16);   // hi = 16
  TEST_BEGIN("inner/view-strides-sized-at-bound");
  {
    // {A=3, S} : inner symbolic.  strides must be [hi, 1] = [16, 1], numel A*hi.
    Shape sh={0}; sh.ndim=2; sh.dims[0]=3; sh.dims[1]=kvar_pack_extent(s);
    u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
    printf("  {3,S}  strides=[%d,%d] numel=%u (want [16,1] 48)\n",
           TENS[t].view.strides[0], TENS[t].view.strides[1], TENS[t].view.numel);
    if (TENS[t].view.strides[0]!=16 || TENS[t].view.strides[1]!=1) f++;
    if (TENS[t].view.numel != 48) f++;
  }
  TEST_BEGIN("inner/view-strides-outer-symbolic");
  {
    // {S, 4} : outer symbolic.  strides [4,1] (kvar not in the product), numel hi*4.
    Shape sh={0}; sh.ndim=2; sh.dims[0]=kvar_pack_extent(s); sh.dims[1]=4;
    u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
    printf("  {S,4}  strides=[%d,%d] numel=%u (want [4,1] 64)\n",
           TENS[t].view.strides[0], TENS[t].view.strides[1], TENS[t].view.numel);
    if (TENS[t].view.strides[0]!=4 || TENS[t].view.strides[1]!=1) f++;
    if (TENS[t].view.numel != 64) f++;
  }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
