# ENIGMA versus the CASC portfolio: plan and pre-registration

Date: 2026-10-01. This plan is active. It replaces the phase gates of docs/plans/atp_neural_selection.md for all ENIGMA work until the CASC 2027 freeze. Related documents: docs/plans/casc_j13_postmortem.md, docs/atp/casc_strategy.md, docs/atp/ml_guidance.md, tools/atp_enigma/README.md.

How this file changes. Each gate here is fixed before the data it governs exists. A later change is a dated amendment, appended to section 5.4. An amendment may tighten a gate, or change a gate whose data does not exist yet. It may do nothing else.

Ground rules:
- Main only. The enigma-lane branch is one commit (b8443afb) on top of main 2185bba8 and adds --lane and --list-lanes. It is merged at M0 and then deleted. ENIGMA work lands on main in small commits from short-lived worktrees.
- Pods only. Every run of FindProof or of a trainer happens on Prime Intellect pods. The Mac edits code, syntax-checks it and reads result files.
- Full focus. No new hand-built lanes for J13 problems while this plan runs. Engine fixes from any track reach every arm, because all arms are one binary.

Summary:
- ENIGMA is the learned layer of the FindProof schedule. Its core is a set of guided twins: copies of the lanes that own the hard tail, whose weight picks go through a listwise head that is conditioned on the lane and trained in on-policy loops.
- Three further components are conditional. They are built only if their pre-gates pass on pool data:
  - learned critical-pair admission;
  - a learned launch plan;
  - machine-invented lanes.
- ENIGMA is compared with NL, the best non-learned portfolio that can be built from the same binary. NL covers three configurations:
  - today's schedule;
  - the schedule with every non-learned fix;
  - the schedule with the twins present but their learning switched off.
  So parity cannot be reached by renaming the portfolio, and hygiene fixes are never credited to learning.
- The primary protocol is the one in the task:
  - the 400 J13 UEQ (unit equality) problems;
  - 180 s on 16-vCPU pods;
  - --proof;
  - only Solutions that pass strict CheckProof count.
  A secondary wall is calibrated to deployment speed: about 53-60 s on a pod, equivalent to StarExec's 180 s.
- J13 is sealed. Its lane-matrix rows and per-problem tables are not read until the final look. Every decision before that is made on the pool. That includes a manufactured band set, DEV-S, which gives the gates the statistical power the pool lacks.
- Honest priors:
  - about 0.2 that ENIGMA is as powerful as NL with a measured learned contribution;
  - 0.15-0.2 that it is better at the deployment-calibrated wall;
  - about 0.07 that it is better at 180 s on pods.
  The plan spends little before it knows. The first kill point is loop 1 of M3 (January 2027). The second is loop 3 (February 2027).

## 1. Target and comparison protocol

### 1.1 Arms

Every arm is the same binary, the competition build (make -f tools/casc/Makefile bin/FindProof). Only the flags differ.

| Arm | Flags (names provisional)       | Content |
|-----|---------------------------------|---------|
| P0  | --schedule                      | today's 46-lane portfolio (SCHEDULE[] in tools/atp_cli/main.c) |
| P1  | --schedule=p1                   | P0 plus every non-learned change that passed its own test in M1: liveness, retirements, memory caps, width |
| PD  | --schedule=enigma --guide-zero  | P1 plus the guided twins with zero-correction heads; each twin is then an exact copy of its base lane (gate G2.2) |
| E   | --schedule=enigma               | P1 plus the guided twins with trained heads, plus every conditional component that passed |
| E8  | --schedule=enigma8              | the standalone reading: only the guided lanes (at most 8), run 8 at a time |

At each wall, NL is whichever of P0, P1 and PD solves the most problems in the same run. Picking NL on the test data makes every comparison stricter for E. At the freeze, the schedule modes that lost are deleted.

### 1.2 Claims (pre-registered; decided only at the final look, L2)

| Claim | Meaning | Test on J13 at L2 |
|-------|---------|-------------------|
| A: as powerful | E is no worse than the best non-learned portfolio, and its learning contributes | At W180: net(E - NL) >= 0, and the 95% CI lower bound >= -delta. Also, at W180 or at WDEP: the 97.5% CI lower bound of net(E - PD) > 0. (97.5% per wall keeps a 5% error rate over the two chances.) |
| B: better | E beats NL under the task protocol | At W180: the 95% CI lower bound of net(E - NL) > 0 |
| BD: better at deployment speed | E beats NL at the calibrated wall and does not lose at 180 s | At WDEP: the CI lower bound of net(E - NL) > 0. At W180: net(E - NL) >= 0 |
| S: standalone | the learned lanes alone match the portfolio | At W180: the CI lower bound of net(E8 - NL) >= -delta |

delta = min(4, max(2, ceil(sqrt(D)))). D is the number of J13 problems on which the two L0 passes of P0 disagree at that wall.

Ship rule:
- E becomes the CASC 2027 configuration if and only if claim A holds and net(E - NL) >= 0 at both walls.
- Otherwise NL ships: P1 if it is no worse than P0 at either wall, else P0.
- Nothing with a negative net at W180 ships.
- Nothing ships on a WDEP result alone.

### 1.3 Protocol

Problems and command:
- Problems: the 400 CASC-J13 UEQ problems (~/.cache/thvm/enigma/matrix/j13_ueq.tgz).
- Command: FindProof --schedule[=MODE] --szs --proof --wall=W problem. It runs under env -i with TPTP set, inside timeout -k 5 (W+15), with stdin from /dev/null.

Walls:
- W180 = 180 s. Primary; this is the task protocol.
- WDEP is secondary. It is the pod wall at which the J13 submission build (~/.cache/thvm/casc/FindProof.tgz), run with its own CASC command line including --proof, best reproduces its official J13 UEQ result of 269 solved.
  - It is measured at M0 from 45, 53, 60 and 70 s.
  - We expect 53-60 s, the StarExec equivalent at the measured 3.0-3.4x speed factor.
- Each wall is its own run, and a WDEP result is never read off a W180 run. The reason is that three things read the wall: the LRS (limited resource strategy) lanes, the Horn release, and the FOF (first-order form) budgets.

Hardware and settings:
- One problem at a time on a 16-vCPU pod, with nothing else running.
- lscpu output (model, threads per core) is logged on every row.
- The pod needs at least 72 GB of RAM. Otherwise every arm runs with THVM_ATP_RSS_ABORT_MB=3500.
- THVM_ATP_SCHED_MAX_PAR=16 is set explicitly in every arm. The binary default is 5, which silently changes the lane set.
- The work clock is the default CPU clock, as at CASC.

Pairing:
- All arms of a comparison run on the same pod.
- The arm order is shuffled per problem with a fixed seed.
- Each pod first runs a fixed set of 10 calibration problems. A pod more than 15% off the reference speed is replaced. Pods differ by about 2x: GRP415-1 took 37.5 s on one pod and 18.1 s on another.

Repeats:
- One pass per arm.
- Two more passes per arm for two kinds of problem: those where the arms disagree, and those whose solving lane uses a clock-gated discipline (--lrs, --cp-set-ir-budget).
- The majority of three decides.

What counts as a Solution:
- A Solution has all three of: SZS status Unsatisfiable; a CNFRefutation emitted within W; a strict bin/CheckProof verdict of VerifiedGood.
- Unknown counts as unsolved in every arm.
- Unknowns are reported per arm and per winning lane, because the Unknown rate depends on the lane: equals-encoding proofs come back Unknown, while narrowq proofs verify.
- If two arms' Unknown counts differ by more than delta, that comparison is reported as inconclusive.
- Any VerifiedBad stops the line until its cause is fixed.

Runner discipline:
- The runner runs from a snapshot copy.
- It has per-job and runner deadlines, and writes sentinel files.
- Results are harvested before any pod is terminated.
- prime pods list is checked at the end of every session.

Simulation may only select. Every claim comes from pod runs under this protocol.

### 1.4 Problem sets and the data rule

| Set   | Content | Use |
|-------|---------|-----|
| TRAIN | The pool's train split (sha256 of the name, mod 100, below 80): 664 attempted problems. Removed: every problem that shares a name stem with a J13 problem, and the half of the hard tail that goes to DEV-T. | training; OOF (out-of-fold) gates |
| VAL   | the existing 15% validation slice of TRAIN, chosen by a second hash | model selection and early stopping only |
| DEV-H | the 171 attempted held-out pool problems | gates; read only at gates |
| DEV-T | The hard tail: held-out problems, plus half of the train problems (by sha256 of the stem), that meet any one of three conditions: the fastest lane needs more than 10 s at 60 s; only the 180 s supplement solves it; every lane that searched it ended on the memory guard. | gates |
| DEV-S | manufactured band problems built from held-out runs (M2); frozen before any ENIGMA arm runs on them | gates; the only set whose band is populated at both walls |
| J13   | the 400 CASC-J13 UEQ problems | evaluation only: looks L0, L1, L2 |
| T-NEW | UEQ Unsatisfiable problems added by any TPTP release after v9.3.1, if at least 30 exist before the freeze | an independent test at L2 |

The data rule is stricter than "the pool minus the J13 names":
- Pool problems that share a name stem with a J13 problem are excluded from TRAIN. 50 J13 problems have such variants, for example COL042-6..9 for COL042-1. 5 of the 50 are in the unsolved residue.
- Hindsight goals (section 3.2) are dropped if they match any J13 conjecture modulo variable and symbol renaming.
- Competitor schedules, command lines and proofs are not used as seeds or labels: Vampire's mined UEQ schedule, E's schedule.vars and the like. They were mined on TPTP including J13.
- Pool problems that share an axiom system with J13 problems stay in TRAIN, as such problems will at CASC 2027. For example, LCL385-10 has the axioms of J13's LCL054/060/375/390/395-10. To show the effect, J13 results are also reported on the axiom-novel subset: problems whose stem and canonical axiom set occur nowhere in TRAIN.
- Synthetic -10 encodings of non-UEQ TPTP problems enter only if the user approves decision D1 (section 6.3).

The J13 seal:
- When the lane matrix lands, its J13 rows go into a sealed archive. The archive's sha256 is recorded in section 5.4. No script opens it before L2, and every analysis script asserts that it reads no j13 row.
- There are exactly three looks:
  - L0 (M0) measures P0 only, with two passes per wall, plus the WDEP calibration.
  - L1 (M5) tests futility and harm only. It cannot establish a claim.
  - L2 (M6) is the confirmatory test.
- At L0 and L1, only the pre-registered aggregates are read:
  - totals, nets and CIs per arm and wall;
  - discordant counts;
  - nets per domain;
  - band counts;
  - Unknown counts.
  Per-problem tables go to the sealed archive and are opened after L2 for the post-mortem.
- No J13 problem is dissected for ENIGMA. Statistics computed from the J13 problem files alone are allowed: live-lane counts from the static gate replica, and axiom-set keys. Outcomes are not.

### 1.5 Statistics and reporting

Every comparison reports:
- the paired net (gains minus losses);
- the exact sign (McNemar) test on discordant problems;
- a 95% bootstrap CI (confidence interval) over problems, clustered by name stem, with 10,000 resamples;
- the net with the best single TPTP domain dropped;
- the per-problem disagreement table: problem, domain, winning lane per arm, times, and the end class of the losing arm's lanes. For J13 this table stays sealed until L2;
- the near-wall band: each arm's solves in (W/2, W], and its misses solved within 2W where a 2W run exists;
- the end state of every miss: memory guard, wall, saturated, immediate exit, or alarm;
- Unknown counts per arm and per lane;
- for J13, results on these subsets:
  - the 232 CASC-30 recurring, 59 CASC-28-only and 109 new problems;
  - the 116 -10 problems against the other 284;
  - the axiom-novel subset;
  - the residue classes;
- the conditions behind every number: wall, pod type, whether --proof was on, and whether the count is of Solutions or of plain SZS solves.

Every DEV gate of the form "net >= +k" also requires a one-sided exact sign test on the discordant problems at p < 0.05. For example, 5 gains and 0 losses passes, as do 8 gains and 1 loss. This keeps a gate from passing on noise.

## 2. Thesis

In one paragraph: hand-built lanes are close to the limit of what dissection can add without overfitting J13. The gap that remains is the -10 class and the problems new to CASC, and that is where learned selection moved Vampire. ENIGMA can beat the portfolio only by changing long searches in the lanes that own the hard tail, raced as extra lanes in slots that win nothing today. It cannot lose solves by displacing lanes, because it takes only those slots, and every non-learned improvement is credited to the comparator. Our own data say the required long-run speedup does not exist yet: we measure 0.97 at 10 s and more, against the 0.75 needed. So the plan is built to find out cheaply. By February 2027 the loops either produce that speedup on a set where it would show, or the core bet stops.

### 2.1 Where the portfolio stands

- Official CASC-J13 UEQ results: FindProof 269 (4th), Vampire 5.0.1 370, Vampire 5.0 322, Twee 286, E 255.
- Current estimates: about 354 at a 180 s pod wall and about 334 at 60 s on pods. They come from the July pod re-sweep of 43 lanes plus the September RNG arm, without --proof, best of three passes. These are upper bounds: no run of current main exists under the protocol, and M0 makes one.
- The hardware-adjusted gap to Vampire 5.0.1 is 36-41 problems.
  - 26 of it is on the 116 -10 problems (Horn problems in the ifeq encoding): 89 against 115 at 60 s on pods.
  - On the other 284 we are at 245 against 255 at 60 s, and at 254 against 255 at 180 s.
- The gap is largest on problems new to CASC.
  - On the 109 J13 problems that were in neither CASC-28 nor CASC-30, we solve 75 at 60 s on pods (83 at 240 s). Vampire 5.0.1 solves 96, Vampire 5.0 solves 77.
  - On the 232 CASC-30 problems the lanes were tuned on, we solve 208 against Vampire 5.0.1's 220.

### 2.2 Why more hand lanes will not close it

- About 25 J13 solves come from 9 lanes added after J13, each built by dissecting a J13 or CASC-30 miss. That is tuning on the test set, and it shows: at J13 we tied Twee on the 232 tuned problems (176 against 175) and trailed it by 18 on the other 168.
- Searching the configuration space was audited empty twice, on 2026-07-18 and 2026-07-20. Most post-J13 flips needed a new mechanism, not a recombination of existing knobs: the Otter loop, tgt=4 naming, LRS, structural dedup.
- The 43 J13 problems that no lane solves are floods, or they need inferences we lack:
  - 14 are -10 problems that only Vampire 5.0.1 solves. The dissected ones flood to about 2M critical pairs, with 94-95% rule retention, and never touch the goal.
  - 15 have existential goals.
  - 8 are AC (associative-commutative) theories.
  - 3 are Moufang loops.
  - 3 are problems almost nobody solves.
- The schedule is saturated.
  - Every J13 problem has 17-28 live lanes (median 21) for 16 slots.
  - bare-gj never starts at t=0. lazy-gj misses t=0 on 339 problems and wm-gj on 295. 10 lanes have never been seen winning.
  - A lane that shares its physical core with a busy hyperthread sibling runs 1.31x slower. tgt-sel on GRP661-1 takes 34.8-35.2 s alone and 45.7-46.4 s in a width-8 schedule on an 8-vCPU pod (~/.cache/thvm/casc/J13/deflake/results/ANALYSIS.txt).
  - These levers are real but not learned, so they go into P1 and are never credited to ENIGMA.

### 2.3 Why a learned layer can add solves at a fixed wall

1. It is what moved the field on these problems. The only search change in Vampire 5.0.1 over 5.0 is neural clause selection. In J13 UEQ it gained 58 problems and lost 10. On the -10 class it scored 115 against 87; on LCL -10, 41 against 27. On the 109 new problems it scored 96 against 77. It was 4-6x faster where 5.0 needed 10 s or more.

2. Twins race in slots that win nothing today.
   - A twin takes a slot freed by retiring a lane that has no pool value and no documented unique solve. So it cannot cost a solve by displacing another lane.
   - It also searches differently from its base: the top-K heads overrode 79-86% of the engine's picks.
   - On the 134 held-out problems both arms solved, the DAgger (dataset aggregation) head took fewer steps on 83 and more on 25. The first head ranged from 5.1x fewer steps (LAT177-1: 9,569 -> 1,865) to 2.4x more (GRP420-1: 2,263 -> 5,532).
   - Raced beside its base, a twin keeps the base's solves and adds the problems on which it is faster. That converts near-wall problems even at a modest mean speedup.
   - At WDEP the J13 band holds 8, 11, 18 and 20 problems for speedups of 1.28, 1.5, 2 and 4.
   - In 'Teaching Vampire New Tricks' (2026), single strategies boosted by 19-30% still gave +13.9% to a 5-strategy union, and +4.1% at full portfolio scale (+12.2% at budgets of about 10 s).

3. Proofs can fit under the memory guard. Our one online conversion so far, GRP441-1 (VerifiedGood, 3 of 3 runs), was a guided proof at 19,612 selections. twee hits the 3.5 GB RSS (resident set size) guard at 20,480. 13 of twee's 37 held-out misses at 300 s end on that guard.

4. Each failure of the 2026 arc has an identified cause and a fix here:

| Failure in the 2026 arc | Evidence | Fix in this plan |
|---|---|---|
| The evaluation pool has no band | held-out: nothing solved between 38 and 300 s; the 13-lane VBS (virtual best solver) needs at most 2.55 s on a 160-problem sample | DEV-S, a manufactured band set; DEV-T; WDEP |
| The step cut fades on long runs | DAgger head steps ratio: 0.82 on twee solves over 1 s (n=20), 0.88 over 5 s (n=9), 0.97 over 10 s (n=4); phase 0c 2.2x worse than the engine on runs with 1000+ selections | a long-run offline gate at loop 1; run-length weights; layered-lazy integration |
| The top-K ceiling | a proof CP (critical pair) is in the engine's top 32 at only 76% of held-out selections | layered-lazy mode looks past the top 32 |
| Labels came only from the lane's own run | a miss yields no positive; Tier 2 lost 67 problems with no rows | positives from every lane's proofs (cross-lane); hindsight goals |
| One DAgger round | the literature runs 3-24 loops, each adding solves | up to 8 loops with temperature diversity |
| One lane, twee, which equals the pool union | twee wins only 5 logged J13 problems | twin families chosen by who owns the pool's hard tail; one lane-conditioned model |
| Data scale and mix | 835 problems, 48% GRP, 9% -10 (J13: 29%) | weights by TPTP rating and run length; hindsight goals; D1 |
| Cost and deployability | the Tier-2 GCN (graph convolutional network) needed a runtime compiler; armed, it solved 73-75 against 136 | hand-written C, weights compiled in, a per-pick budget gate |

No online gate has ever passed:
- Tier 1: 11 = 11.
- Tier 2: 73-75 against 136.
- Phases 0d and 0e: 134 = 134 at 60 s (+1 at 300 s).

The fixes above target the causes. They do not guarantee the effect.

### 2.4 What the learning has to achieve

- At W180, the band holds 1-2 J13 problems: GRP706-1 at 156 s and DAT436-1 at 216 s of pod time.
  - Being better at W180 therefore requires solves that no lane reaches in 180 s, that is, conversions from the residue.
  - No learned component of ours has produced one on J13. This is the long shot.
- At WDEP, a twin must be about 1.3x faster than every other lane on near-wall problems.
  - Concretely: a selections ratio of at most 0.75 on runs with 1000+ selections, in the families that own those problems.
  - Today's head reaches 0.97 on runs of 10 s and more.
  - Closing that gap is the M3 bet, and loop 3 shows whether it is happening.
- The per-pick cost must stay below 5% of a lane's time.
  - twee spends about 1.44 ms per selection, a budget of about 70 us. Today's head costs 35-113 us per pick.
  - WM-family (Waldmeister) lanes run up to 20-27k selections per second, a budget of about 2 us.
  - Twin families are therefore chosen by step rate as well as by which lanes own the tail.

### 2.5 Expected outcome

The components compete for the same band problems. These are joint estimates, not sums.

| Quantity                     | W180                            | WDEP      |
|------------------------------|---------------------------------|-----------|
| net(E - NL), central         | +1                              | +3        |
| 80% interval                 | -1 to +3                        | -1 to +8  |
| P(claim holds)               | A: 0.2; B: 0.07; S: below 0.03  | BD: 0.15-0.2 |

The largest upside we can see is D1:
- The -10 class holds 26 of the 36-problem gap, and it is where Vampire's neural selection gained most.
- The pool has 74 -10 problems (13 LCL -10). J13 has 116 (42 LCL -10).

If every learned component is killed, the year still produces:
- P1, the measured non-learned fixes;
- the first deployment-faithful baseline;
- the signal-teardown fix;
- the lane matrix;
- training and evaluation infrastructure that any later attempt reuses.

## 3. Architecture: what ENIGMA is at the end

### 3.1 Inside FindProof

```
FindProof --schedule=enigma problem.p
  parent : parse once -> problem features phi(p): gate bits (existential
           goal, ifeq dispatcher, definitions, ground compound in an axiom,
           conjecture-cone coverage, distributivity shape), AC pairs, size
           buckets.  One function also serves --problem-features, so
           training and deployment see the same numbers.
  launch : the P1 order (or the learned plan table, if M4b passed); every
           slice keeps the remaining-wall rule; refill on exit; the first
           proof wins, as today.
  lanes  : the P1 lanes, flags unchanged
           + guided twins enigma-<base> = <base flags> --guide=<family>,
             in the slots P1 freed, placed so they start at t=0
           + invented lanes (if M4c passed)
  in a twin, at every weight pick (FIFO and age picks untouched):
           selection layer (src/atp/guide.c) -> the chosen CP
  in memory-bound families, at every push (if M4a passed):
           admission layer (src/atp/freezer.c) -> queue or freeze
  weights: static const float arrays generated into the binary
```

phi(p) holds only what a deployed problem carries.
- The TPTP rating and SPC header fields, which build and weight the training sets (section 3.2), are printed after phi(p) as meta_* columns with the layout version. They are never selection inputs: deployed problems have no header, and the SPC status is the label.
- Size features are floor(log2(1 + n)) buckets, so the layer needs no fitted normalization.
- The parent computes phi(p) in a forked child under a 1 s CPU budget. A problem past the budget runs without the selection layer instead of delaying the lanes.

The selection layer generalizes src/atp/topk.c.
- Inputs for each candidate:
  - the engine's rank, the queue size and the CP's priority (phase 0b showed that the signal is relative to the queue);
  - the CP's push features, cleaned as the phase 0d audit asked: f4, a copy of f0, is dropped; the raw sequence number f8 is replaced by the number of selections since push; the nominal symbol ids f9/f10 are replaced by goal-symbol overlap;
  - CP depth, queue growth and the RSS fraction;
  - phi(p) and the lane id.
  Features are normalized per problem.
- Model: a listwise MLP (multi-layer perceptron) with one hidden layer of at most 128 units: s = -kappa log1p(rank) + correction. The correction starts at zero, so loop 0 reproduces the engine exactly (gate G2.2).
- Integration mode, chosen per family on VAL:
  - top-K, with K at most 64, as today;
  - layered-lazy: score only the engine's next candidate and accept it if it scores above a threshold; otherwise demote it and score the next one, with at most 64 evaluations per pick.
- Either mode can hand every r-th weight pick to the plain engine. It does this through its own counter, not through the w2_modulo branch, which scans the whole heap on every such pick.
- In lanes with --lrs, the layer never changes the key that LRS ranks by; demotion uses a side key.
- Cost: per-pick time is counted and reported, and must stay at or below 5% of lane wall by gate.

