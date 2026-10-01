// ===================================================================
// main
// ===================================================================

// --schedule: run a fixed sequence of configurations, each in a child
// process (fork/exec of argv[0] with the config's flags and a slice of
// the wall budget), stopping at the first proof -- the standard CASC
// portfolio-wrapper shape, with process isolation guaranteeing each
// slice a fresh engine.  Slice fractions are provisional until the
// CASC-30 coverage matrix tunes them.
typedef struct { const char *name; const char *flags; double frac; } SchedSlice;
// Ordered by value: run_schedule runs slices CONCURRENTLY in a pool of
// SCHED_MAX_PAR, EACH with the FULL wall, so the first SCHED_MAX_PAR
// entries are the ones that run to completion -- strongest + most
// complementary first.  frac is now vestigial (each slice gets the full
// wall, not frac*wall); kept so the struct/consumers are unchanged.
static const SchedSlice SCHEDULE[] = {
  // (1) The WM preset -- the strongest general UEQ solver.  Needs the FULL
  // wall for its slower proofs (LCL113-10 23s, REL028-2 15s), which the old
  // sequential 0.22 slice (6.6s at a 30s wall) starved.
  {"wm",      "",                                          0.34},
  // (1b) EXISTENTIAL-GOAL EQUALS ENCODING (Twee addNarrowing port):
  // the WM preset over the eq/true/false-encoded problem.
  // Complementary to the narrowing lane every other slice uses on a
  // non-ground goal: COL009-1 proves in 0.1s encoded (schedule-wide
  // Timeout narrowing); COL004-1 is the exact reverse.  =2 makes the
  // slice exit immediately on ground-goal problems, so it only ever
  // spends a pool slot on the existential family.
  {"equals",  "--equals-goal=2 --fof-sat=0",               0.0},
  // (1b') CRITICAL-GOAL QUEUE narrowing: best-first goal-descendant
  // search (WM PM_Existenzziele / GKC query_preference=1 -- GKC's
  // winning COL011-1 strategy, 0.54s where every completion-side
  // treatment is structurally blind: a goal variable closes only by
  // INSTANTIATION, and the oracle derivations are 5 narrow steps with
  // the o-definition's unorientable RHS face opening the chain).
  // Complementary to the equals encoding on the same existential
  // family (encoding drowns the eq-chain in the CP flood; the queue
  // keeps goal descendants in their own selection lane).  =2 exits
  // immediately on ground-goal problems, exactly like the equals
  // slice.
  {"narrowq", "--narrow-queue=2 --fof-sat=0",              0.0},
  // (1b'') EQUALS x TWEE-CORE: the equals encoding under the twee
  // slice's lean completion core with the STRUCTURAL shared-subterm
  // discount (--twee-struct-dedup) -- the Twee-default search shape
  // for the existential-goal ifeq/CD family.  The LCL927-10
  // dissection (CASC-J13's only Twee-solo UEQ solve: Twee 55.85s,
  // everyone else TMO) pinned the portfolio's structural gap: Twee
  // always equals-encodes existential goals and its winning variant's
  // ONLY load-bearing flag is --no-flatten-goal, but our equals slice
  // runs the WM preset whose Horn release + MaxWeight give up in ~1s,
  // and no lane paired the encoding with the twee core.  With the
  // pointer-identity dup discount the proof cascade's B-permutation
  // CPs (repeated instantiated conditions) sat 220 pri above the
  // selection band forever; the structural discount puts them inside
  // it and the single proves in ~115s / <2.5GB (twee-core-without-SD
  // ResourceOuts at every RSS bound tried up to 6GB).  =2 exits
  // immediately on ground-goal problems, exactly like equals/narrowq.
  {"equals-twee", "--equals-goal=2 --twee-struct-dedup --preset=none --horn=0 --simpler-join --lazy-ground-join --backward-subsume --backward-demod --cp-weight=8 --w2-modulo=0 --orphan-murder=0 --cp-set-interreduce --cp-set-ir-budget=5 --max-steps=10000000 --fof-sat=0", 0.0},
  // (1c) CONDENSED-DETACHMENT lane: the encoded-Horn chaining under
  // the classic CD prover discipline (Otter's max_weight + subsumption
  // on the LCL corpus) -- a formula-size cap with iterative deepening
  // kills the promiscuous major-premise towers, generalization
  // subsumption collapses the instance flood, pure smallest-first
  // given selection, and ~80% of the wall on the chaining itself.
  // The blind v1 agenda saturates its 400k-release budget on these
  // without ever nearing the goal (and Vampire's whole casc schedule
  // times out on LCL195-10 too).  =2: exits immediately unless a
  // dispatcher + ground goal is detected, freeing the pool slot on
  // every non-CD problem exactly like the equals slice.
  {"horn-cd", "--horn-cd=2 --fof-sat=0",                   0.0},
  // (1d) BACKWARD goal-directed condensed detachment: regress the ground
  // goal to axiom instances by iterative-deepening DFS over the decoded
  // modus-ponens rule, so the ground goal anchors every subgoal instead
  // of flooding forward with ever-more-general theorem schemas (the
  // forward CD flaw that times out thvm, Twee's 20k-rule cap AND Vampire
  // on the deep LCL family).  =2: exits immediately unless a dispatcher +
  // ground goal is detected, freeing the pool slot on every non-CD
  // problem exactly like the horn-cd slice.
  {"horn-back", "--horn-back=2 --fof-sat=0",                0.0},
  // --- FOF PORTFOLIO (--fof-slice=2 exit-fast pack) --------------------
  // On a fof( input every UEQ-tuned slice runs the IDENTICAL clausal
  // front-end (clausify + Horn release + or-lane + the 60%-remaining-
  // wall clausal pre-pass) and differs only in engine knobs the
  // pre-pass never reads -- the measured FOF timeout class (26/56 of a
  // stratified FEQ+FNE sample at 60s) was ONE strategy racing itself.
  // These slices vary the levers that DO matter for the front-end:
  //   fof-fifo / fof-best  pre-pass given-clause selection (age ratio
  //                        1 = pure FIFO fairness / 0 = pure best-first
  //                        vs the default 5-to-1 interleave);
  //   fof-flat             pre-pass WITHOUT the conjecture-ancestry
  //                        tier (flat weight order -- the goal cone no
  //                        longer crowds out axiom-side saturation);
  //   fof-nopre            no pre-pass at all: the Horn/or equational
  //                        lanes get the whole wall instead of the 60%
  //                        pre-pass gamble;
  //   fof-or2              eq-reified or-lane encoding (predicate
  //                        atoms wrapped as eq(p,true) too);
  //   fof-full             pre-pass on the FULL clausified set where
  //                        the default would SInE-cone it (>512
  //                        clauses; below that it mirrors wm).
  // Each exits immediately on non-fof input (raw-text probe
  // tp_input_is_fof, the equals/horn-cd =2 discipline), so on the UEQ
  // corpus the pack costs one fork + read per slice.  The UEQ-tuned
  // slices below in turn carry --fof-sat=0: their pre-pass duplicated
  // the wm slice's byte-for-byte (same clausified input, same
  // selection) and burned up to 60% of the slice wall before the
  // engine ever differed -- on FOF input the pre-pass now runs only in
  // the lanes that vary it (wm keeps the default discipline).
  //   fof-deep             default selection but a step budget the 60%
  //                        wall guard, not the 20000-step cap, bounds
  //                        (the SInE'd cone runs 20k steps in a third
  //                        of the guard on CSR-class KBs -- the cap,
  //                        not the wall, starves the saturation).
  {"fof-fifo",  "--fof-slice=2 --fol-age-ratio=1",  0.0},
  {"fof-best",  "--fof-slice=2 --fol-age-ratio=0",  0.0},
  {"fof-flat",  "--fof-slice=2 --fol-goal-tier=0",  0.0},
  {"fof-deep",  "--fof-slice=2 --fof-sat=1000000",  0.0},
  {"fof-nopre", "--fof-slice=2 --fof-sat=0",        0.0},
  {"fof-or2",   "--fof-slice=2 --fof-or=2 --fof-sat=0", 0.0},
  {"fof-full",  "--fof-slice=2 --fof-sine=0",       0.0},
  //   fof-split            k-bounded ground case splitting: near-Horn
  //                        sets whose or-clauses carry only GROUND
  //                        positives run one Horn branch child per
  //                        choice (AVATAR-lite; every branch must
  //                        refute).  =2 exits immediately on any
  //                        other shape -- non-ground or-positives
  //                        (the AGT trichotomy class), pure Horn, or
  //                        too many/too-wide or-clauses.
  {"fof-split", "--fof-slice=2 --fof-split=2 --fof-sat=0", 0.0},
  // AVATAR-lite lanes: component splitting reshapes the prepass search --
  // it wins problems the plain prepass loses (COM149+1, MGT061+1,
  // SWV055+1 at 240s) and loses ones it wins, so it races as its own
  // slices instead of moving the default (the -8/+3 pod A/B).
  {"fof-avatar",  "--fof-slice=2 --fol-avatar=1", 0.0},
  {"fof-avatar2", "--fof-slice=2 --fol-avatar=2", 0.0},
  // Vampire-strategy-dimension lanes (frequency-ranked from the CASC
  // schedule source: sp 2396 / nwc 2356 / sos 1722 / lrs 2664 of 5307
  // strategies).  Each varies pre-pass dimensions the pack above never
  // moves: symbol precedence, multiplicative goal weighting, set of
  // support, literal selection, and the saturation wall share.
  {"fof-freq",  "--fof-slice=2 --fol-sp=1 --fol-nwc=2.5", 0.0},
  {"fof-sos",   "--fof-slice=2 --fol-sos=1 --fol-sp=1", 0.0},
  {"fof-wall",  "--fof-slice=2 --fof-sat-wall=95 --fol-lrs=1", 0.0},
  {"fof-selL",  "--fof-slice=2 --fol-select=3 --fol-sp=1 --fof-sat-wall=95", 0.0},
  {"fof-spb",   "--fof-slice=2 --fol-spb=1 --fol-nwc=5", 0.0},
  // FULL AC-completion (AC operators detected from their commutativity
  // and associativity axioms, AC-matching and Stickel AC-unification,
  // src/atp/ac.c) stays OUT of the competition schedule -- for
  // VALUE now, not soundness.  Both AC-match soundness bugs are fixed
  // (bare-variable multiplicity drop, then the deferred-count re-scan
  // double-charge; the CASC-J13-reported false refutations LAT016-1 /
  // RNG030-6/031-6/032-6 all stay non-refuting under forced
  // --full-ac=2, held to a 120s wall; tests/soundness covers them).
  // But the SOUND matcher no longer over-matches, so it also no longer
  // finds the short RNG-family proofs the unsound one did (RNG021-6:
  // 3s unsound -> Timeout sound) -- the slice's former ~8-problem gain
  // was an artifact of the unsoundness plus schedule variance, and a
  // sound full-ac lane measured ZERO recoveries on that set.  Re-add
  // only if AC-completion ever earns real coverage.
  // {"full-ac", "--full-ac=2 --fof-sat=0", 0.0},   // sound but earns nothing
  // DEFINITION UNFOLDING (Vampire definition_unfolding port), exit-fast:
  // =2 gives up immediately when the problem has no pure function
  // definition f(X..)=t, so on most of the corpus the slot frees
  // instantly.  Where one exists the folded symbol is KBO-irreducible
  // (big-term -> f(X..)) and the flatten/wm lanes' reachability of the
  // goal depends on presentation chaos: COL002-5 proves in 1.5s or
  // Times Out at 30s purely on input clause order/orientation, and the
  // canonical presentation is a LOSING one in all three corpus
  // releases (casc29/casc30/J12).  Unfolded + flatten-goal the goal's
  // ground-subterm cone folds onto minimal constants (= the exact
  // setup of Vampire's 6-superposition oracle proof: its winning slice
  // is dis+10_10_tgt=ground) and the derivation surfaces in the first
  // few steps: 5 steps, 0.0s, under ANY presentation.
  {"unfold",  "--def-unfold=2 --flatten-goal --lazy-ground-join --backward-subsume --backward-demod --auto-max-weight=6 --max-weight-slope=1 --fof-sat=0", 0.0},
  // DEF-UNFOLD x TGT composition (Vampire's KLE122-10 winner,
  // lrs+10_1:32 tgt=full sp=frequency under the default fde=all):
  // unfolding eliminates the defined symbols (KLE122-10: 9 defs --
  // diamonds, if_then_else, complement...) so the conjecture speaks
  // pure domain-algebra, then tgt folds its compound subterms onto
  // minimal constants and goal-directed CPs dominate selection.
  // Neither ingredient alone gets there (unfold-dressed and tgt both
  // Time Out at 60s; composed it proves in 6.4s, 7x under Vampire's
  // 44.4s -- sym-prec=frequency measured NOT load-bearing).  =2 exits
  // immediately when no definition unfolds, like the unfold slice.
  {"unfold-tgt", "--def-unfold=2 --tgt=1 --preset=none --horn=0 --fof-sat=0", 0.0},
  // DEF-UNFOLD x TGT x AGE-WEIGHT (the LCL306-10 dissection, Vampire's
  // winner lrs+10_28_tgt=full:drc=ordering, 0.165s / 1120 activations).
  // With the 13 goal-subterm definitions in place our unfold-tgt
  // trajectory holds EVERY vampire proof milestone by pick ~2900 --
  // except the endgame modus shells ifeq(theorem(or(not(or(sFa,sFb)),
  // X)),true,theorem(X),true), formed and queued at weight ~12 but
  // buried forever behind a self-feeding weight-6..11 flood of
  // derived junk factories (double-negation towers = 31% of all picks,
  // or-padding generalizations) that Vampire's stream never activates.
  // Vampire's carrier for the heavy general schemas is the AGE queue:
  // in its activation trace every heavy proof clause (f20 w16, the
  // B/C-theorem shells w16-18) lands at activation index == 1 mod 29 --
  // the 1-in-29 oldest-first pick of awr 1:28.  Composing our existing
  // --awr FIFO dimension with the tgt lane converts LCL306-10 from
  // unreachable (TMO at every round-2 config) to PROVED 46-49s at RSS
  // 2500; ablation: def-unfold + tgt + awr load-bearing, tgt-fold /
  // cp-weight=0 (Vampire whole-clause ADD weight) / --lrs each
  // neutral-to-helpful on the specimen.  awr=4 beats Vampire's 28 here
  // because our flood is already active by the time fairness matters --
  // the age pointer must reach the seq-1900 shells within the wall.
  // LRS rides along for queue bounding (live cps 11k vs 175k without;
  // the schedule shares RSS across par slots).  =2 exit-fast like the
  // unfold/unfold-tgt lanes: frees the slot when no definition unfolds.
  {"unfold-tgt-awr", "--def-unfold=2 --tgt=1 --tgt-fold=1 --cp-weight=0 --awr=4 --lrs --preset=none --horn=0 --fof-sat=0", 0.0},
  // (2) Goal FLATTENING (Twee/Vampire naming port) + LAZY ground-join
  // (Twee selection-time redundancy) + backward-simplify + auto-max-weight.
  // Cracks deep-goal UEQ that completion stalls on (COL060-1, GRP167-4);
  // connectedness dropped (net cost under lazy-GJ; GRP167-4 26s->1.8s).
  {"flatten",  "--flatten-goal --lazy-ground-join --backward-subsume --backward-demod --auto-max-weight=6 --max-weight-slope=1 --fof-sat=0", 0.33},
  // (2b) BARE flattening: the dressed flatten slice's extras (lazy-GJ,
  // backward-simplify, auto-max-weight) each independently push the
  // wall-edge flatten proofs past the limit (ALG212-10 proves in 27.5s
  // bare, TIMES OUT with ANY of them added) -- the unadorned transform
  // needs its own lane.  Horn-free: with (6) it keeps a released-unit-
  // free lane for problems whose baseline proof the Horn releases
  // perturb (BOO017-10).
  {"flatten-bare", "--flatten-goal --horn=0 --fof-sat=0", 0.0},
  // (3) Goal-directed MNF front + ground-join + a growing MaxWeight bound
  // -- complementary to both (goal-direction the WM preset lacks, pruning
  // the bare MNF front lacks).
  // (5) TWEE-SCORED completion: flatten + Twee's CP score (mode 8, now with
  // the faithful formation-depth term) + pure best-first (w2-modulo=0, no
  // WM age interleave) + dynamic passive re-scoring (cp-set-interreduce =
  // Twee simplifyPassive) + the simplerThan-gated join discipline
  // (--simpler-join, Twee Join.hs allSteps): the join pass culls CPs the
  // plain doR/doE joinability keeps, which on the lattice family turns a
  // wall-starved backup lane into a sprint -- LAT159-1 proves in 1.8s
  // with the gate vs 20.3s without (the 20.3s run never fit the backup
  // slot's leftover wall, so the problem TIMED OUT schedule-wide).  A
  // genuinely different selection regime from the WM/MNF slices above --
  // the portfolio's Twee-native angle.  Displaced from the top-5
  // concurrent pool by the encompass slice (its flatten + lazy-GJ core
  // overlaps slice 2); launches as a pool slot frees.
  // --cp-set-ir-budget=5 puts the passive re-scoring on Twee's OWN task
  // discipline (freq 10s + 5% budget + 20%-dead sample trigger) instead
  // of the every-16-rules full sweep, which ate 82% of the wall on the
  // lattice/group residuals (LAT138-1 @30s: 24.5s in the sweep) -- 3x
  // the completion throughput at the same score function.  Flips
  // LAT149-1 (17.7s), GRP177-1 (14.9s), GRP753-1 (2.7s), GRP766-1
  // (11.9s) standalone at 30s, each a prior schedule-wide Timeout.
  // --orphan-murder=0: Twee has NO Waisenmord -- passive CPs survive
  // parent-rule collapse (Twee re-scores them; WM deletes them).  The
  // GRP706-1 dissection showed the lane's missing content is exactly a
  // buried CP class whose parents collapse before selection: the
  // ld(X,X)-chain CP (twee lemma 100 generalization) forms at pick ~750
  // (pri 839 vs a MixWeight frontier that never passes 265), then its
  // mother (twee lemma 98's rule) is collapsed by the late lemma-96 rule
  // -- under Waisenmord the class must re-derive from scratch each time.
  // With the flag the slice matches Twee's passive-set semantics and
  // GRP706-1 proves (102s dressed; 12.8s with the lean core
  // --cp-weight=8 --w2-modulo=0 --orphan-murder=0 --simpler-join),
  // a prior Timeout at every wall tried.
  {"twee",   "--simpler-join --flatten-goal --lazy-ground-join --backward-subsume --backward-demod --cp-weight=8 --w2-modulo=0 --orphan-murder=0 --cp-set-interreduce --cp-set-ir-budget=5 --fof-sat=0", 0.0},
  // (5'') TWEE-SCORED completion with the STEEP formation-depth penalty
  // (Twee portfolio's --depth-weight lever) on the plain WM-lazy base --
  // no simpler-join / connectedness (both thvm approximations FALSE-DROP
  // the cascade-opening CP on the encoded lattice family: LAT224-10's
  // generalized-absorption lemma join(meet(X,Y), meet(X, join(Y,Z))) ->
  // meet(X, join(Y,Z)), which Twee keeps under its below-peak joining).
  // Depth 3x the default (14 -> 42 per unit, *7-scaled) sinks the
  // deep-derivation permutation/guard-tower flood that shares the
  // 330-400 score band with the depth-1..3 lemma cascade.  The lever is
  // MEANINGFUL only since the r_depth compaction-shift + requeue-depth
  // fixes (an interreduce-simplified rule re-enters at its victim's
  // depth, Twee semantics; before, depths were garbage after any rule
  // collapse and the depth term was noise).  Flips LAT224-10 (4.8s,
  // schedule-wide GaveUp before; Vampire 2.1s / Twee 10.06s at
  // CASC-J13) and LAT229-10 (48.2s) standalone at 240s; LAT186-10 /
  // LAT065-10 still flood (queue-cap variants measured WORSE on
  // LAT229 -- trajectory perturbation -- so the lane runs uncapped
  // under the slice RSS guard).  --max-steps raised: the win crosses
  // the 500k default (LAT229 1.75M steps).
  {"twee-deep", "--cp-weight=8 --w2-modulo=0 --cp-depth-weight=6 --max-steps=10000000 --fof-sat=0", 0.0},
  // (5''') TWEE-FREQ: the twee slice under Twee's CASC variant-1
  // ordering pair -- lhs-weight 1 (CP score counts the larger side at
  // coefficient 1, so small-RHS lemmas stop being over-taxed) +
  // frequency precedence (most common symbol LOWEST, Twee
  // --flip-ordering).  The LAT138-1 dissection pinned the residual
  // divergence AFTER the faithful simpler-join port to SELECTION, not
  // joining: default-ordering Twee itself misses LAT138-1 at 300s,
  // while `twee --lhs-weight 1 --flip-ordering` proves it in 13s and
  // NEITHER flag alone does at 90s.  Ported pair on the twee slice:
  // LAT138-1 proves in 0.5s (prior schedule-wide GaveUp at 240s).
  // Same lean core, so the lane inherits the twee slice's passive-set
  // + re-scoring discipline.
  {"twee-freq", "--simpler-join --flatten-goal --lazy-ground-join --backward-subsume --backward-demod --cp-weight=8 --cp-lhs-weight=1 --sym-prec=frequency --w2-modulo=0 --orphan-murder=0 --cp-set-interreduce --cp-set-ir-budget=5 --fof-sat=0", 0.0},
  // (5''') AXIOM-SIDE NAMING (Twee --flatten / tgt=4): fresh minimal
  // constants for every ground compound axiom subterm, definitions
  // born inter-reduced (--tgt-fold).  The loop-theory order
  // conditions (GRP689-1: op_d^9 = unit, op_c^4 = unit) are ground
  // towers completion cannot shatter -- named, the powers become
  // shallow definitional layers and the c*d = d*c commutation proves
  // in 1s (bare single; Timeout at every bound without, and twee's
  // OWN default config misses it at 300s while its portfolio's
  // --flatten variant proves in 1s -- the transform, not the score,
  // is the content).  Exit-fast (--tgt-exit) on problems with no
  // axiom-side ground compound subterm: there the transform adds
  // nothing over the flatten lanes' goal-side naming.
  {"tgt-ax", "--tgt=4 --tgt-fold=1 --tgt-exit --horn=0 --fof-sat=0", 0.0},
  // CONJECTURE-SYMBOL-WEIGHTED bare completion (the GRP66x Moufang
  // dissection, E's auto winner G-E--_107_C37_F1_PI_AE_Q4_CS_SP_PS_S0Y
  // on the whole 661-1..664-12 subfamily, 25-45s official, where every
  // FindProof slice TMO'd at CASC-J13).  E's queue mix is dominated by
  // two conjecture-symbol-relative evals (ConjectureGeneralSymbolWeight
  // PreferNonGoals 200,100,200,50,50,1,100: goal-cone functions 50 vs
  // 200; RelevanceLevelWeight2 SimulateSOS: rel-1 symbols 300 vs rel-2
  // 800, vars 400) -- on the Moufang family the goal cone is pure
  // mult/skolem while the axioms carry ld/rd/unit, so clauses dragging
  // division-operator debris are taxed ~3-4x and the DEEP pure-mult
  // superposition chain (E proof DAGs: depth 47-57, 285-410 clauses)
  // stays inside the selection band.  Our port of exactly this eval
  // already existed (--tgt-sel, the ifeq-guard-style CP-priority scale
  // over the conjecture-symbol mask, LCL-round documented); the missing
  // piece was a LANE arming it: bare + tgt-sel=1 + tgt-nongoal=4 (E's
  // 200/50 function ratio) proves GRP661-1 89s / GRP662-1 110s /
  // GRP663-1 92s as 240s singles where bare alone and bare+awr=8 both
  // ResourceOut.  --awr measured slightly WORSE composed (102s vs 89s
  // on 661-1): E's own FIFO component is only 1/9 of its picks.
  // --lrs rides along AND is load-bearing twice over: per-pick cost
  // scales with the live queue, so pruning beyond the reach horizon
  // triples the pick rate (661-1: 29.8s / 94 picks/s vs 89s / 43),
  // and it is what fits the 664-1x subfamily inside the slice RSS cap
  // (664-10: PROVED 71s at 4500MB with lrs; eager needs ~6GB/290s and
  // ResourceOuts at 2500 -- the flood is 1.9M SMALL cps, mean 35
  // symbols, below the raw>=50 descriptor lane's class).  Placed in
  // the leftover pool (not wave 1): on its family the early exit-fast
  // lanes free slots within seconds, and the proven wave-1/2 indices
  // stay unshifted (the LAT138 placement lesson).  Exit-fast when the
  // conjecture cone covers the signature (uniform scale = duplicate
  // of bare).
  {"tgt-sel", "--tgt-sel=1 --tgt-nongoal=4 --lrs --tgt-sel-exit --preset=none --horn=0 --fof-sat=0", 0.0},
  // VAMPIRE OTTER-LOOP lane (the RNG Moufang dissection, CASC-J13 UEQ
  // cluster RNG027-5/7/8/9/10, 028-5/7/8, 029-5/6/7: Vampire + Prover9
  // only; E and Twee, both DISCOUNT-loop provers, time out).  Vampire's
  // slot lrs+10_2_to=lpo:sp=const_frequency:spb=goal:fd=preordered:
  // bd=preordered:fde=none:nwc=1 solves RNG027-5 in 91 activations as
  // lrs or ott and FAILS as dis: 19 of its proof's 84 demodulations use
  // never-selected clauses (the reassociation law 11 times).  Composed:
  // LPO under Vampire's const_frequency + spb=goal precedence (the goal
  // operator above the additive theory -- distributivity expands, the
  // associator/commutator definitions fold), the Otter simplification
  // set (--otter: orientable queued CPs demodulate as passive rules,
  // replayed newest-first per batch), oriented-only selection normalize
  // (--pop-rules-only, the fd=preordered analog), NO WM orphan murder
  // (Vampire keeps a simplified clause's descendants; with it the lane
  // ResourceOuts), age:weight 1:2, LRS, and --mixmost-nf so proof
  // extraction replays the goal check's own normal forms.  Ablated on
  // RNG027-5: --otter, LPO, spb=goal and orphan-murder-off are each
  // load-bearing.  --otter-raw demodulates each CP from its raw overlap,
  // once, as Vampire's unprocessed loop does, and --ir-run-order
  // interreduces under the lane's LPO (the FT walk gates unorientable
  // instances by KBO): single-lane runs (instruction clock, 177s) then
  // prove RNG027-5/7/10, 028-5/7 and 029-5/6/7 of the J13 ring set (029-5
  // and 029-6 stay open without them) and the same 8 of the 31 held-out
  // RNG UEQ problems.  Prover9's clause penalties (--nest-penalty/
  // --depth-penalty) cost this lane six of the eight.  Exit-fast
  // (--otter-exit) unless a distributivity law orients as expansion under
  // the lane's precedence.  Leftover pool after tgt-sel: the wave-1/2
  // indices stay unshifted.
  {"otter-lpo", "--ord=lpo --sym-prec=const-frequency --spb-prec --otter --otter-raw --ir-run-order --otter-exit --pop-rules-only --forward-subsume --lrs --awr=2 --cp-weight=0 --mixmost-nf --uid-orphan=0 --eager-sweep=0 --orphan-murder=0 --preset=none --horn=0 --fof-sat=0", 0.0},
  // The same lane under Vampire's sp=const_min (Vampire's own casc slot
  // for RNG027-8) with Prover9's clause-weight penalties (nest 2, depth
  // 2).  With the goal stated through the defined operator
  // (associator(x, x*y, z) = associator(x,y,z)*x), spb=goal lifts
  // associator too and const_frequency ranks it above multiply -- the
  // definition unfolds; const_min's reverse arity keeps multiply above
  // it.  Of the otter lanes only this one proves RNG027-8 (62s single-
  // lane); without the penalties it slows to 145s.  The multiply-form
  // members stay with otter-lpo.  --otter-raw and --ir-run-order are not
  // armed: with --ir-run-order the rule set explodes on RNG027-5/028-5
  // (1.2K to 6.5K rules between steps 500 and 640, every adoption
  // interreducing every older rule) and the lane keeps 3 of its 8 J13
  // solves; --otter-raw alone trades RNG027-8 for 028-5/029-5/029-6,
  // which otter-lpo proves.
  {"otter-lpo-cmin", "--ord=lpo --sym-prec=const-min --spb-prec --otter --otter-exit --pop-rules-only --forward-subsume --lrs --awr=2 --cp-weight=0 --mixmost-nf --uid-orphan=0 --eager-sweep=0 --orphan-murder=0 --nest-penalty=2 --depth-penalty=2 --preset=none --horn=0 --fof-sat=0", 0.0},
  // Prover9's LADR symbol order.  --otter-raw / --ir-run-order leave its
  // single-lane solves unchanged (10 of the J13 ring set, 21 of the 31
  // held-out), so it runs without them.
  {"otter-ladr", "--ord=lpo --sym-prec=ladr --otter --otter-exit --pop-rules-only --forward-subsume --lrs --awr=2 --cp-weight=0 --mixmost-nf --uid-orphan=0 --eager-sweep=0 --orphan-murder=0 --preset=none --horn=0 --fof-sat=0", 0.0},
  {"mnf-gj-amw", "--mnf --ground-join --connectedness --auto-max-weight=6 --max-weight-slope=1 --fof-sat=0", 0.33},
  // (4) ENCOMPASSMENT DEMODULATION (Twee reducesSkolem): the WM preset
  // with the skolem-ground unorient gate -- variable-permuted variants
  // canonicalize, keeping R lean on the AC/comm-heavy families where
  // completion otherwise drowns in variants.  Cracks 6 of the 23
  // twee-residual CASC-30 UEQ targets standalone in <=30s (MVA003-1
  // 7.2s, MVA009-1 7.1s, NUN134-1 0.3s, LAT163-1 5.6s, LAT174-1 21.3s,
  // GRP721-1 1.0s -- all Twee-oracle-confirmed), each unreachable by
  // every prior slice.  --max-steps raised: the canonical search runs
  // 30k+ steps/s and hits the 500k default mid-wall.
  {"encompass", "--encompass-demod --max-steps=5000000 --fof-sat=0", 0.0},

  // (5) PURE bare completion (bare engine defaults, GT weight, NO WM preset
  // heuristics, NO ground-join/connectedness).  The WM preset's selection +
  // MaxWeight actively DIVERGES on some CASC UEQ problems -- notably the
  // ifeq-encoded Boolean/relation families (BOO014-10: WM preset + WM-emu
  // fail even @150s, but pure bare PROVES it in 6.0s; ground-join or
  // connectedness on top of bare break it again).  This slice is the
  // portfolio's bare-completion angle for the WM-preset-divergent set.
  {"bare",   "--preset=none --horn=0 --fof-sat=0", 0.0},   // horn-free (see flatten-bare)
  // (5b) IFEQ GUARD DISCIPLINE: bare completion + goal flattening with
  // the CP selection weight scaled by THVM_ATP_IFEQ_PENALTY per
  // undecided ifeq guard a CP's conclusion retains beyond the free
  // budget, plus a step for two-compound-sides CPs (theory-algebra
  // churn) and per distinct variable over THVM_ATP_IFEQ_VARCAP (the
  // ever-more-general theorem-schema towers) -- the equational port of
  // Vampire's guard-resolving selection on the machine-equationalized
  // '-10' family.  On LCL306-10 it reshapes the search visibly (the
  // oracle's ground guard-chain lemmas surface; or/not algebra and the
  // 4-5-var towers sink) but does not yet prove any of the 7 gap
  // targets standalone at 30s -- kept BELOW the proven slices so it
  // only consumes leftover pool slots on ifeq problems.  =2: exits
  // immediately when no dispatcher axiom ifeqN(X,X,Y[,Z]) = Y is
  // present, freeing the pool slot on every non-ifeq problem exactly
  // like the equals/horn-cd slices.  Horn-free: keeps the full wall
  // for the engine.
  {"ifeq",   "--ifeq-guard=2 --flatten-goal --goal-subterm --preset=none --horn=0 --fof-sat=0", 0.0},
  // (5c) VAMPIRE tgt (twee_goal_transformation, --tgt=1 'full'):
  // pr_tgt_transform names every compound subterm of the negated
  // conjecture bottom-up with a fresh minimal symbol (full mode also
  // names non-ground subterms behind the linear-term guard), so the
  // conjecture cone folds onto small definitions and goal-directed CPs
  // dominate selection -- the option Vampire's UEQ schedule leans on
  // hardest (tgt in 191 of ~400 strategies).  Flips SYN615-10
  // standalone at 30s (11.6s bare; every prior slice, ifeq-guard
  // included, Timeout; Vampire 1.7s).  Measured WORSE composed with
  // --ifeq-guard on SYN615-10 (the guard penalty buries the folded
  // fact tier: Timeout at 30s), so the lane runs bare.  On a problem
  // whose conjecture has no compound subterms the transformation is
  // empty and the lane duplicates 'bare' -- kept below the proven
  // slices with the other leftover-pool lanes.
  {"tgt",    "--tgt=1 --preset=none --horn=0 --fof-sat=0", 0.0},
  // (5c') tgt with Vampire-faithful BORN-INTER-REDUCED definitions
  // (--tgt-fold: each definition body folds its children onto their
  // fresh symbols and the goal collapses in place, the semantics of
  // TweeGoalTransformation.cpp "born already inter-reduced").  Where
  // the goal cone genuinely folds this stops the definition churn
  // outright: SYN615-10 proves in 0.1s / 1266 CPs vs 9.9s / 64445
  // CPs on the plain tgt lane (99x).  Races ALONGSIDE the plain lane
  // (not replacing it): on problems where the deep definitions win by
  // chance the old lane still carries them.
  {"tgt-fold", "--tgt=1 --tgt-fold=1 --preset=none --horn=0 --fof-sat=0", 0.0},
  // RAW-PRESENTATION recovery lanes: canonical input (default ON) is
  // score-neutral in aggregate but systematically flips individual
  // problems BOTH ways (pod A/B, identical hardware: ON-only gains
  // KLE110-10/MVA003-1/GRP706-1/LAT139-1; OFF-only keeps LAT149-1 +
  // LAT159-1 [twee lane, 2 corpora] and BOO015-10/ALG212-10 [bare /
  // flatten-bare lane]).  The union beats either choice: these variants
  // re-run the exact slices that carried the OFF-only solves with the
  // original presentation.
  {"twee-raw", "--canon-input=0 --simpler-join --flatten-goal --lazy-ground-join --backward-subsume --backward-demod --cp-weight=8 --w2-modulo=0 --orphan-murder=0 --cp-set-interreduce --cp-set-ir-budget=5 --fof-sat=0", 0.0},
  {"bare-raw", "--canon-input=0 --preset=none --horn=0 --fof-sat=0", 0.0},
  {"flatten-bare-raw", "--canon-input=0 --flatten-goal --horn=0 --fof-sat=0", 0.0},
  // Backups -- only launch if one of the first five exits early.
  {"wm-gj",   "--ground-join --connectedness --fof-sat=0", 0.0},
  {"lazy-gj", "--preset=waldmeister-lazy --ground-join --connectedness --fof-sat=0", 0.0},
  {"bare-gj", "--preset=none --ground-join --connectedness --fof-sat=0", 0.0},
};
enum { N_SCHED = (int)(sizeof(SCHEDULE) / sizeof(SCHEDULE[0])) };

