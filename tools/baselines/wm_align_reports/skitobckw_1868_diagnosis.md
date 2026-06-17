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

## BLOCKER for the fix

The fix needs WM's discrimination-tree traversal order as ground truth, but the
source-built ELProver goal-reduces SKIToBCKW (cannot reach the eq-17 batch to
BA_DumpBaum).  The reference `wmcli` (deterministic 44823, blind-saturate) was
built with a DIFFERENT config than this source tree's ELProver (478KB vs 419KB;
identical source, all my edits inert/stats-only; CMake builds ELProver with
WALD_LIB=1=MathLink, not the standalone wmcli) -- that build config is not yet
identified.  Options: (a) crack the reference build config to get a saturating
instrumented ELProver; (b) reason about `wmo_tops_rank` from the batch rules'
LHS structures + WM's known order without the tree.
