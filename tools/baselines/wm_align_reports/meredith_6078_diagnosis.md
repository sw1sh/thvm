# Meredith AndAssoc @6078 -- root cause (instrumented-wmcli diagnosis)

## GAP FULLY MEASURED (2026-06-21, run-confirmed, slot-reuse un-confounded)

CP-gen is correct IN THE ACTUAL RUN (not just isolation): grepping the cpgen by
the rule126/rule36 TERMS (not slots), thvm does 5 rule126-OUTER x rule36-INNER
overlaps at positions (root),1.,1.1.,1.1.0.,1.1.1., and the **pos=1.1.** one
yields cp(norm) = `C3(x_0, C3(x_0, x_1)) = C3(C3(x_1, x_1), x_0)` = `a.(a.b) =
(b.b).a` = T. So thvm forms T from rule126 x rule36 exactly like WM. The earlier
"rule126 never an outer T-producer (count 0)" was the SAME canonicalization
artifact (a too-specific T pattern).

The gap is the keep-MULTIPLICITY, now measured on a 6500-pick run:
* thvm GENERATES T (cp(norm)=T) **451** times; CLASSIFIES/keeps only **28**.
* The kept-T cp_seqs have a **drought of 7879 between seq 15233 and 23112**;
  pick 6078 (~seq 23000) falls INSIDE it -> the pri=120 band is dry exactly there.
* WM keeps T ~283 times and replenishes through the 6078 epoch.
* T is `joinable=0` for all 451 (never dropped as joinable); cpfate shows no
  perm/rule/queue DROP for T. So the 451->28 reduction is NOT a traced filter --
  it is the push-time re-normalization / rule-set-timing (a generated T reduces to
  a different CP under the rules live at push, or the wmo_rank batch keeps fewer).

=> The lever is the keep-multiplicity of T across the drought, governed by
wm_order.c's FIFO-formation reproduction + the rule-set/normalize timing. NOT a
CP-gen gap. Closing it = reproduce WM's exact T-keep multiplicity/order (global
wm_order rework, baseline-regression risk). The OPEN sub-question for a targeted
fix: WHY do 423 of 451 generated T's not survive to the queue (push-renormalize
to non-T? a transient rule reducing T that WM lacks?) -- pin that and the keep
gap may be closable without a full global reorder.

## CONTRADICTION RESOLVED (2026-06-21): thvm DOES form T from rule126 x rule36

The long-running "UNRESOLVED CONTRADICTION" (thvm's complete enumeration of the
@6078 parent pair allegedly lacks the 2-var target T) is settled: it was a
term-matching artifact, the SAME canonicalization blind spot that bit every
"thvm is missing CP X" finding this lineage.

Method: captured the @6078 CP's RELIABLE formation parents from the instrumented
ELProver (WM_CLASSDUMP, w2=23993): aP=126 = `x.(y.((z.x).(w.z))) -> (y.z).x`
[**4 vars** -- the prior unit test used a wrong 3-var rule126, an ElternNr-reuse
artifact], oP=36 = `x.((x.y).(y.z)) -> y.x`, overlap position = rule126's pos-1
subterm `y.((z.x).(w.z))`. New probe tests/test_cp_mered6078.c builds those exact
terms and enumerates all CPs. thvm's pos-1 overlap (A1#1) NORMALIZES to
`(C3 V0 (C3 V0 V1)) # (C3 (C3 V1 V1) V0)` = `a.(a.b) # (b.b).a`, which IS T
`(x.x).y # y.(y.x)` under the renaming V0<->V1 + orientation swap. (The probe's
own is_T check still reports 0 -- it shares the same_eqn/normalize_vars
canonicalization artifact; the PRINTED normalized form is the ground truth.)

So thvm's CP-gen is CORRECT: it forms T from rule126 x rule36 exactly as WM does.
@6078 is therefore NOT a CP-generation gap. It is the CP-formation MULTIPLICITY/
TIMING drought (thvm forms T 16 times, last at cp_seq 15233, consumed before pick
6078; WM keeps replenishing) -- exactly the WM-side deferred conclusion the
"RESOLVED-AS-WM-SIDE" section below reached independently. Both routes now agree:
thvm-side has no defect; the divergence is WM's CP-formation order/multiplicity,
deferred (same family as the residual deep-multiplicity matrix rows).



