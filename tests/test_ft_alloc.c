// test_ft_alloc.c - dual-arena allocator unit tests.
//
// Exercises ft_init / ft_destroy, persistent slab + free-list recycle,
// scratch bump + reset + overflow grow, and ft_walk_persistent over
// multiple blocks.
//
// Built only when THVM_ATPFT_ALLOC is defined (Makefile flag); the
// test file #includes the runtime first, then ft_alloc.c directly so
// the gated code is exercised even though src/thvm.c does not yet
// pick it up via the usual include chain.

#include "../src/thvm.c"

#ifndef THVM_ATPFT_ALLOC
#define THVM_ATPFT_ALLOC 1
#endif

#include "../src/atp/ft.h"
#include "../src/atp/ft_alloc.c"

#include "test.h"

// --- helpers -----------------------------------------------------------

typedef struct {
  u64  count;
  u32  sym_xor;   // cheap distinctness check -- xor of every visited sym
} VisitAcc;

static void visit_count_xor(AtpFtCell *cell, void *ctx) {
  VisitAcc *acc = (VisitAcc *)ctx;
  acc->count   += 1u;
  acc->sym_xor ^= cell->sym;
}

// Build a fully-linked chain of N persistent cells (cell[i].next ==
// cell[i+1]), returning {first, last}.  Used to test ft_free_span's
// chain-push idiom.
static void build_chain(AtpFt *a, u32 n, AtpFtCell **out_first, AtpFtCell **out_last) {
  AtpFtCell *first = ft_alloc_persistent(a);
  first->sym = 1u;
  AtpFtCell *prev = first;
  for (u32 i = 1; i < n; i++) {
    AtpFtCell *c = ft_alloc_persistent(a);
    c->sym = i + 1u;
    prev->next = c;
    prev = c;
  }
  prev->next = NULL;
  *out_first = first;
  *out_last  = prev;
}

