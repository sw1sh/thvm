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
- G2.1: Export v2 only observes. On 7 problems per family, with export on and off, the selection sequence, the proof and the stats are identical (instruction clock).
- G2.2: Zero correction is exact. On 8 problems per family, a twin with zero-correction weights is byte-identical to its base lane: same selections, proof, stats and LRS prune counts. This makes PD a pure duplicate and isolates the effect of learning.
- G2.3: Unarmed builds are byte-identical. test_atp passes in full (136835/136835 on b8443afb), the OA pin is 257971/735/2461555, and the DN pin is 2848/254/768876.
- G2.4: C and Python agree to 1e-9, with the same argmax on 8 problems per family.
- G2.5: DEV-S is valid if all three hold:
  - it has at least 60 band problems at WDEP and at least 30 at W180;
  - they come from at least 15 axiom systems;
  - P0's A/A disagreement on DEV-S is at most 5%.

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
| M2 Substrate, data  | 10-26 to 12-18     | G2.1 to G2.5                                 | family not guided; DEV-S fallback |
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
| WDEP and delta per wall      | at L0           | |
| protocol fingerprint (run_arms.py; amendment A3) | e820fb2367a1 | 2026-10-02 |
| calib_ref.tsv (pod-speed reference) and its pod's lscpu line | before any pod is judged against it | |
| L2 unseal token | after the freeze (stats.py --look L2 needs it) | |
| section 5.3 outputs          | ~/.cache/thvm/enigma/matrix/decisions_5_3.txt; summary below | 2026-10-02 |

Section 5.3 outputs (2026-10-02, from the sealed matrix; train and held-out only):
- Pool: TRAIN 664 problems, VBS at 60 s 550 (528 within 1 s), best single lane twee-raw 488, greedy cover reaches the union at k = 9. HELDOUT 171: VBS 143, best lane twee / twee-raw 134, union at k = 3. G0.1 passes (56,810 of 56,810 rows; 24 crashes, 0.062%).
- Hard tail: TRAIN 10 problems (fastest lane over 10 s), HELDOUT 1 (REL033-3). After the J13 stem exclusion, DEV-T = 5 and TRAIN-T = 4 problems. The pool has almost no band, so DEV-S (M2) and the 180 s supplement carry the gates.
- M4b pre-gate: FAIL. The oracle plan and simulated P1 both solve 505 on TRAIN + DEV-T at 60, 53 and 45 s; only 10 problems are solved by exactly one lane. No learned launch plan.
- M4c pre-gate: FAIL, final. 8 TRAIN Unsatisfiable problems rated below 1.0 are unsolved at 60 s (25 needed). No invention pilot.
- P1 retirement candidates by the letter of the rule: wm, horn-back, mnf-gj-amw, ifeq, tgt, tgt-fold, wm-gj, lazy-gj, bare-gj, plus 13 fof-* lanes the CNF pool cannot judge. wm is the fastest lane on 39 pool problems and owns REL033-3, so it stays for M1's paired test. All candidates wait for the 180 s supplement.
- Twin families: provisional, decided by ties because TRAIN-T has 4 problems: horn-cd, twee, encompass, flatten-bare-raw. Revisited after the supplement and DEV-S.
- Supplement: 142 problems, 2,882 jobs (SYN986-10.005/.006 segfault until the deep-term fix lands). Label harvest: 8,574 pairs on 500 TRAIN problems.
- T7: 40 front-end proofs come from equals, narrowq and unfold, which the static replica calls dead. These lanes run the Horn front end before their exit test (the M1 liveness item).

Decisions recorded (2026-10-01, by the user):
- D1: yes. Training may include synthetic -10 encodings of non-UEQ TPTP problems from our FOF front end; J13 names and stems are excluded in every division.
- D3: no. Every calibration stays on pods; WDEP is estimated from the submission build's official J13 result as in section 1.3.

