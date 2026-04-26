(* ::Package:: *)
(* ATP.wl - WL surface for the IC-native ATP saturation engine
   (stage 8.7 onwards in docs/plans/waldmeister_ic_atp_tasks.md).

   Public surface
     TATP[axioms, conjecture, opts]  -- run TATP on a list of
                                        equational axioms + a single
                                        conjecture; returns an
                                        Association with
                                        Status/Steps/Rules/QueueSize.
     TATP[File["path.pr"], opts]      -- file-form: parse a Waldmeister
                                        .pr spec via wald_parse_file
                                        on the C side and run the
                                        saturator (stage 9.2).

   Options
     MaxSteps -> 64                   -- saturation step budget.
     Witness  -> {x_, y_, ...}        -- existential goal: solve for
                                        these vars (stage 8.9e).
     AllWitnesses -> True             -- enumerate ALL witnesses via
                                        thvm_atp_narrow_all (stage
                                        9.1c).  Result key flips from
                                        "Witness" to "Witnesses".
     MaxDepth, MaxWitnesses           -- bounds for AllWitnesses.

   See docs/plans/waldmeister_ic_atp.md for the algorithmic intent.
*)

BeginPackage["THVMLink`"];

TATP::usage = "TATP[{lhs == rhs, ...}, conjecture] runs the IC-native ATP saturation on the given equational axioms and conjecture, returning an Association with Status, Steps, Rules, QueueSize.  Variables are written as `x_` (Pattern[name, Blank[]]).  TATP[File[path]] parses a Waldmeister .pr file and runs the saturator directly.";

Begin["`Private`"];

(* === LibraryLink loaders ============================================ *)

(* 8.7b: ATP runner.  Takes a packed Int64 NumericArray of Term
   values (`[n_axioms, lhs_0, rhs_0, ..., goal_lhs, goal_rhs]`),
   max_steps, and max_label.  Returns a 4-element Int64
   NumericArray `[status, n_rules, n_trace, n_cps]`. *)
$atpRunFn        := $atpRunFn        = load["thvm_wl_atp_run", {{"NumericArray", "Shared"}, Integer, Integer}, "NumericArray"];

(* 8.9e: existential ATP runner.  Same as $atpRunFn but takes an
   extra witness-id MTensor; output array gains n_witness trailing
   Term values. *)
$atpRunExistFn   := $atpRunExistFn   = load["thvm_wl_atp_run_existential", {{"NumericArray", "Shared"}, Integer, Integer, {Integer, 1}}, "NumericArray"];

(* 9.1c: multi-witness ATP runner.  Saturates first, then calls
   thvm_atp_narrow_all on the original goal.  Output array layout:
   [status, n_rules, n_trace, n_cps, n_found,
    w_0_id_0, ..., w_(max_witnesses-1)_id_(n_witness-1)].
   Length = 5 + max_witnesses * n_witness. *)
$atpRunAllFn     := $atpRunAllFn     = load["thvm_wl_atp_run_all_witnesses", {{"NumericArray", "Shared"}, Integer, Integer, {Integer, 1}, Integer, Integer}, "NumericArray"];

(* 9.2: file-driven ATP runner.  Takes a Waldmeister .pr path,
   parses it via wald_parse_file, runs the saturator, and returns
   a 4-element [status, n_rules, n_trace, n_cps] NumericArray. *)
$atpRunFileFn    := $atpRunFileFn    = load["thvm_wl_atp_run_file", {"UTF8String", Integer}, "NumericArray"];

(* 8.7c: CTR-builder for the ATP expression encoder.  Takes a
   label and a NumericArray of child Term values; returns the
   packed Term value of the new TAG_CTR. *)
$termNewCtrFn    := $termNewCtrFn    = load["thvm_wl_term_new_ctr", {Integer, {Integer, 1}}, Integer];

(* === WL-expression-to-Term encoder ================================== *)

