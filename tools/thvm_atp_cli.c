// FindProof: thvm's ATP completion engine as a standalone CLI --
// the CASC UEQ entry.  (Former name: thvm-atp; bin/thvm-atp remains an
// alias.)
//
// Exposes the same knob surface as TFindProof's Method option, mapped
// onto the engine's env-var toggles, and runs the exact code path of
// tests/test_atp_wolfram_bench.c (this file includes that translation
// unit with its main() renamed, so a CLI run is byte-identical to a
// bench run given the same knobs).
//
//   thvm-atp [--preset=waldmeister|waldmeister-lazy|none]
//            [--<knob>[=<value>] ...]
//            [--max-steps=N] [--wall=SEC] [--szs] [--proof]
//            problem.pr | problem.p
//
// CASC compatibility: a TPTP CNF UEQ problem (unit equalities, one or
// more negative units) is converted in-process to Waldmeister .pr form
// -- include('Axioms/X.ax') directives resolve against the problem's
// directory, $TPTP, and the CWD -- and the SZS result line uses the
// CNF ontology (Unsatisfiable / Satisfiable / GaveUp / ResourceOut /
// Timeout).  --proof prints an SZS CNFRefutation block: the input
// clauses, the derivation of every rule the closing rewrite chain
// cites (walked from the completion trace), and the goal chain itself.
//
// A multi-goal CONCLUSION block (or several negative TPTP units) is
// proved as a conjunction: every goal must join.  For several negative
// units that is sound for Unsatisfiable but stricter than refutation
// requires (any single one suffices); CASC UEQ problems carry exactly
// one.
//
// Encoded-Horn preprocessing: problems that encode Horn logic through
// a dispatcher axiom ifeqN(X,X,Y,Z) = Y (TPTP's -10 UEQ class) are
// decoded and a budgeted forward-chaining release loop adds the
// modus-ponens consequences as extra unit equations before the engine
// runs -- see tp_horn_release.  --horn=0 disables.
//
// FOF front-end: fof() records (and cnf() records that are not unit
// equations) are clausified (src/fol NNF + Skolemization +
// distribution; Tseitin on distribution blow-up) and equationalized:
// the Horn fragment with exactly that ifeq scheme, non-Horn clauses
// through a sound-but-incomplete or/eq reification -- see tp_fof_load.
// Success on a conjecture reports SZS Theorem; failure is always
// GaveUp (never CounterSatisfiable).  --fof=0 disables; --fof-or=0
// restores the Horn-only give-up on non-Horn input.

#define main atp_bench_main
#include "../tests/test_atp_wolfram_bench.c"
#undef main

// Concurrent-portfolio process management (run_schedule).
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

// --check mode: validate a first-order TSTP proof (the ProoVer task).
// Reuses the src/fol saturation engine already linked into this binary.
#include "proof_check.c"

// ===================================================================
// knob table
// ===================================================================

// One CLI knob: kebab-case flag (named after the TFindProof Method
// suboption), the engine env var it drives, whether it takes a value
// (0 = boolean 0/1 with default 1 when bare), and the Method name.
typedef struct {
  const char *flag;
  const char *env;
  int         has_value;
  const char *method;
} AtpKnob;

