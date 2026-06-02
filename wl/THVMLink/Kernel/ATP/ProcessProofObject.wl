(* thvm/atp -- SZS derivation -> thvm-shaped ProofObject builder.

   Build a thvm `ProofObject["EquationalLogic", goal, axioms, ds]`
   from a parsed SZS derivation (the Association list returned by
   `Wolfram`Parser`TPTPImport[..., "SZS"]`).  ATP-AGNOSTIC: works
   for any prover that emits SZS-framed fof-with-inference output
   (Vampire, E, iProver, Twee with --tstp, Otter, ...).  The
   per-prover wrappers (TVampireProof, TEProverProof, ...) just
   produce the parsed derivation; this module turns it into the
   canonical WL ProofObject shape.

   With both an internal preset and an external CLI returning the
   same ProofObject shape, downstream code (the proof shape
   comparator, the WL ProofObject UI, the ProofFunction verifier)
   sees a uniform interface.

   Limitations of the produced ProofObject:
   * Statement fields are the parsed SZS formulas as-given -- they
     do NOT auto-translate to the WL operator names the original
     theory uses.  For shape comparison this is fine; the
     ProofFunction verifier needs a theory-specific symbol-name
     reverse-translator (a follow-up).
   * Proof-field metadata (Construct/Rule/Orientation/Position) is
     STUBBED -- only Construct/MatchingConstruct/Input parent links
     are populated, so the shape comparator sees the right number
     of parents but the verifier path is unsupported.
   * Rule names that aren't in `$SZSRuleToConstruct` fall through
     to "SubstitutionLemma" (a benign default; tweak the table to
     add a prover's idiosyncratic inference). *)

BeginPackage["THVMLink`ATP`"]

TSZSDerivationToProofObject::usage =
    "TSZSDerivationToProofObject[derivation_List] builds a thvm-shaped " <>
    "ProofObject[...] from a parsed SZS derivation (the kind returned " <>
    "by Wolfram`Parser`TPTPImport[..., \"SZS\"]['Derivation']).  Works " <>
    "for any ATP that emits SZS-framed fof+inference output (Vampire, " <>
    "E, iProver, Twee --tstp, Otter, ...).  See $SZSRuleToConstruct " <>
    "for the SZS-rule -> thvm-construct mapping table."

$SZSRuleToConstruct::usage =
    "$SZSRuleToConstruct is an Association mapping SZS inference-rule " <>
    "names ('superposition', 'forward_demodulation', etc.) to thvm " <>
    "ProofObject construct types ('CriticalPairLemma', " <>
    "'SubstitutionLemma', etc.).  Edit this Association to support a " <>
    "prover's idiosyncratic inference rule.  Unmapped rules fall " <>
    "through to 'SubstitutionLemma'."

TVampireProofObject::usage =
    "TVampireProofObject[\"Theory\", \"thm\", opts] runs Vampire CLI " <>
    "via TVampireProof then converts the result through " <>
    "TSZSDerivationToProofObject to return a thvm-shaped ProofObject.  " <>
    "Used as the Method -> \"VampireProcess\" dispatch target in " <>
    "TFindProof so two ProofObjects from differing Methods (internal " <>
    "preset vs external CLI) can be compared structurally."

TWaldmeisterProofObject::usage =
    "TWaldmeisterProofObject[\"path/to/file.pr\", opts] runs the local " <>
    "wmcli binary via TWaldmeisterProof and converts the proof " <>
    "protocol via TSZSDerivationToProofObject to return a thvm-shaped " <>
    "proof Association.  Used as the Method -> \"WaldmeisterProcess\" " <>
    "dispatch target in TFindProof.  Path form only -- the two-arg " <>
    "(Theory, thm) form needs a TPTP -> .pr converter (deferred)."

TTweeProofObject::usage =
    "TTweeProofObject[\"Theory\", \"thm\", opts] runs Twee CLI via " <>
    "TTweeProof.  Twee's --tstp output is SZS-FRAMED but the proof " <>
    "body is Twee's own human-readable equation chain (not TPTP fof " <>
    "inferences), so we return the lemma-list shape directly rather " <>
    "than via TSZSDerivationToProofObject -- the dataset is keyed by " <>
    "{\"Axiom\", n} / {\"Lemma\", n} only, with no per-step " <>
    "construct-class metadata.  Used as Method -> \"TweeProcess\" in " <>
    "TFindProof."

Begin["`Private`"]

(* TPTP-symbol -> WL-operator reverse map.  Mirrors the encoder
   table in tools/vampire/export_all.wls's $opNames (the script
   that generated the TPTP problem files in
   tools/baselines/vampire_tptp/).  Keep in sync with that table.

   For operators NOT in this map, parsed TPTP heads stay as
   String-headed compounds ("op_overtilde"[...], etc.) which the
   shape comparator already handles. *)
$tptpToWlOp = <|
    "and"      -> CircleTimes,
    "or"       -> CirclePlus,
    "not"      -> OverBar,
    "nand_op"  -> CircleMinus,
    "fop"      -> CircleDot,
    "diamond"  -> Diamond,
    "star"     -> Star,
    "wedge"    -> Wedge,
    "vee"      -> Vee,
    "circ"     -> SmallCircle,
    "mul"      -> Times,
    "add"      -> Plus,
    "equiv"    -> Equivalent,
    "implies"  -> Implies,
    "lnot"     -> Not,
    "land"     -> And,
    "lor"      -> Or,
    "nand"     -> Nand,
    "nor"      -> Nor
|>

(* Walk a parsed TPTP expression and replace String-headed
   compounds with their WL-symbol heads where the map knows them.
   `op_overtilde` etc. (unknown) stay String-headed. *)
reverseEncodeFormula[expr_] := expr //. {
    h_String[args___] /; KeyExistsQ[$tptpToWlOp, h] :>
        $tptpToWlOp[h][args]
}