Date: 2026-06-17. Branch: main (worktree wm-parity).

## RESOLUTION (2026-06-17, supersedes everything below)

**thvm's CP-generation is CORRECT.  It is NOT a CP-gen completeness gap.**

Every section below that pins @6078 on thvm "missing a 126x36 superposition"
or "under-forming the 2-var CP" was built on a **wrong-parent-terms artifact**:
the parents "rule 126" / "rule 36" were extracted by *rule-orientation-number*
(`added as new rule N`), which is a DIFFERENT counter from the `ElternNr` the
heap records in `CP(126,36)`.  The unit test therefore overlapped the wrong two
terms and (correctly, for those wrong inputs) failed to produce the 2-var CP.

Anchoring by an invariant fixed it.  The 2-var CP carries a stable FIFO
classification counter `w2 = ++CPNr = 23993` (NewClassification.c `C_Classify`),
immune to WM's rule slot/ElternNr reuse.  A new gated dump
`WM_CLASSDUMP_W2=23993` prints the TRUE formation parents at classification:

    actualParent (ElternNr 126): f(x1, f(x2, f(f(x3,x1), f(x4,x3)))) -> f(f(x2,x3), x1)   [4 vars]
    otherParent  (ElternNr 36):  f(x1, f(f(x1,x2), f(x2,x3)))        -> f(x2,x1)
    formedCP:                    f(f(x1,x1),x2) # f(x2,f(x2,x1))

Feeding *these* terms to `thvm_critical_pairs` (test `atp/meredith-6078-126x36-
superposition`) yields **16 CPs including the 2-var** (CP5, verified by a
bijective-variant check).  thvm forms the same 126x36 CP Waldmeister does.

**Consequence:** @6078 is NOT a CP-gen bug.  If a divergence remains, it is
UPSTREAM in rule derivation -- whether thvm derives the same ElternNr-126/36
parent rules (esp. the 4-variable parent 126) at the same point.  That is a
different investigation; the CP-generation path is exonerated.

The instrumented-WM recipe (vendored waldmeister repo, env-gated, all in
`KPVerwaltung.c` + `NewClassification.c`): `WM_HEAPDUMP_AT=<sel>` dumps the CP
heap before a selection; `WM_CLASSDUMP_W2=<n>` dumps a CP's true formation
parents by FIFO counter; `WM_CPP_A`/`WM_CPP_B` dump by ElternNr pair (unreliable
-- ElternNr is reused, so it can miss the formation event).  Build:
`make -f Makefile.MacOSX-ARM64 MLINKDIR=/tmp/wmlink ELProver`; run with
`DYLD_FRAMEWORK_PATH=/tmp/wmlink/CompilerAdditions ./ELProver -a 4 <file.pr>`.

## Headline (SUPERSEDED -- see RESOLUTION above; kept for the trail)

The @6078 divergence is **NOT an emission-order bug** (the class all 12 prior
fixes addressed).  ~~It is a **CP-generation completeness gap**: at pick 6078
thvm's CP heap is *missing* a weight-120 critical pair that Waldmeister has.~~
**Refuted** -- the "missing CP" was a measurement artifact (wrong parent terms).

## Method (the user's "modify wmcli and debug it properly" steer)

Built a faithful standalone instrumented Waldmeister CLI from source
(`/Users/swish/src/wolfram/waldmeister/ELProver`, byte-identical -a4 trace to
the reference `wmcli` modulo a `## FIFO ##` annotation + whitespace) and added
two env-gated dumps:

* WM side -- `WM_HEAPDUMP_AT=<sel>` (KPVerwaltung.c `Select`): dumps the whole
  CP heap (parents, w1 weight, w2 FIFO age, term pair) just before a given
  selection, via the existing `KPV_Dump`/`UEOutput_TW` walk.
* thvm side -- `THVM_ATP_CP_FORM_TRACE=1` (src/atp/_.c, both `cp_seq` stamp
  sites): one `CPFORM seq=.. w=.. lhs=.. rhs=..` line per CP at classification.

## What the two heaps show at pick 6078

* WM picks the 2-var CP `dot(dot(a,a),b) # dot(b,dot(b,a))`  (heap w2=23993).
* thvm picks the 3-var CP `dot(a,dot(b,c)) # dot(dot(c,b),a)` (thvm seq=23220).
* WM only reaches that 3-var CP at pick 6115 (37 selections later).