static const AtpKnob KNOBS[] = {
  {"cp-weight",              "THVM_ATP_CP_WEIGHT",       1, "CriticalPairWeight"},
  // Twee CP-score coefficients (--cp-weight=8 only; Twee.CP.score,
  // src/Twee/CP.hs: score = lhsweight*max(m,n) + rhsweight*min(m,n)
  // + depthweight*depth).  Twee CLI flags --lhs-weight/--rhs-weight/
  // --depth-weight; defaults 4/1/2 = Twee CP.defaultConfig, which the
  // engine reproduces byte-identically when the knobs are unset.
  {"cp-lhs-weight",          "THVM_ATP_TWEE_LHS_W",      1, "(Twee lhsWeight: coefficient on the LARGER CP side, default 4)"},
  {"cp-rhs-weight",          "THVM_ATP_TWEE_RHS_W",      1, "(Twee rhsWeight: coefficient on the SMALLER CP side, default 1)"},
  {"cp-depth-weight",        "THVM_ATP_TWEE_DEPTH_W",    1, "(Twee depthWeight: coefficient on CP formation depth, default 2; 60 = depth-dominant shallow selection)"},
  {"max-weight",             "THVM_ATP_MAXW",            1, "MaxWeight"},
  {"max-term-size",          "THVM_ATP_MAX_TERM_SIZE",   1, "(Twee --max-term-size: per-side CP node cap; drop CP if max(|lhs|,|rhs|) > N; 0 = off)"},
  {"auto-max-weight",        "THVM_ATP_AUTO_MAXW",       1, "AutoMaxWeight"},
  {"max-cp-queue",           "THVM_ATP_MAX_CP_QUEUE",    1, "MaxCpQueue"},
  {"max-weight-slope",       "THVM_ATP_MAXW_SLOPE",      1, "AutoMaxWeight slope"},
  {"selection-ratio",        "THVM_ATP_SELECTION_RATIO", 1, "SelectionRatio"},
  {"goal-interleave",        "THVM_ATP_GOAL_INTERLEAVE", 1, "GoalInterleave"},
  {"ground-join",            "THVM_ATP_GROUND_JOIN",     0, "GroundJoin"},
  {"lazy-ground-join",       "THVM_ATP_LAZY_GJ",         0, "LazyGroundJoin"},
  {"pop-gj",                 "THVM_ATP_POP_GJ",          1, "(Twee Join.hs pop-time ground-joinability case-split; 0 off)"},
  {"pop-gj-limit",           "THVM_ATP_POP_GJ_LIMIT",    1, "(Twee --ground-joining-limit: max case-ordering models examined per CP; 0 = unlimited)"},
  {"connectedness",          "THVM_ATP_CONNECT",         0, "Connectedness"},
  {"rhs-interreduce",        "THVM_ATP_RHS_IR",          0, "RHSInterreduce"},
  {"unfailing-cp",           "THVM_ATP_UNFAILING",       0, "UnfailingCP"},
  {"cp-set-interreduce",     "THVM_ATP_CP_SET_IR",       0, "CPSetInterreduce"},
  {"cp-set-ir-period",       "THVM_ATP_CP_SET_IR_PERIOD",1, "CPSetInterreduce period"},
  {"cp-set-ir-budget",       "THVM_ATP_CP_SET_IR_BUDGET",1, "CPSetInterreduce budget % (Twee task discipline)"},
  {"lazy-normalize",         "THVM_ATP_LAZY_NORM",       0, "LazyNormalize"},
  {"demote-on-lhs-simplify", "THVM_ATP_WM_DEMOTE",       0, "DemoteOnLhsSimplify"},
  {"orphan-murder",          "THVM_ATP_ORPHAN",          0, "OrphanMurder"},
  {"eager-sweep",            "THVM_ATP_EAGER_SWEEP",     0, "(eager interreduce-time orphan sweep; 0 keeps collapsed-parent CPs queued -- the GRP678 tgt=3 content killer)"},
  {"pop-subsume",            "THVM_ATP_POP_SUBSUME",     0, "PopSubsume"},
  {"eset-subsume",           "THVM_ATP_ESET_SUBSUME",    0, "ESetSubsume"},
  {"queue-subsume",          "THVM_ATP_QUEUE_SUBSUME",   0, "QueueSubsume"},
  {"flat-subsume",           "THVM_ATP_FLAT_SUBSUME",    0, "FlatSubsume"},
  {"comm-subsume",           "THVM_ATP_COMM_SUBSUME",    0, "CommSubsume"},
  {"emission-order",         "THVM_ATP_EMISSION_ORDER",  0, "EmissionOrder"},
  {"intake-order",           "THVM_ATP_INTAKE_ORDER",    0, "IntakeOrder"},
  {"mixmost-nf",             "THVM_ATP_MIXMOST_NF",      0, "MixmostNF"},
  {"formation-fifo",         "THVM_ATP_FORMATION_FIFO",  0, "FormationFifo"},
  {"fifo-threshold",         "THVM_ATP_FIFO_THRESHOLD",  1, "FIFO interleave threshold"},
  {"eset-distdir",           "THVM_ATP_ESET_DISTDIR",    0, "EsetDistdir"},
  {"drain-chainpos",         "THVM_ATP_DRAIN_CHAINPOS",  0, "FormationFifo drain (chainpos)"},
  {"drain-revface",          "THVM_ATP_DRAIN_REVFACE",   0, "FormationFifo drain (revface)"},
  {"mnf",                    "THVM_ATP_MNF",             0, "GoalDirected (MNF front)"},
  {"record-norm",            "THVM_ATP_RECORD_NORM",     0, "RecordNorm"},
  {"free-var-instance",      "THVM_ATP_FVI",             0, "FreeVarInstance"},
  {"implicit-cp",            "THVM_ATP_IMPLICIT_CP",     0, "UseImplicitCp"},
  {"einsstern",              "THVM_ATP_EINSSTERN",       0, "Einsstern"},
  {"no-overlap-below-skolem","THVM_ATP_NO_OVERLAP_BELOW_SKOLEM", 0, "NoOverlapBelowSkolem"},
  {"reclassify",             "THVM_ATP_RECLASSIFY",      0, "Reclassify"},
  {"reversed-completion",    "THVM_ATP_REVERSED",        0, "ReversedCompletion"},
  {"sue-management",         "THVM_ATP_SOS",             0, "SUEManagement"},
  {"goal-subterm",           "THVM_ATP_GOAL_SUBTERM",    0, "GoalSubtermRelevance"},
  {"flatten-goal",           "THVM_ATP_FLATTEN_GOAL",    0, "FlattenGoal"},
  {"goal-narrow",            "THVM_ATP_GOAL_NARROW",     0, "GoalNarrow (ground-CC + goal-cone e-match endgame)"},
  {"gn-stride",              "THVM_ATP_GN_STRIDE",       1, "(goal-narrow sweep stride, rule revisions)"},
  {"gn-iters",               "THVM_ATP_GN_ITERS",        1, "(goal-narrow e-match passes per sweep)"},
  {"gn-max-nodes",           "THVM_ATP_GN_MAX_NODES",    1, "(goal-narrow E-graph node cap)"},
  {"gn-max-inst",            "THVM_ATP_GN_MAX_INST",     1, "(goal-narrow e-match instance-node cap)"},
  {"gn-inst-sweep",          "THVM_ATP_GN_INST_SWEEP",   1, "(goal-narrow instance quota per sweep)"},
  {"gn-up-slack",            "THVM_ATP_GN_UP_SLACK",     1, "(goal-narrow expansion slack, symbols)"},
  {"gn-cone",                "THVM_ATP_GN_CONE",         1, "(goal-narrow subjects: 1 goal-cone only, 0 any small node)"},
  {"gn-subjects",            "THVM_ATP_GN_SUBJECTS",     1, "(goal-narrow e-match subjects per pass)"},
  {"gn-subj-size",           "THVM_ATP_GN_SUBJ_SIZE",    1, "(goal-narrow max subject symbols)"},
  {"gn-inst-size",           "THVM_ATP_GN_INST_SIZE",    1, "(goal-narrow max instance symbols)"},
  {"forward-subsume",        "THVM_ATP_FWD_SUB",         0, "ForwardSubsume"},
  {"backward-subsume",       "THVM_ATP_BWD_SUB",         0, "BackwardSubsume"},
  {"backward-demod",         "THVM_ATP_BWD_DEMOD",       0, "BackwardDemod"},
  {"backward-ground-join",   "THVM_ATP_BWD_GROUND_JOIN", 0, "BackwardGroundJoin"},
  {"gj-delete-rule",         "THVM_ATP_GJ_DELETE_RULE",  0, "GjDeleteRule"},
  {"gj-index",               "THVM_ATP_GJ_INDEX",        1, "(GJ indexed top scan; default 1)"},
  {"encompass-demod",        "THVM_ATP_ENCOMPASS",       0, "(skolem-ground unorient demodulation)"},
  {"full-ac",                "THVM_ATP_FULL_AC",         0, "(auto-detect AC operators + run AC-matching/AC-unification completion)"},
  {"simpler-join",           "THVM_ATP_SIMPLER_JOIN",    0, "(Twee simplerThan-gated CP join)"},
  {"pop-rules-only",         "THVM_ATP_POP_RULES_ONLY",  0, "(Twee pop-join base: oriented rules only, no ungated doE)"},
  {"gj-budget",              "THVM_ATP_GJ_BUDGET",       1, "(GJ branch budget; small = early-KEEP)"},
  {"full-interreduce",       "THVM_ATP_FULL_INTERREDUCE",0, "FullInterreduce"},
  {"cp-side",                "THVM_ATP_CP_SIDE",         0, "CPSide"},
  {"wm-trie-faithful",       "THVM_ATP_WM_TRIE_FAITHFUL",0, "(faithful trie)"},
  {"initial-ultimate",       "THVM_ATP_INITIAL_ULTIMATE",0, "IntakeOrder initial stamp"},
  {"database-ultimate",      "THVM_ATP_DATABASE_ULTIMATE",0,"(WM database=ultimate)"},
  {"incremental-ir",         "THVM_ATP_INCR_IR",         0, "(incremental interreduce)"},
  {"cp-index",               "THVM_ATP_CP_INDEX",        0, "(CP index)"},
  {"right-reduce",           "THVM_ATP_RIGHT_REDUCE",    0, "(right reduce)"},
  {"lr-sortieren",           "THVM_ATP_LR_SORTIEREN",    0, "(WM LR sort)"},
  {"w2-mode",                "THVM_ATP_W2_MODE",         1, "(FIFO w2 mode)"},
  {"w2-modulo",              "THVM_ATP_W2_MODULO",       1, "(FIFO w2 modulo)"},
  {"discount",               "THVM_ATP_DISCOUNT",        1, "(DISCOUNT given-clause discipline: passive CPs queue RAW, R+E normalized + joined only at activation; pair with --awr)"},
  {"awr",                    "THVM_ATP_AWR",             1, "(Vampire age_weight_ratio 1:N -- one oldest-by-age pick per N weight picks; awr=50 == dis+_50)"},
  {"perm-subsume",           "THVM_ATP_PERM_SUB",        0, "(permutative subsume)"},
  {"random-ratio",           "THVM_ATP_RANDOM_RATIO",    1, "(random CP ratio)"},
  {"random-seed",            "THVM_ATP_RANDOM_SEED",     1, "(random seed)"},
  {"sym-prec",               "THVM_ATP_SYM_PREC",        1, "(Vampire symbol precedence: frequency|reverse-frequency|weighted-frequency)"},
  {"kws",                    "THVM_ATP_KWS",             1, "(KBO symbol weight scheme: const|precedence|inv-precedence|arity|inv-arity|arity-sq|inv-arity-sq|frequency|inv-frequency)"},
  {"kmz",                    "THVM_ATP_KMZ",             0, "(kbo-max-zero: max-precedence UNARY symbol gets weight 0)"},
  {"ord",                    "THVM_ATP_ORD",             1, "(reduction ordering: lpo selects LPO over the default KBO)"},
  {"equals-goal",            "THVM_ATP_EQUALS_GOAL",     1, "(existential-goal equals encoding; 2 = give up if inapplicable)"},
  {"narrow-queue",           "THVM_ATP_NARROW_QUEUE",    1, "(existential-goal critical-goal queue narrowing; 2 = give up if the goal is ground)"},
  {"goal-drive",             "THVM_ATP_GOAL_DRIVE",      1, "(refutational goal driver: backward condensed-detachment regression over the raw ifeq equations, interleaved with completion)"},
  {"def-unfold",             "THVM_ATP_DEF_UNFOLD",      1, "(function-definition unfolding: replace f(X..)=t everywhere + drop the axiom; 2 = give up if inapplicable)"},
  {"curry",                  "THVM_ATP_CURRY",           1, "(wide-functor n-ary->binary ap2 decomposition; 0 disables)"},
  {"fof",                    "THVM_ATP_FOF",             1, "(FOF/clausal front-end: clausify + Horn ifeq encoding; 0 disables)"},
  {"fof-or",                 "THVM_ATP_FOF_OR",          1, "(non-Horn or/eq lane: 0 restores the Horn-only give-up, 2 eq-reifies predicate atoms too)"},
  {"fof-distrib",            "THVM_ATP_FOF_DISTRIB",     1, "(per-record CNF distribution bound; past it the record clausifies via Tseitin)"},
  {"fof-sat",                "THVM_ATP_FOF_SAT",         1, "(non-Horn clausal-saturation pre-pass step cap; default wall-governed, 0 disables)"},
  {"fof-sine",               "THVM_ATP_FOF_SINE",        1, "(clausal pre-pass SInE cone above N clausified clauses; 0 off, 1 = default threshold 512, N>1 = threshold N)"},
  {"s2a",                    "THVM_ATP_S2A",             0, "(Vampire sine_to_age: postpone goal-distant CPs in the age dimension by effective age = cp_seq + N*sine_level; unset/0 off, N = coeff)"},
  {"fof-slice",              "THVM_ATP_FOF_SLICE",       1, "(FOF-portfolio slice gate: 2 = give up unless the input carries a fof() record)"},
  {"fof-split",              "THVM_ATP_FOF_SPLIT",       1, "(k-bounded ground case splitting for near-Horn FOF; 2 = give up if inapplicable)"},
  {"fof-split-k",            "THVM_ATP_FOF_SPLIT_K",     1, "(fof-split or-clause bound, default 4)"},
  {"fol-age-ratio",          "THVM_FOL_AGE_RATIO",       1, "(clausal pre-pass pick-given interleave: every Nth pick FIFO; 1 pure FIFO, 0 pure best-first, default 5)"},
  {"fol-goal-tier",          "THVM_FOL_GOAL_TIER",       1, "(clausal pre-pass goal tier: 0 drops the non-conjecture-ancestry pick-best penalty)"},
  {"fol-index",              "THVM_FOL_INDEX",           1, "(clausal pre-pass literal indexing: partner lists + subsumption buckets; 0 disables)"},
  {"fol-avatar",             "THVM_FOL_AVATAR",          1, "(clausal pre-pass AVATAR-lite variable-disjoint component splitting; 0 disables)"},
  {"fol-select",             "THVM_FOL_SELECT",          1, "(clausal pre-pass literal selection: 0 none = ordered-maximal, 1 first-negative (default), 2 first-positive, 3 largest-negative)"},
  {"fol-lrs",                "THVM_FOL_LRS",             1, "(clausal pre-pass limited-resource strategy: lazily discard passive clauses unreachable at the observed pick rate; 0 off)"},
  {"fol-sp",                 "THVM_FOL_SP",              1, "(clausal pre-pass KBO symbol precedence: 0 label order (default), 1 frequency (rare symbols greater, Vampire sp=frequency), 2 reverse frequency)"},
  {"fol-spb",                "THVM_FOL_SPB",             1, "(clausal pre-pass precedence bias: 1 lifts conjecture-cone symbols above all others, Vampire spb=goal)"},
  {"fol-nwc",                "THVM_FOL_NWC",             1, "(clausal pre-pass non-goal weight coefficient, multiplicative, e.g. 2.5; 0 keeps the additive tier penalty)"},
  {"fol-sos",                "THVM_FOL_SOS",             1, "(clausal pre-pass set of support: generating inferences need a conjecture-descended parent; 0 off)"},
  {"fof-sat-wall",           "THVM_ATP_FOF_SAT_WALL",    1, "(clausal pre-pass share of the REMAINING wall, percent; default 60, 95+ = the whole slice wall)"},
  {"fof-stats",              "THVM_ATP_FOF_STATS",       0, "(front-end probe: clausify, print component-split stats, exit -- no engine)"},
  {"canon-input",            "THVM_ATP_CANON_INPUT",     1, "(canonical input presentation: greater side left + sorted clauses; 0 disables)"},
  {"horn",                   "THVM_ATP_HORN",            1, "(encoded-Horn release: 0 disables)"},
  {"horn-ratio",             "THVM_ATP_HORN_RATIO",      1, "(Horn pick-given size:age ratio, 0..4)"},
  {"horn-keep",              "THVM_ATP_HORN_KEEP",       1, "(Horn kept-release cap)"},
  {"horn-wall",              "THVM_ATP_HORN_WALL",       1, "(Horn chaining wall, seconds)"},
  {"horn-max",               "THVM_ATP_HORN_MAX",        1, "(Horn release-pool cap)"},
  {"horn-att",               "THVM_ATP_HORN_ATT",        1, "(Horn join-attempt budget)"},
  {"horn-ground",            "THVM_ATP_HORN_GROUND",     1, "(Horn rank: ground tier)"},
  {"horn-bwd",               "THVM_ATP_HORN_BWD",        1, "(Horn backward subgoal cone: 0 disables)"},
  {"horn-bwd-depth",         "THVM_ATP_HORN_BWD_DEPTH",  1, "(Horn backward cone depth cap)"},
  {"horn-bwd-max",           "THVM_ATP_HORN_BWD_MAX",    1, "(Horn backward cone subgoal cap)"},
  {"horn-goal-stop",         "THVM_ATP_HORN_GOAL_STOP",  1, "(stop chaining on a goal-subsuming release: 0 disables)"},
  {"horn-rel",               "THVM_ATP_HORN_REL",        1, "(Horn relevance gate on oversized KBs: 0 disables)"},
  {"horn-rel-tol",           "THVM_ATP_HORN_REL_TOL",    1, "(Horn relevance-gate SInE tolerance)"},
  {"horn-cd",                "THVM_ATP_HORN_CD",         1, "(condensed-detachment lane: subsumption + size-cap deepening; 2 = give up if inapplicable)"},
  {"horn-cd-cap",            "THVM_ATP_HORN_CD_CAP",     1, "(CD initial released-size cap; default auto)"},
  {"horn-cd-step",           "THVM_ATP_HORN_CD_STEP",    1, "(CD deepening cap increment)"},
  {"horn-cd-vars",           "THVM_ATP_HORN_CD_VARS",    1, "(CD max distinct variables per conclusion)"},
  {"horn-back",              "THVM_ATP_HORN_BACK",       1, "(backward goal-directed CD lane: iterative-deepening goal regression; 2 = give up if inapplicable)"},
  {"horn-back-depth",        "THVM_ATP_HORN_BACK_DEPTH", 1, "(backward CD max iterative-deepening depth)"},
  {"horn-back-size",         "THVM_ATP_HORN_BACK_SIZE",  1, "(backward CD subgoal term-size cap)"},
  {"ifeq-guard",             "THVM_ATP_IFEQ_GUARD",      1, "(guard-aware CP selection on ifeq-encoded problems; 2 = give up if inapplicable)"},
  {"ifeq-penalty",           "THVM_ATP_IFEQ_PENALTY",    1, "(ifeq-guard per-undecided-guard weight factor, default 8)"},
  {"ifeq-decide",            "THVM_ATP_IFEQ_DECIDE",     0, "(ifeq-guard's complementary CP-selection BOOST for guard-DECIDING CPs; bare flag = default 0.25 multiplier, --ifeq-decide=0.1 tunes it, 0 disables)"},
  {"ifeq-strip",             "THVM_ATP_IFEQ_STRIP",      0, "(formation-time resolved ifeq-guard stripping for the LazyNormalize raw class)"},
  {"lazy-score-nf",          "THVM_ATP_LAZY_SCORE_NF",   1, "(LazyNormalize raw-class CP scoring from a step-capped-NF size estimate; 1 = cap 4, N = cap N)"},
  {"tgt",                    "THVM_ATP_TGT",             1, "(Vampire tgt / twee goal transformation: name conjecture subterms; 1 full, 2 ground, 3 all axioms = gtg=all)"},
  {"tgt-fold",               "THVM_ATP_TGT_FOLD",        0, "(tgt fidelity: definitions born inter-reduced + goal folded in place, Vampire-faithful; stops the deep-definition CP flood)"},
  {"distrib",                "THVM_ATP_DISTRIB",         0, "(Twee distributivity heuristic: fresh f_g(X,Y,Z)=g(f(X,Y),Z) definition per distributivity-shaped axiom)"},
  {"tgt-sel",                "THVM_ATP_TGT_SEL",         1, "(goal-targeted CP-selection weight scale: 1 full, 2 ground)"},
  {"tgt-nongoal",            "THVM_ATP_TGT_NONGOAL",     1, "(tgt-sel non-goal symbol weight factor, default 2)"},
  {"spb-goal",               "THVM_ATP_SPB_GOAL",        0, "(Vampire spb=goal analog: conjecture-DESCENDED CPs keep their weight, everything else scales by nwc)"},
  {"nwc",                    "THVM_ATP_NWC",             1, "(Vampire nwc analog: non-goal-derived CP weight multiplier, float, default 2.0)"},
  {"drc-off",                "THVM_ATP_DRC_OFF",         0, "(Vampire drc=off analog: keep join-redundant CPs whose sides carry a folded sF* symbol -- they become rules)"},
  {"drc-needle",             "THVM_ATP_DRC_NEEDLE",      0, "(narrows drc-off's keep set to CPs whose sF-unfolding is an original-axiom instance or trivial x=x)"},
  {"heap-cells",             "THVM_HEAP_CELLS",          1, "(heap size, cells)"},
  {"profile",                "THVM_ATP_PROFILE",         1, "(profiling level)"},
  {"diag",                   "THVM_ATP_DIAG",            1, "(diagnostics)"},
  // ENIGMA Tier-1 learned CP selection.  --enigma routes CP-queue
  // priority through the trained logistic/MLP scorer (cp_weight_mode =
  // ATP_CP_WEIGHT_LEARNED); --enigma-weights=<path> loads CASC-trained
  // weights (else the baked-in defaults score).  Both default OFF, so an
  // unset run is byte-identical to the classical selection.
  {"enigma",                 "THVM_ATP_ENIGMA",          0, "(route CP selection through the learned ENIGMA scorer)"},
  {"enigma-weights",         "THVM_ATP_ENIGMA_WEIGHTS",  1, "(path to a trained learned-scorer weights file)"},
};
static const int N_KNOBS = (int)(sizeof(KNOBS) / sizeof(KNOBS[0]));

