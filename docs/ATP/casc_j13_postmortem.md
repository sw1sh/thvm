# CASC-J13 and ProoVer 2026: post-mortem

Competition ran 2026-07-27. Sources: `tptp.org/CASC/J13/` realtime results
(Division Summary, Detailed Results) and `proover-competition.github.io`.

## Results vs what we predicted

| Division | Our prediction | Actual | Place |
|---|---|---|---|
| UEQ (unit equality) | 265/300 = 88%, "beats Vampire 263" | **269/400 = 67%** | 4th of 10 |
| FOF (first-order form) | ~143/500 = 28.6% | **91/400 = 22.8%** | 11th of 17 |
| ProoVer (proof verification) | validated clean on our corpus | **-110/150** | 10th of 10 (last) |

UEQ podium: Vampire 5.0.1 370, Vampire 5.0 322, Twee 2.7 286, FindProof 269,
then CSI_PP 258, E 3.5.1 255, Prover9 208, iProver 198, Drodi 163, mrs 76.

FOF: Vampire 5.0.1 381, Vampire 5.0 328, E 264, CSI_PP 264, iProver 250,
Drodi 204, LisaST 199, Zipperpin 192, Prover9 2026-6A 143, Prover9 1109a 92,
FindProof 91, then mrs 79, SUPr 73, ConnectPP 64, VIP 26, SATResetCoP 20,
SPASS-SCL 14.

ProoVer: GAPT 114, VaLeaDate 97, Norgler 93, ProofCheck 67, ProofGuard 55,
mrs-proover 37, PyCheck 19, GDV 2.0 -1, GDV-LP -5, CheckProof -110.

## 1. UEQ: the engine was fine, the forecast was not

The result itself is respectable. 4th in a division where the top two entries
are Vampire, ahead of CSI_PP, E, Prover9, iProver and Drodi.

The forecast was wrong for two compounding reasons.

**We benchmarked against a frozen, year-old bar.** Our measurement document
compared us to CASC-30 (2025) numbers: Vampire 263, Twee 258. We beat those.
But competitors shipped: Twee went 2.6.0 to 2.7 (258 to 286), and Vampire
fielded 5.0.1 at 370. Measuring a freshly-tuned system against last year's
frozen scores is not a prediction, it is a comparison against systems that no
longer exist.

**We tuned on the test set, and it shows on the problems we did not tune
on.** We swept and dissected CASC-30's exact 300 UEQ problems for weeks,
flipping specific named problems. J13 was not a fresh selection: 232 of its
400 UEQ problems were in CASC-30's 300 and 59 more in CASC-28's (corrected
2026-09-29; this section first said "400 freshly selected problems"). Solved
counts per subset, from the official results table:

| J13 subset | n | Vampire 5.0.1 | Vampire 5.0 | Twee 2.7 | FindProof | CSI_PP | E 3.5.1 | Prover9 2026-6A |
|---|---|---|---|---|---|---|---|---|
| in CASC-30 | 232 | 220 | 192 | 175 | **176** (3rd) | 154 | 152 | 120 |
| in CASC-28 only | 59 | 54 | 53 | 51 | **41** (5th) | 42 | 41 | 36 |
| in neither | 109 | 96 | 77 | 60 | **52** (6th, tied) | 62 | 62 | 52 |
| all | 400 | 370 | 322 | 286 | **269** (4th) | 258 | 255 | 208 |

The two gaps separate cleanly. On the problems we tuned against we tied Twee
(176 vs 175). The whole 18-problem deficit to Twee sits in the other 168,
where E and CSI_PP also beat us: that is the overfitting. Separately, the
tuned-on set measured 266/300 (89%) before the competition at 240s wall,
without `--proof`, on pods; its 232 problems that recurred in J13 scored 176
(76%) under competition conditions. That drop is the deployment gap, not
overfitting.

**The hardware caveat was real and under-weighted.** We documented that the
measurement pods ran 16 slices at full speed while StarExec runs 16 threads on
8 physical cores, and called 265 "a slight upper bound". It was optimistic by
more than "slight". The measurement protocol since then: 180s wall, `--proof`,
a Solutions pass under strict CheckProof, and hardware calibration against the
submission build's official times.

## 2. FOF: over-predicted a division we knew was weak

Predicted 28.6%, delivered 22.8%. Both Prover9 entries beat us, including the
1109a build from 2009. This is consistent with our own written assessment
(an equational completion engine translating FOF, not a native saturation
prover) and with the decision not to invest there. The engine result is not a
surprise; the estimate being 25% high is the error.

## 3. ProoVer: a self-inflicted catastrophe

