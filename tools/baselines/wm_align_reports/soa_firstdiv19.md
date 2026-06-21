# soa first selection divergence: firstdiv=19 (CORRECTED root, 2026-06-20)

This file went through TWO wrong roots before landing the honest one. Both wrong
roots came from an ad-hoc canonicalizer bug (`/tmp/cmp_order.py` numbered vars by
l-then-r order BEFORE sorting, so `a=b` and `b=a` got different keys -> it missed
CP occurrences in the non-canonical orientation). The fix: number vars per
orientation and take min over both orientations (`min(co(l,r), co(r,l))`).
ALWAYS trust `align.py` for the divergence and an orientation-independent
canonicalizer for term identity.

## Withdrawn wrong roots (do not revisit)
1. "eqn-10 forms ~290 CPs late (pick-125)": FALSE. pick-125 was stale; the real
   align.py firstdiv is 19. eqn-10 tracks (thvm seq 370 ~ WM w2 414).
2. "thvm's rules-only join misses eqn-7, so it keeps CP_B that WM joins+drops":
   FALSE. With the corrected canonicalizer, WM KEEPS CP_B too (first at w2=181;
   thvm seq=168). WM's KPBehandelt is deliberately rules-only (doE=FALSE,
   `-kg` default "r"; see src/atp/ft_norm.c:853-858), and thvm's
   `atp_rules_only_normalize_ft` join FAITHFULLY matches it. Making the join
   equation-aware would DIVERGE from WM -- do NOT do it.

## The real divergence (verified)
align.py: firstdiv=19, prefix=18 identical, weights byte-exact (all relevant CPs
w1=120 -> pure FIFO-age tiebreak). At selection 19:
- WM picks `x.x = x.(y.(y.y))`   (CP_A)
- thvm picks `(x.(x.x)).y = y.y`  (CP_B), seq=202

Both CPs are formed early in BOTH engines and track closely (corrected canon):
- CP_A `x.x = x.(y.(y.y))`:   WM first w2=2,   thvm first seq=1
- CP_B `(x.(x.x)).y = y.y`:   WM first w2=181, thvm first seq=168  (thvm ~13 earlier)
Neither is dropped; both are kept by both engines. So the divergence is NOT
keep-vs-drop, NOT weight, NOT orientation, NOT the join. It is the **FIFO-age
(formation-order) assigned to the CP copies**: thvm forms CP_B slightly earlier
and/or in a different multiplicity than WM, so at pick-19 a CP_B copy out-ages the
CP_A copy WM selects. This is the global CP-formation-order reproduction handled
by `src/atp/wm_order.c` (the discrimination-tree retrieval order that stamps FIFO
ages to mirror WM's Regelbaum/Gleichungsbaum scan) -- i.e. exactly the class the
original `soa.txt` deferred. The 32685-vs-5607 selection blowup is the downstream
consequence of diverging at 19.

## PINNED: formation order first diverges at CP #12 (the 3rd-axiom overlap order)
With the corrected canonicalizer, the two formation sequences are BYTE-IDENTICAL
for CPs #0..#10, then diverge at **#12** (WM w2=12 vs thvm seq=11). Both diverging
CPs are critical pairs of the 3rd axiom eqn-2 = `(x.(y.z)).(x.(y.z)) =
((y.y).x).((z.z).x)` (parent -2): WM's #12 is `((a.a).(a.b)).((b.b).(a.b)) = a.b`,
thvm's is `((x.x).y)^2 = (y.(x.y))^2`. So thvm and WM enumerate eqn-2's overlap
CPs in a DIFFERENT ORDER from the 12th CP on. That reorders the FIFO ages of the
whole eqn-2 overlap batch (w2/seq ~12-20), which propagates to CP_B's age (168 vs
181) and surfaces as the pick-19 selection divergence.