// ===================================================================
// TPTP CNF UEQ -> .pr conversion
// ===================================================================

// Set during TPTP parsing when any negative unit carries a variable
// (the re-negated goal is existential); gates the Satisfiable claim.
// g_cli_sine_applied: SInE dropped axioms, so the engine saturated an
// INCOMPLETE subset -- a saturation then proves nothing about the full
// problem and must report GaveUp, never Satisfiable.
static int g_cli_goal_existential = 0;
static int g_cli_sine_applied     = 0;
// Set in main before conversion so the over-bound handler can emit a
// competition-valid SZS line instead of crashing.
static int g_cli_szs_wanted = 0;
// Diagnostic verbosity.  Default OFF: a CASC/competition run must emit
// ONLY the SZS status line + solution block on stdout and nothing on
// stderr.  The engine bench TU writes its per-run diagnostics (PRSYM,
// the completion stats block, progress lines) to stderr; the schedule
// slice children inherit fd 2, so silencing the parent narration is not
// enough -- the children's stderr is sent to /dev/null too.  --verbose
// or THVM_ATP_VERBOSE=1 restores everything for development.
static int g_cli_verbose = 0;
static const char *g_cli_probpath = "";
// Defined with the provenance module / FOF front-end below; the Horn
// release pass (earlier in the file) gates on them.
static int g_prov_on;
static int g_cli_fof_applied;

static void tp_giveup(const char *why) {
  // Under a schedule slice the wrapper owns the single final status, so
  // stay silent on stdout and let the non-zero exit advance the slice.
  if (g_cli_szs_wanted && getenv("FINDPROOF_SLICE") == NULL)
    printf("%% SZS status GaveUp for %s\n", g_cli_probpath);
  fprintf(stderr, "FindProof: %s\n", why);
  exit(1);
}

#define TP_MAX_CLAUSES  65536
#define TP_MAX_SYMS     262144
#define TP_MAX_VARS     4096
#define TP_NAME_LEN     64
#define TP_SIDE_LEN     2048

typedef struct {
  char lhs[TP_SIDE_LEN], rhs[TP_SIDE_LEN];
  int  neg;
  int  conj;   // TPTP role negated_conjecture (a positive '=' unit may
               // carry it: the refutation encoding's hypothesis parts)
  // Conversion-lineage provenance for --proof output (CASC-J13
  // solution compliance): which conversion produced this unit and the
  // index into the matching provenance table.  0/0 (PROV_NONE, the
  // calloc default) = parsed verbatim from the problem file -- the
  // emitter's file() citation stays byte-identical for that case.
  int  prov_kind;
  int  prov_ix;
} TpClause;

typedef struct {
  char name[TP_NAME_LEN];
  int  arity;
} TpSym;

typedef struct {
  TpClause cls[TP_MAX_CLAUSES];
  int      n_cls;
  TpSym    syms[TP_MAX_SYMS];
  int      n_syms;
  char     vars[TP_MAX_VARS][TP_NAME_LEN];
  int      n_vars;
} TpProblem;