thvm picks the 3-var CP early **because its heap lacks the 2-var CP** that WM
selects at 6078.

## The smoking gun

`align.py` multiset delta (canonical, authoritative):

    WM-only   x116  opCenterdot(opCenterdot(v1,v1),v2)#opCenterdot(v2,opCenterdot(v2,v1))

WM selects this 2-var CP **116 more times** across the run than thvm.

thvm `CPFORM` classification ages for that exact CP:

    741 1274 1287 5594 ... 14707 14711 15228 15233 [ GAP 29330 ] 44563 48430 ...

thvm stops forming the CP after seq **15233** and does not form it again until
seq **44563**.  Pick 6078 falls inside that hole, so the heap runs dry of this
CP exactly there while WM keeps replenishing it.

## Causes RULED OUT (instrumented, 2026-06-17)

The CP heap lacks the 2-var CP at pick 6078 because (a) thvm consumed all its
earlier-formed instances (last formed at cp_seq 15233) before pick 6078, and
(b) thvm forms NO fresh instance in the pick-6078 epoch, whereas WM forms one
(heap w2=23993) from a rule overlap right before pick 6078.  The cause of (b)
is narrowed by ruling these out (combined CPRAW_DEBUG+CP_FORM_TRACE+CP_PICK_TRACE
run):

* **queue-vs-queue subsumption** -- OFF under the WM preset
  (`thvm_atp_set_use_queue_subsume(s,0)`, test bench line 898).  WM has none
  either (`SS_TermpaarSubsummiertVonGM` matches only the active rule set).
* **trivial-joinability drop** -- the 2-var CP is enumerated and every instance
  shows `joinable=0` (never dropped as joinable).
* **orphan-murder / pop-subsume disposal** -- 0 of the formed 2-var instances
  are orphaned or pop-subsumed; formed count == selected count exactly.
* **periodic CP-set interreduction** (WM `KPV_KPMengeInterreduzieren`/`do_KPIR`,
  thvm `THVM_ATP_CP_SET_IR`) -- enabling it (default period) makes Meredith
  firstdiv *worse*: 6078 -> 74.  So thvm's CP-set-IR does not match WM's
  checkpoint schedule and is NOT the missing knob.  WM keeps the unorientable
  axiom 1 as an E-equation (parent `-1`) and forms CPs from it bidirectionally;
  the 2-var CP is partly formed from such equation overlaps (~20 of WM's 80
  distinct producing parent-pairs have a negative/equation parent).

The remaining locus is a subtle unfailing-equation CP-formation timing
divergence: thvm and WM form ~the same set of 2-var CPs (~80 overlaps each) but
thvm's last formation before pick 6078 is cp_seq 15233 (all consumed), while WM
forms a fresh one (heap w2=23993) from an equation/rule overlap in the 6078
epoch.  A formation-order aligner does not cleanly apply (WM forms ~600k CP
events vs thvm ~125k -- joinability filtering differs), so the next step is a
targeted parent-rule correlation of WM's age-23993 overlap to thvm's rule set.

The `WM-only x116` multiset delta is mostly a *downstream* effect: WM's run is
64800 picks vs thvm's 35891, so WM simply forms+selects this CP ~114 more times
over its longer trajectory.  The *causal* gap is the single missing fresh
formation at the 6078 epoch.

## Confirmed by direct thvm heap dump (THVM_ATP_HEAPDUMP_AT=6078)

thvm's CP queue at pick 6078: 18250 CPs, 46 in the chosen pri=120 band.
The chosen 3-var CP is the lowest-seq (23220) in the band.  The 2-var CP
`(C3 (C3 V0 V0) V1)#(C3 V1 (C3 V1 V0))` is **absent from the band (count 0)** --
direct confirmation that thvm's heap genuinely lacks it, not a representation
artifact.  thvm's last formation of it was cp_seq 15233; the next is 44563, and
pick 6078 sits in that hole.

## Equation-overlap under-formation (2026-06-17, faithful ground truth)