int main(void) {
  thvm_init();

  // ---- Test 1: init / destroy is a no-op on an empty arena ---------
  TEST_BEGIN("ft_alloc/init-destroy-empty");
  {
    AtpFt a;
    ft_init(&a);
    CHECK_EQ(a.n_blocks, 0u);
    CHECK_EQ(a.n_persistent_alive, 0u);
    CHECK_EQ(a.n_scratch_alive, 0u);
    CHECK(a.blocks == NULL);
    CHECK(a.free_head == NULL);
    CHECK(a.scratch_base == NULL);
    ft_destroy(&a);
    CHECK_EQ(a.n_blocks, 0u);
  }

  // ---- Test 2: 10000 persistent alloc + walk_persistent counts -----
  TEST_BEGIN("ft_alloc/persistent-walk-counts");
  {
    AtpFt a;
    ft_init(&a);
    enum { N2 = 10000 };
    u32 sym_xor_expected = 0;
    for (u32 i = 0; i < N2; i++) {
      AtpFtCell *c = ft_alloc_persistent(&a);
      c->sym = i + 1u;          // distinct, nonzero
      sym_xor_expected ^= c->sym;
    }
    CHECK_EQ(a.n_persistent_alive, (u64)N2);
    VisitAcc acc = {0, 0};
    ft_walk_persistent(&a, visit_count_xor, &acc);
    CHECK_EQ(acc.count, (u64)N2);
    CHECK_EQ(acc.sym_xor, sym_xor_expected);
    ft_destroy(&a);
  }

  // ---- Test 3: free_span recycles via the free list ----------------
  TEST_BEGIN("ft_alloc/free-span-recycles");
  {
    AtpFt a;
    ft_init(&a);
    enum { N3 = 5000 };
    AtpFtCell *first = NULL, *last = NULL;
    build_chain(&a, N3, &first, &last);
    CHECK_EQ(a.n_persistent_alive, (u64)N3);
    ft_free_span(&a, first, last);
    CHECK_EQ(a.n_persistent_alive, 0u);
    // Allocate another N3 -- the alive counter should hit exactly N3
    // and the walk should see only N3 cells, proving the freed cells
    // recycled instead of inflating the live set.
    for (u32 i = 0; i < N3; i++) {
      AtpFtCell *c = ft_alloc_persistent(&a);
      c->sym = i + 1u;
    }
    CHECK_EQ(a.n_persistent_alive, (u64)N3);
    VisitAcc acc = {0, 0};
    ft_walk_persistent(&a, visit_count_xor, &acc);
    CHECK_EQ(acc.count, (u64)N3);
    ft_destroy(&a);
  }

  // ---- Test 4: scratch bump + reset reuses memory ------------------
  //
  // The point of reset is that AFTER the region has reached its final
  // size, a reset followed by N more allocations hands out the SAME
  // pointers at the SAME offsets (no realloc, no malloc/free churn).
  //
  // Pointers captured DURING the first round are stale -- the bump
  // arena grew several times to reach its final capacity, and each
  // grow may have moved scratch_base.  So compare round-2 pointers
  // against the round-1 BASE recorded immediately before reset.
  TEST_BEGIN("ft_alloc/scratch-bump-reset-reuse");
  {
    AtpFt a;
    ft_init(&a);
    enum { N4 = 100000 };
    for (u32 i = 0; i < N4; i++) (void)ft_alloc_scratch(&a);
    CHECK_EQ(a.n_scratch_alive, (u64)N4);
    AtpFtCell *base_after_grow = a.scratch_base;
    AtpFtCell *end_after_grow  = a.scratch_end;
    ft_scratch_reset(&a);
    CHECK_EQ(a.n_scratch_alive, 0u);
    CHECK(a.scratch_top == a.scratch_base);
    CHECK(a.scratch_base == base_after_grow);  // reset does NOT realloc
    AtpFtCell *second_round_first = NULL;
    AtpFtCell *second_round_last  = NULL;
    for (u32 i = 0; i < N4; i++) {
      AtpFtCell *c = ft_alloc_scratch(&a);
      if (i == 0)          second_round_first = c;
      if (i == N4 - 1)     second_round_last  = c;
    }
    // No realloc happened: base + end identical to pre-reset values.
    CHECK(a.scratch_base == base_after_grow);
    CHECK(a.scratch_end  == end_after_grow);
    // Second round handed out the same pointer range at the same
    // offsets -- this is the "reset reuses memory" invariant.
    CHECK(second_round_first == base_after_grow);
    CHECK(second_round_last  == base_after_grow + (N4 - 1));
    ft_destroy(&a);
  }

  // ---- Test 5: scratch overflow grows the region -------------------
  TEST_BEGIN("ft_alloc/scratch-grows-on-overflow");
  {
    AtpFt a;
    ft_init(&a);
    // 256 KB / 24 B per cell ~= 10923 cells.  Bump until we cross
    // that, then verify the region grew (end - base > initial cap).
    u32 init_cells = ATPFT_SCRATCH_INIT_BYTES / sizeof(AtpFtCell);
    u32 N5 = init_cells * 3u + 17u;
    for (u32 i = 0; i < N5; i++) {
      AtpFtCell *c = ft_alloc_scratch(&a);
      CHECK(c != NULL);
    }
    u64 cur_cap = (u64)(a.scratch_end - a.scratch_base);
    CHECK(cur_cap >= (u64)N5);
    CHECK(cur_cap > (u64)init_cells);
    ft_destroy(&a);
  }

  // ---- Test 6: ft_destroy frees a non-empty arena without crash ----
  TEST_BEGIN("ft_alloc/destroy-nonempty");
  {
    AtpFt a;
    ft_init(&a);
    // Multi-block persistent + non-trivial scratch region.
    enum { N6 = 80000 };          // > 2 blocks worth (60000)
    for (u32 i = 0; i < N6; i++) (void)ft_alloc_persistent(&a);
    for (u32 i = 0; i < 5000; i++) (void)ft_alloc_scratch(&a);
    CHECK(a.n_blocks >= 2u);
    ft_destroy(&a);
    CHECK_EQ(a.n_blocks, 0u);
    CHECK(a.blocks == NULL);
    CHECK(a.scratch_base == NULL);
  }

  thvm_free();
  TEST_REPORT();
}