// Provenance kinds (TpClause.prov_kind).  PROV_NONE keeps the legacy
// file() citation; the rest key the --proof emitter's conversion-chain
// records (see the "CASC-J13 solution-output provenance" section).
enum {
  PROV_NONE = 0,      // verbatim problem clause: file() leaf
  PROV_CNF,           // equality-encoding of CNF clause g_prov_cnf[ix]
  PROV_HELPER,        // encoding helper axiom: introduced(definition)
  PROV_UNFOLD,        // def-unfold rewrite: g_prov_unf[ix]
  PROV_EQGOAL,        // equals-goal reification: g_prov_eqg[ix]
  PROV_FUNNEL_GOAL,   // false != true funnel goal: parents = every
                      // negative clause's CNF record
  PROV_TRIVIAL_GOAL,  // clausification derived $false: ix = CNF clause
  PROV_HORN,          // Horn-chaining released unit: g_hpr[ix] carries
                      // the hyper-resolution derivation record
  PROV_SPLIT,         // fof-split branch clause: g_prov_split[ix] carries
                      // the avatar-shaped case-assumption records
};
// Capture entry points defined with the provenance module below; the
// def-unfold / equals-goal transforms (earlier in the file) call them.
static int  prov_def_new(const TpClause *cl);
static int  prov_unf_new(const TpClause *cl);
static void prov_unf_add_def(int unf, int def);
static int  prov_eqg_new(const TpClause *cl);

static char *tp_read_file(const char *path, size_t *len_out) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) return NULL;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)n + 1u);
  if (buf == NULL) { fclose(fp); return NULL; }
  size_t rd = fread(buf, 1, (size_t)n, fp);
  fclose(fp);
  buf[rd] = '\0';
  if (len_out != NULL) *len_out = rd;
  return buf;
}

// Resolve an include('NAME') against dirname(problem), $TPTP, CWD.
static char *tp_resolve_include(const char *problem_path, const char *inc) {
  static char cand[4096];
  const char *slash = strrchr(problem_path, '/');
  if (slash != NULL) {
    snprintf(cand, sizeof cand, "%.*s/%s",
             (int)(slash - problem_path), problem_path, inc);
    char *b = tp_read_file(cand, NULL);
    if (b != NULL) return b;
  }
  const char *tptp = getenv("TPTP");
  if (tptp != NULL) {
    snprintf(cand, sizeof cand, "%s/%s", tptp, inc);
    char *b = tp_read_file(cand, NULL);
    if (b != NULL) return b;
  }
  return tp_read_file(inc, NULL);
}

// Strip %-comments and splice include directives (one level -- TPTP
// axiom files do not nest for the UEQ library).
static char *tp_preprocess(const char *problem_path, const char *raw) {
  size_t cap = strlen(raw) * 4u + 65536u;
  char *out = (char *)malloc(cap);
  size_t o = 0;
  const char *p = raw;
  while (*p != '\0') {
    const char *eol = strchr(p, '\n');
    size_t ll = (eol != NULL) ? (size_t)(eol - p) : strlen(p);
    const char *t = p;
    size_t tl = ll;
    while (tl > 0 && isspace((unsigned char)*t)) { t++; tl--; }
    if (tl > 0 && *t == '%') { /* comment line: drop */ }
    else if (tl > 8 && strncmp(t, "include(", 8) == 0) {
      const char *q1 = strchr(t, '\'');
      const char *q2 = (q1 != NULL) ? strchr(q1 + 1, '\'') : NULL;
      if (q1 != NULL && q2 != NULL) {
        char inc[1024];
        size_t n = (size_t)(q2 - q1 - 1);
        if (n >= sizeof inc) n = sizeof inc - 1;
        memcpy(inc, q1 + 1, n);
        inc[n] = '\0';
        char *body = tp_resolve_include(problem_path, inc);
        if (body == NULL) {
          fprintf(stderr, "FindProof: cannot resolve include('%s')\n", inc);
          exit(2);
        }
        // strip comments in the included body too (no nested includes)
        const char *bp = body;
        while (*bp != '\0') {
          const char *beol = strchr(bp, '\n');
          size_t bll = (beol != NULL) ? (size_t)(beol - bp) : strlen(bp);
          const char *bt = bp;
          size_t btl = bll;
          while (btl > 0 && isspace((unsigned char)*bt)) { bt++; btl--; }
          if (!(btl > 0 && *bt == '%')) {
            if (o + bll + 2 >= cap) { cap = cap * 2 + bll; out = (char *)realloc(out, cap); }
            memcpy(out + o, bp, bll); o += bll; out[o++] = '\n';
          }
          if (beol == NULL) break;
          bp = beol + 1;
        }
        free(body);
      }
    } else {
      if (o + ll + 2 >= cap) { cap = cap * 2 + ll; out = (char *)realloc(out, cap); }
      memcpy(out + o, p, ll); o += ll; out[o++] = '\n';
    }
    if (eol == NULL) break;
    p = eol + 1;
  }
  out[o] = '\0';
  return out;
}

// Open-addressing name hash over the symbol table: the linear lookup is
// O(n_syms) per token, unusable on the CSR-class 8k-clause / 10k-symbol
// problems.  Key = name only (the arity-clash check still runs on hit).
#define TP_SYM_HASH_SLOTS (TP_MAX_SYMS * 2)
static int g_tp_sym_hash[TP_SYM_HASH_SLOTS];
static u8  g_tp_sym_hash_init = 0;
static u64 tp_name_hash(const char *name, size_t n) {
  u64 h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++) h = (h ^ (u8)name[i]) * 1099511628211ull;
  return h;
}
static int tp_sym_note_ix(TpProblem *tp, const char *name, size_t n, int arity) {
  if (n >= TP_NAME_LEN) n = TP_NAME_LEN - 1;
  if (!g_tp_sym_hash_init) {
    for (int i = 0; i < TP_SYM_HASH_SLOTS; i++) g_tp_sym_hash[i] = -1;
    g_tp_sym_hash_init = 1;
  }
  u64 h = tp_name_hash(name, n);
  u32 slot = (u32)(h % (u64)TP_SYM_HASH_SLOTS);
  for (;;) {
    int ix = g_tp_sym_hash[slot];
    if (ix < 0) break;
    if (strlen(tp->syms[ix].name) == n &&
        memcmp(tp->syms[ix].name, name, n) == 0) {
      if (tp->syms[ix].arity != arity) {
        fprintf(stderr, "FindProof: symbol %.*s used with arities %d and %d\n",
                (int)n, name, tp->syms[ix].arity, arity);
        exit(2);
      }
      return ix;
    }
    slot = (slot + 1u) % (u32)TP_SYM_HASH_SLOTS;
  }
  if (tp->n_syms >= TP_MAX_SYMS) tp_giveup("problem exceeds symbol bound");
  memcpy(tp->syms[tp->n_syms].name, name, n);
  tp->syms[tp->n_syms].name[n] = '\0';
  tp->syms[tp->n_syms].arity = arity;
  g_tp_sym_hash[slot] = tp->n_syms;
  return tp->n_syms++;
}
static void tp_sym_note(TpProblem *tp, const char *name, size_t n, int arity) {
  (void)tp_sym_note_ix(tp, name, n, arity);
}
// Reset the symbol table + hash (the post-SInE signature rebuild).
static void tp_syms_reset(TpProblem *tp) {
  tp->n_syms = 0;
  tp->n_vars = 0;
  for (int i = 0; i < TP_SYM_HASH_SLOTS; i++) g_tp_sym_hash[i] = -1;
}

static void tp_var_note(TpProblem *tp, const char *name, size_t n) {
  if (n >= TP_NAME_LEN) n = TP_NAME_LEN - 1;
  for (int i = 0; i < tp->n_vars; i++) {
    if (strlen(tp->vars[i]) == n && memcmp(tp->vars[i], name, n) == 0) return;
  }
  if (tp->n_vars >= TP_MAX_VARS) tp_giveup("problem exceeds variable bound");
  memcpy(tp->vars[tp->n_vars], name, n);
  tp->vars[tp->n_vars][n] = '\0';
  tp->n_vars++;
}

// Walk a term string, recording functor arities and variables.
static void tp_scan_term(TpProblem *tp, const char *s) {
  const char *p = s;
  while (*p != '\0') {
    if (isalnum((unsigned char)*p) || *p == '_') {
      const char *name = p;
      size_t n = 0;
      while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
      const char *q = p + n;
      while (isspace((unsigned char)*q)) q++;
      if (*q == '(') {
        // functor: count top-level args inside the balanced parens
        int depth = 1, args = 1;
        const char *r = q + 1;
        while (*r != '\0' && depth > 0) {
          if (*r == '(') depth++;
          else if (*r == ')') depth--;
          else if (*r == ',' && depth == 1) args++;
          r++;
        }
        tp_sym_note(tp, name, n, args);
      } else if (isupper((unsigned char)*name)) {
        tp_var_note(tp, name, n);
      } else {
        tp_sym_note(tp, name, n, 0);
      }
      p += n;
    } else {
      p++;
    }
  }
}

