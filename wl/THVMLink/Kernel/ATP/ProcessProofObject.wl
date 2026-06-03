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

BeginPackage["THVMLink`ATP`", {"Wolfram`Parser`"}]

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

TEproverProofObject::usage =
    "TEproverProofObject[\"Theory\", \"thm\", opts] runs the E prover " <>
    "CLI on the canonical TPTP problem file and lifts the SZS proof " <>
    "into the shared thvm Association shape via " <>
    "TSZSDerivationToProofObject.  E's --proof-object --tstp-format " <>
    "emits the same SZS-framed fof+inference DAG as Vampire's " <>
    "--proof tptp, so the lift path is shared.  Options: TimeConstraint " <>
    "(default 30), Binary (default Automatic), ParseFormulas " <>
    "(default False), LiftToProofObject (default False -- when True, " <>
    "wraps the Association into a literal ProofObject[...] head)."

Begin["`Private`"]

(* TPTP-symbol -> WL-operator reverse map.  Mirrors the encoder
   table in tools/vampire/export_all.wls's $opNames (the script
   that generated the TPTP problem files in
   tools/baselines/vampire_tptp/).  Keep in sync with that table.

   For operators NOT in this map, parsed TPTP heads stay as
   String-headed compounds ("op_overtilde"[...], etc.) which the
   shape comparator already handles. *)
$tptpToWlOp = <|
    "and"          -> CircleTimes,
    "or"           -> CirclePlus,
    "not"          -> OverBar,
    "nand_op"      -> CircleMinus,
    "fop"          -> CircleDot,
    "diamond"      -> Diamond,
    "star"         -> Star,
    "wedge"        -> Wedge,
    "vee"          -> Vee,
    "circ"         -> SmallCircle,
    "mul"          -> Times,
    "add"          -> Plus,
    "equiv"        -> Equivalent,
    "implies"      -> Implies,
    "lnot"         -> Not,
    "land"         -> And,
    "lor"          -> Or,
    "nand"         -> Nand,
    "nor"          -> Nor,
    (* Unicode/diacritic operators emitted as op_<name> by
       tools/baselines/tptp_to_pr.wls (and then sanitized through
       TPTPImport's lowercasing path to opovertilde / opoverbar /
       etc.).  Map both spellings so reverse-encode lands on the
       WL symbol regardless of casing. *)
    "op_overtilde" -> OverTilde,
    "opovertilde"  -> OverTilde,
    "op_overbar"   -> OverBar,
    "opoverbar"    -> OverBar,
    "op_circle"    -> SmallCircle,
    "opcircle"     -> SmallCircle
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
   top-level ForAll is stripped by TPTPImport.

   Pre-process: TPTPImport's grammar rejects nullary functor calls
   written `name()` (only `name` is accepted for arity-zero), but
   Vampire/WM SZS output writes skolem constants as `skC1()` etc.
   Strip the trailing empty parens before parsing. *)
