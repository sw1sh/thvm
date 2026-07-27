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

// ===================================================================
// Wide-functor currying (n-ary decomposition into binary applications)
// ===================================================================
//
// The .pr flatterm term reader holds at most 8 arguments per node
// (test_atp_wolfram_bench.c pr_term: Term kids[8]); a higher-arity
// functor (TOP052's tuple/140) is otherwise unreadable and the CLI
// gives up.  Curry each application of a wide functor f/n (n > 8) into
// a left-nested chain of a fresh BINARY symbol ap2:
//     f(t1,...,tn)  ->  ap2(ap2(...ap2(f, c1), c2)..., cn)
// where f becomes a 0-arity constant and each ci is the recursively
// curried argument.  This is a bijective term re-encoding: ap2 is a
// fresh free binary symbol carrying no equations, so two curried terms
// are equal (under congruence) iff their heads agree and every argument
// is equal -- exactly the equality the original n-ary functor induced.
// Provability is therefore unchanged; only the representation shrinks
// to arities the reader admits.  A no-op unless some functor exceeds 8,
// so it never perturbs the byte-exact WM emulation on existing corpora.
#define CURRY_ARITY_CAP 8

// Recursive-descent term rewriter over a clause-side string.  Returns a
// malloc'd rewritten term; sets *endp past the input it consumed.
static char *curry_term(const char *s, const char **endp, const char *ap2) {
  while (*s == ' ') s++;
  const char *name = s;
  size_t n = 0;
  while (isalnum((unsigned char)s[n]) || s[n] == '_') n++;
  const char *p = s + n;
  while (*p == ' ') p++;
  if (*p != '(') {                       // variable or 0-arity atom
    *endp = p;
    return strndup(name, n);
  }
  p++;                                   // consume '('
  char **args = NULL;
  int na = 0, cap = 0;
  for (;;) {
    const char *e;
    char *a = curry_term(p, &e, ap2);
    if (na >= cap) { cap = cap ? cap * 2 : 8; args = realloc(args, (size_t)cap * sizeof *args); }
    args[na++] = a;
    p = e;
    while (*p == ' ') p++;
    if (*p == ',') { p++; continue; }
    if (*p == ')') { p++; break; }
    break;                               // malformed: stop at first junk
  }
  *endp = p;
  char *out;
  if (na <= CURRY_ARITY_CAP) {           // narrow: re-emit as-is
    size_t len = n + 2;
    for (int i = 0; i < na; i++) len += strlen(args[i]) + 1;
    out = (char *)malloc(len + 1);
    size_t o = 0;
    memcpy(out + o, name, n); o += n; out[o++] = '(';
    for (int i = 0; i < na; i++) {
      if (i) out[o++] = ',';
      size_t l = strlen(args[i]); memcpy(out + o, args[i], l); o += l;
    }
    out[o++] = ')'; out[o] = '\0';
  } else {                               // wide: left-nested ap2 chain
    char *cur = strndup(name, n);        // head becomes a 0-arity atom
    size_t apl = strlen(ap2);
    for (int i = 0; i < na; i++) {
      size_t cl = strlen(cur), al = strlen(args[i]);
      size_t l = apl + 1 + cl + 1 + al + 1;
      char *nx = (char *)malloc(l + 1);
      size_t o = 0;
      memcpy(nx + o, ap2, apl); o += apl; nx[o++] = '(';
      memcpy(nx + o, cur, cl); o += cl; nx[o++] = ',';
      memcpy(nx + o, args[i], al); o += al; nx[o++] = ')'; nx[o] = '\0';
      free(cur); cur = nx;
    }
    out = cur;
  }
  for (int i = 0; i < na; i++) free(args[i]);
  free(args);
  return out;
}

static void tp_curry_wide(TpProblem *tp) {
  int wide = 0;
  for (int i = 0; i < tp->n_syms; i++)
    if (tp->syms[i].arity > CURRY_ARITY_CAP) { wide = 1; break; }
  if (!wide) return;
  char ap2[TP_NAME_LEN];
  tp_fresh_sym(tp, "ap2", ap2, sizeof ap2);
  int changed = 0;
  for (int c = 0; c < tp->n_cls; c++) {
    TpClause *cl = &tp->cls[c];
    char *sides[2] = { cl->lhs, cl->rhs };
    for (int sd = 0; sd < 2; sd++) {
      const char *e;
      char *rw = curry_term(sides[sd], &e, ap2);
      if (strcmp(rw, sides[sd]) != 0) {
        if (strlen(rw) >= TP_SIDE_LEN) { free(rw); tp_giveup("curried term exceeds side buffer"); }
        memcpy(sides[sd], rw, strlen(rw) + 1u);
        changed++;
      }
      free(rw);
    }
  }
  tp_syms_reset(tp);
  for (int i = 0; i < tp->n_cls; i++) {
    tp_scan_term(tp, tp->cls[i].lhs);
    tp_scan_term(tp, tp->cls[i].rhs);
  }
  fprintf(stderr,
          "FindProof: curried %d wide-functor side(s) via %s (bijective n-ary decomposition)\n",
          changed, ap2);
}

// ===================================================================
// Canonical input presentation (orientation + order + variable names)
// ===================================================================
//
// CASC deliberately varies how the SAME theory is presented: corpus
// releases permute clause order and flip equation sides (the TPTP 8.2
// vs 9.x MVA001-0.ax differ ONLY in those two ways, and the encompass
// config proves MVA003-1 under one presentation while timing out
// under the other; a controlled morph pinned clause ORDER as the
// discriminator).  Normalize the presentation at ingestion so the
// outcome cannot depend on it:
//   orientation  the GREATER side goes LEFT; greater = more symbol
//                occurrences (term size), ties broken toward the
//                lexicographically greater canonical clause text
//                (both candidate orientations compared under
//                clause-level first-occurrence variable renaming);
//   names        variables renamed <pfx>1, <pfx>2, ... by first
//                occurrence within their clause (whitespace dropped);
//   order        positive clauses stable-sorted by (clause size,
//                canonical text); negative units keep their slots and
//                relative order (tp_emit_pr already lists them last,
//                under CONCLUSION).
// Runs AFTER the content transforms (horn/equals/curry/SInE) and the
// capacity gates, immediately before .pr emission; the .pr bench path
// never sees it.  THVM_ATP_CANON_INPUT=0 disables (kill switch).

#define TP_CANON_MAX_CVARS 256

// Term size = number of identifier tokens (functors, constants,
// variables), the KBO weight under the emitted all-1 weight table.
static int tp_side_weight(const char *s) {
  int w = 0;
  for (const char *p = s; *p != '\0'; ) {
    if (isalnum((unsigned char)*p) || *p == '_') {
      w++;
      while (isalnum((unsigned char)*p) || *p == '_') p++;
    } else {
      p++;
    }
  }
  return w;
}

// Rewrite side `src` into `dst`, dropping whitespace and renaming each
// variable to pfx + (1 + clause-level first-occurrence index).  `map`
// carries the original names seen so far in this clause (shared across
// both sides).  pfx == NULL keeps original variable names (the
// no-safe-prefix fallback).  Returns 0 on overflow.
static int tp_canon_rewrite(const char *src, char *dst, size_t cap,
                            char (*map)[TP_NAME_LEN], int *n_map,
                            const char *pfx) {
  size_t o = 0;
  for (const char *p = src; *p != '\0'; ) {
    unsigned char c = (unsigned char)*p;
    if (isspace(c)) { p++; continue; }
    if (!(isalnum(c) || c == '_')) {
      if (o + 1 >= cap) return 0;
      dst[o++] = *p++;
      continue;
    }
    size_t n = 0;
    while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
    const char *q = p + n;
    while (isspace((unsigned char)*q)) q++;
    if (*q == '(' || !isupper(c) || pfx == NULL) {
      // functor / constant (or fallback): verbatim
      if (o + n >= cap) return 0;
      memcpy(dst + o, p, n);
      o += n;
    } else {
      int ix = -1;
      for (int i = 0; i < *n_map; i++)
        if (strlen(map[i]) == n && memcmp(map[i], p, n) == 0) { ix = i; break; }
      if (ix < 0) {
        if (*n_map >= TP_CANON_MAX_CVARS || n >= TP_NAME_LEN) return 0;
        memcpy(map[*n_map], p, n);
        map[*n_map][n] = '\0';
        ix = (*n_map)++;
      }
      char tok[TP_NAME_LEN + 16];
      int tl = snprintf(tok, sizeof tok, "%s%d", pfx, ix + 1);
      if (o + (size_t)tl >= cap) return 0;
      memcpy(dst + o, tok, (size_t)tl);
      o += (size_t)tl;
    }
    p += n;
  }
  dst[o] = '\0';
  return 1;
}

// A printable variable prefix that cannot collide with the signature:
// no symbol may read as prefix + digits.  (An uppercase-initial name
// only enters the symbol table as a FUNCTOR -- tp_scan_term classifies
// uppercase non-functors as variables -- so collisions are near
// impossible; the scan is a guard.)
static const char *tp_canon_var_prefix(const TpProblem *tp) {
  static const char *cands[] = {"X", "Y", "Z", "V", "W", "U"};
  for (size_t c = 0; c < sizeof cands / sizeof cands[0]; c++) {
    size_t pl = strlen(cands[c]);
    int clash = 0;
    for (int i = 0; i < tp->n_syms && !clash; i++) {
      const char *nm = tp->syms[i].name;
      if (strncmp(nm, cands[c], pl) != 0 || nm[pl] == '\0') continue;
      clash = 1;
      for (const char *q = nm + pl; *q != '\0'; q++)
        if (!isdigit((unsigned char)*q)) { clash = 0; break; }
    }
    if (!clash) return cands[c];
  }
  return NULL;
}

// Canonicalize one clause in place: pick the orientation (greater side
// left), then rewrite both sides with clause-level variable renaming.
static void tp_canon_clause(TpClause *cl, const char *pfx) {
  static char ka[4 * TP_SIDE_LEN], kb[4 * TP_SIDE_LEN];
  char map[TP_CANON_MAX_CVARS][TP_NAME_LEN];
  int wl = tp_side_weight(cl->lhs), wr = tp_side_weight(cl->rhs);
  int flip = 0;
  if (wl < wr) flip = 1;
  else if (wl == wr) {
    // weight tie: compare the two candidate orientations as full
    // canonical clause texts ("lhs=rhs", variables renamed by clause
    // first occurrence to \x01<i> -- a token no TPTP identifier can
    // contain).  Per-side comparison is NOT enough: for sides with
    // identical skeletons (f(X,Y,Z) = f(Y,Z,X)) only the clause-level
    // renaming distinguishes the orientations consistently across
    // presentations.
    int nm = 0;
    size_t half = sizeof ka / 2;
    if (tp_canon_rewrite(cl->lhs, ka, half, map, &nm, "\x01")) {
      size_t la = strlen(ka);
      ka[la++] = '=';
      if (tp_canon_rewrite(cl->rhs, ka + la, sizeof ka - la, map, &nm, "\x01")) {
        nm = 0;
        if (tp_canon_rewrite(cl->rhs, kb, half, map, &nm, "\x01")) {
          size_t lb = strlen(kb);
          kb[lb++] = '=';
          if (tp_canon_rewrite(cl->lhs, kb + lb, sizeof kb - lb, map, &nm, "\x01"))
            flip = (strcmp(ka, kb) < 0);
        }
      }
    }
  }
  int nm = 0;
  const char *nl_src = flip ? cl->rhs : cl->lhs;
  const char *nr_src = flip ? cl->lhs : cl->rhs;
  if (!tp_canon_rewrite(nl_src, ka, sizeof ka, map, &nm, pfx) ||
      !tp_canon_rewrite(nr_src, kb, sizeof kb, map, &nm, pfx))
    tp_giveup("canonical clause exceeds rewrite bound");
  if (strlen(ka) >= TP_SIDE_LEN || strlen(kb) >= TP_SIDE_LEN)
    tp_giveup("canonical term exceeds side-length bound");
  memcpy(cl->lhs, ka, strlen(ka) + 1u);
  memcpy(cl->rhs, kb, strlen(kb) + 1u);
}

typedef struct { int w; int ix; } TpCanonKey;
static TpProblem *g_canon_tp = NULL;
static int tp_canon_cmp(const void *a, const void *b) {
  const TpCanonKey *x = (const TpCanonKey *)a, *y = (const TpCanonKey *)b;
  if (x->w != y->w) return (x->w < y->w) ? -1 : 1;
  const TpClause *cx = &g_canon_tp->cls[x->ix], *cy = &g_canon_tp->cls[y->ix];
  int c = strcmp(cx->lhs, cy->lhs);
  if (c != 0) return c;
  c = strcmp(cx->rhs, cy->rhs);
  if (c != 0) return c;
  // original position: a total tie-break makes qsort's ordering
  // deterministic (stable) regardless of libc pivot choices
  return (x->ix < y->ix) ? -1 : (x->ix > y->ix);
}

static void tp_canon_input(TpProblem *tp) {
  const char *pfx = tp_canon_var_prefix(tp);
  for (int i = 0; i < tp->n_cls; i++) tp_canon_clause(&tp->cls[i], pfx);
  int n_pos = 0;
  int      *pos  = (int *)malloc(sizeof(int) * (size_t)(tp->n_cls + 1));
  TpCanonKey *keys = (TpCanonKey *)malloc(sizeof(TpCanonKey) * (size_t)(tp->n_cls + 1));
  if (pos == NULL || keys == NULL) tp_giveup("OOM in canonical ordering");
  for (int i = 0; i < tp->n_cls; i++) {
    if (tp->cls[i].neg) continue;
    pos[n_pos] = i;
    keys[n_pos].ix = i;
    keys[n_pos].w  = tp_side_weight(tp->cls[i].lhs) + tp_side_weight(tp->cls[i].rhs);
    n_pos++;
  }
  g_canon_tp = tp;
  qsort(keys, (size_t)n_pos, sizeof(TpCanonKey), tp_canon_cmp);
  TpClause *tmp = (TpClause *)malloc(sizeof(TpClause) * (size_t)(n_pos + 1));
  if (tmp == NULL) tp_giveup("OOM in canonical ordering");
  for (int k = 0; k < n_pos; k++) tmp[k] = tp->cls[keys[k].ix];
  for (int k = 0; k < n_pos; k++) tp->cls[pos[k]] = tmp[k];
  free(tmp); free(keys); free(pos);
  // Rebuild signature + variable list from the canonical text in
  // canonical order (the VARIABLES line follows first-seen order).
  tp_syms_reset(tp);
  for (int i = 0; i < tp->n_cls; i++) {
    tp_scan_term(tp, tp->cls[i].lhs);
    tp_scan_term(tp, tp->cls[i].rhs);
  }
}

// ===================================================================
// Encoded-Horn (ifeq) decode + forward-chaining release
// ===================================================================
//
// TPTP's -10 UEQ problems encode Horn logic as unit equations: a
// dispatcher axiom ifeqN(X,X,Y,Z) = Y (several functor names may
// coexist: ifeq, ifeq2, ...; arity-3 variants ifeq(X,X,Y) = Y too)
// plus conditional clauses ifeqN(c, t, then, e) = e -- "if c = t
// then then = e" -- nesting in the then-position for multiple
// conditions, with propositions as terms equated to the constant
// true.  Pure completion drowns re-deriving the conditional
// machinery, so this pass decodes the encoding and runs a budgeted
// forward-chaining release loop (positive hyper-resolution): whenever
// every condition of a decoded clause simultaneously unifies with
// known unit facts (each fact renamed apart), the instantiated
// conclusion is released as a new unit equation.  Released units are
// logical consequences of the input, so Unsatisfiable stays valid;
// g_cli_horn_applied gates the Satisfiable claim exactly like
// g_cli_sine_applied does.  THVM_ATP_HORN=0 disables; otherwise the
// pass is a no-op unless a dispatcher axiom is detected.

static int g_cli_horn_applied = 0;
static int g_prov_on;   // --proof lineage capture (provenance module below)

// Terms live in a growable node/arg arena.  head >= 0 is an index
// into the TpProblem symbol table; head < 0 is variable -(v+1) with v
// local to the owning clause.  Unification runs over (node, var-space
// offset) pairs: the rule's variables are offset 0, the fact renamed
// into condition i's space at offset (i+1)*HORN_VSPACE.
#define HORN_MAX_COND   8
#define HORN_VSPACE     64
// Var spaces: rule 0, condition facts 1..HORN_MAX_COND, subgoal
// patterns HORN_SG_OFF, probe side HORN_PROBE_OFF (a clean space for
// unifying a canonical release against a subgoal while a join's
// bindings are still live in spaces 0..HORN_MAX_COND).
#define HORN_SG_OFF     ((HORN_MAX_COND + 1) * HORN_VSPACE)
#define HORN_PROBE_OFF  ((HORN_MAX_COND + 2) * HORN_VSPACE)
#define HORN_NSLOTS     ((HORN_MAX_COND + 3) * HORN_VSPACE)
#define HORN_MAX_RELEASES 400000
#define HORN_MAX_ATTEMPTS 10000000L
#define HORN_WALL_SEC   2.5
#define HORN_SIDE_MAX   900     // printed chars per released side
#define HORN_APPLY_MAX  400     // nodes per released side
#define HORN_REL_VARS   12      // distinct vars per released unit

typedef struct { int head, nargs, argp; } HNode;
static HNode *g_hn = NULL; static int g_hn_n = 0, g_hn_cap = 0;
static int   *g_ha = NULL; static int g_ha_n = 0, g_ha_cap = 0;

static int horn_node(int head, int nargs, const int *args) {
  if (g_hn_n >= g_hn_cap) {
    g_hn_cap = (g_hn_cap == 0) ? (1 << 16) : g_hn_cap * 2;
    g_hn = (HNode *)realloc(g_hn, (size_t)g_hn_cap * sizeof(HNode));
  }
  if (nargs > 0 && g_ha_n + nargs > g_ha_cap) {
    g_ha_cap = (g_ha_cap == 0) ? (1 << 16) : g_ha_cap * 2 + nargs;
    g_ha = (int *)realloc(g_ha, (size_t)g_ha_cap * sizeof(int));
  }
  if (g_hn == NULL || (nargs > 0 && g_ha == NULL)) tp_giveup("OOM in Horn release");
  HNode *nd = &g_hn[g_hn_n];
  nd->head = head; nd->nargs = nargs; nd->argp = g_ha_n;
  for (int i = 0; i < nargs; i++) g_ha[g_ha_n + i] = args[i];
  g_ha_n += nargs;
  return g_hn_n++;
}
#define HARG(t, i) (g_ha[g_hn[(t)].argp + (i)])

// --- per-clause recursive-descent parser ----------------------------
typedef struct {
  const char *p;
  int  nv;
  char vnames[HORN_VSPACE][TP_NAME_LEN];
  int  fail;
} HParse;

static int horn_parse_term(HParse *ps, TpProblem *tp, int depth) {
  if (ps->fail || depth > 200) { ps->fail = 1; return -1; }
  while (isspace((unsigned char)*ps->p)) ps->p++;
  const char *name = ps->p;
  size_t n = 0;
  while (isalnum((unsigned char)name[n]) || name[n] == '_') n++;
  if (n == 0) { ps->fail = 1; return -1; }
  ps->p += n;
  while (isspace((unsigned char)*ps->p)) ps->p++;
  if (*ps->p == '(') {
    ps->p++;
    int args[16], na = 0;
    for (;;) {
      if (na >= 16) { ps->fail = 1; return -1; }
      args[na++] = horn_parse_term(ps, tp, depth + 1);
      if (ps->fail) return -1;
      while (isspace((unsigned char)*ps->p)) ps->p++;
      if (*ps->p == ',') { ps->p++; continue; }
      if (*ps->p == ')') { ps->p++; break; }
      ps->fail = 1; return -1;
    }
    return horn_node(tp_sym_note_ix(tp, name, n, na), na, args);
  }
  if (isupper((unsigned char)*name)) {
    if (n >= TP_NAME_LEN) n = TP_NAME_LEN - 1;
    for (int i = 0; i < ps->nv; i++)
      if (strlen(ps->vnames[i]) == n && memcmp(ps->vnames[i], name, n) == 0)
        return horn_node(-(i + 1), 0, NULL);
    if (ps->nv >= HORN_VSPACE) { ps->fail = 1; return -1; }
    memcpy(ps->vnames[ps->nv], name, n);
    ps->vnames[ps->nv][n] = '\0';
    return horn_node(-(++ps->nv), 0, NULL);
  }
  return horn_node(tp_sym_note_ix(tp, name, n, 0), 0, NULL);
}

// Parse one clause side into the arena; vars are clause-local via ps.
static int horn_parse_side(TpProblem *tp, HParse *ps, const char *s) {
  ps->p = s; ps->fail = 0;
  int t = horn_parse_term(ps, tp, 0);
  if (ps->fail) return -1;
  while (isspace((unsigned char)*ps->p)) ps->p++;
  return (*ps->p == '\0') ? t : -1;
}

// --- structural equality (same var space) ---------------------------
static int horn_eq(int a, int b) {
  if (g_hn[a].head != g_hn[b].head || g_hn[a].nargs != g_hn[b].nargs) return 0;
  for (int i = 0; i < g_hn[a].nargs; i++)
    if (!horn_eq(HARG(a, i), HARG(b, i))) return 0;
  return 1;
}

// --- unification with trail ------------------------------------------
typedef struct { int t, off; } HBind;
static HBind g_hb[HORN_NSLOTS];
static int g_htrail[HORN_NSLOTS]; static int g_htrail_n = 0;
static long g_horn_attempts = 0;

static void horn_deref(int *t, int *off) {
  while (g_hn[*t].head < 0) {
    int slot = *off + (-g_hn[*t].head - 1);
    if (g_hb[slot].t < 0) return;
    int nt = g_hb[slot].t, noff = g_hb[slot].off;
    *t = nt; *off = noff;
  }
}

static int horn_occurs(int slot, int t, int off, int depth) {
  if (depth > 400) return 1;   // treat overdeep as occurs: fail the bind
  horn_deref(&t, &off);
  if (g_hn[t].head < 0) return off + (-g_hn[t].head - 1) == slot;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_occurs(slot, HARG(t, i), off, depth + 1)) return 1;
  return 0;
}

static int horn_unify(int t1, int o1, int t2, int o2, int depth) {
  if (depth > 400) return 0;
  horn_deref(&t1, &o1); horn_deref(&t2, &o2);
  if (g_hn[t1].head < 0 && g_hn[t2].head < 0 &&
      o1 + (-g_hn[t1].head - 1) == o2 + (-g_hn[t2].head - 1)) return 1;
  if (g_hn[t1].head < 0) {
    int slot = o1 + (-g_hn[t1].head - 1);
    if (horn_occurs(slot, t2, o2, 0)) return 0;
    g_hb[slot].t = t2; g_hb[slot].off = o2;
    g_htrail[g_htrail_n++] = slot;
    return 1;
  }
  if (g_hn[t2].head < 0) {
    int slot = o2 + (-g_hn[t2].head - 1);
    if (horn_occurs(slot, t1, o1, 0)) return 0;
    g_hb[slot].t = t1; g_hb[slot].off = o1;
    g_htrail[g_htrail_n++] = slot;
    return 1;
  }
  if (g_hn[t1].head != g_hn[t2].head || g_hn[t1].nargs != g_hn[t2].nargs) return 0;
  for (int i = 0; i < g_hn[t1].nargs; i++)
    if (!horn_unify(HARG(t1, i), o1, HARG(t2, i), o2, depth + 1)) return 0;
  return 1;
}

static void horn_undo(int mark) {
  while (g_htrail_n > mark) g_hb[g_htrail[--g_htrail_n]].t = -1;
}

// --- substitution application with canonical var renaming ------------
// Unbound slots map to fresh canonical vars V0, V1, ... in traversal
// order; the map is reset per release via the touched list.
static int g_can_map[HORN_NSLOTS];
static int g_can_touch[HORN_NSLOTS]; static int g_can_touch_n = 0, g_can_nv = 0;

static void horn_can_reset(void) {
  while (g_can_touch_n > 0) g_can_map[g_can_touch[--g_can_touch_n]] = -1;
  g_can_nv = 0;
}

