// kvar-begin shrink READ: read a SINGLE row at a RUNTIME (kvar) offset out of a
// {n,d} table -- the GPT-2 decode POSITIONAL-embedding path (posTable[pos:pos+1]
// in tokenLmDecodeStep).  Distinct from the cache APPEND (a kvar-begin shrink as
// an ASSIGN *dst*, which is tested + works): here the kvar-begin shrink is a
// realize ROOT / read value.  Repro for the decode-echoes-the-prompt bug
// surfaced by LEVEL B (the toy decode tests had no positional embedding).
#include "../src/thvm.c"
#include "test.h"

int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("symbolic/kvar-begin-shrink-read");
  u32 n=8, d=4;
  Shape cs={0}; cs.ndim=2; cs.dims[0]=n; cs.dims[1]=d;
  u32 T=tensor_alloc(&CPU_BACKEND, cs, DT_FP32);
  float tbl[8*4]; for(u32 i=0;i<n;i++) for(u32 j=0;j<d;j++) tbl[i*d+j]=(float)i;
  CPU_BACKEND.buf_write(TENS[T].buf_id, tbl, (u64)n*d*4);

  u32 s=kvar_alloc("pos",1,n);
  u32 P=kvar_pack_extent(s);
  u32 want[3]={0,2,5};
  for(u32 ti=0; ti<3; ti++){
    u32 pos=want[ti];
    kvar_set_runtime(s, pos);
    u32 be[4]={ P, P+1, 0, d };
    Term sh = uop_shrink(term_new(0,TAG_TEN,DT_FP32,T), 2, be);
    Term r  = thvm_realize(sh);
    printf("  pos=%u -> realize tag=%d (TEN=%d)\n", pos, term_tag(r), TAG_TEN);
    CHECK(term_tag(r)==TAG_TEN);
    if(term_tag(r)==TAG_TEN){
      u32 rt=(u32)term_val(r);
      float o[4]; CPU_BACKEND.buf_read(TENS[rt].buf_id, o, (u64)d*4);
      printf("    row = %.1f %.1f %.1f %.1f  (want %u %u %u %u)\n",
             o[0],o[1],o[2],o[3], pos,pos,pos,pos);
      CHECK(o[0]==(float)pos && o[1]==(float)pos && o[2]==(float)pos && o[3]==(float)pos);
    }
  }

  // IN-CONTEXT (in-kernel): the SAME kvar-begin shrink consumed by an ADD.  The
  // standalone realize above goes through materialize_root_alias (gather), which
  // resolves the begin via kvar_extent_runtime (correct).  As a kernel INPUT the
  // offset must ALSO resolve to kvar_runtime -- but the codegen path uses the
  // kvar id / static extent, so it reads a FIXED wrong row (the GPT-2 decode
  // posT-dropped bug: posT = posTable[pos:pos+1] feeds `x = embed + posT`).
  Shape ds={0}; ds.ndim=2; ds.dims[0]=1; ds.dims[1]=d;
  u32 D=tensor_alloc(&CPU_BACKEND, ds, DT_FP32);
  float dz[4]={100,100,100,100}; CPU_BACKEND.buf_write(TENS[D].buf_id, dz, (u64)d*4);
  kvar_set_runtime(s, 2);
  u32 be2[4]={ P, P+1, 0, d };
  Term sh2 = uop_shrink(term_new(0,TAG_TEN,DT_FP32,T), 2, be2);
  Term add = uop_binary(UOP_ADD, term_new(0,TAG_TEN,DT_FP32,D), sh2);
  Term ra  = thvm_realize(add);
  printf("  in-kernel add(100, table[2]):\n");
  CHECK(term_tag(ra)==TAG_TEN);
  if(term_tag(ra)==TAG_TEN){
    u32 rt=(u32)term_val(ra);
    float o[4]; CPU_BACKEND.buf_read(TENS[rt].buf_id, o, (u64)d*4);
    printf("    = %.1f %.1f %.1f %.1f  (want 102 102 102 102)\n", o[0],o[1],o[2],o[3]);
    CHECK(o[0]==102.0f && o[1]==102.0f && o[2]==102.0f && o[3]==102.0f);
  }

  TEST_REPORT();
}