// The CSR knowledge bases carry symbol names up to 224 chars; both the
// TpSym table and the engine's .pr reader cap names at 64, and silent
// truncation would conflate distinct symbols (and desync the SIGNATURE
// from the verbatim equation text).  Shorten every over-long
// identifier IN PLACE to prefix + "_h" + 16-hex-FNV(full name) -- 58
// chars, deterministic, collision-safe in practice, case of the first
// character (variable-ness) preserved.
// TPTP quoted atoms ('+', '>=', ...) survive preprocessing as-is, but
// no downstream identifier grammar (.pr emission, the engine reader,
// the Horn parser) admits them -- LCL890-10 died on a pr parse error.
// Rewrite every DISTINCT quoted atom to a fresh plain name (qtd0,
// qtd1, ...): an isomorphic renaming, so provability is untouched.
// Candidate names are collision-guarded against the input text.
static char *tp_unquote_names(char *text) {
  if (strchr(text, '\'') == NULL) return text;
#define TP_MAX_QUOTED 512
  char *qname[TP_MAX_QUOTED];
  char  qrepl[TP_MAX_QUOTED][24];
  int nq = 0;
  size_t cap = strlen(text) + 4096;
  char *out = (char *)malloc(cap);
  size_t o = 0;
  if (out == NULL) tp_giveup("OOM in quoted-atom rename");
  for (const char *r = text; *r != '\0'; ) {
    if (o + 32 >= cap) {
      cap *= 2;
      out = (char *)realloc(out, cap);
      if (out == NULL) tp_giveup("OOM in quoted-atom rename");
    }
    if (*r != '\'') { out[o++] = *r++; continue; }
    const char *q2 = r + 1;
    while (*q2 != '\0' && *q2 != '\'')
      q2 += (q2[0] == '\\' && q2[1] != '\0') ? 2 : 1;
    if (*q2 == '\0' || q2 == r + 1) { out[o++] = *r++; continue; }
    size_t n = (size_t)(q2 - (r + 1));
    int ix = -1;
    for (int i = 0; i < nq; i++)
      if (strlen(qname[i]) == n && memcmp(qname[i], r + 1, n) == 0) { ix = i; break; }
    if (ix < 0 && nq < TP_MAX_QUOTED) {
      qname[nq] = strndup(r + 1, n);
      snprintf(qrepl[nq], sizeof qrepl[nq], "qtd%d", nq);
      while (strstr(text, qrepl[nq]) != NULL &&
             strlen(qrepl[nq]) + 2 < sizeof qrepl[nq])
        strcat(qrepl[nq], "x");
      ix = nq++;
    }
    if (ix < 0) { out[o++] = *r++; continue; }   // table full: leave as-is
    size_t rl = strlen(qrepl[ix]);
    memcpy(out + o, qrepl[ix], rl); o += rl;
    r = q2 + 1;
  }
  out[o] = '\0';
  for (int i = 0; i < nq; i++) free(qname[i]);
  free(text);
  return out;
}

static void tp_shorten_names(char *text) {
  char *r = text, *w = text;
  while (*r != '\0') {
    if (isalnum((unsigned char)*r) || *r == '_') {
      size_t n = 0;
      while (isalnum((unsigned char)r[n]) || r[n] == '_') n++;
      if (n >= TP_NAME_LEN) {
        u64 h = tp_name_hash(r, n);
        memmove(w, r, 40);
        w += 40;
        w += snprintf(w, 20, "_h%016llx", (unsigned long long)h);
      } else {
        memmove(w, r, n);
        w += n;
      }
      r += n;
    } else {
      *w++ = *r++;
    }
  }
  *w = '\0';
}

// Parse the preprocessed text into clauses + signature.
static void tp_parse(TpProblem *tp, const char *text) {
  const char *p = text;
  while ((p = strstr(p, "cnf(")) != NULL) {
    p += 4;
    // balanced scan to the closing ')' of this cnf(...)
    int depth = 1;
    const char *body = p;
    while (*p != '\0' && depth > 0) {
      if (*p == '(') depth++;
      else if (*p == ')') depth--;
      p++;
    }
    size_t blen = (size_t)(p - body - 1);
    // split off name and role at the first two top-level commas
    int d = 0, commas = 0;
    const char *lit = NULL;
    const char *role = NULL;
    for (size_t i = 0; i < blen; i++) {
      char c = body[i];
      if (c == '(') d++;
      else if (c == ')') d--;
      else if (c == ',' && d == 0) {
        commas++;
        if (commas == 1) role = body + i + 1;
        if (commas == 2) { lit = body + i + 1; break; }
      }
    }
    if (lit == NULL) continue;
    int is_conj = 0;
    if (role != NULL) {
      size_t rl = (size_t)(lit - 1 - role);
      is_conj = (memmem(role, rl, "negated_conjecture", 18) != NULL);
    }
    size_t llen = blen - (size_t)(lit - body);
    // trim spaces and one level of ( )
    while (llen > 0 && isspace((unsigned char)*lit)) { lit++; llen--; }
    while (llen > 0 && isspace((unsigned char)lit[llen - 1])) llen--;
    while (llen >= 2 && lit[0] == '(' && lit[llen - 1] == ')') {
      // only strip if balanced as an outer wrapper
      int dd = 0; int wraps = 1;
      for (size_t i = 0; i + 1 < llen; i++) {
        if (lit[i] == '(') dd++;
        else if (lit[i] == ')') { dd--; if (dd == 0 && i + 1 < llen - 1) { wraps = 0; break; } }
      }
      if (!wraps) break;
      lit++; llen -= 2;
      while (llen > 0 && isspace((unsigned char)*lit)) { lit++; llen--; }
      while (llen > 0 && isspace((unsigned char)lit[llen - 1])) llen--;
    }
    // find the (top-level) equality operator
    int neg = 0;
    size_t oppos = (size_t)-1;
    int d2 = 0;
    for (size_t i = 0; i < llen; i++) {
      if (lit[i] == '(') d2++;
      else if (lit[i] == ')') d2--;
      else if (d2 == 0 && lit[i] == '!' && i + 1 < llen && lit[i + 1] == '=') { neg = 1; oppos = i; break; }
      else if (d2 == 0 && lit[i] == '=' && (i == 0 || lit[i - 1] != '!')) { oppos = i; break; }
    }
    if (oppos == (size_t)-1) { fprintf(stderr, "FindProof: non-equational clause\n"); exit(2); }
    if (tp->n_cls >= TP_MAX_CLAUSES) tp_giveup("problem exceeds clause bound");
    TpClause *cl = &tp->cls[tp->n_cls++];
    size_t ln = oppos, rs = oppos + (neg ? 2u : 1u);
    while (ln > 0 && isspace((unsigned char)lit[ln - 1])) ln--;
    while (rs < llen && isspace((unsigned char)lit[rs])) rs++;
    size_t rn = llen - rs;
    if (ln >= sizeof cl->lhs || rn >= sizeof cl->rhs) tp_giveup("term exceeds side-length bound");
    memcpy(cl->lhs, lit, ln); cl->lhs[ln] = '\0';
    memcpy(cl->rhs, lit + rs, rn); cl->rhs[rn] = '\0';
    cl->neg = neg;
    cl->conj = is_conj;
    cl->prov_kind = PROV_NONE;
    cl->prov_ix   = 0;
    if (neg) {
      // uppercase-initial token anywhere in the goal = goal variable
      for (const char *q = lit; *q != '\0' && q < lit + llen; q++) {
        if (isupper((unsigned char)*q) &&
            (q == lit || !(isalnum((unsigned char)q[-1]) || q[-1] == '_'))) {
          g_cli_goal_existential = 1;
          break;
        }
      }
    }
    tp_scan_term(tp, cl->lhs);
    tp_scan_term(tp, cl->rhs);
  }
}

static int tp_sym_cmp(const void *a, const void *b) {
  const TpSym *x = (const TpSym *)a, *y = (const TpSym *)b;
  if (x->arity != y->arity) return x->arity - y->arity;
  return strcmp(x->name, y->name);
}

