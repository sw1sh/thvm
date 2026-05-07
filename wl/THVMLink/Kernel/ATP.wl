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

TFindEquationalProof::usage = "TFindEquationalProof[conjecture, axioms] runs the IC-native ATP and returns a real WL ProofObject -- the same head FindEquationalProof returns, supporting the full property interface (p[\"ProofDataset\"], p[\"ProofGraph\"], p[\"ProofFunction\"], p[\"ProofLength\"], etc.).  The 4th-arg Association is built to satisfy ProofObjectQ, which causes WL to skip its auto-dispatch back to FindEquationalProof and preserve thvm's data.  ProofDataset entries are keyed by {\"Axiom\" | \"Hypothesis\" | \"SubstitutionLemma\" | \"Conclusion\", k} with Statement and Proof sub-fields.  Returns $Failed if the prover times out or queue empties without proving.";

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

(* TFindEquationalProof: runs ATP and returns a UTF8String containing
   the run summary header line followed by the serialized trace
   (PCL-shaped from atp_trace_serialize).  WL parses the lines into
   structured ProofObject entries. *)
$atpRunTracedFn  := $atpRunTracedFn  = load["thvm_wl_atp_run_traced", {{"NumericArray", "Shared"}, Integer, Integer}, "UTF8String"];

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

(* === TFindEquationalProof: ProofObject scaffold ====================== *)

(* Reverse a label->name map keyed by name into id->name map. *)
reverseEncoderState[state_Association] := <|
    "label_to_sym" -> Association[Reverse /@ Normal[state["sym"]]],
    "id_to_var"    -> Association[Reverse /@ Normal[state["var"]]]
|>

(* Parse a single trace line of the form
     "<idx> (<type>[<rest>]): lhs = rhs"
   where <type> in {axiom, orient, cp} and <rest> is "" / " from N" /
   " from N, M".  Returns Association with Index, Type, Parents, Lhs, Rhs.
   Lhs/Rhs are returned as raw text fragments to be reified later. *)
parseTraceLine[line_String] := Module[
    {toks, idx, type, parents, lhs, rhs},
    toks = StringCases[line,
        RegularExpression["^(\\d+) \\((axiom|orient|cp)( from (\\d+)(, (\\d+))?)?\\): (.+) = (.+)$"] :>
            {"$1", "$2", "$4", "$6", "$7", "$8"}];
    If[ Length[toks] == 0, Return[Missing["UnparseableTraceLine"]]];
    toks = First[toks];
    idx = ToExpression[toks[[1]]];
    type = toks[[2]];
    parents = Cases[ToExpression /@ {toks[[3]], toks[[4]]}, _Integer];
    lhs = toks[[5]];
    rhs = toks[[6]];
    <|"Index" -> idx, "Type" -> type, "Parents" -> parents,
      "Lhs" -> lhs, "Rhs" -> rhs|>
]

(* Parse the term-pretty-printer text "Cn", "Cn(args)", "x_n" back to a
   WL expression using the reverse-encoder lookup. *)
ClearAll[parsePrettyTerm];
parsePrettyTerm[text_String, rev_Association] :=
  parsePrettyTermAt[text, 1, rev][[1]]

