// thvmlink_atp.c -- ATP-related LibraryLink entries.
//
// Sibling of thvmlink.c; included via `#include "thvmlink_atp.c"`
// so the single-TU build is preserved.  Exports:
//
//   thvm_wl_term_new_ctr          -- 8.7c CTR-builder for the WL
//                                    encoder.
//   thvm_wl_atp_run               -- 8.7b universal-goal runner.
//   thvm_wl_atp_run_existential   -- 8.9e narrow-mode runner that
//                                    extracts witness bindings.
//   thvm_wl_atp_run_all_witnesses -- 9.1c multi-witness DFS.
//   thvm_wl_atp_run_file          -- 9.2 .pr file driver.
//
// All entries assume `thvmlink.c` has already pulled in
// `../../../src/thvm.c` (so `Term`, `AtpState`, KBO/LPO, etc., are
// in scope).

// Forward WL Abort[] / TimeConstrained[] into the ATP engine.  The C
// saturation loop is otherwise uninterruptible; it polls
// thvm_atp_abort_hook, which we point at the current evaluation's
// AbortQ for the duration of a run so a host abort returns promptly.
#ifdef __APPLE__
#include <pthread.h>
#include <sys/qos.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#endif
#include <time.h>
static WolframLibraryData g_atp_abort_libData = NULL;
static int atp_abort_cb(void) {
  return (g_atp_abort_libData != NULL) ? g_atp_abort_libData->AbortQ() : 0;
}

// === 8.7c: CTR-builder helper for the WL-side ATP encoder ========
//
// Builds a TAG_CTR Term whose children come from a list of
// pre-built Term integer values.  Returns the new Term's raw
// integer value.
//
// Inputs:
//   args[0] = label (mint).
//   args[1] = children MTensor (Integer rank-1); empty list for
//             nullary CTR.
//
// Output: mint = packed Term value.
//
// Uses MTensor (rather than NumericArray) for the children list
// because empty MTensor[] flows cleanly through LibraryLink type
// coercion.
EXTERN_C DLLEXPORT int thvm_wl_term_new_ctr(WolframLibraryData libData,
                                            mint argc, MArgument *args,
                                            MArgument res) {
  (void)argc;
  mint label = MArgument_getInteger(args[0]);
  MTensor t  = MArgument_getMTensor(args[1]);
  mint n     = libData->MTensor_getFlattenedLength(t);
  const mint *data = libData->MTensor_getIntegerData(t);

  Term children[REWRITE_MAX_ARITY];
  if ((u32)n > REWRITE_MAX_ARITY) return LIBRARY_FUNCTION_ERROR;
  for (mint i = 0; i < n; i++) children[i] = (Term)data[i];

  Term out = term_new_ctr((u32)label, children, (u32)n);
  MArgument_setInteger(res, (mint)out);
  return LIBRARY_NO_ERROR;
}

// === 8.7b: ATP runner via LibraryLink ============================
//
// Inputs:
//   args[0] = MNumericArray (Int64) of packed Term values:
//             [n_axioms, lhs_0, rhs_0, lhs_1, rhs_1, ...,
//              lhs_{n-1}, rhs_{n-1}, goal_lhs, goal_rhs,
//              flag_0, flag_1, ..., flag_{n-1}].
//             Length = 1 + 2*n_axioms + 2 + n_axioms = 3*n_axioms + 3.
//             flag_i: 0 = equation (engine orients via KBO),
//                     1 = pre-oriented (use lhs -> rhs directly).
//             The flag tail is APPENDED so every existing reader's
//             lhs/rhs/goal offset stays valid; only the length check
//             and the axiom-add loops change.
//   args[1] = max_steps  (mint)
//   args[2] = max_label  (mint; sizes the trivial precedence /
//             weights tables.  v0 uses a uniform config that
//             gives KBO_UN for most comparisons -- the saturator
//             falls into unfailing fallback.  Future stages can
//             pass a real precedence array.)
//
// Output: MNumericArray (Int64) `[status, n_rules, n_trace, n_cps]`.
//
// Designed for direct WL test usage with manually-built Terms
// (per the 8.7a memo's two-layer plan).  Stage 8.7c-d add the
// WL-side encoder + TATP[] surface.
#define ATP_WL_CFG_MAX_LABELS 64
// === 8.9e: existential ATP runner ================================
//
// Mirrors thvm_wl_atp_run but takes an extra `witness_ids`
// MTensor (1-D Integer): the user-declared FVR ids whose
// bindings should be returned.  Output array grows by `n_witness`
// trailing Term values (raw Int64 packed Term form).
//
// Inputs:
//   args[0] = packed_terms NumericArray (Int64), same layout as
//             thvm_wl_atp_run.
//   args[1] = max_steps     (mint).
//   args[2] = max_label     (mint).
//   args[3] = witness_ids   MTensor (Integer rank-1).
//
// Output: Int64 NumericArray of length 4 + n_witness:
//   [status, n_rules, n_trace, n_cps,
//    witness_term_0, witness_term_1, ...].
EXTERN_C DLLEXPORT int thvm_wl_atp_run_existential(
    WolframLibraryData libData, mint argc,
    MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na  = MArgument_getMNumericArray(args[0]);
  mint max_steps    = MArgument_getInteger(args[1]);
  mint max_label    = MArgument_getInteger(args[2]);
  MTensor witness_t = MArgument_getMTensor(args[3]);

  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;
  if (naf->MNumericArray_getType(na) != MNumericArray_Type_Bit64) {
    return LIBRARY_FUNCTION_ERROR;
  }
  mint flat_len = naf->MNumericArray_getFlattenedLength(na);
  if (flat_len < 3) return LIBRARY_FUNCTION_ERROR;
  const int64_t *data = (const int64_t *)naf->MNumericArray_getData(na);

  int64_t n_ax_i = data[0];
  if (n_ax_i < 0 || (int64_t)flat_len != 3 * n_ax_i + 3) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 n_ax = (u32)n_ax_i;

  mint n_witness         = libData->MTensor_getFlattenedLength(witness_t);
  const mint *witness_ids = libData->MTensor_getIntegerData(witness_t);

  if ((u32)max_label >= ATP_WL_CFG_MAX_LABELS) {
    return LIBRARY_FUNCTION_ERROR;
  }
  static u32 wl_weights2[ATP_WL_CFG_MAX_LABELS];
  static u32 wl_precedence2[ATP_WL_CFG_MAX_LABELS];
  for (u32 i = 0; i < (u32)max_label + 1; i++) {
    wl_weights2[i] = 1;
    wl_precedence2[i] = i + 1;
  }
#ifdef ATP_AUTO_PREC
  // Waldmeister-style auto-precedence: replace the syntactic
  // `i+1` precedence with one derived from per-operator algebraic-
  // property analysis of the axiom set (PhilMarlow /
  // Praezedenzgenerator port, src/atp/precedence.c).
  {
    static Term ax_lhs[ATP_WL_CFG_MAX_LABELS];
    static Term ax_rhs[ATP_WL_CFG_MAX_LABELS];
    u32 n_ax_use = n_ax < ATP_WL_CFG_MAX_LABELS ? n_ax
                                                : ATP_WL_CFG_MAX_LABELS;
    for (u32 i = 0; i < n_ax_use; i++) {
      ax_lhs[i] = (Term)data[1 + 2 * i + 0];
      ax_rhs[i] = (Term)data[1 + 2 * i + 1];
    }
    atp_auto_precedence(ax_lhs, ax_rhs, n_ax_use,
                        (u32)max_label + 1, wl_precedence2);
  }
#endif
  static KboConfig wl_kbo2;
  wl_kbo2.weights    = wl_weights2;
  wl_kbo2.precedence = wl_precedence2;
  wl_kbo2.n_labels   = (u32)max_label + 1;
  wl_kbo2.var_weight = 1;

  AtpState *atp = thvm_atp_init(&wl_kbo2, (u32)max_steps);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;

  for (u32 i = 0; i < n_ax; i++) {
    Term lhs  = (Term)data[1 + 2 * i + 0];
    Term rhs  = (Term)data[1 + 2 * i + 1];
    int64_t f = data[1 + 2 * n_ax + 2 + i];
    if (f == 1) {
      // Pre-oriented: install returns count=0 on duplicate (benign),
      // count=1 on success.  Either is fine for the FFI -- only a NULL
      // state would matter, which we already checked.
      (void)thvm_atp_install_oriented_rule(atp, lhs, rhs);
    } else if (!thvm_atp_add_equation(atp, lhs, rhs)) {
      thvm_atp_free(atp);
      return LIBRARY_FUNCTION_ERROR;
    }
  }
  Term goal_lhs = (Term)data[1 + 2 * n_ax + 0];
  Term goal_rhs = (Term)data[1 + 2 * n_ax + 1];
  if (!thvm_atp_set_goal_existential(atp, goal_lhs, goal_rhs)) {
    thvm_atp_free(atp);
    return LIBRARY_FUNCTION_ERROR;
  }

  AtpStatus st = thvm_atp_run(atp);

  // Pack stats + witness terms.
  mint dims[1] = {4 + n_witness};
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_Bit64, 1, dims, &out);
  int64_t *odata = (int64_t *)naf->MNumericArray_getData(out);
  odata[0] = (int64_t)st;
  odata[1] = (int64_t)atp->n_rules;
  odata[2] = (int64_t)atp->n_trace;
  odata[3] = (int64_t)atp->n_cps;
  for (mint i = 0; i < n_witness; i++) {
    Term wt = thvm_atp_get_witness(atp, (u32)witness_ids[i]);
    odata[4 + i] = (int64_t)wt;
  }

  thvm_atp_free(atp);
  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_atp_run(WolframLibraryData libData, mint argc,
                                       MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  mint max_steps   = MArgument_getInteger(args[1]);
  mint max_label   = MArgument_getInteger(args[2]);

  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;
  if (naf->MNumericArray_getType(na) != MNumericArray_Type_Bit64) {
    return LIBRARY_FUNCTION_ERROR;
  }
  mint flat_len = naf->MNumericArray_getFlattenedLength(na);
  if (flat_len < 3) return LIBRARY_FUNCTION_ERROR;
  const int64_t *data = (const int64_t *)naf->MNumericArray_getData(na);

  int64_t n_ax_i = data[0];
  if (n_ax_i < 0 || (int64_t)flat_len != 3 * n_ax_i + 3) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 n_ax = (u32)n_ax_i;

  // Build a trivial KboConfig: uniform weight=1, precedence=label.
  // Most comparisons return KBO_UN, falling into unfailing fallback;
  // this is functionally correct but inefficient.  Future stages can
  // accept a real precedence + weights array.
  if ((u32)max_label >= ATP_WL_CFG_MAX_LABELS) {
    return LIBRARY_FUNCTION_ERROR;
  }
  static u32 wl_weights[ATP_WL_CFG_MAX_LABELS];
  static u32 wl_precedence[ATP_WL_CFG_MAX_LABELS];
  for (u32 i = 0; i < (u32)max_label + 1; i++) {
    wl_weights[i] = 1;
    wl_precedence[i] = i + 1;
  }
  static KboConfig wl_kbo;
  wl_kbo.weights    = wl_weights;
  wl_kbo.precedence = wl_precedence;
  wl_kbo.n_labels   = (u32)max_label + 1;
  wl_kbo.var_weight = 1;

  AtpState *atp = thvm_atp_init(&wl_kbo, (u32)max_steps);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;

  // Push axioms.  flag_i (tail block) dispatches between equation
  // (engine orients via KBO) and pre-oriented (lhs -> rhs directly).
  for (u32 i = 0; i < n_ax; i++) {
    Term lhs  = (Term)data[1 + 2 * i + 0];
    Term rhs  = (Term)data[1 + 2 * i + 1];
    int64_t f = data[1 + 2 * n_ax + 2 + i];
    if (f == 1) {
      (void)thvm_atp_install_oriented_rule(atp, lhs, rhs);
    } else if (!thvm_atp_add_equation(atp, lhs, rhs)) {
      thvm_atp_free(atp);
      return LIBRARY_FUNCTION_ERROR;
    }
  }
  // Set goal (allow 0/0 to mean "completion mode").
  Term goal_lhs = (Term)data[1 + 2 * n_ax + 0];
  Term goal_rhs = (Term)data[1 + 2 * n_ax + 1];
  // Completion mode: a (0, 0) conjecture pair means "no goal" -- the
  // engine then saturates the axioms until the queue empties (a finite
  // complete system) or the step/wall budget is hit, and the caller
  // reads the derived rule set (MainRules) as the completion lemmas.
  // goal_check returns ATP_RUNNING whenever no goal is set, so the run
  // is bounded only by MaxSteps / TimeConstraint.
  if (goal_lhs != 0u || goal_rhs != 0u) {
    thvm_atp_set_goal(atp, goal_lhs, goal_rhs);
  }

  g_atp_abort_libData = libData;
  thvm_atp_abort_hook = atp_abort_cb;
  AtpStatus st = thvm_atp_run(atp);
  thvm_atp_abort_hook = NULL;
  g_atp_abort_libData = NULL;
  if (st == ATP_ABORTED) { thvm_atp_free(atp); return LIBRARY_FUNCTION_ERROR; }

  // Pack stats into a 4-element Int64 NumericArray.
  mint dims[1] = {4};
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_Bit64, 1, dims, &out);
  int64_t *odata = (int64_t *)naf->MNumericArray_getData(out);
  odata[0] = (int64_t)st;
  odata[1] = (int64_t)atp->n_rules;
  odata[2] = (int64_t)atp->n_trace;
  odata[3] = (int64_t)atp->n_cps;

  thvm_atp_free(atp);

  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