// ── SInE-style goal-directed axiom selection (Hoder-Voronkov shape) ──
// The CSR-class UEQ problems are 8k-clause knowledge bases; completion
// drowns before touching the goal.  Select the goal-relevant slice:
//   gen(s)   = number of clauses containing symbol s (generality);
//   trigger  = clause C is triggered by symbol s in C iff
//              gen(s) <= gt  or  gen(s) <= tol * min_gen(C)
//              (s is near-least-general in C), where min_gen(C) runs
//              over the gt-GENERAL symbols of C only (gen > gt);
//   reach    = start from the negative units' symbols; a triggered
//              clause joins the selection and its symbols become
//              reached; iterate to fixpoint (or `rounds` levels).
// The generality threshold gt is Vampire's sine_generality_threshold
// (Shell/SineUtils.cpp): a near-fresh symbol (gen <= gt) neither
// defines the clause's min generality nor needs the tolerance test to
// trigger.  Without it a single fresh constant drags min_gen(C) to 1
// and no reachable symbol can ever trigger C: CSR058-10's one missing
// axiom holds the answer-string constant s_terrorist... (gen=1), so
// its best reached trigger (c_terroristgroup, gen=26) failed
// 26 <= tol*1 at every tolerance and the cone saturated goal-free.
// Raw Vampire semantics over-corrects, though: excluding the fresh
// constant from `mtvisible(c_fresh_mt) = true` hands min_gen to the
// HUB symbol mtvisible (gen 1746), which then self-triggers -- all
// 1746 facts join and the cone explodes 66 -> 5141 on CSR058.  The
// rare-cap rc keeps the exclusion exactly where it helps: the
// threshold-filtered min stands only if it is still RARE (<= rc);
// otherwise the plain min over all symbols governs, restoring the
// old untriggerable state for fresh-defined hub facts.  Swept offline
// against the dumped selection inputs: gt=1 rc=64 selects every
// oracle-proof axiom of CSR058 at cone 109 (was: 1 missing at 66) and
// of CSR036 at tol 1.5 (932), with the hub flood contained.
// The encoding glue (ifeq4/true, generality in the thousands) never
// triggers, exactly as SInE intends.  Negative units are always kept.
// Returns the kept count; compacts tp->cls in place.
static int tp_sine_pass(TpProblem *tp, double tol, int rounds, u32 gt,
                        u32 rc) {
  int n = tp->n_cls;
  enum { SINE_CS = 48 };
  int (*csym)[SINE_CS] = malloc((size_t)n * sizeof *csym);
  int *csym_n = calloc((size_t)n, sizeof(int));
  u32 *gen    = calloc((size_t)tp->n_syms, sizeof(u32));
  u8  *reach  = calloc((size_t)tp->n_syms, 1);
  u8  *keep   = calloc((size_t)n, 1);
  if (csym == NULL || csym_n == NULL || gen == NULL || reach == NULL ||
      keep == NULL) tp_giveup("OOM in axiom selection");
  // Per-clause dedup'd symbol-id lists (functors + constants, no vars).
  for (int c = 0; c < n; c++) {
    const char *sides[2] = { tp->cls[c].lhs, tp->cls[c].rhs };
    for (int sd = 0; sd < 2; sd++) {
      const char *pp = sides[sd];
      while (*pp != '\0') {
        if (isalnum((unsigned char)*pp) || *pp == '_') {
          const char *name = pp;
          size_t ln = 0;
          while (isalnum((unsigned char)pp[ln]) || pp[ln] == '_') ln++;
          if (!isupper((unsigned char)*name)) {
            const char *q = pp + ln;
            while (isspace((unsigned char)*q)) q++;
            int arity = 0;
            if (*q == '(') {
              int depth = 1; arity = 1;
              const char *r = q + 1;
              while (*r != '\0' && depth > 0) {
                if (*r == '(') depth++;
                else if (*r == ')') depth--;
                else if (*r == ',' && depth == 1) arity++;
                r++;
              }
            }
            int ix = tp_sym_note_ix(tp, name, ln, arity);
            u8 seen = 0;
            for (int k = 0; k < csym_n[c]; k++)
              if (csym[c][k] == ix) { seen = 1; break; }
            if (!seen && csym_n[c] < SINE_CS) csym[c][csym_n[c]++] = ix;
          }
          pp += ln;
        } else pp++;
      }
    }
    for (int k = 0; k < csym_n[c]; k++) gen[csym[c][k]]++;
  }
  // Seed: every conjecture-role unit (the refutation encoding splits
  // the query into a '!=' goal plus positive '=' hypothesis units, all
  // carrying negated_conjecture) -- kept unconditionally, symbols
  // reached.
  for (int c = 0; c < n; c++) {
    if (!tp->cls[c].neg && !tp->cls[c].conj) continue;
    keep[c] = 1;
    for (int k = 0; k < csym_n[c]; k++) reach[csym[c][k]] = 1;
  }
  // Levels to fixpoint (or the round cap).
  for (int rd = 0; rd < rounds; rd++) {
    u8 grew = 0;
    for (int c = 0; c < n; c++) {
      if (keep[c]) continue;
      u32 min_all = 0xffffffffu, min_ex = 0xffffffffu;
      for (int k = 0; k < csym_n[c]; k++) {
        u32 g = gen[csym[c][k]];
        if (g < min_all) min_all = g;
        if (g > gt && g < min_ex) min_ex = g;
      }
      u32 mingen = (min_ex <= rc) ? min_ex : min_all;
      u8 hit = 0;
      for (int k = 0; k < csym_n[c] && !hit; k++) {
        int s = csym[c][k];
        if (!reach[s]) continue;
        if (gen[s] <= gt || (double)gen[s] <= tol * (double)mingen) hit = 1;
      }
      if (hit) {
        keep[c] = 1;
        grew = 1;
        for (int k = 0; k < csym_n[c]; k++) reach[csym[c][k]] = 1;
      }
    }
    if (!grew) break;
  }
  int w = 0;
  for (int c = 0; c < n; c++) {
    if (!keep[c]) continue;
    if (w != c) tp->cls[w] = tp->cls[c];
    w++;
  }
  free(csym); free(csym_n); free(gen); free(reach); free(keep);
  return w;
}

// Debug: THVM_ATP_SINE_DUMP=path writes the final cone to `path` and
// the selection's full input set to `path.in`, one clause per line.
static void tp_sine_dump(const TpProblem *tp, const char *path, int n) {
  FILE *df = fopen(path, "w");
  if (df == NULL) return;
  for (int i = 0; i < n; i++)
    fprintf(df, "%s %s %s\n", tp->cls[i].lhs,
            tp->cls[i].neg ? "!=" : (tp->cls[i].conj ? "=c" : "="),
            tp->cls[i].rhs);
  fclose(df);
}

// Driver: progressively tighten until the selection fits the engine
// caps -- TOLERANCE first at full depth, rounds only as a last resort.
// The CSR knowledge-base proofs ride 15+-trigger-deep chains (CSR036's
// genls towers, CSR040's tptpcol_k subsumption tower: the Vampire-4.9
// and E-3.1 oracle proofs use ~16 chain axioms each), so the old
// rounds={64,3,2,1} ladder amputated exactly the axioms the goal
// needs whenever the first pass overflowed the 256-equation cap.
// On success rebuilds the signature from the kept clauses (the
// dropped ones own most of the 10k-symbol table).  THVM_ATP_SINE=0
// disables; THVM_ATP_SINE_TOL overrides the first tolerance;
// THVM_ATP_SINE_GT overrides the generality threshold.
static void tp_sine_select(TpProblem *tp) {
  double tol0 = 1.5;
  {
    const char *e = getenv("THVM_ATP_SINE_TOL");
    if (e != NULL && e[0] != '\0') tol0 = atof(e);
    if (tol0 < 1.0) tol0 = 1.0;
  }
  u32 gt = 1;
  {
    const char *e = getenv("THVM_ATP_SINE_GT");
    if (e != NULL && e[0] != '\0') {
      long v = atol(e);
      gt = (v < 0) ? 0u : (u32)v;
    }
  }
  u32 rc = 64;
  {
    const char *e = getenv("THVM_ATP_SINE_RC");
    if (e != NULL && e[0] != '\0') {
      long v = atol(e);
      rc = (v < 0) ? 0u : (u32)v;
    }
  }
  const char *dump = getenv("THVM_ATP_SINE_DUMP");
  if (dump != NULL && dump[0] != '\0') {
    char inpath[1024];
    snprintf(inpath, sizeof inpath, "%s.in", dump);
    tp_sine_dump(tp, inpath, tp->n_cls);
  }
  const double tols[6]  = { tol0, 1.2, 1.0, 1.0, 1.0, 1.0 };
  const int    rnds[6]  = { 64,   64,  64,  3,   2,   1  };
  int n0 = tp->n_cls;
  for (int a = 0; a < 6; a++) {
    tp->n_cls = n0;   // each attempt filters the (previously kept) set
    int kept = tp_sine_pass(tp, tols[a], rnds[a], gt, rc);
    fprintf(stderr,
            "FindProof: SInE pass %d tol=%.2f rounds=%d gt=%u rc=%u "
            "kept=%d/%d\n", a, tols[a], rnds[a], gt, rc, kept, n0);
    int n_neg = 0;
    for (int i = 0; i < kept; i++) n_neg += tp->cls[i].neg;
    if ((kept - n_neg) <= (int)PR_MAX_EQNS) {
      tp->n_cls = kept;
      tp_syms_reset(tp);
      for (int i = 0; i < kept; i++) {
        tp_scan_term(tp, tp->cls[i].lhs);
        tp_scan_term(tp, tp->cls[i].rhs);
      }
      fprintf(stderr, "FindProof: SInE selected %d/%d clauses (tol=%.2f)\n",
              kept, n0, tols[a]);
      if (dump != NULL && dump[0] != '\0') tp_sine_dump(tp, dump, kept);
      g_cli_sine_applied = 1;
      return;
    }
    // over the cap: the pass already compacted keep-first, but a
    // tighter re-run must see the ORIGINAL clause set -- it does not
    // (dropped clauses were overwritten).  Tighten on the KEPT set
    // instead: strictly narrower each attempt, so this converges.
    tp->n_cls = kept;
    n0 = kept;
  }
  // Even the tightest ladder rung is over the cap (the AGT-class agent
  // KBs hold a goal cone wider than the engine's equation capacity:
  // 26 of 29 schedule lanes died HERE on AGT011+2, spending the whole
  // wall on nothing).  Truncate deterministically instead of dying:
  // negatives + conjecture units all stay, then the kept axioms in
  // selection order up to the cap.  A subset is sound for Theorem;
  // saturation already reports GaveUp under g_cli_sine_applied.
  {
    int n = tp->n_cls;
    int w = 0, n_ax = 0, ax_cap = (int)PR_MAX_EQNS;
    for (int pass = 0; pass < 2; pass++)
      for (int c = 0; c < n; c++) {
        int is_goal = tp->cls[c].neg || tp->cls[c].conj;
        if ((pass == 0) != (is_goal != 0)) continue;
        if (pass == 1 && n_ax >= ax_cap) continue;
        if (pass == 1) n_ax++;
        else if (!tp->cls[c].neg) ax_cap--;   // positive conjecture units
                                              // count against the eqn cap
        if (w != c) {
          TpClause tmp = tp->cls[w];
          tp->cls[w] = tp->cls[c];
          if (pass == 0) tp->cls[c] = tmp;   // pass 1 discards, no swap-back
        }
        w++;
      }
    int n_neg = 0;
    for (int i = 0; i < w; i++) n_neg += tp->cls[i].neg;
    if ((w - n_neg) > (int)PR_MAX_EQNS)
      tp_giveup("axiom selection cannot fit the engine caps");
    tp->n_cls = w;
    tp_syms_reset(tp);
    for (int i = 0; i < w; i++) {
      tp_scan_term(tp, tp->cls[i].lhs);
      tp_scan_term(tp, tp->cls[i].rhs);
    }
    fprintf(stderr, "FindProof: SInE over cap at every tolerance; "
            "truncated to %d/%d clauses\n", w, n);
    if (dump != NULL && dump[0] != '\0') tp_sine_dump(tp, dump, w);
    g_cli_sine_applied = 1;
  }
}

