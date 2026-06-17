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
