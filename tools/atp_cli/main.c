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
  // FULL AC-completion stays OUT of the competition schedule -- for
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
  // FULL AC-COMPLETION: auto-detect AC operators (add/meet/join/mult from
  // their commutativity+associativity axioms) and run AC-matching +
  // Stickel AC-unification.  The WM preset only uses AC to FILTER CPs
  // (perm-subsume); leaving the engine mask set activates the full
  // AC-saturation path, which on the ring/lattice families finds the
  // SHORT proof the plain KB completion drowns in commutativity-CP
  // explosion trying to reach (RNG021-6/021-7/025-5/027-7 prove in 1-3s
  // where every non-AC slice Times Out -- competitors solve these in
  // 0.3s BECAUSE they reason modulo AC).  The mechanism existed
  // (src/atp/ac.c, built into the CLI) but was inert: nothing set the
  // engine ac_mask.  No-AC problems detect an empty mask and fall
  // straight through to plain completion, so this is a near-free lane.
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
static const int N_SCHED = (int)(sizeof(SCHEDULE) / sizeof(SCHEDULE[0]));

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
// The handler killpg's every launched child group before exiting.  pids are
// stored by slice index; a reaped slot is set back to -1.
#define SCHED_PID_MAX 32
static pid_t g_sched_pids[SCHED_PID_MAX];
static volatile sig_atomic_t g_sched_active = 0;
static void sched_sig_teardown(int sig) {
  if (g_sched_active) {
    for (int i = 0; i < SCHED_PID_MAX; i++)
      if (g_sched_pids[i] > 0) {
        killpg(g_sched_pids[i], SIGKILL);
        kill(g_sched_pids[i], SIGKILL);
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
static void cli_alarm_fire(int sig) {
  (void)sig;
  if (g_alarm_fd >= 0 && g_alarm_szs[0] != '\0') {
    size_t n = 0;
    while (g_alarm_szs[n] != '\0') n++;
    ssize_t w = write(g_alarm_fd, g_alarm_szs, n);
    (void)w;
  }
  if (g_sched_active) sched_sig_teardown(SIGALRM);
  _exit(6);
}
static void cli_arm_wall_alarm(double wall, int grace, int fd,
                               const char *szs_line) {
  if (szs_line != NULL)
    snprintf(g_alarm_szs, sizeof g_alarm_szs, "%s", szs_line);
  else
    g_alarm_szs[0] = '\0';
  g_alarm_fd = fd;
  struct sigaction sa;
  sa.sa_handler = cli_alarm_fire;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGALRM, &sa, NULL);
  alarm((unsigned)(wall + grace + 0.999));
}

// CONCURRENT portfolio: run the slices in a bounded pool (SCHED_MAX_PAR at
// a time), EACH with the FULL wall -- not wall/N as the old sequential
// scheme did.  Sequential slicing starved every strategy (a problem
// needing >frac*wall of one config failed even though that config solves
// it within the full wall -- e.g. LCL113-10 needs 23s of the WM preset but
// the 0.22 slice gave it 6.6s).  First slice to exit 0 (proof found) wins;
// the rest are killed.  Each child writes its stdout to a temp file; only
// the winner's is replayed, so SZS/proof output never interleaves.
static int run_schedule(const char *self, const char *file, double wall,
                        int szs, int want_proof) {
  // Install the teardown handler + register the (initially empty) pid slots
  // so an external timeout can't orphan the slice subtrees.
  for (int i = 0; i < SCHED_PID_MAX; i++) g_sched_pids[i] = -1;
  struct sigaction sa, old_term, old_int;
  sa.sa_handler = sched_sig_teardown;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, &old_term);
  sigaction(SIGINT,  &sa, &old_int);
  g_sched_active = 1;
  // Belt-and-braces: if every slice wedges past its own hard wall, the
  // parent still emits one status line and reaps the subtrees.
  {
    char line[1100];
    line[0] = '\0';
    if (szs) snprintf(line, sizeof line, "%% SZS status Timeout for %s\n", file);
    cli_arm_wall_alarm(wall, 60, 1, line[0] ? line : NULL);
  }
  // Bound each child's RSS unless the caller set a tighter cap already.
  if (getenv("THVM_ATP_RSS_ABORT_MB") == NULL) {
    char capbuf[16];
    snprintf(capbuf, sizeof capbuf, "%d", SCHED_SLICE_RSS_MB);
    setenv("THVM_ATP_RSS_ABORT_MB", capbuf, 1);
  }
  pid_t pids[N_SCHED];
  char  outs[N_SCHED][64];
  int   done[N_SCHED];
  for (int i = 0; i < N_SCHED; i++) { pids[i] = -1; done[i] = 0; outs[i][0] = 0; }
  int launched = 0, next = 0, winner = -1, n_inerr = 0;
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
      if (v >= 1 && v <= N_SCHED) max_par = v;
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
  // Launch up to max_par, then refill as children exit.
  while (winner < 0 && (next < N_SCHED || launched > 0)) {
    while (winner < 0 && launched < max_par && next < N_SCHED) {
      double rem = budget - (horn_now() - sched_t0);
      if (rem < 2.0) break;   // no wall budget left: stop launching slices
      int i = next++;
      double slice_wall = rem;
      snprintf(outs[i], sizeof outs[i], "/tmp/thvm_sched_%d_%d.out",
               (int)getpid(), i);
      if (g_cli_verbose)
        fprintf(stderr, "%% FindProof schedule slice %s (%.0fs, remaining wall)\n",
                SCHEDULE[i].name, slice_wall);
      pid_t pid = fork();
      if (pid == 0) {
        // Child: own process group so the parent can kill the WHOLE
        // subtree (this child + the shell system() forks + its FindProof
        // grandchild) with one killpg -- otherwise killing the child
        // blocked in system() would orphan the grandchild engine, which
        // keeps running to its wall and could overlap the next problem.
        setpgid(0, 0);
        // Redirect stdout to the slice temp file, run the slice.
        int fd = open(outs[i], O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) { dup2(fd, 1); close(fd); }
        // Silence the slice's diagnostics (bench PRSYM/stats/progress all
        // go to stderr): the grandchild engine inherits this fd 2, so the
        // whole portfolio stays quiet.  The winner's proof is on stdout
        // (the temp file above), so nothing checkable is lost.  --verbose
        // keeps the stream for debugging.
        if (!g_cli_verbose) {
          int dn = open("/dev/null", O_WRONLY);
          if (dn >= 0) { dup2(dn, 2); close(dn); }
        }
        char cmd[4096];
        snprintf(cmd, sizeof cmd, "FINDPROOF_SLICE=1 '%s' %s --wall=%.0f%s%s '%s'",
                 self, SCHEDULE[i].flags, slice_wall,
                 szs ? " --szs" : "", want_proof ? " --proof" : "", file);
        // Forward the slice's real exit code: 2 (input/environment error,
        // e.g. an unresolvable include) must stay distinguishable from 1
        // (searched and gave up) so the parent doesn't mask a broken
        // input as an honest GaveUp.
        int st = system(cmd);
        _exit((st != -1 && WIFEXITED(st)) ? WEXITSTATUS(st) : 1);
      }
      // Set the child's pgid parent-side too (race-free: whichever of
      // parent/child wins, the group id equals the child pid).
      if (pid > 0) setpgid(pid, pid);
      pids[i] = pid;
      if (i < SCHED_PID_MAX) g_sched_pids[i] = pid;   // for the signal handler
      launched++;
    }
    if (launched == 0) break;
    int status = 0;
    pid_t w = wait(&status);
    if (w <= 0) break;
    for (int i = 0; i < N_SCHED; i++) {
      if (pids[i] == w) {
        done[i] = 1; launched--;
        if (i < SCHED_PID_MAX) g_sched_pids[i] = -1;   // reaped: clear
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) winner = i;
        if (WIFEXITED(status) && WEXITSTATUS(status) == 2) n_inerr++;
        break;
      }
    }
  }
  // Kill + reap any still-running slices (the winner made them redundant).
  // killpg on the child's group takes down the shell + FindProof grandchild
  // too, so no engine is left orphaned to run to its wall.
  for (int i = 0; i < N_SCHED; i++)
    if (pids[i] > 0 && !done[i]) { killpg(pids[i], SIGKILL); kill(pids[i], SIGKILL); }
  for (int i = 0; i < N_SCHED; i++)
    if (pids[i] > 0 && !done[i]) { int st; waitpid(pids[i], &st, 0);
      if (i < SCHED_PID_MAX) g_sched_pids[i] = -1; }
  // Teardown handler no longer needed: restore the previous dispositions.
  alarm(0);
  g_sched_active = 0;
  sigaction(SIGTERM, &old_term, NULL);
  sigaction(SIGINT,  &old_int,  NULL);
  int rc = 1;
  if (winner >= 0) {
    // Replay only the winning slice's captured output.
    FILE *fp = fopen(outs[winner], "r");
    if (fp != NULL) {
      char buf[4096]; size_t n;
      while ((n = fread(buf, 1, sizeof buf, fp)) > 0) fwrite(buf, 1, n, stdout);
      fclose(fp);
    }
    rc = 0;
  } else if (n_inerr == N_SCHED) {
    // Every slice failed before searching (the input error precedes the
    // per-slice transforms, so it hits all of them identically): this is
    // a broken input or environment, not a give-up.
    fprintf(stderr, "FindProof: all schedule slices reported an input error\n");
    if (szs) printf("%% SZS status Error for %s\n", file);
    rc = 2;
  } else if (szs) {
    printf("%% SZS status GaveUp for %s\n", file);
  }
  for (int i = 0; i < N_SCHED; i++) if (outs[i][0]) unlink(outs[i]);
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
        fwrite(bufp, 1, plen, stdout);
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
    if (nlen == 3 && strncmp(name, "szs", 3) == 0) { szs = 1; continue; }
    if (nlen == 8 && strncmp(name, "schedule", 8) == 0) { want_schedule = 1; continue; }
    if (nlen == 5 && strncmp(name, "check", 5) == 0) { want_check = 1; continue; }
    if (nlen == 7 && strncmp(name, "verbose", 7) == 0) { g_cli_verbose = 1; continue; }
    if (nlen == 5 && strncmp(name, "proof", 5) == 0) {
      g_cli_want_proof = 1;
      // The proof extractor cites each goal-closing rule through its
      // completion-trace lineage; the default 128K trace soft cap drops
      // the derivations of rules formed late in a deep completion, so
      // those citations resolve to nothing and the checker cannot bridge
      // the step.  Raise the cap for --proof runs so every proof-critical
      // lineage survives.  Trajectory-neutral: the CP formation stream is
      // BYTE-IDENTICAL with the cap raised (INSERT_TRACE on LAT171-1, a
      // WM/orphan-murder problem that hits the default cap) because CP
      // selection reads the uid-stamp layer, not r_trace.  setenv with
      // overwrite=0 keeps any explicit user THVM_ATP_TRACE_MAX.  Applies
      // in the schedule too: run_schedule re-execs each child with
      // --proof, so the child sets this before its engine init.
      setenv("THVM_ATP_TRACE_MAX", "16777216", 0);
      // The packed trace corrupts emission-replay NORM_STEP chains in the
      // goal-cone bracket (var=var pairs past REWRITE_MAX_VAR -- the
      // span-sourced byte store/load in atp_trace_eq_store_ft; open bug).
      // Proofs must be citable, so --proof runs use the unpacked trace:
      // byte-correct emission at a bounded RSS cost during search.
      // overwrite=0 keeps an explicit user THVM_ATP_NO_TRACE_PACK=0.
      setenv("THVM_ATP_NO_TRACE_PACK", "1", 0);
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
  if (file == NULL) { usage(stderr); return 2; }
  // Engine wall, set BEFORE any front-end work: the FOF clausal
  // pre-pass budgets itself as a fraction of the remaining wall.
  g_cli_wall_sec = (atof(wall) > 0.0) ? atof(wall) : 120.0;

  if (want_check) return proof_check_main(file, szs);

  if (want_schedule) {
    return run_schedule(argv[0], file, atof(wall), szs, g_cli_want_proof);
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
      char *pre = tp_preprocess(file, raw);
      pre = tp_unquote_names(pre);
      tp_shorten_names(pre);
      TpProblem *tpp = (TpProblem *)calloc(1, sizeof(TpProblem));
      if (tpp == NULL) { fprintf(stderr, "FindProof: OOM\n"); return 2; }
#define tp (*tpp)
      // FOF records (or non-unit / non-equational CNF) go through the
      // clausal front-end; pure UEQ input keeps the byte-identical
      // tp_parse path.
      if (tp_needs_clausal(pre)) tp_fof_load(&tp, pre);
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
      // Encoded-Horn (ifeq) problems: decode the dispatcher encoding
      // and forward-chain released units BEFORE SInE, so the releases
      // participate in axiom selection.  THVM_ATP_HORN=0 disables.
      {
        const char *he = getenv("THVM_ATP_HORN");
        if (he == NULL || he[0] != '0') tp_horn_release(&tp);
      }
      // Goal reached by the chaining itself: some released unit (a
      // logical consequence, by the modus-ponens-only invariant)
      // unifies with a negative clause, i.e. the axioms entail an
      // instance of the negated equation -- a refutation with no
      // engine run.  Decisive for problems the engine cannot even
      // load (PUZ039's state/12 exceeds the term reader's arity cap).
      // --proof runs stay on the engine path for the CNFRefutation.
      if ((g_horn_goal_reached || g_cli_fof_trivial || g_cli_fof_satref) &&
          szs && !g_cli_want_proof) {
        printf("%% SZS status %s for %s\n",
               g_cli_fof_conjecture ? "Theorem" : "Unsatisfiable", file);
        fflush(stdout);
        return 0;
      }
      // --proof with a stashed front-end refutation (clausal pre-pass
      // win or a $false at clausification): print the conversion-
      // lineage proof and skip the engine -- the honest solution
      // output for these routes.
      if (g_cli_want_proof && g_cli_prepass_proof != NULL) {
        if (szs)
          printf("%% SZS status %s for %s\n",
                 g_cli_fof_conjecture ? "Theorem" : "Unsatisfiable", file);
        fputs(g_cli_prepass_proof, stdout);
        fflush(stdout);
        return 0;
      }
      // fof-split case analysis (near-Horn over ground or-clauses):
      // one branch child per positive-literal choice, sequential,
      // every branch must refute.  The orchestrator owns the rest of
      // the wall and the process exits with its verdict.
      if (g_fof_split_pending)
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
          if (du[0] == '2' && g_cli_defunfold_n == 0)
            tp_giveup("def-unfold slice: no function definition");
          n_neg = 0;
          for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
        }
      }
      // Existential (non-ground) goals, opt-in: encode them
      // equationally the way Twee's addNarrowing does, BEFORE SInE
      // (the encoding's clauses carry the conjecture role and seed
      // the selection).
      {
        const char *ee = getenv("THVM_ATP_EQUALS_GOAL");
        if (ee != NULL && ee[0] != '0') tp_equals_goal(&tp);
        n_neg = 0;
        for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
      }
      // Wide-functor currying: decompose any n-ary functor (n > 8) into
      // nested binary ap2 applications so the arity-8 term reader can
      // load it (TOP052's tuple/140).  Sound bijective re-encoding; a
      // no-op unless a wide functor is present.  THVM_ATP_CURRY=0 keeps
      // the old clean give-up.
      {
        const char *ce = getenv("THVM_ATP_CURRY");
        if (ce == NULL || ce[0] != '0') tp_curry_wide(&tp);
      }
      // Oversized problems (the CSR knowledge bases run to 8k clauses):
      // goal-directed SInE selection cuts them to the relevant slice
      // BEFORE the capacity checks.  THVM_ATP_SINE=0 restores the old
      // clean give-up.
      {
        const char *se = getenv("THVM_ATP_SINE");
        u8 sine_on = (se == NULL || se[0] != '0');
        if (sine_on && (tp.n_cls - n_neg) > (int)PR_MAX_EQNS) {
          tp_sine_select(&tp);
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
              if (len > 0) buf[len++] = ' ';
              memcpy(buf + len, nm, nl);
              len += nl;
              buf[len] = '\0';
            }
          }
          if (len > 0) setenv("THVM_ATP_TGT_SYMS", buf, 1);
          free(buf);
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
    if (ee != NULL && atoi(ee) >= 2 && !g_cli_equals_applied)
      tp_giveup("equals-goal slice: no existential goal to encode");
  }
  // --narrow-queue=2 (the schedule's narrowq slice): same exit-fast
  // discipline -- the critical-goal queue lane only differs from the
  // WM lane on an existential (non-ground) conjecture.
  {
    const char *nqe = getenv("THVM_ATP_NARROW_QUEUE");
    if (nqe != NULL && atoi(nqe) >= 2 && !g_cli_goal_existential)
      tp_giveup("narrow-queue slice: goal is ground");
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
  if (g_horn_cd || g_cli_fof_applied) {
    double left = atof(wall) - (horn_now() - main_t0);
    if (left < 1.0) left = 1.0;
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
    if (szs && !slice)
      snprintf(line, sizeof line, "%% SZS status %s for %s\n",
               g_cli_tptp_mode ? "Timeout" : "ResourceOut", file);
    cli_arm_wall_alarm(atof(wall), 30, outfd, line[0] ? line : NULL);
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
  if (g_atp_bench_status == (int)ATP_PROVED) return 0;
  return (rc != 0) ? rc : 1;
}
