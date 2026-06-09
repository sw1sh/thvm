// Minimal repro: lazy FUSED multi-head split over a symbolic axis mis-addresses
// rows>0.  {S, nH*dH} with val[r,c]=r*10+c; split head1 (cols dH..2dH-1) ->
// {S,dH}; reduce-sum over dH -> {S}; row r = (10r+dH)+(10r+dH+1)+... .
// BAR env: 0=fully lazy (1 realize), 1=realize the split before the reduce.
#include "../src/thvm.c"
#include "test.h"
static int BAR;
static Term R(Term t){ return term_new(0,TAG_TEN,DT_FP32,(u32)term_val(term_resolve(thvm_realize(t)))); }
static Term br(Term t){ return BAR?R(t):t; }
int main(void){ thvm_init(); BAR=getenv("BAR")?atoi(getenv("BAR")):0; int f=0;
  TEST_BEGIN("symbolic/split-fuse-rows");
  u32 hi=8, nH=2, dH=2, dim=nH*dH, h=1, s=kvar_alloc("s",1,hi);
  Shape sh={0}; sh.ndim=2; sh.dims[0]=kvar_pack_extent(s); sh.dims[1]=dim;
  u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);
  float*b=malloc((u64)hi*dim*4); for(u32 r=0;r<hi;r++)for(u32 c=0;c<dim;c++)b[r*dim+c]=(float)(r*10+c);
  CPU_BACKEND.buf_write(TENS[t].buf_id,b,(u64)hi*dim*4); free(b);
  u32 P=kvar_pack_extent(s);
  // split head h: reshape {S,dim}->{S,nH,dH}; permute {1,0,2}->{nH,S,dH};
  //   shrink {{h,h+1},{0,S},{0,dH}}->{1,S,dH}; reshape ->{S,dH}.
  u32 snh[3]={P,nH,dH}; u32 perm[3]={1,0,2}; u32 beg[6]={h,h+1,0,P,0,dH}; u32 sd[2]={P,dH};
  Term sp = uop_reshape(uop_shrink(uop_permute(uop_reshape(term_new(0,TAG_TEN,DT_FP32,t),3,snh),3,perm),3,beg),2,sd);
  sp = br(sp);
  Term red = uop_reduce(REDUCE_SUM,1,sp);   // {S}
  kvar_set_runtime(s,4);
  Term r=term_resolve(thvm_realize(red));
  float o[16]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,16);
  printf("  BAR=%d rowsums -> %g %g %g %g (want 5 25 45 65)\n",BAR,o[0],o[1],o[2],o[3]);
  float w[4]={5,25,45,65}; for(u32 i=0;i<4;i++) if(fabsf(o[i]-w[i])>0.01f) f++;
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
