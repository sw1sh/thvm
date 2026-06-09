// headStitch mirror: concat nH=2 per-head {S,dH=2} tensors into {S,dim=4} via
// per-head PAD into the column slot + SUM, with a SYMBOLIC leading axis S.
// head0 (=1.0) pads END {0,2}; head1 (=2.0) pads BEGIN {2,0}; sum -> rows {1,1,2,2}.
// Isolates: begin-pad over a symbolic-axis tensor + summing two symbolic PADs.
#include "../src/thvm.c"
#include "test.h"
static u32 symten(u32 hi,u32 s,u32 d,float fill){Shape sh={0};sh.ndim=2;sh.dims[0]=kvar_pack_extent(s);sh.dims[1]=d;u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);u32 n=hi*d;float*b=malloc(n*4);for(u32 i=0;i<n;i++)b[i]=fill;CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4);free(b);return t;}
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("symbolic/pad-concat-headstitch");
  u32 hi=16, dH=2, s=kvar_alloc("s",1,hi);
  Term h0=term_new(0,TAG_TEN,DT_FP32,symten(hi,s,dH,1.0f)); // {S,2}=1
  Term h1=term_new(0,TAG_TEN,DT_FP32,symten(hi,s,dH,2.0f)); // {S,2}=2
  u32 wEnd[4]={0,0, 0,dH};    // head0 -> cols [0,2)
  u32 wBeg[4]={0,0, dH,0};    // head1 -> cols [2,4)
  Term p0=uop_pad(h0,2,wEnd), p1=uop_pad(h1,2,wBeg);
  Term sum=uop_binary(UOP_ADD,p0,p1);   // {S,4}
  kvar_set_runtime(s,4);
  Term r=term_resolve(thvm_realize(sum));
  float o[64]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,(u64)4*4*4);
  printf("  row0=%g %g %g %g (want 1 1 2 2)\n", o[0],o[1],o[2],o[3]);
  for(u32 i=0;i<4;i++){ float w[4]={1,1,2,2}; for(u32 j=0;j<4;j++) if(fabsf(o[i*4+j]-w[j])>0.01f) f++; }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