(* Parse a single SZS formula-body string into a WL expression by
   wrapping it in a fof(p, axiom, ...).  on the way out, the body
   parses to WL form via TPTPImport's regular (non-SZS) mode; the
   top-level ForAll is stripped by TPTPImport. *)
parseFormulaBody[body_String] := Block[
    {wrapped, parsed},
    wrapped = "fof(p, axiom, " <> body <> ").";
    parsed = Quiet @ Check[
        Wolfram`Parser`TPTPImport[wrapped],
        $Failed
    ];
    Which[
        AssociationQ[parsed] && Length[parsed["Axioms"]] > 0,
            reverseEncodeFormula @ First @ parsed["Axioms"],
        AssociationQ[parsed] && parsed["Conjecture"] =!= Missing[],
            reverseEncodeFormula @ parsed["Conjecture"],
        True,
            body  (* fall back to raw string -- comparator still works *)
    ]
]
parseFormulaBody[other_] := other

(* The canonical SZS-rule -> thvm-construct mapping.  Editable as a
   public Association.  Defaults cover the standard saturation-prover
   inference vocabulary; unmapped rules default to SubstitutionLemma
   in `constructTypeOf`. *)
$SZSRuleToConstruct = <|
    (* file = input axiom or negated conjecture; caller's
       assignConstructKeys must distinguish via Role. *)
    "file"                              -> "Axiom",
    "superposition"                     -> "CriticalPairLemma",
    "forward_demodulation"              -> "SubstitutionLemma",
    "backward_demodulation"             -> "SubstitutionLemma",
    "demodulation"                      -> "SubstitutionLemma",
    "rewrite"                           -> "SubstitutionLemma",
    "subsumption_resolution"            -> "Conclusion",
    "forward_subsumption_resolution"    -> "Conclusion",
    "backward_subsumption_resolution"   -> "Conclusion",
    "trivial_inequality_removal"        -> "Conclusion",
    "equality_resolution"               -> "SubstitutionLemma",
    "equality_factoring"                -> "SubstitutionLemma",
    "factoring"                         -> "SubstitutionLemma",
    "resolution"                        -> "CriticalPairLemma",
    "binary_resolution"                 -> "CriticalPairLemma",
    "paramodulation"                    -> "CriticalPairLemma"
