# AC-completion soundness: one bug fixed, one residual (2026-07-18)

Context: CASC J13 soundness testing (Geoff Sutcliffe) reported FindProof
refuting Satisfiable problems.  c68b1bd7/f2e17fcc fixed three general bugs
and additionally REMOVED the `full-ac` slice from the competition schedule
(tools/thvm_atp_cli.c, commented out) because AC-completion itself
(`--full-ac` / `THVM_ATP_FULL_AC`, src/atp/ac.c + the `THVM_ATP_AC` paths in
src/atp/_.c and src/atp/ft_ac_match.c) refutes two of the six reported
problems (LAT016-1, RNG030-6). This doc is the follow-up: diagnose, fix what
can be fixed soundly, and record what's still open.

## Fixed: bare-variable multiplicity silently dropped in AC-flat matching

**Symptom**: `THVM_ATP_FULL_AC=1 ./bin/FindProof --wall=15 --szs LAT016-1.p`
reported `Unsatisfiable` for a Satisfiable ortholattice problem (McCune's
"E1 fails for ortholattices" -- LAT016-1 exists specifically to witness that
E1 is NOT a theorem, so any refutation is wrong by construction).

**Root cause** (`atp_match_ac_flat`, src/atp/ac.c; mirrored in
`ft_match_ac_flat`, src/atp/ft_ac_match.c): when an AC pattern like
`join(x0, join(x1, complement(x1)))` gets fully flattened (join is AC, so
the nested `join(x1,complement(x1))` flattens INTO the outer multiset), the
pattern yields 3 conceptual leaves: `{x0, x1, complement(x1)}`.  Pass 1
processes leaves left-to-right in ONE sweep.  When `x1`'s bare-leaf
occurrence (index 1) is scanned BEFORE the `complement(x1)` CTR-leaf
(index 2) that recursively binds `x1` as a side effect, the bare occurrence
gets deferred ("unbound, continue") and is never revisited once `x1`
actually becomes bound two leaves later.  The existing "already bound"
cleanup (added earlier to fix a DIFFERENT double-binding bug, see the old
comment block at ac.c:426-451) simply REMOVED such a var from the
leftover-absorption candidate list -- it never went back and consumed a
matching subject leaf for the bare occurrence it had deferred.  The result:
a single-var multiset SLOT vanishes from the accounting, and a LATER
genuinely-fresh var's pass-2 "absorb everything left over" clause silently
swallows a subject leaf that rightfully belonged to the dropped occurrence.

Concretely, on LAT016-1: `join(x0, join(x1,complement(x1)))` was accepted
against the 4-leaf goal subterm `{complement(X), L1, L2, L3}` (X, L1..L3
unrelated compound terms) with `x1:=X` (matched via the CTR leaf) and
`x0:=join(L1,join(L2,L3))` (leftover chain) -- a match that requires the
subject to ALSO contain a bare leaf equal to `X`, which it does not.  The
resulting "rewrite" produced `join(X,complement(X))` from a subterm that
was never AC-equal to that expression, and three such steps collapsed the
goal to `n1 != n1`, an unsound refutation of a Satisfiable problem. This is
precisely the shape of E1 (the distributive collapse `(u meet Y) join (u meet Y') = u`)
that LAT016-1 exists to show does NOT hold in ortholattices.

**Fix** (src/atp/ac.c `atp_match_ac_flat`, src/atp/ft_ac_match.c
`ft_match_ac_flat`): when the "re-scan ub_vid for already-bound vars" step
finds a var bound (by a sibling CTR-leaf recursion during the same pass),
it now CONSUMES `ub_mult[k]` more unused subject leaves equal to the
binding (the same `kbo_eq`/`ft_eq` check the "already bound" branch of
pass 1 runs) before dropping the var from the leftover-absorption
candidates, and FAILS the whole match (returns 0, sound) if not enough
matching leaves remain.  `leftover`/`left_buf` are now computed AFTER this
consumption so pass 2 never double-counts a leaf the fix already claimed.

Verified:
- Hand-derivation of the LAT016-1 trace (see below) predicted the bug
  exactly; the fix makes the SAME derivation fail cleanly (`return 0`).
