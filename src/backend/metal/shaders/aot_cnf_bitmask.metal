// backend/metal/shaders/aot_cnf_bitmask.metal -- Lever 3.
//
// Direct-evaluation CNF SAT kernel.  Bypasses the IC reduction
// pipeline entirely: takes a CNF formula encoded as two parallel
// uint arrays (positive-literal bitmasks per clause + negative-
// literal bitmasks per clause) and a variable count V, dispatches
// with grid=2^V, and writes 1/0 per leaf indicating whether the
// assignment encoded in tid satisfies the formula.
//
// The IC SUP-tree path runs Church-bool reduction per leaf at
// ~500 IC interactions per thread.  This kernel does ~3 ops per
// clause.  At V=10 / C=20 the per-thread cost drops from ~500 IC
// fires (~12.5 us) to ~60 bitwise ops (~10 ns) -- 1000x in raw
// per-thread cost.  Total wall-clock for the bench's V=10 case
// should drop from 156 ms (Lever 1) to single-digit ms.
//
// Encoding:
//   bit i of `assignment` = value of variable i+1 (0 = false, 1 = true)
//   pos_mask[c] = bitmask of variables that appear POSITIVELY in clause c
//   neg_mask[c] = bitmask of variables that appear NEGATIVELY in clause c
//
// Per-clause satisfiability:
//   sat_c = ((assignment & pos_mask[c]) | ((~assignment) & neg_mask[c])) != 0
// This is "any positive literal whose var is true, OR any negative
// literal whose var is false" -- the standard CNF clause check.
//
// All-clauses AND: short-circuit after the first unsatisfied clause
// to skip remaining work on that thread (per-thread, not warp-wide).
//
// Limitation: V <= 32 because we use uint bitmasks.  V > 32 would
// need ulong masks (Apple GPU supports 64-bit bitwise; it's a
// straightforward extension when needed).

#include <metal_stdlib>
using namespace metal;

kernel void aot_cnf_bitmask(
    constant uint *clauses_pos    [[buffer(0)]],   // [n_clauses] positive masks
    constant uint *clauses_neg    [[buffer(1)]],   // [n_clauses] negative masks
    constant uint *n_clauses_buf  [[buffer(2)]],   // single-element constant
    device   uint *result         [[buffer(3)]],   // [2^n_vars] outputs (1/0)
    uint           tid            [[thread_position_in_grid]])
{
  uint assignment = tid;
  uint nc = *n_clauses_buf;
  uint all_sat = 1u;
  for (uint c = 0u; c < nc; c++) {
    uint pos = clauses_pos[c];
    uint neg = clauses_neg[c];
    uint sat_c = ((assignment & pos) | ((~assignment) & neg)) != 0u ? 1u : 0u;
    all_sat &= sat_c;
    if (all_sat == 0u) break;  // short-circuit: this thread's assignment fails
  }
  result[tid] = all_sat;
}
