// KV-cache APPEND: write a new token K_t {1,dim} into a persistent
// {nCtx,dim} cache buffer at a RUNTIME (kvar) row offset t, in place,
// leaving every other row untouched.  This is the WRITE side of the
// decode KV-cache (the READ side is test_sym_kvcache.c).
//
// Faithful to tinygrad's append: an ASSIGN whose dst is a SHRUNK view of
// the persistent cache at a symbolic row offset --
//   AFTER(cache, STORE(cache[..., start_pos:start_pos+T, ...], k))
// (gpt2.py:201).  thvm's kvar IS that Variable.  The append is
//   uop_assign( uop_shrink(cache, {{t,t+1},{0,d}}), K_t )
// with begin `t` a kvar-packed value, realized after kvar_set_runtime.
#include "../src/thvm.c"
#include "test.h"

// Build an UOP_ASSIGN(dst, src) term.  Heap layout [dst, src] (the same
// cells interact_assign/materialize read).  No public uop_assign() yet
// (the WL surface builds it via the generic head->opcode mapping), so
// the test constructs it inline.
static Term mk_assign(Term dst, Term src){
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, dst);
  heap_set(loc + 1, src);
  return term_new(0, TAG_UOP, UOP_ASSIGN, loc);
}

int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("symbolic/kvcache-append-at-runtime-offset");
  u32 nctx=8, d=3;

  // (1) persistent cache {nctx,d}, zero-filled (all 24 floats).
  Shape cs={0}; cs.ndim=2; cs.dims[0]=nctx; cs.dims[1]=d;
  u32 Kc=tensor_alloc(&CPU_BACKEND,cs,DT_FP32);
  float z[8*3]={0}; CPU_BACKEND.buf_write(TENS[Kc].buf_id,z,(u64)nctx*d*4);

  // (2) new token K_t {1,d} = {7,8,9}.
  Shape qs={0}; qs.ndim=2; qs.dims[0]=1; qs.dims[1]=d;
  u32 qid=tensor_alloc(&CPU_BACKEND,qs,DT_FP32);
  float kt[3]={7,8,9}; CPU_BACKEND.buf_write(TENS[qid].buf_id,kt,d*4);

  // (3) runtime row offset t = 4 (a kvar in [1,nctx]).
  u32 s=kvar_alloc("pos",1,nctx); kvar_set_runtime(s,4);
  u32 P=kvar_pack_extent(s);

  // (4) append: dst = cache[t:t+1, 0:d]; ASSIGN K_t into it; realize.
  //     begin axis0 = pack(s) (kvar), end axis0 = pack(s)+1 ({t,t+1} row).
  u32 be0[4]={ P, P+1, 0, d };
  Term dst=uop_shrink(term_new(0,TAG_TEN,DT_FP32,Kc), 2, be0);
  Term app=mk_assign(dst, term_new(0,TAG_TEN,DT_FP32,qid));
  thvm_realize(app);

  // (5) read all 24 floats from the cache buffer; row 4 == {7,8,9},
  //     every other row still 0.
  float o[8*3]; CPU_BACKEND.buf_read(TENS[Kc].buf_id,o,(u64)nctx*d*4);
  printf("  after append@4: row4 = %.1f %.1f %.1f (want 7 8 9)\n",
         o[12],o[13],o[14]);
  CHECK(o[12]==7.0f && o[13]==8.0f && o[14]==9.0f);
  for(u32 j=0;j<nctx;j++) if(j!=4)
    for(u32 k=0;k<d;k++) CHECK(o[j*d+k]==0.0f);

  // (6) re-append at a DIFFERENT runtime offset t = 2 with {1,2,3};
  //     the runtime offset is LIVE (row 2 written, row 4 survives).
  kvar_set_runtime(s,2);
  float kt2[3]={1,2,3}; CPU_BACKEND.buf_write(TENS[qid].buf_id,kt2,d*4);
  u32 be1[4]={ P, P+1, 0, d };
  Term dst2=uop_shrink(term_new(0,TAG_TEN,DT_FP32,Kc), 2, be1);
  Term app2=mk_assign(dst2, term_new(0,TAG_TEN,DT_FP32,qid));
  thvm_realize(app2);

  CPU_BACKEND.buf_read(TENS[Kc].buf_id,o,(u64)nctx*d*4);
  printf("  after append@2: row2 = %.1f %.1f %.1f (want 1 2 3), "
         "row4 = %.1f %.1f %.1f (want 7 8 9)\n",
         o[6],o[7],o[8], o[12],o[13],o[14]);
  CHECK(o[6]==1.0f && o[7]==2.0f && o[8]==3.0f);   // new write landed
  CHECK(o[12]==7.0f && o[13]==8.0f && o[14]==9.0f); // first write survives
  for(u32 j=0;j<nctx;j++) if(j!=2 && j!=4)
    for(u32 k=0;k<d;k++) CHECK(o[j*d+k]==0.0f);

  TEST_REPORT();
}