// Per-slice RSS cap (MB) when running slices concurrently, so K parallel
// engines stay well under the machine ceiling.  Each child inherits it as
// THVM_ATP_RSS_ABORT_MB unless the caller already set a (lower) value.
#define SCHED_SLICE_RSS_MB 4500
// Max slices resident at once.  K concurrent engines * SCHED_SLICE_RSS_MB
// bounds peak memory (5 * 4.5GB = 22.5GB, under a 24GB ceiling on 48GB;
// measured practical peak is far lower -- ~7GB for 4 slices on GRP721-1 --
// since slices rarely approach their cap).
#define SCHED_MAX_PAR 5

// Signal-safe teardown of the concurrent schedule's slice children: an
// external SIGTERM/SIGINT to THIS process (e.g. a wrapping `timeout`) would
// otherwise kill the parent while its slice subtrees (child -> shell ->
// FindProof engine) keep running orphaned and accumulate across problems.
// The handler removes the slices' output files and killpg's every launched
// child group before exiting.  pids and output paths are stored by slice
// index, one slot per lane; a reaped pid slot is set back to -1.  A slice's
// own copies name only the slice itself (see run_schedule).
static volatile sig_atomic_t g_sched_pids[N_SCHED];
_Static_assert(sizeof g_sched_pids / sizeof g_sched_pids[0] >= (size_t)N_SCHED,
               "the teardown registry needs a slot for every schedule lane");
