# SKIToBCKW c1/c2 @1868 -- root cause (reference-trace + thvm heap dump)

Date: 2026-06-17. Branch: main (worktree wm-parity).

## RESOLVED 2026-06-20 (commit cf8aa3bb)

@1868 is FIXED.  Root cause: the discrimination-tree removal-collapse re-issue
`wmo_sprung_reissue_cb` (src/atp/wm_order.c) only fired when the re-hung leaf's
node `up` had exactly ONE model sibling (`n_leaf_kids==1`).  At @1868 W's (trace
2087) inner node collapses and re-hangs at a node with THREE model siblings, so
the re-issue was skipped and W's depth-1 jump stayed mid-list behind Y's,
flipping the var-query CP-arrival tie -> thvm picked Y(C9) where WM picks W(C8).
Fix: `wmo_sprung_reissue_multi_cb` handles the `n_models>=2` fan (head-placement
for the newest leaf, group-aware guard isolating the case).  SKIToBCKW c1 AND c2
pick 1868 now = W(C8), 1869 = Y, matching WM through pick 1877+; prefix content
byte-identical; all 10 wm_order baselines unchanged; test_atp 136232.  Every
earlier dive below (collapse-interleaving, split-time placement,
insertion-decision) correctly proved its layer byte-faithful and narrowed to
this gate -- kept as the trail.  The exact NEW firstdiv (whether c1/c2 are now
fully identical) needs a wmcli re-alignment.

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

## RESOLVED root cause (2026-06-20 deep dive -- supersedes the fresh-jump-guard hypothesis)

The earlier "W-rule has NO jumps" finding was an INCOMPLETE trace: THVM_WMO_CT
only logged `wmo_jump_prepend` (PREPEND) and the polieren splices; it did NOT
log the removal-collapse REWIRE path (`wmo_rewire_cb`).  Instrumenting the rewire
(WMOCT REWIRE) shows the W-leaf (trace 2087) DOES get its depth-1 / depth-3 /
depth-5 jumps -- via the collapse that re-hangs it as the surviving sibling.  So
the W-leaf IS in the depth-1 `n->exits`; the divergence is the LIST ORDER, not a
missing jump.  The fresh-jump-guard avenue is a dead end: @1868's split (2062 vs
2027, i=5 j=6) is MINIMAL (j == i+1), so neither thvm's `POLIER-FRESH-HEAD`
(requires j > i+1) nor WM's BlattAufgeteilt chain-pop emits any chain-node jump
there -- both correctly re-target in place.

The competing depth-1 jumps, in CREATION order, at the f=155 query
`App(v1, App(K, v1))` (decode f5=I f6=K f7=S f8=W f9=Y f10=App):
  1. PREPEND sub `App(W, App(S,I))` -> rule-2027's leaf, emitted during 2027's
     PLAIN insertion suffix-drain (2027 = `App(App(W,App(S,I)), Y)`, hung i=5).
  2. PREPEND sub `App(Y, App(S,I))` -> Y-leaf (2072), emitted during 2072's hang.
  3. (2062 splits 2027: re-targets jump 1 onto the new depth-6 node, IN PLACE.)
  4. W-leaf (2087) hangs PLAIN at i=6 (intro = a VARIABLE at the LAST cell); its
     only enclosing pending is the ROOT (depth-0), suppressed by Untergrenze =
     EintragEins -- so W gets NO own depth-1 jump (FAITHFUL: WM's
     NeuesBlattEinhaengen `StapelUeber(Stapeluntergrenze)` suppresses it too).
  5. COLLAPSE (sib = W-leaf, up_depth=5, n_freed=1) REWIRES jump 1 onto W,
     PRESERVING its outgoing-list position (the OLD, pre-Y position).

So the W-path jump sits at jump-1's OLD position (created BEFORE Y); Y's jump is
NEWER (nearer the head-first list head) -> Y consulted first -> Y-before-W.