static int horn_apply(int t, int off, int *budget) {
  if (--*budget < 0) return -1;
  horn_deref(&t, &off);
  if (g_hn[t].head < 0) {
    int slot = off + (-g_hn[t].head - 1);
    if (g_can_map[slot] < 0) {
      if (g_can_nv >= HORN_REL_VARS) return -1;
      g_can_map[slot] = g_can_nv++;
      g_can_touch[g_can_touch_n++] = slot;
    }
    return horn_node(-(g_can_map[slot] + 1), 0, NULL);
  }
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_apply(HARG(t, i), off, budget);
    if (args[i] < 0) return -1;
  }
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

// --- printing (canonical, offset-free terms) --------------------------
// Variables are renumbered by first occurrence in the printed order via
// remap (size HORN_REL_VARS, -1-initialized, shared across the sides of
// one equation) -- so an equation and its mirror print to comparable
// dedup keys.
static int horn_print(const TpProblem *tp, char *buf, size_t cap, size_t *o,
                      int t, int *remap, int *nv) {
  if (g_hn[t].head < 0) {
    int v = -g_hn[t].head - 1;
    if (v >= HORN_REL_VARS) return 0;
    if (remap[v] < 0) remap[v] = (*nv)++;
    if (*o + 8 >= cap) return 0;
    *o += (size_t)snprintf(buf + *o, cap - *o, "V%d", remap[v]);
    return 1;
  }
  const char *name = tp->syms[g_hn[t].head].name;
  size_t nl = strlen(name);
  if (*o + nl + 2 >= cap) return 0;
  memcpy(buf + *o, name, nl); *o += nl;
  if (g_hn[t].nargs > 0) {
    buf[(*o)++] = '(';
    for (int i = 0; i < g_hn[t].nargs; i++) {
      if (i > 0) { if (*o + 1 >= cap) return 0; buf[(*o)++] = ','; }
      if (!horn_print(tp, buf, cap, o, HARG(t, i), remap, nv)) return 0;
    }
    if (*o + 1 >= cap) return 0;
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
  return 1;
}

// --- dedup set over canonical "lhs=rhs" strings ------------------------
// Sized 2x the release cap (two keys per release: itself + mirror) so
// the open-addressing probe never wraps a full table.  The CD lane
// allocates 4x that: its wall-long runs push several million accepts,
// and a saturated table silently rejects every later candidate as a
// duplicate.
#define HORN_DEDUP_SLOTS (1 << 21)
static char **g_hdup = NULL;
static int g_hdup_n = 0;
static u32 g_hdup_slots = HORN_DEDUP_SLOTS;

static int horn_dedup_add(const char *key) {   // 1 = new
  if ((u32)g_hdup_n >= g_hdup_slots - (g_hdup_slots >> 2)) return 0;
  u64 h = tp_name_hash(key, strlen(key));
  u32 slot = (u32)(h % (u64)g_hdup_slots);
  for (;;) {
    if (g_hdup[slot] == NULL) break;
    if (strcmp(g_hdup[slot], key) == 0) return 0;
    slot = (slot + 1u) % g_hdup_slots;
  }
  g_hdup[slot] = strdup(key);
  g_hdup_n++;
  return 1;
}

// --- facts, rules, head index ------------------------------------------
// src (--proof provenance): where this fact came from -- a release
// index (>= 0), an input-clause snapshot (~g_hpc index, < 0), or
// HORN_SRC_NONE when capture is off / unavailable.
#define HORN_SRC_NONE (-(1 << 30))
typedef struct {
  int l, r, nv, size, src;
  u8 processed, sghit, rel, conj, given, dead;
} HFact;
static HFact *g_hf = NULL; static int g_hf_n = 0, g_hf_cap = 0;

static int horn_size(int t) {
  int s = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) s += horn_size(HARG(t, i));
  return s;
}

// --- condensed-detachment lane (--horn-cd) ----------------------------
// The LCL condensed-detachment family regresses the blind v1 chaining
// into promiscuous major-premise towers (weakening instances or(V, t)
// growing without bound) and an instance flood no exact dedup catches.
// The classic CD prover discipline (Otter on the LCL corpus) is:
//   - a WEIGHT CAP on released conclusions, raised by ITERATIVE
//     DEEPENING when the capped space saturates without the goal
//     (kills the towers);
//   - GENERALIZATION SUBSUMPTION: a released theorem(t) is redundant
//     if a kept theorem(s) matches onto it (forward), and a kept
//     instance is dead weight once a more general release arrives
//     (backward, done lazily at pop time here);
//   - pure smallest-first given selection.
// All of it is gated on g_horn_cd so every other flavor's trajectory
// stays byte-identical.
static int  g_horn_cd = 0;             // THVM_ATP_HORN_CD (2 = slice mode)
static double g_cli_wall_sec = 120.0;  // engine wall, for the CD lane budget
static double g_cli_main_t0  = 0.0;    // process start (main); wall-remaining anchor
// Parsed negative units (the goals), for the subsumption rank check
// and the CD resonators.
static int g_hg_l[64], g_hg_r[64]; static int g_hg_n = 0;
static int  g_cd_cap = 0;              // live released-size cap
static int  g_cd_step = 4;             // THVM_ATP_HORN_CD_STEP
static int  g_cd_maxvars = 5;          // THVM_ATP_HORN_CD_VARS
static int  g_cd_overcap = 0;          // some conclusion exceeded the cap
static int  g_cd_rounds = 0;
static long g_cd_fsub = 0, g_cd_bsub = 0, g_cd_reso_hits = 0;

// One-way match: pattern variables live in the HORN_SG_OFF slot space
// and bind to canonical (offset-free) instance subterms; instance
// variables are rigid constants.  Bindings ride the shared trail (the
// caller undoes to its mark).  A bound pattern variable re-encountered
// recurses with po = 0, where this reduces to structural equality of
// two canonical instance terms -- instance slots are never bound.
static int horn_cd_match(int p, int po, int t, int depth) {
  if (depth > 400) return 0;
  if (g_hn[p].head < 0) {
    if (po == HORN_SG_OFF) {
      int slot = po + (-g_hn[p].head - 1);
      if (g_hb[slot].t >= 0)
        return horn_cd_match(g_hb[slot].t, g_hb[slot].off, t, depth + 1);
      g_hb[slot].t = t; g_hb[slot].off = 0;
      g_htrail[g_htrail_n++] = slot;
      return 1;
    }
    return g_hn[t].head == g_hn[p].head;   // instance var vs instance var
  }
  if (g_hn[t].head != g_hn[p].head || g_hn[t].nargs != g_hn[p].nargs) return 0;
  for (int i = 0; i < g_hn[p].nargs; i++)
    if (!horn_cd_match(HARG(p, i), po, HARG(t, i), depth + 1)) return 0;
  return 1;
}

// --- discrimination tree over pattern sides (generalization retrieval)
// The subsumers of a candidate are found by walking its l side in
// preorder against a tree of pattern sides: a pattern variable keys as
// STAR (-1) and consumes one whole candidate subterm on lookup, so the
// visit cost is bounded by the shared-prefix structure instead of the
// fact count (a flat scan spent 89% of the wall here on LCL195-10 --
// every CD fact shares the theorem(..) = true shape, so no root filter
// discriminates).  BOTH sides of every pattern fact are inserted
// (orient 0 = l, 1 = r): one query on the candidate's l covers both
// subsumption pairings, and the leaf match validates the full equation
// with variable consistency.  Fully ground facts are never inserted --
// after dedup they cannot strictly subsume anything.
typedef struct { int key, child, next; } DtEdge;
typedef struct { int edge0, leaf0; } DtNode;
typedef struct { int fact, orient, next; } DtLeaf;
static DtNode *g_dtn = NULL; static int g_dtn_n = 0, g_dtn_cap = 0;
static DtEdge *g_dte = NULL; static int g_dte_n = 0, g_dte_cap = 0;
static DtLeaf *g_dtl = NULL; static int g_dtl_n = 0, g_dtl_cap = 0;

static int dt_node_new(void) {
  if (g_dtn_n >= g_dtn_cap) {
    g_dtn_cap = (g_dtn_cap == 0) ? (1 << 12) : g_dtn_cap * 2;
    g_dtn = (DtNode *)realloc(g_dtn, (size_t)g_dtn_cap * sizeof(DtNode));
    if (g_dtn == NULL) tp_giveup("OOM in Horn release");
  }
  g_dtn[g_dtn_n].edge0 = -1;
  g_dtn[g_dtn_n].leaf0 = -1;
  return g_dtn_n++;
}

static int dt_child(int node, int key, int create) {
  for (int e = g_dtn[node].edge0; e >= 0; e = g_dte[e].next)
    if (g_dte[e].key == key) return g_dte[e].child;
  if (!create) return -1;
  int c = dt_node_new();
  if (g_dte_n >= g_dte_cap) {
    g_dte_cap = (g_dte_cap == 0) ? (1 << 12) : g_dte_cap * 2;
    g_dte = (DtEdge *)realloc(g_dte, (size_t)g_dte_cap * sizeof(DtEdge));
    if (g_dte == NULL) tp_giveup("OOM in Horn release");
  }
  g_dte[g_dte_n].key = key;
  g_dte[g_dte_n].child = c;
  g_dte[g_dte_n].next = g_dtn[node].edge0;
  g_dtn[node].edge0 = g_dte_n++;
  return c;
}

static void dt_insert(int t, int fact, int orient) {
  if (g_dtn_n == 0) dt_node_new();   // root
  int stack[512], sp = 0, node = 0;
  stack[sp++] = t;
  while (sp > 0) {
    int cur = stack[--sp];
    int key = (g_hn[cur].head < 0) ? -1 : g_hn[cur].head;
    node = dt_child(node, key, 1);
    if (key >= 0) {
      // A truncated walk would file the fact under a WRONG (shorter)
      // path and let it subsume non-instances: skip oversized patterns
      // entirely (they are also beyond the query flattener's bound).
      if (sp + g_hn[cur].nargs > 500) return;
      for (int i = g_hn[cur].nargs - 1; i >= 0; i--)
        stack[sp++] = HARG(cur, i);
    }
  }
  if (g_dtl_n >= g_dtl_cap) {
    g_dtl_cap = (g_dtl_cap == 0) ? (1 << 12) : g_dtl_cap * 2;
    g_dtl = (DtLeaf *)realloc(g_dtl, (size_t)g_dtl_cap * sizeof(DtLeaf));
    if (g_dtl == NULL) tp_giveup("OOM in Horn release");
  }
  g_dtl[g_dtl_n].fact = fact;
  g_dtl[g_dtl_n].orient = orient;
  g_dtl[g_dtl_n].next = g_dtn[node].leaf0;
  g_dtn[node].leaf0 = g_dtl_n++;
}

// Query state: the candidate's l side, flattened to preorder with
// per-position subterm extents (STAR skips).
static int g_dt_flat[512], g_dt_skipn[512], g_dt_flat_n = 0, g_dt_over = 0;
static void dt_flatten(int t) {
  int start = g_dt_flat_n;
  if (g_dt_flat_n >= 500) { g_dt_over = 1; return; }
  g_dt_flat[g_dt_flat_n++] = t;
  if (g_hn[t].head >= 0)
    for (int i = 0; i < g_hn[t].nargs && !g_dt_over; i++)
      dt_flatten(HARG(t, i));
  g_dt_skipn[start] = g_dt_flat_n - start;
}

// Walk the tree along the flattened candidate: exact-symbol edge, or a
// STAR edge that swallows the subterm.  Complete terms are prefix-free
// in preorder (fixed arities), so leaves are only ever checked when
// the candidate is fully consumed.  A leaf hit still runs the full
// two-sided match (variable consistency, the r side, orientation).
static int dt_probe(int node, int pos, int cl, int cr, int sz_max,
                    int min_idx) {
  if (pos >= g_dt_flat_n) {
    for (int L = g_dtn[node].leaf0; L >= 0; L = g_dtl[L].next) {
      int f = g_dtl[L].fact;
      if (g_hf[f].dead || g_hf[f].size > sz_max || f < min_idx) continue;
      int pl = g_dtl[L].orient ? g_hf[f].r : g_hf[f].l;
      int pr = g_dtl[L].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      int hit = horn_cd_match(pl, HORN_SG_OFF, cl, 0) &&
                horn_cd_match(pr, HORN_SG_OFF, cr, 0);
      horn_undo(mark);
      if (hit) return 1;
    }
    return 0;
  }
  int t = g_dt_flat[pos], c;
  if (g_hn[t].head >= 0 && (c = dt_child(node, g_hn[t].head, 0)) >= 0 &&
      dt_probe(c, pos + 1, cl, cr, sz_max, min_idx)) return 1;
  if ((c = dt_child(node, -1, 0)) >= 0 &&
      dt_probe(c, pos + g_dt_skipn[pos], cl, cr, sz_max, min_idx)) return 1;
  return 0;
}

// Forward subsumption: is the candidate (l, r) an instance of a live
// fact?  Every indexed fact predates the candidate, so no age filter.
static int horn_cd_fwd_subsumed(int l, int r, int sz) {
  if (g_dtn_n == 0) return 0;
  g_dt_flat_n = 0; g_dt_over = 0;
  dt_flatten(l);
  if (g_dt_over) return 0;
  return dt_probe(0, 0, l, r, sz, 0);
}

// --- resonance (Wos): goal-shape steering by skeleton MATCH ------------
// The backward cone on a CD goal regresses only into the promiscuous
// major-premise tower theorem(or(not(V0), .. G)), and probing releases
// by UNIFICATION lets every var-tailed weakening hit it (measured: 11k
// of 19k releases "sg-hit" on LCL195-10).  What is selective is the
// goal's SHAPE: each deep-enough goal subterm, with its leaves
// wildcarded to fresh pattern variables, becomes a one-way MATCH
// pattern.  A release that matches structurally refines the goal
// (Wos's resonators, generated from the goal instead of known proofs):
//   - the weakening tower or(V, ..) fails at the pattern's rigid
//     not(*) position;
//   - the deep PM intermediates (a hypothetical-syllogism instance is
//     itself goal-shaped) match, earning the cap exemption and the
//     priority pop that pure smallest-first would never grant them.
#define CD_RESO_MAX       32
#define CD_RESO_MIN_RIGID 5
static int g_cd_reso[CD_RESO_MAX]; static int g_cd_reso_n = 0;

static int horn_cd_skel_rigids(int t) {   // non-nullary heads
  if (g_hn[t].head < 0 || g_hn[t].nargs == 0) return 0;
  int n = 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    n += horn_cd_skel_rigids(HARG(t, i));
  return n;
}

// Copy t with every leaf (variable or nullary symbol) replaced by a
// fresh pattern variable; -1 when the variable space is exhausted.
static int horn_cd_skel(int t, int *nv) {
  if (g_hn[t].head < 0 || g_hn[t].nargs == 0) {
    if (*nv >= HORN_VSPACE) return -1;
    return horn_node(-(++*nv), 0, NULL);
  }
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_cd_skel(HARG(t, i), nv);
    if (args[i] < 0) return -1;
  }
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

static void horn_cd_reso_build(void) {
  g_cd_reso_n = 0;
  for (int g = 0; g < g_hg_n; g++) {
    int stack[512], sp = 0;
    stack[sp++] = g_hg_l[g]; stack[sp++] = g_hg_r[g];
    while (sp > 0 && g_cd_reso_n < CD_RESO_MAX) {
      int t = stack[--sp];
      if (g_hn[t].head >= 0)
        for (int i = 0; i < g_hn[t].nargs && sp < 500; i++)
          stack[sp++] = HARG(t, i);
      if (horn_cd_skel_rigids(t) < CD_RESO_MIN_RIGID) continue;
      int nv = 0;
      int p = horn_cd_skel(t, &nv);
      if (p < 0) continue;
      int dup = 0;
      for (int i = 0; i < g_cd_reso_n && !dup; i++)
        dup = horn_eq(g_cd_reso[i], p);
      if (!dup) g_cd_reso[g_cd_reso_n++] = p;
    }
  }
}

// Does either side of the release structurally refine a goal skeleton?
// (Strict one-way match: pattern wildcards are only the skeleton's own
// leaf variables.  A linked-instance-variable tolerance was tried and
// measured to flood -- 395k of 400k releases "resonant" on LCL195-10,
// whose 1.6-family goal shape saturates the whole detachment space.)
static int horn_cd_resonant(int l, int r) {
  int hit = 0;
  for (int i = 0; i < g_cd_reso_n && !hit; i++) {
    for (int side = 0; side < 2 && !hit; side++) {
      int t = side ? r : l;
      int mark = g_htrail_n;
      hit = horn_cd_match(g_cd_reso[i], HORN_SG_OFF, t, 0);
      horn_undo(mark);
    }
  }
  return hit;
}

// Lazy back subsumption at pop time: only facts added AFTER f can
// subsume it (older generals rejected f at its own emit).
static int horn_cd_pop_subsumed(int f) {
  if (g_dtn_n == 0) return 0;
  g_dt_flat_n = 0; g_dt_over = 0;
  dt_flatten(g_hf[f].l);
  if (g_dt_over) return 0;
  return dt_probe(0, 0, g_hf[f].l, g_hf[f].r, g_hf[f].size, f + 1);
}

// Min-heaps over unprocessed fact indices, keyed by term size
// (smallest first, ties by insertion order).  g_hheap is the main
// given-fact agenda; g_hpheap is the goal-directed priority agenda
// (facts that unify with a backward-cone subgoal), popped first.
typedef struct { int *a; int n, cap; } HHeap;
static HHeap g_hheap  = {NULL, 0, 0};
static HHeap g_hpheap = {NULL, 0, 0};
static int horn_heap_lt(int a, int b) {
  if (g_hf[a].size != g_hf[b].size) return g_hf[a].size < g_hf[b].size;
  return a < b;
}
static void horn_heap_push(HHeap *h, int f) {
  if (h->n >= h->cap) {
    h->cap = (h->cap == 0) ? (1 << 12) : h->cap * 2;
    h->a = (int *)realloc(h->a, (size_t)h->cap * sizeof(int));
    if (h->a == NULL) tp_giveup("OOM in Horn release");
  }
  int i = h->n++;
  h->a[i] = f;
  while (i > 0) {
    int p = (i - 1) / 2;
    if (!horn_heap_lt(h->a[i], h->a[p])) break;
    int t = h->a[i]; h->a[i] = h->a[p]; h->a[p] = t;
    i = p;
  }
}
static int horn_heap_pop(HHeap *h) {
  if (h->n == 0) return -1;
  int top = h->a[0];
  h->a[0] = h->a[--h->n];
  int i = 0;
  for (;;) {
    int l = 2 * i + 1, r = l + 1, m = i;
    if (l < h->n && horn_heap_lt(h->a[l], h->a[m])) m = l;
    if (r < h->n && horn_heap_lt(h->a[r], h->a[m])) m = r;
    if (m == i) break;
    int t = h->a[i]; h->a[i] = h->a[m]; h->a[m] = t;
    i = m;
  }
  return top;
}

// Head index: per symbol, a chain of (fact, orient) entries; orient 0
// matches conditions against (l, r), orient 1 against (r, l).  A fact
// side headed by a variable goes on the var-head chain (matches any
// condition head).
typedef struct { int fact, orient, next; } HIdxE;
static HIdxE *g_hix = NULL; static int g_hix_n = 0, g_hix_cap = 0;
static int *g_hix_head = NULL;   // per symbol id
static int g_hix_varhead = -1;

static void horn_index_entry(int head, int fact, int orient) {
  if (g_hix_n >= g_hix_cap) {
    g_hix_cap = (g_hix_cap == 0) ? (1 << 14) : g_hix_cap * 2;
    g_hix = (HIdxE *)realloc(g_hix, (size_t)g_hix_cap * sizeof(HIdxE));
    if (g_hix == NULL) tp_giveup("OOM in Horn release");
  }
  HIdxE *e = &g_hix[g_hix_n];
  e->fact = fact; e->orient = orient;
  if (head < 0) { e->next = g_hix_varhead; g_hix_varhead = g_hix_n; }
  else          { e->next = g_hix_head[head]; g_hix_head[head] = g_hix_n; }
  g_hix_n++;
}

static void horn_fact_add(int l, int r, int nv) {
  if (g_hf_n >= g_hf_cap) {
    g_hf_cap = (g_hf_cap == 0) ? (1 << 12) : g_hf_cap * 2;
    g_hf = (HFact *)realloc(g_hf, (size_t)g_hf_cap * sizeof(HFact));
    if (g_hf == NULL) tp_giveup("OOM in Horn release");
  }
  // CD lane: index the new fact as a subsumption pattern (both sides).
  if (g_horn_cd && nv > 0) {
    dt_insert(l, g_hf_n, 0);
    dt_insert(r, g_hf_n, 1);
  }
  g_hf[g_hf_n].l = l; g_hf[g_hf_n].r = r; g_hf[g_hf_n].nv = nv;
  g_hf[g_hf_n].src = HORN_SRC_NONE;
  g_hf[g_hf_n].size = horn_size(l) + horn_size(r);
  g_hf[g_hf_n].processed = 0;
  g_hf[g_hf_n].sghit = 0;
  g_hf[g_hf_n].rel = 1;
  g_hf[g_hf_n].conj = 0;
  g_hf[g_hf_n].given = 0;
  g_hf[g_hf_n].dead = 0;
  horn_index_entry(g_hn[l].head, g_hf_n, 0);
  horn_index_entry(g_hn[r].head, g_hf_n, 1);
  horn_heap_push(&g_hheap, g_hf_n);
  g_hf_n++;
}

typedef struct {
  int cl[HORN_MAX_COND], ct[HORN_MAX_COND];
  int nc, concl_l, concl_r, nv;
  int hpc;                // --proof: g_hpc snapshot of the rule clause (-1 off)
  u8 rel, conj, ghrel;    // ghrel: admitted by the goalward head closure
} HRule;
static HRule *g_hr = NULL; static int g_hr_n = 0, g_hr_cap = 0;

// Rule-condition index: (rule, condition position) entries bucketed by
// the condition's head symbol, so a given fact only visits the rules
// that can bind it (the CSR knowledge bases carry ~5k rules; scanning
// them all per given burns the whole budget before the goal chain).
typedef struct { int rule, cpos, next; } HRIdxE;
static HRIdxE *g_hrix = NULL; static int g_hrix_n = 0, g_hrix_cap = 0;
static int *g_hrix_head = NULL;   // per symbol id
static int g_hrix_varhead = -1;

static void horn_rule_index(int rule, int cpos, int head) {
  if (g_hrix_n >= g_hrix_cap) {
    g_hrix_cap = (g_hrix_cap == 0) ? (1 << 12) : g_hrix_cap * 2;
    g_hrix = (HRIdxE *)realloc(g_hrix, (size_t)g_hrix_cap * sizeof(HRIdxE));
    if (g_hrix == NULL) tp_giveup("OOM in Horn release");
  }
  HRIdxE *e = &g_hrix[g_hrix_n];
  e->rule = rule; e->cpos = cpos;
  if (head < 0) { e->next = g_hrix_varhead; g_hrix_varhead = g_hrix_n; }
  else          { e->next = g_hrix_head[head]; g_hrix_head[head] = g_hrix_n; }
  g_hrix_n++;
}

// Pending releases (kept as strings for ranking + TpClause append;
// tl/tr keep the arena terms for the goal-subsumption rank check;
// n_weak counts single-occurrence variables -- weakening instances
// like or(V, lemma) rank below their workhorse cores).
typedef struct {
  char *l, *r;
  int tl, tr;
  int goal_hit, sg_hit, goal_syms, ground, n_weak, size, order;
  int grp, w;   // head-symbol group + within-group rank (diversity quota)
  int dead;     // back-subsumed (CD lane): never appended to the engine
  int prov;     // --proof: g_hpr derivation record (-1 = none/failed)
} HRel;
static HRel *g_hrel = NULL; static int g_hrel_n = 0, g_hrel_cap = 0;

static u8 *g_horn_disp = NULL;   // per symbol id: 1 = dispatcher functor

