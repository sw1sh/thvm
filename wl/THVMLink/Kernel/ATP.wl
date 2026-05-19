(* ::Package:: *)
(* ATP.wl - WL surface for the IC-native ATP saturation engine.

   Public surface
     TATP[axioms, conjecture, opts]
         Run the ATP saturator on a list of equational axioms + a
         single conjecture.  Returns an Association with
         Status / Steps / Rules / QueueSize.  With Witness opts,
         result also carries Witness / Witnesses bindings.

     TATP[File["path.pr"], opts]
         File-form: parse a Waldmeister .pr spec via wald_parse_file
         on the C side, run the saturator, return the same kind of
         Association (no witnesses).

     TFindEquationalProof[conjecture, axioms, opts]
     TFindEquationalProof["Theorem", "Theory", opts]
         Run thvm's C ATP completion engine on the conjecture +
         axioms and return a real WL ProofObject -- the same head
         FindEquationalProof returns, supporting the property
         interface (p["ProofDataset"], p["ProofGraph"],
         p["ProofFunction"], p["ProofLength"], ...).  The string
         form resolves theorem + theory names through
         AxiomaticTheory.  Returns $Failed when the conjecture is
         not proved.

   Options
     MaxSteps       (TATP)                  -> 64
     MaxSteps       (TFindEquationalProof)  -> 200000
     Witness        (TATP)                  -> {}    list of x_
     AllWitnesses   (TATP)                  -> False
     MaxDepth       (TATP / AllWitnesses)   -> 8
     MaxWitnesses   (TATP / AllWitnesses)   -> 16

   See docs/plans/waldmeister_ic_atp.md for the algorithmic intent. *)

BeginPackage["THVMLink`"];

TATP::usage = "TATP[{lhs == rhs, ...}, conjecture] runs the IC-native ATP saturation on the given equational axioms and conjecture, returning an Association with Status, Steps, Rules, QueueSize.  Variables are written as `x_` (Pattern[name, Blank[]]).  TATP[File[path]] parses a Waldmeister .pr file and runs the saturator directly.";

TFindEquationalProof::usage = "TFindEquationalProof[conjecture, axioms] runs thvm's C ATP completion engine and returns a real WL ProofObject -- the same head FindEquationalProof returns, supporting the full property interface (p[\"ProofDataset\"], p[\"ProofGraph\"], p[\"ProofFunction\"], p[\"ProofLength\"], etc.).  TFindEquationalProof[\"Theorem\", \"Theory\"] resolves the theorem and theory names through AxiomaticTheory.  The C engine saturates the axioms; the resulting equational rewrite chain is decoded into a verifier-shaped ProofObject.  Returns $Failed when the conjecture is not proved.";