This is the one that deserves real attention. Exact confusion matrix, recovered
from the per-problem detailed results (100 problems, sums to exactly -110):

| gold | our verdict | count | points |
|---|---|---|---|
| valid | VerifiedGood | 30 | +30 |
| valid | VerifiedBad | 12 | -12 |
| valid | abstain | 8 | 0 |
| invalid | VerifiedBad | 26 | +52 |
| **invalid** | **VerifiedGood** | **18** | **-180** |
| invalid | abstain | 6 | 0 |

The 18 invalid proofs we accepted as valid are the entire result. They cost
-180; everything else we did netted +70. **Had those 18 been abstentions, the
score would have been +70 and 4th place** instead of last.

### Root cause: the implementation inverted its own design intent

Our own project notes state the requirement correctly: "the checker must be
CONSERVATIVE (abstain to NotVerified unless every step confirmed)". The shipped
checker did the opposite. It was confidently wrong 30 times (18 false accepts
plus 12 false rejects) and abstained only 14 times. A checker that abstains
less often than it errs is not conservative.

Three contributing factors, all verifiable in our tree:

**The penalty asymmetry never reached the code's mental model.**
`tools/proof_check.c` documents its fallback as "the conservative fallback is
Unknown (0 points), never a wrong VerifiedBad (-10)". But -10 is the penalty
for a wrong *VerifiedGood*; a wrong VerifiedBad costs only -1. The design was
guarding the cheap mistake and leaving the expensive one open. (The arithmetic
in `wl/Examples/proover/synth/score.py` was right; the C design comment and one
label string carry the inversion.)

**Validation was a closed world we authored ourselves.** The public sample set
was 7 proofs. Beyond that we validated against 80 self-generated proofs from 4
toy scenario families (witness, horn, trans, oneex) and 4 bug classes we
invented (unentail, neg, sk_absent, sk_reuse). We tested our checker against
the bugs we had already thought of, using proofs written in our own rendering
of the annotation schema. The competition used 100 proofs from real provers
with bugs chosen adversarially by the organizers.

**The blind spot is clustered, not diffuse.** The 18 accepted-bad proofs are
PRV031, 036, 038, 039, 046, 047, 049, 050, 051, 052, 053, 054, 058, 083, 084,
092, 093, 095. The run PRV046 through PRV054 is 8 near-contiguous failures.
That pattern indicates one or two specific bug families we systematically fail
to model, not general weakness. Finding that family is the highest-value single
action available.

**We also declined the tool the rules offered.** ProoVer explicitly permits
invoking an external ATP to discharge unspecified inference steps. We chose an
internal-only backend. That choice traded away the one mechanism that turns an
unverifiable step into a decided one, and it shows on both error axes.

## What worked and should be kept

**Zero unsound results in CASC.** In both divisions, Solved equals Solutions
(269/269 in UEQ, 91/91 in FOF): every problem we claimed, we produced a
verified solution for. Several rivals lost ground here (iProver 250 solved but
248 solutions, Drodi 204/203). The full-TPTP soundness sweep and the three
unsoundness fixes we landed did exactly their job. That discipline was correct
and is worth preserving.

The irony is precise and worth stating: we ran an exhaustive soundness campaign
on the prover so it would never claim a false proof, and simultaneously shipped
a checker that accepted 18 false proofs.

## Actions, in priority order

1. **Obtain the official ProoVer 2026 problem and proof set, run CheckProof
   against it, and diagnose the PRV046-054 cluster first.** One bug family
   plausibly accounts for half the -180.
2. **Invert the verdict policy to abstain-by-default.** VerifiedGood must
   require positive confirmation of every step. Any step not positively
   confirmed forces Unknown. Fix the -10 / -1 inversion in the
   `tools/proof_check.c` design comment and the `score.py` label so the code's
   documented incentives match the real ones.
3. **Use the external ATP hook for unspecified steps**, as the rules allow.
4. **Re-validate on real prover output**, not self-generated proofs: harvest
   TSTP proofs from Vampire and E, mutate them, and hold out a set never used
   for tuning.
5. **For CASC forecasting: stop predicting from frozen prior-year scores, and
   hold out problems.** Any future estimate should be measured on problems we
   have never dissected, and should assume competitors improve year over year.

## Addendum (2026-07-28): the checker rebuild

Actions 1-4 are done (worktree proover-rebuild).  The official PRV
problem+proof set is NOT yet published (the site's last commit predates the
competition; proofs appear "shortly after" per the proceedings) -- re-check
https://proover-competition.github.io/ and tptp.org/CASC/J13/.  In its place,
the blind spots were diagnosed by auditing the shipped checker and CONFIRMING
each hypothesis against the old binary with crafted counterexamples.

