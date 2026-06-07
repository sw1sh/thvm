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

## Phase 5 work

1. **Port WM's auto-weight derivation**.  Read
   `sources/CLAS/NewClassification.c` (atp_auto_precedence-style
   logic).  Add `atp_auto_weights` producing a per-label weight
   array.  Wire into `atp_cp_weight_base` (Mix / Mix2 / Max paths).
2. **Re-trace agioc**.  Confirm assoc moves to rule #2 with both
   auto-weights AND INITIAL_ULTIMATE.
3. **Verify**: IOC closes from 18 -> 20 OR the endgame chain
   becomes 3-step.