Amendments (2026-10-02, before any protocol run; tools/atp_enigma/protocol implements each):
- A1 Memory. "At least 72 GB" reads as an effective memory limit, min(MemTotal, the pod's cgroup limit), of at least 16 x 4500 MiB, so 16 slices at the schedule's cap fit. The cap is fixed per comparison (--rss-cap default or 3500, the same on every pod and look of it); a pod that cannot honour it is refused, stats.py refuses rows of one wall with mixed caps, and every row records its OOM kills. WDEP is calibrated only under the default cap (J13's); rows at 3500 make G0.3 uncalibrated.
- A2 CPUs and binaries. 16 vCPUs means 16 effective CPUs: the affinity mask capped by the cgroup CPU quota. Every arm runs one binary (another needs --allow-binaries and is logged as a deviation). FindProof rejects an unknown --schedule mode (exit 2), and two arms with different flags whose binary lists the same lanes are refused unless declared (PD and E).
- A3 Protocol parameters, hashed into the protocol fingerprint on every row: CheckProof wall 60 s (THVM_PROOFCHECK_WALL=60 under timeout -k 5 75); seed 20261001 for the arm shuffle and the bootstrap, 10,000 resamples (another value at a look needs a named amendment); the clock-gated set is --lrs, --fol-lrs (the FOF front end's LRS) and --cp-set-ir-budget, read from the winning lane's flags in that arm's own binary lane table, and a winning lane missing from the table stops the runner; "the arms disagree" means their pass-1 Solution flags differ at that wall, and repeats run per wall.
- A4 Slice stderr. The runner adds THVM_ATP_SCHED_ERRDIR (slice stderr to files, for the end class of every lane). Before L0, P0 runs on DEV-H at WDEP with and without it, paired on one pod. If the paired |net| exceeds 2, or the median time ratio over the problems both solve is more than 2% off 1, every timed pass runs without it and end classes come from an untimed rerun of the misses.
- A5 Pod-speed calibration. The --schedule races 46 lanes and solves almost every TRAIN problem in under 3 s, so the calibration runs one deterministic configuration (the default preset, --szs --wall=120) on TRAIN problems of 2.6-30 s (protocol/calib_candidates.tsv); the reference keeps the 10 longest stable ones. Each pod runs 2 passes; it fails when a problem is unsolved, the geometric mean of the per-problem median ratios is more than 15% off, or one problem is more than 30% off; the reference's rss cap must match. Width contention is measured separately (M0 deliverable 5).
- A6 The seal. The WDEP runs are J13 runs of look L0: only the per-wall aggregates and the G0.3 decision are read, and the per-problem agreement table is sealed. Sealed runners log a heartbeat (base-job progress rounded down to 10%) instead of per-job lines and pack every per-problem output into sealed.tgz; run.stop names no problem, and opening the archive for a VerifiedBad is the section 1.3 exception. A look reads all 400 J13 problems together, and --look L2 needs the unseal token above.
- A7 Aggregates at L0 and L1. Domains with fewer than 10 compared problems are pooled into OTHER (while OTHER holds 1-9, the smallest named domain joins it); DROP-BEST picks among the pooled buckets; per-domain gains and losses are printed, and M5's "no TPTP domain loses more than 3 problems" reads a pooled bucket's loss count. Unknowns are printed per arm; the per-lane counts are sealed.
- A8 Completeness. At L0-L2 a problem without complete cells in every compared arm makes that wall INVALID, and the runner resumes until it is complete; dev analyses compare such a problem on pass 1 of every arm and count it. Every comparison at a wall uses one problem set, and NL is chosen once per wall on it, a tie broken against the first comparison that names NL. G0.2 and delta need both A/A passes on all 400 problems. A resume never splits a problem across pods: a problem incomplete on a dead pod reruns whole on its replacement, and a resume under another configuration is refused.
- A9 Readings. G3.3's "McNemar p < 0.05" is the one-sided exact sign test, as in section 1.5. An unsolved cell whose passes tie between Unknown and another non-Solution outcome counts as Unknown (conservative toward INCONCLUSIVE); the number decided by that tie is printed per arm.

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