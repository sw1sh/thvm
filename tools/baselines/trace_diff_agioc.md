# AbelianGroup/InverseOfComposite -- thvm vs WM CLI trajectory diff

Captured 2026-06-07 via:

  # thvm:
  env THVM_ATP_CP_PICK_TRACE=1 THVM_ATP_WALDMEISTER=1 \
      bin/test_atp_wolfram_bench agioc 99999999 8 2>&1 | grep CPSEL

  # WM CLI:
  DYLD_FRAMEWORK_PATH="/Applications/Wolfram 15.0.app/Contents/Frameworks" \
      /Users/swish/src/wolfram/waldmeister/wmcli \
      tools/baselines/wm_pr/AbelianGroupAxioms__InverseOfComposite.pr

## Rule-derivation order

| Rank | WM CLI rule                              | thvm rule (pick→rules counter)         |
| ---- | ---------------------------------------- | --------------------------------------- |
| 1    | `and(x1, e) -> x1`             (id-rt)   | `and(x, e) -> x`              (id-rt)   |
| 2    | `(xy)z -> x(yz)`               (assoc)   | `and(e, x) -> x`              (id-lt)   |
| 3    | `and(x, not(x)) -> e`          (inv-rt)  | `and(x, not(x)) -> e`         (inv-rt)  |
| 4    | `and(e, x) -> x`               (id-lt)   | `not(e) -> e`                 (inv-e)   |
| 5    | `not(e) -> e`                  (inv-e)   | `(xy)z -> x(yz)`              (assoc)   |
| 6    | `and(x, and(not(x), y)) -> y`            | (similar derived rules, different order)|
| 7    | `not(not(x)) -> x`             (doubleN) |                                         |
| ...  |                                          |                                         |
| 12   | `and(not(x), not(y)) -> not(and(y, x))`  | (analogous: thvm's CPL8)                |

## First algorithmic divergence

**ASSOCIATIVITY priority**:
- WM picks assoc as rule #2.
- thvm picks assoc as rule #5.

thvm's `MixWeight` (the engine default) assigns:
- comm  `xy = yx`             pri 3
- id-rt `xe = x`              pri 3
- id-lt `ex = x`              pri 3 (derived after id-rt)
- inv-rt `x*inv(x) = e`       pri 4
- inv-e `inv(e) = e`          pri 2
- assoc `(xy)z = x(yz)`       pri 5  <-- HIGH

(Lower pri = picked first in min-heap.  thvm puts assoc LAST among
the axiom-class CPs.)

WM's `Mix` weight pre-ranks assoc much lower so it orients SECOND.
Likely WM weights "lhs unifies trivially" CPs lower, or its Mix
formula penalizes large RHS less than thvm's.

## Hypothesis to test

The `pri` value comes from `atp_cp_priority_sized` which calls
`atp_cp_priority` -> mode-specific calc.  For `ATP_CP_WEIGHT_MIX` the
weight is some `f(|lhs|, |rhs|, ordering rank)`.  Inspect:

  src/atp/_.c    atp_cp_priority_mix (or equivalent)

vs WM source:

  sources/CLP/ClasHeuristics.c CH_MixWeight

Diff the calc.  The first formula difference is the algorithmic
mismatch.  Fixing it should move assoc to rank #2 in thvm's
trajectory, and the cascading derived-rule order should align.

## Endgame parity

After matching the rule-derivation order, the CP-set IR sweep timing
+ tiebreak (probably `FifoTiebreak`) determine the per-step trajectory.
WM's final goal-closure has 4 entries (77-80); thvm has 1
(`{Conclusion,1}` with `Source -> "trivial"` post c0354f0c).  The
+2 gap in parity_wm_wmcli.tsv stems from the trajectory difference,
not just the trivial-Conclusion split.

## Phase 4 findings

**The Mix formula is already byte-identical**: thvm's
`atp_cp_priority_mix` (`src/atp/_.c:5330`) is exactly WM's
`CH_MixWeight` (`sources/CLAS/ClasHeuristics.c:131`) -- both:

  res = (lhs>rhs) ? w_lhs : (lhs<rhs) ? w_rhs : w_lhs+w_rhs
  return (w_lhs+w_rhs)*res + res + (w_lhs+w_rhs)

**The real divergence is in the SYMBOL WEIGHT TABLE used by both
sides of the formula**:
  WM:    `CF_Phi` sums per-symbol weights from `SG_SymbolGewichtCP`,
         which is AUTO-DERIVED per problem from `atp_analyze_axioms`.
  thvm:  uses uniform `atp_symbol_count` (every symbol weight 1).

Empirical: with `THVM_ATP_CP_WEIGHT=4` (Mix) + uniform weights:
  - thvm picks comm pri=48, id-rt pri=19, inv-rt pri=29, assoc pri=65.
  - Selection order: id-rt(rule 1), inv-rt(rule 2), comm(eq), id-lt(rule 3), inv-of-e(rule 4), assoc(rule 5).

WM picks assoc as rule #2, not #5.  So WM's auto-derived weights
give assoc a much lower priority -- probably by weighting binary
operators lower than terminals, or by ranking variables higher.

**INITIAL_ULTIMATE shifts assoc to rank 3**: porting WM's
"initial = ultimate" action (axioms forced to heap front) closes
part of the gap but not all of it.

**Endgame chain is the remaining +2**: even with all rule-orders
matched, WM emits 3 SubstitutionLemma steps at the endgame where
thvm collapses into 1.  The CP-set IR sweep's per-rule emission
granularity is the next algorithmic mismatch.

## Phase 5 findings

The auto-weight hypothesis was WRONG.  WM .pr files for these cases
specify ALL symbol weights = 1 (matches thvm's `atp_symbol_count`).
The Mix formula matches.  Yet WM picks axiom-orient order differently.

**The remaining divergence is subtle**: under `initial=ultimate` +
`heuristic=mixweight`, both WM and thvm should pick lowest-Mix-pri
ultimate first.  For glid:
  id-rt    pri 19
  inv-rt   pri 29
  assoc    pri 65
thvm picks in this order (id-rt, inv-rt, assoc) -- matches the
formula.  WM picks (id-rt, assoc, inv-rt).

Possible causes:
- WM's CP-set IR reweights initial CPs after first orient, dropping
  assoc's apparent priority.
- WM's "database=ultimate" classification of derived CPs cascades
  differently than thvm's ultimate-only.
- WM's PCL protocol re-numbers axioms by some canonical order, and
  the orient(N,x) protocol's N doesn't refer to .pr position.

Without source-level instrumentation in wmcli, the next move is to
either:
1. Rebuild wmcli with WRITE_TRACES=1 to dump per-CP selection details
   (cmake + mathlink dep -- substantial work).
2. Accept the close-but-not-byte-identical match (8 cases at byte
   parity vs WM CLI; 4 cases within 1-2 picks).

## Status

**Cumulative wins**: mccune 49s -> 10.0s (-80%) via Mix flip
(b409b6ef) + precedence flip (e3334d46).  WL parity TSV unaffected
because WL's preset was already on Mix.  The C-bench WM-faithful
preset is now correctly aligned.

## Phase 6 -- the wolfram-cracking gap

WM CLI cracks `WolframAxioms__Commutativity` in **2.5s** with 661
rules / 738k critical pairs.  thvm `wolfram` bench (same axiom +
goal) runs 90s+ at 80k steps, 529 rules, never closes.

Root cause: WM's default config sets BOTH `initial=ultimate` AND
`database=ultimate` (Parameter.c:166).  The second flag means CPs
derived from chasing the rule-database during the CP-set IR sweep
ALSO get ULTIMATE classification (heap-front).  This creates a
depth-first bias -- newly-derived CPs are processed BEFORE older
axiom-CPs.

thvm has `INITIAL_ULTIMATE` (port of WM's first flag) but no
`DATABASE_ULTIMATE` equivalent.  Without it, thvm's saturator stays
breadth-first on the axiom-CP-set and doesn't dive into the deep
derived chain that contains the closure.

**Port target**: tag CPs derived during atp_cp_set_interreduce (or
during the per-step "chase" cycle) with the same ultimate bit
INITIAL_ULTIMATE uses.  See `s->cp_ultimate` field +
`atp_cp_before`.  The cascade should match WM's depth-first
trajectory on hard cases like wolfram.

## Phase 6 outcome (d1913175)

DATABASE_ULTIMATE shipped as opt-in `THVM_ATP_DATABASE_ULTIMATE=1`.
Empirical (all THVM_ATP_WALDMEISTER=1):

  config                                             wolfram (300s)
  baseline                                           RUNNING 339r
  +DB_U                                              RUNNING 190r
  +DB_U +IU                                          RUNNING 605r
  +DB_U +IU +SR=51                                   RUNNING 491r
  +DB_U +IU +CP_SET_IR_PERIOD=1                      RUNNING 216r 5k cps
  +DB_U +IU +CP_SET_IR_PERIOD=1 (300s)               RUNNING 480r

WM cracks the same problem in **2.5s** with 661 rules and 738k CPs.

The remaining gap is engine throughput, not heuristic config:
  WM:    738928 CPs / 2.5s = **300k CPs/sec**
  thvm:  IR_PERIOD=1 gives ~30 CPs/sec processed (aggressive IR
         dominates per-step cost).

Engine-level perf is needed -- incremental rule-database
interreduce, CP-batch normalization, or term-index sharing.  Out
of scope for this trace-diff session; the algorithmic ports
(Mix + precedence + DB_U + IU) are correct and aligned with WM's
WM-default classification.