// --- definitional expansion --------------------------------------------
// A unit fact f(X1..Xk) = r (k >= 1, pairwise-distinct variable
// arguments, f absent from r, every variable of r among the arguments)
// DEFINES f.  The chaining universe is normalized by expanding every
// defined symbol, so unification sees a single alphabet -- e.g. the
// LCL encodings state detachment over implies(..) but their axioms
// interleave it with or/not through implies(X,Y) = or(not(X),Y).
// Expansion preserves equational consequence, so releases stay sound.
static int *g_hdef_l = NULL, *g_hdef_r = NULL;   // per symbol: node or -1
static int *g_hdef_cls = NULL;                   // per symbol: defining clause
static int g_hexp_fail = 0, g_hexp_limit = 0;

// --- release provenance capture (PROV_HORN; --proof runs only) ---------
// Each accepted release records the positive hyper-resolution step that
// produced it: the rule clause, the fact bound at every condition (an
// input clause or a prior release), and the definitional-expansion
// axioms used to normalize those parents into the chaining alphabet.
// The proof emitter materializes the cited cone as cnf(h<k>, ...,
// inference(hyper_resolution, ...)) records, so a released unit the
// engine refutation cites is a derived record with problem-formula
// leaves instead of a fake file() leaf (CASC-J13).  Cap-bounded like
// the release pool: past the caps a release carries a TRUNCATED record
// and is excluded from --proof engine input (the pre-provenance
// behavior for that unit) -- never misrepresented.
#define HORN_PROV_DEFS     12         // def parents per clause / per step
#define HORN_PROV_MAX_REL  (1 << 20)  // recorded releases
#define HORN_PROV_MAX_POOL (1 << 23)  // parent-pool ints
typedef struct {   // snapshot of an input clause the chaining used
  char *l, *r;
  int  conj, kind, ix;               // TpClause role + its own provenance
  int  nd, defs[HORN_PROV_DEFS];     // g_hpc defs its normalization expanded
  u8   trunc, emitted;
} HProvCls;
static HProvCls *g_hpc = NULL; static int g_hpc_n = 0, g_hpc_cap = 0;
static int *g_hpc_of = NULL;    // per input-clause index (horn-pass lifetime)
typedef struct {   // one release's hyper-resolution step
  char *l, *r;                       // released unit (ownership taken
                                     // from the HRel strings at cleanup)
  int rule;                          // g_hpc index of the rule clause
  int po;                            // offset into g_hpp
  u16 nf, nd;                        // fact parents / def parents
  u8  trunc, emitted;
} HPr;
static HPr *g_hpr = NULL; static int g_hpr_n = 0, g_hpr_cap = 0;
// Pool layout per release: nf fact refs (>= 0 = release index, < 0 =
// ~g_hpc index) then nd g_hpc def indices.
static int *g_hpp = NULL; static int g_hpp_n = 0, g_hpp_cap = 0;

// Defs touched by the current clause normalization (clause indices).
static int g_hdt[HORN_PROV_DEFS];
static int g_hdt_n = 0, g_hdt_over = 0;

static void horn_prov_touch_reset(void) { g_hdt_n = 0; g_hdt_over = 0; }

static void horn_prov_def_touch(int f) {
  int c = (g_hdef_cls != NULL) ? g_hdef_cls[f] : -1;
  if (c < 0) { g_hdt_over = 1; return; }
  for (int i = 0; i < g_hdt_n; i++) if (g_hdt[i] == c) return;
  if (g_hdt_n >= HORN_PROV_DEFS) { g_hdt_over = 1; return; }
  g_hdt[g_hdt_n++] = c;
}

// Intern input clause c into the snapshot table; -1 on failure.  The
// snapshot is taken NOW because SInE compacts tp->cls after the pass.
static int horn_prov_cls(const TpProblem *tp, int c) {
  if (!g_prov_on || g_hpc_of == NULL || c < 0 || c >= tp->n_cls) return -1;
  if (g_hpc_of[c] >= 0) return g_hpc_of[c];
  if (g_hpc_n >= g_hpc_cap) {
    g_hpc_cap = (g_hpc_cap == 0) ? 256 : g_hpc_cap * 2;
    g_hpc = (HProvCls *)realloc(g_hpc, (size_t)g_hpc_cap * sizeof(HProvCls));
    if (g_hpc == NULL) tp_giveup("OOM in Horn release");
  }
  HProvCls *pc = &g_hpc[g_hpc_n];
  memset(pc, 0, sizeof *pc);
  pc->l = strdup(tp->cls[c].lhs);
  pc->r = strdup(tp->cls[c].rhs);
  pc->conj = tp->cls[c].conj;
  pc->kind = tp->cls[c].prov_kind;
  pc->ix   = tp->cls[c].prov_ix;
  if (pc->l == NULL || pc->r == NULL) pc->trunc = 1;
  g_hpc_of[c] = g_hpc_n;
  return g_hpc_n++;
}

// Attach the currently-touched defs to snapshot j (the defs expanded
// while normalizing that clause into the chaining alphabet).
static void horn_prov_cls_defs(const TpProblem *tp, int j) {
  if (j < 0 || j >= g_hpc_n) return;
  HProvCls *pc = &g_hpc[j];
  if (g_hdt_over) { pc->trunc = 1; return; }
  for (int i = 0; i < g_hdt_n; i++) {
    int dj = horn_prov_cls(tp, g_hdt[i]);
    if (dj < 0 || g_hpc[dj].trunc) { pc->trunc = 1; return; }
    int dup = 0;
    for (int k = 0; k < pc->nd; k++) dup |= (pc->defs[k] == dj);
    if (dup) continue;
    if (pc->nd >= HORN_PROV_DEFS) { pc->trunc = 1; return; }
    pc->defs[pc->nd++] = dj;
  }
}

// Is release k's derivation record complete?
static int horn_prov_ok(int k) {
  return g_hpr != NULL && k >= 0 && k < g_hpr_n && !g_hpr[k].trunc;
}

static int horn_sym_occurs(int f, int t) {
  if (g_hn[t].head == f) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_sym_occurs(f, HARG(t, i))) return 1;
  return 0;
}

static int horn_vars_within(int t, const u8 *allowed) {
  if (g_hn[t].head < 0) return allowed[-g_hn[t].head - 1];
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (!horn_vars_within(HARG(t, i), allowed)) return 0;
  return 1;
}

static void horn_def_note(int l, int r, int cls) {
  if (g_hn[l].head < 0 || g_hn[l].nargs < 1) return;
  int f = g_hn[l].head;
  if (g_horn_disp[f] || g_hdef_r[f] >= 0) return;
  u8 seen[HORN_VSPACE];
  memset(seen, 0, sizeof seen);
  for (int i = 0; i < g_hn[l].nargs; i++) {
    int a = HARG(l, i);
    if (g_hn[a].head >= 0) return;
    int v = -g_hn[a].head - 1;
    if (seen[v]) return;
    seen[v] = 1;
  }
  if (horn_sym_occurs(f, r)) return;
  if (!horn_vars_within(r, seen)) return;
  g_hdef_l[f] = l; g_hdef_r[f] = r;
  if (g_hdef_cls != NULL) g_hdef_cls[f] = cls;
}

static int horn_subst_build(int t, const int *m, int depth) {
  if (depth > 200 || g_hexp_fail || g_hn_n > g_hexp_limit) { g_hexp_fail = 1; return t; }
  if (g_hn[t].head < 0) return m[-g_hn[t].head - 1];
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++)
    args[i] = horn_subst_build(HARG(t, i), m, depth + 1);
  if (g_hexp_fail) return t;
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

static int horn_expand(int t, int depth) {
  if (depth > 64 || g_hexp_fail || g_hn_n > g_hexp_limit) { g_hexp_fail = 1; return t; }
  if (g_hn[t].head < 0) return t;
  int args[16], changed = 0;
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_expand(HARG(t, i), depth + 1);
    changed |= (args[i] != HARG(t, i));
  }
  if (g_hexp_fail) return t;
  int f = g_hn[t].head;
  if (g_hdef_r != NULL && g_hdef_r[f] >= 0) {
    if (g_prov_on) horn_prov_def_touch(f);
    int m[HORN_VSPACE];
    int L = g_hdef_l[f];
    for (int i = 0; i < g_hn[L].nargs; i++)
      m[-g_hn[HARG(L, i)].head - 1] = args[i];
    int e = horn_subst_build(g_hdef_r[f], m, depth + 1);
    if (g_hexp_fail) return t;
    return horn_expand(e, depth + 1);   // chained definitions
  }
  if (!changed) return t;
  return horn_node(f, g_hn[t].nargs, args);
}

// Normalize with fallback: an expansion failure (cyclic definitions,
// blow-up) rolls the arena back and keeps the original term -- either
// form is sound.
static int horn_norm(int t) {
  if (g_hdef_r == NULL) return t;
  g_hexp_fail = 0;
  int mark_n = g_hn_n, mark_a = g_ha_n;
  g_hexp_limit = g_hn_n + 4000;
  int e = horn_expand(t, 0);
  if (g_hexp_fail) { g_hn_n = mark_n; g_ha_n = mark_a; return t; }
  return e;
}

// --- dispatcher / conditional-clause shape checks -----------------------
// ifeqN(X,X,Y[,Z]) = Y with every argument a variable and the other
// side the bare variable Y (either clause orientation).
static int horn_disp_check(int s, int other) {
  if (g_hn[s].head < 0 || g_hn[other].head >= 0) return 0;
  int na = g_hn[s].nargs;
  if (na != 3 && na != 4) return 0;
  for (int i = 0; i < na; i++)
    if (g_hn[HARG(s, i)].head >= 0) return 0;
  int a0 = g_hn[HARG(s, 0)].head, a1 = g_hn[HARG(s, 1)].head;
  int a2 = g_hn[HARG(s, 2)].head, ov = g_hn[other].head;
  if (a0 != a1 || a2 != ov || a2 == a0) return 0;
  return 1;
}

// Push condition (cl, ct), splitting PAIRED conditions first: the CSR
// consistency KBs pack conjunctions as tuple2(p,q) = tuple2(true,true),
// which no unit fact can ever unify.  When both sides carry the same
// rigid head and arity, the argument equations are pushed instead --
// they IMPLY the packed equation by congruence (then the dispatcher
// fires), so the decoded rule stays a sound consequence.  Returns 0 on
// condition-count overflow.
static int horn_cond_push(HRule *ru, int *nc, int cl, int ct, int depth) {
  if (depth < 8 && g_hn[cl].head >= 0 && g_hn[cl].head == g_hn[ct].head &&
      g_hn[cl].nargs == g_hn[ct].nargs && g_hn[cl].nargs > 0) {
    for (int i = 0; i < g_hn[cl].nargs; i++)
      if (!horn_cond_push(ru, nc, HARG(cl, i), HARG(ct, i), depth + 1))
        return 0;
    return 1;
  }
  if (*nc >= HORN_MAX_COND) return 0;
  ru->cl[*nc] = cl;
  ru->ct[*nc] = ct;
  (*nc)++;
  return 1;
}

// Unwrap ifeqN(c, t, then, e) = e (else side must equal the clause's
// other side for arity 4; arity 3 has no else).  Nesting in the
// then-position accumulates conditions; a partial unwrap is still a
// sound rule (the residual term is the conclusion).
static int horn_decode_side(int s, int other, HRule *ru) {
  int nc = 0, cur = s;
  for (;;) {
    if (g_hn[cur].head < 0 || !g_horn_disp[g_hn[cur].head]) break;
    int na = g_hn[cur].nargs;
    if (na != 3 && na != 4) break;
    if (na == 4 && !horn_eq(HARG(cur, 3), other)) break;
    if (!horn_cond_push(ru, &nc, HARG(cur, 0), HARG(cur, 1), 0)) return 0;
    cur = HARG(cur, 2);
  }
  if (nc == 0) return 0;
  ru->nc = nc; ru->concl_l = cur; ru->concl_r = other;
  return 1;
}

// --- the forward-chaining join ------------------------------------------
static int g_horn_stop = 0;
static double g_horn_t0 = 0.0;
static double g_horn_wall = HORN_WALL_SEC;      // THVM_ATP_HORN_WALL
// Absolute wall-clock stop deadline (process-start + slice wall), set in
// tp_horn_release.  Independent of g_horn_wall (which is release-start-
// relative and shapes the search): the STOP polls trip on EITHER, so a
// late-starting release self-terminates at the slice wall without the
// gate-relax/phase-2 timing (fractions of g_horn_wall) shifting.  0 = off.
static double g_horn_abs_deadline = 0.0;
static int g_horn_max_rel = HORN_MAX_RELEASES;  // THVM_ATP_HORN_MAX
// Attempt budget: the CD lane runs wall-governed (its subsumed space
// stays small), every other flavor keeps the fixed v1 cap.  Same for
// the arena-node ceiling: a wall-long CD run holds millions of
// accepted facts live.
static long g_horn_max_attempts = HORN_MAX_ATTEMPTS;
static int  g_horn_max_nodes = (1 << 22);
static double horn_now(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}
// STOP-condition wall test: the release-relative budget (search-shaping)
// OR the absolute slice deadline (overrun guard), whichever comes first.
// Used only where the loop would set g_horn_stop; the search-shaping
// deadlines (phase-2 trigger, gate-relax cones) keep the bare g_horn_wall
// form so a release that finishes before the wall stays byte-identical.
static inline int horn_wall_over(void) {
  double tn = horn_now();
  if (tn - g_horn_t0 > g_horn_wall) return 1;
  if (g_horn_abs_deadline > 0.0 && tn > g_horn_abs_deadline) return 1;
  return 0;
}

// --- goal-directed backward subgoal cone (v2) ---------------------------
// SLD/UR-style regression from the negated goals through the decoded
// rules, bounded by depth, count and a wall slice.  A subgoal is an
// equation PATTERN: any released fact that unifies with one is a
// candidate step of a goal-closing chain, so it is (a) popped from the
// agenda with priority -- its forward joins produce the parent
// subgoal's instance next -- and (b) ranked into the kept window.
// Subgoals are relevance markers only, never released as equations, so
// soundness is untouched.
//
// Regression steps, for a subgoal G and rule C1..Ck -> L=R with the
// conclusion unified against G (either orientation):
//   - abstract:  each condition Ci becomes a subgoal under the
//     conclusion bindings alone (SLD with the siblings postponed);
//     rejected unless enough rigid structure survives to discriminate.
//   - UR-style:  all conditions but one are satisfied against the
//     CURRENT fact set (input units; the cone runs before the loop),
//     and the open condition becomes the subgoal -- the classic
//     unit-resulting negative-unit step, fully constrained.
typedef struct { int l, r, depth, hl, hr, hits; } HGoalPat;
static HGoalPat *g_hsg = NULL; static int g_hsg_n = 0, g_hsg_cap = 0;
static int g_hsg_depth_cap = 6;             // THVM_ATP_HORN_BWD_DEPTH
static int g_hsg_max = 512;                 // THVM_ATP_HORN_BWD_MAX
static int g_hsg_stop = 0;
static double g_hsg_deadline = 0.0;
static long g_hsg_checks = 0;               // release-vs-subgoal unify budget
#define HORN_SG_BUDGET 20000000L
#define HORN_SG_MIN_RIGID 3
// Steering is AUTO-GATED (g_horn_steer): priority pops run only when
// the goal is existential (a reachability/planning query -- PUZ
// states, PLA plans -- where only regression can carve a path through
// an astronomical forward space) or the relevance gate is active (CSR
// KBs).  For ground saturation-style goals (the LCL condensed-
// detachment family) the baseline agenda already reaches goals on a
// razor-thin trajectory, and ANY dilution of its pop order loses them
// (LCL192/LCL211 measured); there the cone is skipped entirely and
// only the direct goal probe runs.  THVM_ATP_HORN_BWD: 0=off, 2=force
// steering, default auto.
static int g_horn_steer = 0;
static int g_horn_goal_stop = 1;            // THVM_ATP_HORN_GOAL_STOP
static int g_horn_goal_reached = 0;

static int horn_rigid_count(int t) {
  int n = (g_hn[t].head >= 0);
  for (int i = 0; i < g_hn[t].nargs; i++) n += horn_rigid_count(HARG(t, i));
  return n;
}

static int horn_has_var(int t) {
  if (g_hn[t].head < 0) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_has_var(HARG(t, i))) return 1;
  return 0;
}

// Canonicalize (cl, ct) under the live bindings into a new subgoal;
// dedup ("?"-prefixed keys share the release dedup set), reject
// under-constrained patterns (too little rigid structure to steer).
static void horn_bwd_emit(const TpProblem *tp, int cl, int ct, int depth) {
  if (g_hsg_n >= g_hsg_max) { g_hsg_stop = 1; return; }
  int hn_mark = g_hn_n, ha_mark = g_ha_n;
  horn_can_reset();
  int budget = HORN_APPLY_MAX;
  int l2 = horn_apply(cl, 0, &budget);
  int r2 = (l2 >= 0) ? horn_apply(ct, 0, &budget) : -1;
  if (l2 < 0 || r2 < 0) goto reject;
  // Specificity guard: a pattern needs enough rigid structure to
  // discriminate -- except a GROUND pattern, which is maximally
  // specific however small (a CSR goal is a bare `a = b`).
  if (g_can_nv > 0 &&
      horn_rigid_count(l2) + horn_rigid_count(r2) < HORN_SG_MIN_RIGID)
    goto reject;
  {
    static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 3];
    size_t lo = 0, ro = 0;
    int remap[HORN_REL_VARS], nv = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
    if (!horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv)) goto reject;
    if (!horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv)) goto reject;
    snprintf(key, sizeof key, "?%s=%s", lb, rb);
    if (!horn_dedup_add(key)) goto reject;
    size_t fo = 0, go = 0;
    int remap2[HORN_REL_VARS], nv2 = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap2[i] = -1;
    if (horn_print(tp, rb, HORN_SIDE_MAX, &fo, r2, remap2, &nv2) &&
        horn_print(tp, lb, HORN_SIDE_MAX, &go, l2, remap2, &nv2)) {
      snprintf(key, sizeof key, "?%s=%s", rb, lb);
      horn_dedup_add(key);
    }
  }
  if (g_hsg_n >= g_hsg_cap) {
    g_hsg_cap = (g_hsg_cap == 0) ? (1 << 9) : g_hsg_cap * 2;
    g_hsg = (HGoalPat *)realloc(g_hsg, (size_t)g_hsg_cap * sizeof(HGoalPat));
    if (g_hsg == NULL) tp_giveup("OOM in Horn release");
  }
  g_hsg[g_hsg_n].l = l2; g_hsg[g_hsg_n].r = r2;
  g_hsg[g_hsg_n].depth = depth;
  g_hsg[g_hsg_n].hl = g_hn[l2].head; g_hsg[g_hsg_n].hr = g_hn[r2].head;
  g_hsg[g_hsg_n].hits = 0;
  g_hsg_n++;
  {
    static int diag = -1;
    if (diag < 0) { const char *d = getenv("THVM_ATP_HORN_DIAG"); diag = (d != NULL && d[0] != '0' && d[0] != '\0'); }
    if (diag && g_hsg_n <= 64) {
      static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
      size_t lo = 0, ro = 0;
      int remap[HORN_REL_VARS], nv = 0;
      for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
      if (horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv) &&
          horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv))
        fprintf(stderr, "[horn-sg %d d%d] %s = %s\n", g_hsg_n - 1, depth, lb, rb);
    }
  }
  return;
reject:
  g_hn_n = hn_mark; g_ha_n = ha_mark;
}

// UR fill: satisfy every condition except `open` from the current fact
// set (no processed filter -- the cone runs before the loop), then emit
// the open condition as a subgoal under the accumulated bindings.
static void horn_bwd_fill(const TpProblem *tp, const HRule *ru, int ci,
                          int open, int depth) {
  if (g_hsg_stop) return;
  while (ci == open) ci++;
  if (ci >= ru->nc) { horn_bwd_emit(tp, ru->cl[open], ru->ct[open], depth); return; }
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > HORN_MAX_ATTEMPTS || horn_now() > g_hsg_deadline ||
       g_hn_n > (1 << 22))) { g_hsg_stop = 1; return; }
  int c = ru->cl[ci], coff = 0;
  horn_deref(&c, &coff);
  int off = (ci + 1) * HORN_VSPACE;
  for (int pass = 0; pass < 2 && !g_hsg_stop; pass++) {
    int e;
    if (pass == 0) e = (g_hn[c].head >= 0) ? g_hix_head[g_hn[c].head] : -2;
    else           e = g_hix_varhead;
    if (e == -2) {
      for (int f = 0; f < g_hf_n && !g_hsg_stop; f++) {
        if (!g_hf[f].rel) continue;   // cone fills stay in the relevant cone
        for (int orient = 0; orient < 2; orient++) {
          int fl = orient ? g_hf[f].r : g_hf[f].l;
          int fr = orient ? g_hf[f].l : g_hf[f].r;
          int mark = g_htrail_n;
          g_horn_attempts++;
          if (horn_unify(ru->cl[ci], 0, fl, off, 0) &&
              horn_unify(ru->ct[ci], 0, fr, off, 0))
            horn_bwd_fill(tp, ru, ci + 1, open, depth);
          horn_undo(mark);
        }
      }
      continue;
    }
    for (; e >= 0 && !g_hsg_stop; e = g_hix[e].next) {
      int f = g_hix[e].fact;
      if (!g_hf[f].rel) continue;     // cone fills stay in the relevant cone
      int fl = g_hix[e].orient ? g_hf[f].r : g_hf[f].l;
      int fr = g_hix[e].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      g_horn_attempts++;
      if (horn_unify(ru->cl[ci], 0, fl, off, 0) &&
          horn_unify(ru->ct[ci], 0, fr, off, 0))
        horn_bwd_fill(tp, ru, ci + 1, open, depth);
      horn_undo(mark);
    }
  }
}

// Breadth-first cone: seed from the parsed negative units, regress
// through every rule whose conclusion unifies (either orientation).
static void horn_backward_cone(const TpProblem *tp) {
  for (int g = 0; g < g_hg_n && !g_hsg_stop; g++)
    horn_bwd_emit(tp, g_hg_l[g], g_hg_r[g], 0);
  for (int qi = 0; qi < g_hsg_n && !g_hsg_stop; qi++) {
    if (g_hsg[qi].depth >= g_hsg_depth_cap) continue;
    int sgl = g_hsg[qi].l, sgr = g_hsg[qi].r, d1 = g_hsg[qi].depth + 1;
    for (int r = 0; r < g_hr_n && !g_hsg_stop; r++) {
      const HRule *ru = &g_hr[r];
      if (!ru->rel) continue;   // relevance-gated (oversized KBs)
      int chl = g_hn[ru->concl_l].head, chr = g_hn[ru->concl_r].head;
      for (int orient = 0; orient < 2 && !g_hsg_stop; orient++) {
        int gl = orient ? sgr : sgl, gr = orient ? sgl : sgr;
        if (chl >= 0 && g_hn[gl].head >= 0 && chl != g_hn[gl].head) continue;
        if (chr >= 0 && g_hn[gr].head >= 0 && chr != g_hn[gr].head) continue;
        int mark = g_htrail_n;
        g_horn_attempts++;
        if (horn_unify(ru->concl_l, 0, gl, HORN_SG_OFF, 0) &&
            horn_unify(ru->concl_r, 0, gr, HORN_SG_OFF, 0)) {
          for (int j = 0; j < ru->nc && !g_hsg_stop; j++) {
            horn_bwd_emit(tp, ru->cl[j], ru->ct[j], d1);
            if (ru->nc >= 2) horn_bwd_fill(tp, ru, 0, j, d1);
          }
        }
        horn_undo(mark);
        if ((g_horn_attempts & 0xfff) == 0 && horn_now() > g_hsg_deadline)
          g_hsg_stop = 1;
      }
    }
  }
}