parseFormulaBody[body_String] := Block[
    {cleaned, wrapped, parsed},
    cleaned = StringReplace[body,
        RegularExpression["([a-zA-Z_][a-zA-Z_0-9]*)\\(\\)"] :> "$1"];
    wrapped = "fof(p, axiom, " <> cleaned <> ").";
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

(* ---- Single-rewrite reconstruction ------------------------------
   Given Input.eq, Construct.eq, Step.eq (all `Inactive[Equal][lhs, rhs]`
   shaped, with Construct's vars as Pattern[v, Blank[]]), find which
   (Side ∈ {1,2}, ConstructSide ∈ {1,2}, Position ∈ Positions(Input[Side]))
   tuple yields Step.eq when Construct is used as a rewrite rule.

   Returns an Association with Side / ConstructSide / Position /
   Orientation / Rule keys, or $Failed if no fit.  Handles only
   SubstitutionLemma + Conclusion (single-rewrite) steps; CriticalPairLemma
   superposition reconstruction is deferred. *)
equalSides[Inactive[Equal][a_, b_]] := {a, b};
equalSides[Equal[a_, b_]]           := {a, b};
equalSides[h_HoldForm]              := equalSides[ReleaseHold[h]];
equalSides[_]                       := $Failed;

(* Strip Pattern[v, _] -> v so the matched bindings (decorating
   the LHS) substitute correctly into the RHS. *)
stripPatternHeads[expr_] := expr //.
    Verbatim[Pattern][v_, _] :> v;

(* Hand-construct a Rule that matches the preset's stored shape:
   LHS is Pattern-wrapped (vars rendered as `name_`); RHS uses
   the same Symbol names bare so bound vars substitute correctly. *)
mkRule[lhsBase_, rhsBase_, vars_List : {}] := With[
    {lh = withVariablePatterns[stripPatternHeads[lhsBase], vars],
     rh = stripPatternHeads[rhsBase]},
    Rule @@ {lh, rh}];

(* For Replace -- defers RHS evaluation past binding capture. *)
mkRewriteRule[lhsBase_, rhsBase_, vars_List : {}] := With[
    {lh = withVariablePatterns[stripPatternHeads[lhsBase], vars],
     rh = stripPatternHeads[rhsBase]},
    RuleDelayed @@ {lh, rh}];

reconstructSingleRewrite[inputEq_, constructEq_, stepEq_,
        varSet_List : {}] := Block[
    {iSides, cSides, sSides, candidates, hit, closureMode},
    iSides = equalSides[inputEq];
    cSides = equalSides[constructEq];
    (* True closure: Step.Statement collapses to `True` because the
       rewrite makes both sides of Input syntactically equal.
       Accept candidates whose rewritten equation has lhs === rhs. *)
    closureMode = stepEq === True;
    sSides = If[ closureMode, Missing[], equalSides[stepEq]];
    If[ iSides === $Failed || cSides === $Failed
        || (! closureMode && sSides === $Failed), Return[$Failed]];
    (* Statements are stored with BARE variables in the dataset; we
       need Pattern[v, Blank[]] form for MatchQ to fire correctly. *)
    cSides = withVariablePatterns[#, varSet] & /@ cSides;
    candidates = Flatten[Table[
        Block[{rule, lhsPat, allPositions},
            lhsPat = cSides[[cs]];  (* already Pattern-wrapped earlier *)
            rule = mkRewriteRule[lhsPat, cSides[[3 - cs]], varSet];
            allPositions = Prepend[
                Position[iSides[[s]], _], {}];
            Table[
                Block[{subterm, newSubterm, newSide, rewrittenEq},
                    subterm = If[ pos === {}, iSides[[s]],
                        Extract[iSides[[s]], pos]];
                    If[ ! MatchQ[subterm, lhsPat], Nothing,
                        newSubterm = Replace[subterm, rule];
                        newSide = If[ pos === {}, newSubterm,
                            ReplacePart[iSides[[s]],
                                pos -> newSubterm]];
                        rewrittenEq = ReplacePart[iSides,
                            s -> newSide];
                        {s, cs, pos, rewrittenEq}]],
                {pos, allPositions}]],
        {s, 2}, {cs, 2}], 2];
    hit = SelectFirst[candidates,
        If[ closureMode,
            #[[4, 1]] === #[[4, 2]],
            (Sort @ #[[4]]) === (Sort @ sSides)] &,
        Missing["NoFit"]];
    If[ MissingQ[hit], $Failed,
        <|"Side" -> hit[[1]],
          "ConstructSide" -> hit[[2]],
          "Position" -> hit[[3]],
          "Orientation" -> If[hit[[2]] === 1, 1, -1],
          "Rule" -> mkRule[cSides[[hit[[2]]]],
              cSides[[3 - hit[[2]]]], varSet]|>]
];

(* ---- Superposition (CriticalPairLemma) reconstruction -----------
   A CPL inference superposes two rules: the Construct rule's LHS
   contains a non-variable subterm at some Position, and that
   subterm unifies with the MatchingConstruct rule's LHS.  The
   resulting critical pair is:
     σ(Construct.RHS)  ==  σ(Construct.LHS[Position <- MatchingConstruct.RHS])
   where σ is the most-general unifier.

   Enumerates (Side, Orientation, MatchingSide, MatchingOrientation,
   Position) and runs cplUnify on each candidate.  Returns metadata
   with Side / ConstructSide / Orientation / Subpattern /
   MatchingConstruct (passed through) / MatchingOrientation /
   MatchingSide / Position / Rule / MatchingRule keys, or $Failed. *)
applySubstitution[expr_, sub_Association] := expr //. Normal[sub];

(* Strip Pattern[v, Blank[]] -> v from an expression so we can
   walk it as a plain term-tree for subpattern enumeration. *)
unpatternize[expr_] := expr //.
    Verbatim[Pattern][v_, _] :> v;

(* A position is "non-variable" if the subterm at that position
   isn't a Pattern-headed leaf -- superposition rules require the
   matched subterm to be a non-variable position. *)
nonVarPositions[expr_, varSet_List] := Block[
    {plain = unpatternize[expr], positions},
    (* Heads -> False so the head-slot {0} doesn't appear (rewriting
       at the head of a compound term is undefined for superposition). *)
    positions = Position[plain, _, Heads -> False];
    Select[positions,
        ! MemberQ[varSet, Extract[plain, #]] &]
];

reconstructSuperposition[
        constructEq_, matchingEq_, stepEq_, varSet_List] := Block[
    {cSides, mSides, sSides, candidates, hit, freshen, freshVars,
     allVars},
    cSides = equalSides[constructEq];
    mSides = equalSides[matchingEq];
    sSides = equalSides[stepEq];
    If[ cSides === $Failed || mSides === $Failed
        || sSides === $Failed, Return[$Failed]];
    cSides = unpatternize /@ cSides;
    mSides = unpatternize /@ mSides;
    sSides = unpatternize /@ sSides;
    (* Freshen MatchingConstruct's variables so they don't collide
       with Construct's identical-name vars during unification. *)
    freshen = AssociationThread[varSet,
        Table[Unique["cplR"], {Length[varSet]}]];
    mSides = mSides /. freshen;
    freshVars = Values[freshen];
    allVars = Join[varSet, freshVars];
    (* Two host/applied orientations -- the SZS labeling might map
       Construct->host + MatchingConstruct->applied OR the swap.
       Try both: hostSides = either cSides or mSides; appliedSides
       = the other.  Record `swap` so the caller knows which role
       Construct ended up playing. *)
    candidates = Flatten[Table[
        Block[{hostSides, appliedSides, ruleLhs1, ruleRhs1,
               ruleLhs2, ruleRhs2, positions, sub,
               cpLhs, cpRhs},
            If[ swap === 0,
                hostSides = cSides; appliedSides = mSides,
                hostSides = mSides; appliedSides = cSides];
            ruleLhs1 = If[ o1 === 1, hostSides[[s1]],
                hostSides[[3 - s1]]];
            ruleRhs1 = If[ o1 === 1, hostSides[[3 - s1]],
                hostSides[[s1]]];
            ruleLhs2 = If[ o2 === 1, appliedSides[[s2]],
                appliedSides[[3 - s2]]];
            ruleRhs2 = If[ o2 === 1, appliedSides[[3 - s2]],
                appliedSides[[s2]]];
            positions = nonVarPositions[ruleLhs1, allVars];
            Table[
                Block[{subterm, ruleLhs1New},
                    subterm = If[ pos === {}, ruleLhs1,
                        Extract[ruleLhs1, pos]];
                    sub = Quiet @ THVMLink`ATP`Private`cplUnify[
                        subterm, ruleLhs2, allVars];
                    If[ ! AssociationQ[sub], Nothing,
                        cpLhs = applySubstitution[ruleRhs1, sub];
                        ruleLhs1New = If[ pos === {},
                            ruleRhs2,
                            ReplacePart[ruleLhs1, pos -> ruleRhs2]];
                        cpRhs = applySubstitution[ruleLhs1New, sub];
                        {swap, s1, o1, s2, o2, pos, subterm,
                            {cpLhs, cpRhs}}]],
                {pos, positions}]],
        {swap, 0, 1},
        {s1, 2}, {o1, {1, -1}},
        {s2, 2}, {o2, {1, -1}}], 5];
    (* Alpha-equivalence check: a single substitution must
       simultaneously map both sides.  Conjoint unification via
       List[a, b] vs List[c, d] — cplUnify recurses pair-wise
       with one shared σ, so an inconsistency in one slot fails
       the whole match (which the prior independent-unify code
       missed). *)
    Block[{checkPair},
        checkPair[{a_, b_}] := Block[
            {sub},
            sub = Quiet @ THVMLink`ATP`Private`cplUnify[
                List[a, b], List[sSides[[1]], sSides[[2]]],
                allVars];
            If[ AssociationQ[sub], True,
                sub = Quiet @ THVMLink`ATP`Private`cplUnify[
                    List[a, b], List[sSides[[2]], sSides[[1]]],
                    allVars];
                AssociationQ[sub]]];
        hit = SelectFirst[candidates,
            checkPair[#[[8]]] &,
            Missing["NoFit"]]];
    If[ MissingQ[hit], $Failed,
        Block[{cs, ms,
               swapHit = hit[[1]],
               s1 = hit[[2]], o1 = hit[[3]],
               s2 = hit[[4]], o2 = hit[[5]],
               pos = hit[[6]], subp = hit[[7]]},
            (* When swap=1, MatchingConstruct played host -- the
               Rule comes from mSides + the MatchingRule from cSides. *)
            If[ swapHit === 0,
                cs = cSides; ms = mSides,
                cs = mSides; ms = cSides];
            <|"Side" -> s1,
              "Orientation" -> o1,
              "MatchingSide" -> s2,
              "MatchingOrientation" -> o2,
              "Position" -> pos,
              "Subpattern" -> withVariablePatterns[
                  subp, Join[varSet, freshVars]],
              "Rule" -> mkRule[
                If[o1 === 1, cs[[s1]], cs[[3 - s1]]],
                If[o1 === 1, cs[[3 - s1]], cs[[s1]]],
                varSet],
              (* MatchingRule's vars rename back to varSet names
                 (the cplR<n> Unique symbols would otherwise leak
                 into the verifier's pattern machinery and trip
                 internal Unique[] calls). *)
              "MatchingRule" -> Block[{
                  lhsBase = If[o2 === 1, ms[[s2]], ms[[3 - s2]]],
                  rhsBase = If[o2 === 1, ms[[3 - s2]], ms[[s2]]],
                  reverseMap = AssociationThread[
                      freshVars, varSet]},
                  mkRule[lhsBase /. reverseMap,
                      rhsBase /. reverseMap, varSet]]|>]]
];

(* Take an already-lifted prfList (Association of {Type, n} ->
   <|Statement, Proof|>) and, for each SubstitutionLemma /
   Conclusion / CriticalPairLemma entry, reconstruct the rewrite
   metadata.  Falls back to leaving the skeleton Proof unchanged
   when reconstruction fails (so the lifted object still works
   for property dispatch). *)
augmentSingleRewriteEntries[prfList_List, varSet_List : {}] := Block[
    {prfAssoc = Association[prfList], augment},
    augment[key_, entry_Association] := Block[
        {type = First[key], proof, constructKey, matchingKey, inputKey,
         constructEntry, matchingEntry, inputEntry, stepEq, recon},
        proof = Lookup[entry, "Proof", <||>];
        stepEq = entry["Statement"];
        Switch[type,
            "SubstitutionLemma" | "Conclusion",
                inputKey     = Lookup[proof, "Input",     Missing[]];
                constructKey = Lookup[proof, "Construct", Missing[]];
                (* Conclusion can omit Input -- the implicit Input is
                   the Hypothesis (the goal being proved). *)
                If[ type === "Conclusion" && MissingQ[inputKey],
                    inputKey = {"Hypothesis", 1};
                    proof = Association[proof, "Input" -> inputKey]];
                If[ MissingQ[inputKey] || MissingQ[constructKey],
                    Return[entry]];
                inputEntry     = Lookup[prfAssoc, Key[inputKey],     Missing[]];
                constructEntry = Lookup[prfAssoc, Key[constructKey], Missing[]];
                (* If Construct points at a True-closure entry (an
                   SL/Conclusion whose Statement collapsed to True),
                   walk one level back to the actual rewrite rule. *)
                If[ AssociationQ[constructEntry] &&
                    constructEntry["Statement"] === True,
                    Block[{innerKey = Lookup[
                            Lookup[constructEntry, "Proof", <||>],
                            "Construct", Missing[]]},
                        If[ ! MissingQ[innerKey],
                            constructKey = innerKey;
                            constructEntry = Lookup[prfAssoc,
                                Key[innerKey], Missing[]];
                            proof = Association[proof,
                                "Construct" -> innerKey]]]];
                If[ MissingQ[inputEntry] || MissingQ[constructEntry],
                    Return[entry]];
                recon = reconstructSingleRewrite[
                    inputEntry["Statement"],
                    constructEntry["Statement"],
                    stepEq, varSet];
                If[ recon === $Failed, Return[entry]];
                Return[ReplacePart[entry,
                    "Proof" -> Association[proof, recon,
                        "Source" -> If[type === "Conclusion", "cpl", "norm"],
                        "InputOrientation" -> 1,
                        "OutputExpression" -> stepEq]]],
            "CriticalPairLemma",
                constructKey = Lookup[proof, "Construct",         Missing[]];
                matchingKey  = Lookup[proof, "MatchingConstruct", Missing[]];
                If[ MissingQ[constructKey] || MissingQ[matchingKey],
                    Return[entry]];
                constructEntry = Lookup[prfAssoc, Key[constructKey], Missing[]];
                matchingEntry  = Lookup[prfAssoc, Key[matchingKey],  Missing[]];
                If[ MissingQ[constructEntry] || MissingQ[matchingEntry],
                    Return[entry]];
                recon = reconstructSuperposition[
                    constructEntry["Statement"],
                    matchingEntry["Statement"],
                    stepEq, varSet];
                (* Bisect (5a14987a) showed: a placeholder rule that
                   doesn't ACTUALLY derive the Statement causes a
                   verifier Failure with "Can't unify <axiom> with
                   <placeholder>".  So writing a fake identity rule
                   is worse than leaving the entry unaugmented --
                   leave it skeleton-only and let the verifier
                   report its own KeyAbsent failure.  Real fix is
                   making the enumeration find valid superpositions
                   for CPL 2/3 etc. *)
                If[ recon === $Failed, Return[entry]];
                Return[ReplacePart[entry,
                    "Proof" -> Association[proof, recon]]]];
        entry
    ];
    KeyValueMap[#1 -> augment[#1, #2] &, prfAssoc]
];

(* ---- CLI -> verifiable ProofObject lift -------------------------
   Take the Association that TSZSDerivationToProofObject returns and
   produce a literal `ProofObject["EquationalLogic", goal, axioms,
   data]` that WL's property machinery accepts (ProofFunction etc.).

   Three transforms compose:
     1. TPTPImport renders TPTP terms as String tokens wrapped in a
        zero-arg call -- `"x1"[]` for variables, `"k1"[]` for
        constants.  liftStringLeaves promotes both to Global` Symbols.
     2. Axiom variables must appear as Pattern[name, Blank[]] (the
        `name_` form the AxiomaticTheory schemas expect).
        withVariablePatterns rewrites a chosen set of symbols into
        that pattern form.
     3. arg3 axioms use Inactive[Equal][lhs, rhs]; arg4 Proof entries'
        Statement field uses HoldForm[lhs == rhs].  Different
        wrappers around the same canonicalized equation. *)
liftStringLeaves[expr_] := expr //. {
    h_String[] /; StringLength[h] > 0 :> Symbol["Global`" <> h]
};

collectVarSymbols[expr_] := DeleteDuplicates @ Cases[
    {expr},
    s_Symbol /; (Context[s] === "Global`" &&
        StringMatchQ[SymbolName[s],
            "x" ~~ DigitCharacter ..]),
    Infinity];

(* `expr /. v -> Pattern[v, Blank[]]` would loop since the RHS
   contains the LHS literally.  Build the substitution map with
   Hold and ReleaseHold once at the end. *)
withVariablePatterns[expr_, vars_List] := ReleaseHold @ Block[
    {pairs},
    pairs = Map[# -> Hold[Pattern][#, Blank[]] &, vars];
    Hold[expr] /. pairs /. Hold[Pattern] -> Pattern
];

inactivateEqual[Equal[a_, b_]] := Inactive[Equal][a, b];
inactivateEqual[other_]         := other;

holdEqual[Equal[a_, b_]] := With[{x = a, y = b}, HoldForm[x == y]];
holdEqual[other_]         := other;

liftToProofObject[assoc_Association] := Block[
    {goalRaw, axiomsRaw, dsRaw, goalLifted, axiomsLifted,
     varSyms, allSyms, constSyms, axList, prfList},
    goalRaw   = assoc["Goal"];
    axiomsRaw = assoc["Axioms"];
    dsRaw     = assoc["ProofDataset"];
    goalLifted   = liftStringLeaves[goalRaw];
    axiomsLifted = liftStringLeaves /@ axiomsRaw;
    (* Variables come from axioms (the universally-quantified `x`
       names).  The goal's skolem-constants live in constSyms. *)
    varSyms   = collectVarSymbols[axiomsLifted];
    allSyms   = DeleteDuplicates @ Cases[
        {axiomsLifted, goalLifted},
        s_Symbol /; Context[s] === "Global`",
        Infinity];
    constSyms = Complement[allSyms, varSyms];
    axList = Map[
        inactivateEqual @ withVariablePatterns[#, varSyms] &,
        axiomsLifted];
    (* First pass: lift each Statement / preserve skeleton Proof links.
       NOTE: preset stores arg4-Proof Statements with BARE variables
       (HoldForm[a ⊗ b == ...]) and arg3 axioms with Pattern[v, _]
       (Inactive[Equal][(a_) ⊗ (b_), ...]).  So withVariablePatterns
       is applied to arg3 above but NOT to the Statement here. *)
    prfList = KeyValueMap[
        #1 -> Association[
            "Statement" -> Block[{
                raw = liftStringLeaves @
                    Lookup[#2, "Statement", Missing[]]},
                If[ raw === True || MissingQ[raw],
                    raw,
                    holdEqual[raw]
                ]],
            "Proof"     -> Lookup[#2, "Proof", <||>]] &,
        dsRaw];
    (* Second pass: for SubstitutionLemma + Conclusion entries (which
       have a single-rewrite shape: Input parent + Construct parent),
       reconstruct the rewrite metadata the verifier needs.  Walks
       prfList for parent lookups (an Association of {Type, n} ->
       <|Statement, Proof|>).  Augments the Proof field in-place;
       leaves Axioms/Hypotheses/CPLs untouched. *)
    prfList = augmentSingleRewriteEntries[prfList, varSyms];
    ProofObject[
        "EquationalLogic",
        inactivateEqual @ withVariablePatterns[goalLifted, varSyms],
        axList,
        <|
            "Variables" -> varSyms,
            "Constants" -> constSyms,
            "Proof"     -> prfList
        |>]
];

buildDatasetFromDerivation[derivation_List, parseFormulasQ_:False] := Block[
    {folded, nameToKey, entries, stmtFn},
    (* Drop bookkeeping steps (orient, reorient_equations,
       equation_copy) BEFORE construct-key assignment; aliases
       route later references to the step's first real parent. *)
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
    "ParseFormulas" -> False,
    "LiftToProofObject" -> False
}

(* E-prover wrapper: chain TEproverProof + the generic SZS-to-
   ProofObject builder.  E's --proof-object --tstp-format emits
   the same SZS-framed fof+inference DAG Vampire does, so the
   downstream path is shared. *)
Options[TEproverProofObject] = {
    TimeConstraint  -> 30,
    "Binary"        -> Automatic,
    "ParseFormulas" -> False,
    "LiftToProofObject" -> False
}

TEproverProofObject[theory_String, thm_String, opts : OptionsPattern[]] := Block[
    {epR = TEproverProof[theory, thm,
            FilterRules[{opts}, Options[TEproverProof]]],
        liftQ = TrueQ @ OptionValue["LiftToProofObject"],
        parseOpt, assoc},
    parseOpt = "ParseFormulas" -> (liftQ ||
        TrueQ @ OptionValue["ParseFormulas"]);
    If[ epR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool"     -> "Eprover",
            "Status"   -> epR["Status"],
            "Seconds"  -> epR["Seconds"],
            "Strategy" -> epR["Strategy"]
        |>],
        assoc = TSZSDerivationToProofObject[
            epR["Inferences"], parseOpt];
        If[ liftQ,
            THVMLink`ATP`Private`liftToProofObject[assoc],
            assoc
        ]
    ]
]

(* Vampire-specific wrapper: chain TVampireProof + the generic
   SZS-to-ProofObject builder. *)
TVampireProofObject[theory_String, thm_String, opts : OptionsPattern[]] := Block[
    {vampR = TVampireProof[theory, thm,
            FilterRules[{opts}, Options[TVampireProof]]],
        liftQ = TrueQ @ OptionValue["LiftToProofObject"],
        parseOpt, assoc},
    parseOpt = "ParseFormulas" -> (liftQ ||
        TrueQ @ OptionValue["ParseFormulas"]);
    If[ vampR["Status"] =!= "Proved",
        Failure["ExternalNoProof", <|
            "Tool"     -> "Vampire",
            "Status"   -> vampR["Status"],
            "Seconds"  -> vampR["Seconds"],
            "Strategy" -> vampR["Strategy"]
        |>],
        assoc = TSZSDerivationToProofObject[
            vampR["Inferences"], parseOpt];
        If[ liftQ,
            THVMLink`ATP`Private`liftToProofObject[assoc],
            assoc
        ]
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
    "ParseFormulas" -> False,
    (* LiftToProofObject -> True forces ParseFormulas and rewraps
       the resulting Association into a literal 4-arg ProofObject
       so the WL property machinery (ProofFunction, ProofGraph,
       Theorems) dispatches.  pf[Theorems] still fails verification
       because the Proof entries lack the rewrite metadata
       (Orientation, Rule, Side, Position) the verifier needs --
       extracting those from the SZS DAG is a deeper iter. *)
    "LiftToProofObject" -> False
}

(* Two-arg (Theory, thm) form: resolve to a pre-generated .pr file
   under tools/baselines/wm_pr/.  Pre-generate the .pr files via
   tools/baselines/tptp_to_pr.wls; missing .pr returns
   `NoCachedPr` Failure with the converter command line. *)
TWaldmeisterProofObject[theory_String, thm_String,
        opts : OptionsPattern[]] /; ! FileExtension[theory] === "pr" :=
    Block[{path = FileNameJoin[{
            Directory[], "tools", "baselines", "wm_pr",
            theory <> "__" <> thm <> ".pr"}]},
        If[ FileExistsQ[path],
            TWaldmeisterProofObject[path, opts],
            Failure["NoCachedPr", <|
                "Theory" -> theory,
                "Theorem" -> thm,
                "Reason" -> StringJoin[
                    "Run `wolframscript -f tools/baselines/tptp_to_pr.wls ",
                    "tools/baselines/vampire_tptp/", theory, "__", thm,
                    ".p tools/baselines/wm_pr/", theory, "__", thm, ".pr`"
                ]
            |>]
        ]
    ]

TWaldmeisterProofObject[problemFile_String, opts : OptionsPattern[]] /;
        FileExtension[problemFile] === "pr" :=
    Block[
        {wmR = TWaldmeisterProof[problemFile,
                FilterRules[{opts},
                    {TimeConstraint, "Binary", "MathlinkPath"}]],
            liftQ = TrueQ @ OptionValue["LiftToProofObject"],
            parseOpt, assoc},
        (* LiftToProofObject implies ParseFormulas: the lift needs
           WL-parsed formula bodies to walk. *)
        parseOpt = "ParseFormulas" -> (liftQ ||
            TrueQ @ OptionValue["ParseFormulas"]);
        If[ wmR["Status"] =!= "Proved",
            Failure["ExternalNoProof", <|
                "Tool"     -> "Waldmeister",
                "Status"   -> wmR["Status"],
                "Seconds"  -> wmR["Seconds"]
            |>],
            assoc = TSZSDerivationToProofObject[
                wmR["Inferences"], parseOpt];
            If[ liftQ,
                THVMLink`ATP`Private`liftToProofObject[assoc],
                assoc
            ]
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
