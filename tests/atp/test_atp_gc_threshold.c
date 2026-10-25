// test_atp_gc_threshold.c - the packed-trace threshold collection.
//
// atp_transient_gc_due counts the cells allocated since the last
// collection, not the whole from-space: a live set past the threshold
// must not make every step collect again (the WolframAxioms
// ImpliesShefferAxioms run spent 95 of 121 s in such collections).

#include "../../src/thvm.c"
#include "../test.h"

static u32 gt_weights   [2] = {0, 1};
static u32 gt_precedence[2] = {0, 1};
static const KboConfig GT_CFG = {
  .weights     = gt_weights,
  .precedence  = gt_precedence,
  .n_labels    = 2,
  .var_weight  = 1,
};

int main(void) {
  // A threshold of 1M cells keeps the allocations below small.
  setenv("THVM_ATP_GC_MB", "1", 1);
  thvm_init();
  const u64 T = 1ull << 20;

  TEST_BEGIN("atp_gc_threshold/scratch-since-last-collection");
  {
    AtpState *s = thvm_atp_init(&GT_CFG, 16u);
    s->trace_pack = 1u;
    thvm_atp_gc_collect(s);
    CHECK(!atp_transient_gc_due(s));
    heap_alloc(T / 2u);
    CHECK(!atp_transient_gc_due(s));
    heap_alloc(T);
    CHECK(atp_transient_gc_due(s));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp_gc_threshold/live-set-past-threshold");
  {
    AtpState *s = thvm_atp_init(&GT_CFG, 16u);
    s->trace_pack = 1u;
    heap_alloc(2u * T);
    // As if the last collection had left all of it live: the from-space
    // holds twice the threshold, yet nothing was allocated since.
    s->gc_live_after = HEAP_NEXT - gc_from_start();
    CHECK((HEAP_NEXT - gc_from_start()) > T);
    CHECK(!atp_transient_gc_due(s));
    heap_alloc(T + 1u);
    CHECK(atp_transient_gc_due(s));
    s->trace_pack = 0u;
    CHECK(!atp_transient_gc_due(s));
    thvm_atp_free(s);
  }

  TEST_REPORT();
}