(* Returns {expr, next_position}. *)
parsePrettyTermAt[text_String, pos_Integer, rev_Association] := Module[
    {p = pos, ch, name, idx, args, headSym, varName},
    ch = StringTake[text, {p, p}];
    If[ ch == "x" && StringLength[text] >= p + 1 && StringTake[text, {p+1, p+1}] == "_",
        (* x_<digits> -- free variable *)
        Module[{q = p + 2, digits = ""},
            While[q <= StringLength[text] && DigitQ[StringTake[text, {q, q}]],
                digits = digits <> StringTake[text, {q, q}];
                q++];
            idx = ToExpression[digits];
            varName = Lookup[rev["id_to_var"], idx, "x" <> ToString[idx]];
            {Pattern[Symbol[varName], Blank[]], q}
        ],
        If[ ch == "C",
            (* Cn or Cn(args) *)
            Module[{q = p + 1, digits = ""},
                While[q <= StringLength[text] && DigitQ[StringTake[text, {q, q}]],
                    digits = digits <> StringTake[text, {q, q}];
                    q++];
                idx = ToExpression[digits];
                name = Lookup[rev["label_to_sym"], idx, "C" <> ToString[idx]];
                headSym = Symbol[name];
                If[ q <= StringLength[text] && StringTake[text, {q, q}] == "(",
                    (* Has args -- parse comma-separated children. *)
                    q = q + 1;
                    args = {};
                    While[True,
                        Module[{r},
                            r = parsePrettyTermAt[text, q, rev];
                            AppendTo[args, r[[1]]];
                            q = r[[2]];
                            If[ q <= StringLength[text] && StringTake[text, {q, q}] == ",",
                                q = q + 1;
                                If[ q <= StringLength[text] && StringTake[text, {q, q}] == " ", q = q + 1],
                                Break[]
                            ]
                        ]
                    ];
                    If[ q <= StringLength[text] && StringTake[text, {q, q}] == ")", q = q + 1];
                    {headSym @@ args, q}
                    ,
                    (* Nullary *)
                    {headSym, q}
                ]
            ],
            (* Unknown leading character; just consume until comma/paren. *)
            Module[{q = p, frag = ""},
                While[q <= StringLength[text] &&
                      StringFreeQ[StringTake[text, {q, q}], "," | ")"],
                    frag = frag <> StringTake[text, {q, q}];
                    q++];
                {Symbol[StringTrim[frag]], q}
            ]
        ]
    ]
]