// === ENIGMA Tier 1: push a trained CP-selection model ============
//
// Loads a runtime model used by ATP_CP_WEIGHT_LEARNED (CriticalPairWeight
// -> "Learned").  args[0] is a flat Real (f64) parameter vector in the
// thvm_atp_set_learned_scorer layout (kind, hidden, mean[14], inv_std[14],
// then LINEAR or MLP weights).  An EMPTY array clears the model and
// reverts to the baked-in logistic regression.  Returns 1 on success,
// 0 on a malformed blob (engine keeps the baked-in scorer).  Process-
// global: set once, used by every subsequent proof run.
EXTERN_C DLLEXPORT int thvm_wl_atp_set_learned_scorer(WolframLibraryData libData,
                                                      mint argc, MArgument *args,
                                                      MArgument res) {
  (void)argc;
  MTensor t = MArgument_getMTensor(args[0]);
  mint len  = libData->MTensor_getFlattenedLength(t);
  if (len == 0) {
    thvm_atp_clear_learned_scorer();
    MArgument_setInteger(res, 1);
    return LIBRARY_NO_ERROR;
  }
  const double *blob = libData->MTensor_getRealData(t);
  int ok = thvm_atp_set_learned_scorer(blob, (u32)len);
  MArgument_setInteger(res, ok);
  return LIBRARY_NO_ERROR;
}

// === ENIGMA Tier 2: push a trained GCN model ====================
//
// Loads the GNN used by the in-engine C re-rank (thvm_atp_gnn_rerank).
// args[0] is a flat Real (f64) parameter vector in the
// thvm_atp_set_gnn_scorer layout ([R, H], then per-round W1/Ws/Bh, then
// Wout/Bout).  An EMPTY array clears the model.  Returns 1 on success, 0
// on a malformed blob.  Process-global, set once and reused.  The GCN
// forward runs entirely on thvm's own tensor runtime in C, no WL in
// the per-step loop.
EXTERN_C DLLEXPORT int thvm_wl_atp_set_gnn_scorer(WolframLibraryData libData,
                                                  mint argc, MArgument *args,
                                                  MArgument res) {
  (void)argc;
  MTensor t = MArgument_getMTensor(args[0]);
  mint len  = libData->MTensor_getFlattenedLength(t);
  if (len == 0) {
    thvm_atp_clear_gnn_scorer();
    MArgument_setInteger(res, 1);
    return LIBRARY_NO_ERROR;
  }
  const double *blob = libData->MTensor_getRealData(t);
  int ok = thvm_atp_set_gnn_scorer(blob, (u32)len);
  MArgument_setInteger(res, ok);
  return LIBRARY_NO_ERROR;
}