// ===================================================================
// Existential-goal equals encoding (Twee addNarrowing port)
// ===================================================================
//
// Twee never narrows a non-ground goal directly: addNarrowing
// (twee executable/SequentialMain.hs:721) encodes each existential
// goal  s != t  (a variable in the re-negated conjecture is
// existentially quantified) equationally --
//     eq(X,X) = true          (reflexivity)
//     eq(s,t) = false         (one per non-ground goal; the goal's
//                              variables turn universally quantified)
//     true   != false         (the single GROUND replacement goal)
// -- and completion narrows the eq(s,t) axiom at its argument
// positions until the two arguments unify, at which point the root
// overlap with reflexivity yields true = false: the refutation closes
// through an ordinary ground-goal join, no narrowing lane involved.
//
// Conservativity (Twee tags the step "esa" -- equisatisfiable): with
// eq/true/false FRESH symbols,
//   (=>) if S u { s != t } is unsatisfiable, some instance s@ = t@ is
//        S-derivable (Birkhoff), so eq(s@,t@) rewrites to true via
//        reflexivity AND to false via the goal axiom: true = false,
//        contradicting the new goal -- the encoding is unsatisfiable.
//   (<=) a model M of S u { forall X. s != t } extends to the
//        encoding: the goal instances give two distinct elements
//        a != b; set true^M = a, false^M = b, eq^M(u,v) = a if u = v
//        else b.  Reflexivity holds, each eq(s,t) = false holds
//        (every instance of s,t differs in M), true != false holds --
//        the encoding is satisfiable.
// Unsatisfiable therefore transfers both ways.  A saturation under
// the encoding still reports GaveUp, never Satisfiable
// (g_cli_equals_applied, the g_cli_*_applied gate pattern) -- belt
// and suspenders on top of the g_cli_goal_existential gate.
//
// OPT-IN (--equals-goal=1), not the default: the engine's narrowing
// lane decides most of the corpus's existential goals instantly
// (COL004-1 proves at 0.0s narrowing but TIMES OUT encoded), while
// the encoding cracks the goals narrowing starves on (COL009-1
// proves at 0.1s encoded but times out narrowing) -- complementary
// lanes, so the schedule runs the encoding as its own slice.
// --equals-goal=2 additionally EXITS (give-up) when the encoding
// does not apply, so the slice never duplicates the WM preset on
// ground-goal (or .pr) problems.

static int g_cli_equals_applied = 0;

// ===================================================================
// Function-definition unfolding (Vampire definition_unfolding port)
// ===================================================================
//
// A positive unit axiom  f(X1,...,Xn) = t  (either side) whose
// arguments are DISTINCT bare variables, with vars(t) inside
// {X1,...,Xn} and f absent from t, pins f's interpretation pointwise
// in every model: replacing each occurrence f(s1,...,sn) by
// t{Xi := si} in EVERY clause and dropping the axiom preserves
// satisfiability in both directions (Vampire's definition_unfolding
// preprocessing, applied by its default fde=all).  The COL
// combinator family is the motivating shape: COL002-5's
// weak_fixed_point(X) = S-C-B-term is such a definition, and KBO
// orients it big-term -> weak_fixed_point(X), so the folded goal
// side is IRREDUCIBLE and completion must wander presentation-
// dependent detours (1.5s or Timeout at 30s depending on nothing but
// the input clause order/orientation -- the canonical presentation
// LOSES it in every corpus release).  Unfolded, flatten-goal names
// the conjecture's ground subterms (= Vampire tgt=ground) and the
// oracle's 6-superposition derivation surfaces within the first few
// completion steps under ANY presentation (5 steps, 0.0s).
// THVM_ATP_DEF_UNFOLD: 1 = apply, 2 = apply and give up when no
// definition was unfolded (the schedule's exit-fast slice
// discipline, like --equals-goal=2).
#define TP_DU_MAX_ARGS 8
static int g_cli_defunfold_n = 0;

static size_t tp_du_ident_len(const char *p) {
  size_t n = 0;
  while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
  return n;
}

// Parse `side` as  f(V1,...,Vn)  with 1 <= n <= TP_DU_MAX_ARGS bare
// DISTINCT variable arguments; fill fname/args and return n, else -1.
static int tp_du_parse_head(const char *side, char *fname, size_t fcap,
                            char args[][TP_NAME_LEN]) {
  const char *p = side;
  while (isspace((unsigned char)*p)) p++;
  size_t flen = tp_du_ident_len(p);
  if (flen == 0u || flen >= fcap || isupper((unsigned char)*p)) return -1;
  memcpy(fname, p, flen);
  fname[flen] = '\0';
  p += flen;
  while (isspace((unsigned char)*p)) p++;
  if (*p != '(') return -1;
  p++;
  int n = 0;
  for (;;) {
    while (isspace((unsigned char)*p)) p++;
    size_t an = tp_du_ident_len(p);
    if (an == 0u || an >= TP_NAME_LEN || !isupper((unsigned char)*p) ||
        n >= TP_DU_MAX_ARGS)
      return -1;
    memcpy(args[n], p, an);
    args[n][an] = '\0';
    for (int i = 0; i < n; i++)
      if (strcmp(args[i], args[n]) == 0) return -1;    // repeated variable
    p += an;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '(') return -1;                          // compound argument
    n++;
    if (*p == ',') { p++; continue; }
    if (*p == ')') { p++; break; }
    return -1;
  }
  while (isspace((unsigned char)*p)) p++;
  return (*p == '\0' && n >= 1) ? n : -1;
}

// 1 iff every variable token of `t` is among args and the token
// `fname` never occurs in `t`.
static int tp_du_body_ok(const char *t, const char *fname,
                         char args[][TP_NAME_LEN], int nargs) {
  size_t fl = strlen(fname);
  for (const char *p = t; *p != '\0'; ) {
    if (!(isalnum((unsigned char)*p) || *p == '_')) { p++; continue; }
    size_t n = tp_du_ident_len(p);
    if (n == fl && memcmp(p, fname, fl) == 0) return 0;
    if (isupper((unsigned char)*p)) {
      const char *q = p + n;
      while (isspace((unsigned char)*q)) q++;
      if (*q != '(') {
        int known = 0;
        for (int i = 0; i < nargs && !known; i++)
          known = (strlen(args[i]) == n && memcmp(args[i], p, n) == 0);
        if (!known) return 0;
      }
    }
    p += n;
  }
  return 1;
}

// One left-to-right expansion pass: every occurrence of fname(...) in
// src is replaced by body{args := the occurrence's argument spans}
// (argument text copied verbatim, so occurrences nested inside the
// arguments survive into the output and need a further pass).
// Returns the number of occurrences expanded, or -1 on overflow /
// arity mismatch / malformed input.
static int tp_du_pass(const char *src, char *dst, size_t cap,
                      const char *fname, int nargs,
                      char args[][TP_NAME_LEN], const char *body) {
  size_t fl = strlen(fname), o = 0;
  int expanded = 0;
  const char *p = src;
  while (*p != '\0') {
    if (!(isalnum((unsigned char)*p) || *p == '_')) {
      if (o + 1 >= cap) return -1;
      dst[o++] = *p++;
      continue;
    }
    size_t n = tp_du_ident_len(p);
    const char *q = p + n;
    while (isspace((unsigned char)*q)) q++;
    if (!(n == fl && memcmp(p, fname, fl) == 0 && *q == '(')) {
      if (o + n >= cap) return -1;
      memcpy(dst + o, p, n);
      o += n;
      p += n;
      continue;
    }
    // f(...): collect the nargs top-level argument spans
    const char *as[TP_DU_MAX_ARGS];
    size_t      al[TP_DU_MAX_ARGS];
    const char *r = q + 1, *astart = q + 1;
    int depth = 1, ai = 0;
    while (*r != '\0' && depth > 0) {
      if (*r == '(') depth++;
      else if (*r == ')') { depth--; if (depth == 0) break; }
      else if (*r == ',' && depth == 1) {
        if (ai >= nargs) return -1;                    // arity mismatch
        as[ai] = astart;
        al[ai] = (size_t)(r - astart);
        ai++;
        astart = r + 1;
      }
      r++;
    }
    if (*r != ')' || ai >= nargs) return -1;
    as[ai] = astart;
    al[ai] = (size_t)(r - astart);
    ai++;
    if (ai != nargs) return -1;                        // arity mismatch
    // emit body with argument substitution
    for (const char *b = body; *b != '\0'; ) {
      if (!(isalnum((unsigned char)*b) || *b == '_')) {
        if (o + 1 >= cap) return -1;
        dst[o++] = *b++;
        continue;
      }
      size_t bn = tp_du_ident_len(b);
      int sub = -1;
      if (isupper((unsigned char)*b)) {
        const char *bq = b + bn;
        while (isspace((unsigned char)*bq)) bq++;
        if (*bq != '(')
          for (int i = 0; i < nargs && sub < 0; i++)
            if (strlen(args[i]) == bn && memcmp(args[i], b, bn) == 0) sub = i;
      }
      if (sub >= 0) {
        if (o + al[sub] >= cap) return -1;
        memcpy(dst + o, as[sub], al[sub]);
        o += al[sub];
      } else {
        if (o + bn >= cap) return -1;
        memcpy(dst + o, b, bn);
        o += bn;
      }
      b += bn;
    }
    expanded++;
    p = r + 1;
  }
  dst[o] = '\0';
  return expanded;
}