_Static_assert(sizeof(sig_atomic_t) >= sizeof(pid_t) && (sig_atomic_t)-1 < 0,
               "a teardown registry slot holds any pid and the empty mark -1");
static char g_sched_outs[N_SCHED][64];
static volatile sig_atomic_t g_sched_active = 0;
static void sched_sig_teardown(int sig) {
  if (g_sched_active) {
    // Before the kills: in a slice the killpg of its own group ends the
    // slice itself.
    for (int i = 0; i < N_SCHED; i++) {
      if (g_sched_outs[i][0] != '\0') {
        unlink(g_sched_outs[i]);
      }
    }
    for (int i = 0; i < N_SCHED; i++) {
      if (g_sched_pids[i] > 0) {
        killpg(g_sched_pids[i], SIGKILL);
        kill(g_sched_pids[i], SIGKILL);
      }
    }
  }
  _exit(128 + sig);
}

// --- hard wall enforcement -----------------------------------------
// The engine's wall check lives between completion steps, and a single
// step can starve it for hours: COL-class term blowup drives
// interreduction into GC thrash (gc_evacuate over gigabytes of live
// cells, re-entered immediately), so the step loop never regains
// control.  alarm(2) fires regardless of where the process is stuck.
// The handler is async-signal-safe: it writes a pre-built SZS line to
// the preserved stdout, tears down any schedule slices, and exits.
// The grace above the soft wall lets the engine's own graceful path
// (status line + proof) win whenever it is able to run.
static char g_alarm_szs[1200];
static int  g_alarm_fd = -1;
static int  g_alarm_exit_code = 6;   // 0 once a status is out (fe_decided)
static void cli_alarm_fire(int sig) {
  (void)sig;
  if (g_alarm_fd >= 0 && g_alarm_szs[0] != '\0') {
    size_t n = 0;
    while (g_alarm_szs[n] != '\0') {
      n++;
    }
    ssize_t w = write(g_alarm_fd, g_alarm_szs, n);
    (void)w;
  }
  if (g_sched_active) {
    sched_sig_teardown(SIGALRM);
  }
  _exit(g_alarm_exit_code);
}
static void cli_arm_wall_alarm(double wall, int grace, int fd,
                               const char *szs_line) {
  if (szs_line != NULL) {
    snprintf(g_alarm_szs, sizeof g_alarm_szs, "%s", szs_line);
  } else {
    g_alarm_szs[0] = '\0';
  }
  g_alarm_fd = fd;
  struct sigaction sa;
  sa.sa_handler = cli_alarm_fire;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGALRM, &sa, NULL);
  alarm((unsigned)(wall + grace + 0.999));
}