// === ATP runner with proof extraction ============================
//
// Mirrors thvm_wl_atp_run, but on a goal closed by the single-NF
// check it also extracts the equational rewrite chain and ships
// TWO derivations to WL:
//
//   (1) The completion-saturated MAIN state's full DAG -- its rule
//       set R, the completion trace[] (which critical pairs birthed
//       which rules), the rule -> trace map, and the proof chain
//       extracted against R.  A hard goal (DoubleNegation) closes
//       only here, over completion-derived rules.
//
//   (2) A SEPARATE `ext` state whose rule set is the input axioms
//       oriented once -- no completion.  Its proof chain, when it
//       exists, is over the axioms themselves, so the WL side can
//       cite the axioms directly.  Axiom-confluent goals (transitiv-
//       ity, substitution) close here with a verifier-friendly
//       axiom-cited dataset.
//
// The WL consumer prefers the `ext` chain (simple axiom-cited
// dataset) and falls back to the main DAG only when `ext` cannot
// close the goal.
//
// Inputs: identical to thvm_wl_atp_run plus a wall-seconds arg
//   args[0] = packed_terms NumericArray (Int64).
//   args[1] = max_steps  (mint).
//   args[2] = max_label  (mint).
//   args[3] = wall_seconds (Real, 0.0 = unbounded).  The saturator
//             bails with ATP_TIMEOUT once the deadline passes -- the
//             only defense against recursively-defined axioms like
//             CombinatorAxioms' `Y x == x (Y x)` whose CP fan-out
//             is unbounded.
//   args[4] = cp_weight (Integer): AtpCpWeightMode, or -1 to keep
//             thvm_atp_init's default (ATP_CP_WEIGHT_GT).
//   args[5] = ordering  (Integer): 0 = KBO, 1 = LPO.
//   args[6] = auto_prec (Integer): 1 = Waldmeister auto-precedence
//             from axiom analysis, 0 = identity precedence.
//   args[7] = use_mnf (Integer): 1 = enable the MNF goal-directed
//             front search (Method -> "GoalDirected"), 0 = single-NF
//             completion only.  No effect unless the dylib was built
//             with -DATP_MNF (the paclet always is).
//
// Output: one self-describing Int64 NumericArray.
//   header (8 ints):
//     [0] status  [1] n_rules  [2] n_trace  [3] n_cps  [4] n_steps
//     [5] ext_n_rules  [6] ext_n_steps  [7] mnf_n_steps
//   then the MAIN-state blocks, sizes derived from the header:
//     rules    -- 2*n_rules ints: lhs_i, rhs_i (packed Terms)
//     r_trace  -- n_rules ints:   trace index that birthed rule i
//     trace    -- variable: per entry  reason, parent_a, parent_b,
//                 lhs, rhs, pos_len, pos[0..pos_len).  Non-CP
//                 entries have pos_len == 0; a TRACE_CP entry's
//                 pos is the superposition path (see atp_trace_push
//                 / atp_trace_push_cp).
//     steps    -- variable: per step  side, rule, fwd, pos_len,
//                 pos[0..pos_len), before, after (packed Terms).
//   then the EXT-state blocks:
//     ext_rules -- 2*ext_n_rules ints
//     ext_steps -- variable: per step  side, rule, fwd, pos_len,
//                  pos[0..pos_len), before, after.
//     mnf_steps -- variable: same per-step layout; the GREEN/RED
//                  front chains for a goal closed by the MNF search.
//   WL walks the variable-width blocks with a cursor (pos_len
//   drives the stride).
EXTERN_C DLLEXPORT int thvm_wl_atp_run_proof(WolframLibraryData libData,
                                             mint argc, MArgument *args,
                                             MArgument res) {
  (void)argc;
  // Heap-exhaust recovery: when thvm's bump allocator runs out of cells
  // (a diverging saturation on a hard NotableTheorem), instead of
  // exit(1) -- which orphans WolframKernel and ghost-kernels accumulate
  // until the OS panics -- longjmp back here and return
  // LIBRARY_FUNCTION_ERROR.  WL handles that as $Failed and the kernel
  // keeps running cleanly.  See feedback_wolframscript_oom_risk.md.
  //
  // Static-storage jmp_buf so a setjmp/longjmp pair survives early
  // returns from this entry: thvm_heap_exhaust_jmp can't safely point
  // at a stack-local jmp_buf because the many `return
  // LIBRARY_FUNCTION_ERROR` paths below leave it dangling between
  // calls.  A static buffer lives across calls; we just overwrite it
  // on each entry's setjmp.  Not thread-safe, which is fine -- thvm
  // is single-threaded inside a LibraryLink entry.
  static jmp_buf jb;
  thvm_heap_exhaust_jmp = &jb;
  thvm_heap_exhausted   = 0;
  if (setjmp(jb) != 0) {
    thvm_heap_exhaust_jmp = NULL;
    fprintf(stderr, "thvm_wl_atp_run_proof: heap exhausted -- returning "
                    "LIBRARY_FUNCTION_ERROR (kernel preserved)\n");
    return LIBRARY_FUNCTION_ERROR;
  }
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  mint max_steps   = MArgument_getInteger(args[1]);
  mint max_label   = MArgument_getInteger(args[2]);
  double wall_seconds = MArgument_getReal(args[3]);
  // Method knobs (see TFindEquationalProof's Method option):
  //   args[4] cp_weight : -1 = leave thvm_atp_init's default
  //                       (ATP_CP_WEIGHT_GT), else an AtpCpWeightMode.
  //   args[5] ordering  : 0 = KBO (default), 1 = LPO.
  //   args[6] auto_prec : 0 = identity precedence, 1 = Waldmeister-
  //                       style auto-precedence from axiom analysis.
  //   args[17] precedence : explicit per-label precedence (Int64, 1).
  //                       Length 0 = inactive (use identity / auto_prec).
  //                       When non-empty, prec[label] gives the LPO/KBO
  //                       precedence rank of that label (higher = greater),
  //                       overriding both identity and auto_prec.  This is
  //                       the Method "Precedence"/"SkolemHighest" suboption,
  //                       mirroring Waldmeister's `p > q > nand` ORDERING.
  mint cp_weight = MArgument_getInteger(args[4]);
  mint ordering  = MArgument_getInteger(args[5]);
  mint auto_prec = MArgument_getInteger(args[6]);
  mint use_mnf   = MArgument_getInteger(args[7]);
  mint max_cp_w  = MArgument_getInteger(args[8]);   // Waldmeister MaxWeight (0 = unbounded)
  mint goal_intl = MArgument_getInteger(args[9]);   // goal-interleave ratio (0 = off)
  mint gnd_join  = MArgument_getInteger(args[10]);  // ground-joinability deletion (0 = off)
  mint sel_ratio = MArgument_getInteger(args[11]);  // CPdimension FIFO ratio (0 = default 11)

  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;
  if (naf->MNumericArray_getType(na) != MNumericArray_Type_Bit64) {
    return LIBRARY_FUNCTION_ERROR;
  }
  mint flat_len = naf->MNumericArray_getFlattenedLength(na);
  if (flat_len < 3) return LIBRARY_FUNCTION_ERROR;
  const int64_t *data = (const int64_t *)naf->MNumericArray_getData(na);

  int64_t n_ax_i = data[0];
  if (n_ax_i < 0 || (int64_t)flat_len != 3 * n_ax_i + 3) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 n_ax = (u32)n_ax_i;

  if ((u32)max_label >= ATP_WL_CFG_MAX_LABELS) {
    return LIBRARY_FUNCTION_ERROR;
  }
  static u32 wl_weights_p[ATP_WL_CFG_MAX_LABELS];
  static u32 wl_precedence_p[ATP_WL_CFG_MAX_LABELS];
  // Identity precedence: byte-identical to the pre-FVI default
  // (`wl_precedence_p[i] = i + 1`).  Reserved labels 1 and 2 happen
  // to land at ranks 2 and 3 here -- below all user labels (3..N) at
  // ranks 4..N+1.  The FVI hook in `thvm_atp_orient_and_add` only
  // needs the grounded LHS-vs-min_const compare to return KBO_GT,
  // which is satisfied as long as `min_const` sits BELOW the LHS
  // symbols (it does here, since user labels start at 3).
  for (u32 i = 0; i < (u32)max_label + 1; i++) {
    wl_weights_p[i]    = 1;
    wl_precedence_p[i] = i + 1;
  }
  // Waldmeister-style auto-precedence from axiom-set analysis, gated
  // at runtime by the Method "AutoPrecedence" suboption (was a
  // compile-time -DATP_AUTO_PREC switch).
  // auto_prec dispatch:
  //   0 = off (identity precedence, the default initialized above)
  //   1 = Waldmeister structural (atp_auto_precedence)
  //   2 = E/Vampire `sp=frequency` (atp_occurrence_precedence) --
  //       rare > common.  Historical name "Occurrence" kept.
  //   3 = Vampire `sp=reverse_frequency`
  //       (atp_reverse_frequency_precedence) -- common > rare.  Most
  //       common winning shape in tools/baselines/vampire_raw/ (122 wins).
  if (auto_prec != 0) {
    static Term ax_lhs[ATP_WL_CFG_MAX_LABELS];
    static Term ax_rhs[ATP_WL_CFG_MAX_LABELS];
    u32 n_ax_use = n_ax < ATP_WL_CFG_MAX_LABELS ? n_ax
                                                : ATP_WL_CFG_MAX_LABELS;
    for (u32 i = 0; i < n_ax_use; i++) {
      ax_lhs[i] = (Term)data[1 + 2 * i + 0];
      ax_rhs[i] = (Term)data[1 + 2 * i + 1];
    }
    if (auto_prec == 3) {
      atp_reverse_frequency_precedence(ax_lhs, ax_rhs, n_ax_use,
                                       (u32)max_label + 1, wl_precedence_p);
    } else if (auto_prec == 2) {
      atp_occurrence_precedence(ax_lhs, ax_rhs, n_ax_use,
                                (u32)max_label + 1, wl_precedence_p);
    } else {
      atp_auto_precedence(ax_lhs, ax_rhs, n_ax_use,
                          (u32)max_label + 1, wl_precedence_p);
    }
  }
  // Method "Precedence"/"SkolemHighest": an explicit per-label precedence
  // array overrides identity / auto_prec.  args[17] is a label-indexed
  // Int64 NumericArray; a 0-length array leaves the chosen default in
  // place, so the engine is byte-identical unless precedence is supplied.
  {
    MTensor prec_t = MArgument_getMTensor(args[17]);
    if (prec_t != NULL) {
      mint prec_len = libData->MTensor_getFlattenedLength(prec_t);
      if (prec_len > 0) {
        const mint *prec_data = libData->MTensor_getIntegerData(prec_t);
        u32 n_copy = (u32)prec_len <= (u32)max_label + 1 ? (u32)prec_len
                                                         : (u32)max_label + 1;
        for (u32 i = 0; i < n_copy; i++) {
          wl_precedence_p[i] = (u32)prec_data[i];
        }
      }
    }
  }
  // Method "SymbolWeights": an explicit per-label KBO weight array
  // overrides the uniform-1 default initialized above.  Waldmeister
  // SymbolGewichte port (CLAS/SymbolGewichte.c::SG_SymbGewichteEintragen).
  // args[25] is a label-indexed Int64 NumericArray; a 0-length array
  // leaves all entries at 1 (engine byte-identical default).
  {
    MTensor sw_t = MArgument_getMTensor(args[25]);
    if (sw_t != NULL) {
      mint sw_len = libData->MTensor_getFlattenedLength(sw_t);
      if (sw_len > 0) {
        const mint *sw_data = libData->MTensor_getIntegerData(sw_t);
        u32 n_copy = (u32)sw_len <= (u32)max_label + 1 ? (u32)sw_len
                                                       : (u32)max_label + 1;
        for (u32 i = 0; i < n_copy; i++) {
          // Sentinel 0 in the input means "leave at default 1" so the
          // WL side can pass a full label-length array with unset
          // entries.  Genuine weight 0 is supported by passing -1 (which
          // wraps to a high u32 and matches Waldmeister's convention of
          // "default+overrides" rather than a single uniform table).
          if (sw_data[i] > 0) {
            wl_weights_p[i] = (u32)sw_data[i];
          }
        }
      }
    }
  }
  // Method "KboWeightScheme" -> "InvPrecedence": derive per-symbol KBO
  // weights from the just-computed precedence (rank N..1).  Common
  // symbols (high precedence) get LOW weight; rare (low precedence)
  // get HIGH weight.  Mirrors Vampire `kws=inv_precedence`.  Skipped
  // when explicit "SymbolWeights" is supplied (the explicit array
  // wins per Vampire convention) or when no precedence is in play
  // (kws is a tweak on TOP of an ordering; without a precedence
  // gradient it would be uniform-1 anyway).  args[29].
  mint kws_mode = MArgument_getInteger(args[29]);
  if (kws_mode == 1) {
    MTensor sw_t_check = MArgument_getMTensor(args[25]);
    int sw_provided = 0;
    if (sw_t_check != NULL) {
      mint sw_len_c = libData->MTensor_getFlattenedLength(sw_t_check);
      sw_provided = (sw_len_c > 0);
    }
    if (!sw_provided) {
      // Find the max precedence rank actually assigned.  Any unseen
      // label has prec=0 (which stays weight 1 -- the uniform default).
      u32 max_p = 0u;
      for (u32 i = 0; i < (u32)max_label + 1; i++) {
        if (wl_precedence_p[i] > max_p) max_p = wl_precedence_p[i];
      }
      // weight[lab] = max_p - prec[lab] + 1 for seen labels; 1 for unseen.
      for (u32 i = 0; i < (u32)max_label + 1; i++) {
        if (wl_precedence_p[i] > 0u) {
          wl_weights_p[i] = max_p - wl_precedence_p[i] + 1u;
        }
      }
    }
  }
  // Method "LazyNormalize" -> True: DISCOUNT-style deferred CP
  // normalization (queue UN-normalized CPs, defer normalize to
  // selection time).  Engine lever in src/atp/_.c:7012; gated at the
  // saturation loop's lazy push site near line 11238.  args[30].
  mint lazy_norm_mode = MArgument_getInteger(args[30]);
  // Method "VarWeight" -> n: per-variable KBO weight override (default
  // 1).  Mirrors Waldmeister `-w VAR=N`.  args[26].  Pass <= 0 (or
  // omit) to keep the default 1.
  mint var_weight_in = MArgument_getInteger(args[26]);
  static KboConfig wl_kbo_p;
  wl_kbo_p.weights    = wl_weights_p;
  wl_kbo_p.precedence = wl_precedence_p;
  wl_kbo_p.n_labels   = (u32)max_label + 1;
  wl_kbo_p.var_weight = var_weight_in > 0 ? (u32)var_weight_in : 1u;

  AtpState *atp = thvm_atp_init(&wl_kbo_p, (u32)max_steps);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;
  // LPO ordering: drives orientation purely by symbol precedence
  // (same precedence array as KBO).  Attached after init so it
  // overrides the KBO config in thvm_atp_orient_and_add / compare.
  static LpoConfig wl_lpo_p;
  if (ordering == 1) {
    wl_lpo_p.precedence = wl_precedence_p;
    wl_lpo_p.n_labels   = (u32)max_label + 1;
    thvm_atp_set_lpo(atp, &wl_lpo_p);
  }
  // CP-selection weight heuristic (Waldmeister's ClasHeuristics).
  if (cp_weight >= 0) {
    thvm_atp_set_cp_weight_mode(atp, (u32)cp_weight);
  }
  // ENIGMA "coop" interleave (Method "CoopWeight" / "CoopRatio"): every
  // coop_modulo-th selection picks by the secondary weight coop_mode (an
  // AtpCpWeightMode int, e.g. 3 = GT) instead of the primary.  Pairing the
  // learned primary scorer (cp_weight = 9 LEARNED) with a hand-tuned GT
  // secondary mirrors real ENIGMA, which selects cooperatively with the
  // base heuristic rather than by the model alone.  args[31]/[32] are the
  // suboptions; when off (modulo <= 0) fall back to the THVM_ATP_W2_*
  // env vars.  Both off -> the positional-arg path is byte-identical.
  {
    mint coop_mode   = MArgument_getInteger(args[31]);
    mint coop_modulo = MArgument_getInteger(args[32]);
    if (coop_modulo <= 0) {
      const char *w2m = getenv("THVM_ATP_W2_MODE");
      const char *w2k = getenv("THVM_ATP_W2_MODULO");
      if (w2m != NULL && w2k != NULL) {
        coop_mode   = atoi(w2m);
        coop_modulo = atoi(w2k);
      }
    }
    if (coop_mode >= 0 && coop_modulo > 0) {
      thvm_atp_set_w2(atp, (u32)coop_modulo, (u8)coop_mode);
    }
  }
  // Method -> {... "FreeVarInstance" -> True}: Waldmeister
  // RechtsUnfreiErzeugen (FVI) -- when an unorientable equation is
  // added to R, also push a grounded sibling rule substituting
  // ATP_RESERVED_LABEL_MIN_CONST for every free RHS variable absent
  // from the LHS.  Unblocks ExcludedMiddle / Noncontradiction /
  // EqualityOfInverses under Method->"Waldmeister".  args[33];
  // 0 = off (default, engine byte-identical).
  mint use_fvi = MArgument_getInteger(args[33]);
  thvm_atp_set_use_fvi(atp, (u8)(use_fvi != 0));
  // Record per-step normalization chains so the WL ProofObject
  // builder walks (CP -> NORM_STEP* -> ORIENT) linearly instead of
  // reconstructing it by search.  args[19] gates it: the default (any
  // value but 0, including the historical implicit 1) keeps recording
  // on; 0 routes the search through the fast indexed/flatterm normalize
  // (no per-step TRACE_NORM_STEP push, no skipped heap reset) so a long
  // completion -- the deep Sheffer/Wolfram theorems -- saturates at the
  // C-bench rate.  With recording off the WL builder reconstructs the
  // chain through the emitNorm BFS ($AtpUseChain -> False) using the
  // CP/ORIENT/SIMPLIFY trace DAG, which is recorded regardless.
  mint record_norm = MArgument_getInteger(args[19]);
  thvm_atp_set_record_norm_steps(atp, (u8)(record_norm != 0));
  // ENIGMA dataset capture: when THVM_ATP_CP_DATASET names a file, record
  // per-selected-CP features so a PROVED run can label + append them.
  const char *g_cp_dataset = getenv("THVM_ATP_CP_DATASET");
  if (g_cp_dataset != NULL) thvm_atp_set_record_cp_features(atp, 1u);
  // Optional wall deadline: 0 leaves the saturator unbounded.
  if (wall_seconds > 0.0) {
    thvm_atp_set_wall_deadline(atp, wall_seconds);
  }
  // Method -> "GoalDirected": augment the single-NF goal check with the
  // MNF bidirectional front search.  No-op unless built with -DATP_MNF.
  thvm_atp_set_use_mnf(atp, (u8)(use_mnf != 0));
  thvm_atp_set_max_cp_weight(atp, max_cp_w > 0 ? (u32)max_cp_w : 0u);
  thvm_atp_set_goal_interleave(atp, goal_intl > 0 ? (u32)goal_intl : 0u);
  // Method -> {... "GroundJoin" -> True}: drop ground-joinable critical
  // pairs (sound redundancy criterion).  No-op unless built with
  // -DATP_CP_GROUND_JOIN (the shipped paclet is).
  thvm_atp_set_use_ground_join(atp, (u8)(gnd_join != 0));
  // Method -> {... "SelectionRatio" -> n}: Waldmeister CPdimension
  // fairness ratio (1 FIFO pick per n selections).  0 keeps the default.
  thvm_atp_set_selection_ratio(atp, sel_ratio > 0 ? (u32)sel_ratio : 0u);
  // Method -> {... "AutoMaxWeight" -> b}: growing CP-weight bound
  // (base b + 2*deepest-rule-weight) that defers over-weight CPs to a
  // stash and force-drains them when the queue empties -- keeps the CP
  // queue small without losing completeness.  0 = off.
  mint auto_maxw = MArgument_getInteger(args[12]);
  thvm_atp_set_auto_max_cp_weight(atp, auto_maxw > 0 ? (u32)auto_maxw : 0u);
  // Method -> "Waldmeister" / {... "RHSInterreduce" -> True}: Waldmeister-
  // faithful right-hand-side interreduction (IR_InterreduktionRechts).
  // Keeps the rule set fully reduced so the CP queue stays small -- the
  // lever the deep Sheffer/Wolfram theorems need.  0 = off (default).
  mint rhs_ir = MArgument_getInteger(args[13]);
  thvm_atp_set_use_rhs_interreduce(atp, (u8)(rhs_ir != 0));
  // Method -> "Waldmeister" / {... "UnfailingCP" -> True}: both-faces
  // superposition of unorientable equations (unfailing completion's
  // completeness requirement).  THE lever that makes the deep Sheffer /
  // Wolfram theorems reachable.  0 = off (default lhs-only overlap).
  mint unf_cp = MArgument_getInteger(args[14]);
  thvm_atp_set_use_unfailing_cp(atp, (u8)(unf_cp != 0));
  // Method -> {... "LazyNormalize" -> True}: DISCOUNT-style deferred
  // CP normalization.  Engine lever already in
  // src/atp/_.c:7012 (thvm_atp_set_use_lazy_normalize); the saturation
  // loop's lazy push site (~line 11238) gates on this + !use_mnf.
  // 0 = off (default eager push-normalize, engine byte-identical).
  thvm_atp_set_use_lazy_normalize(atp, (u8)(lazy_norm_mode != 0));
  // Method -> "Waldmeister": periodic full-rule-set CP-queue interreduction
  // (Waldmeister KPV_KPMengeInterreduzieren) -- purges queued CPs that have
  // become joinable through the growing rule set so the heap-min selection
  // tracks live, irreducible CPs.  0 = off (default), engine byte-identical.
  mint cp_set_ir = MArgument_getInteger(args[15]);
  thvm_atp_set_cp_set_interreduce(atp, (u8)(cp_set_ir != 0));
  // Bachmair-Dershowitz connectedness CP deletion (Twee section 6.2):
  // drop a CP whose two sides join through terms strictly below the
  // peak.  0 = off (default), engine byte-identical.
  mint conn = MArgument_getInteger(args[16]);
  thvm_atp_set_use_connectedness(atp, (u8)(conn != 0));
  // Method -> {... "FifoTiebreak" -> True}: Waldmeister `-:w1=fifo`
  // secondary key.  Preserve each surviving CP's insertion age across the
  // post-orient normalize sweep so equal-weight ties resolve oldest-first
  // run-wide.  args[18] (after the args[17] precedence MTensor).  0 = off,
  // engine byte-identical.
  mint fifo_tb = MArgument_getInteger(args[18]);
  thvm_atp_set_cp_fifo_tiebreak(atp, (u8)(fifo_tb != 0));
  // Method -> {... "LRS" -> True}: Vampire Limited Resource Strategy
  // (Riazanov & Voronkov, JSC 36, 2003).  When a wall deadline is set,
  // periodically prune CPs above the predicted-reachable weight horizon
  // so the saturator concentrates effort on the budget-tractable subset.
  // args[20].  0 = off, engine byte-identical.
  mint use_lrs = MArgument_getInteger(args[20]);
  thvm_atp_set_use_lrs(atp, (u8)(use_lrs != 0));
  // Method -> {... "SetOfSupport" -> True}: bias the CP-queue heap
  // toward CPs whose terms share symbols with the goal.  args[21].
  // 0 = off.  Set BEFORE goal so the symbol mask snapshot is taken
  // after thvm_atp_set_goal below.  (Actual mask init happens inside
  // thvm_atp_set_use_sos which reads s->goal_lhs/rhs, so we re-call
  // it after the goal is set.)
  mint use_sos = MArgument_getInteger(args[21]);
  // Method -> {... "ForwardSubsume" -> True}: drop the rule add when
  // an already-stored rule subsumes it (Vampire --forward_subsumption
  // analog, unit-only).  Sound + completeness-preserving.  args[22].
  // 0 = off (engine byte-identical).
  mint use_fwd_sub = MArgument_getInteger(args[22]);
  thvm_atp_set_use_fwd_subsume(atp, (u8)(use_fwd_sub != 0));
  // Method -> {... "BackwardSubsume" -> True}: soft-delete existing
  // rules subsumed by the newly-added one.  args[23].  0 = off
  // (engine byte-identical).  Vampire bs=unit_only analog.
  mint use_bwd_sub = MArgument_getInteger(args[23]);
  thvm_atp_set_use_bwd_subsume(atp, (u8)(use_bwd_sub != 0));
  // Method -> {... "BackwardDemod" -> True}: Vampire bd=all analog
  // (LHS half).  args[24].  0 = off (engine byte-identical).
  mint use_bwd_demod = MArgument_getInteger(args[24]);
  thvm_atp_set_use_bwd_demod(atp, (u8)(use_bwd_demod != 0));
  // INCR_IR + CP_INDEX (0a98478e, d86fa599 in the C bench): pure perf
  // wins, byte-identical PROOF output by construction (incremental
  // discrimination-tree rebuilds + skip-CP-on-no-new-rule-top-sym).
  // Always-on in the WL bridge: every Method that fires up the C engine
  // benefits, no option plumbing churn.  Both can be force-disabled
  // via env THVM_ATP_INCR_IR=0 / THVM_ATP_CP_INDEX=0.
  {
    const char *iir = getenv("THVM_ATP_INCR_IR");
    if (iir == NULL || !(iir[0] == '0' && iir[1] == '\0')) {
      thvm_atp_set_use_incr_ir(atp, 1u);
    }
    const char *cpi = getenv("THVM_ATP_CP_INDEX");
    if (cpi == NULL || !(cpi[0] == '0' && cpi[1] == '\0')) {
      thvm_atp_set_use_cp_index(atp, 1u);
    }
  }

  for (u32 i = 0; i < n_ax; i++) {
    Term lhs  = (Term)data[1 + 2 * i + 0];
    Term rhs  = (Term)data[1 + 2 * i + 1];
    int64_t f = data[1 + 2 * n_ax + 2 + i];
    if (f == 1) {
      (void)thvm_atp_install_oriented_rule(atp, lhs, rhs);
    } else if (!thvm_atp_add_equation(atp, lhs, rhs)) {
      thvm_atp_free(atp);
      return LIBRARY_FUNCTION_ERROR;
    }
  }
#ifdef THVM_ATP_AC
  if (getenv("THVM_ATP_AUTO_AC_DEBUG") != NULL) {
    for (u32 i = 0; i < n_ax; i++) {
      Term lhs = (Term)data[1 + 2 * i + 0];
      Term rhs = (Term)data[1 + 2 * i + 1];
      fprintf(stderr, "[bridge] ax%u: lhs.tag=%u ext=%u  rhs.tag=%u ext=%u\n",
              i, term_tag(lhs), term_ext(lhs),
              term_tag(rhs), term_ext(rhs));
    }
    Term gl = (Term)data[1 + 2 * n_ax + 0];
    Term gr = (Term)data[1 + 2 * n_ax + 1];
    fprintf(stderr, "[bridge] goal: lhs.tag=%u ext=%u  rhs.tag=%u ext=%u\n",
            term_tag(gl), term_ext(gl),
            term_tag(gr), term_ext(gr));
  }
#endif
#ifdef THVM_ATP_AC
  // THVM_ATP_AUTO_AC=1 opts the saturation into Bachmair-Plaisted AC
  // reasoning when the axiom analyzer surfaces an AC top symbol (both
  // commutative AND associative).  Default off (env unset or != "1")
  // resets ac_mask to 0 -- the mask is engine-global and could
  // otherwise leak from a previous run.  All AC gates
  // (atp_ordered_try_top extension, atp_overlap_ij CP extension,
  // goal-check AC-eq short-circuit) read ac_mask and stay dormant at
  // 0, so the default build remains byte-identical to a non-AC paclet.
  // Env-var rather than an argument-slot lets a user opt in via
  // Environment["THVM_ATP_AUTO_AC", "1"] without a LibraryFunctionLoad
  // signature change.
  {
    const char *ac_ev = getenv("THVM_ATP_AUTO_AC");
    if (ac_ev != NULL && ac_ev[0] == '1' && n_ax > 0u) {
      Term ax_lhs[ATP_PROOF_MAX_STEPS];
      Term ax_rhs[ATP_PROOF_MAX_STEPS];
      u32 cap = (u32)(sizeof ax_lhs / sizeof ax_lhs[0]);
      u32 m   = n_ax < cap ? n_ax : cap;
      for (u32 i = 0; i < m; i++) {
        ax_lhs[i] = (Term)data[1 + 2 * i + 0];
        ax_rhs[i] = (Term)data[1 + 2 * i + 1];
      }
      thvm_atp_auto_ac(ax_lhs, ax_rhs, m);
    } else {
      thvm_atp_set_ac_mask(0ull);
    }
  }
#endif
  Term goal_lhs = (Term)data[1 + 2 * n_ax + 0];
  Term goal_rhs = (Term)data[1 + 2 * n_ax + 1];
  // Completion mode: a (0, 0) conjecture pair means "no goal" -- the
  // engine then saturates the axioms until the queue empties (a finite
  // complete system) or the step/wall budget is hit, and the caller
  // reads the derived rule set (MainRules) as the completion lemmas.
  // goal_check returns ATP_RUNNING whenever no goal is set, so the run
  // is bounded only by MaxSteps / TimeConstraint.
  if (goal_lhs != 0u || goal_rhs != 0u) {
    thvm_atp_set_goal(atp, goal_lhs, goal_rhs);
  }
  // Apply SoS AFTER the goal is set so the symbol-mask snapshot
  // includes goal_lhs / goal_rhs labels.
  thvm_atp_set_use_sos(atp, (u8)(use_sos != 0));

  // Method -> {... "RandomRatio" -> n, "RandomSeed" -> u64}: Vampire-
  // style random CP-selection.  Default 0/0 = off, engine byte-identical.
  // args[27] = ratio, args[28] = seed.  Seed applied before ratio so the
  // first random pick is reproducible.
  mint random_ratio_in = MArgument_getInteger(args[27]);
  mint random_seed_in  = MArgument_getInteger(args[28]);
  thvm_atp_set_random_seed(atp, (u64)random_seed_in);
  thvm_atp_set_random_modulo(atp, random_ratio_in > 0 ? (u32)random_ratio_in : 0u);

  g_atp_abort_libData = libData;
  thvm_atp_abort_hook = atp_abort_cb;
  /* Iter 133 (workflow plan step 1): bypass the parent-process QoS
   * clamp by installing THREAD_TIME_CONSTRAINT_POLICY on the calling
   * thread for the duration of thvm_atp_run.  This is the mechanism
   * CoreAudio uses to pin its render thread to the performance
   * cluster on Apple Silicon; unlike pthread_set_qos_class_self_np
   * (which is clamped by the parent's QoS hint), the realtime policy
   * is a hard scheduler directive -- in principle.  In practice on
   * a 25s sustained completion the (7ms/9ms) audio-style window makes
   * the kernel THROTTLE the thread, so AndAssociativity goes from
   * 26s Proved to 60s TimedOut with the policy installed.  Kept opt-in
   * (THVM_ATP_PCORE_RT=1) for shorter workloads + future tuning of
   * (period, computation, constraint); default OFF. */
#ifdef __APPLE__
  static int g_do_pcore_rt = -1;
  if (g_do_pcore_rt < 0) {
    const char *evrt = getenv("THVM_ATP_PCORE_RT");
    g_do_pcore_rt = (evrt != NULL && evrt[0] == '1') ? 1 : 0;
  }
  static mach_timebase_info_data_t g_tbi = {0, 0};
  if (g_tbi.denom == 0) mach_timebase_info(&g_tbi);
  thread_time_constraint_policy_data_t saved_tc = {0, 0, 0, 0};
  thread_extended_policy_data_t saved_ext = {1};
  int rt_pinned = 0;
  thread_port_t self_port = mach_thread_self();
  if (g_do_pcore_rt && g_tbi.denom != 0) {
    /* CoreAudio-style 10ms period, 7ms compute, 9ms constraint, preemptible.
     * Reads as P-core-mandatory to the macOS scheduler. */
    uint64_t period_abs      = ((uint64_t)10000000ULL * g_tbi.denom) / g_tbi.numer;
    uint64_t computation_abs = ((uint64_t)7000000ULL  * g_tbi.denom) / g_tbi.numer;
    uint64_t constraint_abs  = ((uint64_t)9000000ULL  * g_tbi.denom) / g_tbi.numer;
    thread_time_constraint_policy_data_t tc;
    tc.period      = (uint32_t)period_abs;
    tc.computation = (uint32_t)computation_abs;
    tc.constraint  = (uint32_t)constraint_abs;
    tc.preemptible = TRUE;
    if (thread_policy_set(self_port, THREAD_TIME_CONSTRAINT_POLICY,
            (thread_policy_t)&tc, THREAD_TIME_CONSTRAINT_POLICY_COUNT)
        == KERN_SUCCESS) {
      rt_pinned = 1;
    }
  }
  /* Fall back to the QoS bump only when RT didn't take.  The two
   * paths conflict (mixing produces UNSPECIFIED scheduling). */
  static int g_do_qos = -1;
  if (g_do_qos < 0) {
    const char *evq = getenv("THVM_ATP_QOS_BUMP");
    g_do_qos = (evq != NULL && evq[0] == '1') ? 1 : 0;
  }
  qos_class_t saved_qos = QOS_CLASS_UNSPECIFIED;
  int saved_rel_prio = 0;
  int qos_bumped = 0;
  if (!rt_pinned && g_do_qos) {
    pthread_get_qos_class_np(pthread_self(), &saved_qos, &saved_rel_prio);
    if (pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0) == 0) {
      qos_bumped = 1;
    }
  }
