// test_cpu_jit_cache.c -- the CPU JIT's in-memory kernel cache holds every
// kernel a process compiles.
//
// The cache maps a rendered-source hash to its compiled function.  A fixed
// 256-slot table left every kernel past its capacity on the UOp walker for
// the rest of the process: slow, and wrong on the kernels the walker
// mis-evaluates (a GPT-2 decode loop, whose position is baked into each
// step's kernels, crossed it in its second generation and decoded garbage).
// Checks: 100000 claimed keys all stay findable with their state; claiming
// a present key returns its slot; reset empties the table.
#include "../src/thvm.c"
#include "test.h"

static u64 key_of(u32 i) {
  return (1ULL << 63) | ((u64)i * 2u + 1u);
}

int main(void) {
  thvm_init();
  enum { N = 100000 };

  TEST_BEGIN("cpu-jit-cache/claim-many");
  int claimed = 1;
  for (u32 i = 0; i < N && claimed; i++) {
    CpuJitSlot *s = cpu_jit_claim_slot(key_of(i));
    claimed = s != NULL && s->key == key_of(i) && s->func == NULL && s->fire_count == 0;
    if (claimed) {
      s->fire_count = i + 1;
    }
  }
  CHECK(claimed);

  TEST_BEGIN("cpu-jit-cache/find-all");
  int found = 1;
  for (u32 i = 0; i < N && found; i++) {
    CpuJitSlot *s = cpu_jit_find_slot(key_of(i));
    found = s != NULL && s->key == key_of(i) && s->fire_count == i + 1;
  }
  CHECK(found);
  CHECK(cpu_jit_find_slot(key_of(N)) == NULL);

  TEST_BEGIN("cpu-jit-cache/claim-present");
  CpuJitSlot *s = cpu_jit_claim_slot(key_of(7));
  CHECK(s != NULL && s->fire_count == 8);

  TEST_BEGIN("cpu-jit-cache/reset");
  cpu_jit_cache_reset();
  int gone = 1;
  for (u32 i = 0; i < N && gone; i++) {
    gone = cpu_jit_find_slot(key_of(i)) == NULL;
  }
  CHECK(gone);
  s = cpu_jit_claim_slot(key_of(3));
  CHECK(s != NULL && s->key == key_of(3) && s->fire_count == 0);

  thvm_free();
  TEST_REPORT();
}