- Standalone unit repro (`atp_match_ac` called directly, mirroring
  tests/atp/test_ac.c's harness) on the isolated pattern
  `add(add(y,inv(y)),z)` vs a 4-leaf `{x0,x0,inv(x0),inv(x0)}` subject
  confirms the fix accepts the legitimately-decomposable case (leaves
  a real leftover match) and rejects the fabricated one.
- `LAT016-1` now `GaveUp` (times out at wall cap, never refutes) under
  `THVM_ATP_FULL_AC=1` -- was falsely `Unsatisfiable` before the fix.
- The default (non-AC) engine trajectory is untouched: this code only
  runs when `thvm_atp_get_ac_mask() != 0`, which requires an explicit
  `--full-ac` / auto-AC detection; `test_atp` (136264/136264, both default
  and the `THVM_ATP_LAZY_RULES=0 THVM_ATP_SW_FT=0 THVM_ATP_TRACE_LEAN_RULES=0`
  kill-switch combo) and a direct FindProof replay of the Sheffer
  `AndAssociativity` trajectory (`tools/baselines/wm_pr/ShefferAxioms__AndAssociativity.pr`,
  non-AC) reproduce the exact rules=482 / cps=779686 / cp-size histogram
  already recorded by `test_atp_session_reuse` (5/5) -- byte-identical.

### Diagnostic tooling added alongside the fix

`src/atp/ft_norm.c`'s `ft_mixmost_reduce_here` (the FT-mixmost normalizer
both goal-check and proof-extraction route through) gained two opt-in,
zero-default-cost debug traces used to pin this bug and its residual
sibling below:
- `THVM_ATP_GOAL_NF_DEBUG=1` now also prints `[mixstep-term]` (the exact
  before/after Term at every proof-extraction splice, not just
  position/rule-uid) -- this is what surfaced the LAT016-1 collapse.
- `THVM_ATP_MIXSTEP_ALL=1` prints `[mixstep-all]` (rule/position/full
  before-after Terms) for EVERY splice this call makes, including
  interreduction / CP push-normalize -- the goal-check-scoped
  `[mixstep]`/`[mixstep-term]` never fire there.  Needed for the RNG030-6
  investigation below since the false rule forms during rule maintenance,
  not goal-checking.

Both are pure `getenv`-gated `fprintf(stderr, ...)` additions; no behavior
change when unset.

## OPEN: a second, distinct unsound derivation survives the fix (RNG030-6 family)

`THVM_ATP_FULL_AC=1 ./bin/FindProof --wall=15 --szs RNG030-6.p` (right-
alternative ring, Satisfiable -- "2*assr(X,X,Y)^3 = additive identity" is NOT a
theorem) STILL reports `Unsatisfiable` after the fix above.  Same family:
**RNG031-6** and **RNG032-6** (siblings, same axiom set, also Satisfiable
per TPTP) ALSO falsely refute.