#endif
  /* Iter 132 phase timing -- probe per-phase wall to localize the
   * paclet-vs-bench overhead.  Enable with THVM_ATP_PHASETIME=1. */
  static int g_phase = -1;
  if (g_phase < 0) {
    const char *evp = getenv("THVM_ATP_PHASETIME");
    g_phase = (evp != NULL && evp[0] == '1') ? 1 : 0;
  }
  struct timespec _tp0, _tp1;
  if (g_phase) clock_gettime(CLOCK_MONOTONIC, &_tp0);
  AtpStatus st = thvm_atp_run(atp);
  if (g_phase) {
    clock_gettime(CLOCK_MONOTONIC, &_tp1);
    double d = (_tp1.tv_sec - _tp0.tv_sec) + (_tp1.tv_nsec - _tp0.tv_nsec)/1e9;
    fprintf(stderr, "[atp-phase] thvm_atp_run: %.3fs n_rules=%u n_trace=%u status=%d rt_pin=%d qos_bump=%d\n",
            d, atp->n_rules, atp->n_trace, (int)st,
#ifdef __APPLE__
            rt_pinned, qos_bumped);
#else
            0, 0);
#endif
  }
#ifdef __APPLE__
  /* Restore the thread's scheduling policy before returning to the
   * caller; otherwise the WolframKernel's normal evaluation would
   * keep running under the RT directive. */
  if (rt_pinned) {
    thread_extended_policy_data_t ext = {1};  /* timeshare=TRUE */
    thread_policy_set(self_port, THREAD_EXTENDED_POLICY,
        (thread_policy_t)&ext, THREAD_EXTENDED_POLICY_COUNT);
  }
  if (qos_bumped) {
    pthread_set_qos_class_self_np(saved_qos, saved_rel_prio);
  }
  mach_port_deallocate(mach_task_self(), self_port);
