// instrument/hot_counters.c -- snapshot + reset helpers for the
// per-context HotCounters block (see thvm.h).  The counters
// themselves are TContext fields; the HOT_* macros from thvm.h
// expand to lvalues so consumers `HOT_*++` directly.
//
// WL-side surface lives in `wl/THVMLink/Kernel/Profile.wl`:
//   - THotCounters[]                  -- snapshot Association.
//   - THotCountersReset[]             -- zero them.
//   - THotCountersDelta[label, body]  -- reset, eval `body`, return
//                                        the snapshot as the delta.
//
// When you add a counter:
//   1. Append a field to `HotCounters` in thvm.h.
//   2. Add a `HOT_*` macro alongside the others in thvm.h.
//   3. Bump HOT_COUNTER_COUNT in thvm.h.
//   4. Append the read in `hot_counters_snapshot` below.
//   5. Append the same name to `$hotCounterNames` in Profile.wl.
// Order must match -- the WL side decodes the {Integer, 1} payload by
// position.

fn void hot_counters_reset(void) {
  memset(&CURRENT_CTX->hot, 0, sizeof(HotCounters));
}

fn void hot_counters_snapshot(u64 *out) {
  out[0] = HOT_HEAP_REPLACE_CALLS;
  out[1] = HOT_HEAP_REPLACE_CELLS;
  out[2] = HOT_IS_REDEX_CALLS;
  out[3] = HOT_REDEX_ENUM_CALLS;
  out[4] = HOT_REDEX_ENUM_CELLS;
  out[5] = HOT_WNF_CALLS;
  out[6] = HOT_REALIZE_CALLS;
  out[7] = HOT_MATERIALIZE_CALLS;
  out[8] = HOT_KERNEL_FIRES;
  out[9] = HOT_GRAD_FIRES;
  out[10] = HOT_JIT_REPLAY_CALLS;
  out[11] = HOT_JIT_REPLAY_DISPATCHES;
  out[12] = HOT_JIT_REPLAY_ASSIGNS;
}