// A slice's captured stdout holds an SZS status line only when the slice
// decided the problem (a slice prints nothing else to stdout): an
// engine proof (the status is flushed before the proof block is
// built), a front-end refutation whose proof is still being rebuilt, a
// verified one-element model.  Returns the bytes to replay -- up to the
// last complete line, cut before an unterminated solution block -- or
// 0 when the file holds no status line.
static size_t sched_salvage_len(const char *text, size_t len) {
  const char *st = NULL;
  for (const char *p = text; p < text + len; p++) {
    if ((p == text || p[-1] == '\n') && (size_t)(text + len - p) > 13 &&
        strncmp(p, "% SZS status ", 13) == 0) {
      st = p;
      break;
    }
  }
  if (st == NULL) {
    return 0;
  }
  size_t keep = len;
  while (keep > 0 && text[keep - 1] != '\n') {
    keep--;
  }
  const char *open = NULL;
  for (const char *p = text; p < text + keep; p++) {
    if (p != text && p[-1] != '\n') {
      continue;
    }
    if (strncmp(p, "% SZS output start", 18) == 0) {
      open = p;
    } else if (strncmp(p, "% SZS output end", 16) == 0) {
      open = NULL;
    }
  }
  if (open != NULL) {
    keep = (size_t)(open - text);
  }
  return (keep > (size_t)(st - text)) ? keep : 0;
}

// CONCURRENT portfolio: run the slices in a bounded pool (SCHED_MAX_PAR at
// a time), EACH with the FULL wall -- not wall/N as the old sequential
// scheme did.  Sequential slicing starved every strategy (a problem
// needing >frac*wall of one config failed even though that config solves
// it within the full wall -- e.g. LCL113-10 needs 23s of the WM preset but
// the 0.22 slice gave it 6.6s).  First slice to exit 0 (proof found) wins;
// the rest are killed.  Each child writes its stdout to a temp file; only
// the winner's is replayed, so SZS/proof output never interleaves.
//
// only >= 0 (--lane=NAME) runs SCHEDULE[only] alone through this same
// path -- its env defaults, slice wall, command line, deadline and final
// status -- so a single-lane result is what that lane's slot does inside
// a schedule run at the same --wall.  The slice keeps its stderr (the
// schedule discards it) and reports how it ended, for per-lane harvests.
static int run_schedule(const char *self, const char *file, double wall,
                        int szs, int want_proof, int only) {
  // Install the teardown handler + register the (initially empty) pid slots
  // so an external timeout can't orphan the slice subtrees.
  for (int i = 0; i < N_SCHED; i++) {
    g_sched_pids[i] = -1;
    g_sched_outs[i][0] = '\0';
  }
  struct sigaction sa, old_term, old_int;
  sa.sa_handler = sched_sig_teardown;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, &old_term);
  sigaction(SIGINT,  &sa, &old_int);
  g_sched_active = 1;
  // The signals whose handlers tear the schedule down.  Each launch holds
  // them from recording the slice's output path until the new slice is
  // registered: delivered after fork(), the handler would kill every slice
  // but the one just forked.
  sigset_t teardown_sigs, prev_mask;
  sigemptyset(&teardown_sigs);
  sigaddset(&teardown_sigs, SIGTERM);
  sigaddset(&teardown_sigs, SIGINT);
  sigaddset(&teardown_sigs, SIGALRM);
  // Belt-and-braces: the wait loop below stops the slices at the wall;
  // should the parent itself stall, the alarm still emits one status
  // line a second past the wall and reaps the subtrees.
  {
    char line[1100];
    line[0] = '\0';
    if (szs) {
      snprintf(line, sizeof line, "%% SZS status Timeout for %s\n", file);
    }
    cli_arm_wall_alarm(wall, 1, 1, line[0] ? line : NULL);
  }
#ifdef PR_SET_CHILD_SUBREAPER
  // A lane run's resource use is its slice's.  The wall stop kills the
  // slice's group, orphaning its shell and engine; as their subreaper
  // this process waits for them below, so their CPU time and peak RSS
  // reach whoever measures it (the per-lane harvest's GNU time).
  if (only >= 0) {
    prctl(PR_SET_CHILD_SUBREAPER, 1);
  }
#endif
  // Bound each child's RSS unless the caller set a tighter cap already.
  if (getenv("THVM_ATP_RSS_ABORT_MB") == NULL) {
    char capbuf[16];
    snprintf(capbuf, sizeof capbuf, "%d", SCHED_SLICE_RSS_MB);
    setenv("THVM_ATP_RSS_ABORT_MB", capbuf, 1);
  }
  pid_t pids[N_SCHED];
  int   done[N_SCHED];
  for (int i = 0; i < N_SCHED; i++) {
    pids[i] = -1;
    done[i] = 0;
  }
  int first = (only >= 0) ? only : 0;
  int end   = (only >= 0) ? only + 1 : N_SCHED;
  int launched = 0, next = first, winner = -1, n_inerr = 0;
  // Concurrency width: SCHED_MAX_PAR by default (a laptop can only run a
  // few slices at once), env-overridable so a many-core box runs the
  // WHOLE portfolio in one concurrent wave -- the fair CASC condition is
  // one wall budget with every strategy racing, not sequential waves that
  // multiply the wall.  Clamped to [1, N_SCHED].
  int max_par = SCHED_MAX_PAR;
  {
    const char *mp = getenv("THVM_ATP_SCHED_MAX_PAR");
    if (mp != NULL && mp[0] != '\0') {
      int v = atoi(mp);
      if (v >= 1 && v <= N_SCHED) {
        max_par = v;
      }
    }
  }
  // Global wall deadline: the schedule shares ONE wall budget across all
  // waves.  Each slice launches with the REMAINING wall, never a fresh
  // full wall -- otherwise a wave-2 slice (position > max_par, common on
  // FOF input where far more than max_par lanes apply) runs its own full
  // wall stacked on wave-1's, the parent rides to its wall+60 alarm, and
  // the competition's hard wall/CPU kill truncates the run to NoOutput
  // instead of a clean GaveUp.  A small margin leaves the parent time to
  // print its status line inside the hard limit.  This is the "one wall
  // budget, every strategy racing" condition the concurrency comment
  // above describes -- now actually enforced.
  double sched_t0 = horn_now();
  double budget = (wall > 6.0) ? wall - 3.0 : wall;
  // The status line must be out by the wall whatever the slices do: a
  // slice past its own wall (a stage blind to it, an engine step that
  // never polls) is stopped here, half a second before the wall, instead
  // of holding the parent in wait() until its alarm.
  double deadline = sched_t0 + ((wall > 2.0) ? wall - 0.5 : wall);
  int stopped = 0;
  // Launch up to max_par, then refill as children exit.
  while (winner < 0 && (next < end || launched > 0)) {
    while (winner < 0 && launched < max_par && next < end) {
      double rem = budget - (horn_now() - sched_t0);
      if (rem < 2.0) {
        break;   // no wall budget left: stop launching slices
      }
      int i = next++;
      double slice_wall = rem;
      if (g_cli_verbose) {
        fprintf(stderr, "%% FindProof schedule slice %s (%.0fs, remaining wall)\n",
                SCHEDULE[i].name, slice_wall);
      }
      sigprocmask(SIG_BLOCK, &teardown_sigs, &prev_mask);
      snprintf(g_sched_outs[i], sizeof g_sched_outs[i],
               "/tmp/thvm_sched_%d_%d.out", (int)getpid(), i);
      pid_t pid = fork();
      if (pid == 0) {
        // Child: own process group so the parent can kill the WHOLE
        // subtree (this child + the shell system() forks + its FindProof
        // grandchild) with one killpg -- otherwise killing the child
        // blocked in system() would orphan the grandchild engine, which
        // keeps running to its wall and could overlap the next problem.
        setpgid(0, 0);
        // The slice keeps the parent's teardown handlers, so its copies of
        // the registry and of the output paths name the slice alone: a
        // SIGTERM that reaches the slice (a pkill matching the command line
        // it shares with the parent) takes its shell, its engine and its
        // output file with it, and never a sibling.
        for (int k = 0; k < N_SCHED; k++) {
          g_sched_pids[k] = -1;
          if (k != i) {
            g_sched_outs[k][0] = '\0';
          }
        }
        g_sched_pids[i] = getpid();
        // The slice's shell and engine inherit the mask: with SIGALRM
        // held, the engine's own wall alarm would never fire.
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        // Redirect stdout to the slice temp file, run the slice.
        int fd = open(g_sched_outs[i], O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
          dup2(fd, 1);
          close(fd);
        }
        // Silence the slice's diagnostics (bench PRSYM/stats/progress all
        // go to stderr): the grandchild engine inherits this fd 2, so the
        // whole portfolio stays quiet.  The winner's proof is on stdout
        // (the temp file above), so nothing checkable is lost.  --verbose
        // keeps the stream for debugging.
        // THVM_ATP_SCHED_ERRDIR=<dir> keeps each slice's stderr in
        // <dir>/<slice>.err instead: the per-lane exit reason is the
        // first question an all-lanes-gave-up diagnosis asks, and the
        // verbose stream interleaves every lane's narration unlabeled.
        const char *ed = getenv("THVM_ATP_SCHED_ERRDIR");
        if (ed != NULL && ed[0] != '\0') {
          char ep[1024];
          snprintf(ep, sizeof ep, "%s/%s.err", ed, SCHEDULE[i].name);
          int ef = open(ep, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (ef >= 0) {
            dup2(ef, 2);
            close(ef);
          }
        } else if (!g_cli_verbose && only < 0) {
          int dn = open("/dev/null", O_WRONLY);
          if (dn >= 0) {
            dup2(dn, 2);
            close(dn);
          }
        }
        // FINDPROOF_SLICE carries the slice name: every consumer tests
        // only its presence, and the name labels the process for
        // diagnostics (THVM_ATP_CLOCK_TRACE headers, /proc environ).
        char cmd[4096];
        snprintf(cmd, sizeof cmd, "FINDPROOF_SLICE='%s' '%s' %s --wall=%.0f%s%s '%s'",
                 SCHEDULE[i].name, self, SCHEDULE[i].flags, slice_wall,
                 szs ? " --szs" : "", want_proof ? " --proof" : "", file);
        // Forward the slice's real exit code: 2 (input/environment error,
        // e.g. an unresolvable include) must stay distinguishable from 1
        // (searched and gave up) so the parent doesn't mask a broken
        // input as an honest GaveUp.
        int st = system(cmd);
        // The exit code below folds a signal into 1, the code of an
        // honest give-up; a lane run names the real end on stderr so a
        // crash stays distinguishable from a search that gave up.
        if (only >= 0) {
          if (st == -1) {
            fprintf(stderr, "%% FindProof lane %s: launch failed\n",
                    SCHEDULE[i].name);
          } else if (WIFSIGNALED(st)) {
            fprintf(stderr, "%% FindProof lane %s: signal %d\n",
                    SCHEDULE[i].name, WTERMSIG(st));
          } else {
            fprintf(stderr, "%% FindProof lane %s: exit %d\n",
                    SCHEDULE[i].name, WEXITSTATUS(st));
          }
        }
        _exit((st != -1 && WIFEXITED(st)) ? WEXITSTATUS(st) : 1);
      }
      // Set the child's pgid parent-side too (race-free: whichever of
      // parent/child wins, the group id equals the child pid).
      if (pid > 0) {
        setpgid(pid, pid);
      }
      pids[i] = pid;
      g_sched_pids[i] = pid;   // for the signal handler
      sigprocmask(SIG_SETMASK, &prev_mask, NULL);
      launched++;
    }
    if (launched == 0) {
      break;
    }
    int status = 0;
    pid_t w = waitpid(-1, &status, WNOHANG);
    if (w == 0) {
      if (horn_now() >= deadline) {
        stopped = 1;
        break;
      }
      struct timespec nap = { 0, 10000000L };
      nanosleep(&nap, NULL);
      continue;
    }
    if (w < 0) {
      break;
    }
    for (int i = 0; i < N_SCHED; i++) {
      if (pids[i] == w) {
        done[i] = 1;
        launched--;
        g_sched_pids[i] = -1;   // reaped: clear
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
          winner = i;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == 2) {
          n_inerr++;
        }
        break;
      }
    }
  }
  // Stopped at the wall: a slice still running may already have decided
  // (its status line is out, the proof still being built or rebuilt) --
  // it wins with what it printed.  Stop them all first, so no file grows
  // while it is read.
  size_t salvage_len = 0;
  char *salvage = NULL;
  if (stopped) {
    for (int i = 0; i < N_SCHED; i++) {
      if (pids[i] > 0 && !done[i]) {
        killpg(pids[i], SIGKILL);
        kill(pids[i], SIGKILL);
      }
    }
    for (int i = 0; i < N_SCHED && winner < 0; i++) {
      if (pids[i] <= 0 || done[i] || g_sched_outs[i][0] == '\0') {
        continue;
      }
      size_t len = 0;
      char *txt = tp_read_file(g_sched_outs[i], &len);
      size_t keep = (txt != NULL) ? sched_salvage_len(txt, len) : 0;
      if (keep > 0) {
        winner = i;
        salvage = txt;
        salvage_len = keep;
      } else {
        free(txt);
      }
    }
    fprintf(stderr, "FindProof: schedule stopped at the wall (%.1fs)%s\n",
            horn_now() - sched_t0, (winner >= 0) ? "; a slice had decided" : "");
  }
  // Kill + reap any still-running slices (the winner made them redundant).
  // killpg on the child's group takes down the shell + FindProof grandchild
  // too, so no engine is left orphaned to run to its wall.
  for (int i = 0; i < N_SCHED; i++) {
    if (pids[i] > 0 && !done[i]) {
      killpg(pids[i], SIGKILL);
      kill(pids[i], SIGKILL);
    }
  }
  for (int i = 0; i < N_SCHED; i++) {
    if (pids[i] > 0 && !done[i]) {
      int st;
      waitpid(pids[i], &st, 0);
      g_sched_pids[i] = -1;
    }
  }
  // The killed slice's orphans, reparented here (subreaper above).
  if (only >= 0) {
    double reap_until = horn_now() + 1.0;
    for (;;) {
      int st;
      pid_t r = waitpid(-1, &st, WNOHANG);
      if (r < 0) {
        break;
      }
      if (r == 0) {
        if (horn_now() >= reap_until) {
          break;
        }
        struct timespec nap = { 0, 5000000L };
        nanosleep(&nap, NULL);
      }
    }
  }
  // Teardown handler no longer needed: restore the previous dispositions.
  alarm(0);
  g_sched_active = 0;
  sigaction(SIGTERM, &old_term, NULL);
  sigaction(SIGINT,  &old_int,  NULL);
  int rc = 1;
  if (winner >= 0) {
    // Name the winning slice on stderr: which lane carried the solve is
    // the first question every schedule-level diagnosis asks (proof
    // emission triage, portfolio tuning), and only the index existed.
    fprintf(stderr, "%% FindProof schedule winner: %s\n",
            SCHEDULE[winner].name);
    // Replay only the winning slice's captured output.
    if (salvage != NULL) {
      fwrite(salvage, 1, salvage_len, stdout);
    } else {
      FILE *fp = fopen(g_sched_outs[winner], "r");
      if (fp != NULL) {
        char   buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, fp)) > 0) {
          fwrite(buf, 1, n, stdout);
        }
        fclose(fp);
      }
    }
    rc = 0;
  } else if (n_inerr == end - first) {
    // Every slice failed before searching (the input error precedes the
    // per-slice transforms, so it hits all of them identically): this is
    // a broken input or environment, not a give-up.
    fprintf(stderr, "FindProof: all schedule slices reported an input error\n");
    if (szs) {
      printf("%% SZS status Error for %s\n", file);
    }
    rc = 2;
  } else if (szs) {
    printf("%% SZS status %s for %s\n", stopped ? "Timeout" : "GaveUp", file);
  }
  free(salvage);
  for (int i = 0; i < N_SCHED; i++) {
    if (g_sched_outs[i][0] != '\0') {
      unlink(g_sched_outs[i]);
    }
  }
  return rc;
}