WM forms the 2-var CP from 80 distinct parent-pairs; **36 have an equation
(negative) parent** (WM E-set eqs -2,-6,-8,-10,-11,-12,-13).  thvm (BATCH+CPFORM
orientation tally over its 2-var CP formations): **53 rule×rule, but only 8
equation-involved** (5 rule×eq, 1 eq×rule, 2 eq×eq).  So thvm's rule×rule
formations roughly match WM, but it under-forms the EQUATION-overlap 2-var CPs
(8 vs 36).  That is the @6078 gap: thvm's unorientable-equation set/overlapping
is less thorough than WM's.  Candidate roots: (a) thvm keeps FEWER unoriented
equations (orients some WM keeps bidirectional), (b) thvm misses some
equation-overlap positions, (c) thvm subsumes/normalizes equations WM retains.
NEXT: compare thvm's unoriented-fact count + the specific equations to WM's
E-set at @6078 (distinguish (a) from (b)/(c)).  Faithful ELProver gives WM
ground truth here (unlike SKIToBCKW).

## ROOT CAUSE (2026-06-17, consistent numbering): thvm misses a 126x36 superposition

Using ONE binary (my ELProver) for both the heap dump and the -a4 trace (so
ElternNr is internally consistent -- the reference wmcli and ELProver number
differently, which earlier mixed two numbering schemes):

* @6078 CP = `CP(126,36)` w2=23993 = 2-var CP, formed at **cp 37130, parents
  126x36, RAW = the 2-var CP directly** (no normalization step -- which is why a
  `normalized to:`-gated search missed it).
* rule 126 = `dot(x1,dot(dot(x2,x3),dot(dot(x3,x2),x1)))->dot(x1,x1)` = thvm slot 120.
* rule 36  = `dot(x1,dot(dot(x1,x2),dot(x2,x3)))->dot(x2,x1)`           = thvm slot 33.
* thvm computes the slot-120 x slot-33 overlap **5 times (cpgen), NONE yielding
  the 2-var CP**; WM forms the 2-var CP from 126x36 (cp 37130).

=> thvm's overlap enumeration MISSES the specific superposition position WM uses
to build the 2-var CP from this rule pair.  cpgen logs the RAW overlap pre-filter,
so it is genuinely NOT computed (not filtered).  FAITHFUL-FIXABLE in
`atp_overlap_ij` (missing position/unifier) or the re-overlap path; full ground
truth available (ELProver faithful through pick 6078).

Read atp_overlap_ij (_.c:14853): thvm splits a rule pair's overlaps across the
saturator's TWO visits -- the `i>j` call OWNS root x root overlaps (skip flags 0;
toplevel phase walks TT(l) incl. root vs old tops), the `i<j` call enumerates
PROPER subterm positions only (skip flags 1) -- mirroring WM U1_KPsBildenZuRegel
(toplevel-pass with the rule itself as Ausschluss vs eTT proper-subterm pass).
The 2-var CP comes from superposing rule 36's LHS into a PROPER SUBTERM of rule
126's LHS (rule 36's LHS does not unify with 126's whole LHS -- occurs-check
fails -- so it must match a deeper subterm).  Hypothesis: thvm's `i>j` / `i<j`
skip-flag split (or the FT position walk thvm_critical_pairs_pair_ft) mis-assigns
or skips this proper-subterm superposition for the 120x33 pair.