**False-accept taxonomy (what -180 was made of).**  The shipped checker never
opened the problem file: every file()/introduced() leaf was trusted wholesale,
and unparseable records were silently dropped.  Five invalid families all
verified as VerifiedGood on the old binary: (a) leaf formula differs from the
named problem axiom, (b) introduced() smuggling of an arbitrary formula,
(c) a plain step dodging its check by citing file(), (d) a wrong-conjecture
import (prove something easier than the problem's conjecture), (e) a
corrupted record the parser dropped while the remainder verified.  All are
one mutation family at heart -- LEAF LIES -- which is consistent with the
near-contiguous PRV046-054 cluster being one organizer generator.  The 12
false rejects: record order was assumed topological (the rules allow any
order) and esa steps were judged by entailment, so real-prover output
(cnf_transformation containing skolemization) drew genuine countermodels and
became VerifiedBad; the old binary even rejected FindProof's own valid
SEU140+2 solution.

**The rebuild** (tools/proof_check.c, strict profile = CheckProof binary):
leaves verified against the loaded problem file (semantic mismatch =
VerifiedBad, benign variant = abstain); unread records cap the verdict at
Unknown; duplicate names / citation cycles rejected structurally; parent
resolution order-independent; countermodels refute only thm/cth claims;
Skolem freshness is DAG-cone-aware; multi-symbol skolemisation and
introduced(definition) chains confirm positively; unsettled thm/cth steps
discharge through the external-ATP hook (this binary re-exec'd as prover,
per the rules: the step and its premises only).  FindProof --check keeps the
legacy trusted-leaf profile for internal certification.

**Validation** now runs on real prover output: harvested FindProof+Vampire
TSTP proofs over CASC-J13 problems, mutated by a spec-derived battery
(tools/proover/openworld/), scored with a 30% held-out slice whose hard
requirement is zero false accepts.  The old synth corpus is kept as a
regression floor (138/138).

## Addendum (2026-09-28): scored against the published set

The organizers published the 100 PRV proofs on 2026-07-28 (hours after
the rebuild concluded they were unpublished), then withdrew 9 and
re-scored out of 138: CheckProof's official result is -103/138, still
last (GAPT 111). Exact gold labels were recovered from the ten systems'
per-problem scores with zero conflicts.

**The rebuild scored -1/138** on the set (8 false accepts, 14 false
rejects). **The leaf-lie taxonomy was right for PRV046-054 only**: all of
those are caught by the rebuilt leaf check. The other 8 false accepts
(PRV031, 038, 058, 083, 084, 092, 093, 095) were a different family --
violations of the SPECIFIED skolemize rule and of the refutation
structure, most of them semantically harmless:

- PRV038/058: `skolemize` with status `thm` (or `esa` AND `thm`);
- PRV083/084: the eliminated variable is universal in polarity;
- PRV092: two parents; PRV093: no `skolemize(Var, T)` annotation;
- PRV095: the witness replaces a binder that had no occurrence;
- PRV031: an `assume` step cites the conjecture as a premise.

The 14 false rejects were checker bugs: a shadowed binder nest
conflated by the clausifier (PRV001), `$false` as a parent read as an
ordinary atom (PRV030), vacuous skolemizations flagged "symbol absent"
(PRV010-016, PRV096), and a Skolem-freshness rule that counted
instantiation at the Skolem constant (and a tautology mentioning it) as
a second introduction (PRV019/022/076/089).

**After the fixes: +137/138 as a DEV metric** (47/47 invalid caught,
43/44 valid confirmed, PRV043 abstains), 0 false accepts on the set and
on the open-world held-out slice. Auditing the new code against its own
soundness argument found more false-accept holes the old binary shared
(instantiate binder capture, conjecture/descendant demodulators,
predicate abstraction, definitions with free body variables); crafted
counterexamples are battery cases H10-H18. The held-out gate then caught
one more: the equational routes confirmed steps with UNCITED file-prior
records and axioms as demodulators (the rebuild accepted H20). The
strict profile now judges every step by its stated premises only, as
the rules require of the external ATP -- which also makes FindProof's
own under-cited records (equality-encoding hyper_resolution steps that
omit the ifeq axioms, CSR021's p674 that folds in plus-demodulation)
VerifiedBad: genuine missing premises in FindProof's proof output, and
the open-world corpus's "valid" label on those proofs is wrong under
the ProoVer semantics.

Lesson, again: the -10 cases were not the family we modelled. What
worked this time was reading every error with the proof in hand, and
deriving each fix from the rules text rather than from the case.