// --- fof-split orchestration (parent side) ---------------------------
// See the split module's soundness header.  One child per branch,
// SEQUENTIAL, each on remaining-wall / branches-left; every child
// must refute (exit 0) or the case analysis is GaveUp.  The children
// inherit every knob env through fork; THVM_ATP_FOF_BRANCH carries
// the picks and FINDPROOF_SLICE keeps them silent on failure.

// Print one child's Proof block with its terminal (last) record's
// name prefixed br<b>_ -- the parent's avatar_contradiction_clause
// cites it, and nothing inside the block does (it is the root).
// Interior records keep their names: the shared conversion-lineage
// records are byte-identical across branches.  Returns 0 when the
// block or the root is missing.
static int fof_split_emit_branch(FILE *out, int b, const char *path,
                                 char *root, size_t root_cap) {
  size_t len = 0;
  char *txt = tp_read_file(path, &len);
  if (txt == NULL) return 0;
  char *start = strstr(txt, "% SZS output start");
  char *bodynl = (start != NULL) ? strchr(start, '\n') : NULL;
  char *end = (bodynl != NULL) ? strstr(bodynl, "% SZS output end") : NULL;
  if (bodynl == NULL || end == NULL) { free(txt); return 0; }
  char *body = bodynl + 1;
  *end = '\0';
  char *head = NULL;
  for (char *p = body; *p != '\0'; p++) {
    if ((p == body || p[-1] == '\n') &&
        (strncmp(p, "cnf(", 4) == 0 || strncmp(p, "fof(", 4) == 0)) head = p;
  }
  if (head == NULL) { free(txt); return 0; }
  char *nm = head + 4;
  size_t o = 0;
  while ((isalnum((unsigned char)nm[o]) || nm[o] == '_') && o + 1 < root_cap) {
    root[o] = nm[o];
    o++;
  }
  root[o] = '\0';
  fwrite(body, 1, (size_t)(nm - body), out);
  fprintf(out, "br%d_", b);
  fputs(nm, out);
  free(txt);
  return o > 0;
}

static int fof_split_orchestrate(const char *self, const char *file,
                                 const char *preset, const char *max_steps,
                                 int szs, int want_proof) {
  int nor = g_fof_split_nor;
  int B = 1;
  for (int j = 0; j < nor; j++) B *= g_fof_split_or[j].width;
  int slice = getenv("FINDPROOF_SLICE") != NULL;
  fprintf(stderr, "FindProof: fof-split: %d ground or-clause(s), "
          "%d branch(es)\n", nor, B);
  // Hard wall over the whole case analysis (grace covers a child's
  // own grace window so its alarm normally fires first).
  {
    double left = g_cli_wall_sec - (horn_now() - g_cli_main_t0);
    if (left < 1.0) left = 1.0;
    char line[1100];
    line[0] = '\0';
    if (szs && !slice)
      snprintf(line, sizeof line, "%% SZS status Timeout for %s\n", file);
    cli_arm_wall_alarm(left, 60, 1, line[0] ? line : NULL);
  }
  static char outs[FOF_SPLIT_MAX_BR][128];
  static char roots[FOF_SPLIT_MAX_BR][64];
  static char picks[FOF_SPLIT_MAX_BR][FOF_SPLIT_MAX_OR * 8];
  int all_ok = 1;
  int b = 0;
  for (b = 0; b < B; b++) {
    // Decode branch b row-major over the or-clause widths.
    int digit[FOF_SPLIT_MAX_OR];
    int rem = b;
    for (int j = nor - 1; j >= 0; j--) {
      digit[j] = rem % g_fof_split_or[j].width;
      rem /= g_fof_split_or[j].width;
    }
    size_t po = 0;
    picks[b][0] = '\0';
    for (int j = 0; j < nor; j++)
      po += (size_t)snprintf(picks[b] + po, sizeof picks[b] - po, "%s%d",
                             (j > 0) ? "," : "", digit[j]);
    double left = g_cli_wall_sec - (horn_now() - g_cli_main_t0);
    double wb = left / (double)(B - b);
    if (wb < 1.0) wb = 1.0;
    snprintf(outs[b], sizeof outs[b], "/tmp/thvm_split_%d_%d.out",
             (int)getpid(), b);
    char cmd[4096];
    snprintf(cmd, sizeof cmd,
             "FINDPROOF_SLICE=1 THVM_ATP_FOF_BRANCH='%s' '%s' --preset=%s "
             "--max-steps=%s --wall=%.1f --szs%s '%s' > '%s'%s",
             picks[b], self, preset, max_steps, wb,
             want_proof ? " --proof" : "", file, outs[b],
             g_cli_verbose ? "" : " 2>/dev/null");
    int st = system(cmd);
    int ok = (st != -1 && WIFEXITED(st) && WEXITSTATUS(st) == 0);
    fprintf(stderr, "FindProof: fof-split branch %d/%d [%s]: %s "
            "(cap %.1fs)\n", b + 1, B, picks[b],
            ok ? "refuted" : "open", wb);
    if (!ok) { all_ok = 0; break; }   // one open branch: case analysis dead
  }
  alarm(0);
  int rc = 1;
  if (all_ok) {
    if (szs)
      printf("%% SZS status %s for %s\n",
             g_cli_fof_conjecture ? "Theorem" : "Unsatisfiable", file);
    if (want_proof) {
      // Stitch: every branch block (terminal renamed br<b>_*), then
      // the parent's or-clause lineage + split records closing the
      // case-analysis DAG at $false.
      char *bufp = NULL;
      size_t plen = 0;
      FILE *ms = open_memstream(&bufp, &plen);
      int ok = (ms != NULL);
      for (int i = 0; i < B && ok; i++)
        ok = fof_split_emit_branch(ms, i, outs[i], roots[i], sizeof roots[i]);
      if (ok) {
        prov_reset_emitted();
        for (int j = 0; j < nor && ok; j++) {
          FofSplitOr *so = &g_fof_split_or[j];
          ok = prov_emit_cnf(ms, so->cnf_ix);
          for (int i = 0; i < so->width && ok; i++) {
            if (so->pi_text[i] == NULL) { ok = 0; break; }
            fprintf(ms, "fof(spl%d_%d, definition, (spl%d_%d <=> (%s)), "
                        "introduced(definition, [new_symbols(naming, "
                        "[spl%d_%d])], [avatar_definition])).\n",
                    j, i, j, i, so->pi_text[i], j, i);
          }
          if (!ok) break;
          fprintf(ms, "fof(split_c%d, plain, (", j);
          for (int i = 0; i < so->width; i++)
            fprintf(ms, "%sspl%d_%d", (i > 0) ? " | " : "", j, i);
          fprintf(ms, "), inference(avatar_split_clause, [status(thm)], "
                      "[c%d", so->cnf_ix);
          for (int i = 0; i < so->width; i++) fprintf(ms, ", spl%d_%d", j, i);
          fputs("])).\n", ms);
        }
        for (int i = 0; i < B && ok; i++) {
          int digit[FOF_SPLIT_MAX_OR];
          int rem = i;
          for (int j = nor - 1; j >= 0; j--) {
            digit[j] = rem % g_fof_split_or[j].width;
            rem /= g_fof_split_or[j].width;
          }
          fprintf(ms, "fof(branch_%d, plain, (", i);
          for (int j = 0; j < nor; j++)
            fprintf(ms, "%s~spl%d_%d", (j > 0) ? " | " : "", j, digit[j]);
          fprintf(ms, "), inference(avatar_contradiction_clause, "
                      "[status(thm)], [br%d_%s])).\n", i, roots[i]);
        }
        if (ok) {
          fputs("fof(split_refutation, plain, $false, "
                "inference(avatar_sat_refutation, [status(thm)], [", ms);
          for (int j = 0; j < nor; j++) fprintf(ms, "%ssplit_c%d",
                                                (j > 0) ? ", " : "", j);
          for (int i = 0; i < B; i++) fprintf(ms, ", branch_%d", i);
          fputs("])).\n", ms);
        }
      }
      if (ms != NULL) fclose(ms);
      if (ok && bufp != NULL) {
        printf("%% SZS output start Proof for %s\n", file);
        cli_proof_write(stdout, bufp, plen);
        printf("%% SZS output end Proof for %s\n", file);
      } else {
        fprintf(stderr, "FindProof: fof-split: branch proof stitching "
                "incomplete; omitting the solution block\n");
      }
      free(bufp);
    }
    fflush(stdout);
    rc = 0;
  } else if (szs && !slice) {
    printf("%% SZS status GaveUp for %s\n", file);
    fflush(stdout);
  }
  for (int i = 0; i < B; i++)
    if (i <= b && outs[i][0] != '\0') unlink(outs[i]);
  return rc;
}

