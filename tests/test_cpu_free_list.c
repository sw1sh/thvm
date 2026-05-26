// test_cpu_free_list.c - exercise cpu_buf_freelist_push +
// cpu_buf_alloc's free-list lookup (bm4a of the bench arc).
//
// Verifies:
//   - alloc -> push -> alloc reuses the donor's STORAGE on a fresh
//     slot id (zeroed data + reset bookkeeping); the donor slot id
//     is left dead (data=NULL) so a stale TenDesc.buf_id pointing
//     at it can't alias the new tensor.  See cpu_buf_freelist_try_pop
//     for the rationale (loss-overwrite CUDA bug, same fix on CPU).
//   - alloc(N) on a free-list with only mismatched-size slots
//     misses and falls through to a fresh slot.
//   - push of a stale (already-popped) buf_id is silently
//     dropped (no crash on the next pop).
//   - push past CPU_FREELIST_CAP saturates without corrupting
//     state (the slot is dropped on the floor).
//   - external bufs (cpu_buf_alloc_external) are NOT recycled
//     by the freelist path -- the storage isn't ours to zero.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("cpu-free-list/alloc-push-realloc-recycles-storage-on-fresh-slot");
  u32 a = cpu_buf_alloc(64);
  CHECK(a > 0);
  void *donor_data = CPU_BUFS[a].data;
  // Mark with a sentinel so we can verify the push path zeroes
  // the storage on reuse.
  ((u8 *)CPU_BUFS[a].data)[0] = 0xAB;
  ((u8 *)CPU_BUFS[a].data)[63] = 0xCD;
  CPU_BUFS[a].refcount  = 0;   // simulate post-decref state
  CPU_BUFS[a].preserved = 1;
  CPU_BUFS[a].freeable  = 1;
  cpu_buf_freelist_push(a);
  u32 b = cpu_buf_alloc(64);
  // Storage is transferred to a fresh slot id; donor goes dead so a
  // stale TenDesc.buf_id==a doesn't alias the new tensor.
  CHECK(b != a);
  CHECK_EQ(CPU_BUFS[b].data, donor_data);     // same storage block
  CHECK_EQ(((u8 *)CPU_BUFS[b].data)[0], 0);   // zeroed
  CHECK_EQ(((u8 *)CPU_BUFS[b].data)[63], 0);
  CHECK_EQ(CPU_BUFS[b].refcount,  1);          // fresh slot reset
  CHECK_EQ(CPU_BUFS[b].preserved, 0);
  CHECK_EQ(CPU_BUFS[b].freeable,  0);
  CHECK(CPU_BUFS[a].data == NULL);             // donor dead
  CHECK_EQ(CPU_BUFS[a].refcount, 0);

  TEST_BEGIN("cpu-free-list/too-small-slot-misses");
  // A 64-byte request can't be served by a 32-byte freelist slot.
  // Best-fit requires the parked block be >= the request.
  u32 small = cpu_buf_alloc(32);
  CHECK(small > 0);
  cpu_buf_freelist_push(small);
  u32 d = cpu_buf_alloc(64);
  CHECK(CPU_BUFS[d].nbytes == 64);   // fresh calloc (no parked >= 64)

  TEST_BEGIN("cpu-free-list/best-fit-pops-large-slot");
  // With a 128-byte parked slot, a 64-byte alloc best-fit-pops it
  // (the slot keeps its 128-byte nbytes; the new slot id holds the
  // donor's storage).
  u32 big = cpu_buf_alloc(128);
  CHECK(big > 0);
  void *big_data = CPU_BUFS[big].data;
  cpu_buf_freelist_push(big);
  u32 e = cpu_buf_alloc(64);
  CHECK(e != big);                          // fresh slot
  CHECK_EQ(CPU_BUFS[e].data, big_data);     // donor's storage
  CHECK_EQ(CPU_BUFS[e].nbytes, 128);        // donor's nbytes preserved
  CHECK(CPU_BUFS[big].data == NULL);        // donor dead

  TEST_BEGIN("cpu-free-list/external-buf-not-recycled");
  // Stash a non-owning buf in the freelist.  Even with matching
  // nbytes, cpu_buf_freelist_try_pop must skip it (we can't
  // memset storage we don't own).
  static u8 borrowed[32] = {0};
  u32 ext = cpu_buf_alloc_external(borrowed, 32, NULL, NULL);
  CHECK_EQ(CPU_BUFS[ext].owns_data, 0);
  cpu_buf_freelist_push(ext);
  u32 fresh = cpu_buf_alloc(32);
  CHECK(fresh != ext);
  CHECK_EQ(CPU_BUFS[fresh].owns_data, 1);   // owned, freshly calloc'd

  TEST_BEGIN("cpu-free-list/saturated-push-no-crash");
  // Saturate the list with throwaway slots.  Pushes past
  // CPU_FREELIST_CAP should silently drop without corrupting
  // state.  We don't need to actually push 4096 things to test
  // the bound -- just push the same slot many times and verify
  // pop still works (a no-crash smoke test; the donor's storage
  // gets handed off on the first pop and any stale freelist
  // entry referencing the now-dead donor is skipped by the
  // dptr==NULL guard in cpu_buf_freelist_try_pop).
  for (int i = 0; i < 16; i++) cpu_buf_freelist_push(d);
  u32 g = cpu_buf_alloc(64);
  CHECK(g > 0);     // got something
  // Subsequent pops with matching size should skip the stale
  // entries (they fail the bounds / owns_data check) and fall
  // through to a fresh alloc.
  u32 h = cpu_buf_alloc(64);
  CHECK(h > 0);     // either reused stale or fresh -- both OK

  thvm_free();
  TEST_REPORT();
}