// Does the fact (l, r) unify with some subgoal?  l/r live in a var
// space of their own (canonical release vars, or an input fact's
// clause-local vars): probe space vs subgoal space, both clean even
// while a join's bindings are live in spaces 0..HORN_MAX_COND.
static int horn_sg_probe(int l, int r) {
  if (g_hsg_n == 0 || g_hsg_checks > HORN_SG_BUDGET) return 0;
  if (g_hpheap.n > 4096) return 0;    // steering saturated: stop paying
  int hl = g_hn[l].head, hr = g_hn[r].head;
  for (int s = 0; s < g_hsg_n; s++) {
    for (int orient = 0; orient < 2; orient++) {
      int a = orient ? r : l, b = orient ? l : r;
      int ha = orient ? hr : hl, hb = orient ? hl : hr;
      if (ha >= 0 && g_hsg[s].hl >= 0 && ha != g_hsg[s].hl) continue;
      if (hb >= 0 && g_hsg[s].hr >= 0 && hb != g_hsg[s].hr) continue;
      int mark = g_htrail_n;
      g_hsg_checks++;
      int hit = horn_unify(a, HORN_PROBE_OFF, g_hsg[s].l, HORN_SG_OFF, 0) &&
                horn_unify(b, HORN_PROBE_OFF, g_hsg[s].r, HORN_SG_OFF, 0);
      horn_undo(mark);
      if (hit) { g_hsg[s].hits++; return 1; }   // hits: diagnostics only
    }
  }
  return 0;
}

// --- SInE-lite relevance gate for oversized KBs (v2) --------------------
// The CSR knowledge bases carry 20-40k clauses; a blind given-fact loop
// burns the whole attempt budget on ontology noise and never touches
// the conjecture's neighborhood.  Compute Hoder-Voronkov relevance over
// the DECODED universe (facts + rules), seeded from the conjecture
// clauses: an unreached fact is pre-marked processed -- it stays a
// passive join partner but is never popped as given -- and an
// unreached rule neither joins givens nor hosts cone regressions.
// Purely a search-order device: no clause is dropped, releases stay
// sound consequences.
static u32 horn_syms_count(int t, u32 *gen, u32 *stamp, u32 tag) {
  u32 n = 0;
  if (g_hn[t].head >= 0) {
    if (stamp[g_hn[t].head] != tag) { stamp[g_hn[t].head] = tag; gen[g_hn[t].head]++; n++; }
    for (int i = 0; i < g_hn[t].nargs; i++)
      n += horn_syms_count(HARG(t, i), gen, stamp, tag);
  }
  return n;
}
static u32 horn_syms_min(int t, const u32 *gen) {
  u32 m = 0xffffffffu;
  if (g_hn[t].head >= 0) {
    m = gen[g_hn[t].head];
    for (int i = 0; i < g_hn[t].nargs; i++) {
      u32 c = horn_syms_min(HARG(t, i), gen);
      if (c < m) m = c;
    }
  }
  return m;
}
static int horn_syms_trig(int t, const u32 *gen, const u8 *reach, double lim) {
  if (g_hn[t].head < 0) return 0;
  if (reach[g_hn[t].head] && (double)gen[g_hn[t].head] <= lim) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_syms_trig(HARG(t, i), gen, reach, lim)) return 1;
  return 0;
}
static void horn_syms_reach(int t, u8 *reach) {
  if (g_hn[t].head < 0) return;
  reach[g_hn[t].head] = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) horn_syms_reach(HARG(t, i), reach);
}

static double g_horn_gate_tol = 1.5;
static int    g_horn_gate_on  = 0;
static int    g_horn_input_nf = 0;   // facts below this index are input units

// Recomputable: a WIDER tol yields a superset of relevant clauses, so
// the fixpoint loop can relax the gate iteratively.  Facts that become
// relevant after being parked as passive re-enter the agenda.
// admit_gh: also activate the goalward-head-closure rules (see below)
// -- deferred to the SECOND phase of the chaining wall, because from
// the start the ~19k admitted rules dilute a focused agenda into a
// release flood (CSR036: goal reached at 24k releases under the 511
// generality-gated rules, buried past 291k with the closure active).
static void horn_relevance_gate(const TpProblem *tp, double tol,
                                int admit_gh) {
  u32 *gen   = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
  u32 *stamp = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
  u8  *reach = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
  if (gen == NULL || stamp == NULL || reach == NULL)
    tp_giveup("OOM in Horn release");
  u32 tag = 0;
  for (int f = 0; f < g_hf_n; f++) {
    tag++;
    horn_syms_count(g_hf[f].l, gen, stamp, tag);
    horn_syms_count(g_hf[f].r, gen, stamp, tag);
    // Releases (f >= the input-fact watermark) descend from relevant
    // chains: always active.
    g_hf[f].rel = (f >= g_horn_input_nf) ? 1 : g_hf[f].conj;
  }
  for (int r = 0; r < g_hr_n; r++) {
    tag++;
    for (int j = 0; j < g_hr[r].nc; j++) {
      horn_syms_count(g_hr[r].cl[j], gen, stamp, tag);
      horn_syms_count(g_hr[r].ct[j], gen, stamp, tag);
    }
    horn_syms_count(g_hr[r].concl_l, gen, stamp, tag);
    horn_syms_count(g_hr[r].concl_r, gen, stamp, tag);
    g_hr[r].rel = g_hr[r].conj;
  }
  for (int g = 0; g < g_hg_n; g++) {
    horn_syms_reach(g_hg_l[g], reach);
    horn_syms_reach(g_hg_r[g], reach);
  }
  for (int f = 0; f < g_hf_n; f++)
    if (g_hf[f].conj) { horn_syms_reach(g_hf[f].l, reach); horn_syms_reach(g_hf[f].r, reach); }
  for (int r = 0; r < g_hr_n; r++)
    if (g_hr[r].conj) {
      for (int j = 0; j < g_hr[r].nc; j++) {
        horn_syms_reach(g_hr[r].cl[j], reach);
        horn_syms_reach(g_hr[r].ct[j], reach);
      }
      horn_syms_reach(g_hr[r].concl_l, reach);
      horn_syms_reach(g_hr[r].concl_r, reach);
    }
  // Goalward head closure (magic-set shape, RULES only): admit every
  // rule whose conclusion's root predicate can feed a goal-relevant
  // predicate, walking conclusion -> condition heads backward to the
  // fixpoint.  Generality triggering alone cannot see definition-
  // shaped bridges: CSR040's proof crosses
  //   firstordercollection (gen 244) -> fixedordercollection (gen 9),
  // which needs tol >= gen(trigger)/min_gen = 27 -- a tolerance that
  // floods everything else long before the relax ladder reaches it.
  // Facts stay generality-gated (they ARE the agenda; admitting rules
  // is join-time free: a rule only fires when an ACTIVE given matches
  // one of its condition heads through the rule index).  Encoding
  // scaffolding (dispatcher functors, true/false) never carries
  // goal-relevance between rules: every conclusion is `p(...) = true`
  // shaped, so a `true` head would relate ALL rules.
  {
    u8 *gh = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
    if (gh == NULL) tp_giveup("OOM in Horn release");
    #define HORN_GH_ADD(t) do {                                        \
      int h_ = g_hn[(t)].head;                                         \
      if (h_ >= 0 && !g_horn_disp[h_] &&                               \
          strcmp(tp->syms[h_].name, "true") != 0 &&                    \
          strcmp(tp->syms[h_].name, "false") != 0) gh[h_] = 1;         \
    } while (0)
    for (int g = 0; g < g_hg_n; g++) {
      HORN_GH_ADD(g_hg_l[g]);
      HORN_GH_ADD(g_hg_r[g]);
    }
    for (int f = 0; f < g_hf_n; f++)
      if (g_hf[f].conj) { HORN_GH_ADD(g_hf[f].l); HORN_GH_ADD(g_hf[f].r); }
    for (int r = 0; r < g_hr_n; r++)
      if (g_hr[r].conj)
        for (int j = 0; j < g_hr[r].nc; j++) {
          HORN_GH_ADD(g_hr[r].cl[j]);
          HORN_GH_ADD(g_hr[r].ct[j]);
        }
    for (;;) {
      int grew = 0;
      for (int r = 0; r < g_hr_n; r++) {
        if (g_hr[r].ghrel) continue;
        int hl = g_hn[g_hr[r].concl_l].head, hr = g_hn[g_hr[r].concl_r].head;
        if (!((hl >= 0 && gh[hl]) || (hr >= 0 && gh[hr]))) continue;
        g_hr[r].ghrel = 1;
        grew = 1;
        for (int j = 0; j < g_hr[r].nc; j++) {
          HORN_GH_ADD(g_hr[r].cl[j]);
          HORN_GH_ADD(g_hr[r].ct[j]);
        }
      }
      if (!grew) break;
    }
    #undef HORN_GH_ADD
    int nghr = 0;
    for (int r = 0; r < g_hr_n; r++)
      if (g_hr[r].ghrel) {
        if (admit_gh) g_hr[r].rel = 1;
        nghr++;
      }
    fprintf(stderr, "FindProof: Horn goalward head closure: %d rule(s)%s\n",
            nghr, admit_gh ? " (active)" : " (phase 2)");
    free(gh);
  }
  for (int rd = 0; rd < 24; rd++) {
    int grew = 0;
    for (int f = 0; f < g_hf_n; f++) {
      if (g_hf[f].rel) continue;
      u32 m = horn_syms_min(g_hf[f].l, gen);
      u32 m2 = horn_syms_min(g_hf[f].r, gen);
      if (m2 < m) m = m2;
      double lim = tol * (double)m;
      if (horn_syms_trig(g_hf[f].l, gen, reach, lim) ||
          horn_syms_trig(g_hf[f].r, gen, reach, lim)) {
        g_hf[f].rel = 1; grew = 1;
        horn_syms_reach(g_hf[f].l, reach);
        horn_syms_reach(g_hf[f].r, reach);
      }
    }
    for (int r = 0; r < g_hr_n; r++) {
      if (g_hr[r].rel) continue;
      u32 m = horn_syms_min(g_hr[r].concl_l, gen);
      u32 m2 = horn_syms_min(g_hr[r].concl_r, gen);
      if (m2 < m) m = m2;
      for (int j = 0; j < g_hr[r].nc; j++) {
        u32 c = horn_syms_min(g_hr[r].cl[j], gen);  if (c < m) m = c;
        u32 d = horn_syms_min(g_hr[r].ct[j], gen);  if (d < m) m = d;
      }
      double lim = tol * (double)m;
      int hit = horn_syms_trig(g_hr[r].concl_l, gen, reach, lim) ||
                horn_syms_trig(g_hr[r].concl_r, gen, reach, lim);
      for (int j = 0; j < g_hr[r].nc && !hit; j++)
        hit = horn_syms_trig(g_hr[r].cl[j], gen, reach, lim) ||
              horn_syms_trig(g_hr[r].ct[j], gen, reach, lim);
      if (hit) {
        g_hr[r].rel = 1; grew = 1;
        for (int j = 0; j < g_hr[r].nc; j++) {
          horn_syms_reach(g_hr[r].cl[j], reach);
          horn_syms_reach(g_hr[r].ct[j], reach);
        }
        horn_syms_reach(g_hr[r].concl_l, reach);
        horn_syms_reach(g_hr[r].concl_r, reach);
      }
    }
    if (!grew) break;
  }
  int nf = 0, nr = 0;
  for (int f = 0; f < g_hf_n; f++) {
    if (g_hf[f].rel) {
      nf++;
      if (g_hf[f].processed && !g_hf[f].given) {
        // Parked passive by a tighter gate: back onto the agenda.
        g_hf[f].processed = 0;
        horn_heap_push(&g_hheap, f);
        if (g_hf[f].sghit) horn_heap_push(&g_hpheap, f);
      }
    } else if (!g_hf[f].given) {
      g_hf[f].processed = 1;      // passive: join partner, never given
    }
  }
  for (int r = 0; r < g_hr_n; r++) nr += g_hr[r].rel;
  fprintf(stderr, "FindProof: Horn relevance gate: %d/%d fact(s), "
          "%d/%d rule(s) active (tol=%.2f)\n", nf, g_hf_n, nr, g_hr_n, tol);
  free(gen); free(stamp); free(reach);
}

// Does the fact (l, r) unify with an ORIGINAL goal?  Any common
// instance refutes that negative unit, so the chaining can stop: the
// goal-subsuming release is kept first and the engine closes at once.
static int horn_goal_probe(int l, int r) {
  for (int g = 0; g < g_hg_n; g++) {
    for (int orient = 0; orient < 2; orient++) {
      int a = orient ? r : l, b = orient ? l : r;
      int mark = g_htrail_n;
      int hit = horn_unify(a, HORN_PROBE_OFF, g_hg_l[g], HORN_SG_OFF, 0) &&
                horn_unify(b, HORN_PROBE_OFF, g_hg_r[g], HORN_SG_OFF, 0);
      horn_undo(mark);
      if (hit) return 1;
    }
  }
  return 0;
}

// The fact bound at each rule condition of the join being emitted
// (horn_given_try fills the given's slot, horn_fill the rest) -- the
// hyper-resolution step's fact parents, read at release-record time.
static int g_hpf[HORN_MAX_COND];
static int g_horn_given_ix = -1;

// Append one derivation record: `rule` = g_hpc index of the rule
// clause, fps/nf = the fact parents (>= 0 = g_hpr index, < 0 = ~g_hpc
// index), l/r = the derived unit (ownership passes to the record).
// The record unions in the rule's and every input-fact parent's
// definitional-expansion axioms.  Returns the g_hpr index, or -1 when
// a piece is unavailable and no honest record exists (caps hit,
// truncated ancestor) -- the unit is then PROV_TRUNCATED.
static int horn_prov_new(int rule, const int *fps, int nf, char *l, char *r) {
  if (!g_prov_on || l == NULL || r == NULL) return -1;
  if (g_hpr_n >= HORN_PROV_MAX_REL) return -1;
  if (rule < 0 || rule >= g_hpc_n || g_hpc[rule].trunc) return -1;
  int defs[2 * HORN_PROV_DEFS], ndefs = 0;
  for (int i = 0; i < g_hpc[rule].nd; i++)
    defs[ndefs++] = g_hpc[rule].defs[i];
  for (int ci = 0; ci < nf; ci++) {
    int src = fps[ci];
    if (src == HORN_SRC_NONE) return -1;
    if (src >= 0) {                     // a prior derived unit
      if (!horn_prov_ok(src)) return -1;
    } else {                            // an input clause: union its defs
      int j = ~src;
      if (j >= g_hpc_n || g_hpc[j].trunc) return -1;
      for (int i = 0; i < g_hpc[j].nd; i++) {
        int dj = g_hpc[j].defs[i], dup = 0;
        for (int k = 0; k < ndefs; k++) dup |= (defs[k] == dj);
        if (dup) continue;
        if (ndefs >= (int)(sizeof defs / sizeof defs[0])) return -1;
        defs[ndefs++] = dj;
      }
    }
  }
  if (g_hpp_n + nf + ndefs > HORN_PROV_MAX_POOL) return -1;
  if (g_hpr_n >= g_hpr_cap) {
    g_hpr_cap = (g_hpr_cap == 0) ? (1 << 12) : g_hpr_cap * 2;
    g_hpr = (HPr *)realloc(g_hpr, (size_t)g_hpr_cap * sizeof(HPr));
    if (g_hpr == NULL) tp_giveup("OOM in Horn release");
  }
  if (g_hpp_n + nf + ndefs > g_hpp_cap) {
    g_hpp_cap = (g_hpp_cap == 0) ? (1 << 14) : g_hpp_cap;
    while (g_hpp_cap < g_hpp_n + nf + ndefs) g_hpp_cap *= 2;
    g_hpp = (int *)realloc(g_hpp, (size_t)g_hpp_cap * sizeof(int));
    if (g_hpp == NULL) tp_giveup("OOM in Horn release");
  }
  HPr *p = &g_hpr[g_hpr_n];
  memset(p, 0, sizeof *p);
  p->l = l; p->r = r;
  p->rule = rule;
  p->po = g_hpp_n;
  p->nf = (u16)nf;
  p->nd = (u16)ndefs;
  for (int i = 0; i < nf; i++) g_hpp[g_hpp_n++] = fps[i];
  for (int i = 0; i < ndefs; i++) g_hpp[g_hpp_n++] = defs[i];
  return g_hpr_n++;
}

// Record a forward release's hyper-resolution step (rule + the fact
// bound at every condition, from g_hpf).
static int horn_prov_record(const HRule *ru, const HRel *re) {
  if (!g_prov_on) return -1;
  int fps[HORN_MAX_COND];
  for (int ci = 0; ci < ru->nc; ci++) {
    int f = g_hpf[ci];
    fps[ci] = (f >= 0 && f < g_hf_n) ? g_hf[f].src : HORN_SRC_NONE;
  }
  return horn_prov_new(ru->hpc, fps, ru->nc, re->l, re->r);
}

static void horn_emit_release(TpProblem *tp, const HRule *ru) {
  // Rejected releases (dedup hits, guards) roll their applied terms
  // back out of the arena -- only accepted facts persist.
  int hn_mark = g_hn_n, ha_mark = g_ha_n;
  horn_can_reset();
  int budget = HORN_APPLY_MAX;
  int l2 = horn_apply(ru->concl_l, 0, &budget);
  int r2 = (l2 >= 0) ? horn_apply(ru->concl_r, 0, &budget) : -1;
  if (l2 < 0 || r2 < 0) goto reject;
  if (horn_eq(l2, r2)) goto reject;                  // trivial
  // Collapse guards: a bare-variable side whose variable does not
  // occur in the other side releases a near-degenerate equation.
  if (g_hn[l2].head < 0 || g_hn[r2].head < 0) {
    int v = (g_hn[l2].head < 0) ? l2 : r2, w = (g_hn[l2].head < 0) ? r2 : l2;
    if (g_hn[w].head < 0) goto reject;               // X = Y
    int slot = -g_hn[v].head - 1, hit = 0;
    // canonical terms are offset-free: reuse occurs via a tiny scan
    int stack[512], sp = 0; stack[sp++] = w;
    while (sp > 0 && !hit) {
      int t = stack[--sp];
      if (g_hn[t].head < 0) { if (-g_hn[t].head - 1 == slot) hit = 1; }
      else for (int i = 0; i < g_hn[t].nargs && sp < 512; i++) stack[sp++] = HARG(t, i);
    }
    if (!hit) goto reject;
  }
  // CD lane: the weight cap.  Over-cap conclusions are rejected BEFORE
  // the dedup add, so a later deepening round re-derives and admits
  // them; g_cd_overcap records that the cap (not saturation) pruned.
  // Emissions are wall-checked directly: the subsumption machinery
  // below runs between the join loops' attempt-counter checks, and an
  // emit-heavy stretch must not outrun the budget.
  // CONE EXEMPTION (the resonance idea, with the backward cone as the
  // hint generator): a conclusion that unifies a goal-regression
  // subgoal is a candidate step of a goal-closing chain -- it is kept
  // WHATEVER its size.  The deep PM intermediates (hypothetical
  // syllogism instances run ~25 symbols) sit above any cap the
  // forward space can saturate, but they lie ON the regression cone.
  {
    int cd_size = 0, cd_reso = 0;
    if (g_horn_cd) {
      static int cd_emits = 0;
      if ((++cd_emits & 0xff) == 0 && horn_wall_over()) {
        g_horn_stop = 1;
        goto reject;
      }
      // Otter's max_distinct_vars: the classic CD branching cut.  A
      // conclusion sprouting many distinct variables is a promiscuous
      // joiner whatever its size.
      if (g_can_nv > g_cd_maxvars) goto reject;
      cd_size = horn_size(l2) + horn_size(r2);
      cd_reso = horn_cd_resonant(l2, r2);
      if (cd_size > g_cd_cap && !cd_reso) { g_cd_overcap = 1; goto reject; }
    }
  {
  static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 2];
  size_t lo = 0, ro = 0;
  int remap[HORN_REL_VARS], nv = 0;
  for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
  if (!horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv)) goto reject;
  if (!horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv)) goto reject;
  snprintf(key, sizeof key, "%s=%s", lb, rb);
  if (!horn_dedup_add(key)) goto reject;
  // Suppress the mirror too: print r-then-l under a fresh renumbering
  // and pre-claim that key, so the flipped orientation is never
  // released as a separate unit.
  {
    static char fl[TP_SIDE_LEN], fr[TP_SIDE_LEN];
    size_t fo = 0, go = 0;
    int remap2[HORN_REL_VARS], nv2 = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap2[i] = -1;
    if (horn_print(tp, fr, HORN_SIDE_MAX, &fo, r2, remap2, &nv2) &&
        horn_print(tp, fl, HORN_SIDE_MAX, &go, l2, remap2, &nv2)) {
      snprintf(key, sizeof key, "%s=%s", fr, fl);
      horn_dedup_add(key);
    }
  }
  // CD lane: generalization subsumption -- an instance of a live fact
  // (input schema or earlier release) is redundant.  The dedup entry
  // above stays claimed: the candidate is redundant forever (whatever
  // subsumes its subsumer subsumes it).
  if (g_horn_cd && horn_cd_fwd_subsumed(l2, r2, cd_size)) {
    g_cd_fsub++;
    goto reject;
  }
  // New unit: joins the fact set immediately (chains within the round
  // are picked up next round via the semi-naive base) and the pending
  // release list for the final append.  In the CD lane the resonance
  // hit plays the sg-hit role: priority pop + kept-window tier.
  int sgh = g_horn_cd ? cd_reso : horn_sg_probe(l2, r2);
  if (g_horn_cd && cd_reso) {
    g_cd_reso_hits++;
    // A resonator most of the space matches carries no signal -- the
    // goal is shaped like a generic lemma of its calculus (measured:
    // 252k of 255k releases on LCL054-10, whose Frege goal is the
    // generic implication-composition shape).  Drop the resonators and
    // let the pure weight discipline run.
    if (g_cd_reso_hits > 2000 && g_cd_reso_hits * 2 > (long)g_hrel_n) {
      g_cd_reso_n = 0;
      fprintf(stderr, "FindProof: Horn CD: resonators flooded "
              "(%ld of %d released) -- disabled\n",
              g_cd_reso_hits, g_hrel_n);
    }
  }
  horn_fact_add(l2, r2, g_can_nv);
  if (sgh) {
    g_hf[g_hf_n - 1].sghit = 1;
    horn_heap_push(&g_hpheap, g_hf_n - 1);
  }
  if (g_horn_goal_stop && !g_horn_goal_reached && g_hg_n > 0 &&
      horn_goal_probe(l2, r2)) {
    g_horn_goal_reached = 1;
    g_horn_stop = 1;
    fprintf(stderr, "FindProof: Horn release: goal-subsuming unit %s = %s\n",
            lb, rb);
  }
  if (g_hrel_n >= g_hrel_cap) {
    g_hrel_cap = (g_hrel_cap == 0) ? (1 << 12) : g_hrel_cap * 2;
    g_hrel = (HRel *)realloc(g_hrel, (size_t)g_hrel_cap * sizeof(HRel));
    if (g_hrel == NULL) tp_giveup("OOM in Horn release");
  }
  HRel *re = &g_hrel[g_hrel_n];
  re->l = strdup(lb); re->r = strdup(rb);
  re->tl = l2; re->tr = r2;
  re->ground = (g_can_nv == 0);
  re->sg_hit = sgh;
  re->goal_syms = 0;   // filled at ranking time
  re->n_weak = 0;
  {
    int cnt[HORN_REL_VARS];
    memset(cnt, 0, sizeof cnt);
    int stack[512], sp = 0;
    stack[sp++] = l2; stack[sp++] = r2;
    while (sp > 0) {
      int t = stack[--sp];
      if (g_hn[t].head < 0) cnt[-g_hn[t].head - 1]++;
      else for (int i = 0; i < g_hn[t].nargs && sp < 512; i++)
        stack[sp++] = HARG(t, i);
    }
    for (int i = 0; i < g_can_nv; i++) re->n_weak += (cnt[i] == 1);
  }
  re->size = (int)(lo + ro);
  re->order = g_hrel_n;
  re->goal_hit = 0;   // filled at ranking time, once bindings are clean
  re->dead = 0;
  re->prov = horn_prov_record(ru, re);
  if (re->prov >= 0) g_hf[g_hf_n - 1].src = re->prov;   // the fact above
  g_hrel_n++;
  if (g_hrel_n >= g_horn_max_rel) g_horn_stop = 1;
  return;
  }
  }
reject:
  g_hn_n = hn_mark; g_ha_n = ha_mark;
}