The admission layer (conditional, M4a):
- A parental score built from the two parent rules plus push features, about 1k multiply-accumulates per CP.
- A CP that scores below the threshold is frozen as a compact descriptor (parents, position), and its trace entry is deferred.
- Frozen CPs are revived when the live queue empties, so completeness holds.
- It is not used in otter lanes. There every queued CP is a passive demodulator, so freezing would change the calculus.

The launch plan (conditional, M4b):
- A class table or k-nearest-neighbour table over phi(p), compiled in.
- A gated lane whose gate fires keeps its t=0 slot.

Invented lanes (conditional, M4c):
- Flag strings over the existing knobs, found by automated search on the pool.
- Admitted only after soundness and proof checks.

### 3.2 The training loop (tools/atp_enigma, on pods)

Harvest:
- Uses --lane=NAME with history export v2.
- Export v2 keeps the observation-only 64-byte records, and adds a lane tag and records of rule content.
- It decodes no IC-heap terms, because FindProof has no collector.
- Runs use --proof, plus --work-clock=instr for the clock-gated lanes.

Labels:
- the trace-DAG ancestors of the records that the emitted proof cites (np0_extract.py);
- cross-lane positives: canonical equation keys from other lanes' proofs of the same problem, used only between lanes that share the same input transform;
- hindsight positives: the derivation ancestors of rules derived late in failed runs.

Trainer:
- guide_net.py, grown from the train2 step of topk_net.py.
- A listwise loss over the live candidate set, with positive weighting.
- Problems are weighted by TPTP rating and run length, not by the J13 family mix.
- 5-fold OOF models over TRAIN, with early stopping on VAL.

Loops:
- Loop 0 is the existing D32h64 DAgger head on twee, and the engine itself in every other family.
- Loop k harvests with the head armed at temperatures 0, 1/9 and 1/3, with 2 seeds, then retrains warm-started.
- At most 8 loops.

Export:
- safetensors are converted into generated C arrays.
- A C/Python parity tool, grown from the parity and featparity steps of topk_net.py, checks them.

### 3.3 Shipping constraints

- Hand-written C loops only. No BLAS (basic linear algebra subprograms), no runtime compiler, no dependence on a tensor-to-C exporter. Gather and scatter are on main, but nothing here needs them.
- Weights are plain static const float arrays. Const-object initializers broke clang 14 before (c59b353b).
- The code builds with gcc, with clang 14, and in the static delivery build.
- Unarmed, every hook is one predictable branch, and the engine is byte-identical.
- Weights total at most 2 MB, because every lane is a separate process.
- At the freeze:
  - the schedule modes that lost are deleted;
  - the J13-contaminated Tier-1 and Tier-2 corpora and the unloadable F=6 assets are retired;
  - the WL Method -> "ENIGMA" preset points at the shipped configuration.

## 4. Milestones

Dates are targets. Gates and kill criteria are fixed as written. Each analysis script is committed before the run it judges.

### M0 Truth (2026-10-01 to 10-28)

Deliverables:
1. Merge enigma-lane into main and delete the branch. Then fix SCHED_PID_MAX, writing the test first:
   - Today it is 32 for 46 lanes (tools/atp_cli/main.c), so slices 32-45 escape the signal handler when the parent is killed from outside.
   - The fix sizes it from N_SCHED, with a static assertion.
2. The lane matrix harvested, validated and sealed, and the pool tables built (section 5).
3. A 180 s supplement: every pool problem that no lane solves at 60 s is rerun single-lane, on every lane that searched it.
4. An RSS composition census and a memory oracle:
   - The census is an observation-only breakdown printed at a memory-guard abort: packed CPs, trace buffers, simpler-join peaks, slot arrays, term heap, indices.
   - The oracle reruns the pool's memory-guard misses single-lane at 4x the cap, with THVM_HEAP_CELLS raised so that the 2^28-cell term heap does not bind first.
5. Hardware measurements:
   - lscpu topology on every pod type;
   - a contention factor per width: 40 TRAIN problems, each with its live lanes, at widths 1, 8 and 16 on a 16-vCPU pod;
   - the WDEP calibration.
6. L0:
   - P0 on J13 at W180 and WDEP, two passes each. The two passes give the A/A noise floor.
   - P0 on DEV-H and DEV-T, two passes each.
7. The label harvest: every TRAIN (problem, lane) pair the matrix proves is rerun with --proof and history export.

Gates (data validity, pre-registered):
- G0.1: Rows exist for at least 98% of the 56,810 matrix jobs. Crashes plus killed-by-timeout make up at most 1%. Every kernel OOM (out-of-memory) victim is found and rerun.
- G0.2: The two L0 passes of P0 disagree on at most 12 J13 problems at each wall. If they disagree on more, the noise source is found first. delta is recorded per wall.
- G0.3: Some wall in {45, 53, 60, 70} s reproduces the submission build's official solved set with at least 95% per-problem agreement and a total within 8 of 269. That wall is WDEP.
  - If no wall qualifies, WDEP = 180/c, with c the geometric-mean time ratio, and every WDEP number is labelled uncalibrated.
- G0.4: A 46-lane run killed by SIGTERM leaves no FindProof process behind, and no run in a 20-problem dry run ends later than W + 10 s.

Kill: none. This is measurement. A failed gate blocks M1 to M6 until it is fixed.

### M1 P1, the best non-learned portfolio (10-19 to 11-20)

Deliverables: candidate non-learned changes. Each gets its own paired test on DEV-H plus DEV-T at both walls.
- Liveness:
  - equals, narrowq and unfold test their exit conditions before the Horn release. Today they hold a slot for up to HORN_WALL_SEC = 2.5 s on every problem with an ifeq dispatcher, which is 106 of the 400 J13 problems.
  - fof-* forks are skipped on CNF (clause normal form) input.
- Retirements. A lane is retired only if all three hold:
  - it has no unique pool solve at 60 s or at 180 s;
  - it adds nothing to a greedy cover of the pool;
  - its comment in tools/atp_cli/main.c documents no unique solve on any CASC problem.
  On today's evidence the candidates are wm-gj, lazy-gj, bare-gj, mnf-gj-amw, tgt (dominated by tgt-fold) and ifeq.
- Memory caps: a per-slice RSS cap derived from MemTotal. This is considered only if the memory oracle converted at least 3 pool problems. J13 gave each run 128 GiB, of which 16 x 4.5 GB uses 72 GB.
- Width below 16 is considered only if both hold:
  - the contention measurement shows a median per-lane speedup of at least 1.3x, with a 90% CI lower bound of at least 1.15;
  - the simulated loss at W180 on DEV is zero.
  Narrower widths have cost solves before. SYN615-10 timed out at width 8 and was solved at width 29. RNG027-8 was starved at width 4.

Gate: a change enters P1 if and only if its paired net is at least 0 at both walls, with at most 1 loss.

Kill: none. If nothing passes, P1 = P0. P1 is frozen at the end of M2; after that, only engine fixes shared by all arms change it.

### M2 Substrate and data (10-26 to 12-18)

Deliverables:
1. src/atp/guide.c: the selection layer, the zero-correction mode, and both integration modes.
   - The --guide, --guide-zero and --guide-model knobs.
   - The generator that turns weights into C.
   - tests/test_atp_guide.c. Today topk.c and history.c have no unit tests.
2. History export v2 for every candidate family, with history hooks added where they are missing. src/atp/otter.c has none.
3. The pipeline for cross-lane and hindsight labels, run over the M0 label harvest.
4. DEV-S:
   - Source runs: held-out problems, on lanes that run on the untransformed input (wm, twee, bare and similar), at 180 s with export v2.
   - Candidates: rules oriented in the second half of the run, 5-60 symbols long, not an instance of an axiom. Each is skolemized into a ground goal.
   - Filter: drop any candidate that matches a J13 conjecture modulo renaming. Keep at most 10 per axiom system.
   - Measurement: each candidate runs under P0 at WDEP and at W180, and at twice each wall. It is kept if solved in (W/2, 2W].
   - DEV-S is frozen before any ENIGMA arm runs on it.
5. If D1 is approved, the synthetic -10 corpus:
   - produced by tools/atp_cli/fof_prepass.c, which reproduces TPTP's -10 ifeq scheme for the Horn fragment;
   - J13 names and stems excluded in every division;
   - split 80/20 into TRAIN-X and DEV-X.

Gates:
- G2.1: Export v2 only observes. On 7 problems per family, with export on and off, the selection sequence, the proof and the stats are identical (step clock, amendments A3 and A4).
- G2.2: Zero correction is exact. On 8 problems per family, a twin with zero-correction weights is byte-identical to its base lane: same selections, proof, stats and LRS prune counts. This makes PD a pure duplicate and isolates the effect of learning.
- G2.3: Unarmed builds are byte-identical. test_atp passes in full (136835/136835 on b8443afb), the OA pin is 257971/735/2461555, and the DN pin is 2848/254/768876.
- G2.4: C and Python agree to 1e-9, with the same argmax on 8 problems per family.
- G2.5: DEV-S is valid if all three hold:
  - it has at least 60 band problems at WDEP and at least 30 at W180;
  - they come from at least 15 axiom systems;
  - P0's A/A disagreement on DEV-S is at most 5%.
- G2.6 (amendment A7, section 5.4): the synthetic -10 corpus is valid. It governs only deliverable 5; G2.3 covers the encoder's effect on unarmed runs.

Kill:
- A family that fails G2.1 or G2.2 is not guided.
- If G2.5 fails, M3's online gates move to DEV-T and DEV-H and use a selection-budget metric: guided and unguided runs are capped at the same number of selections. The statistical power of those gates is computed and written into section 5.4 before any M3 result exists.

### M3 Guided selection, the core (2026-12-01 to 2027-03-19)

Deliverables:
- The twin families, chosen by the rule in section 5.3.
- Loops 0 to L, with L at most 8.
- Loop 0 includes an in-schedule run of the existing D32h64 head as a twee twin on DEV. This gives a first E-versus-PD reading at no training cost.
- Per-loop diagnostics: proof-pick rate on the head's own states, selections, RSS, end states.
- OOF models and per-pick cost counters.
- enigma-<base> lanes in SCHEDULE[], off by default until M5.

Gates, per family:
- G3.1 (loop 1, offline, by 2027-01-08). Evaluated on the states the family visited in its first armed harvest (VAL, OOF). Both must hold:
  - The top-1 proof-CP hit rate is at least 1.8x the engine's, with a bootstrap lower bound of at least 1.4.
  - On states from runs with 1000+ selections, the median-percentile ratio is at most 0.80, with a CI upper bound below 1.0. Phase 0c was 2.2x worse than the engine there.
- G3.2 (loop 3, online, by 2027-02-12). A single lane on TRAIN OOF plus DEV-T plus DEV-S at WDEP. All three must hold:
  - The geometric mean of selections, on common solves with 1000+ base selections, is at most 0.75, with a bootstrap upper bound below 0.90. Today it is 0.97 at 10 s and more.
  - The median per-pick cost is at most 5% of lane wall.
  - The net on DEV-S is at least +5, with the sign test.
- G3.3 (in schedule, by 2027-03-19). E against max(P1, PD) on DEV-H plus DEV-T plus DEV-S. All four must hold:
  - net at WDEP is at least +4, with McNemar p < 0.05;
  - net at W180 is at least 0;
  - net on DEV-T alone is at least 0;
  - no lane's median launch time moves later by more than 10 s.

Kill:
- K3a: G3.1 fails in every family after one revision. In-lane selection stops (January 2027).
- K3b: after loop 3, either the long-run selections geomean is above 0.85 in every family, or the single-lane DEV-S net is at most 0 in every family. In-lane selection stops (February 2027).
- K3c: no loop runs beyond 8. Stop early when two consecutive loops add less than +1 on VAL.
- A family that fails is dropped; the others continue.

### M4 Conditional components (pre-gates decided from M0 data by 2026-11-13; builds until 2027-02-26)

M4a Learned admission (the freezer)
- Pre-gate. Both must hold:
  - the memory oracle converts at least 3 distinct pool problems (VerifiedGood);
  - the census attributes at least 60% of RSS growth to queued-CP storage on at least two thirds of memory-guard aborts.
  If the oracle converts but the term heap or the trace buffers dominate, the lever is a non-learned cap or heap policy for P1, not ENIGMA.
- Build:
  1. First, a non-learned freeze: CPs beyond the LRS reach horizon are frozen instead of deleted, and revived when the live queue empties.
  2. Then, the learned admission head.
- Gate. All of these must hold:
  - proof-CP recall at the threshold is at least 99.5% on OOF solved runs;
  - the frozen share is at least 70% of queued symbol volume;
  - on the pool's memory-guard misses (OOF plus held-out), single lane at W180, it converts at least 3 more than unarmed and at least 2 more than the non-learned freeze, with the sign test;
  - it loses at most 1 of the family's 60 s solves;
  - certify mode finds 0 mismatches;
  - Satisfiable smoke problems still saturate.
- Kill:
  - If the pre-gate fails, there is no freezer.
  - If the learned head fails, the non-learned freeze goes to P1, provided it passes its own test.

M4b Learned launch plan
- Pre-gate (analysis only). All three must hold:
  - on TRAIN plus DEV-T, a simulated per-problem oracle plan with P1's shape beats simulated P1 by at least +5 at WDEP;
  - at least 15 pool problems are solved by exactly one lane;
  - on band problems (VBS time in (W/4, 4W)), the simulator matches live P1 with at least 95% per-problem agreement.
  We expect this to fail: on the pool sample, twee alone equals the union of the 13 lanes.
- Build:
  - The table excludes each problem's own rows and its stem-mates.
  - Short slices are launched with the full wall and stopped at their cap, because LRS lanes read the wall.
- Gate: a live test against P1 on stem-disjoint DEV problems. The net is at least +3 at WDEP (with the sign test) and at least 0 at W180.
- Kill: if the pre-gate or the gate fails, there is no learned scheduler.

M4c Invention pilot
- Pre-gate: at least 25 TRAIN Unsatisfiable problems rated below 1.0 that no lane solves at 60 s or at 180 s.
- Build:
  - 2 rounds of about 2,000 probes over knob strings from tools/atp_cli/knobs.c, centred on the UEQ lanes;
  - each probe runs 20 s under --work-clock=instr;
  - a per-problem local search that prefers default values;
  - no competitor seeds.
- Admission requires all three:
  - 0 false Unsatisfiable claims, on the 261 Satisfiable pool problems and on tests/soundness;
  - VerifiedGood (equals-encoding proofs that come back Unknown do not count);
  - 2 of 2 reproductions, both under the instruction clock and under the CPU clock in the competition build.
- Gate:
  - at least 8 new TRAIN solves and at least 2 held-out solves that no lane reaches, with no tuning on held-out problems;
  - then an in-schedule test against P1: net at least +2 at WDEP (with the sign test) and at least 0 at W180.
- Kill: fewer than 5 new TRAIN solves after 2 rounds, or after 1,500 vCPU-hours.

### M5 Composition and the interim look L1 (2027-03-22 to 04-16)

Deliverables:
- E = P1 plus the components that passed. A twin or invented lane stays only if a regularized greedy cover on DEV selects it in at least 60% of 50 stem-grouped half-splits.
- On DEV, the chain P0 -> P1 -> PD -> E, with each step's contribution reported. Also K*: the smallest width at which guided lanes alone match NL on DEV.
- PD built.
- J13 at both walls, with arms P0, P1, PD and E.

Gate (futility and harm only; L1 cannot establish a claim). Continue if and only if all three hold:
- net(E - NL) is at least -delta at W180;
- net(E - NL) is at least 0 at WDEP;
- no TPTP domain loses more than 3 problems.
Only these aggregates are read.

Kill: if the gate fails, E is not the CASC 2027 configuration and NL ships. The L1 per-problem table is then unsealed for diagnosis. J13 is then used up as a test, and CASC 2027 itself is the only test left.

### M6 Freeze and the final look L2 (2027-05-03 to 06-11)

Deliverables:
- The frozen build, with --schedule=enigma as the default in tools/casc/starexec_run_default and the weights compiled in.
- The delivery battery passed: the soundness battery, 0 false claims on the 261 Satisfiable problems, and builds with gcc, clang 14 and the static build.
- Between L1 and L2, only bug fixes and the steps written here.
- L2 on J13 with arms P0, P1, PD, E and E8, plus E without invented lanes if E has any. Both walls, with repeats as in section 1.3.
- T-NEW, if it exists.

Gates: claims A, B, BD and S exactly as defined in section 1.2, and the ship rule there.

Kill: NL ships instead of E if E's net against NL is negative at W180, or if E falls more than delta below its L1 result at either wall.

After L2:
- The J13 matrix rows and every per-problem table are unsealed for the post-mortem.
- The user decides D2.

### Summary

| Milestone           | Dates              | Gate                                         | Kill |
|---------------------|--------------------|----------------------------------------------|------|
| M0 Truth            | 10-01 to 10-28     | G0.1 to G0.4 (validity)                      | none; blocks the rest |
| M1 P1               | 10-19 to 11-20     | per change: net >= 0 at both walls, at most 1 loss | none (P1 = P0) |
| M2 Substrate, data  | 10-26 to 12-18     | G2.1 to G2.6                                 | family not guided; DEV-S fallback; no -10 corpus |
| M3 Guided selection | 12-01 to 03-19     | G3.1 (Jan 8), G3.2 (Feb 12), G3.3 (Mar 19)   | K3a (Jan), K3b (Feb), K3c |
| M4a Freezer         | pre-gate by Nov 13 | recall; conversions beyond the non-learned freeze | pre-gate or gate |
| M4b Launch plan     | pre-gate by Nov 13 | oracle >= +5; 15 single-solver problems; simulator fidelity | pre-gate or gate |
| M4c Invention       | pre-gate by Nov 13 | >= 8 TRAIN and >= 2 held-out new solves      | under 5 after 2 rounds |
| M5 L1               | 03-22 to 04-16     | futility and harm only                       | NL ships |
| M6 L2 and freeze    | 05-03 to 06-11     | claims A, B, BD, S                           | NL ships |

Budget:
- People: about 30 agent-weeks.
- Compute: about 500-800 pod-hours, or $200-320 at $0.40 per 16-vCPU pod-hour, capped at $1,000.
- A J13 look with 4-5 arms at both walls costs about 35-45 pod-hours, about 3 hours on 12-15 pods.
- The binding limits are the 16-pod team cap, shared with the NN and ATP sessions, and the rule against running heavy agents in parallel.

## 5. First concrete step: the lane matrix

What is running now:
- 56,810 jobs in 6 shards: 1,235 problems x 46 lanes, each a single lane at a 60 s wall, without --proof, on the default CPU clock.
- The binary is sha256 d3946ff1, built from b8443afb.
- It was launched on 2026-10-01 from about 14:00 UTC on Nebius pods:
  - shards 2, 5 and 6 on 16 vCPU / 64 GB pods at width 16;
  - shards 1 and 3 on 8 vCPU / 32 GB pods at width 8;
  - shard 4's pod is not yet logged.
- Each runner stops launching jobs after 25,200 s.
- All 46 lanes of a problem are in one shard, so lanes compared on the same problem ran on the same pod. Times across shards come from two pod types and two widths.
- Both pod types hold more memory cap than RAM: 16 x 4.5 GB = 72 GB on 64 GB, and 8 x 4.5 GB = 36 GB on 32 GB. A row that looks like a memory-guard stop may really be a kernel OOM kill.
- The matrix has no --proof, so its times exclude proof emission. It informs choices and never decides a claim.

### 5.1 Now, before the rows land

1. Commit this file. It is the pre-registration.
2. Write and commit tools/atp_enigma/matrix_tables.py. It only processes text, so it may run on the Mac like the other read-only analyses. It has two modes:
   - --seal moves every row with split j13 into ~/.cache/thvm/enigma/sealed/j13_matrix.tsv.gz and prints the sha256 for section 5.4.
   - --tables asserts that no row has split j13, then writes the tables listed in 5.2 step 4.
3. Merge enigma-lane and delete the branch. Then fix SCHED_PID_MAX, writing the test first. The running binary is already recorded and is not affected.
4. Prepare the next pod jobs so they start as pods free up, one heavy job at a time:
   - the submission-build calibration on J13 at 45, 53, 60 and 70 s;
   - L0;
   - the generators for the supplement and for the label harvest.

### 5.2 When the rows land

1. Harvest before terminating anything.
   - From each pod, copy out/shard_K.tsv, its .started and .done sentinels, .logs/ and the runner log.
   - Save dmesg -T filtered for "oom" and "killed process", and save lscpu.
   - If a shard's .done reads status=deadline (likely for the width-8 shards), resume it on its pod; the runner skips rows it already has.
   - Terminate a pod only after its files are on the Mac, then check prime pods list.
2. Validate against G0.1:
   - every shard has as many rows as jobs;
   - the binary sha in .started matches on every shard;
   - crashes plus killed-by-timeout are at most 1%;
   - kernel OOM victims are matched to their rows and rerun at a width whose caps fit in RAM.
3. Seal J13: run matrix_tables.py --seal and record the hash. From this point the working copies hold only train and held-out rows.
4. Build the pool tables, separately for TRAIN and for held-out:
   - T1, per lane: problems proved; problems proved by that lane alone; problems proved within 1, 3, 10, 30 and 57 s; counts per end class.
   - T2: the VBS curve, the best single lane, and greedy k-lane covers for k = 1 to 16.
   - T3: the problems no lane solves, grouped by how their lanes ended.
   - T4: the hard tail, meaning problems whose fastest lane needs over 10 s, or where every lane that searched hit the memory guard. Also the owner of each tail problem (its fastest lane).
   - T5: median selections per CPU second for each lane, from the steps and cpu_s columns. This sets each lane's per-pick budget.
   - T6: lane redundancy (Jaccard similarity of solved sets), and the lanes that are fastest on no problem.
   - T7: the static gate replica against the fast-exit class. A lane the replica calls dead must never prove a problem.
   - T8, per lane: memory-guard ends among unsolved problems, and the maxrss distribution.
5. Apply the rules in 5.3, and write their outputs into section 5.4 before any ENIGMA run.
6. Launch these jobs in order, one heavy job at a time:
   1. the WDEP calibration with the submission build;
   2. L0: P0 on J13 at W180 and WDEP, two passes each, using a current-main binary that includes the PID fix;
   3. the 180 s supplement;
   4. the label harvest;
   5. the DEV-H and DEV-T baselines.

### 5.3 Decision rules fed by the matrix (pre-registered)

| Decision | Rule |
|---|---|
| P1 retirement candidates | A lane qualifies only if all three hold: no unique pool solve at 60 s (and at 180 s, once the supplement is in); no marginal value in the pool cover; no documented unique solve on a CASC problem in the lane's main.c comment. |
| Twin families | Rank base lanes by how many TRAIN tail problems they solve fastest. Take the top 4 whose median time per selection is at least 20x the head's measured per-pick cost. |
| DEV-T and TRAIN-T | Use the hard-tail rule of section 1.4. Split the train tail problems by sha256 of the stem. |
| Supplement list | Every pool problem no lane solves at 60 s, run on every lane whose class on it was not fast-exit. |
| Label-harvest list | Every TRAIN (problem, lane) pair proved at 60 s. |
| M4a inputs | Memory-guard misses per lane; the list of oracle reruns. |
| M4b pre-gate | The simulated oracle plan against simulated P1 on TRAIN plus DEV-T; the count of problems solved by exactly one lane. |
| M4c pre-gate | TRAIN Unsatisfiable problems rated below 1.0 that no lane solves at 60 s or at 180 s. |

### 5.4 Registry and amendments

