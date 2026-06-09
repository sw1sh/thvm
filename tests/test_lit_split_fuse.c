// Same split-fuse as test_sym_split_fuse but with a LITERAL leading axis (no
// kvar).  Determines whether the permute-addr-row-major bug is symbolic-only
// or general.  val[r,c]=r*10+c; split head1; reduce dH -> {S}; want 5 25 45 65.
#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("literal/split-fuse-rows");
  u32 S=4, nH=2, dH=2, dim=nH*dH, h=1;
  Shape sh={0}; sh.ndim=2; sh.dims[0]=S; sh.dims[1]=dim;
  u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);
  float*b=malloc((u64)S*dim*4); for(u32 r=0;r<S;r++)for(u32 c=0;c<dim;c++)b[r*dim+c]=(float)(r*10+c);
  CPU_BACKEND.buf_write(TENS[t].buf_id,b,(u64)S*dim*4); free(b);
  u32 snh[3]={S,nH,dH}; u32 perm[3]={1,0,2}; u32 beg[6]={h,h+1,0,S,0,dH}; u32 sd[2]={S,dH};
  Term sp = uop_reshape(uop_shrink(uop_permute(uop_reshape(term_new(0,TAG_TEN,DT_FP32,t),3,snh),3,perm),3,beg),2,sd);
  Term red = uop_reduce(REDUCE_SUM,1,sp);
  Term r=term_resolve(thvm_realize(red));
  float o[16]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,16);
  printf("  LITERAL rowsums -> %g %g %g %g (want 5 25 45 65)\n",o[0],o[1],o[2],o[3]);
  float w[4]={5,25,45,65}; for(u32 i=0;i<4;i++) if(fabsf(o[i]-w[i])>0.01f) f++;
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