CONFIRMED count: thvm computes 5 superpositions for the pair (4 from (120,33)
[120 outer, all positions incl root x 33 root] + 1 from (33,120) [33 outer,
proper subterms x 120 root]); WM forms 8 (cp 37094,37130,37265,37343,37471,
37579,37580,37581, all in rule-126's ADD batch -- NOT re-overlaps).  The 2-var
CP (cp 37130) is a **126-OUTER** superposition (rule 36's root into a PROPER
SUBTERM of rule 126's LHS), so it belongs to thvm's (120,33) call (4 of WM's
~N 126-outer).  thvm misses it there.

FIX LOCUS: the FT position walk `cp_visit` in `thvm_critical_pairs_pair_ft`
(_.c overlap path) under-enumerates the OUTER rule's proper-subterm positions,
OR the FT unifier fails on the NON-LINEAR case (x1 occurs twice in rule 126's
LHS -- root arg AND deep subterm; the 2-var superposition unifies 36's LHS with
the deep `dot(dot(x3,x2),x1)` and must bind the repeated x1 consistently).
REFINED (read src/atp/ft_cp.c): `ft_cp_walk_positions` (ft_cp.c:340) visits ALL
non-var positions of the outer rule recursively to CP_MAX_DEPTH (>> the depth-3
positions here), calling `ft_unify(sub, lj_renamed)` at each via `ft_cp_visit`
(ft_cp.c:290).  The 66 byte-identical theorems all pass through this exact path,
so the walk+unify are generically correct => the miss is a NARROW edge case in
`ft_unify` / `ft_cp_replace_subst` / `ft_unify_apply` for this specific 126x36
superposition, NOT a depth/coverage miss.
NEXT (decisive): EITHER instrument ft_cp_visit (gate a flag in atp_overlap_ij
when {i,j}=={120,33}; log per-position p + ft_unify pass/fail + produced
cp_lhs/cp_rhs) to find the failing/wrong-result position, OR write a minimal
unit test (add rule 126 + rule 36; call thvm_critical_pairs; assert the 2-var CP
appears) for a fast isolated repro.  Fix in ft_cp.c; verify vs matrix (66
byte-identical hold, Meredith firstdiv past 6078).

## (CONFOUNDED, superseded by the ROOT CAUSE above) equation-retention finding

The "thvm 15 vs WM 39 equations" and "8 vs 36 equation-overlap formations" below
are **full-run counts** -- thvm's run is 35891 picks vs WM's 64800, so thvm
naturally has fewer of everything; the comparison is confounded by run-length,
NOT established as the @6078 cause.  Verified: the @6078-selected 2-var CP in
WM's heap is `CP(126, 36)` w2=23993 -- **both RULES (rule x rule), not an
equation overlap**.  rule 126 = `dot(x1,dot(dot(x2,x3),dot(dot(x3,x2),x1)))
-> dot(x1,x1)`; rule 36 = `dot(x1,dot(dot(x1,x2),dot(x2,x3))) -> dot(x2,x1)`.
And thvm forms MORE rule x rule 2-var CPs than WM (53 vs 44), so it is NOT
under-forming rule x rule.  => @6078 is most likely a formation-TIMING gap (thvm
forms the 126x36-analog 2-var CP at a different cp_seq, so it is absent/consumed
at exactly pick 6078), not a missing-equation gap.

## OBSTACLE: WM multi-counter numbering blocks clean correlation
WM uses several independent counters -- ElternNr (KP identity, shown in heap
`CP(a,b)`), heap w2 (++CPNr classification age, the selection tie-break),
SUE-add age (a different counter in `added to SUE: w,age`), and rule-orient order
(`added as new rule N`).  These do NOT coincide (verified: the 3-var CP's
SUE-age 24441 != any weight-120 heap-w2), and parent numbering may even differ
between the instrumented ELProver and the reference wmcli.  This has blocked
cleanly mapping WM's @6078 formation event to thvm's facts across ~16 iterations.
UNBLOCK CANDIDATE: add a WM-side per-classification dump (ElternNr, w1, w2,
parent ElternNrs + parent TERMS, CP term) at NewClassification, giving one
consistent map; then correlate the w2=23993 CP to its parents' terms and check
thvm's enumeration of that exact rule x rule overlap.

## (CONFOUNDED -- see correction above) thvm keeps fewer unoriented equations

Decisive counts (faithful WM trace + thvm BATCH orientation tally):
* WM keeps **39 equations** total; **19 distinct** of them produce the 2-var CP.
* thvm uses only **~15 distinct unoriented facts** as CP producers total, and only
  **6 distinct** produce the 2-var CP.

So thvm's orientation/equation-retention criterion KEEPS ~2.6x fewer unoriented
equations than WM -- thvm orients (or subsumes away) equations WM retains as
bidirectional E-set members.  Those missing equations never emit their
equation-overlap CPs in thvm, including the 2-var-CP instances WM forms in the
6078 epoch -> the @6078 heap lacks the 2-var CP.  (Consistent with prefix
matching to 6077: the extra equation CPs are higher-weight and not selected
until 6078.)

This is FAITHFUL-FIXABLE with ground truth (ELProver is faithful for Meredith):
the divergence is in the KBO orientation criterion (when is an equation
orientable?) and/or equation subsumption.  WM keeps a KBO-incomparable equation
unoriented (bidirectional); thvm likely orients some of these (or its eq-subsume
drops them).  NEXT: pick one WM-kept equation (e.g. a -11/-12/-13 term), find
the analogous thvm fact, and confirm thvm orients/drops it where WM keeps it;
then port WM's orientation/retention criterion.  Verify against the matrix.

## Open question (next step)

Identify the rule newly added around WM age ~23993 whose overlap produces
`dot(dot(a,a),b) # dot(b,dot(b,a))`, and check whether thvm (1) adds the
analogous rule at the analogous point and (2) enumerates that specific
rule x rule overlap.  Most likely a producing-rule LIFECYCLE divergence: the
rule whose overlap yields this CP is interreduced/deleted earlier in thvm than
in WM (same removal family as SKIToBCKW @1868), so thvm stops forming the CP
while WM keeps replenishing it.  Trace: `THVM_ATP_CPGEN_DEBUG=1` -> grep the
`[cpgen] from rules X x Y` lines whose normalized cp is the 2-var CP, watch the
producing (X,Y) pairs stop appearing around cp_seq 15233.

## CORRECTION (2026-06-17, path): the matrix uses the NON-FT cp_visit, not ft_cp.c

The default `bin/test_atp_wolfram_bench` (what the matrix/align uses) is built
WITHOUT THVM_ATPFT_UNIFY, so `ft_cp.c` is not even compiled -- atp_overlap_ij
routes through the NON-FT path `pf[skip1] = {thvm_critical_pairs_pair,
thvm_critical_pairs_pair_noroot}` -> cp_visit in **src/cp/_.c**.  So the earlier
"fix locus = ft_cp.c" was the wrong path; the bug is in the non-FT cp_visit.

Added a gated non-FT trace: THVM_CP_TRACE_I/J + g_cp_visit_trace (src/cp/_.c
cp_visit logs per-position thvm_unify pass/fail).  For the slot-120 x slot-33
pair it showed (120,33)[outer, incl root] = 5 positions all unify=1, and
(33,120)[proper] = pos[1] unify=1 but pos[1,1]/[1,1,1] unify=0.

CAVEAT (new confound): slot numbers are REUSED after rule removal+backfill, so
gating the trace by slot {120,33} may catch DIFFERENT rules than the 126/36-
analogs at trace time (the (33,120) unify=0 at a position that should unify for
the 36-analog hints at this).  Slot-based parent attribution (incl the cpgen
"from rules X x Y") is therefore unreliable; only TERM-anchored findings are
solid: thvm misses forming the 2-var CP TERM in the 6078 epoch (CPFORM gap
15233->44563) while WM forms it.  NEXT: gate the cp_visit trace by the rule
TERMS (not slots), or add parent-TERMS to the cpgen/CPFORM log, to reliably pin
the missed overlap; then fix cp_visit/thvm_unify; verify vs matrix.

## MAJOR REFRAME (2026-06-17, confound-free unit test): likely NOT a thvm bug

Added a confound-free isolation test (tests/test_atp.c
"atp/meredith-6078-126x36-superposition"): construct rule126 + rule36 by their
TERMS, call thvm_critical_pairs (FULL enumeration -- both directions, all
positions, no slots/counters/run/slot-reuse).  Result: **14 CPs, NONE is the
2-var CP** f(f(a,a),b) # f(b,f(b,a)).  Hand-verified the standard superpositions
(36[1] x 126 -> thvm CP8; 126[1,1] x 36 -> another) -- none yields the 2-var.

=> The 2-var CP is NOT a standard LHS x LHS critical pair of rules 126 and 36 as
extracted.  thvm's CP-gen (cp_visit/thvm_unify) is CORRECT here (consistent with
the 66 byte-identical theorems that exercise this exact path).  So Meredith @6078
is most likely NOT a thvm CP-generation bug.

WM's cp 37130 ("parents 126 and 36", RAW = 2-var) therefore arises from a
mechanism that differs from the standard 126x36 superposition: candidates --
(i) WM's parents 126/36 are INTERREDUCED VARIANTS (different terms than at
"added as new rule 126/36"); (ii) a non-standard overlap (WM superposes where
thvm correctly does not, e.g. an equation/unfailing face if 126 or 36 is treated
as an E-equation); (iii) re-overlap after rule modification.  This re-opens
whether byte-parity here is even the right target (WM may form a CP standard
completion does not).