#endif
  thvm_atp_abort_hook = NULL;
  g_atp_abort_libData = NULL;
  if (st == ATP_ABORTED) { thvm_atp_free(atp); return LIBRARY_FUNCTION_ERROR; }

  // (1) MAIN-state proof extraction: re-normalize both goal sides
  // under the completion-saturated R, recording every forward
  // rewrite.  A hard goal's chain cites completion-derived rules,
  // each with an r_trace[] lineage into the trace DAG.
  static AtpProofStep proof[ATP_PROOF_MAX_STEPS];
  u32 n_steps  = thvm_atp_proof_extract(atp, proof, ATP_PROOF_MAX_STEPS);
  u32 n_rules  = atp->n_rules;
  u32 n_trace  = atp->n_trace;

  // ENIGMA dataset: a PROVED run labels its selected CPs by trace-DAG
  // reachability from the goal-closing step, then appends them to the
  // dataset file (header written only if the file is new).
  if (g_cp_dataset != NULL && st == ATP_PROVED) {
    thvm_atp_cp_label(atp);
    static int g_cp_header_done = 0;
    thvm_atp_cp_dataset_append(atp, g_cp_dataset, !g_cp_header_done);
    g_cp_header_done = 1;
  }

  // (2) EXT-state proof extraction: a separate state whose R is the
  // input axioms oriented once -- no completion, no interreduction.
  // An axiom-confluent goal closes here over the axioms themselves,
  // giving a verifier-friendly axiom-cited chain.
  // (1b) MNF proof extraction: a symmetric goal closed by the MNF
  // front search has no single-NF chain (n_steps == 0) -- the GREEN
  // and RED parent chains up from the join term carry the proof
  // instead.  Extracted against the same completion-saturated R.
  static AtpProofStep mnf_proof[ATP_PROOF_MAX_STEPS];
  u32 mnf_n_steps = 0;
#ifdef ATP_MNF
  if (use_mnf && st == ATP_PROVED && n_steps == 0) {
    mnf_n_steps = thvm_atp_mnf_proof_extract(atp, mnf_proof,
                                             ATP_PROOF_MAX_STEPS);
  }
#endif

  static AtpProofStep ext_proof[ATP_PROOF_MAX_STEPS];
  u32 ext_n_steps = 0, ext_n_rules = 0;
  AtpState *ext = NULL;
  // EXT proof extraction is only meaningful for PROVED status -- the
  // axiom-cited chain reconstruction needs a closed proof to walk back
  // through.  For TimedOut / Saturated / Aborted runs this block is
  // wasted work AND a known hang trigger: after the main saturator times
  // out, the ext state's freshly-init'd KBO state shares the persistent
  // memo with the timed-out main state and atp_compare on the first
  // axiom add can spin without polling wall_deadline.  Gate on PROVED.
  // Iter 132 diagnostic: temporarily skip EXT-state extraction to
  // isolate where the ~10s paclet overhead vs the C-bench wall is
  // spent.  Set THVM_ATP_SKIP_EXT=1 to skip; default is the historical
  // path.  When skipping, downstream extractors fall back to the
  // completion-chain dataset (no axiom-cited ext lemmas).
  static int g_skip_ext = -1;
  if (g_skip_ext < 0) {
    const char *ev = getenv("THVM_ATP_SKIP_EXT");
    g_skip_ext = (ev != NULL && ev[0] == '1') ? 1 : 0;
  }
  if (!g_skip_ext && st == ATP_PROVED) {
    ext = thvm_atp_init(&wl_kbo_p, (u32)max_steps);
    if (ext != NULL) {
      ext->wall_deadline_us = atp->wall_deadline_us;
      for (u32 i = 0; i < n_ax; i++) {
        Term lhs  = (Term)data[1 + 2 * i + 0];
        Term rhs  = (Term)data[1 + 2 * i + 1];
        int64_t f = data[1 + 2 * n_ax + 2 + i];
        if (f == 1) {
          thvm_atp_install_oriented_rule(ext, lhs, rhs);
        } else {
          thvm_atp_orient_and_add(ext, lhs, rhs);
        }
      }
      thvm_atp_set_goal(ext, goal_lhs, goal_rhs);
      ext_n_steps = thvm_atp_proof_extract(ext, ext_proof, ATP_PROOF_MAX_STEPS);
      ext_n_rules = ext->n_rules;
    }
  }

  // Size the output: 8-int header + main blocks + ext blocks + the
  // MNF steps block.  A trace entry's pos_len is its TRACE_CP
  // superposition-path length (0 for AXIOM / ORIENT / SIMPLIFY).
  mint out_len = 8 + 2 * (mint)n_rules + (mint)n_rules
               + 2 * (mint)ext_n_rules;
  for (u32 i = 0; i < n_trace; i++) {
    Term e = atp->trace[i];
    u32  arity = term_ctr_n(e);
    out_len += 6 + (mint)(arity > 5u ? arity - 5u : 0u);
  }
  for (u32 i = 0; i < n_steps; i++)     out_len += 6 + (mint)proof[i].pos_len;
  for (u32 i = 0; i < ext_n_steps; i++) out_len += 6 + (mint)ext_proof[i].pos_len;
  for (u32 i = 0; i < mnf_n_steps; i++) out_len += 6 + (mint)mnf_proof[i].pos_len;

  mint dims[1] = {out_len};
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_Bit64, 1, dims, &out);
  int64_t *odata = (int64_t *)naf->MNumericArray_getData(out);
  odata[0] = (int64_t)st;
  odata[1] = (int64_t)n_rules;
  odata[2] = (int64_t)n_trace;
  odata[3] = (int64_t)atp->n_cps;
  odata[4] = (int64_t)n_steps;
  odata[5] = (int64_t)ext_n_rules;
  odata[6] = (int64_t)ext_n_steps;
  odata[7] = (int64_t)mnf_n_steps;
  mint w = 8;

  // MAIN rules block: lhs_i, rhs_i (the completed rule set R).
  for (u32 i = 0; i < n_rules; i++) {
    odata[w++] = (int64_t)atp->lhs[i];
    odata[w++] = (int64_t)atp->rhs[i];
  }
  // MAIN r_trace block: the trace index that birthed each rule.
  for (u32 i = 0; i < n_rules; i++) odata[w++] = (int64_t)atp->r_trace[i];

  // MAIN trace block: each entry is a TAG_CTR(reason)[NUM(pa),
  // NUM(pb), lhs, rhs, (NUM(pos_len), NUM(pos_0), ...)] -- see
  // atp_trace_push / atp_trace_push_cp.  TRACE_CP entries carry the
  // superposition path in children 4+; AXIOM / ORIENT / SIMPLIFY
  // entries have arity 4 and we emit pos_len 0 for them.
  // TRACE_NORM_STEP entries (atp_trace_push_norm_step) layer two
  // extra NUMs after pos[] -- side (0 / 1) and fwd (0 / 1) -- so the
  // WL extractor can re-emit the SubstitutionLemma's Side and
  // Orientation without re-deriving them.
  for (u32 i = 0; i < n_trace; i++) {
    Term e      = atp->trace[i];
    u32  reason = term_ext(e);
    u32  arity  = term_ctr_n(e);
    u32  pos_len = (arity > 5u) ? (u32)term_val(term_ctr_at(e, 4)) : 0u;
    odata[w++] = (int64_t)reason;                            // reason
    odata[w++] = (int64_t)term_val(term_ctr_at(e, 0));       // parent_a
    odata[w++] = (int64_t)term_val(term_ctr_at(e, 1));       // parent_b / rule_idx
    odata[w++] = (int64_t)term_ctr_at(e, 2);                 // lhs Term
    odata[w++] = (int64_t)term_ctr_at(e, 3);                 // rhs Term
    odata[w++] = (int64_t)pos_len;                           // pos_len
    for (u32 k = 0; k < pos_len; k++) {
      odata[w++] = (int64_t)term_val(term_ctr_at(e, 5u + k));
    }
    if (reason == TRACE_NORM_STEP) {
      odata[w++] = (int64_t)term_val(term_ctr_at(e, 5u + pos_len));      // side
      odata[w++] = (int64_t)term_val(term_ctr_at(e, 5u + pos_len + 1u)); // fwd
    }
  }
  // MAIN steps block.
  for (u32 i = 0; i < n_steps; i++) {
    const AtpProofStep *p = &proof[i];
    odata[w++] = (int64_t)p->side;
    odata[w++] = (int64_t)p->rule;
    odata[w++] = (int64_t)p->fwd;
    odata[w++] = (int64_t)p->pos_len;
    for (u8 k = 0; k < p->pos_len; k++) odata[w++] = (int64_t)p->pos[k];
    odata[w++] = (int64_t)p->before;
    odata[w++] = (int64_t)p->after;
  }
  // EXT rules block.
  for (u32 i = 0; i < ext_n_rules; i++) {
    odata[w++] = (int64_t)ext->lhs[i];
    odata[w++] = (int64_t)ext->rhs[i];
  }
  // EXT steps block.
  for (u32 i = 0; i < ext_n_steps; i++) {
    const AtpProofStep *p = &ext_proof[i];
    odata[w++] = (int64_t)p->side;
    odata[w++] = (int64_t)p->rule;
    odata[w++] = (int64_t)p->fwd;
    odata[w++] = (int64_t)p->pos_len;
    for (u8 k = 0; k < p->pos_len; k++) odata[w++] = (int64_t)p->pos[k];
    odata[w++] = (int64_t)p->before;
    odata[w++] = (int64_t)p->after;
  }
  // MNF steps block: same per-step layout as MAIN / EXT.  Side 0 is
  // the goal_lhs (GREEN) front chain, side 1 the goal_rhs (RED) chain;
  // both run from the goal side to the join term.
  for (u32 i = 0; i < mnf_n_steps; i++) {
    const AtpProofStep *p = &mnf_proof[i];
    odata[w++] = (int64_t)p->side;
    odata[w++] = (int64_t)p->rule;
    odata[w++] = (int64_t)p->fwd;
    odata[w++] = (int64_t)p->pos_len;
    for (u8 k = 0; k < p->pos_len; k++) odata[w++] = (int64_t)p->pos[k];
    odata[w++] = (int64_t)p->before;
    odata[w++] = (int64_t)p->after;
  }

  if (ext != NULL) thvm_atp_free(ext);
  thvm_atp_free(atp);
  MArgument_setMNumericArray(res, out);
  thvm_heap_exhaust_jmp = NULL;
  return LIBRARY_NO_ERROR;
}