(* Forward-declare symbols owned by sibling files (Switch.wl owns
   the IC term constructors) so bare references inside
   Begin[`Private`] resolve to THVMLink`X instead of a phantom
   THVMLink`Private`X.  ATP.wl is parsed first in alphabetical
   order, so these public names don't yet exist when this file's
   body runs.  Mirrors the same guard in Lazy.wl. *)
{TDef, TRef, TIfZero, TOp2, TNum, TSup, TApp, TLam,
 TCollapse, TCnf, TTermTag, TTermVal, TTermExt, THeapRead,
 FromTTerm, TTerm};

Begin["`Private`"];

(* === LibraryLink loaders =========================================== *)

(* ATP runner.  Takes a packed Int64 NumericArray
     [n_axioms, lhs_0, rhs_0, ..., goal_lhs, goal_rhs]
   plus max_steps and max_label.  Returns a 4-element Int64
   NumericArray [status, n_rules, n_trace, n_cps]. *)
$atpRunFn := $atpRunFn = load[
    "thvm_wl_atp_run",
    {{"NumericArray", "Shared"}, Integer, Integer},
    "NumericArray"
]

(* Proof runner: $atpRunFn + proof extraction.  Returns one
   self-describing Int64 NumericArray -- a 5-int header
   [status, n_rules, n_trace, n_cps, n_steps] followed by the
   rules / r_trace / trace / steps blocks (see thvmlink_atp.c). *)
$atpRunProofFn := $atpRunProofFn = load[
    "thvm_wl_atp_run_proof",
    {{"NumericArray", "Shared"}, Integer, Integer},
    "NumericArray"
]

(* Existential runner: $atpRunFn + a witness-id MTensor; output
   gains n_witness trailing Term values. *)
$atpRunExistFn := $atpRunExistFn = load[
    "thvm_wl_atp_run_existential",
    {{"NumericArray", "Shared"}, Integer, Integer, {Integer, 1}},
    "NumericArray"
]

(* Multi-witness: saturate first, then thvm_atp_narrow_all.  Output
   layout:
     [status, n_rules, n_trace, n_cps, n_found,
      w_0_id_0, ..., w_(max_witnesses-1)_id_(n_witness-1)].
   Length = 5 + max_witnesses * n_witness. *)
$atpRunAllFn := $atpRunAllFn = load[
    "thvm_wl_atp_run_all_witnesses",
    {{"NumericArray", "Shared"}, Integer, Integer, {Integer, 1},
     Integer, Integer},
    "NumericArray"
]

(* File-driven runner: parses a Waldmeister .pr spec via
   wald_parse_file and runs the saturator. *)
$atpRunFileFn := $atpRunFileFn = load[
    "thvm_wl_atp_run_file",
    {"UTF8String", Integer},
    "NumericArray"
]

(* CTR-builder for the expression encoder: takes a label + a
   NumericArray of child Term values, returns the packed Term value
   of the new TAG_CTR. *)
$termNewCtrFn := $termNewCtrFn = load[
    "thvm_wl_term_new_ctr",
    {Integer, {Integer, 1}},
    Integer
]

(* === WL-expression to Term encoder ================================ *)

(* Map:
     Pattern[name, Blank[]]   -> term_new_fvr(var_id)
     Symbol[name]             -> nullary CTR
     head[args...]            -> CTR with encoded children
   State is threaded explicitly: takes (expr, state) and returns
   {term, state'} where state is
     <|"sym" -> <|name -> label|>,
       "var" -> <|name -> id|>,
       "next_lab" -> next_label|>.
   Patterns are matched via Verbatim[Pattern] so the Pattern head
   isn't itself parsed as a pattern. *)

(* Look up an existing var or extend the var table; return
   {var_id, state'}. *)
ensureVar[varName_String, state_Association] := Block[{
    vars = state["var"]
},
    If[ KeyExistsQ[vars, varName],
        {vars[varName], state},
        {Length[vars],
         Append[state, "var" -> Append[vars, varName -> Length[vars]]]}
    ]
]

(* Look up an existing symbol or assign a fresh label; return
   {label, state'}. *)
ensureSym[sym_String, state_Association] := Block[{
    syms = state["sym"],
    nextLab = state["next_lab"]
},
    If[ KeyExistsQ[syms, sym],
        {syms[sym], state},
        {
            nextLab,
            Append[state,
                <|
                    "sym" -> Append[syms, sym -> nextLab],
                    "next_lab" -> nextLab + 1
                |>
            ]
        }
    ]
]

encodeAtpTerm[Verbatim[Pattern][name_Symbol, Blank[]], state_Association] := Block[{
    varName = SymbolName[Unevaluated[name]],
    varId, st
},
    {varId, st} = ensureVar[varName, state];
    {THVMLink`Private`$termNewFn[0, 22 (* TAG_FVR *), varId, 0], st}
]

encodeAtpTerm[s_Symbol, state_Association] := Block[{
    sym = ToString[Unevaluated[s]],
    lab, st
},
    {lab, st} = ensureSym[sym, state];
    {THVMLink`Private`$termNewCtrFn[lab, {}], st}
]

(* Fold step that threads the encoder state through a list of
   children: accumulator is {encoded_terms_so_far, state}. *)
encodeChildStep[{terms_, state_}, child_] := Block[{
    enc = encodeAtpTerm[child, state]
},
    {Append[terms, enc[[1]]], enc[[2]]}
]

encodeAtpTerm[expr_, state_Association] := Block[{
    sym = ToString[Head[expr]],
    lab, st, childEncs
},
    {lab, st} = ensureSym[sym, state];
    {childEncs, st} =
        Fold[encodeChildStep, {{}, st}, List @@ expr];
    {THVMLink`Private`$termNewCtrFn[lab, childEncs], st}
]

encodeAtpTermInit[] := <|"sym" -> <||>, "var" -> <||>, "next_lab" -> 1|>

(* === Status decoder + stats Association builder =================== *)

$atpStatusName = <|
    0 -> "RUNNING", 1 -> "PROVED", 2 -> "REFUTED",
    3 -> "TIMEOUT", 4 -> "QUEUE_EMPTY"
|>

atpStatusFor[code_Integer] :=
    Lookup[$atpStatusName, code, "UNKNOWN(" <> ToString[code] <> ")"]

(* Build the public Status/Steps/Rules/QueueSize assoc from the
   first four entries of any runner's stats array. *)
atpStatsAssoc[stats_List] := <|
    "Status" -> atpStatusFor[stats[[1]]],
    "Steps" -> stats[[3]],
    "Rules" -> stats[[2]],
    "QueueSize" -> stats[[4]]
|>

(* === Shared problem encoder ======================================= *)

(* Apply Pattern[v, Blank[]] substitution for each bound symbol to
   a held body, returning HoldComplete[body-with-patterns].  Building
   the substitution rules via Function with HoldFirst keeps each
   bound symbol unevaluated during rule construction. *)
applyForAllSubst[hcBody_HoldComplete, vars_List] := Block[{
    rules
},
    rules = Function[{v}, v :> Pattern[v, Blank[]],
        {HoldAll}] /@ vars;
    hcBody /. rules
]

(* Strip the outermost ForAll wrapper from a held equation, replacing
   each bound bare-symbol occurrence inside the body with
   Pattern[var, Blank[]].  Pass-through when there's no ForAll.
   Wrapping body in HoldComplete before /. keeps tautology shapes
   like `f[x] == f[x]` from evaluating to True before substitution
   runs. *)
forAllToPattern[axHC_HoldComplete] := Replace[axHC, {
    HoldComplete[ForAll[v_Symbol, body_]] :>
        applyForAllSubst[HoldComplete[body], {v}],
    HoldComplete[ForAll[Verbatim[List][vars__Symbol], body_]] :>
        applyForAllSubst[HoldComplete[body], List @@ Hold[vars]],
    _ :> axHC
}]

(* Encode a single equation HoldComplete[Equal[lhs, rhs]] (or
   HoldComplete[ForAll[..., Equal[lhs, rhs]]]) into
   {term_lhs, term_rhs, state'}.  Throws "TATPError" Failure on
   shape mismatch. *)
encodeEquation[axHCRaw_HoldComplete, state_, label_] := Block[{
    axHC, lhs, rhs, lr, rr
},
    axHC = forAllToPattern[axHCRaw];
    If[ ! MatchQ[axHC, HoldComplete[Equal[_, _]]],
        Throw[Failure["TATPParseError",
            <|"Axiom" -> label, "Reason" -> "expected `lhs == rhs`"|>],
            "TATPError"
        ]
    ];
    lhs = Extract[axHC, {1, 1}, HoldComplete];
    rhs = Extract[axHC, {1, 2}, HoldComplete];
    lr = encodeAtpTerm[lhs[[1]], state];
    rr = encodeAtpTerm[rhs[[1]], lr[[2]]];
    {lr[[1]], rr[[1]], rr[[2]]}
]

(* Fold step over axiom HoldCompletes; threads the encoder state
   and accumulates packed [lhs, rhs] term pairs. *)
encodeAxiomFold[{terms_, state_, idx_}, axHC_] := Block[{
    r = encodeEquation[axHC, state, idx]
},
    {Join[terms, {r[[1]], r[[2]]}], r[[3]], idx + 1}
]

(* Encode the (axioms, conjecture) pair into the packed Int64 NA
   the FFI runners expect, plus everything the WL-side
   post-processors need.  Throws "TATPError" Failures on shape
   mismatch.  HoldComplete is used throughout so `a == a` doesn't
   pre-evaluate to True. *)
SetAttributes[atpEncodeProblem, HoldAll];
atpEncodeProblem[axioms_, conjecture_] := Block[{
    axHCsRaw, axHCs, cjHC, axTermsAndState, axTerms, st,
    goalRes, goalLhs, goalRhs, axPairs, conjPair, n
},
    If[ ! ListQ[Unevaluated[axioms]],
        Throw[Failure["TATPParseError",
            <|"Reason" -> "axioms must be a List"|>], "TATPError"
        ]
    ];
    ensureInit[];
    n = Length[Unevaluated[axioms]];
    axHCsRaw = HoldComplete /@ Unevaluated[axioms];
    (* Normalize each axiom: strip the outermost ForAll wrapper (if
       present) and rewrite bound bare-symbol occurrences as
       Pattern[var, Blank[]]. *)
    axHCs = forAllToPattern /@ axHCsRaw;
    axTermsAndState = Fold[encodeAxiomFold, {{}, encodeAtpTermInit[], 1}, axHCs];
    axTerms = axTermsAndState[[1]];
    st = axTermsAndState[[2]];
    cjHC = forAllToPattern[HoldComplete[conjecture]];
    goalRes = encodeEquation[cjHC, st, "conjecture"];
    goalLhs = goalRes[[1]];
    goalRhs = goalRes[[2]];
    st = goalRes[[3]];
    (* Extract {lhs, rhs} pairs from each (stripped) held axiom
       directly via positions {1,1}/{1,2} of
       HoldComplete[lhs==rhs].  Avoids ReleaseHold which
       auto-evaluates `a == a` axioms to True. *)
    axPairs = (
        {Extract[#, {1, 1}], Extract[#, {1, 2}]} & /@ axHCs
    );
    conjPair = {Extract[cjHC, {1, 1}], Extract[cjHC, {1, 2}]};
    <|
        "Packed" -> NumericArray[
            Join[{n}, axTerms, {goalLhs, goalRhs}],
            "Integer64"
        ],
        "MaxLab" -> st["next_lab"],
        "State" -> st,
        "AxPairs" -> axPairs,
        "ConjPair" -> conjPair,
        "AxHCsRaw" -> axHCsRaw,
        "ConjHCRaw" -> HoldComplete[conjecture]
    |>
]

(* === TATP[] WL surface ============================================ *)

(* HoldAll: WL evaluates `a == a` to True before reaching us; we
   need the syntactic Equal[lhs, rhs] form to destructure. *)
SetAttributes[TATP, HoldAll];
Options[TATP] = {
    MaxSteps -> 64,
    Witness -> {},
    AllWitnesses -> False,
    MaxDepth -> 8,
    MaxWitnesses -> 16
};

(* Resolve one Witness entry to its {name, id} pair using the
   encoder state.  Throws on names not present in axioms. *)
witnessPair[w_, state_Association] := Block[{
    wn, wid
},
    If[ ! MatchQ[w, Verbatim[Pattern][_Symbol, Blank[]]],
        Throw[Failure["TATPParseError",
            <|"Reason" -> "Witness entries must be `x_` patterns"|>],
            "TATPError"]
    ];
    wn = Replace[w,
        Verbatim[Pattern][s_Symbol, Blank[]] :>
            SymbolName[Unevaluated[s]]];
    wid = Lookup[state["var"], wn, $Failed];
    If[ wid === $Failed,
        Throw[Failure["TATPParseError",
            <|"Reason" -> "Witness var `" <> wn <>
                "` not present in axioms / conjecture"|>],
            "TATPError"]
    ];
    {wn, wid}
]