NEXT: find WM's ACTUAL formation of cp 37130 -- dump rules 126/36's terms AT
cp-37130-time (not add-time) from the instrumented ELProver, and check whether
the 2-var is a standard CP of THOSE terms; if still not, WM uses a non-standard
overlap and the divergence is WM-side (document, likely unfixable faithfully
without reproducing WM's exact mechanism).

## UNRESOLVED CONTRADICTION (2026-06-17) -- prior (b) reframe was PREMATURE

Per-position trace on the clean unit test (g_cp_visit_trace=1 in
thvm_critical_pairs(126,36)): thvm VISITS ALL 5 non-var positions of rule 126
([], [1], [10], [11], [110]) and all of rule 36 -- complete position coverage,
14 CPs, none the 2-var.  Verified rules 126/36 are added ONCE (no
re-orientation/interreduction before cp 37130), so cp-time terms == add-time
terms == the unit-test terms.

CONTRADICTION: (i) thvm's COMPLETE standard enumeration of 126x36 lacks the
2-var; (ii) WM forms the 2-var as a 126x36 CP (cp 37130); (iii) WM's overlap
"filters" (EinsStern/Nusf/Kern/Back via KPFilterErgaenzen) only RESTRICT
generated overlaps, so WM's CPs MUST be a subset of the standard overlaps.
(i)+(iii) => the 2-var is NOT standard; (ii)+(iii) => it IS standard.  Both
can't hold.  My previous "(b) thvm is correct, not a bug" reframe was therefore
PREMATURE -- the conclusion is currently UNRESOLVED (I have flip-flopped a<->b).

The resolution requires WM-side overlap instrumentation: instrument WM's
CP-formation (Unifikation1.c U1_KPsBildenZuRegel / the KPAction path) to print,
for cp 37130, the exact Vater, Mutter, VaterStelle (position), face, and
unifier.  That reveals whether WM used (1) a standard 126x36 position thvm's
thvm_unify handles differently [=> thvm_unify bug], (2) a non-standard face/
generation [=> port it], or (3) a parents attribution that is not a direct
126x36 superposition [=> the gap is elsewhere].  Until then, @6078's root cause
is genuinely UNDETERMINED.

## RESOLVED-AS-WM-SIDE (2026-06-20): drought is a CP-formation MULTIPLICITY divergence, not a thvm bug

A confound-free, WM-trace-free investigation of the three thvm-determinable
levers (KBO self-consistency, equation subsumption, producing-overlap lifecycle)
finds **no thvm-side defect**.  The drought is a genuine CP-formation
order/multiplicity divergence that needs WM's formation sequence to close; it is
NOT thvm-determinable.  Detail below; instrumentation is in-tree (gated).

Ground truth re-confirmed on current HEAD (cf8aa3bb): thvm forms the target T
`(C3 (C3 V0 V0) V1) # (C3 V1 (C3 V1 V0))` (w=10) EXACTLY 16 times at cp_seq
{741, 1274, 1287, 5594, 5660, 5661, 6401, 6568, 6782, 6942, 7222, 10739, 14707,
14711, 15228, 15233}, last at 15233, then a gap to 44567.  Pick 6078 (seq ~23171)
sits in [15233, 44567], so the pri=120 band genuinely lacks T at 6078.  AndAssoc
and OrAssoc are byte-identical here.

### Lever 1 -- KBO orientation is SELF-CONSISTENT (no orient-despite-incomparable)

Added a gated probe `THVM_ATP_ORIENT_KBO_CHECK=1` (src/atp/_.c orient site): when
thvm orients a fact (r_orient==1), it re-runs the memo-free Baader-Nipkow oracle
`thvm_kbo_naive` on (lhs, rhs) and prints `ORIENT_KBO_INCONSISTENT` if the oracle
does not agree it is strictly GT.  Across MeredithAxioms__And/OrAssociativity
(6200 picks each) and BooleanAxioms__Noncontradiction / McCuneAxioms__Associativity
/ HillmanAxioms__Commutativity: **0 inconsistencies**.  thvm never orients a
KBO-incomparable fact, so the unfailing-completion "keep incomparable facts as
bidirectional E-equations" rule is satisfied.  (Note: orientation IS the KBO call
-- atp_compare == KBO_GT -- so this also confirms the production comparator agrees
with the naive oracle on every oriented Meredith fact, i.e. no memo/cache hazard.)