EVERY step is byte-faithful to the WM source (verified against
waldmeister/sources/WDT/DSBaumOperationen.c + INF/Unifikation1.c):
  - Schrumpfen (collapse, :1057-1095): `AlleEingehendenSpruengeUmsetzen` (:808)
    redirects INCOMING jumps onto the neighbor leaf but NEVER touches the
    outgoing `NaechsterZieleintrag` order; `BK_NachfolgenLassen` (:1083) issues
    NO jump on the collapse-target re-hang.  thvm's `up`-only head-move in
    `wmo_rewire_cb` is ALREADY a thvm divergence (added for the McCune avalanche
    corner); it does not fire at depth-1 here (up_depth=5) and is irrelevant.
  - AltesBlattPolieren else-branch (:534-561): re-targets in place, NO fresh jump.
  - DeltaFreieVar0 (:784-789): the var-query DFS walks jump exits in pure
    `Sprungausgaenge` list order -- exactly thvm's `wmo_dfs` jump loop.
By the spec, jump 1 is created BEFORE Y's in BOTH thvm and WM (identical rule-
insertion order; align prefix = 1867 byte-identical, thvm-only=0), and the
redirect preserves position in BOTH -> the spec PREDICTS Y-before-W for both,
yet WM's banked order is W-before-Y.

WHY (the residual divergence): thvm's depth-1 arrival order at the f=155 query is
  2131, 2072(Y), 2046, 2087(W), 2011, 2032, 1810, ...
WM's banked formation order is `4 145 140(W) 138(Y) 136 133 ...`, i.e.
  2131, 2087(W), 2072(Y), 2046, ...
-- so it is NOT a clean 2-element W/Y swap: thvm ALSO floats 2046 ahead of W and
re-orders 2072/2046 relative to WM.  The depth-1 list (88 exits) is a complex
mix of head-prepended fresh jumps and position-preserved redirected jumps, with
MANY older-jump-nearer-head inversions, all set by the construction+removal
HISTORY.  Reproducing WM's exact list order needs WM's exact jump genealogy --
which diverges from thvm's only in a removal/redirect-ordering corner (the
documented McCune avalanche family, wm_order.c header :89-98), invisible in the
rule-insertion order.

EXPERIMENTS RUN (all gated, all reverted -- none lands W EXACTLY before Y):
  - sort var-query jump exits by sub cells (THVM_WMO_JSORT): global reshuffle,
    W moves to 1866; un-faithful (DeltaFreieVar0 is pure list order).
  - head-move ALL rewired jumps (THVM_WMO_REWIRE_ALLHEAD): over-corrects, 1868
    becomes seq 2218.
  - head-move rewired jumps only at var-gated nodes (THVM_WMO_REWIRE_VARGATE):
    same over-correction -- a head-move lands W at the ABSOLUTE head, not the one
    slot before Y.
  - disable the `up` head-move (THVM_WMO_REWIRE_NOUPHEAD): 1868 unchanged
    (confirms the existing `up` hack is not the lever here).
The head-move overshoots (W -> head); position-preservation undershoots (W stays
at jump-1's old slot).  No single local rule lands W at exactly Y-1 without WM's
actual genealogy.

## CONCRETE NEXT MECHANISM
Get WM's actual depth-1 `Sprungausgaenge` order for the f=155 query (the only
remaining ground truth gap).  The instrumentation is now in hand: re-add the
reverted WMOCT-D1 / WMOINS COLLAPSE / WMODFS jump-sub traces (this dive's gated
scaffolding) and, on the WM side, dump `BK_Sprungausgaenge` at the depth-1 node
right before the eq-17 batch.  Then diff the two 88-entry lists to find the
SINGLE redirect/prepend event whose relative order differs -- it will be a
collapse or interreduce earlier in the saturation where thvm's
`wmo_rewire_cb`/`wmo_kill_entries_to` ordering diverges from WM's
`AlleEingehendenSpruengeUmsetzen` + `NullifizierteEingehendeSpruengeLoeschen`
sequence (DSBaumOperationen.c :1076-1093).  Port that exact sequence (thvm
currently kills-then-rewires in two passes; WM nullifies-rewires-then-deletes in
a specific interleaving that can leave a redirected jump at a different list
slot).  That is the faithful fix; the JSORT/head-move shortcuts are not.

To reach the WM batch for the dump: the source-built ELProver goal-reduces, so
build the standalone saturating `wmcli` config (the 478KB binary) WITH the
WMOCT-equivalent BK_Sprungausgaenge dump, OR add a one-shot
`BA_DumpSprungliste(depth1_node)` call gated on CPNr == the eq-17 batch start.