// === multi-witness ATP runner ================================
//
// Saturates first (no goal set, so no early exit on goal_check),
// then enumerates witnesses by `thvm_atp_narrow_all` on the original
// (goal_lhs, goal_rhs).  Returns one binding row per witness for
// every requested witness_id.
//
// Inputs:
//   args[0] = packed_terms NumericArray (Int64), same layout as
//             thvm_wl_atp_run_existential.
//   args[1] = max_steps     (mint).
//   args[2] = max_label     (mint).
//   args[3] = witness_ids   MTensor (Integer rank-1).
//   args[4] = max_depth     (mint).
//   args[5] = max_witnesses (mint).
//
// Output: Int64 NumericArray of length 5 + max_witnesses * n_witness:
//   [status, n_rules, n_trace, n_cps, n_witnesses_found,
//    w_0_id_0, w_0_id_1, ..., w_(max_witnesses-1)_id_(n-1)].
// Unused slots in the witnesses block are zero-padded.
EXTERN_C DLLEXPORT int thvm_wl_atp_run_all_witnesses(
    WolframLibraryData libData, mint argc,
    MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na    = MArgument_getMNumericArray(args[0]);
  mint max_steps      = MArgument_getInteger(args[1]);
  mint max_label      = MArgument_getInteger(args[2]);
  MTensor witness_t   = MArgument_getMTensor(args[3]);
  mint max_depth      = MArgument_getInteger(args[4]);
  mint max_witnesses  = MArgument_getInteger(args[5]);

  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;
  if (naf->MNumericArray_getType(na) != MNumericArray_Type_Bit64) {
    return LIBRARY_FUNCTION_ERROR;
  }
  mint flat_len = naf->MNumericArray_getFlattenedLength(na);
  if (flat_len < 3) return LIBRARY_FUNCTION_ERROR;
  const int64_t *data = (const int64_t *)naf->MNumericArray_getData(na);

  int64_t n_ax_i = data[0];
  if (n_ax_i < 0 || (int64_t)flat_len != 3 * n_ax_i + 3) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 n_ax = (u32)n_ax_i;

  mint n_witness          = libData->MTensor_getFlattenedLength(witness_t);
  const mint *witness_ids = libData->MTensor_getIntegerData(witness_t);

  if ((u32)max_label >= ATP_WL_CFG_MAX_LABELS) return LIBRARY_FUNCTION_ERROR;
  if (max_witnesses <= 0 || max_witnesses > 64) return LIBRARY_FUNCTION_ERROR;

  static u32 wl_weights3[ATP_WL_CFG_MAX_LABELS];
  static u32 wl_precedence3[ATP_WL_CFG_MAX_LABELS];
  for (u32 i = 0; i < (u32)max_label + 1; i++) {
    wl_weights3[i] = 1;
    wl_precedence3[i] = i + 1;
  }
  static KboConfig wl_kbo3;
  wl_kbo3.weights    = wl_weights3;
  wl_kbo3.precedence = wl_precedence3;
  wl_kbo3.n_labels   = (u32)max_label + 1;
  wl_kbo3.var_weight = 1;

  AtpState *atp = thvm_atp_init(&wl_kbo3, (u32)max_steps);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;

  for (u32 i = 0; i < n_ax; i++) {
    Term lhs  = (Term)data[1 + 2 * i + 0];
    Term rhs  = (Term)data[1 + 2 * i + 1];
    int64_t f = data[1 + 2 * n_ax + 2 + i];
    if (f == 1) {
      (void)thvm_atp_install_oriented_rule(atp, lhs, rhs);
    } else if (!thvm_atp_add_equation(atp, lhs, rhs)) {
      thvm_atp_free(atp);
      return LIBRARY_FUNCTION_ERROR;
    }
  }
  Term goal_lhs = (Term)data[1 + 2 * n_ax + 0];
  Term goal_rhs = (Term)data[1 + 2 * n_ax + 1];

  // Saturate first with no goal set so thvm_atp_run does not
  // early-exit via goal_check; then enumerate witnesses against the
  // resulting rule set.
  AtpStatus st = thvm_atp_run(atp);

  RewriteSubst witnesses[64] = {{{0}}};
  u32 n_found = thvm_atp_narrow_all(atp, goal_lhs, goal_rhs,
                                    (u32)max_depth, (u32)max_witnesses,
                                    witnesses);

  // Pack stats + witness rows.
  mint n_witness_slots = max_witnesses * n_witness;
  mint dims[1] = {5 + n_witness_slots};
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_Bit64, 1, dims, &out);
  int64_t *odata = (int64_t *)naf->MNumericArray_getData(out);
  odata[0] = (int64_t)st;
  odata[1] = (int64_t)atp->n_rules;
  odata[2] = (int64_t)atp->n_trace;
  odata[3] = (int64_t)atp->n_cps;
  odata[4] = (int64_t)n_found;
  for (mint w = 0; w < max_witnesses; w++) {
    for (mint k = 0; k < n_witness; k++) {
      Term wt = ((u32)w < n_found)
        ? witnesses[w].bindings[(u32)witness_ids[k]]
        : (Term)0;
      odata[5 + w * n_witness + k] = (int64_t)wt;
    }
  }

  thvm_atp_free(atp);
  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

// === 9.2: file-driven ATP runner ===================================
//
// Parses a Waldmeister .pr spec via wald_parse_file, builds the
// KBO/LPO config from the parsed precedences, runs the saturator,
// and returns [status, n_rules, n_trace, n_cps].
//
// EXISTS sections are honoured: the run uses set_goal_existential
// so the narrow path engages.  v0 does not surface witness bindings
// (witness names live in the spec's variable table; mapping back to
// WL symbols would duplicate WL-side encoder state).  Callers that
// need witnesses keep using the expression form of TATP[].
//
// Inputs:
//   args[0] = path      (UTF8String).
//   args[1] = max_steps (Integer).
// Output: Int64 NumericArray of length 4 -- [status, n_rules,
// n_trace, n_cps].  status == ATP_RUNNING (0) signals a parse
// failure.
EXTERN_C DLLEXPORT int thvm_wl_atp_run_file(WolframLibraryData libData,
                                            mint argc, MArgument *args,
                                            MArgument res) {
  (void)argc;
  char *path     = MArgument_getUTF8String(args[0]);
  mint  max_step = MArgument_getInteger(args[1]);

  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;

  AtpStatus st_out  = ATP_RUNNING;
  u32       n_rules = 0, n_trace = 0, n_cps = 0;

  WaldSpec *spec = wald_init();
  if (spec != NULL && path != NULL) {
    WaldErr e = wald_parse_file(path, spec);
    if (e == WALD_OK) {
      static u32 weights_f[ATP_WL_CFG_MAX_LABELS];
      static u32 prec_f[ATP_WL_CFG_MAX_LABELS];
      for (u32 i = 0; i < ATP_WL_CFG_MAX_LABELS; i++) {
        weights_f[i] = 0; prec_f[i] = 0;
      }
      u32 max_label = 0;
      for (u32 i = 0; i < spec->n_symbols; i++) {
        if (spec->symbols[i].label > max_label) {
          max_label = spec->symbols[i].label;
        }
      }
      if (max_label < ATP_WL_CFG_MAX_LABELS) {
        for (u32 i = 0; i < spec->n_symbols; i++) {
          weights_f[spec->symbols[i].label] = 1;
          prec_f[spec->symbols[i].label]    =
            spec->symbols[i].prec_rank + 1;
        }
        static KboConfig cfg_f;
        cfg_f.weights    = weights_f;
        cfg_f.precedence = prec_f;
        cfg_f.n_labels   = max_label + 1;
        cfg_f.var_weight = 1;

        static LpoConfig lpo_cfg_f;
        lpo_cfg_f.precedence = prec_f;
        lpo_cfg_f.n_labels   = max_label + 1;

        AtpState *atp = thvm_atp_init(&cfg_f, (u32)max_step);
        if (atp != NULL) {
          if (spec->ordering_kind == WALD_ORDER_LPO) {
            thvm_atp_set_lpo(atp, &lpo_cfg_f);
          }
          for (u32 i = 0; i < spec->n_eqns; i++) {
            thvm_atp_add_equation(atp, spec->eqn_lhs[i], spec->eqn_rhs[i]);
          }
          if (spec->n_existential > 0) {
            thvm_atp_set_goal_existential(atp, spec->goal_lhs,
                                               spec->goal_rhs);
          } else {
            thvm_atp_set_goal(atp, spec->goal_lhs, spec->goal_rhs);
          }
          st_out  = thvm_atp_run(atp);
          n_rules = atp->n_rules;
          n_trace = atp->n_trace;
          n_cps   = atp->n_cps;
          thvm_atp_free(atp);
        }
      }
    }
  }
  if (spec != NULL) wald_free(spec);
  if (path != NULL) libData->UTF8String_disown(path);

  mint dims[1] = {4};
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_Bit64, 1, dims, &out);
  int64_t *odata = (int64_t *)naf->MNumericArray_getData(out);
  odata[0] = (int64_t)st_out;
  odata[1] = (int64_t)n_rules;
  odata[2] = (int64_t)n_trace;
  odata[3] = (int64_t)n_cps;
  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

// === ground QF_UF decider via src/cc congruence closure ==========
//
// Decides a ground quantifier-free uninterpreted-equality problem
// through the module's own congruence-closure engine (src/cc/_.c),
// the C twin of the WL prototype TSatEUF.  Returns 1 (SAT) / 0
// (UNSAT) so the WL FindFiniteModels "C" Method can prune partial
// operation tables exactly like its "CongruenceClosure" Method.
//
// Inputs (all Integer MTensors, rank-1 / flat):
//   args[0] = terms   : the distinct subterms in topological order
//                       (args precede the application that uses
//                       them).  A flat record stream where term id
//                       t (0-based, assigned in stream order) is
//                       [tag, sym, nargs, arg0 .. arg_{nargs-1}]:
//                         tag 0 -> atom, sym is the atom label;
//                         tag 1 -> application, sym is the fn label
//                                  and the nargs trailing entries
//                                  are the (already-built) child
//                                  term ids.
//   args[1] = eqPairs : flat even-length list a0,b0,a1,b1,.. of
//                       asserted-equal term-id pairs.
//   args[2] = nePairs : flat even-length list of asserted-unequal
//                       term-id pairs.
//
// Output: Integer 1 (CC_SAT) / 0 (CC_UNSAT).
// === incremental congruence-closure handle table ====================
//
// A persistent CcState exposed to WL by an integer handle so a whole
// finite-model search shares ONE engine: intern atoms/apps once, then
// push / assert / check / pop per backtracking node instead of
// re-deciding the entire problem at every cell.  Handles index a small
// grow-on-demand table; thvm_cc_free_handle nulls a slot for reuse.
static CcState **g_cc_handles = NULL;
static u32        g_cc_handle_cap = 0;

static int cc_handle_alloc(CcState *s) {
  u32 i = 0;
  for (; i < g_cc_handle_cap; i++) {
    if (g_cc_handles[i] == NULL) break;
  }
  if (i == g_cc_handle_cap) {
    u32 cap = g_cc_handle_cap ? g_cc_handle_cap * 2u : 8u;
    g_cc_handles = (CcState **)realloc(g_cc_handles, cap * sizeof(CcState *));
    for (u32 j = g_cc_handle_cap; j < cap; j++) g_cc_handles[j] = NULL;
    g_cc_handle_cap = cap;
  }
  g_cc_handles[i] = s;
  return (int)i;
}

static CcState *cc_handle_get(mint h) {
  if (h < 0 || (u32)h >= g_cc_handle_cap) return NULL;
  return g_cc_handles[h];
}

// thvm_cc_new[] -> handle (Integer).
EXTERN_C DLLEXPORT int thvm_cc_new(WolframLibraryData libData, mint argc,
                                   MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  CcState *s = cc_init();
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;
  MArgument_setInteger(res, (mint)cc_handle_alloc(s));
  return LIBRARY_NO_ERROR;
}