(* Use the System` symbols ProofObject's keys are built from, so
   our ProofDataset matches the FindEquationalProof shape exactly. *)
$AxiomSym             = "Axiom";
$HypothesisSym        = "Hypothesis";
$SubstitutionLemmaSym = "SubstitutionLemma";
$ConclusionSym        = "Conclusion";

(* Build the proof association expected by ProofObject's metadata
   slot.  Walks the parsed trace, mapping:
     TRACE_AXIOM   -> {Axiom, k}             (Statement = oriented form
                                              if a matching TRACE_ORIENT
                                              follows; else original)
     TRACE_ORIENT  -> not emitted; folded into the source Axiom's
                      Statement.  Builds the oriented rewrite rule list
                      used by the Conclusion synthesizer.
     TRACE_CP      -> {SubstitutionLemma, k}  (with Input/Construct)
   Last CP, OR a synthesized step from goal normalization, becomes
   {Conclusion, 1}.
   Plus {Hypothesis, 1} for the original conjecture. *)
buildProofDataset[traceRecords_List, conjecture_, rev_Association] := Module[
    {axCount = 0, lemmaCount = 0, traceIdxToKey = <||>,
     entries = <||>, conclusionKey, orientedRules = {},
     conjLhs, conjRhs, lhsNorm, rhsNorm, normRules},
    Do[
        Module[{rec = traceRecords[[i]], lhs, rhs, key, parentIdx, parentKey},
            lhs = parsePrettyTerm[rec["Lhs"], rev];
            rhs = parsePrettyTerm[rec["Rhs"], rev];
            Switch[rec["Type"],
                "axiom",
                    axCount += 1;
                    key = {$AxiomSym, axCount};
                    entries[key] = <|"Statement" -> Equal[lhs, rhs],
                                     "Proof"     -> <||>|>;
                    traceIdxToKey[rec["Index"]] = key,
                "orient",
                    (* Don't overwrite the Axiom's Statement -- thvm's
                       orient steps are fully-normalized across already-
                       oriented rules, so the orient lhs/rhs is often a
                       derived form (e.g. axiom b == c oriented after
                       a -> b becomes c == a, which doesn't match the
                       original axiom).  Keep original axiom statements
                       and just accumulate the orient rule for the
                       Conclusion synthesizer. *)
                    parentIdx = If[ Length[rec["Parents"]] >= 1,
                                    rec["Parents"][[1]], -1];
                    parentKey = Lookup[traceIdxToKey, parentIdx, None];
                    AppendTo[orientedRules, Rule[lhs, rhs]];
                    traceIdxToKey[rec["Index"]] = parentKey,
                "cp",
                    lemmaCount += 1;
                    key = {$SubstitutionLemmaSym, lemmaCount};
                    entries[key] = <|
                        "Statement" -> Equal[lhs, rhs],
                        "Proof" -> <|
                            "Input" -> If[Length[rec["Parents"]] >= 1,
                                Lookup[traceIdxToKey, rec["Parents"][[1]], None],
                                None],
                            "Construct" -> If[Length[rec["Parents"]] >= 2,
                                Lookup[traceIdxToKey, rec["Parents"][[2]], None],
                                None],
                            "Position"         -> {},
                            "Rule"             -> Rule[lhs, rhs],
                            "Orientation"      -> 1,
                            "ConstructSide"    -> 1,
                            "InputOrientation" -> 1,
                            "Side"             -> 1,
                            "OutputExpression" -> Equal[lhs, rhs],
                            "Source"           -> "cp"
                        |>
                    |>;
                    AppendTo[orientedRules, Rule[lhs, rhs]];
                    traceIdxToKey[rec["Index"]] = key
            ]
        ],
        {i, Length[traceRecords]}
    ];
    (* Insert Hypothesis. *)
    entries[{$HypothesisSym, 1}] = <|"Statement" -> Equal @@ conjecture,
                                      "Proof"     -> <||>|>;
    (* Synthesize Conclusion: apply oriented rewrite rules to both
       sides of the conjecture; when both normalize to the same
       expression, the closing tautology is `lhs_norm == lhs_norm`.
       Mirrors what WL's ProofObject Conclusion entry looks like
       (e.g., c == c after rewriting a == c with the rules). *)
    {conjLhs, conjRhs} = conjecture;
    normRules = orientedRules;
    lhsNorm = conjLhs //. normRules;
    rhsNorm = conjRhs //. normRules;
    Which[
        lhsNorm === rhsNorm,
            (* Found a tautology -- synthesize Conclusion.
               HoldForm prevents Equal[a, a] from auto-evaluating to
               True; With substitutes the actual normalized value
               into the held form (else Module's local symbol leaks). *)
            With[{n = lhsNorm,
                  constructKey = If[lemmaCount > 0,
                                     {$SubstitutionLemmaSym, lemmaCount},
                                     {$AxiomSym, axCount}],
                  ruleForm = Rule @@ conjecture},
                entries[{$ConclusionSym, 1}] = <|
                    "Statement" -> HoldForm[Equal[n, n]],
                    "Proof" -> <|
                        "Input"            -> {$HypothesisSym, 1},
                        "Construct"        -> constructKey,
                        "Position"         -> {},
                        "Rule"             -> ruleForm,
                        "Orientation"      -> 1,
                        "ConstructSide"    -> 1,
                        "InputOrientation" -> 1,
                        "Side"             -> 1,
                        "OutputExpression" -> HoldForm[Equal[n, n]],
                        "Source"           -> "goal_close"
                    |>
                |>
            ],
        lemmaCount > 0,
            (* Fall back to promoting the last lemma. *)
            conclusionKey = {$SubstitutionLemmaSym, lemmaCount};
            entries[{$ConclusionSym, 1}] = entries[conclusionKey];
            KeyDropFrom[entries, conclusionKey]
    ];
    SortBy[Normal[entries], $ProofKeyOrder[First[#]] &]
]

$ProofKeyOrder[{"Axiom",             k_}] := {1, k}
$ProofKeyOrder[{"Hypothesis",        k_}] := {2, k}
$ProofKeyOrder[{"SubstitutionLemma", k_}] := {3, k}
$ProofKeyOrder[{"Conclusion",        k_}] := {4, k}
$ProofKeyOrder[_]                          := {5, 0}

(* Note: WL's built-in ProofObject already provides all the access
   methods (ProofGraph, ProofDataset, ProofLength, etc.) when the
   4th-arg Association satisfies ProofObjectQ -- which our dataset
   does, since it's a List of Rules with {_String, _Integer} keys.
   No custom MakeBoxes / lookup needed; WL handles it. *)

SetAttributes[TFindEquationalProof, HoldAll];
TFindEquationalProof[conjecture_, axioms_,
    OptionsPattern[{MaxSteps -> 64}]] :=
  Catch[
    Module[
        {state, axTerms, ax, lhs, rhs, lhsRes, rhsRes, cj,
         goalLhs, goalRhs, packed, maxLab, rawText, lines, header,
         headerData, traceLines, records, rev, dataset, conjPair,
         varNames, constNames},
        ensureInit[];
        state = encodeAtpTermInit[];
        axTerms = {};
        If[ Head[Unevaluated[axioms]] =!= List,
            Throw[Failure["TATPParseError",
                <|"Reason" -> "axioms must be a List"|>], "TATPError"]];
        Do[
            ax = Extract[Hold[axioms], {1, i}, HoldComplete];
            If[ !MatchQ[ax, HoldComplete[Equal[_, _]]],
                Throw[Failure["TATPParseError",
                    <|"Axiom" -> i, "Reason" -> "expected `lhs == rhs`"|>],
                    "TATPError"]];
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
        If[ !MatchQ[cj, HoldComplete[Equal[_, _]]],
            Throw[Failure["TATPParseError",
                <|"Reason" -> "conjecture must be `lhs == rhs`"|>],
                "TATPError"]];
        lhsRes = encodeAtpTerm[Extract[cj, {1, 1}, HoldComplete][[1]], state];
        goalLhs = lhsRes[[1]];
        state = lhsRes[[2]];
        rhsRes = encodeAtpTerm[Extract[cj, {1, 2}, HoldComplete][[1]], state];
        goalRhs = rhsRes[[1]];
        state = rhsRes[[2]];
        conjPair = {Extract[cj, {1, 1}], Extract[cj, {1, 2}]};
        packed = NumericArray[
            Join[{Length[Unevaluated[axioms]]}, axTerms, {goalLhs, goalRhs}],
            "Integer64"];
        maxLab = state["next_lab"];

        rawText = $atpRunTracedFn[packed, OptionValue[MaxSteps], maxLab];
        If[ !StringQ[rawText],
            Throw[Failure["TATPInternalError",
                <|"Reason" -> "FFI returned non-string"|>], "TATPError"]];

        lines = StringSplit[rawText, "\n"];
        If[ Length[lines] == 0, Return[$Failed]];
        header = First[lines];
        traceLines = Select[Rest[lines], StringLength[#] > 0 &];

        (* Parse "META status=N n_rules=N n_trace=N n_cps=N" *)
        headerData = Association[
            (StringSplit[#, "="] & /@
                Select[StringSplit[StringDelete[header, "META "]],
                       StringContainsQ[#, "="] &]) /.
            {k_String, v_String} :> (k -> ToExpression[v])];

        If[ headerData["status"] =!= 1,
            Return[$Failed]];

        rev = reverseEncoderState[state];
        records = parseTraceLine /@ traceLines;
        records = Select[records, AssociationQ];

        dataset = buildProofDataset[records, conjPair, rev];

        varNames = Symbol /@ Values[rev["id_to_var"]];
        constNames = Complement[
            Symbol /@ Values[rev["label_to_sym"]], varNames];

        Module[{conclEq = Equal @@ conjPair,
                axEq    = Equal @@@ Hold[axioms][[1]]},
            (* Construct a real ProofObject.  The 4th-arg Association
               must satisfy ProofObjectQ (Variables -> {_Symbol...},
               Constants -> {_?AtomQ...}, Proof -> List of Rules with
               {_String, _Integer} keys); when it does, WL skips the
               auto-dispatch back to FindEquationalProof at line 599 of
               EquationalProof.m, and our metadata is preserved. *)
            ProofObject[
                "EquationalLogic",
                conclEq,
                axEq,
                <|"Variables" -> varNames,
                  "Constants" -> constNames,
                  "Proof"     -> dataset|>
            ]
        ]
    ],
    "TATPError"]

End[];
EndPackage[];
