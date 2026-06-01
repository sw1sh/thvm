# AtpFt + LPO/Selection Plan (revised after Stage 7 profile)

ATP is its own venture, separate from heap-cell `Term` used by IC / UOP / WL.
Stages 1-7 stood up a native flatterm representation (`AtpFtCell`) modeled
on Waldmeister's `TermzellenT` with its own arenas, operations, and full
match/splice/normalize/discrim-descent/CP-queue pipeline -- all gated by
env flags, all live-verified at `test_atp 135624/135624` under
`THVM_ATPFT_NORM_VERIFY=1` (live differential against the Term path).

**The original plan assumed push-norm us/cp was the dominant cost.**
Stages 6/6b/7 landed sound at parity, confirming the hypothesis was wrong.
A profile workflow (4 parallel agents: `sample`-based hot-function
attribution, phase decomposition, trajectory shape, wmcli A/B) gave the
revised picture below.

## True bottleneck ranking (post-Stage-7 profile)

| Rank | Bottleneck                                       | % wall  | Plan-as-written? |
|------|--------------------------------------------------|---------|------------------|
| 1    | LPO recursion (30.5% self / 66.6% incl)          | 30-45%  | **Not targeted** |
| 2    | Dual-path legacy `atp_dt_*` + `acp_unpack_term`  | 25-30%  | Stages 8-9       |
| 3    | CP-selection quality (35% unorientable rules)    | ~20%    | **Not targeted** |
| 4    | push-norm full-R normalize (linear in `\|R\|`)   | ~15%    | Stage 8 NORM     |
| 5    | KBO/CPQ auxiliary                                | ~5%     | Already landed   |

`sample`-based top-10 self-time confirms KBO group at 30.5% self
(`kbo_lin_addto` 9.0%, `kbo_memo_combine` 7.3%, `kbo_subtree_memo` 6.1%,
`kbo_vortest` 4.8%), legacy DT at 13.7% self, byte-Term unpack at
13.0% self.

## Trajectory data

| wall  | steps | rules | unorientable | rules/sec |
|-------|-------|-------|--------------|-----------|
| 20s   | 4003  | 397   | 84 (21%)     | 20        |
| 60s   | 7047  | 516   | (growing)    | 3.0       |
| 180s  | 13080 | 640   | 224 (35%)    | 1.0       |

Rule-acquisition rate decays 20x over 9x wall. wmcli sustains
~162 rules/sec (1673 rules in 10.2s).  **Rate decay is the trajectory
problem**, not per-CP cost.

## Revised Stages 8-10

### Stage 8 (revised): LPO orientability cache + shape-hash memo key

Replace the planned "precedence + weight/priority AtpFt" port.

Implement:
- Per-(lhs, rhs) orientability triple-cache {LR, RL, UNORIENTABLE}
  keyed by `(term_shape_hash, term_shape_hash)`.
- Re-key `kbo_subtree_memo` / `kbo_lin_addto` memos from term-pointer
  to shape-hash so the 35% hit rate lifts toward 70%+.
- Short-circuit `kbo_vortest` on cache hit before any recursion.

Expected wall impact: **25-35% reduction** -- LPO sits inside both
unorient-step (50% wall) and push-norm (42% wall).

Risk: LOW.  KBO precedence/weights are run-invariant so invalidation
is trivial; reuses existing shape-hash infra from FT-RI.

### Stage 9 (revised): Retire legacy byte-Term mirror

Keep the original Stage 8/9 intent reframed as code-hygiene cleanup.

Implement:
- Delete `atp_dt_descend_rec/child/insert_term` + the byte-queue
  `atp_rewrite_normalize` path.
- Drop `acp_unpack_term` from the hot path.
- Make FT-RI authoritative for indexing/unification.

Expected wall impact: **20-25% reduction** (directly removes 22.7% self
+ GC pressure relief).

Risk: MED.  Touch is wide -- every push-norm/cp-gen call site -- but
the FT path is already proven equivalent (Stage 7 sound parity).
Mostly mechanical deletion.

### Stage 10 (revised, scoped down): CP-selection bias against unorientable

Add an LPO-orientability check at CP-selection time (free if Stage 8's
cache is in place).  De-prioritize or hard-defer CPs whose normalized
form yields an unorientable rule.

Expected wall impact: **40-60% reduction** to reach the same rule count,
BUT risk that we just defer work rather than eliminate it.

Risk: HIGH.  Easy to mis-tune and starve the search.  A/B harness
with wmcli's rule-trajectory as ground truth (wmcli IS runnable on
this box via `DYLD_FRAMEWORK_PATH="/Applications/Wolfram 15.1.app/Contents/Frameworks"`).

## What is OUT OF SCOPE

- No further AtpFt porting beyond what Stage 9 cleanup requires.
  The 5000+ LOC foundation is sound and complete.
- Precedence / weight-priority queues stay on the legacy representation
  (not on the critical path).
- Sunk-cost porting beyond profile-driven justification is cancelled.

## Stage 1-7 status (all committed, all green)

| Stage | Commit   | Acceptance                                            |
|-------|----------|-------------------------------------------------------|
| 1     | 85565a95 | test_ft_alloc 32811/32811                              |
| 2     | c86e288a | test_ft 34/34 (1000-term round-trip + hash agreement)  |
| 3     | 88c2423b | test_ft_order 100k/100k differential                   |
| 4     | 9e72c6fd | test_atp_ft 135624/135624 (bit-identical bench)        |
| 5     | 53ebd9e8 | test_ft_match 100k/100k differential                   |
| 6     | f1259b1a | test_atp_ft_norm 135624/135624 + live VERIFY           |
| 6b    | ca132d6d | test_atp_ft_ri 135624/135624                           |
| 7     | 03bd2429 | test_atp_ft_cpq 135624/135624 + ALL FLAGS + VERIFY     |

Every flag combination including
`THVM_ATPFT_CPQ=1 THVM_ATPFT_RI=1 THVM_ATPFT_NORM=1 THVM_ATPFT_NORM_VERIFY=1`
produces 135624/135624.

## wmcli reference

| problem      | wall   | rules | CPs       | result        |
|--------------|--------|-------|-----------|---------------|
| andassoc.pr  | 10.22s | 1673  | 4,574,980 | Goal proved.  |
| wolfram.pr   | 3.93s  | 600   | 631,573   | Goal proved.  |

Wrapper: `DYLD_FRAMEWORK_PATH="/Applications/Wolfram 15.1.app/Contents/Frameworks" wmcli <file.pr>`.
