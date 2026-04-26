# SupGen-style search inside saturation: design memo (stage 8.10a)

> Last design memo of the IC-native ATP arc.  Decision document
> for stage 8.10's choice of "what additional superposition adds
> non-overlapping value" given that the CP-priority queue
> already implements the SupGen pattern.

## Starting position

The IC-native ATP arc landed several SupGen-style mechanisms
along the way:

1. **CP-priority queue** (5.3): every queued CP `(lhs, rhs)`
   gets wrapped as `INC^k(CTR_label=i([lhs, rhs]))` where
   `k = symbol_count(lhs) + symbol_count(rhs)`.  All n_cps
   wrappings fold into a single binary SUP tree.
   `thvm_collapse_ordered` walks the tree and emits the leaves
   sorted by INC depth ascending -- the first emitted leaf is
   the cheapest CP.  This *is* SupGen-style search at the CP-
   selection level.
2. **SUP-encoded CP enumeration** (8.1): the per-pair
   unification work flows through APP-PRI evaluation so
   alternative CP candidates can be superposed and collapsed
   together.  The IC path delivered byte-identical outputs to
   the C path on the v0 corpus.
3. **--mix heuristic** (8.8): the priority weight gets a
   penalty for unorientable CPs, biasing the collapse toward
   cleaner candidates first.

So the question for 8.10 is genuinely: *what new superposition
adds non-overlapping value beyond what already ships?*

## Three candidate extensions

### Option A: superpose the unfailing-orient direction

Today, when KBO/LPO returns `KBO_UN`, `thvm_atp_orient_and_add`
atomically pushes BOTH `lhs -> rhs` and `rhs -> lhs` rules.
This doubles the rule count but is what unfailing completion
mandates for completeness.

A SupGen variant: superpose the two directions, let collapse
pick.  But **picking just one direction breaks completeness** --
unfailing's whole point is that we can't decide which direction
to keep, so we keep both.  Replacing the atomic 2-push with a
"choose one via SUP" would lose proofs.

**Verdict**: not useful.

### Option B: superpose the ordering choice (KBO vs LPO)

Run saturation under both orderings simultaneously, with
priority-aware collapse picking whichever path closes the goal
first.  Useful when KBO and LPO produce different rule
orientations.

But stage 8.5d empirically showed: **on our corpus, KBO and LPO
orient identically**.  The classical KB literature says they
agree when the precedence and weight functions are aligned.
For our v0 problems, B is redundant.  Could matter on other
TPTP-UEQ subdivisions where the orderings diverge, but that's
a follow-on corpus-expansion item, not a v0 deliverable.

**Verdict**: theoretically useful, empirically redundant on the
current corpus.  Defer to TPTP-UEQ corpus expansion.

### Option C: superpose heuristic mode (--add vs --mix)

Run with both heuristics and let collapse pick CPs by
`min(add_priority, mix_priority)`.

But `--mix` priority is `add_priority + penalty_if_uno`, so
`min(add, mix) = add` always.  Superposition gives no new
information.

**Verdict**: trivially redundant.

### Option D: superpose at the trace level (research-grade)

Build a search tree where each node is `(R_state, CP_queue,
goal_state)` and edges are saturation steps.  Superpose
alternative paths; let collapse pick the shortest closing
sequence.

This is the genuine SupGen vision -- and is multi-firing,
research-grade.  Requires backtracking machinery (our
saturator is greedy, single-trace), state-snapshotting, and
substantial new APIs.  Out of scope for 8.10.

**Verdict**: huge research undertaking; defer.

## Decision

**8.10b ships a small, demonstrative extension**: an exposed
"top-K" CP peek API that uses the existing INC-priority
collapse infrastructure but doesn't pop -- callers see the
cheapest K CPs without committing.

```c
// 8.10b: peek the top K cheapest CPs without popping.  Returns
// the actual count peeked (<= K, <= n_cps).  Out-params receive
// (lhs, rhs) pairs in priority order.  Same priority logic as
// thvm_atp_select_cp.
fn u32 thvm_atp_peek_top_k(AtpState *s, u32 k,
                           Term *out_lhs, Term *out_rhs);
```

**Why this**: it (a) exercises the SupGen mechanism in a
standalone, testable way; (b) hands a hook to future research
(branching CP selectors, multi-CP look-ahead heuristics);
(c) lands cleanly in one firing with bounded scope.

**Why NOT something larger**: A is unsound, B is empirically
redundant on the corpus, C is trivially redundant, D is
research-grade and multi-firing.  The substantive deliverable
of 8.10 is the arc-closing memo (8.10c), not 8.10b.

## Scope for 8.10b

- New helper `thvm_atp_peek_top_k` in `src/atp/_.c` (~40 LOC).
  Reuses the existing INC-wrap + SUP-fold + collapse_ordered
  pipeline from `thvm_atp_select_cp`; difference is no
  pop / shift.
- Tests: 3-4 cases in `tests/test_atp.c`.  Verify
  monotone-non-decreasing priority across the peeked sequence;
  verify pop after peek still works; verify K > n_cps clamps
  to n_cps.

## Scope for 8.10c

- New memo `docs/plans/atp_arc_summary.md` recapping the
  arc.  Stages 1-8.10, what shipped, what's deferred (8.6
  awaits HVM4 upstream; sort-aware KBO; multi-witness
  narrowing), and natural follow-ons (TPTP file parsing in WL,
  AC matching, full pure-IC KBO port, larger TPTP-UEQ corpus
  + Twee comparison expansion).

## Acknowledgement

The IC-native ATP arc started with a stage-1 design that
proposed SupGen-driven search as a long-term ambition.  Mid-arc
findings (the empirical results on the small fixtures, the
domination lemmas in connectedness / rule-subsumption) made
clear that SupGen at the trace level is more research than
engineering for this codebase today.  The CP-priority queue is
the SupGen mechanism that DID find a home; deeper integration
is left for follow-on work, when a use case (e.g., a TPTP
problem family that genuinely benefits from multi-trace
exploration) emerges.

## Verification

This is a documentation-only resolution.  No `make test` /
`make wl-test` impact; both remain green from the prior 8.9e
landing.