// Backtracking fill: satisfy every condition except skip_pos (already
// bound to the given fact) from PROCESSED facts, under the current
// bindings.  The given-fact discipline enumerates each fact
// combination once -- when its last-processed member is the given.
static void horn_fill(TpProblem *tp, const HRule *ru, int ci, int skip_pos) {
  if (g_horn_stop) return;
  while (ci == skip_pos) ci++;
  if (ci >= ru->nc) { horn_emit_release(tp, ru); return; }
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > g_horn_max_attempts || horn_wall_over() ||
       g_hn_n > g_horn_max_nodes)) { g_horn_stop = 1; return; }
  int c = ru->cl[ci], coff = 0;
  horn_deref(&c, &coff);
  int off = (ci + 1) * HORN_VSPACE;
  // candidate chains: the condition head's bucket (or every bucket for
  // a still-unbound condition head) plus the var-headed facts.
  for (int pass = 0; pass < 2 && !g_horn_stop; pass++) {
    int e;
    if (pass == 0) e = (g_hn[c].head >= 0) ? g_hix_head[g_hn[c].head] : -2;
    else           e = g_hix_varhead;
    if (e == -2) {  // unbound condition head: scan all facts directly
      for (int f = 0; f < g_hf_n && !g_horn_stop; f++) {
        if (!g_hf[f].processed || g_hf[f].dead) continue;
        for (int orient = 0; orient < 2; orient++) {
          int fl = orient ? g_hf[f].r : g_hf[f].l;
          int fr = orient ? g_hf[f].l : g_hf[f].r;
          int mark = g_htrail_n;
          g_horn_attempts++;
          // Wall-only inner poll: the attempt/node caps stay ENTRY-gated
          // (as before this fix) so a fast release that trips the node
          // ceiling mid-loop stops at the exact same point and its proof
          // stays byte-identical; only the wall overrun needed catching
          // inside the loop (one horn_fill can sweep a long candidate
          // chain without re-entry).
          if ((g_horn_attempts & 0xfff) == 0 && horn_wall_over())
            g_horn_stop = 1;
          if (!g_horn_stop &&
              horn_unify(ru->cl[ci], 0, fl, off, 0) &&
              horn_unify(ru->ct[ci], 0, fr, off, 0)) {
            g_hpf[ci] = f;
            horn_fill(tp, ru, ci + 1, skip_pos);
          }
          horn_undo(mark);
        }
      }
      continue;
    }
    for (; e >= 0 && !g_horn_stop; e = g_hix[e].next) {
      int f = g_hix[e].fact;
      if (!g_hf[f].processed || g_hf[f].dead) continue;
      int fl = g_hix[e].orient ? g_hf[f].r : g_hf[f].l;
      int fr = g_hix[e].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      g_horn_attempts++;
      if ((g_horn_attempts & 0xfff) == 0 && horn_wall_over())
        g_horn_stop = 1;
      if (!g_horn_stop &&
          horn_unify(ru->cl[ci], 0, fl, off, 0) &&
          horn_unify(ru->ct[ci], 0, fr, off, 0)) {
        g_hpf[ci] = f;
        horn_fill(tp, ru, ci + 1, skip_pos);
      }
      horn_undo(mark);
    }
  }
}

// One given step: bind condition cpos of ru with the given fact
// (orientation fixed), then fill the remaining conditions from
// processed facts.
static void horn_given_try(TpProblem *tp, const HRule *ru, int cpos,
                           int fl, int fr) {
  int off = (cpos + 1) * HORN_VSPACE;
  int mark = g_htrail_n;
  g_horn_attempts++;
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > g_horn_max_attempts || horn_wall_over() ||
       g_hn_n > g_horn_max_nodes)) { g_horn_stop = 1; return; }
  if (horn_unify(ru->cl[cpos], 0, fl, off, 0) &&
      horn_unify(ru->ct[cpos], 0, fr, off, 0)) {
    g_hpf[cpos] = g_horn_given_ix;
    horn_fill(tp, ru, 0, cpos);
  }
  horn_undo(mark);
}

// Dispatch a given fact through the rule-condition index: only the
// rules whose condition head matches (plus var-headed conditions, and
// every entry when the fact side itself is a bare variable) join.
static void horn_given_dispatch(TpProblem *tp, int given) {
  g_horn_given_ix = given;
  for (int orient = 0; orient < 2 && !g_horn_stop; orient++) {
    int fl = orient ? g_hf[given].r : g_hf[given].l;
    int fr = orient ? g_hf[given].l : g_hf[given].r;
    int h = g_hn[fl].head;
    if (h < 0) {   // var-headed side: any condition could bind it
      for (int e = 0; e < g_hrix_n && !g_horn_stop; e++) {
        if (!g_hr[g_hrix[e].rule].rel) continue;
        horn_given_try(tp, &g_hr[g_hrix[e].rule], g_hrix[e].cpos, fl, fr);
      }
      continue;
    }
    for (int pass = 0; pass < 2 && !g_horn_stop; pass++) {
      int e = (pass == 0) ? g_hrix_head[h] : g_hrix_varhead;
      for (; e >= 0 && !g_horn_stop; e = g_hrix[e].next) {
        if (!g_hr[g_hrix[e].rule].rel) continue;
        horn_given_try(tp, &g_hr[g_hrix[e].rule], g_hrix[e].cpos, fl, fr);
      }
    }
  }
}

static int g_horn_rank_ground = 0;   // THVM_ATP_HORN_GROUND: ground tier on

static int horn_rel_cmp(const void *a, const void *b) {
  const HRel *x = (const HRel *)a, *y = (const HRel *)b;
  if (x->goal_hit != y->goal_hit) return y->goal_hit - x->goal_hit;
  if (x->sg_hit != y->sg_hit) return y->sg_hit - x->sg_hit;   // goal-cone step
  if (g_horn_rank_ground && x->ground != y->ground) return y->ground - x->ground;
  if (x->goal_syms != y->goal_syms) return y->goal_syms - x->goal_syms;
  if (x->n_weak != y->n_weak) return x->n_weak - y->n_weak;   // no weakening vars
  if (x->size != y->size) return x->size - y->size;           // smaller first
  return x->order - y->order;                                 // earlier first
}

// Final selection order: within-group rank FIRST (a round-robin over
// the head-symbol groups), then the quality tiers.  One prolific
// fragment (a ring's additive lemmas) can no longer crowd every other
// signature group out of the kept window; single-group problems are
// unaffected (w then equals the global rank).
static int horn_rel_cmp_rr(const void *a, const void *b) {
  const HRel *x = (const HRel *)a, *y = (const HRel *)b;
  if (x->goal_hit != y->goal_hit) return y->goal_hit - x->goal_hit;
  if (x->sg_hit != y->sg_hit) return y->sg_hit - x->sg_hit;
  if (x->w != y->w) return x->w - y->w;
  return horn_rel_cmp(a, b);
}

// ===================================================================
// Backward goal-directed condensed detachment (--horn-back)
// ===================================================================
// The LCL condensed-detachment family encodes modus ponens over one
// binary connective as a dispatcher rule
//   theorem(implies(Y,X)) & theorem(Y) -> theorem(X)
// plus axiom schemas (axiom(A_k) & rule_1 theorem-ify them) and a
// GROUND goal theorem(G).  Forward chaining floods: it releases ever
// more general theorem SCHEMAS while the goal is one specific ground
// formula, so the schema space explodes before the ground instance is
// reached (231k-1.46M releases, 0 resonance).  This lane regresses the
// goal instead -- an iterative-deepening DFS over the decoded rules
// with the ground goal as the root, so the ground goal anchors every
// subgoal's structure (the major-premise unification is constrained by
// the goal, not generative as in forward chaining).
//
// Soundness: each closure is a real unifier of a subgoal with an axiom
// instance (base) or a rule conclusion (a modus-ponens step); a proved
// GROUND goal is a genuine theorem, memoized and reusable anywhere.  On
// success theorem(G)=true is released as a unit (a real consequence),
// which refutes the negated goal true != theorem(G); g_cli_horn_applied
// gates the Satisfiable claim.  THVM_ATP_HORN_BACK unset/0 disables and
// the whole lane is a no-op (default build byte-identical).

static int    g_bk_on = 0;                 // THVM_ATP_HORN_BACK
static int   *g_bk_bind = NULL;            // per global var id: node or -1
static int    g_bk_bind_cap = 0;
static int   *g_bk_trail = NULL;
static int    g_bk_trail_n = 0, g_bk_trail_cap = 0;
static int    g_bk_nv = 0;                 // next fresh global var id
static int    g_bk_over = 0;               // terminal budget exhaustion
static long   g_bk_nodes = 0, g_bk_node_cap = 0;
static double g_bk_deadline = 0.0;
static int    g_bk_size_cap = 0;
static int    g_bk_goal_hpr[64];           // per goal: derivation record
#define BK_MAXOPEN   256
#define BK_KEYLEN    (2 * TP_SIDE_LEN + 2)
#define BK_DEPTH_MAX 200

typedef struct { int l, r, d, id; } BkGoal;   // id: derivation-log node

static void bk_bind_ensure(int v) {
  if (v < g_bk_bind_cap) return;
  int nc = (g_bk_bind_cap == 0) ? (1 << 14) : g_bk_bind_cap;
  while (nc <= v) nc *= 2;
  g_bk_bind = (int *)realloc(g_bk_bind, (size_t)nc * sizeof(int));
  if (g_bk_bind == NULL) tp_giveup("OOM in Horn back");
  for (int i = g_bk_bind_cap; i < nc; i++) g_bk_bind[i] = -1;
  g_bk_bind_cap = nc;
}
static void bk_trail_push(int v) {
  if (g_bk_trail_n >= g_bk_trail_cap) {
    g_bk_trail_cap = (g_bk_trail_cap == 0) ? (1 << 14) : g_bk_trail_cap * 2;
    g_bk_trail = (int *)realloc(g_bk_trail, (size_t)g_bk_trail_cap * sizeof(int));
    if (g_bk_trail == NULL) tp_giveup("OOM in Horn back");
  }
  g_bk_trail[g_bk_trail_n++] = v;
}
static void bk_undo(int mark) {
  while (g_bk_trail_n > mark) g_bk_bind[g_bk_trail[--g_bk_trail_n]] = -1;
}

// All backward-prover variables are globally unique (standardized apart
// at rule/fact instantiation), so unification runs in one flat space --
// no per-clause offsets.
static int bk_deref(int t) {
  while (g_hn[t].head < 0) {
    int v = -g_hn[t].head - 1;
    if (v >= g_bk_bind_cap || g_bk_bind[v] < 0) break;
    t = g_bk_bind[v];
  }
  return t;
}
static int bk_occurs(int v, int t, int depth) {
  if (depth > 400) return 1;
  t = bk_deref(t);
  if (g_hn[t].head < 0) return (-g_hn[t].head - 1) == v;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (bk_occurs(v, HARG(t, i), depth + 1)) return 1;
  return 0;
}
static int bk_unify(int a, int b, int depth) {
  if (depth > 400) return 0;
  a = bk_deref(a); b = bk_deref(b);
  if (a == b) return 1;
  if (g_hn[a].head < 0) {
    int v = -g_hn[a].head - 1;
    if (bk_occurs(v, b, 0)) return 0;
    bk_bind_ensure(v); g_bk_bind[v] = b; bk_trail_push(v); return 1;
  }
  if (g_hn[b].head < 0) {
    int v = -g_hn[b].head - 1;
    if (bk_occurs(v, a, 0)) return 0;
    bk_bind_ensure(v); g_bk_bind[v] = a; bk_trail_push(v); return 1;
  }
  if (g_hn[a].head != g_hn[b].head || g_hn[a].nargs != g_hn[b].nargs) return 0;
  for (int i = 0; i < g_hn[a].nargs; i++)
    if (!bk_unify(HARG(a, i), HARG(b, i), depth + 1)) return 0;
  return 1;
}

// Standardize a clause term apart: its clause-local variables (head
// -(loc+1), loc < HORN_VSPACE) map to fresh global ids via `map`, which
// the caller shares across the clause's conclusion and conditions so
// their variables link.
static int bk_rename(int t, int *map) {
  if (g_hn[t].head < 0) {
    int loc = -g_hn[t].head - 1;
    if (loc >= HORN_VSPACE) { g_bk_over = 1; return t; }
    if (map[loc] < 0) {
      int fv = g_bk_nv++;
      bk_bind_ensure(fv); g_bk_bind[fv] = -1;
      map[loc] = fv;
    }
    return horn_node(-(map[loc] + 1), 0, NULL);
  }
  int na = g_hn[t].nargs;
  if (na > 16) { g_bk_over = 1; return t; }
  int args[16];
  for (int i = 0; i < na; i++) args[i] = bk_rename(HARG(t, i), map);
  return horn_node(g_hn[t].head, na, args);
}