static void usage(FILE *fp) {
  fprintf(fp,
    "usage: FindProof [options] problem.pr | problem.p\n"
    "  --preset=waldmeister        TFindProof Method -> \"Waldmeister\" stack\n"
    "  --preset=waldmeister-lazy   Method -> \"WaldmeisterLazy\" stack\n"
    "  --preset=none               bare engine defaults\n"
    "  --<knob>[=v]                individual Method suboptions (override preset);\n"
    "                              bare boolean flags mean =1 (--list-knobs for all)\n"
    "  --max-steps=N               CP-processing cap        (default 500000)\n"
    "  --wall=SEC                  wall-clock cap           (default 120)\n"
    "  --szs                       print an SZS status line (CNF ontology for TPTP input)\n"
    "  --schedule                  time-sliced portfolio of configurations (child\n"
    "                              process per slice; first proof wins)\n"
    "  --lane=NAME                 run one schedule lane alone, exactly as the\n"
    "                              schedule launches it (its stderr kept)\n"
    "  --list-lanes                print the schedule's lane names in order and exit\n"
    "  --proof                     print an SZS CNFRefutation block on success\n"
    "  --keep-pr                   keep (and name) the .pr generated from TPTP input\n"
    "  --list-knobs                print the knob table and exit\n"
    "TPTP CNF UEQ input (.p) is converted to Waldmeister .pr form in-process;\n"
    "include('Axioms/*.ax') resolves against the problem directory, $TPTP, CWD.\n"
    "TPTP FOF input (and non-unit CNF) is clausified and encoded into unit\n"
    "equations: Horn clauses via the '-10' ifeq scheme, non-Horn clauses via a\n"
    "sound-but-incomplete or/eq reification (--fof-or=0 restores the Horn-only\n"
    "give-up); success reports SZS Theorem for a conjecture, failure always\n"
    "GaveUp (--fof=0 disables the front-end).\n");
}

