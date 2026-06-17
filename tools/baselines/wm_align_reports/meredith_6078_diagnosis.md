# Meredith AndAssoc @6078 -- root cause (instrumented-wmcli diagnosis)

Date: 2026-06-17. Branch: main (worktree wm-parity).

## Headline

The @6078 divergence is **NOT an emission-order bug** (the class all 12 prior
fixes addressed).  It is a **CP-generation completeness gap**: at pick 6078
thvm's CP heap is *missing* a weight-120 critical pair that Waldmeister has.

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

NEXT: enumerate thvm's 5 slot-120x33 overlap positions vs WM's 126x36 set; find
the missing position; inspect why atp_overlap_ij skips that superposition (or
whether WM forms it via rule re-processing thvm lacks).  Verify against matrix.

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