## PROVEN deferred-divergent (not a localizable bug)
Aligning the whole eqn-2 overlap batch (orientation-independent canon) shows a
SCRAMBLE, not a clean swap:
  WM w2=12,13,14,15  ->  thvm seq=38,37,39,40   (WM's early-batch CPs land ~25
                                                  positions later in thvm)
  thvm seq=11,12,13,14,15 -> WM w2=36,37,40,38,39
  WM w2=16..25 -> thvm seq=NONE (not formed in thvm's first 40); and many thvm
  seq=16..24 -> WM=NONE likewise.
So the two engines traverse eqn-2's self+cross overlaps in fundamentally
different orders and form different subsets first -- a multiplicity/enumeration-
order scramble, NOT a single tie-break inversion that a small wm_order patch
could flip. This is the genuinely-divergent tail (cf. memory
project_atp_wm_preset_tuning "Residual 12 = deep AC-completion multiplicity
DEFERRED"). Matching it needs reproducing WM's exact eqn-2 discrimination-tree
traversal wholesale -- the deepest part of wm_order.c -- with high risk of
regressing the 73 aligned theorems. Engineering call: do NOT chase soa with a
risky wm_order rewrite; the achievable WM-parity frontier is the aligned set
(73 + SKIToBCKW @1868 landed this session). soa stays DEFERRED, now with a proof
(the scramble) rather than a hunch.

The original mechanism note below stands but is subsumed by the scramble proof:
This is purely `src/atp/wm_order.c`'s discrimination-tree retrieval-order ranking
(wmo_rank / wmo_tops_rank / the preorder/leaflist keys) for a single big symmetric
equation's self+cross overlaps. It is the soa.txt-deferred class: not a missing
overlap, not subsumption, not weight, not the join (all ruled out) -- a tie-order
difference in the most-tuned heuristic in the tree. A fix must reproduce WM's
exact eqn-2 overlap enumeration order at CP #12 AND hold every
`tools/baselines/wm_align_reports` md5 + test_atp/test_kbo. Concrete next probe:
dump thvm's wmo_rank key vs WM's `##K`/age order for the eqn-2 overlap batch
(facts active at CP #12) and find the first key inversion.

## ATTEMPTED the eqn-2-overlap-order rework (2026-06-20) -- localizable but INERT
Localized the CP-#12 batch-order divergence precisely: the f=2 (3rd-axiom) batch
keys order phase-0 (A = distinguished-face tops) first, but WM forms its phase-4
(D = reverse-face tops) CPs first. Root: thvm labels eqn-2's distinguished face
via `atp_wmo_eq_dist_rhs_base` -> TRACE_AXIOM returns dist_rhs=1, but WM stores the
axiom with its .pr LHS distinguished (dist_rhs=0; verified vs WM CLASSDUMP w2=3).
A gated fix (TRACE_AXIOM -> 0) was implemented + tested: it CORRECTLY flips the
eqn-2 batch order to WM's (combos 2,3 -> 0,1, phases realign). BUT soa firstdiv
STAYS 19 -- even combined with THVM_ATP_CP_SIDE. The eqn-2 reorder is MOOT for
the pick-19 selection (thvm still picks CP_B seq=202 vs WM's CP_A; the reordered
CPs don't change the w1=120 FIFO tiebreak). Probe reverted (inert).

CONCLUSION (now by direct attempt, not assumption): soa firstdiv=19 is NOT
localizable to the eqn-2 overlap order. The pick-19 divergence is the downstream
formation-age/multiplicity of CP_A vs CP_B copies -- exactly the IRREDUCIBLE
TENSION the engine already documents at src/atp/_.c:5005-5029: the axiom-swap soa
needs forks the *tracked* CombinatorAxioms__BCKWToSKI__c2 at pick-55 via a
downstream FIFO cascade "whichever stored order WM and thvm agree on", which is
why `use_cp_side` defaults OFF. soa stays DEFERRED for that documented,
principled reason. soa is also non-tracked (not in the 82 baseline); the tracked
port is 78 identical + cf8aa3bb SKIToBCKW.

## Durable tooling built this session (env-gated, revertible)
WM source (revert: `git -C /Users/swish/src/wolfram/waldmeister checkout sources/`):
- `NewClassification.c` WM_CLASSDUMP (+w1, pos, hist, true parent terms)
- `KPVerwaltung.c` WM_OVLBUILD (per-build CP + parents) + WM_DROPDUMP (drop reason)
- `Unifikation1.c` WM_OVLBUILD (unused path)
Run plain (drop `-a4`): `WM_CLASSDUMP=1 ELProver /tmp/soa.pr`.
thvm: `THVM_ATP_CP_FORM_TRACE` (CPFORM), `THVM_ATP_CPGEN_DEBUG` (producers),
`THVM_ATP_CP_PICK_TRACE` (CPSEL). align.py is the divergence oracle.

## firstdiv campaign progression (env-gated knobs, all DEFAULT OFF)
Measured against the real 2807-selection WM trace /tmp/soa_align/wm.txt via
align.py. Each knob clears one emission-order tie class; OFF byte-identical,
test_atp green 136232/136232.
  19 -> 125 -> 288 -> 290 -> 712 -> 778 -> 966
Knobs (cumulative): THVM_ATP_WALDMEISTER, _CP_WM_SIDE, _WM_FLAT_SUBSUME,
_WM_COMM_REAGE, _WM_COMM_DROP_DUP, _WM_LEAF_TIEBREAK (-> 778),
_WM_REVFACE_GROUP (-> 966).

### firstdiv 778 -> 966: THVM_ATP_REVFACE_GROUP (use_revface_group)
The w=209 cluster at picks 778/779/780 is a pure permutation (thvm-only=0),
same KIND as the w=120 LEAF_TIEBREAK cluster but a DIFFERENT band and a
DIFFERENT partner class. In thvm batch f=36 (the new equation -21
`x.(y.x) = ((z.(z.z)).y).x`) three reverse-face CPs share one tops overlap
group (D phase, identical phase/k1/k2): a PERMUTATION partner
`(x.(x.x)).y = y.(x.(x.x))` (var-differ==0) whose reverse overlap reduces to the
Cshape `(x.(x.x)).y = z.(z.z) # y`, plus two var-differ==1 (WM-oriented) partners
whose CPs reduce to the `x1 # ...` forms. WM emits the Cshape copy ADJACENT to
the earlier same-shape Cshape CP (pick 778), AHEAD of the oriented-partner forms
(779/780); thvm's independent leaf DFS keys the permutation copy at arrival
rank 6 (vs the anchor's 1), scattering it to pick 780 with the oriented forms at
778/779. The knob re-keys the permutation reverse-face CP to sort immediately
after the largest-keyed same-group CP it ALPHA-matches (orientation-insensitive
on the normalized joined pair), restoring WM's adjacency. Scoped HARD: D phase,
i == f, partner reverse face (combo bit0), var-differ==0 permutation partner,
identical group prefix, alpha-equal reduced pairs. Generalizes: one re-key clears
the w=209 cluster AND several sibling Cshape clusters (w=189/209), jumping
firstdiv 778 -> 966. Ring ZeroIsAbsorbing knob ON proves cleanly (cps=1097).
Diff: src/atp/_.c thvm_atp_generate_cps_wm re-key pass + atp_pair_alpha_eq /
atp_term_alpha_eq helpers; struct field/setter/env per LEAF_TIEBREAK plumbing.

### residual firstdiv 966 (DISTINCT class, deferred)
The w=120 cluster at picks 966-970 (thvm batch rules=42) is a MULTI-SHAPE
interleave, not a single same-shape displacement: WM wants P,Q,Q,P,P,Q,Q where
thvm emits P,P,Q,Q,Q,P,Q (P = `(x.(x.x)).y # y.y`, Q = square-of-`(x.y)` forms).
The REVFACE_GROUP single-shape-adjacency rule does not apply; this is a
different intra-batch emission-grouping tie (WM groups by overlap position, not
reduced shape). A separate mechanism, deferred.