### Lever 3 -- no T-producer is RETIRED or SUBSUMED

Every producing rule of the 16 T-formations stays LIVE through the drought.
RULE_TRACE (`RULEADD` + `RETIRE` events) over the whole run: 23 RETIREs, NONE of
them a T-producer term.  thvm even keeps T ITSELF as an unoriented bidirectional
equation (RULE 28, trace 1203: `C3(C3(x_0,x_0),x_1) -> C3(x_1,C3(x_1,x_0))`,
unorientable) and re-derives T from its overlap (seq 1287:
outer=T-equation x inner=commutativity).  The 16 T-CPs are all `joinable=0`
(none dropped as joinable; queue-subsume is OFF in the preset).  So neither
orientation nor subsumption nor joinability removes a T producer.

### overlap_exhaust is INNOCENT

The atp-wm-overlap-exhaust feature (`use_overlap_exhaust`, ON in the preset) only
blocks `flat-transposition x exhausted-equation` re-overlap; T's producers are
not flat transpositions.  `THVM_ATP_NO_OVERLAP_EXHAUST=1` leaves the T count at
16 and @6078 unchanged.

### Producing overlaps of the 16 T-formations (provenance, slot-alias-safe)

Captured via the extended `THVM_ATP_CPGEN_DEBUG=1` trace (now appends parent
rule TERMS + overlap position per CP).  The 16 fall into producer cohorts, each a
DIFFERENT freshly-added fact's add-batch overlapping the existing set:

* seq 741/1274/1287  -- early rules (slot2 commutativity, slot17, slot28=T-eq)
* seq 5594..7222     -- slot56/57 outer x {slot18,26,47,59,60,61,62,63} inner
* seq 10739          -- slot50 x slot70
* seq 14707..15233   -- slot64/65/66 x slot87/89 (the w=8 rules
  `C3(C3 V0 V0)(C3 V1 (C3 V1 V0))->...` and `C3(C3 V0 (C3 V0 V1))(C3 V1 V1)->...`)

After slot89's batch (~seq 15233) the rules thvm adds (slots 90..148) are
progressively deeper; NONE of their add-batch overlaps produces the small w=10 T.
The next T producer is slot149 (`C3(C3 V0 (C3 V0 V1)) (C3 V0 (C3 (C3 V1 V1) V2))
-> V0`), whose batch fires at seq 44567.  thvm's architecture forms each fact's
superposition lane ONCE (thvm_atp_generate_cps_wm: overlap f vs all current rules
+ f-as-inner into old facts, then done -- WM-faithful), so a producing fact is
not re-overlapped; T reappears only when a NEW fact whose lane yields T is added.

### The precise drought-cause EVENT

There is no single removal/orient/subsume event.  The drought is the GAP between
two T-producing add-batches: the slot64/65/66 x slot87/89 batch (last T at seq
15233) and the slot149-family batch (next T at seq 44567).  In that window thvm
adds ~60 rules (slots 90..148) none of whose lanes produce T.  WM, over the same
trajectory, forms T ~80 times (the prior section's faithful WM tally: ~36 with an
equation parent) and keeps replenishing the pri=120 band, so it still has a T-CP
to select at pick 6078.

### Why this is WM-side (not thvm-determinable) and STOP

The producing facts are identical-and-alive in thvm; thvm's orientation is
KBO-self-consistent; nothing thvm subsumes/retires/exhausts removes a T producer.
The only remaining difference is WM's CP-formation MULTIPLICITY/ORDER -- how many
distinct (parent x partner x position) overlaps WM forms for T and in what age
order.  Pinning thvm's missing equation-overlap instances to WM's would require
WM's exact per-classification formation sequence in the 6078 epoch (a
NewClassification.c dump), i.e. a WM trace.  Per the directive, NO wmcli/ELProver
was run.  thvm-side: NO defect found; no faithful single-criterion fix advances
@6078 without breaking a byte-identical baseline (orientation/retention changes
are global).  CONCLUSION: @6078 is a CP-formation multiplicity divergence,
WM-side, deferred -- same family as the residual 12 deep-AC-completion-multiplicity
rows in the WM selection-sequence matrix.

In-tree scaffolding (both env-gated, default-OFF, baselines byte-identical):
* `THVM_ATP_ORIENT_KBO_CHECK=1` -- orient-despite-incomparable self-check.
* `THVM_ATP_CPGEN_DEBUG=1` -- now also prints each CP's producing parent rule
  TERMS + overlap position (slot-alias-safe provenance).
