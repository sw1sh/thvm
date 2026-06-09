// Stage 2 repro: after the (now-fixed) per-head split, the {S,S} einsum.
// A = split head1 of {S,dim=4} (val[r,c]=r*10+c) -> {S,dH=2}; A[i]=(10i+2,10i+3).
// scores[i,j] = sum_d A[i,d]*A[j,d]; out[i] = sum_j scores[i,j].  Compare a
// fully-LAZY fused build to an EAGER (realize-every-op) reference.
#include "../src/thvm.c"
#include "test.h"
static int EAGER;
static Term R(Term t){ return term_new(0,TAG_TEN,DT_FP32,(u32)term_val(term_resolve(thvm_realize(t)))); }
static Term e_(Term t){ return EAGER?R(t):t; }
int main(void){ thvm_init(); int f=0; float ref[4];
  u32 hi=8, nH=2, dH=2, dim=nH*dH, h=1, s=kvar_alloc("s",1,hi);
  for (int pass=0; pass<2; pass++){
    EAGER = (pass==0);
    Shape sh={0}; sh.ndim=2; sh.dims[0]=kvar_pack_extent(s); sh.dims[1]=dim;
    u32 t=tensor_alloc(&CPU_BACKEND,sh,DT_FP32);
    float*b=malloc((u64)hi*dim*4); for(u32 r=0;r<hi;r++)for(u32 c=0;c<dim;c++)b[r*dim+c]=(float)(r*10+c);
    CPU_BACKEND.buf_write(TENS[t].buf_id,b,(u64)hi*dim*4); free(b);
    u32 P=kvar_pack_extent(s);
    u32 snh[3]={P,nH,dH}, perm[3]={1,0,2}, beg[6]={h,h+1,0,P,0,dH}, sd[2]={P,dH};
    Term A=e_(uop_reshape(uop_shrink(uop_permute(uop_reshape(term_new(0,TAG_TEN,DT_FP32,t),3,snh),3,perm),3,beg),2,sd));
    // scores = einsum id,jd->ij
    u32 s1d[3]={P,1,dH}, _1sd[3]={1,P,dH}, ssd[3]={P,P,dH};
    Term ae=e_(uop_expand(uop_reshape(A,3,s1d),3,ssd));
    Term be=e_(uop_expand(uop_reshape(A,3,_1sd),3,ssd));
    Term scores=e_(uop_reduce(REDUCE_SUM,2,e_(uop_binary(UOP_MUL,ae,be)))); // {S,S}
    Term out=uop_reduce(REDUCE_SUM,1,scores); // {S}
    kvar_set_runtime(s,4);
    Term r=term_resolve(thvm_realize(out));
    float o[16]; TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id,o,16);
    if(pass==0){ for(int i=0;i<4;i++) ref[i]=o[i]; printf("  EAGER  -> %g %g %g %g\n",o[0],o[1],o[2],o[3]); }
    else       { printf("  LAZY   -> %g %g %g %g\n",o[0],o[1],o[2],o[3]);
                 for(int i=0;i<4;i++) if(fabsf(o[i]-ref[i])>0.01f) f++; }
  }
  printf("  %s (%d lazy-vs-eager mismatches)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