(* 8.7c: WL-expression-to-Term encoder.  Maps:
     Pattern[name, Blank[]]  -> term_new_fvr(var_id)
     Symbol[name]            -> nullary CTR
     head[args...]           -> CTR with encoded children
   State is threaded explicitly: takes (expr, state) and returns
   {term, state'} where state is an Association
   {"sym" -> <|name -> label|>, "var" -> <|name -> id|>,
    "next_lab" -> next_label}.
   Patterns are matched via Verbatim[Pattern] so the Pattern head
   isn't itself parsed as a pattern. *)
encodeAtpTerm[Verbatim[Pattern][name_Symbol, Blank[]], state_Association] :=
  Module[{vars, varName, varId, st = state},
    varName = SymbolName[Unevaluated[name]];
    vars = st["var"];
    If[ KeyExistsQ[vars, varName],
      varId = vars[varName],
      varId = Length[vars];
      st = ReplacePart[st, "var" -> Append[vars, varName -> varId]];
    ];
    {THVMLink`Private`$termNewFn[0, 22 (* TAG_FVR *), varId, 0], st}
  ]

encodeAtpTerm[s_Symbol, state_Association] :=
  Module[{sym, syms, lab, st = state},
    sym = ToString[Unevaluated[s]];
    syms = st["sym"];
    If[ KeyExistsQ[syms, sym],
      lab = syms[sym],
      lab = st["next_lab"];
      st = ReplacePart[st,
        {"sym"      -> Append[syms, sym -> lab],
         "next_lab" -> lab + 1}];
    ];
    {THVMLink`Private`$termNewCtrFn[lab, {}], st}
  ]

encodeAtpTerm[expr_, state_Association] :=
  Module[{h, sym, syms, lab, st = state, childEncs, childRes, ctr},
    h = Head[expr];
    sym = ToString[h];
    syms = st["sym"];
    If[ KeyExistsQ[syms, sym],
      lab = syms[sym],
      lab = st["next_lab"];
      st = ReplacePart[st,
        {"sym"      -> Append[syms, sym -> lab],
         "next_lab" -> lab + 1}];
    ];
    childEncs = {};
    Do[
      childRes = encodeAtpTerm[expr[[i]], st];
      AppendTo[childEncs, childRes[[1]]];
      st = childRes[[2]],
      {i, Length[expr]}
    ];
    ctr = THVMLink`Private`$termNewCtrFn[lab, childEncs];
    {ctr, st}
  ]

(* Convenience wrapper: build the empty encoder state. *)
encodeAtpTermInit[] := <|"sym" -> <||>, "var" -> <||>, "next_lab" -> 1|>

(* === TATP[] WL surface ============================================== *)

(* 8.7d: TATP[axioms, conjecture] WL surface.  Encodes axioms +
   goal via encodeAtpTerm, calls $atpRunFn, decodes the stats
   into a notebook-friendly Association.

   Each axiom must be a `lhs == rhs` form (Equal[lhs, rhs]); the
   conjecture is a single such form.  Variables are written as
   `x_` (Pattern[x, Blank[]]).  Symbols and head[args...] forms
   are translated to nullary / compound CTRs.

   Returns: <|"Status" -> str, "Steps" -> n, "Rules" -> n, ...|>.
   Status decoded from the AtpStatus enum:
     0 -> "RUNNING" (shouldn't appear after thvm_atp_run returns)
     1 -> "PROVED"
     2 -> "REFUTED"
     3 -> "TIMEOUT"
     4 -> "QUEUE_EMPTY"
*)
$atpStatusName = <|
  0 -> "RUNNING", 1 -> "PROVED", 2 -> "REFUTED",
  3 -> "TIMEOUT", 4 -> "QUEUE_EMPTY"
|>

(* HoldAll on TATP: WL evaluates `a == a` to True before
   reaching us; we need the syntactic Equal[lhs, rhs] form to
   destructure.  Inside TATP we map over axioms[i] / conjecture
   without forcing evaluation.  Internal helper TATPCore takes
   the held-list-of-equations and the held-conjecture. *)
SetAttributes[TATP, HoldAll];

(* 9.2: file-form dispatch.  When the first argument is a literal
   File[path_String], parse the .pr file via wald_parse_file on the
   C side and run the saturator.  The .pr file already contains
   the axioms, EXISTS section, and conjecture, so no second
   argument is needed.  Returns the same Status/Steps/Rules/
   QueueSize Association as the expression form (witness bindings
   are not surfaced in v0 -- callers that need them keep using the
   expression form). *)
TATP[File[path_String],
     OptionsPattern[{MaxSteps -> 64}]] :=
  Catch[
    Module[{stats, statusCode},
      ensureInit[];
      stats = Normal @ $atpRunFileFn[path, OptionValue[MaxSteps]];
      statusCode = stats[[1]];
      <|
        "Status"    -> Lookup[$atpStatusName, statusCode,
                              "UNKNOWN(" <> ToString[statusCode] <> ")"],
        "Steps"     -> stats[[3]],
        "Rules"     -> stats[[2]],
        "QueueSize" -> stats[[4]]
      |>
    ],
    "TATPError"
  ]

TATP[axioms_, conjecture_,
     OptionsPattern[{MaxSteps -> 64, Witness -> {},
                     AllWitnesses -> False, MaxDepth -> 8,
                     MaxWitnesses -> 16}]] :=
  Catch[
    Module[
      {state, packed, axTerms, goalLhs, goalRhs, stats,
       maxLab, statusCode, ax, lhs, rhs, lhsRes, rhsRes, cj,
       witnessSpec, witnessIds, witnessNames, wn, wid,
       witnessAssoc, baseResult, witnessVals,
       allWitnesses, maxDepth, maxWitnesses, nFound,
       witnessRows, witnessAssocs, k, ws},
      ensureInit[];
      state = encodeAtpTermInit[];
      axTerms = {};
      If[ Head[Unevaluated[axioms]] =!= List,
        Throw[Failure["TATPParseError",
          <|"Reason" -> "axioms must be a List"|>], "TATPError"]
      ];
      Do[
        ax = Extract[Hold[axioms], {1, i}, HoldComplete];
        If[ ! MatchQ[ax, HoldComplete[Equal[_, _]]],
          Throw[Failure["TATPParseError",
            <|"Axiom" -> i, "Reason" -> "expected `lhs == rhs`"|>],
            "TATPError"]
        ];
        lhs = Extract[ax, {1, 1}, HoldComplete];
        rhs = Extract[ax, {1, 2}, HoldComplete];
        lhsRes = encodeAtpTerm[lhs[[1]], state];
        AppendTo[axTerms, lhsRes[[1]]];
        state = lhsRes[[2]];
        rhsRes = encodeAtpTerm[rhs[[1]], state];
        AppendTo[axTerms, rhsRes[[1]]];
        state = rhsRes[[2]],
        {i, Length[Unevaluated[axioms]]}
      ];
      cj = Extract[Hold[conjecture], 1, HoldComplete];
      If[ ! MatchQ[cj, HoldComplete[Equal[_, _]]],
        Throw[Failure["TATPParseError",
          <|"Reason" -> "conjecture must be `lhs == rhs`"|>],
          "TATPError"]
      ];
      lhsRes = encodeAtpTerm[Extract[cj, {1, 1}, HoldComplete][[1]], state];
      goalLhs = lhsRes[[1]];
      state = lhsRes[[2]];
      rhsRes = encodeAtpTerm[Extract[cj, {1, 2}, HoldComplete][[1]], state];
      goalRhs = rhsRes[[1]];
      state = rhsRes[[2]];
      packed = NumericArray[
        Join[{Length[Unevaluated[axioms]]}, axTerms, {goalLhs, goalRhs}],
        "Integer64"
      ];
      maxLab = state["next_lab"];

      (* 8.9e: Witness option.  Each entry is a Pattern[name, _]
         (the `x_` syntax); resolve to its FVR id via the encoder
         state.  Names not in state["var"] yield Failure.
         OptionValue[Witness] returns the list directly -- the
         Pattern[] structures are atomic in WL (don't auto-evaluate)
         so we can index into them without holding. *)
      witnessSpec   = OptionValue[Witness];
      allWitnesses  = OptionValue[AllWitnesses];
      maxDepth      = OptionValue[MaxDepth];
      maxWitnesses  = OptionValue[MaxWitnesses];
      If[ Length[witnessSpec] == 0,
        (* Universal goal -- existing path. *)
        stats = Normal @ $atpRunFn[packed, OptionValue[MaxSteps], maxLab];
        statusCode = stats[[1]];
        <|
          "Status" -> Lookup[$atpStatusName, statusCode,
                             "UNKNOWN(" <> ToString[statusCode] <> ")"],
          "Steps"  -> stats[[3]],
          "Rules"  -> stats[[2]],
          "QueueSize" -> stats[[4]]
        |>,
        (* Existential goal -- resolve witness names to FVR ids. *)
        witnessNames = {};
        witnessIds   = {};
        Do[
          With[{w = witnessSpec[[i]]},
            If[ ! MatchQ[w, Verbatim[Pattern][_Symbol, Blank[]]],
              Throw[Failure["TATPParseError",
                <|"Reason" -> "Witness entries must be `x_` patterns"|>],
                "TATPError"]
            ];
            wn = Replace[w,
              Verbatim[Pattern][s_Symbol, Blank[]] :>
                SymbolName[Unevaluated[s]]];
            AppendTo[witnessNames, wn];
            wid = Lookup[state["var"], wn, $Failed];
            If[ wid === $Failed,
              Throw[Failure["TATPParseError",
                <|"Reason" -> "Witness var `" <> wn <>
                              "` not present in axioms / conjecture"|>],
                "TATPError"]
            ];
            AppendTo[witnessIds, wid];
          ],
          {i, Length[witnessSpec]}
        ];
        If[ allWitnesses,
          (* 9.1c: multi-witness path -- saturate then narrow_all. *)
          stats = Normal @ $atpRunAllFn[packed, OptionValue[MaxSteps],
                                        maxLab, witnessIds,
                                        maxDepth, maxWitnesses];
          statusCode = stats[[1]];
          nFound = stats[[5]];
          k = Length[witnessIds];
          witnessRows =
            If[ nFound > 0 && k > 0,
              Partition[stats[[6 ;; 5 + nFound * k]], k],
              {}
            ];
          witnessAssocs = Table[
            AssociationThread[Symbol /@ witnessNames -> ws],
            {ws, witnessRows}
          ];
          <|
            "Status"    -> Lookup[$atpStatusName, statusCode,
                                  "UNKNOWN(" <> ToString[statusCode] <> ")"],
            "Steps"     -> stats[[3]],
            "Rules"     -> stats[[2]],
            "QueueSize" -> stats[[4]],
            "Witnesses" -> witnessAssocs
          |>,
          (* Single-witness path (8.9e). *)
          stats = Normal @ $atpRunExistFn[packed, OptionValue[MaxSteps],
                                          maxLab, witnessIds];
          statusCode = stats[[1]];
          witnessVals = stats[[5 ;; 4 + Length[witnessIds]]];
          witnessAssoc = AssociationThread[
            Symbol /@ witnessNames -> witnessVals
          ];
          <|
            "Status"    -> Lookup[$atpStatusName, statusCode,
                                  "UNKNOWN(" <> ToString[statusCode] <> ")"],
            "Steps"     -> stats[[3]],
            "Rules"     -> stats[[2]],
            "QueueSize" -> stats[[4]],
            "Witness"   -> witnessAssoc
          |>
        ]
      ]
    ],
    "TATPError"
  ]

End[];
EndPackage[];