static int bk_size(int t, int depth) {
  if (depth > 400) return 1 << 20;
  t = bk_deref(t);
  if (g_hn[t].head < 0) return 1;
  int s = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) s += bk_size(HARG(t, i), depth + 1);
  return s;
}
static int bk_is_ground(int t, int depth) {
  if (depth > 400) return 0;
  t = bk_deref(t);
  if (g_hn[t].head < 0) return 0;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (!bk_is_ground(HARG(t, i), depth + 1)) return 0;
  return 1;
}
// Print a ground (deref'd) term canonically into buf; 0 on overflow.
static int bk_print(const TpProblem *tp, char *buf, size_t cap, size_t *o, int t) {
  t = bk_deref(t);
  if (g_hn[t].head < 0) return 0;   // caller guarantees ground
  const char *name = tp->syms[g_hn[t].head].name;
  size_t nl = strlen(name);
  if (*o + nl + 2 >= cap) return 0;
  memcpy(buf + *o, name, nl); *o += nl;
  if (g_hn[t].nargs > 0) {
    buf[(*o)++] = '(';
    for (int i = 0; i < g_hn[t].nargs; i++) {
      if (i > 0) { if (*o + 1 >= cap) return 0; buf[(*o)++] = ','; }
      if (!bk_print(tp, buf, cap, o, HARG(t, i))) return 0;
    }
    if (*o + 1 >= cap) return 0;
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
  return 1;
}

// --- ground-goal memo -------------------------------------------------
// success: a set of ground goal keys proved (a real lemma, reusable at
// any depth).  failure: per key, the largest remaining-depth at which
// the goal was fully explored and failed (a true exhaustion -- never a
// budget cutoff), so any query with <= that depth prunes.
#define BK_MEMO_SLOTS (1 << 20)
// faildepth<0 (=-1): proved.  onpath: this ground goal is an open
// ancestor on the current DFS branch (regularity: re-deriving it is a
// loop -- prune).  hpr: the committed derivation record of a proved
// key (--proof), -1 when none.
typedef struct { char *key; int faildepth; int hpr; u8 onpath; } BkMemo;
static BkMemo *g_bk_memo = NULL;
static int     g_bk_memo_n = 0;

static BkMemo *bk_memo_find(const char *key, int create) {
  if (g_bk_memo == NULL) return NULL;
  u64 h = tp_name_hash(key, strlen(key));
  u32 slot = (u32)(h % (u64)BK_MEMO_SLOTS);
  for (;;) {
    if (g_bk_memo[slot].key == NULL) {
      if (!create) return NULL;
      if ((u32)g_bk_memo_n >= BK_MEMO_SLOTS - (BK_MEMO_SLOTS >> 3)) return NULL;
      g_bk_memo[slot].key = strdup(key);
      g_bk_memo[slot].faildepth = -2;   // fresh, unset
      g_bk_memo[slot].hpr = -1;
      g_bk_memo[slot].onpath = 0;
      g_bk_memo_n++;
      return &g_bk_memo[slot];
    }
    if (strcmp(g_bk_memo[slot].key, key) == 0) return &g_bk_memo[slot];
    slot = (slot + 1u) % BK_MEMO_SLOTS;
  }
}

// Build the ground key for goal (l,r); 0 if either side is non-ground or
// oversize.
static int bk_key(const TpProblem *tp, int l, int r, char *key) {
  size_t o = 0;
  if (!bk_is_ground(l, 0) || !bk_is_ground(r, 0)) return 0;
  if (!bk_print(tp, key, BK_KEYLEN - 2, &o, l)) return 0;
  key[o++] = '=';
  if (!bk_print(tp, key, BK_KEYLEN, &o, r)) return 0;
  return 1;
}

// --- backward-proof derivation log (--proof) ---------------------------
// Under g_prov_on the DFS logs every goal closure (id-keyed entries,
// truncated with the trail on backtrack).  When a GROUND goal commits,
// its self-contained log segment is serialized into g_hpr derivation
// records (bk_pf_commit) and replaced by one committed-record entry,
// so the released theorem(G) = true unit carries a full
// hyper-resolution chain exactly like a forward release.
typedef struct {
  int id, gl, gr;              // goal node + its terms at closure
  int kind, who;               // 0 fact / 1 rule / 2 committed record
  int kids[HORN_MAX_COND];     // rule closures: the condition node ids
  int nkids;
} BkLog;
static BkLog *g_bkl = NULL; static int g_bkl_n = 0, g_bkl_cap = 0;
static int g_bk_id = 0;

static void bk_log_push(int id, int gl, int gr, int kind, int who,
                        const int *kids, int nkids) {
  if (!g_prov_on) return;
  if (g_bkl_n >= g_bkl_cap) {
    g_bkl_cap = (g_bkl_cap == 0) ? (1 << 12) : g_bkl_cap * 2;
    g_bkl = (BkLog *)realloc(g_bkl, (size_t)g_bkl_cap * sizeof(BkLog));
    if (g_bkl == NULL) tp_giveup("OOM in Horn back");
  }
  BkLog *e = &g_bkl[g_bkl_n++];
  e->id = id; e->gl = gl; e->gr = gr; e->kind = kind; e->who = who;
  e->nkids = (nkids > HORN_MAX_COND) ? HORN_MAX_COND : nkids;
  for (int i = 0; i < e->nkids; i++) e->kids[i] = kids[i];
}

// Print a term instance under the CURRENT bindings; leftover free
// variables render canonically per record (V0, V1, ...).
static int bk_print_inst(const TpProblem *tp, char *buf, size_t cap,
                         size_t *o, int t, int *vids, int *nv) {
  t = bk_deref(t);
  if (g_hn[t].head < 0) {
    int v = -g_hn[t].head - 1, slot = -1;
    for (int i = 0; i < *nv; i++) if (vids[i] == v) slot = i;
    if (slot < 0) {
      if (*nv >= HORN_REL_VARS) return 0;
      vids[*nv] = v; slot = (*nv)++;
    }
    if (*o + 12 >= cap) return 0;
    *o += (size_t)snprintf(buf + *o, cap - *o, "V%d", slot);
    return 1;
  }
  const char *name = tp->syms[g_hn[t].head].name;
  size_t nl = strlen(name);
  if (*o + nl + 2 >= cap) return 0;
  memcpy(buf + *o, name, nl); *o += nl;
  if (g_hn[t].nargs > 0) {
    buf[(*o)++] = '(';
    for (int i = 0; i < g_hn[t].nargs; i++) {
      if (i > 0) { if (*o + 1 >= cap) return 0; buf[(*o)++] = ','; }
      if (!bk_print_inst(tp, buf, cap, o, HARG(t, i), vids, nv)) return 0;
    }
    if (*o + 1 >= cap) return 0;
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
  return 1;
}

// Serialize node `id` of the log segment [lo, g_bkl_n).  Returns a
// parent ref (>= 0 g_hpr record, < 0 ~g_hpc input clause) or
// HORN_SRC_NONE on failure.
static int bk_pf_build(const TpProblem *tp, int id, int lo) {
  const BkLog *e = NULL;
  for (int i = g_bkl_n - 1; i >= lo; i--)
    if (g_bkl[i].id == id) { e = &g_bkl[i]; break; }
  if (e == NULL) return HORN_SRC_NONE;
  if (e->kind == 2)
    return (e->who >= 0) ? e->who : HORN_SRC_NONE;
  if (e->kind == 0)
    return (e->who >= 0 && e->who < g_hf_n) ? g_hf[e->who].src
                                            : HORN_SRC_NONE;
  const HRule *ru = (e->who >= 0 && e->who < g_hr_n) ? &g_hr[e->who] : NULL;
  if (ru == NULL || e->nkids != ru->nc) return HORN_SRC_NONE;
  int fps[HORN_MAX_COND];
  for (int i = 0; i < e->nkids; i++) {
    fps[i] = bk_pf_build(tp, e->kids[i], lo);
    if (fps[i] == HORN_SRC_NONE) return HORN_SRC_NONE;
  }
  static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
  size_t ob = 0, orr = 0;
  int vids[HORN_REL_VARS], nv = 0;
  if (!bk_print_inst(tp, lb, sizeof lb, &ob, e->gl, vids, &nv) ||
      !bk_print_inst(tp, rb, sizeof rb, &orr, e->gr, vids, &nv))
    return HORN_SRC_NONE;
  char *sl = strdup(lb), *sr = strdup(rb);
  int ix = horn_prov_new(ru->hpc, fps, e->nkids, sl, sr);
  if (ix < 0) { free(sl); free(sr); return HORN_SRC_NONE; }
  return ix;
}

// Commit the just-proved goal node (bindings still live): serialize
// its subtree; a bare input-clause ref (the goal closed as one fact
// instance) wraps in a record so the result is citable.  Returns the
// g_hpr index or -1.
static int bk_pf_commit(const TpProblem *tp, int id, int gl, int gr, int lo) {
  int ref = bk_pf_build(tp, id, lo);
  if (ref == HORN_SRC_NONE) return -1;
  if (ref >= 0) return ref;
  static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
  size_t ob = 0, orr = 0;
  int vids[HORN_REL_VARS], nv = 0;
  if (!bk_print_inst(tp, lb, sizeof lb, &ob, gl, vids, &nv) ||
      !bk_print_inst(tp, rb, sizeof rb, &orr, gr, vids, &nv)) return -1;
  char *sl = strdup(lb), *sr = strdup(rb);
  int ix = horn_prov_new(~ref, NULL, 0, sl, sr);
  if (ix < 0) { free(sl); free(sr); return -1; }
  return ix;
}

static int bk_budget_ok(void) {
  if (g_bk_over) return 0;
  if ((++g_bk_nodes & 0x3fff) == 0) {
    if (horn_now() > g_bk_deadline) { g_bk_over = 1; return 0; }
  }
  if (g_bk_node_cap > 0 && g_bk_nodes > g_bk_node_cap) { g_bk_over = 1; return 0; }
  if (g_hn_n > (1 << 24)) { g_bk_over = 1; return 0; }   // arena ceiling
  return 1;
}

static int bk_prove_seq(const TpProblem *tp, BkGoal *gs, int n);

// Try every way to close goal (gl,gr) at remaining depth d, then prove
// the pending tail: BASE = a unit fact (an axiom instance); STEP = a
// rule conclusion (a modus-ponens regression), spawning the rule's
// conditions ahead of the tail (major condition first).  `id` is the
// goal's derivation-log node.
static int bk_prove_choices(const TpProblem *tp, int id, int gl, int gr,
                            int d, BkGoal *tail, int tn) {
  // BASE: match a unit fact.  Input clauses arrive in either orientation
  // (true = atom OR atom = true), so try both pairings.
  for (int f = 0; f < g_hf_n && !g_bk_over; f++) {
    for (int orient = 0; orient < 2 && !g_bk_over; orient++) {
      int mark = g_bk_trail_n, an = g_hn_n, aa = g_ha_n, nvm = g_bk_nv;
      int lmark = g_bkl_n;
      int map[HORN_VSPACE];
      for (int i = 0; i < HORN_VSPACE; i++) map[i] = -1;
      int fl = bk_rename(orient ? g_hf[f].r : g_hf[f].l, map);
      int fr = bk_rename(orient ? g_hf[f].l : g_hf[f].r, map);
      if (!g_bk_over && bk_unify(gl, fl, 0) && bk_unify(gr, fr, 0)) {
        bk_log_push(id, gl, gr, 0, f, NULL, 0);
        if (bk_prove_seq(tp, tail, tn)) return 1;
      }
      g_bkl_n = lmark;
      bk_undo(mark); g_hn_n = an; g_ha_n = aa; g_bk_nv = nvm;
      if (horn_eq(g_hf[f].l, g_hf[f].r)) break;  // symmetric: 2nd orient redundant
    }
  }
  if (g_bk_over) return 0;
  // STEP: regress through a rule conclusion (either orientation).
  if (d > 0) {
    for (int r = 0; r < g_hr_n && !g_bk_over; r++) {
      const HRule *ru = &g_hr[r];
      for (int orient = 0; orient < 2 && !g_bk_over; orient++) {
        int mark = g_bk_trail_n, an = g_hn_n, aa = g_ha_n, nvm = g_bk_nv;
        int lmark = g_bkl_n;
        int map[HORN_VSPACE];
        for (int i = 0; i < HORN_VSPACE; i++) map[i] = -1;
        int cl0 = bk_rename(orient ? ru->concl_r : ru->concl_l, map);
        int cr0 = bk_rename(orient ? ru->concl_l : ru->concl_r, map);
        if (!g_bk_over && bk_unify(gl, cl0, 0) && bk_unify(gr, cr0, 0)) {
          BkGoal ng[BK_MAXOPEN];
          int kids[HORN_MAX_COND];
          int m = 0, ok = 1;
          for (int c = 0; c < ru->nc && ok; c++) {
            if (m >= BK_MAXOPEN - tn) { ok = 0; break; }
            ng[m].l = bk_rename(ru->cl[c], map);
            ng[m].r = bk_rename(ru->ct[c], map);
            ng[m].d = d - 1;
            ng[m].id = g_prov_on ? g_bk_id++ : 0;
            kids[c] = ng[m].id;
            m++;
          }
          for (int c = 0; c < tn && ok; c++) {
            if (m >= BK_MAXOPEN) { ok = 0; break; }
            ng[m++] = tail[c];
          }
          if (ok) bk_log_push(id, gl, gr, 1, r, kids, ru->nc);
          if (ok && !g_bk_over && bk_prove_seq(tp, ng, m)) return 1;
        }
        g_bkl_n = lmark;
        bk_undo(mark); g_hn_n = an; g_ha_n = aa; g_bk_nv = nvm;
        if (horn_eq(ru->concl_l, ru->concl_r)) break;  // symmetric
      }
    }
  }
  return 0;
}

// Selection score for the pending subgoal: prefer GROUND (memoizable,
// committed) then the most rigidly-constrained (most non-variable
// structure, fewest variable occurrences).  Decisive for the CD rule
// families whose conditions include a still-free premise theorem(Y)
// alongside a highly-constrained axiom lookup axiom(imp(Y,X)): solving
// the constrained one first binds Y instead of "prove SOME theorem".
static int bk_rigid_score(int t, int depth, int *vars) {
  if (depth > 400) return 0;
  t = bk_deref(t);
  if (g_hn[t].head < 0) { (*vars)++; return 0; }
  int n = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) n += bk_rigid_score(HARG(t, i), depth + 1, vars);
  return n;
}
static int bk_score(int gl) {
  int vars = 0;
  int rigid = bk_rigid_score(gl, 0, &vars);
  if (vars == 0) return 1 << 20;            // ground: pick first
  return rigid * 8 - vars;
}

// Prove the conjunction gs[0..n): select the most-constrained pending
// subgoal, solve it, then the rest.  A conjunction is order-independent
// for correctness, so selection is a pure efficiency lever.  A GROUND
// subgoal is solved in isolation and committed (its proof binds only its
// own fresh variables, none shared with the rest), with memoization; a
// non-ground subgoal backtracks over its solutions against the rest.
static int bk_prove_seq(const TpProblem *tp, BkGoal *gs, int n) {
  if (n == 0) return 1;
  if (!bk_budget_ok()) return 0;
  // Select the best subgoal; move the rest into a contiguous tail.
  int b = 0, best = -(1 << 30);
  for (int i = 0; i < n; i++) {
    int sc = bk_score(bk_deref(gs[i].l));
    if (sc > best) { best = sc; b = i; }
  }
  BkGoal g = gs[b];
  BkGoal tail[BK_MAXOPEN];
  int tn = 0;
  for (int i = 0; i < n; i++) if (i != b) tail[tn++] = gs[i];
  int gl = bk_deref(g.l), gr = bk_deref(g.r);
  int d = (g.d > BK_DEPTH_MAX) ? BK_DEPTH_MAX : g.d;
  if (bk_size(gl, 0) + bk_size(gr, 0) > g_bk_size_cap) return 0;
  if (bk_is_ground(gl, 0) && bk_is_ground(gr, 0)) {
    char key[BK_KEYLEN];
    int have = bk_key(tp, gl, gr, key);
    if (have) {
      BkMemo *mm = bk_memo_find(key, 0);
      if (mm != NULL) {
        if (mm->faildepth == -1) {
          // Reuse: reference the committed derivation record.
          int lmark = g_bkl_n;
          bk_log_push(g.id, gl, gr, 2, mm->hpr, NULL, 0);
          if (bk_prove_seq(tp, tail, tn)) return 1;
          g_bkl_n = lmark;
          return 0;
        }
        if (mm->onpath) return 0;             // regularity: ancestor loop
        if (mm->faildepth >= d) return 0;
      }
    }
    BkMemo *pm = have ? bk_memo_find(key, 1) : NULL;
    if (pm != NULL) pm->onpath = 1;
    int mark = g_bk_trail_n, an = g_hn_n, aa = g_ha_n, nvm = g_bk_nv;
    int lmark = g_bkl_n;
    int ok = bk_prove_choices(tp, g.id, gl, gr, d, NULL, 0);
    // Commit the derivation BEFORE the bindings unwind (the record
    // prints the closure instances), then drop the consumed segment.
    int hpr = (ok && g_prov_on) ? bk_pf_commit(tp, g.id, gl, gr, lmark) : -1;
    bk_undo(mark); g_hn_n = an; g_ha_n = aa; g_bk_nv = nvm;
    g_bkl_n = lmark;
    if (pm != NULL) pm->onpath = 0;
    if (!ok) {
      if (pm != NULL && !g_bk_over && pm->faildepth != -1 && d > pm->faildepth)
        pm->faildepth = d;
      return 0;
    }
    if (pm != NULL) {
      pm->faildepth = -1;       // proved: reusable at any depth
      pm->hpr = hpr;
    }
    // An enclosing rule closure still finds this node id via the
    // committed-record entry.
    bk_log_push(g.id, gl, gr, 2, hpr, NULL, 0);
    return bk_prove_seq(tp, tail, tn);
  }
  return bk_prove_choices(tp, g.id, gl, gr, d, tail, tn);
}

// Identify the goal equation to derive: the negated unit true != theorem(G).
// Returns 1 and sets (*tl,*tr) to (theorem(G), true) oriented for the rule
// conclusions; requires theorem(G) ground.
static int bk_goal_pick(const TpProblem *tp, int gi, int *tl, int *tr) {
  int a = g_hg_l[gi], b = g_hg_r[gi];
  // The `true` side is a nullary constant; the other is the derived atom.
  int atom = -1, tru = -1;
  for (int s = 0; s < 2; s++) {
    int x = s ? b : a, y = s ? a : b;
    if (g_hn[x].head >= 0 && g_hn[x].nargs > 0 && g_hn[y].head >= 0 &&
        g_hn[y].nargs == 0) { atom = x; tru = y; break; }
  }
  if (atom < 0) return 0;
  if (!bk_is_ground(atom, 0)) return 0;   // existential goal: not this lane
  *tl = atom; *tr = tru;
  return 1;
}

// Entry: iterative-deepening backward regression of every ground goal.
// Returns 1 iff some goal is proved (a refutation).
static int horn_backward_prove(const TpProblem *tp) {
  if (g_hr_n == 0 || g_hf_n == 0 || g_hg_n == 0) return 0;
  int depth_max = 40, size_cap = 0;
  {
    const char *de = getenv("THVM_ATP_HORN_BACK_DEPTH");
    const char *se = getenv("THVM_ATP_HORN_BACK_SIZE");
    if (de != NULL && atoi(de) > 0) depth_max = atoi(de);
    if (se != NULL && atoi(se) > 0) size_cap = atoi(se);
  }
  // Default term-size cap: the goal's own size plus generous slack (deep
  // detachment intermediates can exceed the goal, but not without bound).
  int gmax = 0, any = 0;
  for (int gi = 0; gi < g_hg_n; gi++) {
    int tl, tr;
    if (bk_goal_pick(tp, gi, &tl, &tr)) {
      any = 1;
      int s = bk_size(tl, 0) + bk_size(tr, 0);
      if (s > gmax) gmax = s;
    }
  }
  if (!any) return 0;
  if (size_cap == 0) size_cap = gmax + 24;
  g_bk_memo = (BkMemo *)calloc(BK_MEMO_SLOTS, sizeof(BkMemo));
  if (g_bk_memo == NULL) tp_giveup("OOM in Horn back");
  g_bk_memo_n = 0;
  for (int gi = 0; gi < 64; gi++) g_bk_goal_hpr[gi] = -1;
  double wall = 0.85 * g_horn_wall;
  g_bk_deadline = g_horn_t0 + wall;
  g_bk_node_cap = 0;   // wall-governed
  int proved = 0;
  fprintf(stderr, "FindProof: Horn backward CD: %d goal(s), size-cap %d, "
          "depth<=%d, wall %.1fs\n", g_hg_n, size_cap, depth_max, wall);
  for (int D = 1; D <= depth_max && !proved; D++) {
    g_bk_over = 0;
    g_bk_size_cap = size_cap;
    for (int gi = 0; gi < g_hg_n && !proved; gi++) {
      int tl, tr;
      if (!bk_goal_pick(tp, gi, &tl, &tr)) continue;
      // Re-instantiate the goal into a fresh proof space each iteration.
      int an = g_hn_n, aa = g_ha_n, nvm = g_bk_nv, mark = g_bk_trail_n;
      int map[HORN_VSPACE];
      for (int i = 0; i < HORN_VSPACE; i++) map[i] = -1;
      // Goal is ground, so rename is a copy (no vars); keeps the search
      // arena separable from the parsed goal terms.
      BkGoal root;
      root.l = bk_rename(tl, map);
      root.r = bk_rename(tr, map);
      root.d = D;
      root.id = 0;
      g_bkl_n = 0;
      g_bk_id = 1;
      if (!g_bk_over && bk_prove_seq(tp, &root, 1)) {
        proved = 1;
        // The root is ground: its commit landed in the memo -- stash
        // the record for the release site (the memo table frees below).
        if (g_prov_on && gi < 64) {
          char key[BK_KEYLEN];
          BkMemo *mm = bk_key(tp, root.l, root.r, key)
                         ? bk_memo_find(key, 0) : NULL;
          if (mm != NULL && mm->faildepth == -1) g_bk_goal_hpr[gi] = mm->hpr;
        }
      }
      bk_undo(mark); g_hn_n = an; g_ha_n = aa; g_bk_nv = nvm;
    }
    if (g_bk_over && horn_now() > g_bk_deadline) break;   // out of wall
  }
  fprintf(stderr, "FindProof: Horn backward CD: %s (%ld nodes, %d memo, %.1fs)\n",
          proved ? "goal PROVED" : "no proof", g_bk_nodes, g_bk_memo_n,
          horn_now() - g_horn_t0);
  if (g_bk_memo != NULL) {
    for (int i = 0; i < BK_MEMO_SLOTS; i++) free(g_bk_memo[i].key);
    free(g_bk_memo); g_bk_memo = NULL; g_bk_memo_n = 0;
  }
  free(g_bk_bind); g_bk_bind = NULL; g_bk_bind_cap = 0;
  free(g_bk_trail); g_bk_trail = NULL; g_bk_trail_n = g_bk_trail_cap = 0;
  free(g_bkl); g_bkl = NULL; g_bkl_n = g_bkl_cap = 0;
  g_bk_nv = 0;
  return proved;
}

// The pass: decode, chain, rank, append.  Runs after tp_parse and
// BEFORE SInE so released facts participate in axiom selection.
// Standalone dispatcher probe for the --ifeq-guard=2 give-up: does any
// positive clause have the ifeqN(X,X,Y[,Z]) = Y dispatcher shape?
// Reuses the Horn parser (its node arena self-grows) without any of
// tp_horn_release's release machinery, so it works under --horn=0 too.
static int tp_has_ifeq_dispatcher(TpProblem *tp) {
  HParse *ps = (HParse *)malloc(sizeof(HParse));
  if (ps == NULL) return 0;
  int mark_n = g_hn_n, mark_a = g_ha_n, found = 0;
  for (int c = 0; c < tp->n_cls && !found; c++) {
    if (tp->cls[c].neg) continue;
    ps->nv = 0;
    int l = horn_parse_side(tp, ps, tp->cls[c].lhs);
    int r = (l >= 0) ? horn_parse_side(tp, ps, tp->cls[c].rhs) : -1;
    if (l < 0 || r < 0) continue;
    if (horn_disp_check(l, r) || horn_disp_check(r, l)) found = 1;
  }
  g_hn_n = mark_n;   // release the probe's arena nodes
  g_ha_n = mark_a;
  free(ps);
  return found;
}

static void tp_horn_release(TpProblem *tp) {
  int n = tp->n_cls;
  // CD lane flag before any fact is added: fingerprints ride the adds.
  {
    const char *ce = getenv("THVM_ATP_HORN_CD");
    g_horn_cd = (ce != NULL && ce[0] != '0') ? atoi(ce) : 0;
    g_cd_fsub = g_cd_bsub = g_cd_reso_hits = 0;
    g_cd_rounds = 0; g_cd_overcap = 0; g_cd_reso_n = 0;
    const char *be = getenv("THVM_ATP_HORN_BACK");
    g_bk_on = (be != NULL && be[0] != '0') ? atoi(be) : 0;
    g_bk_nodes = 0; g_bk_over = 0;
  }
  // Parse every positive clause; remember each clause's term pair.
  int *pl = (int *)malloc((size_t)n * sizeof(int));
  int *prr = (int *)malloc((size_t)n * sizeof(int));
  int *pnv = (int *)malloc((size_t)n * sizeof(int));
  u8  *is_disp = (u8 *)calloc((size_t)n, 1);
  HParse *ps = (HParse *)malloc(sizeof(HParse));
  if (pl == NULL || prr == NULL || pnv == NULL || is_disp == NULL || ps == NULL)
    tp_giveup("OOM in Horn release");
  g_horn_disp = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
  if (g_horn_disp == NULL) tp_giveup("OOM in Horn release");
  int n_disp = 0;
  g_hg_n = 0;
  for (int c = 0; c < n; c++) {
    pl[c] = prr[c] = -1;
    ps->nv = 0;
    int l = horn_parse_side(tp, ps, tp->cls[c].lhs);
    int r = (l >= 0) ? horn_parse_side(tp, ps, tp->cls[c].rhs) : -1;
    if (l < 0 || r < 0) continue;
    if (tp->cls[c].neg) {
      if (g_hg_n < 64) { g_hg_l[g_hg_n] = l; g_hg_r[g_hg_n] = r; g_hg_n++; }
      continue;
    }
    pl[c] = l; prr[c] = r; pnv[c] = ps->nv;
    if (horn_disp_check(l, r) || horn_disp_check(r, l)) {
      is_disp[c] = 1;
      int f = (g_hn[l].head >= 0) ? g_hn[l].head : g_hn[r].head;
      if (!g_horn_disp[f]) { g_horn_disp[f] = 1; n_disp++; }
    }
  }
  int released_kept = 0;
  if (n_disp == 0) {
    // Slice mode: free the pool slot at once on a non-CD problem.
    if (g_horn_cd >= 2)
      tp_giveup("horn-cd slice: no encoded-Horn dispatcher");
    if (g_bk_on >= 2)
      tp_giveup("horn-back slice: no encoded-Horn dispatcher");
    goto done;   // not an encoded-Horn problem
  }
  if (g_bk_on >= 2) {
    // The backward lane targets ground condensed-detachment goals; the
    // existential (planning) and oversized-KB families crack via the v2
    // steered forward lane in the wm slice.
    int ex = 0;
    for (int g = 0; g < g_hg_n && !ex; g++)
      ex = horn_has_var(g_hg_l[g]) || horn_has_var(g_hg_r[g]);
    if (ex) tp_giveup("horn-back slice: existential goal (v2 lane covers it)");
    if (tp->n_cls > 4000) tp_giveup("horn-back slice: oversized KB");
  }
  if (g_horn_cd >= 2) {
    // The CD discipline targets ground saturation-style goals; the
    // existential (planning) and oversized-KB families already crack
    // via the v2 steered lane in the wm slice.
    int ex = 0;
    for (int g = 0; g < g_hg_n && !ex; g++)
      ex = horn_has_var(g_hg_l[g]) || horn_has_var(g_hg_r[g]);
    if (ex) tp_giveup("horn-cd slice: existential goal (v2 lane covers it)");
    if (tp->n_cls > 4000) tp_giveup("horn-cd slice: oversized KB");
  }
  g_hix_head = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  g_hdup_slots = g_horn_cd ? (HORN_DEDUP_SLOTS << 2) : HORN_DEDUP_SLOTS;
  g_hdup = (char **)calloc(g_hdup_slots, sizeof(char *));
  if (g_hix_head == NULL || g_hdup == NULL) tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) g_hix_head[i] = -1;
  for (int i = 0; i < HORN_NSLOTS; i++) { g_hb[i].t = -1; g_can_map[i] = -1; }
  // Definition pass: collect the definitional unit facts first, so
  // every fact, rule and goal below normalizes into one alphabet.
  g_hdef_l = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  g_hdef_r = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  g_hdef_cls = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  if (g_hdef_l == NULL || g_hdef_r == NULL || g_hdef_cls == NULL)
    tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) {
    g_hdef_l[i] = g_hdef_r[i] = g_hdef_cls[i] = -1;
  }
  // --proof: the clause-index -> snapshot map for provenance capture.
  if (g_prov_on && g_hpc_of == NULL) {
    g_hpc_of = (int *)malloc((size_t)n * sizeof(int));
    if (g_hpc_of == NULL) tp_giveup("OOM in Horn release");
    for (int i = 0; i < n; i++) g_hpc_of[i] = -1;
  }
  int n_defs = 0;
  for (int c = 0; c < n; c++) {
    if (pl[c] < 0 || is_disp[c] || tp->cls[c].neg) continue;
    HRule ru;
    if (horn_decode_side(pl[c], prr[c], &ru) ||
        horn_decode_side(prr[c], pl[c], &ru)) continue;
    horn_def_note(pl[c], prr[c], c);
    horn_def_note(prr[c], pl[c], c);
  }
  for (int i = 0; i < TP_MAX_SYMS; i++) n_defs += (g_hdef_r[i] >= 0);
  // Split parsed positives into rules and facts; seed the dedup set
  // with every original unit (both orientations) so releases never
  // re-add an input equation.
  int n_rules = 0;
  for (int c = 0; c < n; c++) {
    if (pl[c] < 0 || is_disp[c]) continue;
    HRule ru;
    if (horn_decode_side(pl[c], prr[c], &ru) ||
        horn_decode_side(prr[c], pl[c], &ru)) {
      ru.nv = pnv[c];
      ru.rel = 1;
      ru.conj = tp->cls[c].conj;
      ru.ghrel = 0;
      horn_prov_touch_reset();
      for (int i = 0; i < ru.nc; i++) {
        ru.cl[i] = horn_norm(ru.cl[i]);
        ru.ct[i] = horn_norm(ru.ct[i]);
      }
      ru.concl_l = horn_norm(ru.concl_l);
      ru.concl_r = horn_norm(ru.concl_r);
      ru.hpc = horn_prov_cls(tp, c);
      horn_prov_cls_defs(tp, ru.hpc);
      if (g_hr_n >= g_hr_cap) {
        g_hr_cap = (g_hr_cap == 0) ? (1 << 10) : g_hr_cap * 2;
        g_hr = (HRule *)realloc(g_hr, (size_t)g_hr_cap * sizeof(HRule));
        if (g_hr == NULL) tp_giveup("OOM in Horn release");
      }
      g_hr[g_hr_n++] = ru;
      n_rules++;
    } else {
      horn_prov_touch_reset();
      int fl = horn_norm(pl[c]), fr = horn_norm(prr[c]);
      if (!horn_eq(fl, fr)) {          // definitions trivialize themselves
        horn_fact_add(fl, fr, pnv[c]);
        g_hf[g_hf_n - 1].conj = tp->cls[c].conj;
        if (g_prov_on) {
          int j = horn_prov_cls(tp, c);
          horn_prov_cls_defs(tp, j);
          if (j >= 0) g_hf[g_hf_n - 1].src = ~j;
        }
      }
    }
    // dedup seed: canonical print of both orientations
    static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 2];
    for (int orient = 0; orient < 2; orient++) {
      horn_can_reset();
      int hn_mark = g_hn_n, ha_mark = g_ha_n;
      int budget = 1 << 16;
      int a = horn_apply(orient ? prr[c] : pl[c], 0, &budget);
      int b = (a >= 0) ? horn_apply(orient ? pl[c] : prr[c], 0, &budget) : -1;
      size_t lo = 0, ro = 0;
      int remap[HORN_REL_VARS], nv = 0;
      for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
      if (a >= 0 && b >= 0 &&
          horn_print(tp, lb, sizeof lb, &lo, a, remap, &nv) &&
          horn_print(tp, rb, sizeof rb, &ro, b, remap, &nv)) {
        snprintf(key, sizeof key, "%s=%s", lb, rb);
        horn_dedup_add(key);
      }
      g_hn_n = hn_mark; g_ha_n = ha_mark;   // seed applies are temporary
    }
  }
  if (n_rules == 0 || g_hf_n == 0) goto done;
  // Given-fact agenda loop (UR-resolution shape): pop the smallest
  // unprocessed fact, mark it processed, and join it into every rule
  // position against the already-processed facts.  Smallest-first
  // keeps the chaining on small lemmas instead of drowning in deep
  // instantiations, exactly like a weight-ordered given-clause loop.
  g_horn_t0 = horn_now();
  g_horn_stop = 0; g_horn_attempts = 0;
  // CD lane defaults: wall-governed chaining (the deep detachment
  // chains ARE the proof search here, not a preprocessing garnish), so
  // most of the run budget goes to the lane and the attempt cap steps
  // aside.  An explicit --horn-wall still overrides below.
  if (g_horn_cd) {
    double w = 0.8 * g_cli_wall_sec;
    g_horn_wall = (w > HORN_WALL_SEC) ? w : HORN_WALL_SEC;
    g_horn_max_attempts = (long)4e18;
    // Post-subsumption accepts ARE the CD search space: the v1 pool
    // cap ended a healthy cap-18 run mid-wall (400k accepts at 14s).
    g_horn_max_rel = 4000000;
    g_horn_max_nodes = (1 << 24);
  }
  if (g_bk_on) {
    // The backward regression IS the proof search here: give it the wall.
    double w = 0.9 * g_cli_wall_sec;
    g_horn_wall = (w > HORN_WALL_SEC) ? w : HORN_WALL_SEC;
  }
  {
    const char *we = getenv("THVM_ATP_HORN_WALL");
    const char *me = getenv("THVM_ATP_HORN_MAX");
    const char *ae = getenv("THVM_ATP_HORN_ATT");
    if (we != NULL && atof(we) > 0.0) g_horn_wall = atof(we);
    if (me != NULL && atoi(me) > 0) g_horn_max_rel = atoi(me);
    // An EXPLICIT wall governs the run: the 10M attempt cap otherwise
    // stops a deep-KB chain (CSR036: 13k releases, 903 subgoal hits,
    // goal unreached) long before the granted wall, and the
    // relevance-gate relax ladder never engages.
    if (ae != NULL && atol(ae) > 0) g_horn_max_attempts = atol(ae);
    else if (we != NULL && atof(we) > 0.0) g_horn_max_attempts = (long)4e18;
  }
  g_hrix_head = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  if (g_hrix_head == NULL) tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) g_hrix_head[i] = -1;
  for (int i = 0; i < g_hr_n; i++)
    for (int j = 0; j < g_hr[i].nc; j++)
      horn_rule_index(i, j, g_hn[g_hr[i].cl[j]].head);
  // Goals normalize into the releases' (expanded) alphabet BEFORE the
  // loop: the backward cone regresses from them and the emit-time goal
  // probe unifies releases against them.
  for (int g = 0; g < g_hg_n; g++) {
    g_hg_l[g] = horn_norm(g_hg_l[g]);
    g_hg_r[g] = horn_norm(g_hg_r[g]);
  }
  // Backward goal-directed CD lane: regress the ground goal to axiom
  // instances instead of flooding forward.  On success theorem(G)=true
  // is a proved consequence -- release it as a unit (refutes the negated
  // goal true != theorem(G)) and claim the direct SZS route.
  if (g_bk_on && horn_backward_prove(tp)) {
    g_horn_goal_reached = 1;
    g_cli_horn_applied = 1;
    // Emit the proved theorem unit so --proof / the engine reconstruct a
    // refutation (the direct SZS route skips the engine when no proof is
    // requested; the unit is a real consequence either way).  Under
    // --proof the unit carries its committed backward-derivation record
    // (PROV_HORN); a goal whose record failed to commit is withheld --
    // a released theorem citing file() would misrepresent it as a
    // problem formula (CASC-J13).
    for (int gi = 0; gi < g_hg_n && tp->n_cls < TP_MAX_CLAUSES; gi++) {
      int tl, tr;
      if (!bk_goal_pick(tp, gi, &tl, &tr)) continue;
      int hpr = (gi < 64) ? g_bk_goal_hpr[gi] : -1;
      if (g_prov_on && !horn_prov_ok(hpr)) {
        fprintf(stderr, "FindProof: Horn backward CD: proved unit lacks a "
                "complete derivation record -- not released under --proof\n");
        continue;
      }
      static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
      size_t lo = 0, ro = 0;
      if (bk_print(tp, lb, sizeof lb, &lo, tl) &&
          bk_print(tp, rb, sizeof rb, &ro, tr)) {
        TpClause *cl = &tp->cls[tp->n_cls++];
        snprintf(cl->lhs, sizeof cl->lhs, "%s", lb);
        snprintf(cl->rhs, sizeof cl->rhs, "%s", rb);
        cl->neg = 0; cl->conj = 0;
        cl->prov_kind = g_prov_on ? PROV_HORN : PROV_NONE;
        cl->prov_ix = g_prov_on ? hpr : 0;
        tp_scan_term(tp, cl->lhs);
        tp_scan_term(tp, cl->rhs);
      }
    }
    goto done;
  }
  if (g_bk_on >= 2) tp_giveup("horn-back slice: no proof within budget");
  // CD lane: the initial weight cap.  Auto = the largest INPUT fact
  // plus slack; the goal deliberately does NOT enter the max -- the
  // and/equivalent PM goals expand to 25-50 symbols and a goal-sized
  // initial cap admits a space no wall can saturate, so the deepening
  // discipline never engages.  The goal probe is by unification (a
  // small general schema closes a big ground goal) and a goal-sized
  // final conclusion strictly matches a goal resonator, which is
  // cap-exempt.
  if (g_horn_cd) {
    int cap = 0;
    for (int f = 0; f < g_hf_n; f++)
      if (g_hf[f].size > cap) cap = g_hf[f].size;
    g_cd_cap = cap + 2;
    const char *cc = getenv("THVM_ATP_HORN_CD_CAP");
    const char *cs = getenv("THVM_ATP_HORN_CD_STEP");
    const char *cv = getenv("THVM_ATP_HORN_CD_VARS");
    if (cc != NULL && atoi(cc) > 0) g_cd_cap = atoi(cc);
    if (cs != NULL && atoi(cs) > 0) g_cd_step = atoi(cs);
    if (cv != NULL && atoi(cv) > 0) g_cd_maxvars = atoi(cv);
    // Goal-shape resonators from the normalized goals; input facts
    // already goal-shaped (an axiom the goal instantiates) join the
    // priority agenda immediately.
    horn_cd_reso_build();
    for (int f = 0; f < g_hf_n; f++)
      if (horn_cd_resonant(g_hf[f].l, g_hf[f].r)) {
        g_hf[f].sghit = 1;
        horn_heap_push(&g_hpheap, f);
      }
    fprintf(stderr, "FindProof: Horn CD lane: cap %d (step %d, wall %.1fs, "
            "%d resonator(s))\n",
            g_cd_cap, g_cd_step, g_horn_wall, g_cd_reso_n);
  }
  // Oversized KBs: focus the loop on the conjecture's relevance cone.
  // THVM_ATP_HORN_REL=0 disables.
  g_horn_input_nf = g_hf_n;
  {
    const char *rl = getenv("THVM_ATP_HORN_REL");
    const char *te = getenv("THVM_ATP_HORN_REL_TOL");
    if (te != NULL && atof(te) > 0.0) g_horn_gate_tol = atof(te);
    if (!(rl != NULL && rl[0] == '0') && tp->n_cls > 4000) {
      g_horn_gate_on = 1;
      horn_relevance_gate(tp, g_horn_gate_tol, 0);
      // Gated KBs: the relevance-cone chaining IS the credible proof
      // lane (completion drowns in whatever SInE cone fits the engine
      // caps), so it earns a real share of the run wall and the
      // attempt cap steps aside, exactly like the CD lane.  The 2.5s
      // garnish budget stopped CSR036 at 13k releases with the goal a
      // few thousand releases out (goal reached at 24k), and CSR040's
      // gate relax ladder never engaged at all.  Explicit --horn-wall
      // / THVM_ATP_HORN_ATT still override.
      if (getenv("THVM_ATP_HORN_WALL") == NULL && !g_horn_cd && !g_bk_on) {
        double w = 0.4 * g_cli_wall_sec;
        if (w > g_horn_wall) g_horn_wall = w;
        if (getenv("THVM_ATP_HORN_ATT") == NULL)
          g_horn_max_attempts = (long)4e18;
      }
    }
  }
  // Absolute stop deadline for the release: the slice's whole wall,
  // measured from process start.  g_horn_wall (0.4..0.8 of the wall,
  // from g_horn_t0) still shapes the search -- the gate-relax ladder and
  // the phase-2 head-closure trigger are fractions of it, so leaving it
  // untouched keeps a fast release byte-identical.  But g_horn_wall is
  // anchored at the release START, which comes AFTER the clausal pre-pass
  // (up to 0.6 of the wall): g_horn_t0 + 0.4..0.8*wall runs PAST the slice
  // deadline and gets hard-killed with no SZS line (the 240s FOF NoOutput
  // class).  The STOP polls honor this absolute cap too, so the release
  // self-terminates at the slice wall without reshaping the search that
  // finishes before it.  An explicit --horn-wall keeps the old unbounded
  // behavior (a manual single-lane override).
  if (getenv("THVM_ATP_HORN_WALL") == NULL)
    g_horn_abs_deadline = g_cli_main_t0 + g_cli_wall_sec;
  else
    g_horn_abs_deadline = 0.0;
  // Backward subgoal cone (v2): bounded regression from the goals, a
  // wall slice ahead of the forward loop.  Auto-gated (see
  // g_horn_steer); THVM_ATP_HORN_BWD: 0 disables, 2 forces.
  {
    const char *be = getenv("THVM_ATP_HORN_BWD");
    const char *bd = getenv("THVM_ATP_HORN_BWD_DEPTH");
    const char *bm = getenv("THVM_ATP_HORN_BWD_MAX");
    const char *gs = getenv("THVM_ATP_HORN_GOAL_STOP");
    if (bd != NULL && atoi(bd) > 0) g_hsg_depth_cap = atoi(bd);
    if (bm != NULL && atoi(bm) > 0) g_hsg_max = atoi(bm);
    g_horn_goal_stop = !(gs != NULL && gs[0] == '0');
    g_hsg_stop = 0; g_hsg_checks = 0; g_horn_goal_reached = 0;
    int existential = 0;
    for (int g = 0; g < g_hg_n && !existential; g++)
      existential = horn_has_var(g_hg_l[g]) || horn_has_var(g_hg_r[g]);
    g_horn_steer = (be != NULL && be[0] == '2')
                     ? 1 : (g_horn_gate_on || existential);
    if (be != NULL && be[0] == '0') g_horn_steer = 0;
    if (g_horn_steer && g_hg_n > 0) {
      double cw = 0.25 * g_horn_wall;
      g_hsg_deadline = g_horn_t0 + ((cw < 0.6) ? cw : 0.6);
      horn_backward_cone(tp);
      // Input facts already on a subgoal join the priority agenda too.
      for (int f = 0; f < g_hf_n && g_hsg_n > 0; f++)
        if (horn_sg_probe(g_hf[f].l, g_hf[f].r)) {
          g_hf[f].sghit = 1;
          horn_heap_push(&g_hpheap, f);
        }
    }
  }
  // Pick-given ratio (Otter shape): of every 4 pops, HORN_RATIO come
  // from the size heap and the rest by age (FIFO = delta-driven
  // breadth-first).  Pure smallest-first starves any fragment whose
  // small lemmas are unbounded (a ring's additive permutations bury
  // every multiplicative fact), while the age pops keep the frontier
  // fair and reach the DEEP releases (x*0 = 0 needs the 4-condition
  // distributivity joins).  THVM_ATP_HORN_RATIO overrides (0..4).
  {
    // The CD lane is pure smallest-first: its size cap already bounds
    // every fragment, so the age pops that keep v1's frontier fair
    // would only dilute the small-lemma order Otter's CD runs rely on.
    int ratio = g_horn_cd ? 4 : 1;
    const char *re = getenv("THVM_ATP_HORN_RATIO");
    if (re != NULL && re[0] != '\0') ratio = atoi(re);
    if (ratio < 0) ratio = 0;
    if (ratio > 4) ratio = 4;
    int age_cursor = 0;
    long n_pops = 0;
    int gh_admitted = 0;
    for (;;) {
      while (!g_horn_stop) {
        // Phase 2 of the gated-KB wall: the generality-gated cone ran
        // 70% of the budget without closing -- activate the goalward
        // head-closure rules (the definition-shaped bridges generality
        // can never trigger, e.g. CSR040's firstordercollection ->
        // fixedordercollection) and replay every live fact as given so
        // the new rules see the old units (dedup short-circuits the
        // replayed joins, exactly like CD deepening).
        if (g_horn_gate_on && !gh_admitted && !g_horn_goal_reached &&
            (n_pops & 255) == 0 &&
            horn_now() - g_horn_t0 > 0.4 * g_horn_wall) {
          gh_admitted = 1;
          int newr = 0;
          for (int r = 0; r < g_hr_n; r++)
            if (g_hr[r].ghrel && !g_hr[r].rel) { g_hr[r].rel = 1; newr++; }
          if (newr > 0) {
            // Replay INPUT facts only: phase-1 releases stay passive
            // join partners.  A full replay re-pops the phase-1 flood
            // (hundreds of thousands of small releases) ahead of the
            // bridge-crossing input facts and the remaining wall never
            // reaches them; the inputs alone re-fire the newly admitted
            // rules (CSR040: firstordercollection(c) is an input), and
            // each new conclusion enters the agenda as usual.
            for (int f = 0; f < g_horn_input_nf; f++)
              if (!g_hf[f].dead && g_hf[f].processed) {
                g_hf[f].processed = 0;
                g_hf[f].given = 0;
                horn_heap_push(&g_hheap, f);
              }
            age_cursor = 0;
            fprintf(stderr, "FindProof: Horn head-closure phase 2: "
                    "%d rule(s) admitted, input agenda replayed\n", newr);
          }
        }
        int given = -1;
        // Goal-directed pops preempt the ratio scheme: a fact that
        // unifies with a backward subgoal is a candidate step of a
        // goal-closing chain -- joining it first produces the parent
        // subgoal's instance next, walking the cone up to the goal.
        // g_horn_steer keeps this honest: the queue is only ever fed
        // for existential/relevance-gated goals, so the baseline
        // agenda that reaches ground goals on its own (LCL192) stays
        // undiluted.
        do { given = horn_heap_pop(&g_hpheap); }
        while (given >= 0 && (g_hf[given].processed || g_hf[given].dead));
        if (given < 0 && (n_pops++ & 3) >= ratio) {
          while (age_cursor < g_hf_n &&
                 (g_hf[age_cursor].processed || g_hf[age_cursor].dead))
            age_cursor++;
          if (age_cursor < g_hf_n) given = age_cursor;
        }
        if (given < 0) {
          do { given = horn_heap_pop(&g_hheap); }
          while (given >= 0 && (g_hf[given].processed || g_hf[given].dead));
        }
        if (given < 0) {
          while (age_cursor < g_hf_n &&
                 (g_hf[age_cursor].processed || g_hf[age_cursor].dead))
            age_cursor++;
          if (age_cursor >= g_hf_n) break;   // fixpoint: agenda exhausted
          given = age_cursor;
        }
        // CD lane, lazy back subsumption: a fact a LATER release
        // generalizes is dead weight -- its joins only re-derive
        // instances the subsumer covers.  Checked at pop (the only
        // point it would cost anything), tail-scanned (older facts
        // were checked forward at this fact's emit).
        if (g_horn_cd && horn_cd_pop_subsumed(given)) {
          g_hf[given].dead = 1;
          if (given >= g_horn_input_nf)
            g_hrel[given - g_horn_input_nf].dead = 1;
          g_cd_bsub++;
          continue;
        }
        g_hf[given].processed = 1;
        g_hf[given].given = 1;
        horn_given_dispatch(tp, given);
      }
      // CD lane, iterative deepening: the capped space saturated
      // without the goal and the cap (not true saturation) pruned --
      // raise it and re-run every live fact as given again.  The
      // dedup set short-circuits the replayed joins; only conclusions
      // the old cap rejected are new.
      if (g_horn_cd && !g_horn_stop && !g_horn_goal_reached && g_cd_overcap &&
          horn_now() - g_horn_t0 < g_horn_wall) {
        g_cd_cap += g_cd_step;
        g_cd_overcap = 0;
        g_cd_rounds++;
        for (int f = 0; f < g_hf_n; f++)
          if (!g_hf[f].dead && g_hf[f].processed) {
            g_hf[f].processed = 0;
            g_hf[f].given = 0;
            horn_heap_push(&g_hheap, f);
          }
        age_cursor = 0;
        continue;
      }
      // Iterative widening: the RELEVANT closure saturated without the
      // goal and budget remains -- relax the gate, re-run the cone over
      // the newly admitted rules, and continue the loop.
      if (g_horn_stop || !g_horn_gate_on || g_horn_goal_reached) break;
      if (g_horn_gate_tol > 100.0) break;
      g_horn_gate_tol *= 3.0;
      horn_relevance_gate(tp, g_horn_gate_tol, 1);
      if (!gh_admitted) {
        // The relax admitted the head-closure rules: replay the input
        // givens so the new rules see the old units (see phase 2).
        gh_admitted = 1;
        for (int f = 0; f < g_horn_input_nf; f++)
          if (!g_hf[f].dead && g_hf[f].processed) {
            g_hf[f].processed = 0;
            g_hf[f].given = 0;
            horn_heap_push(&g_hheap, f);
          }
      }
      if (g_horn_steer && g_hg_n > 0 && g_hsg_n < g_hsg_max) {
        g_hsg_stop = 0;
        double cd = horn_now() + 0.25 * g_horn_wall;
        g_hsg_deadline = (cd < g_horn_t0 + g_horn_wall)
                           ? cd : g_horn_t0 + g_horn_wall;
        horn_backward_cone(tp);
      }
      age_cursor = 0;   // rescan: parked facts may have re-entered
    }
  }
  // Rank + append.  Under the engine cap keep everything that fits;
  // over it (SInE will run) hand selection up to 256 ranked releases.
  {
    // Goal subsumption: a release some instance of which IS a goal
    // equation closes the refutation in one rewrite -- rank it first.
    // The join's bindings are all undone here, so slot spaces 0
    // (release canonical vars) and HORN_VSPACE (goal vars) are clean.
    // Goals were already normalized into the releases' (expanded)
    // alphabet before the loop.
    int n_goal_hit = 0;
    // Goal-symbol relevance (SInE flavor): the DISCRIMINATING goal
    // symbols are those NOT ubiquitous across the input (frequency
    // <= max(2, n/4) clauses) -- a release mentioning them speaks
    // about the goal's constants and outranks the anonymous
    // variable-lemma flood.
    u32 *gen = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
    u32 *stamp = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
    u8  *gsym = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
    if (gen == NULL || stamp == NULL || gsym == NULL) tp_giveup("OOM in Horn release");
    for (int c = 0; c < n; c++) {
      if (pl[c] < 0) continue;
      int st[512], sp = 0;
      st[sp++] = pl[c]; st[sp++] = prr[c];
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (stamp[g_hn[t].head] != (u32)(c + 1)) {
            stamp[g_hn[t].head] = (u32)(c + 1);
            gen[g_hn[t].head]++;
          }
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
    }
    u32 ubiq = (u32)((n > 8) ? n / 4 : 2);
    for (int g = 0; g < g_hg_n; g++) {
      int st[512], sp = 0;
      st[sp++] = g_hg_l[g]; st[sp++] = g_hg_r[g];
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (gen[g_hn[t].head] <= ubiq) gsym[g_hn[t].head] = 1;
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
    }
    for (int i = 0; i < g_hrel_n; i++) {
      HRel *re = &g_hrel[i];
      int st[512], sp = 0, seen_n = 0;
      int seen[64];
      st[sp++] = re->tl; st[sp++] = re->tr;
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (gsym[g_hn[t].head]) {
            int dup = 0;
            for (int k = 0; k < seen_n; k++) dup |= (seen[k] == g_hn[t].head);
            if (!dup && seen_n < 64) seen[seen_n++] = g_hn[t].head;
          }
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
      re->goal_syms = seen_n;
      re->grp = (g_hn[re->tl].head >= 0) ? g_hn[re->tl].head
                                         : g_hn[re->tr].head;
      if (re->grp < 0) re->grp = 0;
      for (int g = 0; g < g_hg_n && !re->goal_hit; g++) {
        for (int orient = 0; orient < 2 && !re->goal_hit; orient++) {
          int gl = orient ? g_hg_r[g] : g_hg_l[g];
          int gr = orient ? g_hg_l[g] : g_hg_r[g];
          int mark = g_htrail_n;
          if (horn_unify(re->tl, 0, gl, HORN_VSPACE, 0) &&
              horn_unify(re->tr, 0, gr, HORN_VSPACE, 0)) re->goal_hit = 1;
          horn_undo(mark);
        }
      }
      n_goal_hit += re->goal_hit;
    }
    free(gen); free(stamp); free(gsym);
    if (n_goal_hit > 0)
      fprintf(stderr, "FindProof: Horn release: %d goal-subsuming unit(s)\n",
              n_goal_hit);
    int n_pos = 0;
    for (int c = 0; c < tp->n_cls; c++) n_pos += !tp->cls[c].neg;
    int room = (n_pos < (int)PR_MAX_EQNS) ? (int)PR_MAX_EQNS - n_pos
                                          : (int)PR_MAX_EQNS;
    {
      const char *ke = getenv("THVM_ATP_HORN_KEEP");
      const char *ge = getenv("THVM_ATP_HORN_GROUND");
      if (ke != NULL && atoi(ke) > 0 && atoi(ke) < room) room = atoi(ke);
      g_horn_rank_ground = (ge != NULL && ge[0] != '\0' && ge[0] != '0');
    }
    // --proof: released units carry their hyper-resolution derivation
    // record (PROV_HORN) and emit it as the proof's cnf(h<k>, ...)
    // chain, so keeping them is J13-compliant.  A release whose record
    // is TRUNCATED (capture caps hit, or an unrecorded ancestor) is
    // excluded instead -- an axiom leaf citing file() for a derived
    // unit would misrepresent it as a problem formula.
    if (g_hrel_n > room) {
      qsort(g_hrel, (size_t)g_hrel_n, sizeof(HRel), horn_rel_cmp);
      u32 *taken = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
      if (taken == NULL) tp_giveup("OOM in Horn release");
      for (int i = 0; i < g_hrel_n; i++)
        g_hrel[i].w = (int)taken[g_hrel[i].grp]++;
      free(taken);
      qsort(g_hrel, (size_t)g_hrel_n, sizeof(HRel), horn_rel_cmp_rr);
    }
    int n_prov_trunc = 0;
    for (int i = 0, kept = 0; i < g_hrel_n && kept < room; i++) {
      if (g_hrel[i].dead) continue;   // back-subsumed (CD lane)
      if (g_prov_on && !horn_prov_ok(g_hrel[i].prov)) {
        n_prov_trunc++;
        continue;
      }
      if (tp->n_cls >= TP_MAX_CLAUSES) break;
      TpClause *cl = &tp->cls[tp->n_cls++];
      snprintf(cl->lhs, sizeof cl->lhs, "%s", g_hrel[i].l);
      snprintf(cl->rhs, sizeof cl->rhs, "%s", g_hrel[i].r);
      cl->neg = 0; cl->conj = 0;
      cl->prov_kind = g_prov_on ? PROV_HORN : PROV_NONE;
      cl->prov_ix = g_prov_on ? g_hrel[i].prov : 0;
      tp_scan_term(tp, cl->lhs);
      tp_scan_term(tp, cl->rhs);
      released_kept++;
      kept++;
    }
    if (n_prov_trunc > 0)
      fprintf(stderr, "FindProof: Horn release: %d kept release(s) lack a "
              "complete derivation record -- excluded from --proof input\n",
              n_prov_trunc);
  }
  if (released_kept > 0) g_cli_horn_applied = 1;
  {
    int n_sg_hit = 0;
    for (int i = 0; i < g_hrel_n; i++) n_sg_hit += g_hrel[i].sg_hit;
    fprintf(stderr, "FindProof: Horn release: %d dispatcher(s), %d rule(s), "
            "%d fact(s), %d definition(s), %d subgoal(s), %d released "
            "(%d sg-hit%s), %d kept\n",
            n_disp, n_rules, g_hf_n - g_hrel_n, n_defs, g_hsg_n, g_hrel_n,
            n_sg_hit, g_horn_goal_reached ? ", goal reached" : "",
            released_kept);
    if (g_horn_cd)
      fprintf(stderr, "FindProof: Horn CD: cap %d after %d deepening "
              "round(s), %ld fwd-subsumed, %ld back-subsumed, %ld "
              "resonant, %.1fs\n",
              g_cd_cap, g_cd_rounds, g_cd_fsub, g_cd_bsub, g_cd_reso_hits,
              horn_now() - g_horn_t0);
  }
