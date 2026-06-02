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

Begin["`Private`"]

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

buildDatasetFromDerivation[derivation_List] := Block[
    {nameToKey, entries},
    nameToKey = assignConstructKeys[derivation];
    entries = Map[
        step |-> With[
            {
                key        = nameToKey[step["Name"]],
                stmt       = step["Formula"],
                proofField = proofFieldFor[step, nameToKey]
            },
            key -> <|"Statement" -> stmt, "Proof" -> proofField|>
        ],
        derivation
    ];
    Association @@ entries
]

End[]

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
TSZSDerivationToProofObject[derivation_List] := Block[
    {ds, axiomEntries, hypothesisEntries, axioms, goal, hist},
    ds = THVMLink`ATP`Private`buildDatasetFromDerivation[derivation];
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

(* Vampire-specific wrapper: chain TVampireProof + the generic
   SZS-to-ProofObject builder.  Future TEProverProofObject /
   TIProverProofObject etc. follow the same shape. *)
TVampireProofObject[theory_String, thm_String, opts : OptionsPattern[]] := Block[
    {vampR = TVampireProof[theory, thm, opts]},
    If[ vampR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool"     -> "Vampire",
            "Status"   -> vampR["Status"],
            "Seconds"  -> vampR["Seconds"],
            "Strategy" -> vampR["Strategy"]
        |>],
        TSZSDerivationToProofObject[vampR["Inferences"]]
    ]
]

EndPackage[]