Traced (via `THVM_ATP_MIXSTEP_ALL=1` + `THVM_ATP_CPGEN_DEBUG=1`, bisected
with `--max-steps` since the real run derives 500+ rules) to a concrete bad
rule: `RULEADD ... uid=338 lhs=(C8 (C5 V0) V1) rhs=C4`, i.e.
`multiply(additive_inverse(x0), x1) -> additive_identity` -- an
unconditional collapse that is FALSE in general (it would force the whole
ring's multiplication to be trivial) and downstream produces the
even-more-obviously-wrong `multiply(x0,x1) -> additive_identity` a few
steps later (`uid=339`, `RULEADD slot=305 uid=339 lhs=(C8 V0 V1) rhs=C4`).

The raw critical pair that normalizes into `uid=338` is:
```
multiply(additive_inverse(Z), x1) = add(additive_identity, multiply(Z, x1))
  where Z = add(add(x0,x0), add(additive_inverse(x0),additive_inverse(x0)))
```
`Z` is a TRUE zero (`x0+x0-x0-x0 = 0`), so the raw CP is a sound instance
that SHOULD normalize to `additive_identity = additive_identity` (both
sides correctly reducing `Z` to `0`) and be dropped as trivially joinable.
Instead it normalizes to `additive_identity = multiply(additive_inverse(x0),
x1)` -- i.e. one occurrence of `Z` (inside `additive_inverse(Z)`) reduces
correctly toward `0`, while the OTHER occurrence (inside `multiply(Z,x1)`,
reached through the ring's distributivity + associator rules) reduces to
bare `x0` instead of `additive_identity`.

A standalone unit repro (mirroring the LAT016-1 one) confirms the ISOLATED
step `add(add(y,inv(y)),z) -> z` applied directly to `Z` behaves CORRECTLY
under the fixed matcher (`y:=x0`, `z:=add(x0,additive_inverse(x0))`, a
sound one-step reduction, not yet fully `0` but on the right track) --
matching pattern `add(add(y,inv(y)),z)` against `Z`'s 4-leaf flatten
`{x0,x0,inv(x0),inv(x0)}` legitimately succeeds because a genuine second
`x0` leaf exists to satisfy `y`'s bare-occurrence requirement (unlike the
LAT016-1 case, where no such leaf existed and the fixed matcher correctly
fails).  A minimal pure-`add`-fragment reproducer
(commutativity/associativity/identity/inverse only, no multiply) also does
NOT derive `Z = additive_identity` within the AC-completion's current
(deliberately incomplete -- see ac.c:333-341 "partition enumeration
deferred") coverage, so the wrong `Z -> x0` observed in the full run is not
coming from the isolated 2-var extended-rule path already fixed above.

**Not yet isolated**: which specific match, among the ~300+ rules alive at
that point in the RNG030-6 derivation (distributivity-derived rules
interacting with the associator/commutator definitions), performs the
`Z -> x0` step.  The mechanism is very likely the SAME general class (an
AC bare-leaf's multiset-slot requirement satisfied by construction rather
than by an actual matching subject leaf) but manifesting through a
different derived rule with more simultaneously-unbound variables --
`atp_match_ac_flat`'s `n_ub >= 2` "greedy pair-up" branch
(ac.c:495-527 / ft_ac_match.c:333-362) is the next audit target: unlike the
`n_ub == 1` path this fix hardened, the pair-up loop's "first unused
leftover leaf becomes each var's candidate, in declaration order" is
GREEDY and untested against a case where an earlier var's greedy pick
should have gone to a later var -- it may accept an assignment that is
merely leaf-count-consistent without being the UNIQUE decomposition the
rewrite's soundness depends on when the pattern's own vars interact via
further nested AC structure.  A repro harness (in the style of
`repro_rng030.c` used during this investigation -- not checked in, lived
under the session scratchpad) that builds the exact rule set active near
`uid=337`/`uid=338` (RULEADD slot=316 uid=337 `add(multiply(additive_inverse(x0),
add(x1,x1)), multiply(x0,x1)) -> multiply(additive_inverse(x0),x1)` plus
whatever occupied "slot 295" at CP-formation time, `multiply(additive_inverse(Z),x1)
-> additive_identity`) and single-steps the CP-formation normalize with
`THVM_ATP_MIXSTEP_ALL=1` is the fastest path to close this out.

## Verdict: full-ac slice stays DISABLED

Per the decision rule this arc was scoped under: re-enable only if
soundness + flips + gates ALL pass.  Soundness does not (RNG030-6/031-6/032-6
still falsely refute), so `tools/thvm_atp_cli.c`'s commented-out
`{"full-ac", "--full-ac=2 --fof-sat=0", 0.0}` schedule row stays commented
out.  The bare-variable-multiplicity fix is landed anyway: it is a genuine,
narrowly-scoped correctness improvement (strictly REJECTS a class of
previously-accepted unsound matches; never accepts a new one), validated
against the full non-AC regression suite, and closes exactly one of the two
originally-reported false refutations (LAT016-1) with no observed
regression on the RNG021/025/027/028/029-family flips (all five still
`Unsatisfiable` under `--full-ac=2 --wall=30`, i.e. still solved as genuine
theorems by whatever OTHER derivation path they use -- none of them
exercised the bug this fix closes).

## Soundness sweep results (2026-07-18, THVM_ATP_FULL_AC=1 forced, --wall=15)

The 6 CASC-J13-reported problems (tests/atp/soundness/*.p):

| Problem   | Verdict  |
|-----------|----------|
| BOO036-1  | GaveUp   |
| COL069-1  | GaveUp   |
| LAT016-1  | GaveUp (was Unsatisfiable -- FIXED)  |
| LAT060-1  | GaveUp   |
| RNG030-6  | **Unsatisfiable -- STILL UNSOUND (open)** |
| RNG042-2  | GaveUp   |

Additional Satisfiable AC-bearing UEQ problems fetched from tptp.org
(SeeTPTP cgi) and tested the same way:

| Problem   | Domain | Verdict |
|-----------|--------|---------|
| RNG031-6  | Ring (right-alternative, sibling of RNG030-6) | **Unsatisfiable -- UNSOUND (open, same family)** |
| RNG032-6  | Ring (right-alternative, sibling of RNG030-6) | **Unsatisfiable -- UNSOUND (open, same family)** |

LAT017-1 through LAT020-1 (same ortholattice/quasilattice axiom families as
LAT016-1) and ROB020-1/ROB023-1 (Robbins algebra) were also fetched but are
all `Unsatisfiable`/`Unknown` status per TPTP, not Satisfiable, so they are
not soundness probes.  The sweep was stopped at 8 Satisfiable AC problems
(vs. the ~15 targeted) once the RNG030-6 family's residual unsoundness was
independently reconfirmed twice (RNG031-6, RNG032-6): the schedule-slice
decision was already determined (stays disabled), and further problems in
the family were expected -- and confirmed -- to hit the same open bug
rather than surface new information.

## Flip check (2026-07-18, --full-ac=2 --wall=30)

RNG021-6, RNG025-5, RNG027-7, RNG028-5, RNG029-5 (the genuine-theorem flips
the removed schedule slice used to carry) all still report `Unsatisfiable`
after the fix -- the bug closed here was not on their derivation path.

## Reproducers

```
export TPTP=~/.cache/thvm/casc   # has Axioms/ at top level
THVM_ATP_FULL_AC=1 ./bin/FindProof --wall=15 --szs tests/atp/soundness/RNG030-6.p
# still: SZS status Unsatisfiable  (WRONG -- Satisfiable per TPTP)

THVM_ATP_FULL_AC=1 ./bin/FindProof --wall=15 --szs tests/atp/soundness/LAT016-1.p
# now: no SZS status printed / GaveUp on wall cap (correct -- was Unsatisfiable before the fix)
```