// Fully expand `side` into out (TP_SIDE_LEN); iterated passes because
// argument text is copied verbatim.  Each pass strictly decreases the
// occurrence count (f is absent from body), so this terminates.
// Returns 0 on failure (overflow / arity mismatch / runaway growth).
static int tp_du_expand_side(const char *side, char *out,
                             const char *fname, int nargs,
                             char args[][TP_NAME_LEN], const char *body) {
  static char bufa[8 * TP_SIDE_LEN], bufb[8 * TP_SIDE_LEN];
  snprintf(bufa, sizeof bufa, "%s", side);
  char *cur = bufa, *nxt = bufb;
  for (int guard = 0; ; guard++) {
    if (guard >= 64) return 0;
    int e = tp_du_pass(cur, nxt, 8 * TP_SIDE_LEN, fname, nargs, args, body);
    if (e < 0) return 0;
    char *t = cur; cur = nxt; nxt = t;
    if (e == 0) break;
  }
  if (strlen(cur) >= TP_SIDE_LEN) return 0;
  strcpy(out, cur);
  return 1;
}

static void tp_def_unfold(TpProblem *tp) {
  char skip[64][TP_NAME_LEN];
  int  n_skip = 0;
  for (int round = 0; round < 64; round++) {
    char fname[TP_NAME_LEN], args[TP_DU_MAX_ARGS][TP_NAME_LEN];
    int  nargs = -1, def_ix = -1;
    const char *body = NULL;
    for (int i = 0; i < tp->n_cls && def_ix < 0; i++) {
      TpClause *cl = &tp->cls[i];
      if (cl->neg || cl->conj) continue;
      for (int side = 0; side < 2 && def_ix < 0; side++) {
        const char *h = side ? cl->rhs : cl->lhs;
        const char *t = side ? cl->lhs : cl->rhs;
        int na = tp_du_parse_head(h, fname, sizeof fname, args);
        if (na < 1) continue;
        if (!tp_du_body_ok(t, fname, args, na)) continue;
        int skipped = 0;
        for (int k = 0; k < n_skip && !skipped; k++)
          skipped = (strcmp(skip[k], fname) == 0);
        if (skipped) continue;
        nargs = na;
        def_ix = i;
        body = t;
      }
    }
    if (def_ix < 0) break;
    // Dry-run: every side of every other clause must expand within the
    // side buffer -- unfolding is all-or-nothing per symbol (a partial
    // unfold that dropped the axiom would be unsound).
    char tmp[TP_SIDE_LEN];
    int ok = 1;
    for (int i = 0; i < tp->n_cls && ok; i++) {
      if (i == def_ix) continue;
      ok = tp_du_expand_side(tp->cls[i].lhs, tmp, fname, nargs, args, body) &&
           tp_du_expand_side(tp->cls[i].rhs, tmp, fname, nargs, args, body);
    }
    if (!ok) {
      if (n_skip < 64) snprintf(skip[n_skip++], TP_NAME_LEN, "%s", fname);
      continue;
    }
    // Commit: rewrite every clause, then drop the definition axiom.
    // Under --proof, keep the eliminated definition and every
    // clause's pre-unfold text so the emitter can cite
    // inference(definition_unfolding, ..., [u<k>, d<j>..]).
    int def_prov = g_prov_on ? prov_def_new(&tp->cls[def_ix]) : -1;
    for (int i = 0; i < tp->n_cls; i++) {
      if (i == def_ix) continue;
      char nl[TP_SIDE_LEN], nr[TP_SIDE_LEN];
      tp_du_expand_side(tp->cls[i].lhs, nl, fname, nargs, args, body);
      tp_du_expand_side(tp->cls[i].rhs, nr, fname, nargs, args, body);
      if (def_prov >= 0 && (strcmp(nl, tp->cls[i].lhs) != 0 ||
                            strcmp(nr, tp->cls[i].rhs) != 0)) {
        TpClause *cl = &tp->cls[i];
        if (cl->prov_kind != PROV_UNFOLD) {
          int u = prov_unf_new(cl);
          if (u >= 0) { cl->prov_kind = PROV_UNFOLD; cl->prov_ix = u; }
        }
        if (cl->prov_kind == PROV_UNFOLD)
          prov_unf_add_def(cl->prov_ix, def_prov);
      }
      strcpy(tp->cls[i].lhs, nl);
      strcpy(tp->cls[i].rhs, nr);
    }
    for (int i = def_ix + 1; i < tp->n_cls; i++) tp->cls[i - 1] = tp->cls[i];
    tp->n_cls--;
    g_cli_defunfold_n++;
  }
  if (g_cli_defunfold_n > 0) {
    tp_syms_reset(tp);
    for (int i = 0; i < tp->n_cls; i++) {
      tp_scan_term(tp, tp->cls[i].lhs);
      tp_scan_term(tp, tp->cls[i].rhs);
    }
    fprintf(stderr, "%% def-unfold: %d function definition(s) eliminated\n",
            g_cli_defunfold_n);
  }
}

// Does a clause side contain a variable (uppercase-initial token)?
static int tp_side_has_var(const char *s) {
  for (const char *q = s; *q != '\0'; q++) {
    if (isupper((unsigned char)*q) &&
        (q == s || !(isalnum((unsigned char)q[-1]) || q[-1] == '_')))
      return 1;
  }
  return 0;
}

// A fresh function-symbol name: `base`, suffixed with 'q's until it
// matches nothing in the symbol table (any arity).
static void tp_fresh_sym(const TpProblem *tp, const char *base,
                         char *out, size_t cap) {
  snprintf(out, cap, "%s", base);
  for (;;) {
    int clash = 0;
    for (int i = 0; i < tp->n_syms && !clash; i++)
      clash = (strcmp(tp->syms[i].name, out) == 0);
    if (!clash) return;
    size_t l = strlen(out);
    if (l + 2 >= cap) tp_giveup("cannot make a fresh symbol name");
    out[l] = 'q'; out[l + 1] = '\0';
  }
}

static void tp_equals_goal(TpProblem *tp) {
  int n_nonground = 0;
  for (int i = 0; i < tp->n_cls; i++)
    if (tp->cls[i].neg && (tp_side_has_var(tp->cls[i].lhs) ||
                           tp_side_has_var(tp->cls[i].rhs)))
      n_nonground++;
  if (n_nonground == 0) return;
  // Fall back to the narrowing lane if the encoding cannot fit.
  if (tp->n_cls + 2 > TP_MAX_CLAUSES) return;
  char eqn[TP_NAME_LEN], tru[TP_NAME_LEN], fls[TP_NAME_LEN];
  tp_fresh_sym(tp, "eq",     eqn, sizeof eqn);
  tp_fresh_sym(tp, "etrue",  tru, sizeof tru);
  tp_fresh_sym(tp, "efalse", fls, sizeof fls);
  for (int i = 0; i < tp->n_cls; i++) {
    TpClause *cl = &tp->cls[i];
    if (!cl->neg || (!tp_side_has_var(cl->lhs) && !tp_side_has_var(cl->rhs)))
      continue;
    if (strlen(eqn) + strlen(cl->lhs) + strlen(cl->rhs) + 4 >= TP_SIDE_LEN)
      return;
  }
  // Rewrite each non-ground goal in place:  eq(s,t) = false.
  for (int i = 0; i < tp->n_cls; i++) {
    TpClause *cl = &tp->cls[i];
    if (!cl->neg || (!tp_side_has_var(cl->lhs) && !tp_side_has_var(cl->rhs)))
      continue;
    char enc[TP_SIDE_LEN];
    snprintf(enc, sizeof enc, "%s(%s,%s)", eqn, cl->lhs, cl->rhs);
    // --proof: the reified unit cites the ORIGINAL negative clause
    // through an equality_encoding node.  A PROV_CNF clause keeps its
    // provenance (the reification is still an encoding of that same
    // CNF clause).
    if (g_prov_on && cl->prov_kind != PROV_CNF) {
      int e = prov_eqg_new(cl);
      if (e >= 0) { cl->prov_kind = PROV_EQGOAL; cl->prov_ix = e; }
    }
    memcpy(cl->lhs, enc, strlen(enc) + 1u);
    snprintf(cl->rhs, sizeof cl->rhs, "%s", fls);
    cl->neg  = 0;
    cl->conj = 1;   // conjecture-derived: SInE keeps it unconditionally
    tp_scan_term(tp, cl->lhs);
    tp_scan_term(tp, cl->rhs);
  }
  // Reflexivity:  eq(V,V) = true  (any declared variable serves; a
  // non-ground goal guarantees at least one).
  TpClause *rfl = &tp->cls[tp->n_cls++];
  snprintf(rfl->lhs, sizeof rfl->lhs, "%s(%s,%s)",
           eqn, tp->vars[0], tp->vars[0]);
  snprintf(rfl->rhs, sizeof rfl->rhs, "%s", tru);
  rfl->neg  = 0;
  rfl->conj = 1;
  rfl->prov_kind = g_prov_on ? PROV_HELPER : PROV_NONE;
  rfl->prov_ix   = 0;
  tp_scan_term(tp, rfl->lhs);
  tp_scan_term(tp, rfl->rhs);
  // The replacement goal:  true != false.
  TpClause *gl = &tp->cls[tp->n_cls++];
  snprintf(gl->lhs, sizeof gl->lhs, "%s", tru);
  snprintf(gl->rhs, sizeof gl->rhs, "%s", fls);
  gl->neg  = 1;
  gl->conj = 1;
  gl->prov_kind = g_prov_on ? PROV_HELPER : PROV_NONE;
  gl->prov_ix   = 0;
  tp_scan_term(tp, gl->lhs);
  tp_scan_term(tp, gl->rhs);
  g_cli_equals_applied = 1;
  fprintf(stderr,
          "FindProof: equals-encoded %d existential goal(s) (%s/%s/%s)\n",
          n_nonground, eqn, tru, fls);
}

#include "atp_cli/curry.c"
#include "atp_cli/canon_input.c"
#include "atp_cli/horn_release.c"
#include "atp_cli/horn_back.c"
#include "atp_cli/fof_prepass.c"
#include "atp_cli/provenance.c"
#include "atp_cli/fof_split.c"
#include "atp_cli/szs.c"
#include "atp_cli/main.c"
