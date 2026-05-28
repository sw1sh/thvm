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
//              lhs_{n-1}, rhs_{n-1}, goal_lhs, goal_rhs].
//             Length = 1 + 2*n_axioms + 2.
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
  if (n_ax_i < 0 || (int64_t)flat_len != 1 + 2 * n_ax_i + 2) {
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
    Term lhs = (Term)data[1 + 2 * i + 0];
    Term rhs = (Term)data[1 + 2 * i + 1];
    if (!thvm_atp_add_equation(atp, lhs, rhs)) {
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
  if (n_ax_i < 0 || (int64_t)flat_len != 1 + 2 * n_ax_i + 2) {
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

  // Push axioms.
  for (u32 i = 0; i < n_ax; i++) {
    Term lhs = (Term)data[1 + 2 * i + 0];
    Term rhs = (Term)data[1 + 2 * i + 1];
    if (!thvm_atp_add_equation(atp, lhs, rhs)) {
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
  if (n_ax_i < 0 || (int64_t)flat_len != 1 + 2 * n_ax_i + 2) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 n_ax = (u32)n_ax_i;

  if ((u32)max_label >= ATP_WL_CFG_MAX_LABELS) {
    return LIBRARY_FUNCTION_ERROR;
  }
  static u32 wl_weights_p[ATP_WL_CFG_MAX_LABELS];
  static u32 wl_precedence_p[ATP_WL_CFG_MAX_LABELS];
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
  //   2 = Vampire `sp=occurrence` / E InvFreqRank (atp_occurrence_precedence)
  if (auto_prec != 0) {
    static Term ax_lhs[ATP_WL_CFG_MAX_LABELS];
    static Term ax_rhs[ATP_WL_CFG_MAX_LABELS];
    u32 n_ax_use = n_ax < ATP_WL_CFG_MAX_LABELS ? n_ax
                                                : ATP_WL_CFG_MAX_LABELS;
    for (u32 i = 0; i < n_ax_use; i++) {
      ax_lhs[i] = (Term)data[1 + 2 * i + 0];
      ax_rhs[i] = (Term)data[1 + 2 * i + 1];
    }
    if (auto_prec == 2) {
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

  for (u32 i = 0; i < n_ax; i++) {
    Term lhs = (Term)data[1 + 2 * i + 0];
    Term rhs = (Term)data[1 + 2 * i + 1];
    if (!thvm_atp_add_equation(atp, lhs, rhs)) {
      thvm_atp_free(atp);
      return LIBRARY_FUNCTION_ERROR;
    }
  }
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
  AtpStatus st = thvm_atp_run(atp);
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
  if (st == ATP_PROVED) {
    ext = thvm_atp_init(&wl_kbo_p, (u32)max_steps);
    if (ext != NULL) {
      ext->wall_deadline_us = atp->wall_deadline_us;
      for (u32 i = 0; i < n_ax; i++) {
        thvm_atp_orient_and_add(ext, (Term)data[1 + 2 * i + 0],
                                     (Term)data[1 + 2 * i + 1]);
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
  if (n_ax_i < 0 || (int64_t)flat_len != 1 + 2 * n_ax_i + 2) {
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
    Term lhs = (Term)data[1 + 2 * i + 0];
    Term rhs = (Term)data[1 + 2 * i + 1];
    if (!thvm_atp_add_equation(atp, lhs, rhs)) {
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