|>

constructTypeOf[rule_String] :=
    Lookup[$SZSRuleToConstruct, rule, "SubstitutionLemma"]

(* Inference rules that are PURE BOOKKEEPING -- they don't carry a
   semantic step the proof reconstructor should emit.  Examples:
   `orient` (turning an equation into a directed rule -- the rule
   is implicit in any later cp/red that uses it), `reorient_equations`
   (Vampire's parse-time equation-flip), bare axiom renames.

   Drop these from the derivation BEFORE assigning construct keys.
   References to a dropped step transitively resolve to the step's
   own first parent (or the source axiom if there is no parent).
   This is the generalisation of foldReorients in Vampire.wl. *)
$BookkeepingRules = {
    "orient",
    "reorient_equations",
    "equation_copy"  (* WM's `tes-eqn : ... : N` renaming of equation N *)
}

(* Build a name->name alias from every dropped step to its (first)
   parent.  Resolve transitively. *)
foldBookkeeping[derivation_List] := Block[
    {aliases, resolve},
    aliases = Association @ Cases[
        derivation,
        s_Association /; MemberQ[$BookkeepingRules, s["Rule"]]
            && ListQ[s["Parents"]] && Length[s["Parents"]] >= 1 :>
            (s["Name"] -> First[s["Parents"]])
    ];
    resolve[n_] := If[KeyExistsQ[aliases, n], resolve[aliases[n]], n];
    (* Rewrite Parents of non-dropped steps via the alias map. *)
    Map[
        s |-> If[
            MemberQ[$BookkeepingRules, s["Rule"]],
            Nothing,
            Append[s, "Parents" -> Map[resolve, Lookup[s, "Parents", {}]]]
        ],
        derivation
    ] /. Nothing -> Sequence[]
]

isNegatedConjectureQ[step_Association] :=
    step["Rule"] === "file" && step["Role"] === "negated_conjecture"

(* Walk derivation in order, assign each step a thvm-shaped key
   {ConstructType, n} -- sequential per type.  Returns Association
   from SZS-step-name (f1, f2, ...) to the assigned key. *)
assignConstructKeys[derivation_List] := Block[
    {nameToKey = <||>, perTypeCount = <||>, key, t},
    Scan[
        step |-> (
            t = If[isNegatedConjectureQ[step], "Hypothesis",
                constructTypeOf[step["Rule"]]];
            perTypeCount[t] = Lookup[perTypeCount, t, 0] + 1;
            nameToKey[step["Name"]] = {t, perTypeCount[t]}
        ),
        derivation
    ];
    nameToKey
]

(* Build the per-entry Proof-field Association.  Axioms +
   hypotheses get an empty <||>; everything else routes the first
   parent into Construct + the second into either MatchingConstruct
   (for CP-shaped inferences like superposition) or Input (for
   rewrite-shaped inferences like forward_demodulation). *)
proofFieldFor[step_Association, nameToKey_Association] := Block[
    {parents, parentKeys, t = constructTypeOf[step["Rule"]]},
    parents = Lookup[step, "Parents", {}];
    parentKeys = Map[
        Lookup[nameToKey, #, Missing["NotFound"]] &,
        parents
    ];
    Which[
        step["Rule"] === "file" || Length[parentKeys] === 0,
            <||>,
        Length[parentKeys] === 1,
            <|"Construct" -> parentKeys[[1]]|>,
        True,
            If[ t === "SubstitutionLemma" || t === "Conclusion",
                <|"Input" -> parentKeys[[1]], "Construct" -> parentKeys[[2]]|>,
                <|"Construct" -> parentKeys[[1]],
                  "MatchingConstruct" -> parentKeys[[2]]|>
            ]
    ]
]

buildDatasetFromDerivation[derivation_List, parseFormulasQ_:False] := Block[
    {folded, nameToKey, entries, stmtFn},
    (* Drop bookkeeping steps (`orient`, `reorient_equations`)
       BEFORE construct-key assignment.  Aliases route any later
       reference through to the step's first real parent.  See
       $BookkeepingRules + foldBookkeeping above. *)
    folded = foldBookkeeping[derivation];
    nameToKey = assignConstructKeys[folded];
    (* Per-formula wrap-and-parse is SLOW (TPTPImport runs the
       full EBNF parser per call -- 5s/formula on AbelianGroup
       cases, dominating wall on a multi-step proof).  Default to
       raw String statements; opt-in to parsed-WL mode via
       parseFormulasQ when full ProofObject identity / property
       inspection is needed. *)
    stmtFn = If[parseFormulasQ, parseFormulaBody, Identity];
    entries = Map[
        step |-> With[
            {
                key        = nameToKey[step["Name"]],
                stmt       = stmtFn[step["Formula"]],
                proofField = proofFieldFor[step, nameToKey]
            },
            key -> <|"Statement" -> stmt, "Proof" -> proofField|>
        ],
        folded
    ];
    Association @@ entries
]

End[]

Options[TSZSDerivationToProofObject] = {"ParseFormulas" -> False}

(* Public: build a thvm-shaped proof Association from a parsed SZS
   derivation list.  ATP-agnostic -- works for Vampire's
   `--proof tptp`, E's `--proof-object`, Twee's `--tstp`, iProver,
   Otter, anyone that emits SZS-framed fof+inference output.

   Returns an Association with the same keys the internal
   ProofObject exposes via property access, so a comparator can
   dispatch on shape without caring whether the input came from
   the internal saturator or an external CLI.

   Shape:
       <|
         "Backend"     -> "SZS",            (* tag for routing *)
         "Status"      -> "Proved",
         "Goal"        -> the negated_conjecture formula,
         "Axioms"      -> {axiom_formula, ...},
         "ProofDataset"-> Association: {Type, n} -> <|Statement, Proof|>,
         "ProofLength" -> #non-axiom inferences,
         "RuleHistogram" -> KeySort @ Counts[rule names]
       |>

   NOT a literal `ProofObject[...]`: WL's ProofObject auto-validates
   axioms against its theory schemas and downgrades to
   FindEquationalProof when the formulas are raw SZS String-headed
   text (TPTPImport's SZS mode does NOT parse formulas into WL
   expressions, only the outer wrapper).  A future parser pass over
   the formula bodies + theory-specific symbol-name reverse
   translation can lift this back into a real ProofObject. *)
TSZSDerivationToProofObject[derivation_List, opts : OptionsPattern[]] := Block[
    {ds, axiomEntries, hypothesisEntries, axioms, goal, hist,
        parseFlag = TrueQ @ OptionValue["ParseFormulas"]},
    ds = THVMLink`ATP`Private`buildDatasetFromDerivation[derivation, parseFlag];
    axiomEntries      = KeySelect[ds, MatchQ[#, {"Axiom", _}] &];
    hypothesisEntries = KeySelect[ds, MatchQ[#, {"Hypothesis", _}] &];
    axioms = Values @ axiomEntries /. e_Association :> e["Statement"];
    goal = If[
        Length[hypothesisEntries] > 0,
        First[Values @ hypothesisEntries]["Statement"],
        Missing["NoHypothesis"]
    ];
    hist = KeySort @ Counts[Cases[
        derivation,
        s_Association /; s["Rule"] =!= "file" :> s["Rule"]
    ]];
    <|
        "Backend"      -> "SZS",
        "Status"       -> "Proved",
        "Goal"         -> goal,
        "Axioms"       -> axioms,
        "ProofDataset" -> ds,
        "ProofLength"  -> Length[derivation] - Length[axiomEntries],
        "RuleHistogram" -> hist
    |>
]

(* Hand-enumerated: Vampire.wl loads AFTER ProcessProofObject.wl in
   the alphabetical autoloader order, so `Options[TVampireProof]`
   evaluates to {} at file-load time.  Keep this list in sync with
   Options[TVampireProof] in Vampire.wl + the "ParseFormulas" toggle. *)
Options[TVampireProofObject] = {
    TimeConstraint  -> 30,
    "Mode"          -> "casc",
    "Binary"        -> Automatic,
    "ParseFormulas" -> False
}

(* Vampire-specific wrapper: chain TVampireProof + the generic
   SZS-to-ProofObject builder.  Future TEProverProofObject /
   TIProverProofObject etc. follow the same shape. *)
TVampireProofObject[theory_String, thm_String, opts : OptionsPattern[]] := Block[
    {vampR = TVampireProof[theory, thm,
            FilterRules[{opts}, Options[TVampireProof]]],
        parseOpt = "ParseFormulas" -> OptionValue["ParseFormulas"]},
    If[ vampR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool"     -> "Vampire",
            "Status"   -> vampR["Status"],
            "Seconds"  -> vampR["Seconds"],
            "Strategy" -> vampR["Strategy"]
        |>],
        TSZSDerivationToProofObject[vampR["Inferences"], parseOpt]
    ]
]

(* Waldmeister wrapper: chain TWaldmeisterProof + the generic
   SZS-shaped builder.  WM's proof protocol parses into the same
   inference-record Association shape as Vampire's SZS, so the
   exact same TSZSDerivationToProofObject path lifts it.  Only
   supports the PATH form (.pr file) for now; the two-arg
   (Theory, thm) form needs a TPTP->.pr converter, deferred. *)
Options[TWaldmeisterProofObject] = {
    TimeConstraint  -> 30,
    "Binary"        -> Automatic,
    "MathlinkPath"  -> Automatic,
    "ParseFormulas" -> False
}

TWaldmeisterProofObject[problemFile_String, opts : OptionsPattern[]] /;
        FileExtension[problemFile] === "pr" :=
    Block[
        {wmR = TWaldmeisterProof[problemFile,
                FilterRules[{opts},
                    {TimeConstraint, "Binary", "MathlinkPath"}]],
            parseOpt = "ParseFormulas" -> OptionValue["ParseFormulas"]},
        If[ wmR["Status"] =!= "Proved",
            Failure["ExternalNoProof", <|
                "Tool"     -> "Waldmeister",
                "Status"   -> wmR["Status"],
                "Seconds"  -> wmR["Seconds"]
            |>],
            TSZSDerivationToProofObject[wmR["Inferences"], parseOpt]
        ]
    ]

(* Twee wrapper: Twee's --tstp proof body is not TPTP fof, so we
   build a coarser dataset (Axioms + Lemmas with no inference
   metadata) that the shape comparator still reads. *)
Options[TTweeProofObject] = {
    TimeConstraint -> 30,
    "Binary"       -> Automatic
}

TTweeProofObject[theory_String, thm_String, opts : OptionsPattern[]] := Block[
    {tR = TTweeProof[theory, thm,
            FilterRules[{opts}, {TimeConstraint, "Binary"}]],
        ds, axs, lems},
    If[ tR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool"    -> "Twee",
            "Status"  -> tR["Status"],
            "Seconds" -> tR["Seconds"]
        |>],
        axs  = tR["Axioms"];
        lems = tR["Lemmas"];
        ds = Association @@ Join[
            Table[
                {"Axiom", i} -> <|
                    "Statement" -> axs[[i]]["Statement"],
                    "Proof" -> <||>
                |>,
                {i, Length[axs]}
            ],
            Table[
                {"Lemma", i} -> <|
                    "Statement" -> lems[[i]]["Statement"],
                    "Proof" -> <||>
                |>,
                {i, Length[lems]}
            ]
        ];
        <|
            "Backend"       -> "Twee-TSTP",
            "Status"        -> "Proved",
            "Goal"          -> Missing["NotEmittedByTwee"],
            "Axioms"        -> #["Statement"] & /@ axs,
            "ProofDataset"  -> ds,
            "ProofLength"   -> tR["ProofLength"],
            "RuleHistogram" -> <|"twee-rewrite" -> Length[lems]|>
        |>
    ]
]

EndPackage[]