(* Map all Witness specs to {names, ids}. *)
atpResolveWitnessIds[witnessSpec_List, state_Association] := Block[{
    pairs = Map[witnessPair[#, state] &, witnessSpec]
},
    {pairs[[All, 1]], pairs[[All, 2]]}
]

(* File-form dispatch (.pr file). *)
TATP[File[path_String], OptionsPattern[]] := Catch[
    Block[{
        stats
    },
        ensureInit[];
        stats = Normal @ $atpRunFileFn[path, OptionValue[MaxSteps]];
        atpStatsAssoc[stats]
    ],
    "TATPError"
]

(* Universal goal: just status + stats. *)
tatpUniversal[enc_, maxSteps_] := Block[{
    stats = Normal @ $atpRunFn[
        enc["Packed"], maxSteps, enc["MaxLab"]
    ]
},
    atpStatsAssoc[stats]
]

(* Single-witness narrow: one binding per witness name. *)
tatpWitness[enc_, maxSteps_, witnessSpec_List] := Block[{
    names, ids, stats, witnessVals, witnessAssoc
},
    {names, ids} = atpResolveWitnessIds[witnessSpec, enc["State"]];
    stats = Normal @ $atpRunExistFn[
        enc["Packed"], maxSteps, enc["MaxLab"], ids
    ];
    witnessVals = stats[[5 ;; 4 + Length[ids]]];
    witnessAssoc = AssociationThread[Symbol /@ names -> witnessVals];
    Append[atpStatsAssoc[stats], "Witness" -> witnessAssoc]
]

(* Multi-witness: saturate then narrow_all. *)
tatpAllWitnesses[enc_, maxSteps_, witnessSpec_, maxDepth_, maxWitnesses_] := Block[{
    names, ids, stats, nFound, k, witnessRows, witnessAssocs
},
    {names, ids} = atpResolveWitnessIds[witnessSpec, enc["State"]];
    stats = Normal @ $atpRunAllFn[
        enc["Packed"], maxSteps, enc["MaxLab"],
        ids, maxDepth, maxWitnesses
    ];
    nFound = stats[[5]];
    k = Length[ids];
    witnessRows = If[ nFound > 0 && k > 0,
        Partition[stats[[6 ;; 5 + nFound * k]], k],
        {}
    ];
    witnessAssocs = Table[
        AssociationThread[Symbol /@ names -> ws],
        {ws, witnessRows}
    ];
    Append[atpStatsAssoc[stats], "Witnesses" -> witnessAssocs]
]

TATP[axioms_, conjecture_, OptionsPattern[]] := Catch[
    Block[{
        enc = atpEncodeProblem[axioms, conjecture],
        witnessSpec = OptionValue[Witness],
        maxSteps = OptionValue[MaxSteps]
    },
        Which[
            Length[witnessSpec] === 0,
                tatpUniversal[enc, maxSteps],
            OptionValue[AllWitnesses],
                tatpAllWitnesses[enc, maxSteps, witnessSpec,
                    OptionValue[MaxDepth], OptionValue[MaxWitnesses]],
            True,
                tatpWitness[enc, maxSteps, witnessSpec]
        ]
    ],
    "TATPError"
]

(* === ProofDataset key conventions ================================ *)

(* Use the same keys WL's ProofObject expects so our dataset matches
   the FindEquationalProof shape exactly. *)
$AxiomSym = "Axiom";
$HypothesisSym = "Hypothesis";
$SubstitutionLemmaSym = "SubstitutionLemma";
$ConclusionSym = "Conclusion";

(* Convert an `Inactive[Equal][lhs, rhs]` value to the
   `HoldForm[Equal[lhs, rhs]]` shape WL's verifier expects.
   HoldForm[ie] holds ie's value, then /. swaps the held
   Inactive[Equal] head for plain Equal -- Equal stays held by
   HoldForm, so tautologies like Equal[x, x] do NOT collapse to
   True.  Verifier path: With-binds Statement, Inactivate restores
   Inactive[Equal], ReleaseHold strips HoldForm. *)
toHoldEq[expr_] := HoldForm[expr] /. Inactive[Equal] -> Equal

(* === axiom / hypothesis preprocessing ============================= *)

(* Ported from the UnfailingKnuthBendixCompletion resource-function
   notebook: quantifier elimination (ForAll -> Pattern,
   Exists -> Skolem) and canonical pattern-variable naming.  Used by
   the string form of TFindEquationalProof to normalize the
   AxiomaticTheory-resolved formulas. *)

wrap[expr_, head_: List] := Replace[expr, x : Except[_head] :> head[x]]

(* Rename every Pattern variable in `expr` to a canonical short name
   (a, b, c, ..., then a1, b1, ... if more are needed). *)
CanonicalizePatterns[expr_] := Module[{
    chars = CharacterRange["a", "z"],
    patts = DeleteDuplicates[Cases[expr, _Pattern, All, Heads -> True]]
},
    chars = First @ NestWhile[
        Apply[Function[{cs, k},
            {Join[cs, (StringJoin[#, ToString[k]] &) /@ cs], k + 1}]],
        {chars, 1}, Length[First[#]] < Length[patts] &];
    expr /. MapIndexed[
        With[{canonical = Pattern @@ {Symbol[Extract[chars, #2]], Last[#1]}},
            Verbatim[#1] :> canonical] &,
        patts]
]

(* Forward ref: skolemPatterns calls universalPatterns. *)
skolemPatterns[expr_ /; ! FreeQ[expr, _Exists], bound_: {}] :=
    expr //. HoldPattern[Exists[var_, cond___, sub_]] :> With[{
        vars = wrap[var]
    },
        With[{repl = Thread[vars -> (
            If[Length[bound] > 0, Unique[#] @@ bound, Unique[#]] &) /@ vars]},
            If[ Length[{cond}] > 0,
                universalPatterns[sub, bound] && cond /. repl,
                universalPatterns[sub, bound] /. repl]]]
skolemPatterns[expr_, ___] := expr

universalPatterns[expr_ /; ! FreeQ[expr, _ForAll], bound_: {}] :=
    expr //. HoldPattern[
        ForAll[var_, cond___, ForAll[var2_, cond2___, sub_]]] :>
        ForAll[Join[wrap[var], wrap[var2]], cond && cond2, sub] //.
    HoldPattern[ForAll[var_, cond___, sub_]] :> With[{vars = wrap[var]},
        With[{repl = Thread[vars -> Unique /@ vars]},
            With[{patternRepl = MapAt[Pattern[#, _] &, repl, {All, 2}]},
                With[{res = skolemPatterns[sub, Join[vars, bound]] /. patternRepl},
                    If[ Length[{cond}] > 0,
                        With[{newCond = cond /. repl}, res /; newCond],
                        res]]]]]
universalPatterns[expr_, ___] := expr

unquantifyFormula[expr_] := expr /. {
    universal_ForAll :> universalPatterns[universal],
    existential_Exists :> skolemPatterns[existential]}

(* === rewrite-rule list ============================================ *)

(* Strip Pattern[s, Blank[]] wrappers down to the bare symbol s.
   Used to convert an axiom rhs (which has Pattern[s, _] in the same
   shape as the lhs) into a Rule rhs that substitutes the bound
   value back instead of leaking the pattern variable. *)
stripPatterns[expr_] := expr /.
    Verbatim[Pattern][s_Symbol, _Blank] :> s

(* Forward + backward Rule entries for one axiom -- two-element
   list, used by buildRuleList's Table+Flatten.  RHS has Pattern
   wrappers stripped so substitution binds and re-emits bare
   symbols instead of literal `x_`s. *)
oneAxiomRules[axioms_, axiomKeys_, i_] := Block[{
    ax = axioms[[i]],
    key = axiomKeys[[i]]
},
    {
        <|
            "Rule" -> ax[[1]] -> stripPatterns[ax[[2]]],
            "AxiomKey" -> key,
            "Direction" -> 1,
            "OrientedStmt" -> ax
        |>,
        <|
            "Rule" -> ax[[2]] -> stripPatterns[ax[[1]]],
            "AxiomKey" -> key,
            "Direction" -> 2,
            "OrientedStmt" -> Reverse[ax]
        |>
    }
]

buildRuleList[axioms_, axiomKeys_] := Flatten[
    Table[
        oneAxiomRules[axioms, axiomKeys, i],
        {i, Length[axioms]}
    ],
    1
]

(* === ProofDataset builder ========================================= *)

(* Build one chain-step entry for the dataset.  Splits the absolute
   FirstPosition into {Side, RelativePos}.  Returns a Rule
   key -> assoc.

   Orientation carries the chain's use direction for the construct
   axiom: Direction 1 (forward) -> Orientation 1, Direction 2
   (backward) -> Orientation -1.  ConstructSide stays 1.  WL's
   verifier computes orientation = Replace[CS, {2->-1}] * Orientation
   then reverses the construct's Statement iff orientation === -1, so
   a Direction-2 step makes the verifier read the axiom's `lhs ==
   rhs` Statement as the rule `rhs -> lhs` -- exactly the backward
   rewrite the chain applied. *)
chainEntry[stepRec_, isLast_, lemmaIdx_, prevKey_, ruleEntry_] := Block[{
    stepKey, absPos, side, relPos, statement
},
    stepKey = If[ isLast,
        {$ConclusionSym, 1},
        {$SubstitutionLemmaSym, lemmaIdx}
    ];
    absPos = stepRec["Position"];
    side = If[ Length[absPos] >= 1, absPos[[1]], 1];
    relPos = If[ Length[absPos] >= 1, Drop[absPos, 1], {}];
    statement = toHoldEq[stepRec["NewExpr"]];
    stepKey -> <|
        "Statement" -> statement,
        "Proof" -> <|
            "Input" -> prevKey,
            "Construct" -> ruleEntry["AxiomKey"],
            "Position" -> relPos,
            "Rule" -> stepRec["Rule"],
            "Orientation" -> If[ ruleEntry["Direction"] === 2, -1, 1],
            "ConstructSide" -> 1,
            "InputOrientation" -> 1,
            "Side" -> side,
            "OutputExpression" -> statement,
            "Source" -> "synth"
        |>
    |>
]

(* Degenerate "Conclusion" entry for the trivial-tautology case
   (the conjecture was already x == x).  Points back to the
   Hypothesis with no rule applied. *)
trivialConclusionEntry[hypInactive_] := {$ConclusionSym, 1} -> <|
    "Statement" -> toHoldEq[hypInactive],
    "Proof" -> <|
        "Input" -> {$HypothesisSym, 1},
        "Construct" -> {$HypothesisSym, 1},
        "Position" -> {},
        "Rule" -> Rule @@ {hypInactive[[1]], hypInactive[[1]]},
        "Orientation" -> 1,
        "ConstructSide" -> 1,
        "InputOrientation" -> 1,
        "Side" -> 1,
        "OutputExpression" -> toHoldEq[hypInactive],
        "Source" -> "trivial"
    |>
|>

(* Assemble the sorted ProofDataset from a finished rewrite chain.
   `chain` is a list of step records (<|NewExpr, Position, RuleIdx,
   Rule|>); `ruleList` is the forward+backward rule table the
   RuleIdx fields index into. *)
assembleDataset[axioms_, conjecture_, chain_, ruleList_] := Block[{
    axCount = Length[axioms], axiomKeys, hypInactive,
    axiomEntries, chainEntries, allEntries
},
    hypInactive = Inactive[Equal] @@ conjecture;
    axiomKeys = Table[{$AxiomSym, k}, {k, axCount}];
    axiomEntries = Table[
        axiomKeys[[k]] -> <|
            "Statement" -> toHoldEq[Inactive[Equal] @@ axioms[[k]]],
            "Proof" -> <||>
        |>,
        {k, axCount}
    ];
    chainEntries = Table[
        chainEntry[
            chain[[s]],
            s === Length[chain],
            If[ s === Length[chain], 1, s],
            If[ s === 1,
                {$HypothesisSym, 1},
                {$SubstitutionLemmaSym, s - 1}
            ],
            ruleList[[chain[[s, "RuleIdx"]]]]
        ],
        {s, Length[chain]}
    ];
    allEntries = Join[
        axiomEntries,
        {{$HypothesisSym, 1} -> <|
            "Statement" -> toHoldEq[hypInactive],
            "Proof" -> <||>
        |>},
        If[ chain === {},
            {trivialConclusionEntry[hypInactive]},
            chainEntries
        ]
    ];
    SortBy[allEntries, $ProofKeyOrder[First[#]] &]
]

$ProofKeyOrder[{"Axiom", k_}] := {1, k}
$ProofKeyOrder[{"Hypothesis", k_}] := {2, k}
$ProofKeyOrder[{"SubstitutionLemma", k_}] := {3, k}
$ProofKeyOrder[{"Conclusion", k_}] := {4, k}
$ProofKeyOrder[_] := {5, 0}

(* === C-engine proof decoder ======================================= *)

(* ATP terms use two tags: TAG_CTR (20) for labelled constructors
   (a function head or, nullary, a constant symbol) and TAG_FVR (22)
   for first-order variables. *)
$AtpTagCTR = 20;
$AtpTagFVR = 22;

(* Decode a raw packed ATP Term back to a WL expression.  CTR ->
   head[children...] (arity 0 -> bare symbol); FVR -> the bound
   variable's bare symbol.  labelToName / idToName invert the
   encoder state's `sym` / `var` maps. *)
decodeAtpTerm[raw_Integer, labelToName_, idToName_] := Block[{
    tag = THVMLink`Private`$termTagFn[raw]
},
    Which[
        tag === $AtpTagFVR,
            Symbol @ Lookup[idToName, THVMLink`Private`$termExtFn[raw],
                "x" <> ToString[THVMLink`Private`$termExtFn[raw]]],
        tag === $AtpTagCTR,
            Block[{
                label = THVMLink`Private`$termExtFn[raw],
                loc = THVMLink`Private`$termValFn[raw],
                arity, name
            },
                arity = THVMLink`Private`$termValFn[
                    THVMLink`Private`$heapReadFn[loc]];
                name = Lookup[labelToName, label, "C" <> ToString[label]];
                If[ arity === 0,
                    Symbol[name],
                    Symbol[name] @@ Table[
                        decodeAtpTerm[
                            THVMLink`Private`$heapReadFn[loc + k],
                            labelToName, idToName],
                        {k, arity}]
                ]
            ],
        True, Missing["UndecodableTerm", tag]
    ]
]

(* Decode one variable-width steps block starting at cursor `c0`
   (1-based, points one BEFORE the first int of the block).  Reads
   `n` step records; returns {records, cursorAfter}.  Each step is
   side, rule, fwd, pos_len, pos[0..pos_len), before, after. *)
decodeStepsBlock[raw_, c0_, n_, labelToName_, idToName_] := Block[{
    cur = c0, recs
},
    recs = Table[
        Block[{side, ruleIx, posLen, posPath, beforeRaw, afterRaw},
            side = raw[[cur + 1]];
            ruleIx = raw[[cur + 2]];
            posLen = raw[[cur + 4]];
            posPath = If[ posLen === 0, {},
                raw[[cur + 5 ;; cur + 4 + posLen]]];
            beforeRaw = raw[[cur + 5 + posLen]];
            afterRaw = raw[[cur + 6 + posLen]];
            cur = cur + 6 + posLen;
            <|
                "Side" -> side,
                "RuleC" -> ruleIx,
                (* C child indices are 0-based; WL part positions
                   are 1-based. *)
                "PosPath" -> (posPath + 1),
                "Before" -> decodeAtpTerm[beforeRaw, labelToName, idToName],
                "After" -> decodeAtpTerm[afterRaw, labelToName, idToName]
            |>
        ],
        {n}
    ];
    {recs, cur}
]

(* Run the C ATP completion engine + proof extraction.  The C glue
   ships two derivations: the completion-saturated MAIN state's full
   trace DAG, and a no-completion EXT state whose chain (when it
   exists) is over the input axioms directly.  Returns an
   Association
     <|"Status", "ExtSteps", "MainSteps", "MainRules", "RTrace",
       "Trace"|>
   where ExtSteps is the axiom-cited chain (preferred by the simple
   path) and the Main* fields carry the completion DAG for the
   critical-pair lemma path.  ExtSteps / MainSteps are $Failed when
   the corresponding extraction produced nothing. *)
cEngineProof[enc_, maxSteps_] := Block[{
    raw, status, nRules, nTrace, nSteps, extNRules, extNSteps,
    cur, labelToName, idToName, mainSteps, extSteps, mainRules,
    rTrace, traceEntries
},
    raw = Normal @ $atpRunProofFn[enc["Packed"], maxSteps, enc["MaxLab"]];
    status = raw[[1]];
    nRules = raw[[2]]; nTrace = raw[[3]]; nSteps = raw[[5]];
    extNRules = raw[[6]]; extNSteps = raw[[7]];
    labelToName = Association[Reverse /@ Normal[enc["State"]["sym"]]];
    idToName = Association[Reverse /@ Normal[enc["State"]["var"]]];
    If[ status =!= 1,
        Return[<|"Status" -> status, "ExtSteps" -> $Failed,
            "MainSteps" -> $Failed|>]
    ];
    (* MAIN rules block: 2*nRules packed Terms. *)
    cur = 7;
    mainRules = Table[
        Block[{l = raw[[cur + 1]], r = raw[[cur + 2]]},
            cur = cur + 2;
            {decodeAtpTerm[l, labelToName, idToName],
             decodeAtpTerm[r, labelToName, idToName]}
        ],
        {nRules}
    ];
    (* MAIN r_trace block: nRules ints. *)
    rTrace = raw[[cur + 1 ;; cur + nRules]];
    cur = cur + nRules;
    (* MAIN trace block: variable width reason, pa, pb, lhs, rhs,
       pos_len, pos[..]. *)
    traceEntries = Table[
        Block[{reason, pa, pb, l, r, posLen, pos},
            reason = raw[[cur + 1]]; pa = raw[[cur + 2]];
            pb = raw[[cur + 3]]; l = raw[[cur + 4]];
            r = raw[[cur + 5]]; posLen = raw[[cur + 6]];
            pos = If[ posLen === 0, {},
                raw[[cur + 7 ;; cur + 6 + posLen]]];
            cur = cur + 6 + posLen;
            <|
                "Reason" -> reason, "ParentA" -> pa, "ParentB" -> pb,
                "Lhs" -> decodeAtpTerm[l, labelToName, idToName],
                "Rhs" -> decodeAtpTerm[r, labelToName, idToName],
                "Pos" -> (pos + 1)
            |>
        ],
        {nTrace}
    ];
    (* MAIN steps block. *)
    {mainSteps, cur} =
        decodeStepsBlock[raw, cur, nSteps, labelToName, idToName];
    (* EXT rules block (2*extNRules) -- skipped, the simple path
       re-derives rules from the axiom list. *)
    cur = cur + 2 extNRules;
    (* EXT steps block. *)
    {extSteps, cur} =
        decodeStepsBlock[raw, cur, extNSteps, labelToName, idToName];
    <|
        "Status" -> status,
        "ExtSteps" -> If[ extNSteps === 0, {}, extSteps],
        "MainSteps" -> If[ nSteps === 0, {}, mainSteps],
        "MainRules" -> mainRules,
        "RTrace" -> rTrace,
        "Trace" -> traceEntries
    |>
]

(* Identify which buildRuleList entry reproduces one rewrite step:
   rewriting `before` at `relPos` with the entry's Rule must yield
   `after`.  Returns the 1-based ruleList index, or Missing[] for a
   step that used a derived (completion) rule no axiom reproduces. *)
identifyRule[before_, relPos_, after_, ruleList_] := Block[{idx},
    idx = FirstPosition[ruleList,
        re_ /; Quiet[ReplaceAt[before, re["Rule"], relPos]] === after,
        Missing[], {1}, Heads -> False];
    If[ MissingQ[idx], Missing[], First[idx] ]
]

(* Turn the C engine's decoded step list into assembleDataset's
   `chain` shape: reconstruct the running Inactive[Equal][L, R],
   the absolute rewrite position, and the citing rule.  Returns
   $Failed if any step used a rule no axiom reproduces (a
   completion-derived rule -- not expressible in the axiom-citing
   dataset; that proof needs the critical-pair lemma DAG). *)
buildCEngineChain[steps_, conjPair_, ruleList_] := Catch[
    Block[{lhs, rhs, chain = {}},
        {lhs, rhs} = conjPair;
        Do[
            Block[{
                s = step["Side"], relPos = step["PosPath"],
                before = step["Before"], after = step["After"],
                ruleIx, newL, newR
            },
                ruleIx = identifyRule[before, relPos, after, ruleList];
                If[ MissingQ[ruleIx], Throw[$Failed] ];
                If[ s === 0,
                    newL = after; newR = rhs,
                    newL = lhs; newR = after
                ];
                AppendTo[chain, <|
                    "NewExpr" -> Inactive[Equal][newL, newR],
                    "Position" -> Prepend[relPos, s + 1],
                    "RuleIdx" -> ruleIx,
                    "Rule" -> ruleList[[ruleIx]]["Rule"]
                |>];
                lhs = newL; rhs = newR;
            ],
            {step, steps}
        ];
        chain
    ]
]

(* === critical-pair lemma DAG ====================================== *)

(* Trace-entry reasons (src/thvm.h): an input / re-queued equation,
   a CP oriented into a rule, a critical pair. *)
$TraceAxiom = 1;
$TraceOrient = 2;
$TraceCp = 3;
$AtpTraceNone = 4294967295;

(* Assemble a verifier-shaped ProofObject dataset for a
   completion-derived proof, walking the MAIN-state trace DAG the C
   glue ships (cEngineProof's MainSteps / MainRules / RTrace /
   Trace fields).  Emits Axiom entries for TRACE_AXIOM lineage,
   CriticalPairLemma entries for TRACE_CP lineage, and
   SubstitutionLemma steps for the rewrite chain.

   BLOCKER (reported, not worked around): thvm_atp_interreduce
   re-queues a simplified older rule through thvm_atp_add_equation,
   which records a fresh TRACE_AXIOM entry -- severing the
   derivation lineage of every rule born from such a re-queued
   equation.  DoubleNegation's closing rule descends from one of
   these severed TRACE_AXIOM entries (its content is the conjecture
   itself), so the trace DAG holds no valid derivation for it.  A
   verifier-passing CriticalPairLemma DAG is therefore not
   constructible until the C engine records interreduction as a
   reduction chain (a TRACE_SIMPLIFY entry carrying the parent
   rule's trace + the rewrite geometry), the way Waldmeister's PCL
   `Reduktion` events do.  See the handoff report.  Until then this
   returns $Failed so a completion-only goal degrades gracefully
   rather than emitting an unsound (self-citing) proof. *)
buildCplDataset[enc_, conjPair_, cRes_] := $Failed

(* === TFindEquationalProof ========================================= *)

(* Render a held expression in the form WL's ProofObject expects
   for its top-level Axioms list / ConjectureStatement: keep the
   ForAll wrapper if present, but rewrite every nested `Equal[lhs,
   rhs]` to `Inactive[Equal][lhs, rhs]` so trivial tautology axioms
   `a == a` don't collapse to True under ReleaseHold. *)
holdToInactive[axHC_HoldComplete] :=
    ReleaseHold[axHC /. Equal -> Inactive[Equal]]

Options[TFindEquationalProof] = {MaxSteps -> 200000};

(* String form: resolve theorem + theory names through
   AxiomaticTheory, then run the expression form.  The conjecture
   is the named NotableTheorem; the axioms are the theory's axiom
   list.  unquantifyFormula / CanonicalizePatterns normalize the
   quantified formulas (ForAll -> Pattern, Exists -> Skolem, then
   canonical variable names). *)
TFindEquationalProof[thm_String, theory_String, opts:OptionsPattern[]] := Catch[
    Block[{axRaw, cjRaw, axioms, conjecture},
        axRaw = AxiomaticTheory[theory];
        cjRaw = AxiomaticTheory[theory, "NotableTheorems"][thm];
        If[ ! ListQ[axRaw],
            Throw[Failure["TATPParseError",
                <|"Reason" -> "AxiomaticTheory[\"" <> theory <>
                    "\"] did not resolve to an axiom list"|>],
                "TATPError"]
        ];
        If[ MissingQ[cjRaw],
            Throw[Failure["TATPParseError",
                <|"Reason" -> "theorem \"" <> thm <>
                    "\" not in AxiomaticTheory[\"" <> theory <>
                    "\", \"NotableTheorems\"]"|>],
                "TATPError"]
        ];
        (* A NotableTheorem resolves to a one-element list holding
           the theorem formula. *)
        cjRaw = If[ ListQ[cjRaw] && Length[cjRaw] === 1,
            First[cjRaw], cjRaw];
        axioms = CanonicalizePatterns /@ (unquantifyFormula /@ axRaw);
        conjecture = CanonicalizePatterns @ unquantifyFormula @ cjRaw;
        TFindEquationalProof[conjecture, axioms, opts]
    ],
    "TATPError"
]

(* Expression form: run thvm's C ATP completion engine on the
   conjecture + axioms, decode the equational rewrite chain, and
   wrap it in a verifier-shaped WL ProofObject.  Returns $Failed
   when the goal is not proved (or the proof is not expressible in
   the axiom-citing dataset -- a completion-derived chain).

   atpEncodeProblem validates axiom/conjecture shape and surfaces
   the encoder state (the Variables list + the Term decoder maps). *)
TFindEquationalProof[conjecture_, axioms_List, OptionsPattern[]] := Catch[
    Block[{
        enc, conjPair, axiomKeys, ruleList, cRes, extSteps,
        chain, dataset, varNames, axEq, conjStmt
    },
        enc = atpEncodeProblem[axioms, conjecture];
        conjPair = enc["ConjPair"];
        axiomKeys = Table[{$AxiomSym, k}, {k, Length[enc["AxPairs"]]}];
        ruleList = buildRuleList[enc["AxPairs"], axiomKeys];
        cRes = cEngineProof[enc, OptionValue[MaxSteps]];
        (* status 1 == PROVED. *)
        If[ cRes["Status"] =!= 1, Return[$Failed] ];
        extSteps = cRes["ExtSteps"];
        (* Preferred path: the no-completion EXT chain cites the
           input axioms directly, so assembleDataset's axiom-cited
           SubstitutionLemma / Conclusion entries verify. *)
        dataset = $Failed;
        If[ extSteps =!= $Failed,
            Which[
                extSteps === {},
                    If[ conjPair[[1]] === conjPair[[2]],
                        dataset = assembleDataset[enc["AxPairs"],
                            conjPair, {}, ruleList]
                    ],
                True,
                    chain = buildCEngineChain[extSteps, conjPair, ruleList];
                    If[ chain =!= $Failed,
                        dataset = assembleDataset[enc["AxPairs"],
                            conjPair, chain, ruleList]
                    ]
            ]
        ];
        (* Fallback: the EXT chain could not close (or could not be
           expressed over the axioms) -- assemble the critical-pair
           lemma DAG from the completion trace. *)
        If[ dataset === $Failed,
            dataset = buildCplDataset[enc, conjPair, cRes]
        ];
        If[ dataset === $Failed, Return[$Failed] ];
        varNames = Symbol /@ Keys[enc["State"]["var"]];
        axEq = holdToInactive /@ enc["AxHCsRaw"];
        conjStmt = holdToInactive[enc["ConjHCRaw"]];
        ProofObject[
            "EquationalLogic",
            conjStmt,
            axEq,
            <|
                "Variables" -> varNames,
                "Constants" -> {},
                "Proof" -> dataset
            |>
        ]
    ],
    "TATPError"
]

End[];
EndPackage[];
