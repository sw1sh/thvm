# SKIToBCKW c1/c2 @1868 -- root cause (reference-trace + thvm heap dump)

Date: 2026-06-17. Branch: main (worktree wm-parity).

## Headline

@1868 is an **emission-order flip in the equation-tops CP ranking** -- the same
family as the 12 prior wm_order.c fixes, NOT a missing-CP gap (unlike Meredith
@6078) and NOT the "removal-avalanche" earlier notes guessed.  Both c1 and c2
diverge at exactly 1868, so it is in the shared saturation (goal-independent).

## Method note

The source-built instrumented ELProver does NOT reproduce the reference wmcli on
SKIToBCKW (it goal-reduces in 0-7 selections vs the reference's deterministic
44823).  So WM_HEAPDUMP is unavailable here; this used the reference `-a4` trace
(WM's selection + CP-formation order) plus thvm's THVM_ATP_HEAPDUMP_AT=1868.

## The flip

At pick 1868 both heaps hold equal-weight (203) CPs from the unoriented equation
-17.  Combinator->thvm label map (signature index + 3): W=C8, Y=C9, K=C6, S=C7,
I=C5, App=C10.

* WM picks the **W-variant** `App(W,App(S,I)) # App(App(W,Y),App(K,App(W,App(S,I))))`
  -- WM creation age 2216, parents (eq-17, rule **140**).
* thvm picks the **Y-variant** (same with inner Y) -- thvm cp_seq 2215,
  parents (eq-17, rule **138**).
* **thvm HAS the W-variant** at cp_seq 2217 (heap-dump confirmed): both CPs are
  present, only their creation order is swapped.

WM's full eq-17 CP-formation order (DSBaum retrieval over rules):

    4 145 140 138 136 133 135 129 131 124 125 123 117 111 110 118 107 115 103 ...

i.e. **rule 140 before rule 138**.  thvm orders 138 before 140.

## Where in the code

`thvm_atp_generate_cps_wm` (src/atp/_.c ~15143) collects the eq-17 batch and
sorts by `atp_wmo_rank` (wm_order.c:1690).  eq-17 is the outer fact (i==f, j!=f)
=> the **tops phase** (wm_order.c:1721-1738): key = (phase, k1=preorder pos in
eq-17, k2=tree, arr=`wmo_tops_rank` of the rule in the discrimination tree, ch).
For (eq-17,rule140) vs (eq-17,rule138) the tie is broken by k1 (the overlap
position in eq-17) then `arr` (the rule's tops-DFS rank).  One of those two
components is mis-ordered vs WM's DSBaum order.

## Pinned: it is the tops-DFS rank (`wmo_tops_rank` arr), not the position

A gated batch-order trace (THVM_ATP_BATCH_TRACE) over `thvm_atp_generate_cps_wm`
shows eq-17 is thvm fact **f=155** (unoriented).  The W/Y-variant CPs are
`combo=0` overlaps (i=155, j=rule, j_or=1), and their `atp_wmo_rank` keys differ
by EXACTLY 2^28 per CP -- i.e. only the `arr` field (the rule's tops-DFS rank)
varies; phase/k1(position)/k2(tree) are identical.  So all batch rules overlap
eq-17 at the SAME position and the order is purely `wmo_tops_rank`.

thvm's rule order (thvm slot indices, arr = 0,1,2,...):

    148 141 140 143 138 139 133 ...   (j=141 is the Y-rule, j=143 the W-rule)

so thvm ranks the Y-rule (141) before the W-rule (143).  WM's DSBaum retrieval
order has W's rule (140) before Y's rule (138).  => `wmo_tops_rank` walks the
discrimination tree in a different order than WM for this configuration.

## Deeper: it is a jump-exit (Sprungausgaenge) ordering anomaly

`wmo_tops_rank` -> `wmo_dfs` (wm_order.c:1219): the arrival order is the DFS
order.  The W-rule and Y-rule differ by a concrete symbol (W=C8, Y=C9) at a
branch position where eq-17's query has a VARIABLE.  At a var-query node, wmo_dfs
follows var children (descending index) THEN **jump exits `n->exits` in list
order** (1271).  So the W/Y order is the jump-exit list order at that node.

`n->exits` is **head-insert** (newest first; RumpfSprungeintragSetzen head-insert,
wm_order.c:368-384, "head = consulted first").  thvm slots: W-rule=143, Y-rule=141.
143 was added AFTER 141, so head-insert SHOULD prepend W's jump last => W at head
=> W before Y (matching WM).  But thvm emits **Y before W** -- an anomaly: either
the W jump was not head-inserted at that node, or a removal-collapse reissue
(wmo_sprung_reissue_cb / wmo_middle_reissue_cb) reordered the list.  This is the
exact Sprungausgaenge family the 12 fixes addressed; a residual is latent here.

## Drilled to the exact function: wmo_altes_blatt_polieren

The W/Y jump order is set in `wmo_altes_blatt_polieren` (wm_order.c:668) /
`wmo_jump_prepend` (:536).  This function ALREADY handles a W/Y sibling batch
(its else-branch comment cites SKIToBCKW @1113: "rule 564's overlaps against the
W-branch must precede the Y-branch, matching WM's CPNr/FIFO order") -- but that
path assumes both-function leaves sit on **exact-symbol children**
(MitSelbemSymbolAb), so "the ancestor's chain-node jump never gates their
relative DFS order" and it deliberately adds NO fresh jump (keeps `new_fun !=
old_fun` mixed-only).

**@1868 is the uncovered case**: here W/Y are reached via var-query JUMP EXITS
(wmo_dfs :1271), so the jump-list order DOES gate them -- the @1113 assumption
fails.  thvm emits Y-before-W; WM needs W-before-Y.  So either the W jump was
routed through the polieren splice-after-model (landing after Y) when it should
head-prepend, or a reissue reordered it.

## CONSTRUCTION TRACE RESULT (THVM_WMO_CT): W-rule has NO jumps

Ran SKIToBCKW c1 with THVM_WMO_CT + (trace-augmented) BATCH.  eq-17=fact f=155
(trace 2373); W-rule=slot 143 (trace 2087), Y-rule=slot 141 (trace 2072).

* Y-rule (2072): 5 jump constructions -- `WMOCT PREPEND depth={1,3,5,6,7}`.
* W-rule (2087): **ZERO** jump constructions (no PREPEND, no POLIER-PAR, no
  POLIER-FRESH, no else-retarget).

So the W-rule's leaf has no jump exits at all -> it is never placed in any
node's `n->exits`, so the var-query DFS reaches it only after Y's jumps fire ->
Y-before-W.  WM gives the W jump (W-before-Y), thvm skips it.

The likely culprit is the else-branch fresh-jump guard (wm_order.c:795-796):

    if (start_pos < i && e_old == i + 1u && j > i + 1u && (new_fun != old_fun))

`(new_fun != old_fun)` SKIPS the fresh jump when both branch cells are functions
(W=C8, Y=C9 are both concrete) -- correct for @1113 (leaves on exact-symbol
children, jump doesn't gate order) but WRONG for @1868 where W/Y are gated by
the jump list.  The guard needs to distinguish @1868 (needs the jump) from
@1113/Meredith@166 (must not have it).

## NEXT STEP: refine the fresh-jump guard
Find what distinguishes the @1868 tree config (W/Y gated by jumps) from @1113
(W/Y on exact-symbol children) at this `wmo_altes_blatt_polieren` call -- likely
whether the leaves actually become exact-symbol children vs jump-reachable at sn.
Refine the `(new_fun != old_fun)` condition (or add the missing jump for the
gated case), then VERIFY against the full matrix.  Keep @1113/@303/Meredith@166
byte-identical.

## (obsolete) earlier next step
Add a construction trace at wmo_jump_prepend + the polieren splice sites
(:702-734 if-branch, :766-786 else-branch) logging (start node, target leaf's
rule trace, mechanism, resulting head/after-model position) for the W (slot 143)
and Y (slot 141) jumps at the gating node.  Determine which mechanism each took
and why W lands after Y.  Write a targeted condition (analogous to the @1113
handling but for the var-query-jump case) and VERIFY against the full matrix
(66 byte-identical + SKIToBCKW/Meredith firstdivs via align.py) -- that matrix IS
the regression net, so WM's runtime tree is NOT strictly required for a
test-driven fix.  Guard against regressing @1113 / @303 / Meredith @166.

## BLOCKER for the fix (superseded by the NEXT STEP above for the jump-order path)

The fix needs WM's discrimination-tree traversal order as ground truth, but the
source-built ELProver goal-reduces SKIToBCKW (cannot reach the eq-17 batch to
BA_DumpBaum).  The reference `wmcli` (deterministic 44823, blind-saturate) was
built with a DIFFERENT config than this source tree's ELProver (478KB vs 419KB;
identical source, all my edits inert/stats-only; CMake builds ELProver with
WALD_LIB=1=MathLink, not the standalone wmcli) -- that build config is not yet
identified.  Options: (a) crack the reference build config to get a saturating
instrumented ELProver; (b) reason about `wmo_tops_rank` from the batch rules'
LHS structures + WM's known order without the tree.
