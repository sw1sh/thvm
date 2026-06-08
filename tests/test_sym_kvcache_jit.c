// KV-cache append under TJit: a decode step's in-place cache write is captured
// ONCE, then REPLAYED at a rebound row offset.  The append's dst is a SHRUNK
// view of the {nctx,dim} cache at a RUNTIME (kvar) row offset start_pos; the JIT
// ASSIGN replay must re-resolve that row from kvar_runtime at FIRE, not bake the
// capture-time row (offset baked into view.offset is only that one step's row).
//
// This is decode roadmap Lever 2 STEP 5 (per-step replay): capture the append
// at start_pos=2, then rebind start_pos=3 and replay -- the write must land at
// row 3 (the live kvar), not row 2 (the captured value).  Mirrors tinygrad's
// jitted decode: one captured graph, the symbolic start_pos rebinds per token.
#include "../src/thvm.c"
#include "test.h"

static Term mk_assign(Term dst, Term src){
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, dst);
  heap_set(loc + 1, src);
  return term_new(0, TAG_UOP, UOP_ASSIGN, loc);
}

int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("symbolic/kvcache-append-jit-replay-rebind");
  u32 nctx=8, d=3;

  // persistent {nctx,d} cache, zero-filled.
  Shape cs={0}; cs.ndim=2; cs.dims[0]=nctx; cs.dims[1]=d;
  u32 Kc=tensor_alloc(&CPU_BACKEND,cs,DT_FP32);
  float z[8*3]={0}; CPU_BACKEND.buf_write(TENS[Kc].buf_id,z,(u64)nctx*d*4);

  // new token K_t {1,d} = {7,8,9}.
  Shape qs={0}; qs.ndim=2; qs.dims[0]=1; qs.dims[1]=d;
  u32 qid=tensor_alloc(&CPU_BACKEND,qs,DT_FP32);
  float kt[3]={7,8,9}; CPU_BACKEND.buf_write(TENS[qid].buf_id,kt,d*4);

  u32 s=kvar_alloc("pos",1,nctx);
  u32 P=kvar_pack_extent(s);
  u32 be0[4]={ P, P+1, 0, d };

  // CAPTURE: append K_t at the runtime row start_pos = 2.
  u32 slot=jit_capture_begin();
  kvar_set_runtime(s,2);
  Term dst=uop_shrink(term_new(0,TAG_TEN,DT_FP32,Kc), 2, be0);
  Term app=mk_assign(dst, term_new(0,TAG_TEN,DT_FP32,qid));
  thvm_realize(app);
  jit_capture_end();

  float o[8*3];
  CPU_BACKEND.buf_read(TENS[Kc].buf_id,o,(u64)nctx*d*4);
  printf("  after capture@2: row2 = %.1f %.1f %.1f (want 7 8 9)\n",o[6],o[7],o[8]);
  CHECK(o[6]==7.0f && o[7]==8.0f && o[8]==9.0f);   // capture wrote row 2

  // REPLAY at a REBOUND row start_pos = 3, with a FRESH src value {1,2,3}
  // overwritten into the captured src buffer -- proves replay re-reads BOTH the
  // live src bytes AND the live kvar row (not the row baked at capture).
  kvar_set_runtime(s,3);
  float kt2[3]={1,2,3}; CPU_BACKEND.buf_write(TENS[qid].buf_id,kt2,d*4);
  jit_replay(slot);

  CPU_BACKEND.buf_read(TENS[Kc].buf_id,o,(u64)nctx*d*4);
  printf("  after replay@3: row3 = %.1f %.1f %.1f (want 1 2 3), "
         "row2 = %.1f %.1f %.1f (want 7 8 9 -- capture survives)\n",
         o[9],o[10],o[11], o[6],o[7],o[8]);
  CHECK(o[9]==1.0f && o[10]==2.0f && o[11]==3.0f);   // replay landed at REBOUND row 3
  CHECK(o[6]==7.0f && o[7]==8.0f && o[8]==9.0f);      // capture-time row 2 untouched
  for(u32 j=0;j<nctx;j++) if(j!=2 && j!=3)
    for(u32 k=0;k<d;k++) CHECK(o[j*d+k]==0.0f);       // every other row still 0

  // REPLAY again at start_pos = 0 (the byte_off == 0 boundary): row 0 must take
  // the write, exercising the offset-aware path at zero offset.
  kvar_set_runtime(s,0);
  jit_replay(slot);
  CPU_BACKEND.buf_read(TENS[Kc].buf_id,o,(u64)nctx*d*4);
  printf("  after replay@0: row0 = %.1f %.1f %.1f (want 1 2 3)\n",o[0],o[1],o[2]);
  CHECK(o[0]==1.0f && o[1]==2.0f && o[2]==3.0f);       // row-0 rebind landed

  TEST_REPORT();
}
