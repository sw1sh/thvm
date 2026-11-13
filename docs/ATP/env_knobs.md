# ATP environment knobs

The ATP (automated theorem proving) rows of the runtime knob index, split
out of [../NN/env_knobs.md](../NN/env_knobs.md), whose conventions apply
here.

The ATP engine + `bin/FindProof` CLI read well over 200 `THVM_ATP*` /
`THVM_FOL*` / `THVM_PROOFCHECK*` variables. Every STRATEGY knob is
discoverable via `bin/FindProof --list-knobs` (a 183-row
`--flag -> env -> Method-name` table -- each CLI flag is exactly an env
setter); the engineering-level subset is documented in
`docs/ATP/engineering.md`. The rows below are the load-bearing
operational ones.

| Knob | Default | Effect |
| --- | --- | --- |
| `THVM_ATP_RSS_ABORT_MB` | 8192 (schedule slices: 4500) | Live-RSS GROWTH abort cap (MB, `0` disables): bounds the run's own RSS growth over the baseline sampled at its first step-0 poll, and the run degrades to a clean SZS `ResourceOut` instead of flooding the host. Growth-relative (not absolute) so a large-ambient host process -- WolframKernel after a cold-JIT `TNetTrain` sits at 8-9 GB for a while -- does not insta-abort every following proof (the silent post-train `$Failed` wedge). |
| `THVM_ATP_SCHED_MAX_PAR` | 5 | `--schedule` concurrency width (clamped to the slice count). The StarExec run script pins it to the core count. |
| `THVM_ATP_VERBOSE` | off | Restore engine progress chatter on the quiet competition path (same as `--verbose`). |
| `THVM_ATP_PROFILE` | off | `1`: per-phase wall-time breakdown after each run; `2`: also the detail counters (push-normalize parts, discrimination-tree and memo counts). Printed by `thvm_atp_profile_print` (`src/atp/profile.c`) from the bench, `bin/FindProof` and THVMLink's run_proof, so `bin/atp_replay` and `TFindProof` print it too. |
| `THVM_ATP_DIAG` | off | Diagnostics level. |
| `THVM_ATP_TRACE_MAX` | (impl) | ATP proof-trace capacity. |
| `THVM_ATP_RULE_TRACE` | off | Print each rule as it is added (trajectory A/B debugging). |
| `THVM_ATP_GC_MB` | (impl) | Lower the ATP heap-GC trigger (MB) to collect more often. |
| `THVM_ATP_HEAP_ABORT_FRAC` | (impl) | Semi-space fraction past which the saturation aborts cleanly. |
| `THVM_ATP_WALDMEISTER` | off | Bench shorthand for the Waldmeister-preset runtime knobs (SelectionRatio 51, RHSInterreduce, UnfailingCP, lazy normalize, ...). |
| `THVM_ATP_SINE` / `_SINE_TOL` / `_SINE_GT` / `_SINE_RC` | on / (impl) | SInE axiom selection on oversized knowledge bases; `THVM_ATP_SINE=0` disables. |
| `THVM_ATP_ENIGMA_EXPORT` | unset | Path to a TSV: record per-CP features and, on PROVED, append proof-DAG-labelled rows (the ENIGMA training corpus). |
| `THVM_ATP_CP_DATASET` | unset | When set to a filename, capture the ENIGMA CP dataset (WL bridge). |
| `THVM_FOL_AGE_RATIO` | 5 | FOF clausal pre-pass pick-given interleave (1 pure FIFO, 0 pure best-first). |
| `THVM_FOL_GOAL_TIER` | on | FOF pre-pass conjecture-ancestry selection tier; `0` drops it. |
| `THVM_FOL_INDEX` | on | FOF pre-pass literal indexing (partner lists + subsumption buckets); `0` restores the full scans (byte-identical trajectory). |
| `THVM_FOL_AVATAR` | pre-pass on | AVATAR-lite variable-disjoint component splitting in the FOF clausal pre-pass; `0` restores the unsplit calculus exactly, `2` scopes guard spend to input + conjecture-descended clauses. Engine-level states created outside the pre-pass default off. |
| `THVM_PROOFCHECK_WALL` / `_CAP` / `_DEEP` | (impl) | Proof-checker (`--check` / CheckProof) wall, per-step budget cap, and 10x deep-validation mode. |
| `THVM_ATP_GOAL_DRIVE` | off | Refutational goal driver (`--goal-drive`): admit the ground negated goal as an ACTIVE backward condensed-detachment participant, regressing it through the raw ifeq-guarded equations interleaved with completion (`atp_goal_drive_check` in `src/atp/narrow.c`, run from the goal check). Opt-in; a no-op unless the problem carries a detected `ifeq(A,A,B,C)=B` dispatcher. Sound (every closure is a ground instance of R union the dispatcher rules) but does not yet crack the deep '-10' CD gates -- the residual is goal-directed SELECTION quality. |
| `THVM_ATP_GOAL_DRIVE_STRIDE` / `_NODES` / `_DEPTH` / `_CELLS` / `_SIZE` | 100 / 15000 / 12 / 75%-of-heap / 6x-goal | Goal-driver budgets: rule-growth interleave cadence, per-run node cap, iterative-deepening depth cap, bump-heap cell cap, subgoal size ceiling. `THVM_ATP_GOAL_DRIVE_DEBUG` traces per-run variant/fact counts + the decomposed dispatcher rules. |

