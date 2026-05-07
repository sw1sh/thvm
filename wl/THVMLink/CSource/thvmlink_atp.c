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
  thvm_atp_set_goal(atp, goal_lhs, goal_rhs);

  AtpStatus st = thvm_atp_run(atp);

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