done:
  free(pl); free(prr); free(pnv); free(is_disp); free(ps);
  free(g_horn_disp); g_horn_disp = NULL;
  free(g_hn); g_hn = NULL; g_hn_n = g_hn_cap = 0;
  free(g_ha); g_ha = NULL; g_ha_n = g_ha_cap = 0;
  free(g_hf); g_hf = NULL; g_hf_n = g_hf_cap = 0;
  free(g_dtn); g_dtn = NULL; g_dtn_n = g_dtn_cap = 0;
  free(g_dte); g_dte = NULL; g_dte_n = g_dte_cap = 0;
  free(g_dtl); g_dtl = NULL; g_dtl_n = g_dtl_cap = 0;
  free(g_hix); g_hix = NULL; g_hix_n = g_hix_cap = 0;
  free(g_hix_head); g_hix_head = NULL; g_hix_varhead = -1;
  free(g_hr); g_hr = NULL; g_hr_n = g_hr_cap = 0;
  free(g_hheap.a);  g_hheap.a = NULL;  g_hheap.n = g_hheap.cap = 0;
  free(g_hpheap.a); g_hpheap.a = NULL; g_hpheap.n = g_hpheap.cap = 0;
  free(g_hsg); g_hsg = NULL; g_hsg_n = g_hsg_cap = 0;
  free(g_hdef_l); g_hdef_l = NULL;
  free(g_hdef_r); g_hdef_r = NULL;
  free(g_hdef_cls); g_hdef_cls = NULL;
  free(g_hpc_of); g_hpc_of = NULL;
  free(g_hrix); g_hrix = NULL; g_hrix_n = g_hrix_cap = 0;
  free(g_hrix_head); g_hrix_head = NULL; g_hrix_varhead = -1;
  if (g_hdup != NULL) {
    for (u32 i = 0; i < g_hdup_slots; i++) free(g_hdup[i]);
    free(g_hdup); g_hdup = NULL; g_hdup_n = 0;
  }
  // A recorded release's l/r strings are aliased by its g_hpr record,
  // which the proof emitter reads after the pass -- ownership moves
  // there; unrecorded releases free theirs here.
  for (int i = 0; i < g_hrel_n; i++) {
    if (g_hrel[i].prov >= 0) continue;
    free(g_hrel[i].l); free(g_hrel[i].r);
  }
  free(g_hrel); g_hrel = NULL; g_hrel_n = g_hrel_cap = 0;
}

