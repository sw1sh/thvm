// Snapshot and reset of the per-context HotCounters (thvm.h), read from WL as
// THotCounters[] (Profile.wl).
//
// A new counter takes, all at one position, a HotCounters field, a HOT_* macro
// and a HOT_COUNTER_COUNT bump (thvm.h), a read below and a $THotCounterNames
// entry (Profile.wl): WL decodes the snapshot by position.

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
  out[13] = HOT_JIT_GRAPH_RUNS;
  out[14] = HOT_JIT_GRAPH_DISPATCHES;
}