int main(int argc, char **argv) {
  const char *preset = "waldmeister";
  const char *file = NULL;
  const char *max_steps = "500000";
  const char *wall = "120";
  int szs = 0, keep_pr = 0, want_schedule = 0, want_check = 0;
  const char *lane = NULL;
  double main_t0 = horn_now();
  g_cli_main_t0 = main_t0;
  // Bench-intake give-ups (e.g. --full-ac=2 with no AC operator) route
  // through tp_giveup so direct runs still print the SZS line.
  g_atp_bench_giveup_hook = tp_giveup;
  if (getenv("THVM_ATP_VERBOSE") != NULL) g_cli_verbose = 1;

  // Invoked as CheckProof (the ProoVer entry binary), the checker is the
  // default mode -- and the STRICT ProoVer profile (verify leaves against
  // the linked problem file, discharge unmodelled thm/cth steps through
  // the external-ATP hook).  FindProof stays the prover, and its --check
  // keeps the legacy internal-trace profile (trusted file()/introduced()
  // leaves) used to certify FindProof's own proofs.
  // THVM_CHECKPROOF_PROVE=1 forces prover mode regardless of the binary
  // name: the checker's external-ATP hook re-execs this same binary as
  // its per-step oracle.
  {
    const char *self = strrchr(argv[0], '/');
    self = (self != NULL) ? self + 1 : argv[0];
    if (strncmp(self, "CheckProof", 10) == 0 &&
        getenv("THVM_CHECKPROOF_PROVE") == NULL) {
      want_check = 1;
      pc_strict_default = 1;
    }
    pc_self_path = argv[0];
  }

  for (int i = 1; i < argc; i++) {
    char *a = argv[i];
    if (strncmp(a, "--", 2) != 0) {
      if (file != NULL) { fprintf(stderr, "FindProof: one problem file only\n"); return 2; }
      file = a;
      continue;
    }
    char *name = a + 2;
    char *val = strchr(name, '=');
    size_t nlen = (val != NULL) ? (size_t)(val - name) : strlen(name);
    if (val != NULL) val++;
    if (nlen == 4 && strncmp(name, "help", 4) == 0) { usage(stdout); return 0; }
    if (nlen == 10 && strncmp(name, "list-knobs", 10) == 0) {
      for (int k = 0; k < N_KNOBS; k++)
        printf("  --%-26s %-34s %s\n", KNOBS[k].flag, KNOBS[k].env, KNOBS[k].method);
      return 0;
    }
    if (nlen == 10 && strncmp(name, "list-lanes", 10) == 0) {
      for (int k = 0; k < N_SCHED; k++) {
        printf("%s\n", SCHEDULE[k].name);
      }
      return 0;
    }
    if (nlen == 4 && strncmp(name, "lane", 4) == 0) {
      if (val == NULL || val[0] == '\0') {
        fprintf(stderr, "FindProof: --lane needs a lane name (--list-lanes)\n");
        return 2;
      }
      lane = val;
      continue;
    }
    if (nlen == 3 && strncmp(name, "szs", 3) == 0) { szs = 1; continue; }
    if (nlen == 8 && strncmp(name, "schedule", 8) == 0) { want_schedule = 1; continue; }
    if (nlen == 5 && strncmp(name, "check", 5) == 0) { want_check = 1; continue; }
    if (nlen == 7 && strncmp(name, "verbose", 7) == 0) { g_cli_verbose = 1; continue; }
    if (nlen == 5 && strncmp(name, "proof", 5) == 0) {
      g_cli_want_proof = 1;
      // --proof must not change the search: everything below either
      // records beside it or runs after it.
      //
      // The proof extractor cites each goal-closing rule through its
      // completion-trace lineage; the default 128K trace soft cap drops
      // the derivations of rules formed late in a deep completion, so
      // those citations resolve to nothing and the checker cannot bridge
      // the step.  Raise the cap for --proof runs so every proof-critical
      // lineage survives.  The entries past the default cap stay out of
      // the search's way (atp_trace_node): their nodes live in a
      // permanent heap tail the collector never copies (2^28 cells of
      // address space, touched only as entries arrive), their work runs
      // off the clock-gated disciplines' work clock, and the RSS guard
      // discounts their bytes.  An explicit THVM_ATP_TRACE_MAX /
      // THVM_HEAP_PERM_CELLS still wins.  Applies in the schedule too:
      // run_schedule re-execs each child with --proof, so the child
      // makes these requests before its engine init.
      //
      // All of it is requested in-process, never through setenv: the
      // search must run in the no-proof environment (an uncached getenv
      // in a hot path costs work in proportion to the environment's
      // size, which the instruction work clock counts).
      thvm_atp_request_trace_cap(16777216u);
      thvm_heap_request_perm_cells(268435456ull);
      // Deep citation resolve by default (cli_resolve_deep): a goal-
      // chain step whose rule was interreduced away otherwise resolves
      // to NO citation and the emitted rw record is unverifiable
      // (checker abstain).  Emission-time only; the env var overrides.
      //
      // Narrowing proof recorder: existential-goal closures (greedy
      // narrowing / the critical-goal queue) record replayable step
      // descriptors so the emitter prints a real instantiate/rw chain
      // instead of the "no single-NF chain extractable" marker (the
      // CASC-J13 15-problem empty-proof class).  Observation only: the
      // queue normalizes with the engine's own normalizer and logs its
      // splices, so the search is the no-proof search.
      g_atp_bench_narrow_proof = 1;
      // The treatment chains between a trace record and its parent are
      // re-derived at emission (thvm_atp_rederive, over the rule-set
      // journal), never recorded: the treatment recorder
      // (THVM_ATP_RECORD_NORM) changes the search, so it is not armed
      // here.
      continue;
    }
    if (nlen == 7 && strncmp(name, "keep-pr", 7) == 0) { keep_pr = 1; continue; }
    if (nlen == 12 && strncmp(name, "convert-only", 12) == 0) { keep_pr = 2; continue; }
    if (nlen == 6 && strncmp(name, "preset", 6) == 0) { preset = (val != NULL) ? val : "waldmeister"; continue; }
    if (nlen == 9 && strncmp(name, "max-steps", 9) == 0 && val != NULL) { max_steps = val; continue; }
    if (nlen == 4 && strncmp(name, "wall", 4) == 0 && val != NULL) { wall = val; continue; }
    int hit = 0;
    for (int k = 0; k < N_KNOBS; k++) {
      if (strlen(KNOBS[k].flag) == nlen && strncmp(name, KNOBS[k].flag, nlen) == 0) {
        if (KNOBS[k].has_value && val == NULL) {
          fprintf(stderr, "FindProof: --%s needs a value\n", KNOBS[k].flag);
          return 2;
        }
        setenv(KNOBS[k].env, (val != NULL) ? val : "1", 1);
        hit = 1;
        break;
      }
    }
    if (!hit) { fprintf(stderr, "FindProof: unknown option --%.*s\n", (int)nlen, name); usage(stderr); return 2; }
  }
  int lane_ix = -1;
  if (lane != NULL) {
    for (int k = 0; k < N_SCHED; k++) {
      if (strcmp(lane, SCHEDULE[k].name) == 0) {
        lane_ix = k;
        break;
      }
    }
    if (lane_ix < 0) {
      fprintf(stderr, "FindProof: unknown lane '%s' (--list-lanes prints the "
              "%d schedule lanes)\n", lane, N_SCHED);
      return 2;
    }
  }
  if (file == NULL) { usage(stderr); return 2; }
  // Engine wall, set BEFORE any front-end work: the FOF clausal
  // pre-pass budgets itself as a fraction of the remaining wall.
  g_cli_wall_sec = (atof(wall) > 0.0) ? atof(wall) : 120.0;

  if (want_check) return proof_check_main(file, szs);

  if (want_schedule || lane_ix >= 0) {
    return run_schedule(argv[0], file, atof(wall), szs, g_cli_want_proof,
                        lane_ix);
  }

  // Front-end wall (cli_fe_check): every stage before the engine abstains
  // once process start + --wall passes, and the alarm stops whatever does
  // not poll a second later -- silently in a schedule slice, with the
  // Timeout line on a direct run.  The engine re-arms it with its own
  // grace when it starts.
  g_cli_fe_deadline = main_t0 + g_cli_wall_sec;
  {
    char line[1100];
    line[0] = '\0';
    if (szs && getenv("FINDPROOF_SLICE") == NULL)
      snprintf(line, sizeof line, "%% SZS status Timeout for %s\n", file);
    cli_arm_wall_alarm(g_cli_wall_sec, 1, 1, line[0] ? line : NULL);
  }

  // Presets replicate the WL $AtpPresetDefaults stacks; setenv(...,0)
  // never overwrites an explicit knob flag, so flags override the
  // preset exactly like Method suboptions override a preset name.
  if (strcmp(preset, "waldmeister") == 0) {
    setenv("THVM_ATP_WALDMEISTER", "1", 0);
  } else if (strcmp(preset, "waldmeister-lazy") == 0) {
    setenv("THVM_ATP_WALDMEISTER", "1", 0);
    setenv("THVM_ATP_LAZY_NORM", "1", 0);
    setenv("THVM_ATP_CP_SET_IR", "1", 0);
    setenv("THVM_ATP_AUTO_MAXW", "30", 0);
  } else if (strcmp(preset, "none") == 0) {
    // bare engine defaults
  } else {
    fprintf(stderr, "FindProof: unknown preset %s\n", preset);
    return 2;
  }

  // TPTP input -> temp .pr (detected by content: a cnf( / fof( block).
  g_cli_problem = file;
  const char *run_file = file;
  char tmp_pr[1024] = "";
  {
    size_t rl = 0;
    char *raw = tp_read_file(file, &rl);
    if (raw == NULL) { fprintf(stderr, "FindProof: cannot open %s\n", file); return 2; }
    // --fof-slice=2 (the schedule's FOF pack): exit immediately unless
    // the input carries a fof() record, freeing the pool slot on the
    // whole UEQ/CNF corpus (the equals/horn-cd =2 discipline).  Checked
    // on the raw text BEFORE any front-end work, so a non-FOF slice
    // costs one fork + one read.
    {
      const char *fs = getenv("THVM_ATP_FOF_SLICE");
      if (fs != NULL && atoi(fs) >= 2 && !tp_input_is_fof(raw)) {
        g_cli_szs_wanted = szs;
        g_cli_probpath   = file;
        tp_giveup("fof slice: input has no fof() record");
      }
    }
    // TPTP input is recognized by a cnf(/fof( record OR an include()
    // directive -- a problem whose clauses all live in an included axiom
    // file (e.g. RNG042-2 = just `include('Axioms/RNG002-0.ax')`) has no
    // cnf( in the top-level text, and must NOT fall through to the bench,
    // which would run its built-in default problem and emit a spurious
    // proof.  Preprocessing splices the include, so the parse below sees
    // the real clauses.
    // Typed formats (thf(/tff() are NOT supported: reject BEFORE the
    // cnf/fof/include detection, so a typed file carrying include()
    // directives (most do) cannot ride the FOF front-end, and one with
    // neither cannot fall through to the .pr branch.  A mixed file
    // with genuine cnf(/fof( records still takes the TPTP path (the
    // parser ignores the typed records, as before).
    if ((tp_raw_marker(raw, "thf(") || tp_raw_marker(raw, "tff("))
        && !tp_raw_marker(raw, "cnf(") && !tp_raw_marker(raw, "fof(")) {
      g_cli_szs_wanted = szs;
      g_cli_probpath   = file;
      free(raw);
      tp_giveup("typed (thf/tff) input is not supported");
    }
    if (tp_raw_marker(raw, "cnf(") || tp_raw_marker(raw, "fof(")
        || tp_raw_marker(raw, "include(")) {
      g_cli_tptp_mode = 1;
      g_cli_szs_wanted = szs;
      g_cli_probpath = file;
      // Conversion-lineage capture rides every --proof run (the FOF
      // front-end plus the def-unfold / equals-goal transforms); with
      // no transform fired the leaf table stays empty and the emitted
      // bytes are unchanged.
      g_prov_on = g_cli_want_proof;
      // Stand-in names are noted on every run: a --proof derivation and
      // the one-element FiniteModel both print the problem's spelling.
      g_tp_ren_on = 1;
      char *pre = tp_preprocess(file, raw);
      pre = tp_unquote_names(pre);
      tp_shorten_names(pre);
      cli_fe_check("TPTP preprocessing");
      // The term heap is sized once, at its first use: a knowledge base
      // whose clausification outgrows the default semi-space (the 27MB
      // BIO axiom file: ~363k clauses) gets a reservation scaled to its
      // text (the mapping is lazy -- only touched cells cost memory).
      // An explicit THVM_HEAP_CELLS / --heap-cells wins.
      {
        size_t plen = strlen(pre);
        if (getenv("THVM_HEAP_CELLS") == NULL && plen > (8u << 20)) {
          u64 cells = HEAP_CAP;
          while (cells < 64u * (u64)plen && cells < (1ull << 31)) cells <<= 1;
          char hc[32];
          snprintf(hc, sizeof hc, "%llu", (unsigned long long)cells);
          setenv("THVM_HEAP_CELLS", hc, 1);
        }
      }
      TpProblem *tpp = (TpProblem *)calloc(1, sizeof(TpProblem));
      if (tpp == NULL) { fprintf(stderr, "FindProof: OOM\n"); return 2; }
#define tp (*tpp)
      // FOF records (or non-unit / non-equational CNF) go through the
      // clausal front-end; pure UEQ input keeps the byte-identical
      // tp_parse path.
      if (tp_needs_clausal(pre)) tp_fof_load(&tp, pre);
      else if (getenv("THVM_ATP_ONE_ELT_PROBE") != NULL)
        tp_giveup("one-element probe: unit-equality input (no gate)");
      else tp_parse(&tp, pre);
      free(pre);
      int n_neg = 0;
      for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
      if (n_neg == 0 || tp.n_cls == n_neg) {
        // No goal to refute (an axiom-only satisfiability problem like
        // RNG042-2) or no axioms: a completion prover cannot decide
        // satisfiability, so the ONLY sound answer is GaveUp -- never a
        // refutation.  tp_giveup prints SZS GaveUp (exit 1) on a direct
        // run and stays silent under a schedule slice.
        g_cli_szs_wanted = szs;
        g_cli_probpath   = file;
        tp_giveup("no negated conjecture (satisfiability problem; not refutable)");
      }
      // --ifeq-guard=2 (the schedule's ifeq slice): exit cleanly when
      // the problem carries no ifeq dispatcher axiom, freeing the pool
      // slot immediately (the equals/horn-cd =2 pattern).  Checked
      // BEFORE the Horn release so the give-up costs no chaining wall.
      {
        const char *ig = getenv("THVM_ATP_IFEQ_GUARD");
        if (ig != NULL && atoi(ig) >= 2 && !tp_has_ifeq_dispatcher(&tp))
          tp_giveup("ifeq-guard slice: no ifeq dispatcher axiom");
      }
      // Wide-functor currying: decompose any n-ary functor (n > 8) into
      // nested binary ap2 applications so the arity-8 term reader can
      // load it (TOP052's tuple/140).  Sound here, on the unit-encoded
      // (Horn) table, not before it (curry.c); a no-op unless a wide
      // functor is present.  THVM_ATP_CURRY=0 keeps
      // the old clean give-up.  BEFORE the Horn release: the release
      // snapshots strdup the clause strings they chain over, and a
      // later curry left every emitted hyper-resolution record in the
      // UNCURRIED presentation while the engine's trace records print
      // curried -- the two halves of a PUZ039-10 proof then talk about
      // different signatures and no step crossing them verifies.
      {
        const char *ce = getenv("THVM_ATP_CURRY");
        if (ce == NULL || ce[0] != '0') tp_curry_wide(&tp);
      }
      cli_fe_check("the equational encoding");
      // The deferred FOF clausal pre-pass (see tp_fof_load) runs here,
      // once the encoding exists and is curried, with its usual share
      // of the wall -- AHEAD of the Horn release, whose oversized-KB
      // budget (0.4 of the wall) would otherwise eat into the
      // saturation's 60%.  A give-up before this point ran it already,
      // with the whole wall, through the tp_giveup hook.  A refutation
      // sets g_cli_fof_satref: the front-end decision below prints it.
      int prepass_won = fof_prepass_run(0.0);
      // Encoded-Horn (ifeq) problems: decode the dispatcher encoding
      // and forward-chain released units BEFORE SInE, so the releases
      // participate in axiom selection.  THVM_ATP_HORN=0 disables.
      {
        const char *he = getenv("THVM_ATP_HORN");
        if ((he == NULL || he[0] != '0') && !prepass_won) tp_horn_release(&tp);
      }
      // Goal reached by the chaining itself: some released unit (a
      // logical consequence, by the modus-ponens-only invariant)
      // unifies with a negative clause, i.e. the axioms entail an
      // instance of the negated equation -- a refutation with no
      // engine run.  Decisive for problems the engine cannot even
      // load (PUZ039's state/12 exceeds the term reader's arity cap).
      // A --proof run decides here exactly like the no-proof run and
      // prints the same status; a stashed front-end proof follows it,
      // otherwise the engine run below only reconstructs the proof.
      if ((g_horn_goal_reached || g_cli_fof_trivial || g_cli_fof_satref) &&
          szs) {
        printf("%% SZS status %s for %s\n",
               g_cli_fof_conjecture ? "Theorem" : "Unsatisfiable", file);
        fflush(stdout);
        if (!g_cli_want_proof) return 0;
        if (g_cli_prepass_proof != NULL) {
          cli_proof_write(stdout, g_cli_prepass_proof,
                          strlen(g_cli_prepass_proof));
          fflush(stdout);
          return 0;
        }
        g_cli_fe_decided  = 1;
        g_cli_szs_printed = 1;
        g_alarm_exit_code = 0;
        g_alarm_szs[0] = '\0';   // the status is out: an alarm adds none
      }
      // --proof with a stashed front-end refutation (clausal pre-pass
      // win or a $false at clausification): print the conversion-
      // lineage proof and skip the engine -- the honest solution
      // output for these routes.
      if (g_cli_want_proof && g_cli_prepass_proof != NULL) {
        if (szs)
          printf("%% SZS status %s for %s\n",
                 g_cli_fof_conjecture ? "Theorem" : "Unsatisfiable", file);
        cli_proof_write(stdout, g_cli_prepass_proof, strlen(g_cli_prepass_proof));
        fflush(stdout);
        return 0;
      }
      // fof-split case analysis (near-Horn over ground or-clauses):
      // one branch child per positive-literal choice, sequential,
      // every branch must refute.  The orchestrator owns the rest of
      // the wall and the process exits with its verdict.
      cli_fe_check("the Horn release");
      if (g_fof_split_pending && !g_cli_fe_decided)
        return fof_split_orchestrate(argv[0], file, preset, max_steps,
                                     szs, g_cli_want_proof);
      // Function-definition unfolding (Vampire definition_unfolding),
      // opt-in: BEFORE the equals/curry/SInE transforms and the
      // canonical presentation, so selection and canon see the
      // unfolded theory.  =2 gives up when nothing unfolded (the
      // schedule's exit-fast slice discipline).
      {
        const char *du = getenv("THVM_ATP_DEF_UNFOLD");
        if (du != NULL && (du[0] == '1' || du[0] == '2')) {
          tp_def_unfold(&tp);
          if (du[0] == '2' && g_cli_defunfold_n == 0 && !g_cli_fe_decided)
            tp_giveup("def-unfold slice: no function definition");
          n_neg = 0;
          for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
        }
      }
      // tgt=4 axiom-naming lane exit-fast (--tgt-exit): the lane's
      // distinctive content is Twee --flatten's ground-tower
      // shattering (name every ground compound axiom subterm, e.g.
      // GRP689-1's op_d^9 = unit order condition); without such a
      // subterm the transform degenerates to goal-side naming, which
      // the flatten/flatten-bare lanes already carry -- give the
      // schedule slot back.
      {
        const char *tx = getenv("THVM_ATP_TGT_EXIT");
        if (tx != NULL && tx[0] != '\0' && tx[0] != '0'
            && !tp_has_ax_ground_compound(&tp) && !g_cli_fe_decided)
          tp_giveup("tgt-ax slice: no axiom-side ground compound subterm");
      }
      // Existential (non-ground) goals, opt-in: encode them
      // equationally the way Twee's addNarrowing does, BEFORE SInE
      // (the encoding's clauses carry the conjecture role and seed
      // the selection).
      {
        const char *ee = getenv("THVM_ATP_EQUALS_GOAL");
        if (ee != NULL && ee[0] != '0') tp_equals_goal(&tp);
        cli_fe_check("the goal transforms");
        n_neg = 0;
        for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
      }
      // (Wide-functor currying runs BEFORE the Horn release above.)
      // Oversized problems (the CSR knowledge bases run to 8k clauses):
      // goal-directed SInE selection cuts them to the relevant slice
      // BEFORE the capacity checks.  THVM_ATP_SINE=0 restores the old
      // clean give-up.
      {
        const char *se = getenv("THVM_ATP_SINE");
        u8 sine_on = (se == NULL || se[0] != '0');
        if (sine_on && (tp.n_cls - n_neg) > (int)PR_MAX_EQNS) {
          tp_sine_select(&tp);
          cli_fe_check("axiom selection");
          n_neg = 0;
          for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
        }
      }
      // Never hand the .pr reader more than it can hold: an oversized
      // problem exits cleanly as GaveUp rather than aborting downstream
      // with no SZS line.  PR_MAX_* come from the included bench TU.
      if ((tp.n_cls - n_neg) > (int)PR_MAX_EQNS) tp_giveup("problem exceeds equation capacity");
      if (tp.n_syms          > (int)PR_MAX_SYMS) tp_giveup("problem exceeds symbol capacity");
      if (tp.n_vars          > (int)PR_MAX_VARS) tp_giveup("problem exceeds variable capacity");
      // The flatterm term reader holds at most 8 arguments per node; a
      // higher-arity functor (e.g. TOP052's tuple/16) is beyond the term
      // representation, so give up cleanly instead of aborting the reader.
      for (int i = 0; i < tp.n_syms; i++)
        if (tp.syms[i].arity > 8) tp_giveup("functor arity exceeds 8");
      // Canonical presentation (orientation + order + variable names):
      // AFTER every content transform and capacity gate, BEFORE .pr
      // emission, so the emitted .pr is invariant across the corpus
      // releases' permuted/flipped renderings of the same theory.
      // THVM_ATP_CANON_INPUT=0 disables.
      {
        const char *cn = getenv("THVM_ATP_CANON_INPUT");
        if (cn == NULL || cn[0] != '0') tp_canon_input(&tp);
        cli_fe_check("the canonical presentation");
      }
      // tgt conjecture-role export: the .pr format has no clause-role
      // field, and BOTH tgt ports need the conjecture cone -- Vampire
      // works from the WHOLE negated conjecture; on the '-10'
      // encodings the final goal equation is a bare `a != b` while
      // the conjecture content lives in its sibling
      // negated_conjecture units.
      //   * THVM_ATP_TGT (--tgt, the preprocessing port): export the
      //     conjecture-role EQUATION indices, in emitted order, via
      //     THVM_ATP_TGT_EQNS (consumed by pr_tgt_transform).
      //   * THVM_ATP_TGT_SEL (--tgt-sel, the CP-selection scale):
      //     export the signature symbols occurring in a
      //     conjecture-role clause (word-boundary match on the clause
      //     strings) via THVM_ATP_TGT_SYMS (pr-side name -> label
      //     mapping feeds thvm_atp_tgt_add_sym).
      {
        const char *tg = getenv("THVM_ATP_TGT");
        if (tg != NULL && tg[0] != '\0' && tg[0] != '0') {
          size_t ecap = (size_t)tp.n_cls * 8 + 2;
          char *ebuf = (char *)malloc(ecap);
          size_t elen = 0;
          ebuf[0] = '\0';
          int ei = 0;
          for (int c = 0; c < tp.n_cls; c++) {
            if (tp.cls[c].neg) continue;
            if (tp.cls[c].conj) {
              elen += (size_t)snprintf(ebuf + elen, ecap - elen, "%s%d",
                                       (elen > 0) ? " " : "", ei);
            }
            ei++;
          }
          if (elen > 0) setenv("THVM_ATP_TGT_EQNS", ebuf, 1);
          free(ebuf);
        }
      }
      {
        const char *tg = getenv("THVM_ATP_TGT_SEL");
        if (tg != NULL && tg[0] != '\0' && tg[0] != '0') {
          size_t cap = 1;
          for (int i = 0; i < tp.n_syms; i++)
            cap += strlen(tp.syms[i].name) + 1;
          char *buf = (char *)malloc(cap);
          size_t len = 0;
          int n_hit = 0;
          buf[0] = '\0';
          for (int i = 0; i < tp.n_syms; i++) {
            const char *nm = tp.syms[i].name;
            size_t nl = strlen(nm);
            int hit = 0;
            for (int c = 0; c < tp.n_cls && !hit; c++) {
              if (!tp.cls[c].conj) continue;
              for (int side = 0; side < 2 && !hit; side++) {
                const char *hay = side ? tp.cls[c].rhs : tp.cls[c].lhs;
                for (const char *p = strstr(hay, nm); p != NULL;
                     p = strstr(p + 1, nm)) {
                  int lb = (p == hay) ||
                           !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
                  int rb = !(isalnum((unsigned char)p[nl]) || p[nl] == '_');
                  if (lb && rb) { hit = 1; break; }
                }
              }
            }
            if (hit) {
              n_hit++;
              if (len > 0) buf[len++] = ' ';
              memcpy(buf + len, nm, nl);
              len += nl;
              buf[len] = '\0';
            }
          }
          if (len > 0) setenv("THVM_ATP_TGT_SYMS", buf, 1);
          free(buf);
          // tgt-sel lane exit-fast (--tgt-sel-exit): the lane's content
          // is the conjecture-symbol discrimination -- non-goal symbols
          // (GRP66x: ld/rd/unit vs the goal's pure mult/skolem cone)
          // taxed by --tgt-nongoal in the CP eval.  When the conjecture
          // cone covers the whole signature (or no conjecture-role
          // clause survives the transforms) the multiplier is uniform
          // and the slice duplicates 'bare' -- give the schedule slot
          // back, like the tgt-ax/unfold exit-fast discipline.
          {
            const char *tx = getenv("THVM_ATP_TGT_SEL_EXIT");
            if (tx != NULL && tx[0] != '\0' && tx[0] != '0'
                && (n_hit == 0 || n_hit >= tp.n_syms) && !g_cli_fe_decided)
              tp_giveup("tgt-sel slice: conjecture symbols cover the signature");
          }
        }
      }
      // --proof: freeze the final unit -> conversion-lineage map from
      // the exact strings the .pr will carry (post-transform, post-
      // canon), so engine-trace axiom leaves resolve their source.
      if (g_prov_on) prov_build_leaf_table(&tp);
      const char *base = strrchr(file, '/');
      base = (base != NULL) ? base + 1 : file;
      char pname[256];
      snprintf(pname, sizeof pname, "%.*s",
               (int)strcspn(base, "."), base);
      char *pr_text = tp_emit_pr(&tp, pname);
      if (keep_pr != 0) snprintf(tmp_pr, sizeof tmp_pr, "%s.pr", file);
      else {
        snprintf(tmp_pr, sizeof tmp_pr, "/tmp/thvm_atp_%d.pr", (int)getpid());
      }
      FILE *fp = fopen(tmp_pr, "w");
      if (fp == NULL) { fprintf(stderr, "FindProof: cannot write %s\n", tmp_pr); return 2; }
      fputs(pr_text, fp);
      fclose(fp);
      free(pr_text);
      if (keep_pr == 2) { printf("%s\n", tmp_pr); free(tpp); return 0; }
#undef tp
      free(tpp);
      run_file = tmp_pr;
    } else {
      // Not TPTP (no cnf( / fof( / include( record).  The input must then
      // be a genuine Waldmeister .pr file; if it is not (an empty file, a
      // comment-only file, a tff(/thf( problem, junk), it must NOT reach
      // the bench -- the bench would run its BUILT-IN DEFAULT problem and
      // emit a spurious proof (the general form of the include-only
      // soundness bug).  Give up cleanly instead.
      // Comment-aware (tp_raw_marker): a header comment containing the
      // word AXIOMS (SEV326^5's "GVB-MB-AXIOMS") must not pass as a
      // .pr section marker.
      if (!tp_raw_marker(raw, "ORDERING") && !tp_raw_marker(raw, "SIGNATURE")
          && !tp_raw_marker(raw, "CONCLUSION") && !tp_raw_marker(raw, "AXIOMS")) {
        g_cli_szs_wanted = szs;
        g_cli_probpath   = file;
        free(raw);
        tp_giveup("input is neither TPTP (cnf/fof/include) nor a Waldmeister .pr");
      }
    }
    free(raw);
  }

  // --equals-goal=2 (the schedule's equals slice): exit cleanly when
  // the encoding did not apply (ground goal, over-bound goal, or
  // non-TPTP input), freeing the pool slot immediately instead of
  // duplicating the WM lane for a whole wall.
  {
    const char *ee = getenv("THVM_ATP_EQUALS_GOAL");
    if (ee != NULL && atoi(ee) >= 2 && !g_cli_equals_applied &&
        !g_cli_fe_decided)
      tp_giveup("equals-goal slice: no existential goal to encode");
  }
  // --narrow-queue=2 (the schedule's narrowq slice): same exit-fast
  // discipline -- the critical-goal queue lane only differs from the
  // WM lane on an existential (non-ground) conjecture.
  {
    const char *nqe = getenv("THVM_ATP_NARROW_QUEUE");
    if (nqe != NULL && atoi(nqe) >= 2 && !g_cli_goal_existential &&
        !g_cli_fe_decided)
      tp_giveup("narrow-queue slice: goal is ground");
  }

  // The front end spent the wall: nothing is left for the engine, which
  // measures its wall from its own start (the old 1 s floor plus the
  // engine's grace ran a slice up to half a minute past its wall).  A
  // decided run's status is out already.
  if ((g_horn_cd || g_cli_fof_applied || g_cli_fe_decided) &&
      atof(wall) - (horn_now() - main_t0) < 0.5) {
    if (tmp_pr[0] != '\0' && !keep_pr) unlink(tmp_pr);
    if (g_cli_fe_decided) {
      fflush(stdout);
      return 0;
    }
    g_tp_giveup_hook = NULL;
    tp_giveup("the front end spent the wall");
  }

  g_cli_want_szs = szs;
  if (g_cli_want_proof) g_atp_bench_pre_free_hook = cli_proof_hook;
  // Competition-clean stdout: reroute the bench TU's diagnostic chatter
  // to stderr; the SZS lines go to the preserved real stdout.
  int real_stdout = -1;
  if (szs || g_cli_want_proof) {
    real_stdout = dup(1);
    g_cli_out = fdopen(dup(real_stdout), "w");
    fflush(stdout);
    dup2(2, 1);
  }

  // The whole FOF front-end -- clausify, the clausal pre-pass (up to
  // 0.6 of the wall) and the Horn release (up to 0.4) -- runs BEFORE the
  // engine even starts, and the CD lane chains on top of that.  The
  // engine measures its own wall_cap from ITS start, so handing it the
  // full slice wall lets it run to engine_start + wall, PAST the slice
  // deadline (the 240s FOF NoOutput class: GEO288 engine-started at 15s
  // of a 20s wall, ran the full 20s more, hard-killed with no SZS line).
  // Hand it only what remains.  The UEQ path (no FOF front-end) keeps the
  // full wall: its clausify is instant, so the value is unchanged and its
  // proofs stay byte-identical.
  char wall_left[32];
  if (g_horn_cd || g_cli_fof_applied || g_cli_fe_decided) {
    double left = atof(wall) - (horn_now() - main_t0);
    if (left < 0.5) left = 0.5;
    // A decided front-end leaves the engine only the proof to rebuild:
    // bounded, so a lane that cannot rebuild it frees its slot as the
    // no-proof run's shortcut does (another lane's rebuild wins).
    if (g_cli_fe_decided && left > 30.0) left = 30.0;
    snprintf(wall_left, sizeof wall_left, "%.1f", left);
    wall = wall_left;
  }

  // Hard wall: the alarm covers the whole engine run INCLUDING proof
  // emission (cli_proof_hook runs inside atp_bench_main).  A schedule
  // slice stays silent on fire (the wrapper owns the single status);
  // a direct run emits the Timeout/ResourceOut line itself.
  {
    int slice = getenv("FINDPROOF_SLICE") != NULL;
    int outfd = (real_stdout >= 0) ? real_stdout : 1;
    char line[1100];
    line[0] = '\0';
    if (szs && !slice && !g_cli_fe_decided)
      snprintf(line, sizeof line, "%% SZS status %s for %s\n",
               g_cli_tptp_mode ? "Timeout" : "ResourceOut", file);
    cli_arm_wall_alarm(atof(wall), 30, outfd, line[0] ? line : NULL);
    // Proof emission must finish inside the wall: a schedule replays a
    // slice's output only once it exits, and the competition kills at
    // the wall.  Past this point the emission's replays and deep scans
    // give up (records keep their cheap bridges), so a slow emission
    // costs proof quality, never the status.
    {
      double ew = atof(wall) - 1.0;
      if (ew < 1.0) ew = 1.0;
      g_cli_emit_deadline_us = atp_now_us() + (u64)(ew * 1e6);
    }
  }

  char *bench_argv[5];
  bench_argv[0] = (char *)"FindProof";
  bench_argv[1] = (char *)run_file;
  bench_argv[2] = (char *)max_steps;
  bench_argv[3] = (char *)wall;
  bench_argv[4] = NULL;
  int rc = atp_bench_main(4, bench_argv);
  alarm(0);

  if (real_stdout >= 0) {
    fflush(stdout);
    dup2(real_stdout, 1);   // restore the real stdout for the SZS line
    close(real_stdout);
  }
  // Inside a schedule slice, only a SUCCESS prints its SZS line -- the
  // schedule wrapper owns the single final status otherwise.
  if (szs && getenv("FINDPROOF_SLICE") != NULL
      && g_atp_bench_status != (int)ATP_PROVED) szs = 0;
  if (g_cli_szs_printed) szs = 0;
  if (szs) {
    const char *tag;
    if (g_cli_tptp_mode) {
      // FOF ontology: a refuted negated conjecture is a Theorem; the
      // clause ontology's Unsatisfiable covers axioms-only input.
      tag = (g_atp_bench_status == (int)ATP_PROVED)      ?
                (g_cli_fof_conjecture ? "Theorem" : "Unsatisfiable") :
            (g_atp_bench_status == (int)ATP_QUEUE_EMPTY) ?
                (cli_strategy_complete() ? "Satisfiable" : "GaveUp") :
            (g_atp_bench_status == (int)ATP_TIMEOUT)     ? "ResourceOut" :
            // Memory-guard aborts are ResourceOut in the SZS ontology
            // (Timeout is time only; the guard pops fire mid-wall).
            (g_atp_bench_status == (int)ATP_ABORTED)     ? "ResourceOut" :
                                                           "Timeout";
    } else {
      tag = (g_atp_bench_status == (int)ATP_PROVED)      ? "Theorem" :
            (g_atp_bench_status == (int)ATP_QUEUE_EMPTY) ? "GaveUp" :
                                                           "ResourceOut";
    }
    printf("%% SZS status %s for %s\n", tag, file);
  }
  if (tmp_pr[0] != '\0' && !keep_pr) unlink(tmp_pr);
  if (g_atp_bench_status == (int)ATP_PROVED || g_cli_fe_decided) return 0;
  return (rc != 0) ? rc : 1;
}