// thvm_cc_free_handle[handle] -> 0.
EXTERN_C DLLEXPORT int thvm_cc_free_handle(WolframLibraryData libData,
                                           mint argc, MArgument *args,
                                           MArgument res) {
  (void)libData; (void)argc;
  mint h = MArgument_getInteger(args[0]);
  CcState *s = cc_handle_get(h);
  if (s != NULL) { cc_free(s); g_cc_handles[h] = NULL; }
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

// thvm_cc_intern_atom[handle, atomId] -> node (Integer).
EXTERN_C DLLEXPORT int thvm_cc_intern_atom(WolframLibraryData libData,
                                           mint argc, MArgument *args,
                                           MArgument res) {
  (void)libData; (void)argc;
  CcState *s = cc_handle_get(MArgument_getInteger(args[0]));
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;
  MArgument_setInteger(res, (mint)cc_atom(s, (u32)MArgument_getInteger(args[1])));
  return LIBRARY_NO_ERROR;
}

// thvm_cc_intern_app[handle, fnId, argNodes (Integer,1)] -> node.
EXTERN_C DLLEXPORT int thvm_cc_intern_app(WolframLibraryData libData,
                                          mint argc, MArgument *args,
                                          MArgument res) {
  (void)argc;
  CcState *s = cc_handle_get(MArgument_getInteger(args[0]));
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;
  mint fn_id   = MArgument_getInteger(args[1]);
  MTensor at   = MArgument_getMTensor(args[2]);
  mint n       = libData->MTensor_getFlattenedLength(at);
  const mint *d = libData->MTensor_getIntegerData(at);
  if ((u32)n > REWRITE_MAX_ARITY) return LIBRARY_FUNCTION_ERROR;
  u32 argbuf[REWRITE_MAX_ARITY];
  for (mint i = 0; i < n; i++) argbuf[i] = (u32)d[i];
  MArgument_setInteger(res, (mint)cc_app(s, (u32)fn_id, argbuf, (u32)n));
  return LIBRARY_NO_ERROR;
}

// thvm_cc_push[handle] -> mark (Integer).
EXTERN_C DLLEXPORT int thvm_cc_push(WolframLibraryData libData, mint argc,
                                    MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  CcState *s = cc_handle_get(MArgument_getInteger(args[0]));
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;
  MArgument_setInteger(res, (mint)cc_push(s));
  return LIBRARY_NO_ERROR;
}

// thvm_cc_pop[handle, mark] -> 0.
EXTERN_C DLLEXPORT int thvm_cc_pop(WolframLibraryData libData, mint argc,
                                   MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  CcState *s = cc_handle_get(MArgument_getInteger(args[0]));
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;
  cc_pop(s, (u32)MArgument_getInteger(args[1]));
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

// thvm_cc_assert_eq[handle, a, b] -> 0.
EXTERN_C DLLEXPORT int thvm_cc_assert_eq(WolframLibraryData libData,
                                         mint argc, MArgument *args,
                                         MArgument res) {
  (void)libData; (void)argc;
  CcState *s = cc_handle_get(MArgument_getInteger(args[0]));
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;
  cc_assert_eq(s, (u32)MArgument_getInteger(args[1]),
                  (u32)MArgument_getInteger(args[2]));
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

// thvm_cc_assert_ne[handle, a, b] -> 0.
EXTERN_C DLLEXPORT int thvm_cc_assert_ne(WolframLibraryData libData,
                                         mint argc, MArgument *args,
                                         MArgument res) {
  (void)libData; (void)argc;
  CcState *s = cc_handle_get(MArgument_getInteger(args[0]));
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;
  cc_assert_ne(s, (u32)MArgument_getInteger(args[1]),
                  (u32)MArgument_getInteger(args[2]));
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

// thvm_cc_check[handle] -> 1 (SAT) / 0 (UNSAT).
EXTERN_C DLLEXPORT int thvm_cc_check(WolframLibraryData libData, mint argc,
                                     MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  CcState *s = cc_handle_get(MArgument_getInteger(args[0]));
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;
  MArgument_setInteger(res, cc_check(s) == CC_SAT ? 1 : 0);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_cc_decide(WolframLibraryData libData,
                                         mint argc, MArgument *args,
                                         MArgument res) {
  (void)argc;
  MTensor terms_t = MArgument_getMTensor(args[0]);
  MTensor eq_t    = MArgument_getMTensor(args[1]);
  MTensor ne_t    = MArgument_getMTensor(args[2]);

  mint terms_len = libData->MTensor_getFlattenedLength(terms_t);
  mint eq_len    = libData->MTensor_getFlattenedLength(eq_t);
  mint ne_len    = libData->MTensor_getFlattenedLength(ne_t);
  const mint *td = libData->MTensor_getIntegerData(terms_t);
  const mint *ed = libData->MTensor_getIntegerData(eq_t);
  const mint *nd = libData->MTensor_getIntegerData(ne_t);

  CcState *s = cc_init();
  if (s == NULL) return LIBRARY_FUNCTION_ERROR;

  // Map term id (stream order) -> cc node id.
  u32  node_stack[REWRITE_MAX_ARITY];
  u32 *node = (u32 *)malloc(sizeof(u32) * (size_t)(terms_len + 1));
  if (node == NULL) { cc_free(s); return LIBRARY_FUNCTION_ERROR; }
  u32  n_terms = 0;

  mint i = 0;
  while (i + 3 <= terms_len) {
    mint tag   = td[i];
    mint sym   = td[i + 1];
    mint nargs = td[i + 2];
    i += 3;
    if (tag == 0) {
      node[n_terms] = cc_atom(s, (u32)sym);
    } else {
      if ((u32)nargs > REWRITE_MAX_ARITY || i + nargs > terms_len) {
        free(node); cc_free(s); return LIBRARY_FUNCTION_ERROR;
      }
      for (mint a = 0; a < nargs; a++) node_stack[a] = node[td[i + a]];
      node[n_terms] = cc_app(s, (u32)sym, node_stack, (u32)nargs);
      i += nargs;
    }
    n_terms++;
  }

  for (mint p = 0; p + 2 <= eq_len; p += 2) {
    cc_assert_eq(s, node[ed[p]], node[ed[p + 1]]);
  }
  for (mint p = 0; p + 2 <= ne_len; p += 2) {
    cc_assert_ne(s, node[nd[p]], node[nd[p + 1]]);
  }

  CcResult r = cc_check(s);
  free(node);
  cc_free(s);
  MArgument_setInteger(res, r == CC_SAT ? 1 : 0);
  return LIBRARY_NO_ERROR;
}

// === finite-model "ExpressionPruneC" enumeration =====================
//
// thvm_wl_ffmep_solve marshals the WL-built clause DB into FfmepDb and
// returns the per-operator model index lists.  Inputs (all Integer
// MTensors / scalars), matching epClauseDB's emitted arrays:
//   args[0] = opSize   (Integer,1) : per-operator table size k^arity
//                                    (operator order), length nops.
//   args[1] = litFlat  (Integer,1) : every literal-int (cell*k + value),
//                                    contiguous per clause.
//   args[2] = clauseOff(Integer,1) : [nclauses+1] offsets into litFlat.
//   args[3] = setOff   (Integer,1) : [nsets+1] offsets into setClause.
//   args[4] = setClause(Integer,1) : clause ids grouped per clause-set.
//   args[5] = k        (Integer)   : domain size.
//   args[6] = maxItems (Integer)   : solution-tuple cap; -1 == Infinity.
// Output: Integer MTensor, nmodels x nops (row-major), one row per model
// (per-operator FromDigits index, operator order).  WL Union-sorts +
// applies the shared Take[UpTo[MaxItems]] tail.
EXTERN_C DLLEXPORT int thvm_wl_ffmep_solve(WolframLibraryData libData,
                                           mint argc, MArgument *args,
                                           MArgument res) {
  (void)argc;
  MTensor opsize_t   = MArgument_getMTensor(args[0]);
  MTensor lit_t      = MArgument_getMTensor(args[1]);
  MTensor clauseoff_t= MArgument_getMTensor(args[2]);
  MTensor setoff_t   = MArgument_getMTensor(args[3]);
  MTensor setcl_t    = MArgument_getMTensor(args[4]);
  mint    k          = MArgument_getInteger(args[5]);
  mint    max_items  = MArgument_getInteger(args[6]);

  mint nops      = libData->MTensor_getFlattenedLength(opsize_t);
  mint nlit      = libData->MTensor_getFlattenedLength(lit_t);
  mint nclauseo  = libData->MTensor_getFlattenedLength(clauseoff_t);
  mint nseto     = libData->MTensor_getFlattenedLength(setoff_t);
  mint nsetcl    = libData->MTensor_getFlattenedLength(setcl_t);
  const mint *opsize_d = libData->MTensor_getIntegerData(opsize_t);
  const mint *lit_d    = libData->MTensor_getIntegerData(lit_t);
  const mint *clo_d    = libData->MTensor_getIntegerData(clauseoff_t);
  const mint *seto_d   = libData->MTensor_getIntegerData(setoff_t);
  const mint *setcl_d  = libData->MTensor_getIntegerData(setcl_t);

  // Build op_off (cumulative) + op_size from the WL opSize array.
  u32 *op_off  = (u32 *)malloc((size_t)(nops + 1) * sizeof(u32));
  u32 *op_size = (u32 *)malloc((size_t)(nops ? nops : 1) * sizeof(u32));
  i64 *lit     = (i64 *)malloc((size_t)(nlit ? nlit : 1) * sizeof(i64));
  u32 *clo     = (u32 *)malloc((size_t)(nclauseo ? nclauseo : 1) * sizeof(u32));
  u32 *seto    = (u32 *)malloc((size_t)(nseto ? nseto : 1) * sizeof(u32));
  u32 *setcl   = (u32 *)malloc((size_t)(nsetcl ? nsetcl : 1) * sizeof(u32));
  if (!op_off || !op_size || !lit || !clo || !seto || !setcl) {
    free(op_off); free(op_size); free(lit); free(clo); free(seto); free(setcl);
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 acc = 0;
  for (mint i = 0; i < nops; i++) {
    op_off[i]  = acc;
    op_size[i] = (u32)opsize_d[i];
    acc       += (u32)opsize_d[i];
  }
  op_off[nops] = acc;
  for (mint i = 0; i < nlit;     i++) lit[i]   = (i64)lit_d[i];
  for (mint i = 0; i < nclauseo; i++) clo[i]   = (u32)clo_d[i];
  for (mint i = 0; i < nseto;    i++) seto[i]  = (u32)seto_d[i];
  for (mint i = 0; i < nsetcl;   i++) setcl[i] = (u32)setcl_d[i];

  FfmepDb db;
  memset(&db, 0, sizeof(db));
  db.k          = (u32)k;
  db.ncells     = acc;
  db.nops       = (u32)nops;
  db.op_off     = op_off;
  db.op_size    = op_size;
  db.lit        = lit;
  db.clause_off = clo;
  db.nsets      = (nseto > 0) ? (u32)(nseto - 1) : 0u;
  db.set_off    = seto;
  db.set_clause = setcl;

  u32 nmodels = 0;
  i64 *models = ffmep_solve(&db, (i64)max_items, &nmodels);

  free(op_off); free(op_size); free(lit); free(clo); free(seto); free(setcl);
  if (models == NULL && nmodels != 0) return LIBRARY_FUNCTION_ERROR;

  MTensor out;
  mint dims[2] = { (mint)nmodels, nops };
  int err = libData->MTensor_new(MType_Integer, 2, dims, &out);
  if (err != LIBRARY_NO_ERROR) { free(models); return err; }
  mint *dst = libData->MTensor_getIntegerData(out);
  for (mint i = 0; i < (mint)nmodels * nops; i++) dst[i] = (mint)models[i];
  free(models);
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// === ENIGMA Tier 2: anonymised CP hypergraph export ==================
//
// Bridges thvm_atp_cp_graph (src/atp/_.c) to WL.  Builds the purely
// structural typed graph for the critical pair (lhs, rhs) and ships it
// as ONE self-describing f64 NumericArray the WL side unpacks:
//
//   [0] ok        (1 success / 0 overflow-or-failure)
//   [1] n_nodes
//   [2] n_edges
//   [3] feat_dim  (= ATP_CPG_FEAT_DIM)
//   then  node_type[n_nodes]                       (n_nodes f64)
//   then  node_feat[n_nodes * feat_dim] row-major  (n_nodes*feat_dim f64)
//   then  edge_src[n_edges]                         (n_edges f64)
//   then  edge_dst[n_edges]                         (n_edges f64)
//   then  edge_type[n_edges]                        (n_edges f64)
//
// On overflow / failure the header is [0, 0, 0, feat_dim] with no body,
// so the WL caller can fall back to the Tier-1 feature vector.  Node
// features are PURELY STRUCTURAL (kind / arity / occurrence count); the
// concrete symbol labels and variable ids never reach WL.
//
// Inputs:
//   args[0] = lhs Term (mint, packed Term value).
//   args[1] = rhs Term (mint, packed Term value).
//
// The graph is large (~78 KB) so it is malloc'd rather than stacked.
EXTERN_C DLLEXPORT int thvm_wl_atp_cp_graph(WolframLibraryData libData,
                                            mint argc, MArgument *args,
                                            MArgument res) {
  (void)argc;
  Term lhs = (Term)MArgument_getInteger(args[0]);
  Term rhs = (Term)MArgument_getInteger(args[1]);

  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;

  AtpCpGraph *g = (AtpCpGraph *)malloc(sizeof(AtpCpGraph));
  if (g == NULL) return LIBRARY_FUNCTION_ERROR;

  int ok = thvm_atp_cp_graph(lhs, rhs, g);

  // On overflow / failure ship just the 4-int header so WL falls back.
  u32 n_nodes = ok ? g->n_nodes : 0u;
  u32 n_edges = ok ? g->n_edges : 0u;
  mint total  = 4
              + (mint)n_nodes                         // node_type
              + (mint)n_nodes * (mint)ATP_CPG_FEAT_DIM // node_feat
              + (mint)n_edges * 3                      // src/dst/type
              + (mint)n_nodes;                         // node_label

  MNumericArray out;
  mint dims[1] = { total };
  if (naf->MNumericArray_new(MNumericArray_Type_Real64, 1, dims, &out)
        != 0) {
    free(g);
    return LIBRARY_FUNCTION_ERROR;
  }
  double *o = (double *)naf->MNumericArray_getData(out);
  mint at = 0;
  o[at++] = (double)(ok ? 1 : 0);
  o[at++] = (double)n_nodes;
  o[at++] = (double)n_edges;
  o[at++] = (double)ATP_CPG_FEAT_DIM;
  for (u32 i = 0; i < n_nodes; i++) o[at++] = (double)g->node_type[i];
  for (u32 i = 0; i < n_nodes * ATP_CPG_FEAT_DIM; i++)
    o[at++] = (double)g->node_feat[i];
  for (u32 i = 0; i < n_edges; i++) o[at++] = (double)g->edge_src[i];
  for (u32 i = 0; i < n_edges; i++) o[at++] = (double)g->edge_dst[i];
  for (u32 i = 0; i < n_edges; i++) o[at++] = (double)g->edge_type[i];
  // Concrete symbol identity per node (the un-anonymised reconstruction
  // key), tail of the array so older decoders that stop after edge_type
  // are unaffected.
  for (u32 i = 0; i < n_nodes; i++) o[at++] = (double)g->node_label[i];

  free(g);
  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

// === ENIGMA Tier 2: persistent step-wise proof handle (GNN re-rank) ===
//
// thvm_wl_atp_run_proof runs the saturation atomically (init -> run ->
// free in one call).  These expose a PERSISTENT AtpState across
// LibraryLink calls so a WL driver can step the loop, pull the queued
// CPs, score them with the GNN, and push re-ranked priorities back
// (thvm_atp_set_cp_pri_by_seq) -- the amortised-eval-server loop.  The
// handle is the AtpState pointer returned as an Integer.  ONE proof at a
// time: the config arrays below are file-static, valid until the next
// _init (the re-rank measure runs proofs sequentially).  This path
// measures status + step count, not the proof chain, so record-norm is
// off and no proof-object reconstruction is wired here.

EXTERN_C DLLEXPORT int thvm_wl_atp_proof_init(WolframLibraryData libData,
                                              mint argc, MArgument *args,
                                              MArgument res) {
  (void)argc;
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  mint max_steps = MArgument_getInteger(args[1]);
  mint max_label = MArgument_getInteger(args[2]);
  mint cp_weight = MArgument_getInteger(args[3]);
  mint ordering  = MArgument_getInteger(args[4]);
  mint auto_prec = MArgument_getInteger(args[5]);
  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;
  if (naf->MNumericArray_getType(na) != MNumericArray_Type_Bit64) {
    return LIBRARY_FUNCTION_ERROR;
  }
  mint flat_len = naf->MNumericArray_getFlattenedLength(na);
  if (flat_len < 3) return LIBRARY_FUNCTION_ERROR;
  const int64_t *data = (const int64_t *)naf->MNumericArray_getData(na);
  int64_t n_ax_i = data[0];
  if (n_ax_i < 0 || (int64_t)flat_len != 3 * n_ax_i + 3) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 n_ax = (u32)n_ax_i;
  if ((u32)max_label >= ATP_WL_CFG_MAX_LABELS) return LIBRARY_FUNCTION_ERROR;
  static u32 rr_weights[ATP_WL_CFG_MAX_LABELS];
  static u32 rr_prec[ATP_WL_CFG_MAX_LABELS];
  for (u32 i = 0; i < (u32)max_label + 1; i++) {
    rr_weights[i] = 1;
    rr_prec[i]    = i + 1;
  }
  if (auto_prec != 0) {
    static Term ax_lhs[ATP_WL_CFG_MAX_LABELS];
    static Term ax_rhs[ATP_WL_CFG_MAX_LABELS];
    u32 m = n_ax < ATP_WL_CFG_MAX_LABELS ? n_ax : ATP_WL_CFG_MAX_LABELS;
    for (u32 i = 0; i < m; i++) {
      ax_lhs[i] = (Term)data[1 + 2 * i + 0];
      ax_rhs[i] = (Term)data[1 + 2 * i + 1];
    }
    atp_auto_precedence(ax_lhs, ax_rhs, m, (u32)max_label + 1, rr_prec);
  }
  static KboConfig rr_kbo;
  rr_kbo.weights = rr_weights;
  rr_kbo.precedence = rr_prec;
  rr_kbo.n_labels = (u32)max_label + 1;
  rr_kbo.var_weight = 1;
  AtpState *atp = thvm_atp_init(&rr_kbo, (u32)max_steps);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;
  static LpoConfig rr_lpo;
  if (ordering == 1) {
    rr_lpo.precedence = rr_prec;
    rr_lpo.n_labels = (u32)max_label + 1;
    thvm_atp_set_lpo(atp, &rr_lpo);
  }
  if (cp_weight >= 0) thvm_atp_set_cp_weight_mode(atp, (u32)cp_weight);
  // Lean base config (KBO/LPO + cp_weight + auto-precedence), matching
  // the fast structure-recognized completion path; the re-rank overrides
  // selection order via thvm_atp_set_cp_pri_by_seq between chunks.
  // Record-norm off: this path measures status, not the proof chain.
  // (UnfailingCP / AutoMaxWeight / RHSInterreduce intentionally NOT set
  // here -- they exploded the queue on easy orientable goals; add per
  // problem-class once the re-rank latency is addressed.)
  thvm_atp_set_record_norm_steps(atp, 0u);
  for (u32 i = 0; i < n_ax; i++) {
    Term lhs  = (Term)data[1 + 2 * i + 0];
    Term rhs  = (Term)data[1 + 2 * i + 1];
    int64_t f = data[1 + 2 * n_ax + 2 + i];
    if (f == 1) {
      thvm_atp_install_oriented_rule(atp, lhs, rhs);
    } else {
      thvm_atp_add_equation(atp, lhs, rhs);
    }
  }
  Term gl = (Term)data[1 + 2 * n_ax + 0];
  Term gr = (Term)data[1 + 2 * n_ax + 1];
  if (gl != 0u || gr != 0u) thvm_atp_set_goal(atp, gl, gr);
  MArgument_setInteger(res, (mint)(intptr_t)atp);
  return LIBRARY_NO_ERROR;
}

// Run up to k saturation steps; return the AtpStatus code (stops early
// on a terminal status).
EXTERN_C DLLEXPORT int thvm_wl_atp_proof_step(WolframLibraryData libData,
                                              mint argc, MArgument *args,
                                              MArgument res) {
  (void)libData; (void)argc;
  AtpState *atp = (AtpState *)(intptr_t)MArgument_getInteger(args[0]);
  mint k = MArgument_getInteger(args[1]);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;
  AtpStatus st = ATP_RUNNING;
  for (mint i = 0; i < k; i++) {
    st = thvm_atp_step(atp);
    if (st != ATP_RUNNING) break;
  }
  MArgument_setInteger(res, (mint)st);
  return LIBRARY_NO_ERROR;
}

// Pull the live queued CPs as a packed Int64 array
// [count, lhs_0, rhs_0, seq_0, lhs_1, ...].
EXTERN_C DLLEXPORT int thvm_wl_atp_proof_queued(WolframLibraryData libData,
                                                mint argc, MArgument *args,
                                                MArgument res) {
  (void)argc;
  AtpState *atp = (AtpState *)(intptr_t)MArgument_getInteger(args[0]);
  mint cap = MArgument_getInteger(args[1]);
  if (atp == NULL || cap <= 0) return LIBRARY_FUNCTION_ERROR;
  Term *lhs = (Term *)malloc((size_t)cap * sizeof(Term));
  Term *rhs = (Term *)malloc((size_t)cap * sizeof(Term));
  u32  *seq = (u32 *)malloc((size_t)cap * sizeof(u32));
  if (lhs == NULL || rhs == NULL || seq == NULL) {
    free(lhs); free(rhs); free(seq);
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 cnt = thvm_atp_queued_cps(atp, lhs, rhs, seq, (u32)cap);
  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;
  mint dims[1] = {1 + 3 * (mint)cnt};
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_Bit64, 1, dims, &out);
  int64_t *o = (int64_t *)naf->MNumericArray_getData(out);
  o[0] = (int64_t)cnt;
  for (u32 i = 0; i < cnt; i++) {
    o[1 + 3 * i + 0] = (int64_t)lhs[i];
    o[1 + 3 * i + 1] = (int64_t)rhs[i];
    o[1 + 3 * i + 2] = (int64_t)seq[i];
  }
  free(lhs); free(rhs); free(seq);
  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

// Push WL-computed priorities: args[1] seq (Int,1), args[2] pri (Int,1).
EXTERN_C DLLEXPORT int thvm_wl_atp_proof_setpri(WolframLibraryData libData,
                                                mint argc, MArgument *args,
                                                MArgument res) {
  (void)argc;
  AtpState *atp = (AtpState *)(intptr_t)MArgument_getInteger(args[0]);
  MTensor seqT = MArgument_getMTensor(args[1]);
  MTensor priT = MArgument_getMTensor(args[2]);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;
  mint n = libData->MTensor_getFlattenedLength(seqT);
  if (libData->MTensor_getFlattenedLength(priT) != n) {
    return LIBRARY_FUNCTION_ERROR;
  }
  const mint *sd = libData->MTensor_getIntegerData(seqT);
  const mint *pd = libData->MTensor_getIntegerData(priT);
  u32 *seq = (u32 *)malloc((size_t)(n > 0 ? n : 1) * sizeof(u32));
  u32 *pri = (u32 *)malloc((size_t)(n > 0 ? n : 1) * sizeof(u32));
  if (seq == NULL || pri == NULL) { free(seq); free(pri); return LIBRARY_FUNCTION_ERROR; }
  for (mint i = 0; i < n; i++) { seq[i] = (u32)sd[i]; pri[i] = (u32)pd[i]; }
  thvm_atp_set_cp_pri_by_seq(atp, seq, pri, (u32)n);
  free(seq); free(pri);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

// Set the in-engine GNN re-rank period on a persistent proof handle.
// With this period > 0 AND a GNN model loaded (thvm_wl_atp_set_gnn_scorer),
// thvm_atp_step itself re-ranks the CP queue every `period` selections --
// the whole GCN forward runs in C on thvm's tensor runtime, so a driver
// can just step the handle in big chunks with NO WL round-trip between
// re-ranks.  args[1] = period (0 = off).
EXTERN_C DLLEXPORT int thvm_wl_atp_proof_set_gnn_period(WolframLibraryData libData,
                                                        mint argc, MArgument *args,
                                                        MArgument res) {
  (void)libData; (void)argc;
  AtpState *atp = (AtpState *)(intptr_t)MArgument_getInteger(args[0]);
  mint period   = MArgument_getInteger(args[1]);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;
  thvm_atp_set_gnn_rerank_period(atp, period < 0 ? 0u : (u32)period);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_atp_proof_free(WolframLibraryData libData,
                                              mint argc, MArgument *args,
                                              MArgument res) {
  (void)libData; (void)argc;
  AtpState *atp = (AtpState *)(intptr_t)MArgument_getInteger(args[0]);
  if (atp != NULL) thvm_atp_free(atp);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}