// ===================================================================
// FOF / clausal-CNF front-end: TPTP first-order input -> Horn ifeq UEQ
// ===================================================================
//
// Parses fof() records (and cnf() records with non-unit / non-
// equational clauses), clausifies through the src/fol pipeline
// (NNF + Skolemization + distribution), and equationalizes the HORN
// fragment with the ifeq encoding -- byte-for-byte the scheme of
// TPTP's '-10' UEQ problem versions ([Sma18], the Claessen-Smallbone
// CADE-2017 encoding).  Verified against LCL306-3 (CNF, non-unit)
// vs the official LCL306-10:
//   * predicate atom P(t...)      ->  the term P(t...), asserted = true
//   * definite clause C | ~G1 | ... | ~Gk  (C, Gi as equations ci=cr,
//     gil=gir after the predicate mapping)
//       ->  ifeq(g1l,g1r, ifeq(g2l,g2r, ... ifeq(gkl,gkr, cl, cr)
//           ..., cr), cr) = cr
//   * one dispatcher axiom        ifeq(A,A,B,C) = B   (iff ifeq used)
//   * a single unit negative clause ~(s=t) is the goal  s != t
//   * otherwise every negative clause ~G1|...|~Gk becomes the rule
//       ifeq(g1l,g1r, ... ifeq(gkl,gkr, false, true) ..., true) = true
//     and the single goal is      false != true
//     (complete for Horn: an unsatisfiable Horn set is refuted by ONE
//     negative clause against the definite subset's least model).
// A conjecture role is universally closed, negated, and clausified
// with the rest; its clauses carry the conjecture mark for SInE / tgt.
//
// Non-Horn clauses (2+ positive literals P1..Pm after tautology
// deletion, negative literals G1..Gk) take the classical or-encoding
// (THVM_ATP_FOF_OR, default on; =0 restores the old clean give-up):
//   * each positive literal reifies to a boolean-valued term: a
//     predicate atom is itself, an equational literal s=t becomes
//     eq(s,t) with a fresh binary eq (--fof-or=2 eq-reifies predicate
//     atoms too, as eq(p(..),true));
//   * the positives or-chain: D = or(P1', or(..., Pm')...) with a
//     fresh binary or;
//   * the negatives wrap D in the same ifeq scheme:
//       ifeq(g1l,g1r, ... ifeq(gkl,gkr, D, true) ..., true) = true
//     (all-positive clause: just D = true).
//
// Bare or-chains cannot cancel a disjunct, so once the lane fires the
// NEGATIVE clauses additionally export their information in rewrite
// form -- together these give the encoding DPLL-style unit
// propagation over or-chains (interreduction rewrites a falsified
// disjunct away, a true one collapses the chain):
//   * a negative UNIT ~G additionally emits  g = false  (predicate
//     atom: p(t..) = false; equational literal ~(s=t): eq(s,t) =
//     false) -- the funnel/goal emission is unchanged;
//   * a negative NON-unit ~G1|...|~Gk additionally emits the chain
//     or(not(G1'), or(..., not(Gk'))...) = true with a fresh unary
//     not (Gi' = the atom, eq(s,t) for an equational literal);
//   * helper axioms, once each if the family is used:
//       or(true,X) = true      or(X,true) = true
//       or(false,X) = X        or(X,false) = X
//       eq(X,X) = true         not(true) = false    not(false) = true
//     and two ifeq bridges that turn DERIVED boolean units back into
//     usable rewrite rules (a critical pair with a derived rule
//     eq(s,t) -> true yields s = t; with not(g) -> true yields
//     g = false):
//       ifeq(eq(X,Y),true,X,Y) = Y
//       ifeq(not(X),true,X,false) = false
//
// SOUNDNESS of the or-lane: take any model M of the original clause
// set over domain D and extend it to D' = D (+) {tt, ff} with
//   true -> tt, false -> ff,
//   p'(a..) -> tt if M |= P(a..) else ff      (predicate reification),
//   eq(a,b) -> tt if a = b else ff            (identity test on D'),
//   or(a,b) -> tt if a = tt or b = tt; else b if a = ff; else a if
//              b = ff; else ff,
//   not(a)  -> ff if a = tt; tt if a = ff; else ff,
//   ifeq(a,b,c,d) -> c if a = b else d,
// original functions/constants as in M (Skolem/Tseitin symbols by
// their defining property).  Every emitted equation is valid in this
// extension: the dispatcher, helpers, and bridges by case analysis
// on the definitions above; a Horn-encoded clause because either
// some guard gil=gir fails (every ifeq level collapses to its else
// branch = rhs) or all hold and the clause's conclusion literal is
// true in M; an or-encoded clause because when all guards hold some
// literal of the clause is true in M, making its reified disjunct tt
// and the or-chain tt; a negative-unit's g = false because the
// clause says exactly that G is false in M.  So the encoded equation
// set is satisfiable whenever the original set is; deriving the goal
// contradiction (the direct goal pair or false=true) therefore
// implies the original set + negated conjecture has no model, and
// the Theorem claim is sound.  The converse fails -- completion has
// no case split, so or-reasoning beyond unit propagation is out of
// reach (a clause whose disjuncts must be analyzed under separate
// assumptions never resolves) -- but failure is GaveUp, never
// CounterSatisfiable: g_cli_fof_applied joins cli_strategy_complete
// (the horn/equals pattern), which is exactly the honest-incomplete
// discipline.
//
// Distribution blow-up: fol_distribute is worst-case exponential, so
// each record's CNF size is estimated first (sum over AND, capped
// product over OR); past THVM_ATP_FOF_DISTRIB (default 512 clauses)
// the record clausifies via fol_tseitin_extract_clauses instead --
// the definitional (Tseitin) encoding is a conservative extension
// (any model of the original interprets each aux predicate by its
// definition), so the soundness argument above carries over with the
// aux predicates treated as ordinary predicates.  Tseitin output is
// mostly non-Horn, so it rides the or-lane.

static int g_cli_fof_applied     = 0;  // encoding ran: GaveUp, never Satisfiable
static int g_cli_fof_conjecture  = 0;  // conjecture role seen: SZS FOF ontology (Theorem)
static int g_cli_fof_trivial     = 0;  // clausification derived the empty clause
static int g_cli_fof_satref      = 0;  // clausal pre-pass derived the empty clause
static int g_cli_want_proof;           // set in main; defined near the extractor

// Does the (preprocessed) text need the clausal front-end?  Any fof()
// record, or any cnf() record whose literal part is not a unit
// (in)equation.  THVM_ATP_FOF=0 forces the old UEQ-only path.
static int tp_needs_clausal(const char *text) {
  {
    const char *e = getenv("THVM_ATP_FOF");
    if (e != NULL && e[0] == '0') return 0;
  }
  for (const char *p = text; (p = strstr(p, "fof(")) != NULL; p += 4)
    if (p == text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) return 1;
  const char *p = text;
  while ((p = strstr(p, "cnf(")) != NULL) {
    if (p != text && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) { p += 4; continue; }
    p += 4;
    int depth = 1;
    const char *body = p;
    while (*p != '\0' && depth > 0) {
      if (*p == '(') depth++;
      else if (*p == ')') depth--;
      p++;
    }
    size_t blen = (size_t)(p - body - 1);
    // literal part = after the 2nd top-level comma
    int d = 0, commas = 0;
    const char *lit = NULL;
    for (size_t i = 0; i < blen; i++) {
      char c = body[i];
      if (c == '(') d++;
      else if (c == ')') d--;
      else if (c == ',' && d == 0 && ++commas == 2) { lit = body + i + 1; break; }
    }
    if (lit == NULL) continue;
    size_t llen = blen - (size_t)(lit - body);
    int eq = 0;
    for (size_t i = 0; i < llen; i++) {
      char c = lit[i];
      if (c == '|' || c == '~') return 1;           // disjunction / negation
      else if (c == '=') eq = 1;                    // covers '!=' too
    }
    if (!eq) return 1;                              // bare predicate literal
  }
  return 0;
}

// A genuine fof( record (word-boundary) in the raw problem text: the
// --fof-slice=2 exit-fast probe.  Same detection as tp_needs_clausal's
// fof( branch, but on the UNpreprocessed text so the schedule's FOF
// slices can free their pool slot before any include resolution or
// front-end work (the tp_has_ifeq_dispatcher cheap-probe shape).
static int tp_input_is_fof(const char *text) {
  for (const char *p = text; (p = strstr(p, "fof(")) != NULL; p += 4)
    if (p == text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) return 1;
  return 0;
}

// Comment-aware marker sniff: does `raw` contain `marker` OUTSIDE any
// %-comment?  TPTP comments run % to end-of-line; problem headers are
// wall-to-wall comment text and regularly contain words like AXIOMS or
// record-keyword substrings (SEV326^5: "GVB-MB-AXIOMS" in a comment
// matched the Waldmeister .pr marker AXIOMS and routed a higher-order
// file to the bench default -- the CASC-J13 bench-default bug class,
// resurrected through a comment).  Every input-format decision below
// must use this, never a bare strstr on raw.
static int tp_raw_marker(const char *raw, const char *marker) {
  size_t mlen = strlen(marker);
  const char *line = raw;
  while (*line != '\0') {
    const char *eol = strchr(line, '\n');
    const char *end = (eol != NULL) ? eol : line + strlen(line);
    const char *pct = memchr(line, '%', (size_t)(end - line));
    const char *stop = (pct != NULL) ? pct : end;
    for (const char *p = line; p + mlen <= stop; p++) {
      if (memcmp(p, marker, mlen) == 0) return 1;
    }
    if (eol == NULL) break;
    line = eol + 1;
  }
  return 0;
}

// --- fresh symbol names against the pc_intern table ------------------
static char g_fof_true_name[TP_NAME_LEN]  = "true";
static char g_fof_false_name[TP_NAME_LEN] = "false";
static char g_fof_ifeq_name[TP_NAME_LEN]  = "ifeq";
static char g_fof_or_name[TP_NAME_LEN]    = "or";
static char g_fof_eq_name[TP_NAME_LEN]    = "eq";
static char g_fof_not_name[TP_NAME_LEN]   = "not";
static char g_fof_sk_prefix[TP_NAME_LEN]  = "sk";
static char g_fof_def_prefix[TP_NAME_LEN] = "def";

static void fof_fresh_const(char *name, size_t cap) {
  while (pc_lookup(name) != PC_NOT_FOUND) {
    size_t l = strlen(name);
    if (l + 2 >= cap) tp_giveup("cannot make a fresh symbol name");
    name[l] = 'q'; name[l + 1] = '\0';
  }
}
// Numbered-family prefix (Skolem sk<n>, Tseitin def<n>): no interned
// symbol may be <prefix><digits>.
static void fof_fresh_prefix(char *prefix, size_t cap) {
  for (;;) {
    size_t plen = strlen(prefix);
    int clash = 0;
    for (int i = 0; i < pc_nsyms && !clash; i++) {
      const char *nm = pc_sym[i];
      if (strncmp(nm, prefix, plen) != 0) continue;
      const char *d = nm + plen;
      if (*d == '\0') continue;
      while (*d >= '0' && *d <= '9') d++;
      if (*d == '\0') clash = 1;
    }
    if (!clash) return;
    if (plen + 2 >= cap)
      tp_giveup("cannot make a fresh symbol-family prefix");
    prefix[plen] = 'q'; prefix[plen + 1] = '\0';
  }
}

// --- clause-side serializer: fol Term -> .pr-ready text ---------------
typedef struct {
  u32 id[PC_MAX_VARS];   // clause-local variable ids, first-occurrence order
  int n;
} FofVarScope;

// Oversize discipline: inside the axiom-side emitters
// (g_fof_soft_oversize) an encoding that overflows the serializer
// buffer or the .pr side bound DROPS that clause instead of giving up
// the run.  Dropping an input clause only shrinks the set -- a subset
// refutation is still a refutation, and the FOF path never reports
// Satisfiable -- so the cost is completeness the run never had, where
// the hard give-up cost the WHOLE problem (the Mizar SET007 families
// carry a handful of giant definition clauses whose ifeq towers blow
// the 2048-char side bound; every slice died on the first one).  The
// goal-side pushes (direct goal, or-lane helpers, dispatcher) keep
// the hard give-up: an unrepresentable goal has no honest Theorem.
static int g_fof_soft_oversize = 0;   // 1 inside axiom-side emitters
static int g_fof_ser_overflow  = 0;   // current clause overflowed: junk
static int g_fof_dropped_n     = 0;   // oversized clauses dropped

static void fof_ser_emit(char *buf, size_t cap, size_t *o, const char *s) {
  if (g_fof_ser_overflow) return;
  size_t l = strlen(s);
  if (*o + l + 1 >= cap) {
    if (g_fof_soft_oversize) { g_fof_ser_overflow = 1; return; }
    tp_giveup("FOF-encoded clause exceeds side-length bound");
  }
  memcpy(buf + *o, s, l + 1);
  *o += l;
}

static void fof_ser_term(Term t, FofVarScope *sc, char *buf, size_t cap, size_t *o) {
  if (term_tag(t) == TAG_FVR) {
    u32 vid = term_ext(t);
    int ix = -1;
    for (int i = 0; i < sc->n; i++) if (sc->id[i] == vid) { ix = i; break; }
    if (ix < 0) {
      if (sc->n >= (int)PC_MAX_VARS) tp_giveup("FOF clause exceeds variable bound");
      ix = sc->n;
      sc->id[sc->n++] = vid;
    }
    char nm[16];
    snprintf(nm, sizeof nm, "X%d", ix);
    fof_ser_emit(buf, cap, o, nm);
    return;
  }
  if (term_tag(t) != TAG_CTR) tp_giveup("unsupported term in clausified formula");
  u32 lab = term_ext(t);
  char nm[TP_NAME_LEN + 16];
  if (lab >= FOL_LAB_SKOLEM_BASE) {
    snprintf(nm, sizeof nm, "%s%u", g_fof_sk_prefix, lab - FOL_LAB_SKOLEM_BASE);
  } else if (fol_is_tseitin_aux(lab)) {
    snprintf(nm, sizeof nm, "%s%u", g_fof_def_prefix, lab - FOL_LAB_TSEITIN_BASE);
  } else if (lab >= PC_LAB_BASE && lab < PC_LAB_BASE + (u32)pc_nsyms) {
    snprintf(nm, sizeof nm, "%s", pc_sym[lab - PC_LAB_BASE]);
  } else {
    tp_giveup("unsupported symbol in clausified formula");
    return;
  }
  fof_ser_emit(buf, cap, o, nm);
  u32 n = term_ctr_n(t);
  if (n == 0u) return;
  fof_ser_emit(buf, cap, o, "(");
  for (u32 i = 0; i < n; i++) {
    if (i > 0) fof_ser_emit(buf, cap, o, ",");
    fof_ser_term(term_ctr_at(t, i), sc, buf, cap, o);
  }
  fof_ser_emit(buf, cap, o, ")");
}

// --- literal classification -------------------------------------------
// A literal maps to an equation pair (l, r): an equality atom keeps its
// sides; a predicate atom P(t...) pairs with the fresh `true` constant
// (r == 0 marks it, serialized late so the fresh name is final).
// Verdicts for $true/$false atoms: DROP the literal or the whole
// clause (tautology).
enum { FOF_LIT_PAIR, FOF_LIT_DROP_LIT, FOF_LIT_DROP_CLAUSE };

static int fof_lit_classify(const FolLit *lit, Term *l, Term *r) {
  Term a = lit->atom;
  if (term_tag(a) == TAG_FVR)
    tp_giveup("a bare variable is not a first-order atom");
  if (term_tag(a) != TAG_CTR) tp_giveup("unsupported atom in clausified formula");
  u32 lab = term_ext(a);
  if (lab == PC_LAB_TRUE)
    return lit->sign ? FOF_LIT_DROP_LIT : FOF_LIT_DROP_CLAUSE;
  if (lab == PC_LAB_FALSE)
    return lit->sign ? FOF_LIT_DROP_CLAUSE : FOF_LIT_DROP_LIT;
  if (fol_is_connective(lab))
    tp_giveup("clausification left a connective inside an atom");
  if (lab == PC_LAB_EQ) {
    *l = term_ctr_at(a, 0u);
    *r = term_ctr_at(a, 1u);
    return FOF_LIT_PAIR;
  }
  *l = a;
  *r = 0;   // predicate atom: right side is the fresh `true` constant
  return FOF_LIT_PAIR;
}

// --- clause emission ---------------------------------------------------

// Stamp applied by tp_fof_push to the clause it appends; call sites
// set it immediately before each push.  Left at 0/0 outside --proof
// FOF runs, so every other path stays byte-identical.
static int g_cur_prov_kind = PROV_NONE;
static int g_cur_prov_ix   = 0;

static void tp_fof_push(TpProblem *tp, const char *lhs, const char *rhs,
                        int neg, int conj) {
  if (g_fof_ser_overflow ||
      strlen(lhs) >= TP_SIDE_LEN || strlen(rhs) >= TP_SIDE_LEN) {
    if (g_fof_soft_oversize) {   // axiom-side: drop, don't die (see
      g_fof_ser_overflow = 0;    // fof_ser_emit's oversize discipline)
      g_fof_dropped_n++;
      return;
    }
    tp_giveup("FOF-encoded clause exceeds side-length bound");
  }
  if (tp->n_cls >= TP_MAX_CLAUSES) tp_giveup("problem exceeds clause bound");
  TpClause *cl = &tp->cls[tp->n_cls++];
  snprintf(cl->lhs, sizeof cl->lhs, "%s", lhs);
  snprintf(cl->rhs, sizeof cl->rhs, "%s", rhs);
  cl->neg = neg;
  cl->conj = conj;
  cl->prov_kind = g_cur_prov_kind;
  cl->prov_ix   = g_cur_prov_ix;
}

// Serialize one Horn clause as a single unit equation.  `conds` are the
// negative literals' pairs in clause order; the conclusion pair is
// (cl, cr).  cr==0 / cond r==0 stand for the fresh `true` constant;
// cl==0 (with cr==0) is the fresh `false` constant (the funnel rule).
static void fof_emit_horn(TpProblem *tp, Term *cond_l, Term *cond_r, int nc,
                          Term cl, Term cr, int neg, int conj) {
  static char lbuf[TP_SIDE_LEN * 4];
  static char rbuf[TP_SIDE_LEN * 4];
  FofVarScope sc;
  sc.n = 0;
  size_t lo = 0, ro = 0;
  for (int i = 0; i < nc; i++) {
    fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_ifeq_name);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    fof_ser_term(cond_l[i], &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
    if (cond_r[i] == 0) fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_true_name);
    else fof_ser_term(cond_r[i], &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
  }
  // innermost then-branch: the conclusion's left side
  if (cl == 0) fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_false_name);
  else fof_ser_term(cl, &sc, lbuf, sizeof lbuf, &lo);
  // right side (and each level's else-branch): the conclusion's right side
  if (cr == 0) fof_ser_emit(rbuf, sizeof rbuf, &ro, g_fof_true_name);
  else fof_ser_term(cr, &sc, rbuf, sizeof rbuf, &ro);
  for (int i = 0; i < nc; i++) {
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
    fof_ser_emit(lbuf, sizeof lbuf, &lo, rbuf);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
  }
  tp_fof_push(tp, lbuf, rbuf, neg, conj);
}

// --- non-Horn or-lane ---------------------------------------------------
// THVM_ATP_FOF_OR: 0 = off (Horn-only give-up), 1 = on with predicate
// atoms contributed directly when the whole disjunction is predicate
// atoms (default), 2 = on with every disjunct eq-reified.
static int fof_or_mode(void) {
  const char *e = getenv("THVM_ATP_FOF_OR");
  if (e == NULL || *e == '\0') return 1;
  return atoi(e);
}
static int g_fof_or_used  = 0;  // or-encoded clause emitted (helpers due)
static int g_fof_eq_used  = 0;  // an eq-reified disjunct emitted
static int g_fof_not_used = 0;  // a not-reified negative literal emitted
static int g_fof_or_ncl   = 0;  // or-encoded clause count (summary line)

// Reify one literal pair as a boolean-valued term: a predicate atom
// is itself, an equational pair goes through eq(l,r).  force_eq
// (--fof-or=2) eq-reifies predicate atoms too, as eq(p(..),true).
static void fof_ser_reified(Term l, Term r, int force_eq, FofVarScope *sc,
                            char *buf, size_t cap, size_t *o) {
  if (r == 0 && !force_eq) {
    fof_ser_term(l, sc, buf, cap, o);
    return;
  }
  g_fof_eq_used = 1;
  fof_ser_emit(buf, cap, o, g_fof_eq_name);
  fof_ser_emit(buf, cap, o, "(");
  fof_ser_term(l, sc, buf, cap, o);
  fof_ser_emit(buf, cap, o, ",");
  if (r == 0) fof_ser_emit(buf, cap, o, g_fof_true_name);
  else fof_ser_term(r, sc, buf, cap, o);
  fof_ser_emit(buf, cap, o, ")");
}

// Serialize one non-Horn clause: negative literals wrap the positive
// disjunction D in the ifeq scheme (else branch / rhs = true).
static void fof_emit_or(TpProblem *tp, Term *cond_l, Term *cond_r, int nc,
                        Term *pos_l, Term *pos_r, int np, int conj) {
  static char lbuf[TP_SIDE_LEN * 4];
  FofVarScope sc;
  sc.n = 0;
  size_t lo = 0;
  int force_eq = (fof_or_mode() == 2);
  for (int i = 0; i < nc; i++) {
    fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_ifeq_name);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    fof_ser_term(cond_l[i], &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
    if (cond_r[i] == 0) fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_true_name);
    else fof_ser_term(cond_r[i], &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
  }
  // D = or(P1', or(..., Pm')...): right-nested or-chain of the
  // reified positives.
  for (int i = 0; i < np; i++) {
    if (i < np - 1) {
      fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_or_name);
      fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    }
    fof_ser_reified(pos_l[i], pos_r[i], force_eq, &sc, lbuf, sizeof lbuf, &lo);
    if (i < np - 1) fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
  }
  for (int i = 0; i < np - 1; i++)
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
  for (int i = 0; i < nc; i++) {
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
    fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_true_name);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
  }
  tp_fof_push(tp, lbuf, g_fof_true_name, 0, conj);
  g_fof_or_used = 1;
  g_fof_or_ncl++;
}

// Negative-clause rewrite export (or-lane only): a negative unit ~G
// emits g = false, a negative non-unit ~G1|...|~Gk emits the
// not()-reified or-chain.  Both ADD to the funnel/goal emission
// (which stays byte-identical), giving or-chains unit propagation.
static void fof_emit_neg_extra(TpProblem *tp, Term *lit_l, Term *lit_r,
                               int nl, int conj) {
  static char lbuf[TP_SIDE_LEN * 4];
  FofVarScope sc;
  sc.n = 0;
  size_t lo = 0;
  if (nl == 1) {
    fof_ser_reified(lit_l[0], lit_r[0], 0, &sc, lbuf, sizeof lbuf, &lo);
    tp_fof_push(tp, lbuf, g_fof_false_name, 0, conj);
    return;
  }
  g_fof_not_used = 1;
  for (int i = 0; i < nl; i++) {
    if (i < nl - 1) {
      fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_or_name);
      fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    }
    fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_not_name);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    fof_ser_reified(lit_l[i], lit_r[i], 0, &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
    if (i < nl - 1) fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
  }
  for (int i = 0; i < nl - 1; i++)
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
  tp_fof_push(tp, lbuf, g_fof_true_name, 0, conj);
}

#include "atp_cli/provenance.c"
#include "atp_cli/fof_split.c"
#include "atp_cli/szs.c"
#include "atp_cli/main.c"