| Item                         | sha256 or value | Date |
|------------------------------|-----------------|------|
| tptp_pool.tgz                | f3db85ff0268595c741369192ec76c4078b6a39680ca06ef94a724d4c941ce86 | 2026-10-01 |
| matrix binary (b8443afb)     | d3946ff1a965a9be4678f932a6abeec697768a690feb34b4f0e5d723cd2af5c0 | 2026-10-01 |
| sealed J13 matrix rows       | 005488de020d08293a2dd70c54d8fb4ed274c1932ddac22f3fd53078fd072864 (18,400 rows) | 2026-10-02 |
| matrix_tables.py             | dfb13207ae82afa7cb3b443abe3b1aac170f601d2d5fc94d55ace422859ba475 (627aabb2) | 2026-10-02 |
| WDEP (G0.3)                  | 60 s, G0.3 PASS (2026-10-05): the submission build (bin 65abe487887f) on the 400 J13 problems at 45/53/60/70 s, 4 shards, pod check against calib_ref f1172af0, every run pinned to CPUs 0-15 of a 32 vCPU nebius EPYC-Genoa pod. Aggregates: 45 s 273 solved (agreement 0.9750), 53 s 274 (0.9725), 60 s 276 (0.9775), 70 s 276 (0.9775), official 269; all four qualify, 60 s has the highest agreement. Speed factor c about 3.5. Overruns: about 110 runs per wall end after W (the submission build predates the front-end wall stop of 4e95fa95/500ac413). Sealed per-problem table sha256 b1a7b260d4feb6689323b9f66f48e5a3856d18ad823d7b66f9f8f7060abdd4a0 | 2026-10-05 |
| L0 (G0.2), delta per wall    | G0.2 PASS (2026-10-05). P0 = main 7633572f --schedule, 400 J13 UEQ, two passes, strict CheckProof Solutions; one 32 vCPU nebius EPYC-Genoa pod, slots pinned to CPUs 0-15 and 16-31 run in sequence (run_arms holds a pod-wide lock). W180: 320 / 320 Solutions, D = 2, delta = 2, Unknown 14. WDEP 60 s: 306 / 305, D = 1, delta = 2, Unknown 12. Sealed archives L0_1.tgz efb5e522089c148a..., L0_2.tgz 2d14749a4181784f... (sealed/L0/) | 2026-10-05 |
| G0.4 (amendment A1)          | at the pod gate: script and binary sha256, main commit, exit code of each mode | |
| protocol fingerprint (run_arms.py; amendments A11, A19) | efeb738a5185 | 2026-10-02 |
| calib_ref.tsv (pod-speed reference) and its pod's lscpu line | f1172af032ba5af32d9c70c2171843868a48d85bbc56e0e5e0925bafe9ef3dba (10 of 14 candidates, 3 passes, ref 3.10-43.59 s; GRP750-1 43.59 s, GRP754-1 43.08 s, as on the A18 EPYC 9654 pod). lscpu: "AMD EPYC-Genoa Processor, tpc=2, cpus=32, cps=16, sockets=1, aff=16, quota=none"; bin_sha 65abe487887f (the submission build), rss cap default. Pod atp-m0-1, nebius cpu-d3 32 vCPU / 128 GB (MemTotal 131,896,592 kB): no 16-vCPU type offers 16 x 4500 MiB (all are 64 GB), so every WDEP run is pinned by affinity to CPUs 0-15 (8 cores x 2 threads, the nebius 16-vCPU shape), the effective-CPU reading of run_arms.py and the README pod rules | 2026-10-02 |
| L2 unseal token | after the freeze (stats.py --look L2 needs it) | |
| section 5.3 outputs          | ~/.cache/thvm/enigma/matrix/decisions_5_3.txt; summary below | 2026-10-02 |
| twin families (amendment A24) | tgt-sel, encompass, twee-deep, unfold-tgt-awr (section 5.3's rule over TRAIN-T plus the TRAIN-X hard tail; pooled T5, head 35 us; horn-cd and horn-back ineligible). Replacements, in order: twee, wm, equals-twee, unfold, unfold-tgt, flatten-bare, bare, ifeq, tgt, bare-raw, flatten-bare-raw, lazy-gj. matrix_tables.py --xtwins (sha256 9bc6a525cce6e49a...) over matrix.tsv 1f3470d7dfa3b975..., train_t.tsv 6061641e745d437e..., dev_t.tsv 4468a73697631ecf... and xmatrix.tsv debca97bbc66865e...; decisions_a24.txt c57fd633620a98d1... in ~/.cache/thvm/enigma/ifeq10/matrix/. Supersedes the 2026-10-05 set (unfold, twee, flatten-bare-raw) | 2026-10-07 |
| matrix_tables.py with the supplement (amendment A20) | 635db1a233160573123930407dc9240dba18216d59a312866036d4d046a5b89a (f312921c) | 2026-10-05 |
| 180 s supplement             | PASS (2026-10-05, amendment A20): 2,882 of 2,882 rows, missing 0, duplicate 0; supplement.tsv 8fe13fa79483de41b438a52f4726473e56a1c59d0862902aa8111ae7292f8e1a, validate.txt 99008d4d9ceda907..., decisions_5_3.txt 059b52467d4a4c82.... Binary 11fad6a4cb9452f51bcf0544072fdb306878723538234e32293a6ebdd31f9350: FindProof competition build (gcc 11.4.0) of main e2775bab, the same on every slot. Runner f86f43c7..., RUNNER_WALL=180, RUNNER_TIMEOUT=195, width 14 per 16 vCPUs, all six .done status=complete, pending_left=0. Pods (nebius CPU_NODE): atp-sup-1 b17e7a7f (32 vCPU / 128 GB, EPYC-Genoa) ran slots 1 and 2 pinned to CPUs 0-15 and 16-31, then slot 6 on CPUs 0-15 after slot 1 ended; atp-sup-2 1854b21c (16 / 64, EPYC-Genoa) slot 3; atp-sup-3 355da847 and atp-sup-4 472e932f (16 / 64, Intel Xeon Gold 6338) slots 4 and 5. End classes: proved 6, wall 2133, memory-guard 602, gaveup-saturated 97, step-cap 38, crash 6 (0.21%; all six are SYN986-10.005/.006 on bare, bare-raw and tgt-sel, SIGSEGV as at 60 s: the deep-term fix is not on main). 8.9 pod-hours | 2026-10-05 |
| label harvest (M0 deliverable 7) | DONE (2026-10-05): every pair of label_harvest_jobs.tsv (d42d33cb918d0f26..., 8,574 TRAIN pairs on 500 problems; unchanged by the 180 s decisions, which add no 60 s TRAIN solve) ran `FindProof --lane=NAME --szs --proof --wall=60` with THVM_ATP_HISTORY_EXPORT at rate 1 (no CPFEAT) and --work-clock=instr on the 37 clock-gated lanes (A19's reading of `--list-lanes=flags`; perf counters checked on the pod), then np0_extract.py (b72b36c2..., max_neg 100000) and the raw export deleted. Binary 11fad6a4cb94... (gcc 11.4.0, main 1a8dbf1c, the supplement's binary). 8,574 of 8,574 rows, missing 0, duplicate 0; index.tsv bb430a0aee1ac6f1... in ~/.cache/thvm/enigma/labels/ with one npz per pair (1.27 GB). Proved 8,556; npz rows 77,145,377; proof-citation positives 297,165; 8,499 pairs carry positives. Not proved under the harvest's clock: 18 (15 GaveUp at 57-59 s, whose npz hold hindsight labels only; 3 Timeout with no closed session and no npz). Proof withheld by FindProof's own check although proved (no proof block, no labels): 22 (13 'is not derived from what it cites', 9 'no goal reaches $false'; 4 reproduced with the export off, so not the export). The 188 front-end pairs of the matrix are not label-free: in all 188 the export holds an engine session, 176 label in proof mode (255 positives) and 12 hindsight only; frontend_pairs.tsv e728f1c7ab65ffdf... lists them, plus 4 narrowq pairs with no selection at all. Pod atp-lab-1 1f63a491 (nebius 32 vCPU / 128 GB, EPYC-Genoa), two shards of 4,313 and 4,261 pairs pinned to CPUs 0-15 and 16-31, width 16 each, 8.5 min; 2.16 pod-hours, about $1.7 | 2026-10-05 |
| G2.1 (export v2 observes; A3, A4, A8) | 8 of 9 families PASS, horn-cd INCONCLUSIVE (2026-10-05): tests/atp_history_v2_obs.sh (d021755e2cd0a4ee...) over tests/atp_history_v2_obs.tsv (7e6677c470b0acdc...) on binary 11fad6a4cb94..., steps clock, wall 180, A/A on. All 85 rows PASS (no FAIL, NONDET, UNOBSERVED or WALL); twee, wm, flatten, otter (otter-lpo), tgt (tgt-sel), narrow (narrowq), encompass and flatten-bare-raw judge 7 of 10 PASS each; horn-cd has only its 5 listed rows, below the 7 the rule needs, as A5 expected. g21.summary 6522fe182eb483e6..., g21.tsv 6ae64ece9d225899...; the first launch wrote nothing because the script needs an absolute outdir, and its output is kept beside the valid run | 2026-10-05 |
| engine change: explicit-stack term walks (branch enigma-ftdeep) | not merged; L0 runs on the binary recorded in the L0 binary row below. Corrected by A19: this row first said the WDEP calibration does too, but it runs the J13 submission build (section 1.3) | 2026-10-02 |
| correction to the row above | enigma-ftdeep is merged into enigma-m0m2 in full (364cd50d), not in the three stages below; the m0m2 pod gates of 2026-10-01/02 ran on that build. The stage plan and ftdeep_pod.sh's overhead measurement still decide whether it lands on main | 2026-10-02 |
| L0 binary                    | sha256 at L0    | |
| synthetic -10 encoder (D1)   | tools/atp_cli/ifeq10.c (FindProof --emit-ifeq10), a port of jukebox horn-to-unit [CS18], not tools/atp_cli/fof_prepass.c as M2 deliverable 5 says: fof_prepass encodes for proving (one dispatcher, a false != true goal), while TPTP's -10 files have one dispatcher per type pair and keep the source's goals. A record, not a gate amendment; the user to confirm that it is within D1's wording ("from our FOF front end") | 2026-10-02 |
| synthetic -10 corpus (G2.6; amendments A7, A21) | G2.6 PASS (2026-10-06): all four A7 conditions hold on one binary, 11fad6a4cb9452f51bcf0544072fdb306878723538234e32293a6ebdd31f9350, the FindProof competition build (gcc 11.4.0) of main 6a4b244b, the same sha256 as the supplement's and the label harvest's binary. (1) ALL=1 tests/atp_ifeq10_tptp.sh (e668e669...) ok: cases 11 of 11 exact; survey 158 pairs, exact 152, names 5, nest 1, drift 0 (121 skipped: a J13 stem, a case or no source); verdict equivalent; pass file 099080b34d8932ae.... (2) make_ifeq10_corpus.sh (bfda5dc0...; ifeq10_keys.py a6c2e292..., ifeq10_diff.py 9cfdf136...) over TPTP v9.3.1 (TPTP-v9.3.1.tgz 8e9462d9443a2250..., 26,990 problem files, list e855907c763d9661...): .done status=complete in one launch, 0 alerts, pending 0. Manifest rows 14,132 (13,950 candidates, 182 noheader): emitted 1,860, nonhorn 10,181, j13 822, ueq 505, excluded 278, tptp10 138, memory 69 (MEM_MB 3072), j13dup 53, unit 19, input 15, pooltrain 10; no timeout, crash, sat1 or truncated output. (3) Corpus check ok; j13dup 53; the 400 J13 files keyed (379 distinct keys). (4) TRAIN-X 1,464 problems from 36 domains (1,173 stems), DEV-X 396 from 28 domains (321 stems); 9 of them carry sP definitions. manifest.tsv 0dc99b5bb7921baf..., TRAIN-X/pool.tsv 5f4c20a7339da942..., DEV-X/pool.tsv c0e636bb5ee9453e...; a second check of the data rule on the fetched tables (stems, splits, exclusions, files) found 0 errors. Lists: TRAIN_NAMES 7cf4bbb8... (879 names); exclusions 62413841... (217 held-out names), dev_t.tsv 4468a736..., train_t.tsv 6061641e...; J13 names c43f0c49.... Pod atp-x-1 d4bc4d7c (nebius CPU_NODE, 16 vCPU / 64 GB, AMD EPYC-Genoa), every step pinned to CPUs 0-15 at JOBS=14 (in/run.sh 78a7dcb2..., in/setup.sh fca09ab9...); survey 8 s, corpus 3.2 min; 0.26 pod-hours. The J13 seal held: no J13 problem was run, and no J13 outcome was read or printed. Outputs in ~/.cache/thvm/enigma/ifeq10/ (corpus/, harvest/g26_out.tgz a73e542166cfdada...) | 2026-10-06 |
| matrix_tables.py with the X modes (amendment A22) | a998f34f5b510cd1325822fc748ad651bbe98e92d75dcd2937cbd450ca11de56 (a9a255e1, committed before the run; test_matrix_tables.py 2485a73e..., 9 of 9 on every pod) | 2026-10-06 |
| lane matrix of the synthetic -10 corpus (amendment A22) | PASS (2026-10-06): all six validation items of A22 hold. Rows 85,560 of 85,560 (1,860 problems x 46 lanes; missing 0, extra 0, duplicate 0), 24 slots of jobs.tsv 346e74bab0a599a0..., each in one launch with .done status=complete. Binary 11fad6a4cb9452f51bcf0544072fdb306878723538234e32293a6ebdd31f9350 on every launch: the gcc 11.4.0 competition build of main 6a4b244b, rebuilt on each pod, G2.6's sha256; runner f86f43c7..., width 14 on 16 CPUs (taskset 0-15, nproc 16), wall 60 s, timeout 75 s, default RSS cap, no --proof, CPU clock; the binary's --list-lanes=flags gives A22's groups on all three pods. End classes: proved 25,716 (all Unsatisfiable; no satisfiable claim), fast-exit 43,658, wall 11,557, gaveup-saturated 4,372, step-cap 213, memory-guard 44, crash 0, killed-by-timeout 0. OOM: no OOM-killer line in any pod's `sudo dmesg -T`, no SIGKILL end, so no rerun; 87 rows above the 4.5 GB cap (max 11,031 MB). TRAIN-X (1,464): VBS 1,180 / 1,215 / 1,228 at 10 / 30 / 60 s, 236 unsolved; best single lane twee-deep 972 (wm 918 at 10 s); the greedy cover reaches the union at k = 14 (twee-deep +972, equals-twee +117, horn-cd +58, twee +28, tgt-sel +16, unfold-tgt-awr +13, tgt +8, encompass +5, horn-back +3, unfold-tgt +3, narrowq +2, twee-raw, lazy-gj, equals +1 each); hard tail (fastest lane over 10 s) 48, owned by tgt-sel 9, twee-deep 8, encompass 6, horn-cd 5, unfold-tgt-awr 4, twee 3, wm 2, equals-twee 2 and 9 lanes with 1; condition 3 holds for none. DEV-X (396): VBS 336 / 344 / 346, 50 unsolved; best single lane twee 281; union at k = 8 (twee, narrowq, horn-cd, horn-back, flatten-bare, equals-twee, tgt-fold, unfold-tgt); hard tail 10. The six gated -10 lanes (horn-cd, horn-back, ifeq; equals, narrowq, equals-twee) solve 1,057 TRAIN-X and 305 DEV-X problems together, 140 and 40 of them solved by no other lane; without them the VBS at 10 / 30 / 60 s is 1,033 / 1,070 / 1,088 and 288 / 300 / 306. On TRAIN-X: horn-cd 778 (16 alone), narrowq 650, equals 646, ifeq 599 (0 alone, as on DEV-X), horn-back 374, equals-twee 185 (11 alone, strictly fastest on 84). No lane searched 11 problems (10 TRAIN-X, 1 DEV-X): front-end capacity gates (term side length, clause and variable bounds), and SYN915+1-10 and SYN915-1-10, whose encodings reduce to `tuple != tuple`, which the front end reads as having no negated conjecture. Label harvest: label_harvest_jobs.tsv 1564b8d903f9f8ee..., 19,913 TRAIN-X pairs on 1,228 problems; 8,348 of them are front-end proofs (the Horn release before the engine), and 133 problems have only front-end proofs. xmatrix.tsv debca97bbc66865e..., validate.txt c82c7bc6371f67b8..., tables/summary.txt b8833286837744d7..., the 24 harvest archives' sha256 in results/archives/SHA256SUMS.harvest. Pods (nebius CPU_NODE, 16 vCPU / 64 GB, $0.40 an hour): atp-x-1 7af2621f (Intel Xeon Gold 6338) ran 13 slots, atp-x-2 5e4bfe1b (AMD EPYC-Genoa) 9, atp-x-3 e02ce94d (AMD EPYC-Genoa) 2; of the 2-minute acquisition rounds from 09:33Z, 221 found no offer of 16 vCPUs or more listed and 3 created a pod, so at most three pods ran at once, two for most of the run. Slots took 1,776-2,636 s (mean 2,245 s); the jobs used 205 CPU-hours; 16.2 pod-hours, about $6.5. Every pod was terminated only after its slots and its records were on the Mac; prime pods list shows no atp pod left. Times across slots mix two CPU types; all lanes of a problem ran on one pod. Outputs in ~/.cache/thvm/enigma/ifeq10/matrix/ | 2026-10-06 |
| TRAIN-X label harvest (amendment A23) | DONE (2026-10-07): every pair of label_harvest_jobs.tsv (1564b8d9..., 19,913 TRAIN-X pairs on 1,228 problems) ran `FindProof --lane=NAME --szs --proof --wall=60` with the export at rate 1 and --work-clock=instr on the 37 clock-gated lanes, then np0_extract.py (b72b36c2...) and the raw export deleted; binary 11fad6a4cb94... (G2.6's and A22's). summarize.py (45b359cf...): all validation items PASS (19,913 of 19,913 rows, missing 0, extra 0, duplicate 0; one launch per slot, width 14 on 16 CPUs; clock instr on exactly the 37 gated lanes; no Satisfiable claim; no OOM-killer line; no J13 stem). Proved 19,811; labelled in proof mode 18,691; npz rows 186,642,678; proof-citation positives 359,744 (2.99 GB of npz). Twin families of A24: tgt-sel 651 labelled pairs (15,724 positives), encompass 862 (16,349), twee-deep 869 (13,775), unfold-tgt-awr 129 (8,710). Hindsight only 966, front end with no engine selection 122 (of the matrix's 8,348 front-end pairs, 8,208 export an engine session), none 74, extraction failures 0. Not proved under the harvest's --proof and clock: 102 (GaveUp 58, Timeout 44; horn-cd most often, matrix about 45.7 s, harvest GaveUp at 57 s). Proof withheld by FindProof's own check although proved: 56 (the export-off recheck withholds it again in 52 and finds a proof in 4, three on CPU-clock lanes and bare-gj on the instruction clock, where the export's own work moves the clock; 'is not derived from what it cites' and 'no goal reaches $false'), listed in withheld.tsv. Outputs in ~/.cache/thvm/enigma/labels_x/: index.tsv 64959fb402851ce0..., summary.txt 6e2b19cafb9e7ffc..., frontend_pairs.tsv 1d964da60efb8a9c..., withheld.tsv fde20eabbcf65715...; the four slot archives with SHA256SUMS.harvest in run/archives/. Pods atp-x-4 b5128e08 (nebius 16 vCPU / 64 GB, EPYC-Genoa, slots 1, 3 and 4) and atp-x-5 fd384c12 (nebius 16 vCPU / 64 GB, Xeon Gold 6338, slot 2), about 5.9 pod-hours, about $2.4; 1.5 of those hours idle after the harvest agent's session was cut at 20:08 UTC, until the dispatcher (bin/poll.sh, tick.sh) was restarted unchanged at 21:40. Both pods' records were taken before terminate; both are gone from prime pods list | 2026-10-07 |
| devs.py and the DEV-S inputs (amendment A25) | devs.py d91ee61e8eb22fd306c024f1825093cb6d169a5a84700c4aba789a5f9e571da8 and test_devs.py 22c13ae08d01b022214167d7d4a27a89f3fbf510bbb8d98f3bd322c87e08d489, committed with A25 before any DEV-S run; every source pod runs the test before its first job. Inputs in ~/.cache/thvm/enigma/devs/in/: sources.tsv a674dcf8f6a64d0807c0fcaeac118d7e68796880f66e96b10c639a7ae3f7e059 (169 problems), source_lanes.txt e4e1a3bffa1ef9198d88e53059f5b11973b54c16e92b97a52f00b3bb999b85c8 (20 lanes), source_jobs.tsv 9f0685b2bad4e1b6092212c2b1345231c6a48c40eb49216da02d2e5ce6564da2 (3,380 jobs), slots/slot_1.tsv to slot_8.tsv (jobs.txt 18481d37aed179b00589ef1b11ca6aa8a76745072f055b053e4e9cd49ce70920 holds every sha256), lanes_flags.txt d70ceec8dd0362889285e539403dc65e7cded5a6c0e5856165646a4a5629c765 (the --list-lanes=flags of binary 11fad6a4cb94...) | 2026-10-07 |
| DEV-S source runs and candidates (amendment A25 items 3 to 5) | DONE (2026-10-07): all 3,380 source jobs (169 sources x 20 lanes, 8 slots) ran `FindProof --lane=NAME --szs --wall=180` with the export at rate 1, then devs.py extract on the pod; binary 11fad6a4cb94... (git archive of 881d6505, checked on every pod), setup.sh's lane check, test_devs.py and np0_extract.py --selftest passing on both pods. Every slot finished in one launch (runner_rc 0, 0 OOM-killer lines, 0 OOM reruns, 0 extraction failures); its harvest holds the candidate files only (0.6 to 0.8 MB a slot; SHA256SUMS in ~/.cache/thvm/enigma/devs/sources/archives/), so no export left a pod. Pod counters: 2,720 sessions, 3,088,414 rules born, 1,302,727 in the second half, 41,877 candidate rows kept (830 jobs truncated at --top 32; none inexact). devs.py filter (d91ee61e...) on the Mac: dropped j13 1,048 (311 goal keys from the 400 J13 files; counts only, nothing written from them), known 2,133 (1,574 keys from 2,695 pool and corpus files), dup 21,756, cap 15,808, total 132; selected 1,000 candidates from 142 sources in 121 axiom systems (per system: 9 in 86, 8 in 23, 1 to 7 in 12). Outputs in ~/.cache/thvm/enigma/devs/cand/: candidates.tsv b8a251cb88db1ea2..., problems.tsv e6696a3d2c0f1122..., drops.tsv 63c6a90c85f2ef28..., files.sha256 a2c2b8f4e70e6b75... (1,015 files). The atp-test-fixes battery ran on the first pod (test_atp_deep 445/445 under gcc -O2, test_atp_problem_features 20 of 20, test_atp 139859/139859, FindProof with gcc and clang-14) and the branch landed as 627b21e0; it does not touch the DEV-S binary. Pods atp-devs-1 e28f3c8e (nebius 16 vCPU / 64 GB, slots 1-6 and 8, 2.74 h) and atp-devs-2 e5fb990f (16 vCPU / 64 GB, slot 7, 0.43 h): 3.2 pod-hours, about $1.3, both ended by the detached dispatcher (DRIVE DONE 03:12 UTC) after their records were taken. A second copy of the source agent started a duplicate dispatcher in ~/.cache/thvm/enigma/devs/run/ at 00:29; it ran one tick, created no pod, and was stopped at 00:31; that directory is unused. The measurement (A25 items 6 to 9) has not started | 2026-10-07 |
| devs.py and test_devs.py after amendment A26's fix | devs.py 382c49c16cf2a6151106aed35c12f709b1931282eb2ebfb933f2957fd1432e79 (replaces d91ee61e... for stage, band and gate; filter's selection in the row above stands, since filter reads no rows) and test_devs.py 920abac7ebd87810f099024acc7c3de8f9cf206517cf7a6899c6900c3a669ce8 (adds RunArmsRows), committed with A26 before any measurement run; every measurement pod runs the new test before its first task | 2026-10-07 |
| loop-0 binary and the A24 families' gates (amendment A27) | PASS (2026-10-08). Binary 92f8e42474a96f88d2a2ea0b6c34b9b75207033db407beac4e93a2746cce4c62: the gcc 11.4.0 competition build (`make -f tools/casc/Makefile bin/FindProof CC=gcc`, Ubuntu 22.04) of enigma/loop0 81007c73, built on the pod from thvm_src.tgz 4f771e5d1d0ab19e..., whose tar equals `git archive --prefix=thvm/ 81007c73` byte for byte; main fast-forwards to that commit, so the merged code is the gated code. Stage A, 32 of 32 checks PASS (results.tsv 59dc6303c9365fa9...): test_atp 139859/139859, test_atp_guide 1713/1713, test_atp_guide_ac 1753/1753, test_atp_problem_features 265/265; the OA pin 257971/735/2461555 and the DN pin 2848/254/768876 unarmed and as zero twins of the harvest head with the sampled dump on (DUMP_EVERY=7, DUMP_MAX=2000), 257,972 and 2,849 selections identical; the generator round trip; the np0_extract.py, label_join.py and guide_net.py self-tests (guide_net 21 checks); tests/atp_guide_phi.sh 26 pass, 0 fail, 5 noengine; the G2.2 script's default spec 2 of 2; the harvest runner, the label join, the trainer and a compiled-in smoke head end to end (12 runs on 4 lanes; G2.2, G2.4 parity and featparity on 2 rows, 6 of 6); a clang-14 build (820dba3fa2d8b209...). G2.1 (A3, A4, A8; tests/atp_history_v2_obs.sh d021755e..., rows of tests/atp_guide_a27.tsv 31426016...): twee-deep and unfold-tgt-awr PASS, 10 of 10 rows PASS each, the first 7 judged, no FAIL, NONDET, UNOBSERVED or WALL (g21.summary 7e4dba3620a47b7d..., g21.tsv 9c75e5412cd79f69...); tgt-sel and encompass keep the 2026-10-05 PASS (A27 item 3). G2.2 (A8; tests/atp_guide_gates.sh f211548313e3d441... with MODEL=harvest, DUMP_EVERY=7, DUMP_MAX=2000, AA=1, WALL=180, steps clock): tgt-sel, encompass, twee-deep and unfold-tgt-awr PASS, 10 of 10 rows PASS each, 0 FAIL, 0 NONDET; tgt-sel's rows hold 0 to 44 clock-gated decisions and unfold-tgt-awr's 4 to 150, all identical (g22.tsv tgt-sel 25545945d3cadb83..., encompass b9169310fd2b6d17..., twee-deep fb62489f94eb81ca..., unfold-tgt-awr 71cda7749d8fc001...; decisions.txt d0398cabe6e3d714...). The four families are tgt-sel, encompass, twee-deep and unfold-tgt-awr; the replacement order was not reached. Pod atp-tr-1 a3cd481a (nebius 16 vCPU / 64 GB, AMD EPYC-Genoa, $0.40/h), created 02:03 and terminated 02:28 UTC by the detached dispatcher (~/.cache/thvm/enigma/train/run/, DRIVE DONE, rc 0) after its records were on the Mac (records.tgz 94e60dcfb138db60..., 0 OOM-killer lines): 0.42 pod-hours, about $0.17. results/SHA256SUMS (a.tgz fee1b897f8b1ec48..., b.tgz ed536d987ac112de...) checked on the Mac, and both archives equal the unpacked results/a/ and results/b/ | 2026-10-18 |
| loop-0 harvest inputs (amendment A28) | Before any harvest run. jobs.tsv ec4bdc454435952549777afd1bdb4ad7604752ec86420f7d4c3e91de7ddb9f9a (guide_jobs.py e3749860a5b51d72..., picks 1000; 6,280 jobs on 2,058 problems); the pod bundle in.tgz 0af46fed21ee4a1af2c9e180ec5f3379f18de8619ed1991be104311c6c9ecf42, its in/SHA256SUMS f2556c968876730bfcf9848233fb4743da5a071e3d340b1e660728cda4efc1d4 (setup.sh a3714ca8ba69ca66..., pipeline.sh 5660d6d98d3ffa1c..., labels_check.sh 50b390ae0d2edbda..., report.sh 9c04e7a3ce02caad..., pack_results.sh d69ddfb799351b5d..., pack_groups.sh 6619cc52b368390f..., records.sh af7a00e92cac0d52..., thvm_src.tgz 4f771e5d..., tptp_pool.tgz f3db85ff..., trainx_loop0.tgz 5962792e..., trainx_files.sha256 ca8e36be..., gated.txt db3ac941..., labels_m0.list 48212de19128e39b..., labels_x.list e13da5f16ac78520..., labels_m0.sizes 677ee055f103c1cd..., labels_x.sizes 5451ac3ccdc04eb9...); the dispatcher's SHA256SUMS.bin (launch.sh f4efa1cabfc0d412..., drive.sh 1489c6450dd6b7b6..., tick.sh 49a5c4f2fc1a9f54..., upload_labels.sh 5cd050e13088f366..., archive.sh 63aebbda08b4f848..., finish_pod.sh 01c802828eb98338..., wait_event.sh 68284ed907b14588..., summarize.py 51f8b1d8de4b103a...). All in ~/.cache/thvm/enigma/train/loop0/harvest/ | 2026-10-18 |
| loop-0 harvest inputs after amendment A29 | Before any harvest job; replaces A28's row for these files only. in.tgz 29385be214a6d55f5be7a84663ed089410fab8a8f037b0ed01157b660040e19e, in/SHA256SUMS 526891e37295dce6f7c92842f88a70a1bb362c932aa9dc3575d45c5748305863 (setup.sh 755ed192548ac92c..., records.sh 7abc22280def7abc..., pack_results.sh 46fe17e8fb937f7e...; every other file as in A28's row), tick.sh 894d73296569386a... in SHA256SUMS.bin. The first pod's records: results/atp-tr-1/records/ (bin_sha 5033def4eb0badcf...) | 2026-10-18 |
| loop-0 harvest (amendment A28) | DONE (2026-10-19); readouts, not a gate (G3.1 is decided at loop 1). All 6,280 jobs of jobs.tsv ec4bdc45... ran in one launch on atp-tr-2, width 14 on 16 CPUs, 17:33 to 19:57 UTC on 2026-10-18 (8,640 s), .done status=complete, pending_left 0: binary 92f8e42474a96f88... built with A29's toolchain (toolchain.txt: gcc-11 11.4.0-1ubuntu1~22.04.3, libc6-dev 2.35-0ubuntu3.15), the pod's clock-gated list equal to gated.txt, the instruction clock on every row, 0 OOM-killer lines and no rerun. summarize.py (51f8b1d8...) 15 of 15 checks PASS (validation.txt 341449ee67f138f5...). Labels uploaded once (1,017 s) and checked: M0 8,518 files (1,240,846,617 bytes), TRAIN-X 18,695 (2,632,743,264), none missing, resized or extra. keys.tsv 56befe62eb7135fed5d2f030392a3ab455e94dbaa6e31cba99d61353bbb41470: 31,075 proof-mode runs of the M0, TRAIN-X and loop-0 harvests, 780,744 keys, 0 unreadable. Per family (runs; proved here / in the matrix rows; picks, scored; runs joined; groups; groups with an own, cross-lane (cross-lane only) and hindsight positive): tgt-sel 1,731 (487 TRAIN, 1,244 TRAIN-X); 1,002 / 999; 4,563,975, 4,542,971; 1,723; 686,595; 122,416, 118,741 (17,081), 398,642. encompass 2,057 (608, 1,449); 1,381 / 1,385; 46,968,429, 46,878,334; 1,053; 578,273; 146,467, 113,081 (9,845), 201,280. twee-deep 2,057 (608, 1,449); 1,406 / 1,411; 38,602,971, 38,511,990; 1,013; 543,951; 125,415, 120,075 (28,818), 155,179. unfold-tgt-awr 435 (194, 241); 272 / 276; 1,877,830, 1,877,807; 435; 207,877; 52,177, 34,808 (3,299), 93,170. All: 2,058 problems, 6,280 runs, 4,061 proved (matrix 4,071; 27 matrix proofs not proved here, GaveUp 19 and Timeout 8; 17 proved here only), 80,906,810 selections against 118,473,181 in the matrix rows (the head's wall cost, A28 item 5), 4,224 runs joined, 2,016,696 groups, 125,979,715 rows; positive rows own 952,446, cross-lane 1,034,240, hindsight 22,462,566. Run failures: 665, all 'no closed session' (encompass 315, twee-deep 350): runs the schedule stopped at the wall (59.5 s) before the engine closed its export session (664 Timeout, 1 twee-deep GaveUp), so no npz; tgt-sel and unfold-tgt-awr have none, since their unproved runs end GaveUp. No timeout, crash or signal exit. summarize.py prints 1,330 because it appends each failure in its per-family pass and again in its 'all' pass, so failures.tsv lists each twice. Not joined: 2,056 runs, those 665 plus 1,335 with no scored pick (every pick trivial: fewer than 2 CPs or an ultimate root; tgt-sel 8, encompass 663, twee-deep 664), 16 front-end proofs, and 40 in which 1 dumped selection disagrees with the export (encompass 18, twee-deep 22, all TRAIN-X, mostly SYN -10; not yet diagnosed). Outputs in ~/.cache/thvm/enigma/train/loop0/harvest/: results/atp-tr-2/ (results.tgz eb6b853107f05200..., index.tsv b25e8a6e46b8ac9b..., readout.txt 724e51cbe8abc6a7..., failures.tsv 67c13227dbeb160e..., stats.tsv 4a648bbc8a54bb89..., groups.sha256 d19f1fc2b2a95668..., bulk.sha256 fbd140d4de5d24ce..., records.tgz d9c1332b1319a8ec...); archive/atp-tr-2/ (groups.tgz ab5cf1715d33b1b2..., and the dumps, outputs, stderr tails and npz: 24,423 of 24,423 files of bulk.sha256 verified on the Mac). The archival copy ran from 20:31 to 00:34 UTC; its own check then failed, since the dispatcher's PYENV_DIR named a worktree removed during the run and every python3 call through pyenv aborted, and the pod waited 2.6 h in finishing. The check was made by hand (verify_manual.txt), the directory recreated empty, and the unchanged dispatcher then took the records, terminated the pod and ended (DRIVE DONE, rc 0). Pods (datacrunch 16 vCPU / 64 GB, $0.19 an hour): atp-tr-1 1039f294 (A29, 0.06 h) and atp-tr-2 3eecb918 (AMD EPYC 9654), 17:28 on 2026-10-18 to 03:14 UTC on 2026-10-19, 9.77 h; 9.84 pod-hours, about $1.87. Both are gone from prime pods list, and their ssh stanzas are removed | 2026-10-19 |
| loop-0 heads inputs (amendment A30) | Before any training run. Code commit b5091f305e5c277d8542f97926e71f2d5e977350 (`git archive --prefix=thvm/` tar 3ab703dea5913fee..., thvm_src.tgz 5353df2d1752917e...): guide_net.py de65222dcb632f9e..., weights_to_c.py c9a0a3fe2baf3faa..., tests/atp_guide_gates.sh f211548313e3d441... (A27's), tests/atp_guide_a27.tsv 31426016981d9b37...; the groups as the harvest's archive copy groups.tgz ab5cf1715d33b1b2... with groups.sha256 d19f1fc2b2a95668...; tptp_pool.tgz f3db85ff.... The pod bundle in.tgz ad67390e202ffa318cd34571dc6dc90eb0209cc3b0170f4e1650c0850bc1e236, its in/SHA256SUMS b30f66739e309df701d4b1253025405f3d3cdd244f289542a24d60ec98f4a001 (setup.sh 7353d2eedd6c2367..., pipeline.sh 69adcfc64564a6b0..., groups_check.sh 434e2a809d3c5c74..., report.sh 2cdbaec9e7b7658f..., records.sh dc1b9463b441b297..., pack_results.sh e652baf021681176..., pack_bulk.sh e7c3778314209a1b...); the dispatcher's SHA256SUMS.bin c93c3f3e278bba94c3823ec73a669ba59979610f94ed11b7f79bd1e812e25c4a (tick.sh 427c5786cf30e85e..., upload_groups.sh ad5875f6cb3f965f..., finish_pod.sh 61085ee34af6c4f4..., archive_bulk.sh 28c02f6a258b19b1..., drive.sh be6bcbfa55a83451..., launch.sh c5be5b378263f0c1...), in ~/.cache/thvm/enigma/train/loop0/heads/ | 2026-10-19 |
| loop-0 heads gate rerun inputs (amendment A31) | Before the rerun. Commit 23df9a6b70a6b41a2fc3a18f34144c1c0ae91fb8 (`git archive --prefix=thvm/` tar b8affc8ff8ba9ba0..., thvm_src.tgz c021ef5fc61bbc39...): tests/atp_guide_gates.sh dc2ac6be80d80f67... (the no-proof fallback); A30's generated src/atp/guide_models.c 202ff4f0aa5bc903... and head loop0.safetensors f438a6006771e2f6...; the build must be A30's FindProof dd5f400ac3c62104.... The pod bundle in.tgz bea53c6281d1d3ed1426b1812fc8988de605c17e51e0e97eb7d6595d0c937db2, its in/SHA256SUMS 7eabf0fa4ba5e3da3ad73a105d32b6b839ca3c39027dec4923df0ccd6f19700a (setup.sh 3451ed5373c8a0ff..., pipeline.sh 52a0623ef5027be6..., report.sh 2b3c61677b5855e9..., records.sh 26fe94fc11b04f36..., pack_results.sh f95fdf687f3c7268..., pack_bulk.sh 13ff0815f19a70a5...); the dispatcher's SHA256SUMS.bin 6073bf7dfbf5597bd1beb9c74c0efb5bc2fe0194b1d2b45e82abf24ec8ea9005 (tick.sh 6633743e68fa91e0...), in ~/.cache/thvm/enigma/train/loop0/heads_a31/ | 2026-10-19 |

Section 5.3 outputs (2026-10-02, from the sealed matrix; train and held-out only):
- Pool: TRAIN 664 problems, VBS at 60 s 550 (528 within 1 s), best single lane twee-raw 488, greedy cover reaches the union at k = 9. HELDOUT 171: VBS 143, best lane twee / twee-raw 134, union at k = 3. G0.1 passes (56,810 of 56,810 rows; 24 crashes, 0.062%).
- Hard tail: TRAIN 10 problems (fastest lane over 10 s), HELDOUT 1 (REL033-3). After the J13 stem exclusion, DEV-T = 5 and TRAIN-T = 4 problems. The pool has almost no band, so DEV-S (M2) and the 180 s supplement carry the gates.
- M4b pre-gate: FAIL. The oracle plan and simulated P1 both solve 505 on TRAIN + DEV-T at 60, 53 and 45 s; only 10 problems are solved by exactly one lane. No learned launch plan.
- M4c pre-gate: FAIL, final. 8 TRAIN Unsatisfiable problems rated below 1.0 are unsolved at 60 s (25 needed). No invention pilot.
- P1 retirement candidates by the letter of the rule: wm, horn-back, mnf-gj-amw, ifeq, tgt, tgt-fold, wm-gj, lazy-gj, bare-gj, plus 13 fof-* lanes the CNF pool cannot judge. wm is the fastest lane on 39 pool problems and owns REL033-3, so it stays for M1's paired test. All candidates wait for the 180 s supplement.
- Twin families: provisional, decided by ties because TRAIN-T has 4 problems: horn-cd, twee, encompass, flatten-bare-raw. Revisited after the supplement and DEV-S. horn-cd left the set by amendment A5.
- Supplement: 142 problems, 2,882 jobs (SYN986-10.005/.006 segfault until the deep-term fix lands; SWX233-1's 14 crash rows are on other lanes and not yet diagnosed, so the depth class is not known to be closed: tools/atp_enigma/crash_rows.sh reruns all 24 rows, deployed and under AddressSanitizer, before the supplement runs). Label harvest: 8,574 pairs on 500 TRAIN problems.
- T7: 40 front-end proofs come from equals, narrowq and unfold, which the static replica calls dead. These lanes run the Horn front end before their exit test (the M1 liveness item).

Section 5.3 outputs at 180 s (2026-10-05, amendment A20; matrix_tables.py --supplement, --tables, --decisions on the 180 s view; train and held-out only):
- New pool solves at 180 s: 5 of the 142 supplement problems, train 4 and held-out 1: GRP678-1 (unfold 115.27 s, also unfold-tgt-awr), GRP725-1 (twee-deep 131.54 s), LAT167-1 (encompass 57.48 s), LAT183-10 (encompass 74.50 s), COL064-1 (equals-twee 54.63 s). Single-lane solves: encompass 2, twee-deep 1, equals-twee 1. All proved rows are Unsatisfiable. VBS at 180 s: TRAIN 554 of 664 (60 s: 550), HELDOUT 144 of 171 (143); pool union 698 (693); greedy cover reaches it at k = 10: twee-raw, equals, encompass, narrowq, twee-deep, unfold, equals-twee, bare, otter-ladr, horn-cd.
- Hard tail, final: DEV-T = 7 (COL064-1, REL033-3, GRP725-1, LAT075-1, LAT144-1, LAT162-1, LAT391-1), TRAIN-T = 7 (GRP678-1, LAT074-1, LAT159-1, LAT167-1, LAT183-10, LAT230-10, LCL385-10). Condition 2 adds 2 to DEV-T and 3 to TRAIN-T; no supplement problem is a J13 stem-mate. TRAIN = 610 problems. Condition 3 read at 180 s would hold for 0 problems (not applied).
- Twin families, final: TRAIN-T owners encompass 3, horn-cd 1, unfold 1, twee 1, flatten-bare-raw 1. The primary reading (T5 median over all rows, head 35 us) gives horn-cd, unfold, twee, flatten-bare-raw; at 113 us only horn-cd. encompass owns the most but its T5 median is 433 us per selection, under the 700 us bar; over rows with CPU >= 1 s it is 2674 us and the set is encompass, horn-cd, unfold, twee. horn-cd left the set by amendment A5.
- P1 retirement candidates at 180 s: unchanged, 22 lanes qualify at 60 s and 180 s: wm, horn-back, mnf-gj-amw, ifeq, tgt, tgt-fold, wm-gj, lazy-gj, bare-gj, and the 13 fof-* lanes the CNF pool cannot judge. No candidate has a unique 180 s solve or a cover pick; without all 22 the pool union is still 698 of 698. wm stays for M1's paired test (strictly fastest on 39, owns REL033-3).
- M4c, final: 5 TRAIN Unsatisfiable problems rated below 1.0 stay unsolved at 60 s and 180 s (GRP196-1, LAT190-10, LCL270-10, LCL315-10, PLA023-10; the supplement solves GRP678-1, LAT167-1, LAT183-10). The pre-gate needs 25: FAIL, and there is no invention pilot.

Decisions recorded (2026-10-01, by the user):
- D1: yes. Training may include synthetic -10 encodings of non-UEQ TPTP problems from our FOF front end; J13 names and stems are excluded in every division.
- D3: no. Every calibration stays on pods; WDEP is estimated from the submission build's official J13 result as in section 1.3.

Engine change staging (enigma-ftdeep, recorded 2026-10-02).  The branch replaces the per-level recursion of about 40 term walks with explicit stacks.  Results, arena cells and heap words are pinned to the recursion, but every rewritten walk costs different instructions and CPU time, so lanes whose decisions read a clock (`--lrs`, the CP-set and Horn-release budgets, the FOF budgets, under the CPU or the instruction clock) and runs that end near the wall can change.  It therefore lands in stages, each after its own pins pass on a pod:
1. ft.c with term_stack_grow, and atp_ri_flatten (the two SYN986-10 overflows), once their pins, test_atp, the OA / DN pins and tools/atp_enigma/crash_rows.sh pass on the stage-1 build;
2. the remaining walk commits in branch order (term primitives, flatterm spans, CP packing, weights and orientation, ground join, overlap index and mirror, AC, the discrimination-tree descent, KBO, LPO, the proof printers), each with its own pin binary;
3. the brace and wmo_dfs re-entry commit and the test upgrade (mid-depth pins, small-stack deep blocks, test_atp_deep_cp) last.
Before stage 1 and after stage 3, tools/atp_enigma/ftdeep_pod.sh measures the paired instruction and CPU overhead on the OA / DN pins and about 20 TRAIN problems on twee, wm, otter-lpo and tgt-sel.  The change is accepted when the overhead is within noise (at most 2% instructions) and every changed search is explained by a clock or the wall; the numbers go into this registry and the CHANGELOG.  If L0 has already run when a stage lands, L0's binary stays the one recorded above and the change is an amendment.

Amendments (each written before the run it governs):
- A1 (2026-10-02, before any G0.4 data exists): how G0.4 is decided.
  - The judge is tests/atp_sched_teardown.sh, sha256 93dff179d724d0e40b8f006b8d75152ae1e1660ec12b0ea057794adf6b1a29c6, run on a current-main binary that includes the PID fix. The pod log records the script's and the binary's sha256.
  - Teardown, the SIGTERM clause: LAT071-1, a TRAIN problem whose TPTP status is Open, with all 46 lanes racing. The brief named a J13 problem; J13 stays sealed. 5 s into a --wall=60 run the script sends:
    - SIGTERM, SIGINT and SIGALRM to the parent of a --schedule run;
    - SIGTERM to the parent of a --lane run of the last lane;
    - SIGTERM to the last lane's slice process alone, in a --schedule run and in a --lane run;
    - the matrix runner's pkill -TERM -f "^BIN --lane=" to a --lane run.
  - A signalled parent must end through its teardown handler (exit 128 + the signal; pkill excepted). After the slice-only signal the slice's process group must be gone, a --schedule parent and its other lanes must go on, and a --lane parent must end by itself. 3 s after each stop no process of the run and no slice output file in /tmp may be left. A case whose run ends before its signal, or whose last lane is not alive at 5 s, is inconclusive. A --wall=10 run must also end by 20 s and leave nothing; that case can only fail.
  - Dry run, the W + 10 s clause: the 20 rows of tools/atp_enigma/g04_dryrun.tsv, sha256 8fe06a9ee76bf5c1a7abdf697f65f8572d159bf69cd6d237725372e9601e77bc. They are the first 20, by sha256 of the name, of the 81 TRAIN problems that no lane solves at 60 s and whose every searching lane ended at the wall. WALL = 180 s, and again at WDEP once G0.3 fixes it, each in the environment of section 1.3. Every run must end by W + 10 s and leave no process. At least 10 of the 20 must reach the wall, meaning the parent's wall stop or alarm, or an end at W - 4 s or later (each lane stops itself on its CPU clock at the wall less 3 s); otherwise the dry run is inconclusive.
  - G0.4 passes only when the teardown mode and every dry run exit 0. Exit 3 (inconclusive) does not pass. A changed script or list needs a new amendment.
- A2 (2026-10-02, before any G2.2 run): how G2.2 reads a clock-gated lane.
  An armed zero twin runs a few dozen user-space instructions per step
  outside its work-clock exclusion window (the selection log, the goal
  compares, the coop and trivial picks, the window's edges), and its
  environment is longer by the `--guide` variables.  Under the
  instruction clock, a decision of a clock-gated discipline (`--lrs`,
  `--cp-set-ir-budget`) can therefore land a few selections from the
  base lane's.  G2.2 is decided by tests/atp_guide_gates.sh, with the
  base run's environment padded to the twin's length:
  - a lane that logs no clock-gated decision (THVM_ATP_CLOCK_TRACE) passes
    only when the selections, the proof and the stats, LRS prune counts
    included, are identical;
  - a clock-gated lane passes when every selection up to its first
    clock-gated decision whose trace line differs is identical, and when
    the decisions are identical, everything is; the selection index of
    that first drifted decision is reported per run.
  Columns 13, 14 and 19 of the layer's input, phi and the lane are not
  in export v2; the M3 harvest reads them from THVM_ATP_GUIDE_DUMP, and
  `weights_to_c.py featparity` checks the remaining columns against the
  export.
- A3 (2026-10-02, before any G2.1 or G2.2 run): the identity gates run under a gate-only logical work clock, THVM_ATP_WORK_CLOCK=steps. The engine's clock-gated disciplines (CP-set interreduction, LRS, the goal-narrow endgame) then read a count of CP inserts and selections, which an observer (the export, a zero-correction guide) cannot advance. Under the instruction clock the hooks' own framing still counts, so identity could fail by construction on twee, otter-lpo and tgt-sel. Under any deterministic clock (instr or steps), the LRS budget and the endgame span are the configured wall less the work done before their epoch, not a wall reading. A G2.1 or G2.2 row passes only on exact identity; a moved clock-gated decision is a FAIL, and there is no DRIFT class. Identity holds within one binary: instruction-clock baselines pinned on an earlier build are re-verified against the build under test.
- A4 (2026-10-02): G2.1 lists 10 TRAIN problems per family and judges the first 7 conclusive rows in list order (PASS, FAIL or UNOBSERVED); any FAIL fails the family, and NONDET or WALL rows are replaced by the next listed row. A row whose export holds no selection is UNOBSERVED, never PASS.
- A5 (2026-10-02): horn-cd leaves the provisional twin set. Its search runs mostly in the CLI condensed-detachment loop (tools/atp_cli/horn_release.c, horn_back.c), which export v2 does not record, and the sealed matrix holds 5 engine proofs for it at 60 s, below G2.1's 7. It returns only if that loop gets export hooks and 7 observable problems exist before G2.1 runs.
- A6 (2026-10-02): landing the history-export v2 branch requires G2.1 (A3, A4) and G2.3 (test_atp in full, the OA and DN pins) on pods, both passing.
- A7 (2026-10-02, before any pod run of the -10 encoder; no data for it exists): gate G2.6, the validity of the synthetic -10 corpus (M2 deliverable 5, decision D1). The corpus may be used for training (TRAIN-X) or as a gate set (DEV-X) only if all of the following hold, on one FindProof binary:
  1. ALL=1 tests/atp_ifeq10_tptp.sh passes and writes its pass file. Decision rule: it fails if one of the eleven cases fails (a mismatch, or clause names that differ), if a surveyed [CS18] source exits non-zero (input, nonhorn, sat1, timeout, crash), lands at "differs" or cannot be compared, or if the survey does not finish by its deadline (status=deadline; DEADLINE_S 14400 s, 120 s and 3 GB per encoding). These levels are reported and pass: names, order, generated, nest, and drift, a "differs" whose source carries a "% Bugfixes" release newer than the oldest release in the -10 file's Rating line (TPTP edited the source after the conversion). The verdict is exact only if every case and every surveyed pair matches with names and order; otherwise it is equivalent, and both pass. The drift count is reported in the registry row.
  2. The corpus run ends with .done status=complete or complete-alerts with 0 alerts: no sat1 on any candidate, no nonhorn on an _HRN source, no truncated output. complete-unguarded (ALLOW_LEAKY=1) and incomplete fail.
  3. The corpus check is ok, including the content check: no emitted problem has the renaming-blind key of a J13 problem file (ifeq10_keys.py); the j13dup count is reported.
  4. TRAIN-X holds at least 500 emitted problems and DEV-X at least 100, each from at least 10 domains. Below that the corpus adds less than the pool's own TRAIN (664) and DEV-H (171), and D1 is not worth its cost.
  Kill: if G2.6 fails, M3 trains on the pool alone and DEV-X is not a gate set; the encoder is not tuned against the survey's results to pass.

- A8 (2026-10-02, before any G2.1 or G2.2 run): A3 governs G2.2; the clock-gated drift class of A2 is withdrawn. Both identity gates run under THVM_ATP_WORK_CLOCK=steps, where an observer cannot advance the clock, and pass only on exact identity of selections, proof and stats (LRS prune counts included). tests/atp_guide_gates.sh runs under the steps clock. A2's padding of the base run's environment and its note on columns 13, 14 and 19 stay.

Amendments A9-A17 (2026-10-02, before any protocol run; tools/atp_enigma/protocol implements each):
- A9 Memory. "At least 72 GB" reads as an effective memory limit, min(MemTotal, the pod's cgroup limit), of at least 16 x 4500 MiB, so 16 slices at the schedule's cap fit. The cap is fixed per comparison (--rss-cap default or 3500, the same on every pod and look of it); a pod that cannot honour it is refused, stats.py refuses rows of one wall with mixed caps, and every row records its OOM kills. WDEP is calibrated only under the default cap (J13's); rows at 3500 make G0.3 uncalibrated.
- A10 CPUs and binaries. 16 vCPUs means 16 effective CPUs: the affinity mask capped by the cgroup CPU quota. Every arm runs one binary (another needs --allow-binaries and is logged as a deviation). FindProof rejects an unknown --schedule mode (exit 2), and two arms with different flags whose binary lists the same lanes are refused unless declared (PD and E).
- A11 Protocol parameters, hashed into the protocol fingerprint on every row: CheckProof wall 60 s (THVM_PROOFCHECK_WALL=60 under timeout -k 5 75); seed 20261001 for the arm shuffle and the bootstrap, 10,000 resamples (another value at a look needs a named amendment); the clock-gated set is --lrs, --fol-lrs (the FOF front end's LRS) and --cp-set-ir-budget, read from the winning lane's flags in that arm's own binary lane table, and a winning lane missing from the table stops the runner; "the arms disagree" means their pass-1 Solution flags differ at that wall, and repeats run per wall.
- A12 Slice stderr. The runner adds THVM_ATP_SCHED_ERRDIR (slice stderr to files, for the end class of every lane). Before L0, P0 runs on DEV-H at WDEP with and without it, paired on one pod. If the paired |net| exceeds 2, or the median time ratio over the problems both solve is more than 2% off 1, every timed pass runs without it and end classes come from an untimed rerun of the misses.
- A13 Pod-speed calibration. The --schedule races 46 lanes and solves almost every TRAIN problem in under 3 s, so the calibration runs one deterministic configuration (the default preset, --szs --wall=120) on TRAIN problems of 2.6-30 s (protocol/calib_candidates.tsv); the reference keeps the 10 longest stable ones. Each pod runs 2 passes; it fails when a problem is unsolved, the geometric mean of the per-problem median ratios is more than 15% off, or one problem is more than 30% off; the reference's rss cap must match. Width contention is measured separately (M0 deliverable 5).
- A14 The seal. The WDEP runs are J13 runs of look L0: only the per-wall aggregates and the G0.3 decision are read, and the per-problem agreement table is sealed. Sealed runners log a heartbeat (base-job progress rounded down to 10%) instead of per-job lines and pack every per-problem output into sealed.tgz; run.stop names no problem, and opening the archive for a VerifiedBad is the section 1.3 exception. A look reads all 400 J13 problems together, and --look L2 needs the unseal token above.
- A15 Aggregates at L0 and L1. Domains with fewer than 10 compared problems are pooled into OTHER (while OTHER holds 1-9, the smallest named domain joins it); DROP-BEST picks among the pooled buckets; per-domain gains and losses are printed, and M5's "no TPTP domain loses more than 3 problems" reads a pooled bucket's loss count. Unknowns are printed per arm; the per-lane counts are sealed.
- A16 Completeness. At L0-L2 a problem without complete cells in every compared arm makes that wall INVALID, and the runner resumes until it is complete; dev analyses compare such a problem on pass 1 of every arm and count it. Every comparison at a wall uses one problem set, and NL is chosen once per wall on it, a tie broken against the first comparison that names NL. G0.2 and delta need both A/A passes on all 400 problems. A resume never splits a problem across pods: a problem incomplete on a dead pod reruns whole on its replacement, and a resume under another configuration is refused.
- A17 Readings. G3.3's "McNemar p < 0.05" is the one-sided exact sign test, as in section 1.5. An unsolved cell whose passes tie between Unknown and another non-Solution outcome counts as Unknown (conservative toward INCONCLUSIVE); the number decided by that tie is printed per arm.
- A18 (2026-10-02, after the m0m2 pod gates and before their rerun): the A13 reference. On the gates' EPYC 9654 pod the twelve candidates ran in 0.6-0.7 of their matrix times, so only 7 had a median of at least run_arms.py's 3.0 s and the reference could not keep 10 (calib-ref exit 6). The pool holds few more: a survey of the 145 eligible TRAIN problems (stem-disjoint from J13, not -10, not DEV-T, proved by the wm lane at 2.6 s or more, or not proved by it at 60 s) ran the calibration configuration (--szs --wall=120) 12 at a time without a memory cap; it proved the twelve and GRP750-1 and GRP754-1 (43.8 s and 43.3 s alone on that pod at --rss-cap 3500), and the other 131 ended Timeout (90), ResourceOut (3) or killed at 66-121 s with no status (38, memory under that load), so none of them is a stable reference problem. So the candidates add those two, and calib-ref's floor is 2.5 s, A13's own 2.6 s floor measured on the matrix pod (the 2.6-s problems run 2.7 s on the EPYC pod) rather than 3.0 s; the spread bound (10%), the 10 kept and the pod-speed tolerances are unchanged. A faster pod type than the EPYC 9654 needs a new amendment before it serves as the reference.
- A19 (2026-10-02, before any WDEP, L0 or supplement run): three contradictions found when the branches merged.
  1. Memory (A1 against A9). A9 governs every protocol run: the effective memory is min(MemTotal, the cgroup limit), and below 16 x 4500 MiB every arm runs with the 3500 MB cap. A1's G0.4 dry run is a liveness check: it passes when every run ends by W + 10 s and leaves no process, and at least 10 of the 20 reach the wall, so its verdict does not depend on the cap. Its pinned script (tests/atp_sched_teardown.sh, sha256 93dff179d724d0e40b8f006b8d75152ae1e1660ec12b0ea057794adf6b1a29c6) stays unchanged, including its own reading (MemTotal below 72 GiB, no cgroup, sets 3500 unless the caller passes THVM_ATP_RSS_ABORT_MB). Its runs are not protocol rows and enter no comparison.
  2. WDEP binary. Section 1.3 governs: the WDEP calibration runs the J13 submission build (~/.cache/thvm/casc/FindProof.tgz) with its own CASC command line, and L0 runs current main (the L0 binary row). The registry row on the enigma-ftdeep engine change, which put the WDEP calibration on the L0 binary, is corrected above.
  3. The clock-gated set (A11 against A3). The repeat trigger covers every lane discipline that reads the work clock or the wall for a search decision. A lane is clock-gated when its flags in its arm's own lane table, read as FindProof reads them (the last value of a repeated flag wins; a bare boolean flag is 1), leave one of these on:
     - --lrs (engine LRS; on when the value is non-empty and does not start with 0);
     - --fol-lrs (the clausal pre-pass LRS; on when its integer value is not 0);
     - --cp-set-ir-budget (on when its integer value is above 0);
     - --goal-narrow, the goal-narrow endgame (src/atp/goal_narrow.c: past 85% of the span to the wall, read on the work clock, the e-match sweep bursts). Only this flag (THVM_ATP_GOAL_NARROW) enables it, on when the value is non-empty and does not start with 0. No preset sets it, and no lane of the current schedule table carries it;
     - the Horn release (tools/atp_cli/horn_release.c and horn_back.c: the 2.5 s release budget, --horn-wall, and its fractions, the phase-2 trigger, the backward cone, the gate relax and the CD deepening of --horn-cd and --horn-back, all on the work clock). It is on by default: main.c runs it unless THVM_ATP_HORN starts with 0, so every lane without --horn=0 has it, horn-cd and horn-back included;
     - the FOF front-end budget, the clausal pre-pass (tools/atp_cli/fof_split.c: 60% of the remaining wall on the work clock, or --fof-sat-wall's share). It is on by default: it runs unless --fof-sat is 0 or below.
     The Horn release acts only on a problem with an ifeq dispatcher axiom, and the pre-pass only on non-Horn input from the FOF front end. The trigger still reads the lane flags alone, so it is a superset: an extra pass costs pod time and cannot bias a comparison. Under this reading 37 of the 46 lanes in the current table are clock-gated; the 9 that are not are unfold-tgt, flatten-bare, tgt-ax, bare, ifeq, tgt, tgt-fold, bare-raw and flatten-bare-raw. So at W180 and WDEP nearly every solved problem of a --passes 1 comparison runs three passes; L0's --passes 2 design has no repeats and is unaffected. The wall stop of the front end (cli_fe_check) and every lane's wall-less-3-s stop end a run and decide no search step, so they are not in the set. run_arms.py's GATED_RULES implements this and its protocol fingerprint changes from e820fb2367a1 to efeb738a5185 (the registry row above); test_stats.py checks each flag's reading and the gated and ungated lanes of the schedule table.
- A20 (2026-10-05, before any supplement run; no 180 s row exists): how the 180 s supplement (M0 deliverable 3) runs and how the section 5.3 rules read it.
  1. Run. The 2,882 jobs of supplement_jobs.tsv (sha256 ac37a6dbe639d25b9602e45c6939aba7f37219a1b0eb0e6b5977b582edbff7c8; 142 problems, every non-fast-exit lane) run through the matrix runner (runner.sh, sha256 f86f43c7956b90d0efb19d2297d3dc968c45644c824095c1eeddb5576b81abef) at RUNNER_WALL=180 and RUNNER_TIMEOUT=195, on a FindProof competition build (gcc) of current main, default RSS cap, no --proof, CPU clock. The problems are dealt round robin to six slots, all lanes of a problem in one slot. A slot runs at width 14 on 16 vCPUs, so that 14 x 4500 MiB fits in 64 GB (A9's reading; the matrix ran 16 x 4500 MiB on 64 GB); on a 32-vCPU / 128 GB pod two slots run at once, pinned by affinity to CPUs 0-15 and 16-31. The pod, width and binary sha256 of every slot are in its .started sentinel and in supplement/validate.txt.
  2. Reading. The 180 s view is the matrix rows with each supplement row in place of the 60 s row of its (problem, lane); a problem some lane solves at 60 s was not rerun, so its rows stay the 60 s ones. P1 retirement: a lane has a unique pool solve at 180 s when it alone proves a supplement problem at 180 s, and "no marginal value in the pool cover" reads the greedy cover of the 180 s view. Hard tail: condition 2 holds for a problem no lane solves at 60 s and some lane solves at 180 s; conditions 1 and 3 keep their 60 s reading (the count condition 3 would give at 180 s is printed and not applied). The owner of a tail problem is its fastest lane in the 180 s view; the twin rule's selection rates stay T5 of the 60 s TRAIN rows. M4c: the 60 s list minus the problems the supplement solves. tools/atp_enigma/matrix_tables.py --supplement, --tables and --decisions implement this (registry row "matrix_tables.py with the supplement").
- A21 (2026-10-06, before the G2.6 survey or the corpus runs on main; neither has run on this binary, and no corpus exists): how gate G2.6 (A7) runs and how its four conditions read.
  1. Binary. One FindProof for every step: the competition build `make -f tools/casc/Makefile bin/FindProof CC=gcc` (gcc 11.4.0, Ubuntu 22.04) of main 6a4b244b, from `git archive`. Its sha256 goes into the pass file, the corpus's inputs record and the registry row. The ALL=1 survey of the m0m2 pod gates (2026-10-01 UTC: 11 of 11 cases, 158 pairs, verdict equivalent; no corpus) ran on another binary, db671cf2..., the enigma-m0m2 integration build at 2c933e87, before bc586198 bounded the encoder's input nesting. It does not count toward A7.
  2. Pod and width. One CPU_NODE pod with at least 16 vCPUs. Every step runs pinned to CPUs 0-15 with JOBS=14, A20's width per 16 vCPUs; a 32-vCPU pod runs this one slot, since the steps are sequential. The survey and the corpus keep the scripts' defaults: TIMEOUT_S 120, MEM_MB 3072 and DEADLINE_S 14400. TPTP-v9.3.1.tgz is downloaded on the pod from tptp.org, and its sha256 is recorded.
  3. Steps, in order; the first failure stops the run. First tools/atp_enigma/ifeq10_pod.sh without CORPUS: unit, diff, tptp (the ALL=1 survey, J13_NAMES = j13_ueq_names.txt) and driver. Then `TRAIN_NAMES=pool_train_names.txt tools/atp_enigma/make_ifeq10_corpus.sh bin/FindProof TPTP-v9.3.1 OUT j13_ueq_names.txt devh_names.txt dev_t.tsv train_t.tsv`, where:
     - pool_train_names.txt and devh_names.txt are the 879 train rows and the 217 held-out rows of the pool's pool.tsv (sha256 9fdb0135c608d6ed...), as README.md derives them. They contain the 664 attempted TRAIN problems and the 171 of DEV-H;
     - dev_t.tsv and train_t.tsv are the final DEV-T and TRAIN-T of the 180 s decisions, 7 problems each (sha256 4468a73697631ecf... and 6061641e745d437e...). README.md excluded DEV-H and DEV-T only. TRAIN-T is added so that no tail problem, gate or train, has an encoded copy in either split;
     - j13_ueq_names.txt (sha256 c43f0c4914bb4b29...) serves only as the stem exclusion list and as the list of J13 files whose renaming-blind keys condition 3 checks (keys of the J13 files are statistics section 1.4 allows).
  4. Reading.
     - Condition 1 holds when the tptp step ends "atp_ifeq10_tptp: ok" and the pass file names the binary's sha256. The registry copies the pass file's verdict and its drift count (0 when the survey line has no drift entry).
     - Condition 2 holds only for .done "status=complete", because the driver writes complete-alerts only when there is at least one alert.
     - Condition 3 holds when summary.txt reads "corpus check: ok". j13dup is the number of manifest rows with that outcome.
     - Condition 4: a split's size is the row count of its pool.tsv. Its domains are the distinct three-letter prefixes of its emitted names.
  5. Failures. A corpus run that ends status=incomplete, or without a .done, is relaunched on the same OUT_DIR with the same inputs until it ends otherwise. The driver keeps every final row and runs only the pending jobs, and the registry gives the number of launches. A failure outside the encoder and the driver (apt, the download, the disk, a missing tool, a lost pod) reruns every step from the start, on a new pod if needed, and a binary with another sha256 is recorded as such. A failed check of the encoder or the driver is final: a failing unit, diff, tptp or driver step, or a corpus .done of error, complete-alerts or complete-unguarded. G2.6 then fails, and A7's kill rule applies.
  6. Outputs. Before the pod ends, everything is fetched to ~/.cache/thvm/enigma/ifeq10/:
     - the corpus: manifest.tsv, summary.txt, .started, .done, and from .work the inputs, launches, alerts and check_errors files;
     - TRAIN-X and DEV-X, each with its pool.tsv;
     - the pass file, the step logs and sentinels;
     - the pod's build, TPTP and host records.
- A22 (2026-10-06, before any run of the lane matrix of the synthetic -10 corpus; no row of it exists): how that matrix runs, how it is validated and what it tabulates. It is the TRAIN-X and DEV-X counterpart of M0's pool matrix.
  1. Problems and jobs. Every problem of the corpus that passed G2.6 (registry row "synthetic -10 corpus"): TRAIN-X 1,464 and DEV-X 396, 1,860 in all. A capped subsample would be needed above about 2,500 problems, or above 2,000 in TRAIN-X or 400 in DEV-X; the corpus is below all three, so no problem is dropped (0), and `matrix_tables.py --xjobs` refuses a split above its cap. Each problem runs on all 46 lanes (lanes.txt 550f2f6f..., which must equal the binary's --list-lanes): 85,560 jobs. `--xjobs` sorts the problems by (split, domain, name) and deals them round robin to 24 slots, all 46 lanes of a problem in one slot (12 slots of 78 problems, 3,588 jobs, and 12 of 77, 3,542 jobs); within a slot the jobs are ordered by sha256(name TAB lane), as in M0's shards. The job list jobs.tsv (the 24 slot files in order) has sha256 346e74bab0a599a0a81c27d25f7ce69bef47923f907b6acb9f8d1ffdf36801a8; problems.tsv ff72347fe57aea3bbe5f5dbf39128227ac9033405a9daf253921bce93880928c; corpus_files.sha256 3c4fe414a6a2d4eac14a0b0fba41bf229bc2ab9418d576b00da3c9d971217210 lists the 1,862 corpus files (the problem files and the two pool.tsv), which every pod checks before its first slot. A job's path is relative to the corpus root (TRAIN-X/Problems/DOM/NAME.p), which is also its TPTP root; no corpus file has an include.
  2. Run. The M0 matrix runner (runner.sh f86f43c7..., with the job format and the 14 columns of ~/.cache/thvm/enigma/matrix/FORMAT.txt) runs every job as `FindProof --lane=NAME --szs --wall=60`: no --proof, the default CPU clock, the default RSS cap (no THVM_ATP_RSS_ABORT_MB, so --lane's 4500 MB), RUNNER_WALL=60, RUNNER_TIMEOUT=75, RUNNER_DEADLINE_S=14400 per launch, RUNNER_PAR=14. The binary is the same as G2.6's: the competition build `make -f tools/casc/Makefile bin/FindProof CC=gcc` (gcc 11.4.0, Ubuntu 22.04) of main 6a4b244b from `git archive --prefix=thvm/` (thvm_src.tgz 65e6b2dc..., G2.6's archive), built on every pod. A pod whose build is not 11fad6a4cb9452f51bcf0544072fdb306878723538234e32293a6ebdd31f9350, or whose --list-lanes differs from lanes.txt, runs nothing.
  3. Pods and slots. At most 4 atp CPU_NODE pods at once, each with 16 to 64 vCPUs and at least 4 GB per vCPU, so that 14 x 4500 MiB fits per 16 vCPUs (A20's width). A slot runs on 16 CPUs pinned by taskset (CPUs 16h to 16h + 15 for h = 0 up to vCPUs/16 - 1), so a pod runs one slot per 16 vCPUs at once, and slots start in number order as 16-CPU sets free up. A slot whose runner ends status=deadline resumes on the same CPUs of the same pod; the runner skips the rows it has. A slot never runs on two pods: if its pod is lost before the slot is harvested, the slot reruns whole on another pod. Every pod's lscpu and kernel log go into the record before it is terminated.
  4. Validation (`matrix_tables.py --xmatrix`, committed with this amendment; G0.1's bars, applied to the corpus):
     - completeness: one row for every job, with missing, extra and duplicate all 0. This is required in full; a slot resumes until it is complete;
     - every launch's .started names the binary above, width 14 on 16 CPUs (nproc 16), wall 60 s, timeout 75 s and no extra environment, and all launches of a slot ran on one host; every .done reads status=complete with pending_left=0;
     - crash plus killed-by-timeout is at most 1% of the rows;
     - OOM: every pod's `sudo dmesg -T`, read when the pod ends, holds no OOM-killer line. Every row with a SIGKILL end that the outer timeout did not cause (G0.1's reading: class crash with slice signal 9 or exit 137, or the lane parent killed) is rerun on its slot's pod at width 7 (RUNNER_PAR=7, same binary and settings), and the rerun row replaces it. Rows above the 4.5 GB cap are counted;
     - row sanity: every proved row is Unsatisfiable and no row claims Satisfiable or CounterSatisfiable. Every corpus problem is Unsatisfiable, so such a claim is a soundness alarm and stops the line, as a VerifiedBad does;
     - the lane groups of item 5 follow from the binary's own --list-lanes=flags.
     A failed item does not stop the tables, which say so; its cause is found before M3 uses the matrix.
  5. Tables, separately for TRAIN-X and DEV-X (tables/ with summary.txt):
     - T1 per lane: proved, proved alone, proved within 1, 3, 10, 30 and 57 s, the end classes;
     - T2: the VBS curve, read at 10, 30 and 60 s; the best single lane; the greedy cover to the full union;
     - T3: the unsolved problems by end pattern;
     - T4, the hard tail: condition 1 of section 1.4 (the fastest lane needs more than 10 s). Condition 3 (every searching lane ended on the memory guard) is listed with its own label. Condition 2 needs a 180 s run, which A22 does not make;
     - T8: memory;
     - X10, the -10 lanes: every lane in its group, the groups' unions, the problems only the six gated lanes solve, and the VBS without those six at 10, 30 and 60 s.
     The groups come from the lane flags of SCHEDULE[] (tools/atp_cli/main.c at 6a4b244b), read before any row exists:
     - gated on an ifeq dispatcher axiom (the lane exits without one; horn-cd and horn-back also need a ground goal): horn-cd, horn-back, ifeq;
     - gated on a non-ground goal: equals, narrowq, equals-twee;
     - Horn release on (no --horn=0; the release acts on dispatcher problems): wm, unfold, flatten, twee, twee-deep, twee-freq, mnf-gj-amw, encompass, twee-raw, wm-gj, lazy-gj, bare-gj;
     - Horn-free (--horn=0): unfold-tgt, unfold-tgt-awr, flatten-bare, tgt-ax, tgt-sel, otter-lpo, otter-lpo-cmin, otter-ladr, bare, tgt, tgt-fold, bare-raw, flatten-bare-raw;
     - the FOF pack, which exits on cnf input (every corpus file is cnf): the 15 fof-* lanes.
     The six gated lanes are the -10 lanes this matrix singles out.
  6. Label harvest. Section 5.3's rule applied to TRAIN-X: every TRAIN-X (problem, lane) pair proved at 60 s goes into label_harvest_jobs.tsv, in the runner's job format. DEV-X rows are never training data. The tables decide nothing by themselves: they are inputs to M2 and M3, which read TRAIN-X for training and use DEV-X as a gate set.
  7. Outputs, in ~/.cache/thvm/enigma/ifeq10/matrix/: the job lists (jobs.txt holds every sha256), results/ (slot rows, sentinels, launches, crash logs, the harvest archives with their sha256, the pod records), validate.txt, xmatrix.tsv, tables/ and label_harvest_jobs.tsv. The J13 seal holds as in G2.6: no corpus problem shares a stem with J13, both X modes read j13_ueq_names.txt only as a stem exclusion list, and they check that no output names a J13 stem.
- A23 (2026-10-07, before any run of the TRAIN-X label harvest; no row of it exists): how the label harvest of A22 item 6 runs, what it records and how it reads. It is the TRAIN-X counterpart of M0 deliverable 7 (section 3.2; registry row "label harvest").
  1. Pairs. Every pair of label_harvest_jobs.tsv (sha256 1564b8d903f9f8eef8537bb70d36234faf2475ca1f9de5b6b8e88713d9b1c9b0): 19,913 TRAIN-X pairs on 1,228 problems, 8,348 of them front-end proofs in the X matrix. DEV-X is a gate set and is never harvested: no DEV-X pair runs, and no DEV-X file reaches a pod. The pods get only the 1,228 problem files of the list (trainx_files.sha256 9fe6adff65a93eb4..., their lines of A22's corpus_files.sha256, each checked against its file; trainx.tgz 20fcab65d00dbb2f...), and each pod checks them file by file before it runs anything. make_slots.py (ad99c00837b5d198...) sorts the problems by (domain, name) and deals them round robin to 4 slots, all lanes of a problem in one slot: 307 problems each, with 4,910, 4,966, 5,060 and 4,977 pairs. Within a slot the pairs are ordered by sha256(name TAB lane), as in A22's slots. lab_jobs.txt (fc8e1675ba8ca0bb...) records every sha256.
  2. Run. Each pair runs as in the M0 label harvest, through its runner, lab_runner.sh (sha256 2cabd6bb32b1b4407c73ac5748ed0378014f63cd7e4a5f014f01f2582158ca03), unchanged:
     - `FindProof --lane=NAME --szs --proof --wall=60` under env -i with TPTP set to the corpus root, inside timeout -k 5 120, with THVM_ATP_HISTORY_EXPORT at THVM_ATP_HISTORY_RATE=1 (no CPFEAT);
     - --work-clock=instr on the 37 clock-gated lanes of gated.txt (db3ac941e3d991dd...): A19's reading of the binary's --list-lanes=flags through run_arms.py, as the M0 harvest read it. Each pod derives the list again, and it must match;
     - then np0_extract.py (b72b36c2c5b25bfc..., the export v2 reader; max_neg 100000; at most 900 s) turns the export and the emitted proof into npz/<problem>__<lane>.npz, and the raw export is deleted;
     - no job starts after 14,400 s of a launch.
     The binary is G2.6's and A22's: the gcc 11.4.0 competition build of main 6a4b244b from G2.6's archive (thvm_src.tgz 65e6b2dc...), built on every pod. A pod runs nothing unless all of these hold: the build is 11fad6a4cb9452f51bcf0544072fdb306878723538234e32293a6ebdd31f9350; --list-lanes equals lanes.txt (550f2f6f...); the clock-gated list equals gated.txt; the perf counter opens after kernel.perf_event_paranoid=1 (without it the instruction clock silently falls back to the CPU clock); np0_extract.py has the sha256 above and passes its selftest. The pod scripts are pinned by in/SHA256SUMS (47c4d93fc0252cad...), which every pod checks.
  3. Pods and width. At most 4 atp CPU_NODE pods at once, not spot, with 16 to 64 vCPUs and at least 4 GB per vCPU. A slot runs at width 14 on 16 CPUs pinned by taskset (CPUs 16h to 16h + 15), A20's width, so that 14 x 4500 MiB fits per 16 vCPUs. The M0 harvest ran at width 16 on each 16-CPU half of a 128 GB pod; on the gated lanes the instruction clock keeps every clock-read decision independent of the load. A slot whose runner ends status=deadline resumes on the same CPUs of the same pod; the runner skips the pairs that have a row. A slot never runs on two pods: if its pod is lost before the slot is harvested, the slot reruns whole on another pod.
  4. Withheld recheck. When a slot's runner ends complete, every proved pair whose extraction found no proof block (FindProof withheld the proof) runs once more on the same CPUs with the export off (recheck.sh); command, clock and timeout are otherwise unchanged. The recheck records whether FindProof withholds the proof again, and why. The M0 harvest checked 4 of its 22 such pairs this way, by hand.
  5. Harvest. A slot is packed on its pod: index, sentinels, npz, proofs, stderr tails and the recheck. The archive is copied to the Mac, where its sha256 must equal the pod's and its rows the slot's job list. A pod is terminated only after its slots and its records (kernel log, host facts) are on the Mac.
  6. Validation (summarize.py, sha256 45b359cf9f9f4478...):
     - completeness: one row per pair, with missing, extra and duplicate all 0. This is required in full; a slot resumes until it is complete;
     - every launch's .started names the binary, the runner and the extractor above, width 14 with nproc 16, wall 60 s, timeout 120 s, extraction 900 s, max_neg 100000 and 37 clock-gated lanes; all launches of a slot ran on one host; every .done reads status=complete with pending_left=0;
     - the clock column is instr on exactly the 37 gated lanes;
     - row sanity: every proved row is Unsatisfiable, and no row claims Satisfiable or CounterSatisfiable. Such a claim is a soundness alarm and stops the line, as a VerifiedBad does;
     - an npz file exists for exactly the rows with npz bytes, at that size;
     - no harvested name shares a J13 stem (j13_ueq_names.txt is read only as a stem exclusion list).
  7. OOM. Every pod's `sudo dmesg -T` is read before the pod ends. If it holds an OOM-killer line, the pairs of that pod's slots whose run or extraction ended on SIGKILL before their timeout lose their rows and rerun on that pod at width 7 (run_slot.sh TAG CPUS 7). These are the pairs with exit 137 under 115 s, a slice "signal 9", or an extraction that printed nothing. The rerun rows replace the old ones, and the count is reported. Without an OOM-killer line nothing reruns.
  8. Reading. index.tsv has the M0 index's header and its 19 columns. Column 18 reads as in the M0 index:
     - proof and hindsight as the extractor reports them;
     - frontend for a proved pair with no export, no closed session or no selection;
     - withheld for a proved pair whose extraction found no proof block;
     - none for an unproved pair without an export session;
     - any other extractor line is extract-failed, which counts as a failure.
     summary.txt gives the totals and, per family (M0's family map) and per lane: pairs, proved, labelled (proof mode with positives), frontend, hindsight, withheld, rows and positives. It also lists the pairs not proved, with their SZS status and mode; the withheld pairs, with FindProof's reason and the recheck's outcome; and the extraction failures. frontend_pairs.tsv lists the matrix's front-end pairs and every pair the harvest calls frontend, with the harvest's mode, rows, positives and selections, as M0's did. The harvest decides nothing by itself: its labels are the TRAIN-X input of M2 deliverable 3 and of M3's trainer.
  9. Outputs, in ~/.cache/thvm/enigma/labels_x/: npz/, proofs/, err/, index.tsv, summary.txt, frontend_pairs.tsv and withheld.tsv. run/ holds the inputs and scripts, the slot archives with their sha256 and the pod records.
- A24 (2026-10-07, before any loop-0 harvest, any armed run and any M3 row; the user chose this procedure on 2026-10-07 over keeping the registered set and over adding tgt-sel to it): the twin families are chosen by section 5.3's rule over TRAIN and TRAIN-X together, and DEV-X joins M3's online gate sets.
  1. Why. The registered set (unfold, twee and flatten-bare-raw; "Twin families, final" in the 180 s outputs below) came from TRAIN-T alone: 7 problems, each owner holding 1 to 3, so ties and the stem split set it. D1 made TRAIN-X training data (A22 item 6), and its hard tail holds 48 problems. The pool is 9% -10 (section 6.1), J13 is 29% -10, and the -10 class holds 26 of the 36-problem gap (D1).
  2. Tail. TRAIN-T as registered (train_t.tsv, owners in the 180 s view), plus the TRAIN-X hard tail by section 1.4 in the X matrix: condition 1 at 60 s holds for 48 problems and condition 3 for none; condition 2 needs a 180 s run, which A22 does not make. TRAIN-X needs no DEV-T half, since DEV-X is already its held-out split.
  3. Owners. A tail problem is owned by its fastest lanes, every tied lane counted, as --decisions counts TRAIN-T. Equal counts rank in SCHEDULE order.
  4. Time per selection. T5's median of steps per CPU second, over the rows with steps and CPU above 0, pooled over TRAIN's 60 s rows (610 problems) and TRAIN-X's (1,464). The primary reading stays a head of 35 us, so a family needs at least 700 us per selection. The candidates select about 3x slower on -10 rows (2,180 to 2,860 us) than on pool rows (433 to 1,007 us), so the pooled median falls between.
  5. Eligibility. A twin needs a queue to reorder. horn-cd searches in the CLI's condensed-detachment loop (A5), and horn-back in the CLI's iterative-deepening backward chaining (tools/atp_cli/horn_back.c), so neither is eligible. Ineligible lanes are removed before the top 4 are taken, so the next eligible lane takes the place horn-cd would hold. The 2026-10-05 set left that place empty.
  6. Result. matrix_tables.py --xtwins writes decisions_a24.txt (registry row "twin families (amendment A24)"). Tail problems owned (TRAIN-T + TRAIN-X), then us per selection (TRAIN / TRAIN-X / pooled):
     - tgt-sel: 0 + 9; 775 / 2,180 / 1,622;
     - encompass: 3 + 6; 433 / 2,569 / 1,176;
     - twee-deep: 0 + 8; 541 / 2,608 / 1,250;
     - horn-cd: 1 + 5; ineligible;
     - unfold-tgt-awr: 0 + 4; 941 / 2,543 / 1,631;
     - twee: 1 + 3; 1,007 / 2,860 / 1,705.
     The twin families are tgt-sel, encompass, twee-deep and unfold-tgt-awr. unfold-tgt-awr and twee both own 4, and SCHEDULE order ranks unfold-tgt-awr first. These four replace unfold, twee and flatten-bare-raw. Other readings, recorded but not decisive:
     - TRAIN alone gives the registered unfold, twee, flatten-bare-raw;
     - TRAIN-X alone gives the same four as the pooled reading;
     - at a head of 113 us the pooled reading gives ifeq and lazy-gj.
  7. Gates before loop 0. Each family's base lane must pass G2.1 and G2.2 before its loop-0 harvest:
     - G2.1: tgt-sel and encompass ran it (registry row G2.1: PASS). twee-deep and unfold-tgt-awr did not, so each gets 10 TRAIN rows by A4 (the first 7 conclusive rows judge), picked from the lane matrix's 60 s engine proofs.
     - G2.2: 8 problems per lane, under the steps clock (A8).
     - A lane that fails either gate is not guided (M2's kill rule). The next lane in decisions_a24.txt's replacement order (twee, then wm, equals-twee, unfold, ...) takes its place under the same gates, until 4 families pass or the order runs out.
  8. Loop 0 and the training data.
     - Loop 0 is the engine itself (zero-correction twins) in all four families.
     - The D32h64 head was trained on the twee lane, which is no longer a family. M3's in-schedule D32h64 reading on DEV stays a twee-lane reading (E against PD at no training cost) and is no family's loop-0 head.
     - The heads train on the M0 harvest (TRAIN) and the TRAIN-X harvest (A23) together.
     - OOF folds and VAL are cut by stem over TRAIN and TRAIN-X together (stem() for pool names, x_stem() for corpus names), so a stem's pool and -10 problems share a fold.
  9. Gate sets. DEV-X (396 problems, A22) joins M3's online gates, which until now read no -10 set:
     - G3.2's single-lane set becomes TRAIN OOF plus DEV-T plus DEV-S plus DEV-X at WDEP. The selections geomean and the per-pick cost read the whole set. The DEV-S net (at least +5, with the sign test) and K3b's DEV-S reading stay on DEV-S alone.
     - G3.3's set becomes DEV-H plus DEV-T plus DEV-S plus DEV-X, with the same four bars. Its DEV-T-alone bar stays on DEV-T.
     - DEV-X problems run from the corpus root (A22), and the stem-clustered bootstrap clusters them by x_stem.
  10. What A24 does not change: the text of section 5.3's rule, the cost bar, the 4-family cap, M1's paired tests (DEV-H plus DEV-T), the loops, and the kill points K3a to K3c.
- A25 (2026-10-07, before any DEV-S source run, measurement or A/A pass; no DEV-S candidate or row exists): how DEV-S (M2 deliverable 4) is built, measured, frozen and judged by G2.5. tools/atp_enigma/devs.py implements every reading below and is committed with this amendment (a later fix that keeps these readings replaces its registry sha256 before the run it serves); tools/atp_enigma/test_devs.py tests it on synthetic exports and rows (pods only: every pod runs it before its first source job, so the first pod runs it before any source run).
  1. Sources. The pool's heldout split (the matrix problems.tsv, sha256 4b5a54bd2a60b463..., 171 problems) minus every name of dev_t.tsv and train_t.tsv (4468a736..., 6061641e...): 169 problems, since COL064-1 and REL033-3 are the held-out DEV-T problems (sources.tsv a674dcf8f6a64d08...). Their positive records form 129 axiom systems (item 4), 103 of them with a single source. No J13 problem is a source, and no J13 file reaches a pod.
  2. Source lanes. A lane is a source lane when its flags, in its binary's own `--list-lanes=flags` table and read as A19 reads them (the last value of a repeated flag wins, a bare flag is 1, a value starting with 0 is off), leave none of these on: --equals-goal, --narrow-queue, --horn-cd, --horn-back, --fof-slice, --def-unfold, --tgt. Those encode the goal into the axioms, rewrite or add axioms, or search outside the engine. Goal naming (--flatten-goal, which twee, the plan's own example, carries), the canonical presentation and the search-control flags leave the axioms as written. On binary 11fad6a4... (its table lanes_flags.txt d70ceec8dd036288...) the rule gives 20 lanes, in SCHEDULE order: wm, flatten, flatten-bare, twee, twee-deep, twee-freq, tgt-sel, otter-lpo, otter-lpo-cmin, otter-ladr, mnf-gj-amw, encompass, bare, ifeq, twee-raw, bare-raw, flatten-bare-raw, wm-gj, lazy-gj, bare-gj (source_lanes.txt e4e1a3bffa1ef919...). The other 26 are equals, narrowq, equals-twee, horn-cd, horn-back, the 15 fof-* lanes, unfold, unfold-tgt, unfold-tgt-awr, tgt-ax, tgt and tgt-fold.
  3. Source runs. Every source on every source lane: 3,380 jobs (source_jobs.tsv 9f0685b2bad4e1b6...). The problems are dealt in (domain, name) order round robin to 8 slots, all lanes of a problem in one slot, and a slot's jobs are ordered by sha256(name TAB lane): slots/slot_1.tsv holds 22 problems (440 jobs), the other seven 21 (420); jobs.txt in ~/.cache/thvm/enigma/devs/in/ lists every sha256.
     - Each job is `FindProof --lane=NAME --szs --wall=180 problem` under env -i with TPTP set to the pool root (tptp_pool.tgz f3db85ff...), inside timeout -k 5 240, stdin from /dev/null, with THVM_ATP_HISTORY_EXPORT=<file> and THVM_ATP_HISTORY_RATE=1: no --proof, the default CPU clock, --lane's 4500 MB RSS cap. Then `devs.py extract` (nice 5, at most 900 s) writes the job's candidate rows, and the raw export is deleted on the pod.
     - One binary for every DEV-S step: the competition build `make -f tools/casc/Makefile bin/FindProof CC=gcc` (gcc 11.4.0, Ubuntu 22.04) of main at this amendment's commit, from `git archive`, built on every pod. No file the build compiles has changed since 6a4b244b (the only change, src/backend/cuda/jit.c, is outside it), so the build must be 11fad6a4cb9452f51bcf0544072fdb306878723538234e32293a6ebdd31f9350, G2.6's, A22's and A23's binary. A pod runs nothing unless that sha256 holds, `devs.py lanes` over its build's --list-lanes=flags equals source_lanes.txt, and test_devs.py and `np0_extract.py --selftest` pass.
     - Pods, width, resume and OOM as A22 items 3 and 4: at most 4 atp CPU_NODE pods with 16 to 64 vCPUs and at least 4 GB per vCPU; 14 jobs per 16 CPUs pinned by taskset, so a 32-vCPU pod runs two slots; no job starts after 14,400 s of a launch; a slot resumes on its pod and never splits across pods; an OOM-killer line reruns the slot's SIGKILL rows at width 7.
     - A slot is harvested as its candidate files, its job index and its sentinels; no export leaves its pod. In the 60 s lane matrix 587 of these 3,380 pairs ended on the wall, so about 600 runs take the full 180 s.
  4. Candidates (extract, on the pod), as devs.py's header defines them:
     - The kept records of a source: every include() and every record whose literals are all positive, verbatim and in order. Every record with a negative literal goes, whatever its role (GRP720-2's goal has role axiom). Positive negated_conjecture units stay: FindProof's .pr carries no clause role, so every lane derives rules from them (only --tgt marks them goal-derived), and such a rule follows from the kept records only if they stay. An included file with a negative record or a nested include makes its source unsupported; none of the 169 is.
     - The axiom system: ifeq10_diff.problem_key over the positive records, includes resolved, every role read as axiom.
     - A candidate: a 'Q' record of kind born (a selected CP oriented into R or E) with content, in a version-2 session that is no fof-split branch, born at step n_sel // 2 or later (n_sel is the session's 'E' step, else its largest 'S' step: np0_extract.py's second half). It has 5 to 60 symbols (function symbols, constants and variable occurrences of both sides); each symbol has a 'Y' record that is neither fresh, truncated nor arity-saturated and names a symbol of the positive records with that arity; its sides differ; and it is no instance of a positive unit equation (one-way matching, either orientation). Fresh and goal-only symbols therefore never reach a goal.
     - The ground goal: the variables, in first-occurrence order, become fresh constants devsk0, devsk1, ... (with a 'd' prepended while a symbol of the source is that prefix and digits), written `lhs != rhs` as cnf(devs_goal, negated_conjecture, ...).
     - The goal key is the sha256 of the smaller of the two orientations' texts with every symbol and every variable renamed by first occurrence: blind to symbol, variable and ground-constant renaming and to orientation. The rank is sha256("20261001|devs|" + axiom-system key + "|" + goal key).
     - A job writes its first 32 candidates in rank order (a repeated goal keeps its first occurrence) and says whether it dropped more.
  5. Selection (filter, on the Mac), in this order:
     - j13: the goal key is the key of a literal of a J13 goal clause, that is, of a negated_conjecture record or any record with a negative literal of ~/.cache/thvm/casc/J13/UEQ/*.p (includes not read; the 400 files give 311 distinct keys). The rule reads "modulo renaming" as symbol and variable renaming, section 1.4's reading for hindsight goals, which covers ground-constant renaming; sign and orientation are not read. filter prints only counts and writes nothing derived from a J13 file.
     - known: the same against the problems of the pool's train and heldout splits and of TRAIN-X and DEV-X (2,695 files, 1,574 keys). This filter is added here so that no training problem and no other gate problem (DEV-H, DEV-T, DEV-X) recurs in DEV-S, the source's own conjecture included.
     - dup: the rank of a candidate earlier in (source, lane, session, birth step) order, that is, the same goal in the same axiom system.
     - cap: past the 10th by rank in its axiom system.
     - total: past 1,000 in the order (position within its axiom system, rank), a round robin over the axiom systems. This cap bounds the measurement: each candidate takes at most four runs (60, 180, 120 and 360 s) with their teardown and two CheckProof runs, about 900 slot-seconds, so 1,000 candidates take at most about 250 slot-hours. A candidate that P0 proves in seconds costs seconds, and 55 of the 129 axiom systems have no source-lane run over 1 s in the 60 s matrix.
     The rank decides every choice; difficulty decides none. filter refuses to run (exit 3) on a job without a candidate file, a candidate file with status=error, mixed --top values, or a source whose bytes (src_sha) the Mac's pool copy does not reproduce. A truncated job with fewer than 10 rows left after j13 and known would make its axiom system's ranking inexact; the summary lists every such job (none is expected). The output, in ~/.cache/thvm/enigma/devs/cand/, is the TPTP root of every DEV-S run: Problems/<DOM>/<source>-S<k>.p (k in rank order within the source) and the Axioms files they include; the manifest candidates.tsv (candidate, source, lane, axiom-system key, birth step, size, sha256 of the problem file; then path, session, n_sel, oriented, rank, goal key, goal); problems.tsv (run_arms.py's list, split devs); drops.tsv, files.sha256 and summary.txt.
  6. Measurement. P0 is the arm `P0<TAB>FindProof<TAB>--schedule` on item 3's binary, run through tools/atp_enigma/protocol/run_arms.py (protocol fingerprint efeb738a5185; split devs, so no seal):
     - THVM_ATP_SCHED_MAX_PAR=16, `--szs --proof --wall=W`, strict CheckProof built from the same archive (make -f tools/proover/Makefile bin/CheckProof), seed 20261001;
     - --rss-cap default, so on pods with an effective memory of at least 16 x 4500 MiB: a 32-vCPU / 128 GB pod pinned to CPUs 0-15, one run_arms.py at a time, as L0 ran;
     - the pod calibration against calib_ref.tsv (f1172af0...) with the submission build (65abe487887f) as --calib-bin;
     - slice stderr at run_arms.py's default unless A12's DEV-H check registers --no-errdir for timed passes; one setting for every stage and the A/A pass;
     - --passes 1 --no-repeats: a band reads one pass per wall, and item 8's A/A pass measures the noise.
     Each wall is its own run, in three stages, each sharded over the pods with --shard K/N:
     - S1: --walls 60,180 on every candidate;
     - S2: --walls 120 on S1's misses at 60 s (`devs.py stage --wall 60`);
     - S3: --walls 360 on S1's misses at 180 s (`devs.py stage --wall 180`).
     A 2W run is made for the W misses only, because the band reads it for them only. There is no prescreen. A 60 s prescreen (dropping from the 180 s runs every candidate S1 solves within 15 s at 60 s) would save only runs that P0 ends within seconds, a few slot-hours at most: the cost lies in the candidates near or past the walls, which no prescreen may drop. So every candidate gets every run the band reads, and nothing needs an audit. A stage resumes until every listed candidate has its rows (A16), and one candidate's runs of one stage never split across pods.
  7. Band (devs.py band). At W in {60, 180} the decisive time is the W run's when it is a Solution, else the 2W run's when that one is. A candidate is in the W band when its decisive time lies in (W/2, 2W]. A W miss whose 2W Solution came within W/2 is erratic: it is counted and stays out. DEV-S is the union of the two bands. band refuses rows of more than one binary or run condition, or a missing required run (exit 3), and stops on a VerifiedBad (exit 4; the line stops, section 1.3).
  8. The freeze and the A/A pass. band writes devs.tsv (name, path, split, the two band flags, the axiom-system key, the problem's sha256). Its sha256, and that of an archive of item 5's root (every candidate's problem file and the Axioms they include), enter this registry before the A/A pass and before any ENIGMA arm runs on DEV-S. Nothing changes the list after that, neither the A/A result nor any later run. The A/A pass is run_arms.py --passes 2 --no-repeats --walls 60,180 over devs.tsv, with item 6's binary and conditions (devs.py gate refuses others); both passes of a problem run on one pod.
  9. G2.5 (devs.py gate) passes when all three hold:
     - at least 60 DEV-S problems in the WDEP band and at least 30 in the W180 band;
     - at least 15 distinct axiom-system keys among the DEV-S problems;
     - at 60 s and at 180 s alike, at most 5% of the DEV-S problems have two A/A passes that differ on Solution at that wall (D_W / |DEV-S| <= 0.05).
     If G2.5 fails, M2's kill rule applies: M3's online gates move to DEV-T and DEV-H with the selection-budget metric, and their power is entered here before any M3 result exists.
  10. Outputs, all in ~/.cache/thvm/enigma/devs/: in/ (items 1 to 3), tptp_pool/ (the Mac's copy of tptp_pool.tgz), sources/ (the slot harvests), cand/ (item 5), measure/ (the stage rows), band/ (item 7 and devs.tsv) and aa/ (the A/A rows).
- A26 (2026-10-07, before any DEV-S measurement run; no S1, S2, S3 or A/A row exists): how A25's measurement (items 6 to 9) is dispatched, harvested and frozen, and a fix to devs.py's row reader. A25 fixes the runs and their readings; this fixes what it leaves open.
  1. The devs.py fix. run_arms.py writes rows.tsv with its header behind a '#' ("#problem<TAB>split..."), and devs.py's stage, band and gate took that line as written, so every real row file stopped them with a KeyError; test_devs.py's synthetic rows have a plain header, and a dry run of the dispatcher on rows in run_arms.py's format found it. The fix reads the header as run_arms.py's own reader does: the '#' is dropped, and a later '#' line or a row with another field count (a line a crash cut short) is skipped, so such a run counts as missing. No reading of A25 changes. devs.py becomes 382c49c16cf2a615... and test_devs.py 920abac7ebd87810... (RunArmsRows: BandGate's tests on rows written by run_arms.py's own writer); registry row "devs.py and test_devs.py after amendment A26's fix". Every measurement pod runs the new test in a copy of the tree before its first task.
  2. Inputs. Every pod gets one bundle (in/SHA256SUMS 398c6c18a4ff14ee..., checked file by file before anything runs):
     - thvm_src.tgz b9153fff..., the source runs' archive of main 881d6505 (its tar stream is `git archive --prefix=thvm/ 881d6505`, 22977fe0...);
     - tptp_pool.tgz f3db85ff..., the submission build FindProof.tgz 5262b361... (binary 65abe487887f...) and calib_ref.tsv f1172af0...;
     - devs_root.tar eeaf9c5ce4b49017...: a ustar archive of the 1,015 files of files.sha256 (a2c2b8f4...) at their root paths, in path order, mode 0644, owner 0, mtime 0, no directory entries. It is the archive of item 5's root that A25 item 8 registers;
     - the S1 list (cand/problems.tsv e6696a3d...), lanes_flags.txt, source_lanes.txt, item 1's devs.py and test_devs.py, and the pod scripts.
     Setup builds FindProof (it must be 11fad6a4...) and CheckProof (`make -f tools/proover/Makefile bin/CheckProof`, cc = gcc 11.4.0) from that tree. It checks the lane table, `devs.py lanes`, and the sha256 of devs.py, test_devs.py, np0_extract.py and run_arms.py (6eff1450..., the WDEP calibration's). It runs test_devs.py, `np0_extract.py --selftest`, protocol/test_stats.py and item 1's test, which must all pass; checks every DEV-S file against files.sha256; and runs `run_arms.py hostfacts` pinned to CPUs 0-15 (16 effective CPUs, the default caps fit) and `run_arms.py plan` (protocol fingerprint efeb738a5185). The first pod's CheckProof sha256 is the measurement's, and a pod whose build differs stops the dispatcher.
  3. Slice stderr. A12's DEV-H check has no registered outcome: no registry row records it, and G0.3's WDEP runs used --no-errdir because they are calibration runs without CheckProof, not that check. So A25 item 6's default holds: THVM_ATP_SCHED_ERRDIR is set in every stage and in the A/A pass, and every row's cond reads errdir=1.
  4. Pods. CPU_NODE, not spot, 16 to 64 vCPUs and at least 80 GB listed, the cheapest listed offer first, one create round every 2 minutes, at most 4 atp pods at once (any atp-* pod counts). run_arms.py's preflight then refuses a pod whose effective memory is below 16 x 4500 MiB. One run_arms.py runs at a time on a pod, pinned by taskset to CPUs 0-15; the pod's other CPUs only pack harvests. A pod that fails a calibration is ended and its shard reruns whole elsewhere; an offer is not used again after two such failures or one failed hostfacts check.
  5. Shards.
     - S1 is `--shard K/16` over the S1 list: run_arms.py's split, (domain, name) order dealt round robin, 62 or 63 candidates each.
     - When all 16 are harvested, `devs.py stage --wall 60` and `--wall 180` over their rows give S2's and S3's lists. S2 runs in ceil(n/40) shards and S3 in ceil(n/20), S3's first.
     - The A/A pass runs in ceil(|DEV-S|/15) shards.
     - Shards start in that order on whichever pod is free. Each is one run_arms.py out directory with its own pod calibration (run_arms.py's: two passes of the 10 reference problems at its start) and `--deadline-utc` 12 h after its launch, at most 1 h before item 10's hard deadline.
     - A shard whose runner ends status=deadline or terminated, or fails before it completes, resumes on its pod, at most 4 launches; after that the dispatcher stops.
     - A shard runs on one pod only. One whose pod is lost, or unreachable over ssh for 60 minutes, or whose calibration fails, reruns whole on another pod in a fresh directory.
  6. Harvest. A finished shard is packed on its pod: the out directory without scratch/ (rows, decisions, lanes.jsonl, runs/, calibration, sentinels), its launches, finished records and runner log. It is copied to the Mac, checked against the pod's sha256 and unpacked into measure/S1/, S2/ or S3/, the A/A shards into aa/. It counts only with run.done status=complete, base_jobs b/b, b rows, all from one host, and no VerifiedBad. A pod is ended only after its shards and its records (kernel log with its OOM-killer lines, host facts, build, setup and test logs) are on the Mac; then it is terminated, its absence from prime pods list confirmed and its ssh stanza removed. OOM kills are recorded per row (A9) and per pod; no rerun rule applies, since none is registered.
  7. Stops. A VerifiedBad (run.stop) halts: no shard starts, and every live pod is packed as it is, harvested into measure/forced/ and ended. A failed deterministic setup stage stops: no pod is created and no shard starts, while running shards finish.
  8. Band and freeze. When every S1, S2 and S3 shard is harvested, `devs.py band --manifest cand/candidates.tsv --rows <every S1, S2 and S3 shard> --out band/` runs on the Mac. The freeze is a commit on main that adds band/devs.tsv, byte for byte, as docs/plans/enigma_devs.tsv, and a registry row naming its sha256, devs_root.tar's sha256 and the band counts. The A/A shards are defined only after the dispatcher finds that commit on main, with both sha256 values in its registry and the committed file equal to devs.tsv. While it waits, an idle pod is ended 60 minutes after the band.
  9. A/A and gate. The A/A shards run `run_arms.py run --passes 2 --no-repeats --walls 60,180` over devs.tsv, with item 2's binary and A25 item 6's conditions. When all are harvested, `devs.py gate --band band/ --rows <every A/A shard>` decides G2.5 (aa/gate.txt).
  10. The dispatcher (measure/run/bin, SHA256SUMS.bin 23af58cafd25464c...) runs detached on the Mac from a snapshot copy, with a tick every 2 minutes: acquire, set up, launch, resume, harvest, stages, finish. Past its soft deadline, 84 h after launch, it creates no pod and starts no shard; past the hard deadline, 90 h after launch, it packs, harvests and ends every live pod. A pod with no shard is ended at once when nothing waits and no stage can follow, 60 minutes after the band while the freeze is awaited, and otherwise after 180 minutes idle. A stage left incomplete at a deadline resumes from the same state when the dispatcher is launched again.
- A27 (2026-10-07, before any G2.1 run of twee-deep or unfold-tgt-awr and before any G2.2 run; no row of either exists): how A24 item 7's gates run, on the binary the loop-0 harvest will use.
  1. Binary. The gcc 11.4.0 competition build (`make -f tools/casc/Makefile bin/FindProof CC=gcc`, Ubuntu 22.04) of the commit that adds FindProof's phi(p) seam, the sampled whole-run dump and the `harvest` head (branch enigma/loop0; docs/atp/guide.md), from `git archive`, built on the pod. Its commit and sha256 go into the registry row. The gates run only after that build passes the pod battery on the same pod (stage A): test_atp in full, test_atp_guide, test_atp_guide_ac, test_atp_problem_features, the OA and DN pins unarmed and as zero twins of the harvest head with the sampled dump on (selections identical), the generator round trip, the self-tests of np0_extract.py, label_join.py and guide_net.py, tests/atp_guide_phi.sh, the G2.2 script's default spec, an end-to-end run of the harvest runner, the label join, the trainer and a compiled-in trained head with G2.4 parity and featparity, and a clang-14 build. Stage A decides no gate; a failed stage A stops the run before any gate row.
  2. Rows. tests/atp_guide_a27.tsv (sha256 31426016981d9b37...): per lane, 10 TRAIN engine proofs of the sealed lane matrix (matrix.tsv 1f3470d7dfa3b975...) at 60 s in 1 to 40 s, spread over that range (`tests/atp_history_v2_obs.sh --pick`, A4's tool), for the four families of A24 and the twelve lanes of decisions_a24.txt's replacement order. TRAIN is the 610 problems of the 180 s decisions (problems_TRAIN.tsv, problems.tsv's rows of them, sha256 527c0db979f68ad6...), so no J13 stem-mate and no DEV-T problem is a gate row; the 2026-10-05 list read the matrix's train split.
  3. G2.1 (A3, A4, A8) runs for the lanes the 2026-10-05 row did not judge: twee-deep and unfold-tgt-awr, and a replacement lane other than twee, wm and flatten-bare-raw when it is reached. `tests/atp_history_v2_obs.sh <binary> <pool root> <the lane's rows> <out> 180`: steps clock, A/A on, A4's reading (the first 7 conclusive rows judge, any FAIL fails). tgt-sel, encompass, twee, wm and flatten-bare-raw keep their registered PASS: no file under src/atp but guide.c and guide_models.c, the layer and its heads, has changed since that row's binary (main 6a4b244b), and the export (src/atp/history.c) is untouched.
  4. G2.2 runs for every lane reached: tests/atp_guide_gates.sh with MODEL=harvest (the twin then also runs the phi(p) seam and the loop-0 harvest's top-64 scoring), DUMP_EVERY=7 and DUMP_MAX=2000 (the loop-0 harvest's sampled dump), AA=1, WALL=180 and TPTP the pool root, over the lane's 10 rows. Base `THVM_ATP_WORK_CLOCK=steps FindProof --lane=L --proof --szs --wall=180 p` against the twin, the same run with THVM_ATP_GUIDE=L THVM_ATP_GUIDE_ZERO=1 THVM_ATP_GUIDE_MODEL=harvest and the dump variables, the base padded with THVM_ATP_GUIDP* variables of the same lengths in the same places. A row passes only on exact identity of the selections, the clock-gated decisions, stdout and the engine stats (A8). A row whose two base runs differ is NONDET and does not count (A4's A/A reading, carried over to G2.2). A lane passes when none of its 10 rows FAILs and 8 PASS; with fewer than 8 PASS and no FAIL it is INCONCLUSIVE, which does not pass.
  5. Order (A24 item 7). G2.1 and G2.2 for the four families first, in parallel. While fewer than four lanes pass both, the next lane of the replacement order (twee, wm, equals-twee, unfold, unfold-tgt, flatten-bare, bare, ifeq, tgt, bare-raw, flatten-bare-raw, lazy-gj) takes the next place under the same gates, until four pass or the order runs out. A lane that fails or is inconclusive in either gate is not guided (M2's kill rule).
  6. Pod and outputs. One atp-tr-K CPU_NODE pod (16 to 64 vCPU, at least 2 GB per vCPU, not spot; at most 4 atp pods at once, every atp-* pod counted), run by a detached dispatcher on the Mac (~/.cache/thvm/enigma/train/run/bin, from a snapshot copy) that sets the pod up, runs stage A and then stage B, packs and copies each stage's summaries and logs (results/, with their sha256) and the pod's records (kernel log, host facts) before it terminates the pod. The registry row gives the binary, the stage A results, every gate verdict and the four families.
- A28 (2026-10-18, before any loop-0 harvest run; no row of it exists): the loop-0 harvest of A24 item 8 for A27's four families, its keys table and its label join. Section 3.2 fixes the harvest's flags and labels; this fixes the jobs, the runs' settings, the pods, the inputs, the outputs and the readouts. Every input and script below is pinned by the registry row "loop-0 harvest inputs (amendment A28)".
  1. Families. A27's: tgt-sel, encompass, twee-deep and unfold-tgt-awr, each past G2.1 and G2.2; the replacement order was not reached. Loop 0 is the engine itself in all four (A24 item 8): every run is the zero twin of the `harvest` head. The D32h64 head is no family's loop-0 head and is not harvested.
  2. Jobs. `guide_jobs.py` (e3749860a5b51d72...) over matrix.tsv 1f3470d7..., the pool's problems.tsv 4b5a54bd..., dev_t.tsv 4468a736..., xmatrix.tsv debca97b... and the corpus's problems.tsv ff72347fe57aea3b..., lanes tgt-sel,encompass,twee-deep,unfold-tgt-awr, picks 1000: jobs.tsv ec4bdc4544359525.... A job is a TRAIN (the 610 problems of the 180 s decisions) or TRAIN-X (1,464) pair of a family's base lane whose 60 s matrix row shows that the lane searched, proved or not: class proved, wall, gaveup-saturated, step-cap or memory-guard (docs/atp/guide.md, Training step 1). Not searched: fast-exit-not-applicable (the lane's own gate exits before any selection: tgt-sel 122 TRAIN and 220 TRAIN-X, encompass 1 and 15, twee-deep 1 and 15, unfold-tgt-awr 416 and 1,223) and the three crash rows (SWX233-1 on twee-deep and encompass, SIGSEGV after 250 selections; SYN986-10.006 on tgt-sel, SIGSEGV before any). 6,280 jobs on 2,058 problems (609 TRAIN, since no family lane searches PUZ056-10.030, and 1,449 TRAIN-X): tgt-sel 1,731 (487 TRAIN, 1,244 TRAIN-X), encompass 2,057 (608, 1,449), twee-deep 2,057 (608, 1,449), unfold-tgt-awr 435 (194, 241). Their matrix rows: proved 4,071, wall 1,745, gaveup-saturated 457, memory-guard 7; 118,473,181 selections.
  3. Run. Every job runs once, through `guide_harvest.sh` (8a522d854da472e9...), as `FindProof --lane=L --szs --proof --wall=60 --work-clock=instr` under env -i with TPTP the harvest root, inside timeout -k 5 120, with the history export v2 at THVM_ATP_HISTORY_RATE=1 (no CPFEAT) and the twin armed through the environment forms of `--guide=L --guide-zero`: THVM_ATP_GUIDE=L, THVM_ATP_GUIDE_MODEL=harvest, THVM_ATP_GUIDE_ZERO=1. The harvest head's one family `*` makes every family's integration top-K 64. The dump is THVM_ATP_GUIDE_DUMP at the job's rate (item 4) with THVM_ATP_GUIDE_DUMP_MAX=2000 and no row cap.
     - All four lanes are in gated.txt (db3ac941..., the 37 clock-gated lanes), so every job runs on the instruction clock, as section 3.2 and the M0 and TRAIN-X label harvests run them.
     - --proof is on every job, section 3.2's harvest flag. It changes only the output of a run that proves, and gives that run proof-mode labels, as in the M0 harvest.
     - Then `np0_extract.py` (b72b36c2..., max_neg 100000, at most 900 s) writes npz/<problem>__<lane>.npz: proof mode for a proved run, hindsight mode for an unproved run or a withheld proof. The dump is gzipped, the raw export and dump are deleted, and phi.tsv (`--problem-features` of every job's problem) is computed first.
     - No job starts after 14,400 s of a launch. A launch whose .done reads status=deadline is launched again on the same pod, at most 4 launches; the runner skips the jobs that have a row.
  4. The dump's rate. The export runs at rate 1. The dump runs at rate 1, every pick, on the 3,829 jobs whose matrix row has at most 1,000 selections. Above that it samples about one pick in every = ceil(selections / 1000), hash-sampled, so it holds about 1,000 picks spread over the whole run (guide_jobs.py's rule; docs/atp/guide.md). DUMP_MAX 2,000 leaves room for a run longer than its matrix row. The other readings are not used:
     - Rate 1 on every pick of every run would dump the 118 million selections of item 2, about 7 billion candidate rows and terabytes of text. label_join.py holds every group in memory.
     - Rate 1 under the 2,000-pick cap would keep only the first 2,000 picks of each long run, so no state from late in a long run. G3.1's second criterion reads exactly those runs (1000+ selections).
     - A27 item 4's DUMP_EVERY=7 was the gates' fixed rate for rows of 1 to 40 s. The rate changes which picks are written, not which are scored: the twin scores every pick, as the gates' picks=scored lines show.
     - The 250-pick job list prepared on 2026-10-07 (jobs_a24, 43963299725d4eea...) is not used. Picks 1000 is guide_jobs.py's default and the documented value.
     At the jobs' rates the matrix rows' selections give about 2.7 million dumped picks.
  5. What loop 0 is. Under the steps clock the twin's selections are the base lane's (G2.2). The harvest departs from that in two ways, and the readouts count both:
     - Wall cost. The harvest head scores the engine's top 64 at every pick (0.29 of OA's wall in A27's stage A, 0.02 to 0.83 in its smoke runs), and --wall=60 is a real-time alarm. So a run that reaches the wall makes fewer selections than its matrix row, and a matrix proof found late can end here on the wall, with hindsight labels instead of proof labels. The wall stays 60 s, the matrix's and the label harvests'.
     - Clock. On the instruction clock the twin's few dozen instructions per step outside its exclusion window count (src/atp/guide.c). A clock-gated decision can therefore fall at another selection than in an unarmed instruction-clock run.
     Each run's dump and export come from the same run, and the join checks every dumped selection against the export, so the states and their labels agree.
  6. Binary and tree. A27's binary 92f8e42474a96f88d2a2ea0b6c34b9b75207033db407beac4e93a2746cce4c62, built on the pod from thvm_src.tgz 4f771e5d1d0ab19e... (81007c73's `git archive`) with `make -f tools/casc/Makefile bin/FindProof CC=gcc` (gcc 11.4.0, Ubuntu 22.04). Main has moved since A27's row: f46e570c to f1b3baa5 change runtime sources that FindProof's single translation unit includes (src/thvm.c, through tests/test_atp_wolfram_bench.c). So the harvest builds the gated tree, not main. tools/atp_enigma is the same in both trees. A pod runs no job unless all of these hold (setup.sh):
     - the build's sha256 is the one above;
     - `--list-lanes` has 46 lanes, and the clock-gated list read from `--list-lanes=flags` (run_arms.py's parse_lane_listing and is_clock_gated) equals gated.txt;
     - the perf counter opens after kernel.perf_event_paranoid=1, at setup and again at every pipeline start;
     - guide_harvest.sh, np0_extract.py, label_join.py, phi.py, matrix_tables.py and run_arms.py have their sha256, and `np0_extract.py --selftest` and `label_join.py selftest` pass;
     - every TRAIN-X file matches trainx_files.sha256, and every job's path exists.
  7. Root. tptp_pool.tgz (f3db85ff...: the pool's Problems/ and Axioms/) plus trainx_loop0.tgz (5962792eb5998bc7...): the 1,449 TRAIN-X files the jobs name, at their corpus paths. trainx_files.sha256 (ca8e36be0a7e191a...) has one line per file, each a line of A22's corpus_files.sha256. No DEV-X file and no J13 file reaches a pod.
  8. Pods and width.
     - One atp-tr-K pod at a time: CPU_NODE, 16 to 64 vCPU, at least 4 GB per vCPU listed, not spot, 100 GB of disk.
     - Offers below 80 GB come first, since the DEV-S dispatcher needs 80 GB or more; then the cheapest per vCPU; then the most vCPUs. The listing is read every 30 s, and a failed create on an offer is retried after 2 minutes. At most 4 atp pods run at once: every atp-* pod in `prime pods list` is counted before every create.
     - The runner uses g = min(4, floor(vCPU / 16)) groups of 16 CPUs: width 14g pinned by taskset to CPUs 0 to 16g - 1. That is A20's 14 per 16 CPUs, 14 x 4500 MiB per 64 GB.
     - Every job runs on one pod, so on one CPU type. A pod lost before its pipeline ends (unreachable for 60 minutes) is ended, and the harvest reruns whole on a new pod (A23 item 3), at most 3 pods.
     - A pipeline that fails creates no new pod; the dispatcher harvests what exists, ends the pod and stops.
  9. OOM. The rule is A23 item 7's. The kernel log is read when the runner completes. If it holds an OOM-killer line, the rows a SIGKILL ended before their timeout lose their rows and their files and rerun at width 7g, at most 2 launches: exit 137 under 115 s, an extraction line naming signal 9 or Killed, or an export whose extraction printed nothing. Without such a line nothing reruns. The count is reported.
  10. The label sets and the keys table.
     - The pod gets the label sets once, while the harvest runs: both index files (M0 bb430a0a..., TRAIN-X 64959fb4...) and their proof-mode npz whose problem is a harvest problem. That is 8,518 M0 files (1.24 GB) and 18,695 TRAIN-X files (2.63 GB), listed in labels_m0.list and labels_x.list.
     - The copy is rsync, each file checksummed in transfer. On the pod, labels_check.sh checks the index files' sha256 and every listed file against its index size (no file missing, resized or extra), then records the sha256 of every uploaded file.
     - Nothing else of those harvests goes to the pod: keys reads proof-mode rows only, and the two TRAIN-X label problems outside the harvest are not needed.
     - When the harvest is complete and the labels are checked, the pod builds the keys table once: `label_join.py keys keys.tsv labels/index.tsv labels/npz labels_x/index.tsv labels_x/npz out/index.tsv out/npz` (label_join.py fc888ce100263bd7...). It has one row per proof-mode run of the M0 and TRAIN-X label harvests and of this harvest: its input signature (sig, the input transform) and its canonical equation keys.
     - keys.tsv leaves the pod and its sha256 goes into the registry. The npz stay on the pod.
  11. Join. `label_join.py join groups/ keys.tsv out/ --jobs <CPUs>` writes groups_<lane>.npz, stats.tsv and summary.txt. Own positives come from the run's proof; cross-lane positives from keys.tsv, between lanes whose sig is equal and never from the same lane; hindsight positives from failed runs. A run whose dump disagrees with its export or its phi(p) row is left out and listed with its reason; those are failures. Only TRAIN and TRAIN-X are read.
  12. Outputs and the archival copy. Everything is in ~/.cache/thvm/enigma/train/loop0/harvest/: in/ (the pod bundle), bin/ (the dispatcher), state/, results/POD/ and archive/POD/.
     - results/POD/ holds the small outputs: the index, every launch's sentinels, phi.tsv, the logs, keys.tsv, stats.tsv, summary.txt, the groups' and the bulk files' sha256, the OOM records and the pod's records.
     - archive/POD/ holds the bulk. The dumps, proofs and stderr tails are copied every 10 minutes while the pipeline runs. After it ends come groups.tgz, then npz/. Every copied file is checked against the pod's bulk manifest.
     - A piece is copied only if the Mac keeps at least 5 GB free after it (about 20 GB were free when this was written). A piece the floor stops is listed as not copied.
     - The groups can be rebuilt from the archive and keys.tsv: the join is deterministic, runs in problem order per lane.
     - A pod is terminated only after its results and its records are on the Mac. Its absence from `prime pods list` is confirmed, and its ssh stanza is removed.
  13. Validation (summarize.py, 51f8b1d8de4b103a..., on the Mac over the small results). Each check is PASS or FAIL:
     - completeness: one row per job; missing, extra and duplicate all 0;
     - launches: every launch's .started names the binary, the runner and the extractor above, width 14g (7g for an OOM rerun) for its nproc, wall 60, timeout 120, extraction 900, deadline 14,400, max_neg 100000, model harvest, zero 1, dump_max 2000, no row cap and 37 gated lanes, all on one host; the last .done reads status=complete with pending_left=0;
     - clock: instr on every row;
     - soundness: every proved row Unsatisfiable, and no Satisfiable or CounterSatisfiable claim (that stops the line, section 1.3);
     - the dump rate equals the job's on every row, and every split is TRAIN or TRAIN-X and the job's;
     - an npz file and a dump file exist for exactly the rows with their bytes;
     - the keys table was built and its sha256 recorded;
     - the join's lanes are the four families, with runs equal to the index's.
  14. Readouts, per family and over all four. They decide nothing: G3.1 is decided at loop 1 on armed-harvest states.
     - problems and runs;
     - proved here and in the matrix rows, and matrix proofs not proved here, by SZS status;
     - modes (proof, hindsight with the proofs withheld counted, frontend, none), extraction failures, timeouts and signal exits;
     - picks and scored picks (the `% guide:` lines) with frac_wall's quartiles, and selections here and in the matrix rows;
     - npz rows, own positives, npz and dump bytes;
     - the join's runs joined and skipped (with reasons), groups, rows, groups and positives by source (own, cross-lane, hindsight; cross-lane only, hindsight only) and the groups with a proof CP pending;
     - failures.tsv lists every run that failed and why.
- A29 (2026-10-18, before any harvest job; the first pod stopped at A28 item 6's binary check): A27's binary needs A27's toolchain.
  1. What happened. atp-tr-1 1039f294 (datacrunch, 16 vCPU / 64 GB, $0.19 an hour) built 5033def4eb0badcf... from the same checked tree, not 92f8e424.... Its image ships its own gcc-11 and libc6-dev, and A28's setup installed build-essential only where it was missing. A27's pod installed gcc-11 11.4.0-1ubuntu1~22.04.3 and libc6-dev 2.35-0ubuntu3.15 fresh, and both are still the jammy-updates versions. The pod ran no job; its records were taken (records.tgz 8964a6043c7070c5...) and it was terminated after about 4 minutes. The dispatcher, stopped by hand right after, had already created atp-tr-2 (3eecb918, datacrunch, the same offer). Its setup runs the fixed script.
  2. Fix. setup.sh installs exactly these before the build: gcc-11, cpp-11, gcc-11-base and libgcc-11-dev 11.4.0-1ubuntu1~22.04.3; libc6-dev and libc-dev-bin 2.35-0ubuntu3.15. It records the toolchain's versions, binutils and linux-libc-dev included, in toolchain.txt, which the results and the records carry. The build must still be 92f8e42474a96f88...; nothing else of A28 item 6 changes.
  3. A provider whose pod still builds another binary, or cannot install those versions, is not used again in this run. The pod budget becomes 4 pods, this first one included.
  4. New sha256 (registry row "loop-0 harvest inputs after amendment A29"): setup.sh, records.sh, pack_results.sh, in/SHA256SUMS, in.tgz and tick.sh. Every other input, script and reading of A28 stands.
- A30 (2026-10-19, before any loop-0 training run; no head, fold model or readout of it exists): the loop-0 heads of A24 item 8: their data, model, settings and width rule, the readouts, the export, the gates before the head lands, and the per-pick cost reading. Every input and script below is pinned by the registry row "loop-0 heads inputs (amendment A30)".
  1. Data. The loop-0 harvest's join (registry row "loop-0 harvest (amendment A28)"): groups_<lane>.npz of tgt-sel, encompass, twee-deep and unfold-tgt-awr (81c0e81f..., 94773f16..., 07e6de23..., ca86b180...; groups.sha256 d19f1fc2...), 4,224 runs, 2,016,696 groups, 125,979,715 rows. They reach the pod once, as the Mac's archive copy groups.tgz (ab5cf171...), checked against both sha256 lists before any model trains. They hold TRAIN and TRAIN-X runs only (A24); no DEV set and no J13 file reaches the pod. The 2,056 runs the join left out (665 without labels, 1,335 without a scored pick, 16 front-end proofs, 40 dump disagreements) stay out.
  2. Model. One lane-conditioned head over the four families (section 3.1): guide_net.py train over the four files, layout v1 with 64 inputs (the 20 candidate and state columns, the 40 phi(p) columns, and the lane one-hot over LaneNames in the build's --list-lanes order: unfold-tgt-awr, twee-deep, tgt-sel, encompass), one hidden layer, s = -kappa log1p(rank) + correction, starting from w2 = 0 and kappa = 1 (the engine), no output bias. Every family's integration is top-K 64, the harvest's candidate set (Families <lane>:<index>:topk:64:0:0, no coop). Layered-lazy is not trained at loop 0: a lazy head is a second model on rows with x[0] = 0 and needs its threshold chosen on VAL, so section 3.1's choice of the mode per family is read at loop 1.
  3. Settings: guide_net.py's documented defaults, --k 64, --epochs 200, --patience 10, --lr 1e-3, --seed 0 (the full model seeds 0, fold k seeds 1 + k), --max-groups-per-run 256 (each epoch draws at most 256 groups with a positive from each run; VAL is one fixed draw, seed 1234), --w-own 1 --w-cross 1 --w-hind 1, run weights (0.5 + rating) log10(10 + selections) with the median rating for unrated problems, --folds 5, --model-id loop0. The splits are A24 item 8's, over TRAIN and TRAIN-X together: a stem is VAL when int(sha256(stem + "|val")[:8], 16) % 100 < 15, else in fold int(sha256("fold|" + stem)[:8], 16) % 5, so a stem's pool and -10 runs share both. Every model stops early on its VAL listwise loss; the full model (every non-VAL run) at its best VAL epoch is the exported head, and fold k's model scores fold k's states (OOF). --max-hours 12 is a safety net: a model past it stops at its next epoch end with its best VAL epoch, and its record says so.
  4. Widths. Hidden 32, 64 and 128 (at most 128, section 3.1). Each width trains its five fold models and its full model, 18 models in all, each its own process (guide_net.py train --models) on one out directory per width. A model is a deterministic function of its data, settings and seed, so the grouping into processes changes no head; guide_net.py selftest checks a per-subset run against the all-in-one run.
  5. The width rule (guide_net.py choose), fixed here before any model exists: L_w is the full model's best VAL loss at width w and L* the lowest of the three; the chosen width is the narrowest w with L_w <= L* + 0.005 |L*|. The narrowest within 0.5% because the forward pass costs 64 candidates x 64 inputs x hidden multiply-adds a pick, under the 5% cost bar of section 2.4: a wider head ships only for a VAL loss lower than that. The rule reads the three VAL losses only, not the readouts and no gate set. Only the chosen width's full model is compiled in; the other heads and readouts are recorded.
  6. Readouts, not gates (G3.1 is decided at loop 1 on armed-harvest states): guide_net.py readout of each width, per family (lane), on VAL states under the full model and on OOF states under their fold models: the top-1 proof-CP hit of the head's pick against the engine's (row 0) over the selections of proved runs with a proof CP pending (per-run means, their ratio, a 95% interval resampling stems); on runs with 1000+ selections the median-percentile ratio with its interval; the any-source hit.
  7. Export. `weights_to_c.py build example=ex.safetensors harvest=hv.safetensors loop0=<the chosen full model> -o src/atp/guide_models.c`: example stays the first head and the default, harvest is unchanged, loop0 is the third. weights_to_c.py lanes checks loop0's LaneNames against the build's --list-lanes, and the round trip of the shipped two-head file is checked first. The head lands as tools/atp_enigma/models/loop0.safetensors, from which the build reproduces guide_models.c byte for byte.
  8. Gates on the pod, before the head lands, all on one tree: the code commit's archive (registry row) with the generated guide_models.c, which are the C sources that land.
     - Builds: the FindProof competition build (gcc 11.4.0 at A29's versions), the clang-14 build, test_atp, test_atp_wolfram_bench, test_atp_guide and test_atp_guide_ac.
     - G2.3: test_atp in full, test_atp_guide and test_atp_guide_ac; the OA pin 257971/735/2461555 and the DN pin 2848/254/768876 unarmed (THVM_ATP_WALDMEISTER=1 THVM_ATP_LAZY_NORM=1, test_atp_wolfram_bench 500000 280 and 500000 60).
     - G2.2 (A8, A27 item 4) with the new head: tests/atp_guide_gates.sh with MODEL=loop0, AA=1, WALL=180 and TPTP the pool root, over each family's 10 rows of tests/atp_guide_a27.tsv; a family passes with no FAIL and 8 PASS.
     - G2.4: tests/atp_guide_gates.sh with MODEL=loop0, HEAD the head, FEATPARITY=1, WALL=180 and DUMP_MAX=2000 over each family's first 8 of those rows. weights_to_c.py parity (each candidate's correction and score to 1e-9 in float64, the same decision at every dumped pick) and featparity (columns 0-12 and 15-18 and every top-K candidate set against the same run's history export, to 1e-9) must both PASS on all 8. That run's own G2.2 lines are recorded, not judged.
     The head lands only if every gate passes. A failure is diagnosed first; a fix that changes this procedure is a new amendment.
  9. Per-pick cost (section 2.4; a readout here, G3.2 reads the bar at loop 3). After the gates, with nothing else running on the pod and at most one run per physical core: per family, the same first 8 rows, the deployed twin `FindProof --lane=L --guide=L --guide-model=loop0 --szs --wall=60` on the default CPU clock without dump or export, and for reference the harvest head's zero twin (`--guide-model=harvest --guide-zero`: hidden 1, so the cost of finding and decoding the 64 candidates without the MLP). Each run's `% guide:` line gives frac_wall (the layer's time over the time since arming), us_per_pick and us_per_scored. Per family: the median and maximum frac_wall against 0.05, and the median us_per_pick against 5% of the family's pooled time per selection (A24 item 4: 1,622, 1,176, 1,250 and 1,631 us for tgt-sel, encompass, twee-deep and unfold-tgt-awr, so 81, 59, 63 and 82 us).
  10. Pod and dispatch. One atp-tr-K CPU_NODE pod: 16 to 64 vCPU, at least 4 GB per vCPU listed, not spot, 150 GB of disk; offers below 80 GB first (the DEV-S dispatcher needs 80 GB or more), then the cheapest per vCPU, then the most vCPUs; every atp-* pod counted before every create, at most 4. At most min(18, nproc - 1, (MemTotal GiB - 16) / 4) models train at once, each with OPENBLAS_NUM_THREADS=1. The widths' full models start one after another, and a width's other models only after its full model's first epoch: py/thvm's CPU JIT writes each kernel to a /tmp file named by its content hash, without a lock, so one process compiles every kernel and the others load them. A model that fails reruns once; a second failure or a "ran no dispatch rung" line stops the pipeline. The pipeline's deadline is 36 h. The dispatcher runs detached on the Mac (~/.cache/thvm/enigma/train/loop0/heads/bin, from a snapshot copy, with the system Python and no pyenv variables): it creates no pod after 24 h and ends every pod at 48 h; a pod unreachable for 60 minutes is lost and the pipeline reruns whole on a new pod, at most 3 pods.
  11. Outputs, in ~/.cache/thvm/enigma/train/loop0/heads/: in/ (the bundle), bin/, state/, results/POD/ (every model's head, record and log, the readouts, choose.txt, the generated guide_models.c and its heads, the builds' sha256 and logs, the gate summaries and per-row parity outputs, the cost table, results.tsv and the pod's records) and archive/POD/ (the G2.4 dumps, if the Mac keeps 5 GB free after them). The per-row scores (about 6 GB) stay on the pod: score_groups rebuilds them exactly from a head and the groups, which are both on the Mac. A pod is terminated only after its results and records are on the Mac; its absence from prime pods list is confirmed and its ssh stanza removed.
- A31 (2026-10-19, after A30's gates and before any rerun of them; the head has not landed): two of A30 item 8's gate readings could not judge the head, and how G2.2 and G2.4 rerun.
  1. What happened. A30's pod (atp-tr-1 cfc93677, registry row "loop-0 heads (amendments A30, A31)") trained the 18 models, chose hidden 128 and built FindProof dd5f400ac3c62104... with it; test_atp, test_atp_guide, test_atp_guide_ac, the OA and DN pins, G2.2 for tgt-sel, twee-deep and unfold-tgt-awr and G2.4 for tgt-sel and unfold-tgt-awr passed. Three verdicts failed, and neither cause is a disagreement between the twin and its base or between C and Python:
     - G2.2 encompass, LAT230-10: the zero twin stopped at its 177 s wall cap (SZS GaveUp) after 667,117 selections, and they are exactly the base's first 667,117 (cmp on the pod); the base proved at 707,409 and repeated (A/A). A zero twin runs the head's forward pass at every pick: 110.7 s on this run (frac_wall 0.62, 162 us a pick), where A27's harvest head (hidden 1) fit in 180 s. Under the steps clock the twin keeps the base's selections, not its wall time, so the row could not finish.
     - G2.4 featparity on 3 of 32 rows (encompass REL047-1 and GRP718-1, twee-deep GRP184-3): the guided run proved, but FindProof withheld its proof by its own derivation check ("t790 (orient) is not derived from what it cites", "t46465 (orient) ...", "no goal reaches $false"), so np0_extract.py found no proof block and featparity never ran. The harvest saw the same in these two lanes (proof withheld 91 and 92 times, A28 readout). parity passed on all 32 rows (largest relative difference about 1e-14, the same decision at every pick).
  2. G2.2 for the loop0 head reruns at WALL=600, so a twin that pays for the forward pass at every pick can finish what its base finishes; otherwise as A30 item 8 (MODEL=loop0, AA=1, the 10 rows of each family, no FAIL and 8 PASS). A row whose twin still stops at the wall fails, as before.
  3. G2.4 reruns with tests/atp_guide_gates.sh converting a run that proved but whose proof FindProof withheld in np0_extract's no-proof mode (proof path "-"), the guided harvest's own rule for such a run (A28 item 3): featparity reads the export's push, selection and deletion records, which both modes keep, and no label. Otherwise as A30 item 8 (WALL=180, DUMP_MAX=2000, the first 8 rows of each family, parity and featparity 8 of 8).
  4. Both rerun for all four families on one new pod, on the tree that lands: the archive of the commit that adds this amendment and the script change, with A30's generated src/atp/guide_models.c (202ff4f0aa5bc903...) copied in; its FindProof build must be A30's dd5f400ac3c62104..., else nothing runs. The pod, the dispatch and the records are A30 item 10's without the groups and the training (~/.cache/thvm/enigma/train/loop0/heads_a31/). A30's builds, G2.3 checks and cost readout stand: neither change touches the binary, test_atp, the pins or the cost runs.

## 6. Risks and what we will not do

### 6.1 Risks

| Risk | Evidence | Mitigation and detection |
|---|---|---|
| The learned speedup never reaches long runs | No online gate has passed. The steps ratio is 0.97 on runs of 10 s and more. | The long-run gates at loops 1 and 3 (K3a, K3b) end the core bet by February 2027. |
| Gains on the pool do not transfer to J13 | The pool is 48% GRP and 9% -10. The VBS needs at most 2.55 s on the sample. twee alone equals the 13-lane union. | Weights by rating and run length; DEV-T; DEV-S; D1. Detected by the per-domain nets at L1. |
| DEV-S does not resemble real conjectures | Hindsight goals are rules our own engine derived. | DEV-S provides statistical power, not claims; only J13 and T-NEW decide claims. G3.3 also requires a non-negative net on DEV-T. |
| Noise as large as the effect | LCL927-10 and RNG027-9 (5 of 8 against 8 of 8) flip with load and clock. Pods differ by about 2x. | delta from the A/A runs; same-pod pairing; majority of three; the instruction clock in development runs. |
| Checker limits differ by lane | Strict CheckProof returns Unknown on equals-encoding proofs. | Unknowns reported per arm and per lane; the inconclusive rule of section 1.3. |
| Twins cost CPU and slots | 16 slices on 8 physical cores; a shared core costs 1.31x. | Twins use only freed slots; the launch-time check in G3.3; the per-pick budget. |
| Pods differ from StarExec | Pod vCPUs are virtualized EPYC-Genoa, with unknown thread pairing. J13 ran on a Broadwell E5-2620 v4. Fixed CPU-clock budgets, such as the 2.5 s Horn release, do not scale with the wall. | W180 is the primary wall. WDEP is calibrated against the official result. lscpu is logged on every row. See D3. |
| Memory on pods | 64 GB pods hold 72 GB of caps; the matrix's 32 GB pods hold 36 GB. | OOM check at harvest. Use pods with at least 72 GB, or give every arm the same caps. |
| Engineering traps | Listed below. | Each one becomes a test in M2. |
| Using J13 too much | The matrix holds 400 x 46 J13 rows; there are three looks. | The seal; aggregate-only interim looks; no dissection. |
| Calendar and pods | The 16-pod cap is shared; heavy agents run one at a time. | Kill points in January and February keep spending small. |

Engineering traps that M2 must test:
- cp_uid_a/b exist only under orphan murder (src/atp/state.c), and the orphan test fires whenever they exist. Reusing them for features in a lane without orphan murder would silently switch on uid orphan deletion.
- Trace entries past the default 128K soft cap exist only with --proof; tools/atp_cli/main.c raises the cap to 16.7M for --proof runs. A feature that reads them would make the search depend on --proof, which is the divergence c98b59c4 closed.
- src/atp/otter.c has no history-export hooks.
- FindProof has no collector, so hooks must not decode IC-heap terms. The export's F record exhausted the heap on BOO078-1 at 56 s.
- The w2_modulo pick scans the whole heap, so coop picks need their own counter.
- A repo-relative model path breaks under StarExec's working directory, so the weights are compiled in.
- Const-object initializers broke clang 14 (c59b353b).
- In --lrs lanes, the layer must not change the key LRS ranks by.

### 6.2 What we will not do

- Claim parity or superiority against P0 alone. Every claim is against NL, which contains P1 and PD.
- Let J13 inform any decision before L2: no training, tuning, composition or go/no-go decision, no opening the sealed rows, and no dissecting a J13 problem for ENIGMA.
- Use competitor schedules, command lines or proofs as seeds or labels.
- Count Unknown as solved, ship anything with a negative net at W180, or ship on a WDEP result alone.
- Lower the schedule width below 16, or shorten slice walls, without the M1 evidence.
- Learn slice durations on the pool. Its solves finish by 49 s, so learned durations would cut off the J13 lanes that need 60-216 s.
- Strip --lrs or --awr from a twin's flags. tgt-sel needs --lrs to fit GRP664-10 under the cap.
- Build a push-time static single-queue scorer this year. Phase 0c's version of it tied the engine and was 2.2x worse on long runs. We revisit this only if a family reaches a long-run selections ratio of 0.67 or less.
- Build or ship the Tier-2 GCN. It needs a runtime compiler, and armed it solved 73-75 against 136 unarmed. We also will not wait on a tensor-to-C exporter.
- Add hand-built lanes for J13 problems, or start engine work on the existential, AC and Moufang residue within this plan. That work is the pivot if M5 kills E.
- Run anything on the Mac except edits, syntax checks and reading results.

### 6.3 Decisions for the user

- D1 (needed by 2026-10-26): may training include synthetic -10 encodings of non-UEQ TPTP problems?
  - They would come from our FOF-to-ifeq front end.
  - J13 names and stems would be excluded in every division, which also removes the -1, -2 and -3 versions of J13's -10 theorems.
  - This is the biggest data lever for the class that holds 26 of the 36-problem gap.
- D2 (after L2): may the CASC 2027 build be retrained on J13?
- D3 (optional): may one calibration run use StarExec Miami, the CASC hardware, which is publicly available? It is outside the pods-only rule. It would measure WDEP and the hyperthread contention factor directly.
