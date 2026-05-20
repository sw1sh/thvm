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

TFindEquationalProof::usage = "TFindEquationalProof[conjecture, axioms] runs thvm's C ATP completion engine and returns a real WL ProofObject -- the same head FindEquationalProof returns, supporting the full property interface (p[\"ProofDataset\"], p[\"ProofGraph\"], p[\"ProofFunction\"], p[\"ProofLength\"], etc.).  TFindEquationalProof[\"Theorem\", \"Theory\"] resolves the theorem and theory names through AxiomaticTheory; a theorem stated as a multi-equation conjunction (an n-element list, e.g. BooleanAxioms `DeMorgan`) returns a List of n ProofObjects, one per conjunct.  The C engine saturates the axioms; the resulting equational rewrite chain is decoded into a verifier-shaped ProofObject.  Returns $Failed when the conjecture is not proved.";

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

(* Diagnostic: when True, every Throw[$Failed] inside the ProofObject
   dataset assembly (buildCplDataset / buildCEngineChain / cplOrient)
   logs a tag to stderr first.  Off by default; flip from a probe to
   pinpoint which assembly invariant fails on a given trace. *)
$AtpDebugDataset = False;
atpDbgFail[tag_] := If[ TrueQ[$AtpDebugDataset],
    WriteString["stderr", "atp-fail @ ", tag, "\n"]];

(* When True (default), resolveTrace emits a SubstitutionLemma per
   TRACE_NORM_STEP -- linear chain extraction.  When False,
   NORM_STEP entries are transparent (pass through their parent's
   resolved info), so resolveTrace's ORIENT/SIMPLIFY branch reaches
   the CP directly and the older emitNorm BFS bridges the chain.
   TFindEquationalProof flips this for a fallback retry when chain
   extraction produces a Statement the verifier rejects (e.g. the
   Boolean ⊕ Orderless interaction on DeMorgan). *)
$AtpUseChain = True;

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

(* A numeric literal (e.g. the `1` in OverTilde[1], the identity-
   element marker AbelianGroup / McCune / Tarski axiom sets use) is a
   0-arity constant: encode it by value.  Without this rule the
   general clause below folds over List @@ n -- a non-list for an
   atom -- and the encoder diverges. *)
encodeAtpTerm[n:(_Integer | _Real | _Rational), state_Association] := Block[{
    sym = ToString[n, InputForm],
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
atpEncodeProblem[axioms_, conjecture_] :=
    atpEncodeProblem[axioms, conjecture, False];
atpEncodeProblem[axioms_, conjecture_, skolemize_] := Block[{
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
    (* Skolemize: a universal conjecture is proved for an arbitrary
       fixed instance, so strip the bound variables' Pattern wrappers
       to bare constants.  KBO totally orders constants, so an
       unorientable equation (commutativity-style) becomes ordered-
       applicable to the goal -- the single-NF check then closes a
       symmetric goal the variable-keyed goal could not.  Done inside
       HoldComplete so a reflexive `f[x] == f[x]` does not collapse to
       True before encoding. *)
    If[ skolemize, cjHC = cjHC /. Verbatim[Pattern][v_, _] :> v ];
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
        "ConjHCRaw" -> If[skolemize, cjHC, HoldComplete[conjecture]]
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
$ProofKeyOrder[{"CriticalPairLemma", k_}] := {3, k}
$ProofKeyOrder[{"SubstitutionLemma", k_}] := {4, k}
$ProofKeyOrder[{"Conclusion", k_}] := {5, k}
$ProofKeyOrder[_] := {6, 0}

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
                    (* a numeric-literal constant round-trips back to
                       its value; every other 0-arity label is a
                       symbol. *)
                    If[ StringMatchQ[name, NumberString],
                        ToExpression[name], Symbol[name]],
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
        Block[{side, ruleIx, fwd, posLen, posPath, beforeRaw, afterRaw},
            side = raw[[cur + 1]];
            ruleIx = raw[[cur + 2]];
            fwd = raw[[cur + 3]];
            posLen = raw[[cur + 4]];
            posPath = If[ posLen === 0, {},
                raw[[cur + 5 ;; cur + 4 + posLen]]];
            beforeRaw = raw[[cur + 5 + posLen]];
            afterRaw = raw[[cur + 6 + posLen]];
            cur = cur + 6 + posLen;
            <|
                "Side" -> side,
                "RuleC" -> ruleIx,
                (* C engine's proof_extract records 1 for an lhs->rhs
                   rewrite, 0 for an unorientable equation fired
                   rhs->lhs by ordered rewriting.  The goal chain emit
                   reads this to flip the SubstitutionLemma's
                   Orientation when a step used the rule reversed. *)
                "Fwd" -> fwd,
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
       pos_len, pos[..].  TRACE_NORM_STEP carries two extra NUMs
       (side, fwd) after pos[] so the WL extractor can re-emit the
       SubstitutionLemma directly from the C engine's recorded chain
       instead of re-deriving the rewrite path by search. *)
    traceEntries = Table[
        Block[{reason, pa, pb, l, r, posLen, pos, side, fwd},
            reason = raw[[cur + 1]]; pa = raw[[cur + 2]];
            pb = raw[[cur + 3]]; l = raw[[cur + 4]];
            r = raw[[cur + 5]]; posLen = raw[[cur + 6]];
            pos = If[ posLen === 0, {},
                raw[[cur + 7 ;; cur + 6 + posLen]]];
            cur = cur + 6 + posLen;
            side = If[ reason === $TraceNormStep, raw[[cur + 1]], 0];
            fwd  = If[ reason === $TraceNormStep, raw[[cur + 2]], 1];
            If[ reason === $TraceNormStep, cur = cur + 2 ];
            <|
                "Reason" -> reason, "ParentA" -> pa, "ParentB" -> pb,
                "Lhs" -> decodeAtpTerm[l, labelToName, idToName],
                "Rhs" -> decodeAtpTerm[r, labelToName, idToName],
                "Pos" -> (pos + 1),
                "Side" -> side, "Fwd" -> fwd
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
        "Trace" -> traceEntries,
        (* every variable symbol the decode produced: the named
           encoder vars plus any "x<id>" fallbacks for FVR ids
           completion introduced past the original signature.  Both
           the dataset builder and the ProofObject "Variables" list
           need the complete set, or the verifier reads a stray
           completion variable as a constant. *)
        "VarSyms" -> Union[
            Symbol /@ Values[idToName],
            Cases[{traceEntries, mainRules, mainSteps, extSteps},
                s_Symbol /; StringMatchQ[SymbolName[s],
                    "x" ~~ DigitCharacter ..],
                {0, Infinity}]
        ]
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
                If[ MissingQ[ruleIx],
                    atpDbgFail["buildCEngineChain.identifyRule"]; Throw[$Failed]];
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
   a CP oriented into a rule, a critical pair, an interreduce
   re-queue carrying the dropped rule's lineage. *)
$TraceAxiom = 1;
$TraceOrient = 2;
$TraceCp = 3;
$TraceSimplify = 4;
$TraceNormStep = 5;
$AtpTraceNone = 4294967295;

(* Assemble a verifier-shaped ProofObject dataset for a
   completion-derived proof, walking the MAIN-state trace DAG the C
   glue ships (cEngineProof's MainSteps / MainRules / RTrace /
   Trace fields).

   The trace DAG is now fully connected: TRACE_SIMPLIFY (added with
   the C engine's interreduce-lineage fix) keeps every re-queued
   rule parented on the rule it descended from, so DoubleNegation's
   closing rule traces back through CP / ORIENT / SIMPLIFY nodes to
   the single input axiom.

   The remaining work to emit a verifier-passing dataset, fully
   de-risked against the kernel (a hand-rebuilt FindEquationalProof
   dataset verifies; the CriticalPairLemma field semantics are
   known):
     - TRACE_AXIOM   -> an "Axiom" entry.
     - TRACE_CP      -> a "CriticalPairLemma" entry: Construct /
                        MatchingConstruct are the two parent rules'
                        dataset keys, Position is the recorded
                        superposition path, Subpattern is the
                        Construct rule's lhs at that position,
                        Statement is the raw CP.
     - TRACE_ORIENT / TRACE_SIMPLIFY -> the parent equation
                        normalized to the rule: a "SubstitutionLemma"
                        chain, each step's rewrite re-derived by
                        replaying the rule set (the trace does not
                        store the normalization steps themselves).
     - the goal chain (MainSteps) -> "SubstitutionLemma" / final
                        "Conclusion" entries.
   Each Construct / MatchingConstruct / Input cites another entry's
   key; thvm rules map to keys via RTrace.  Variable-bearing rules
   need their FVRs rendered as Pattern[v, Blank[]] on rule lhs's.

   A goal whose lineage needs a TRACE_CP (genuine superposition)
   throws to $Failed for now -- the CriticalPairLemma branch is the
   next increment; the TRACE_ORIENT / TRACE_SIMPLIFY normalization
   path below already covers completion proofs whose derived rules
   come from rule normalization alone. *)

(* Rename the first-order variables (members of varSyms) of `expr`
   to canonical names in first-occurrence order, so two alpha-
   equivalent terms compare equal under SameQ.  Trace entries decode
   FVRs to per-entry symbol names, so equations from different rules
   are alpha-variants that must be compared up to renaming.
   Memoized: a large completion trace (Meredith ~28k) calls cplEqSetQ
   thousands of times on the same equation shapes; the unmemoized
   Cases scan plus ReplaceAll per call was the dominant cost. *)
cplCanonVars[expr_, varSyms_] := cplCanonVars[expr, varSyms] =
    Block[{occ},
        occ = DeleteDuplicates @ Cases[expr,
            v_Symbol /; MemberQ[varSyms, v], {0, Infinity},
            Heads -> True];
        expr /. MapThread[Rule,
            {occ, Table[Symbol["cplV" <> ToString[i]],
                {i, Length[occ]}]}]
    ]

(* True iff the two equation lists are equal up to a side swap and
   variable renaming. *)
cplEqSetQ[a_List, b_List, varSyms_] := With[{
    ca = cplCanonVars[a, varSyms]
},
    ca === cplCanonVars[b, varSyms] ||
    ca === cplCanonVars[Reverse[b], varSyms]
]

(* The set of first-order variables (members of varSyms) occurring in
   `expr`.  Used to gate reverse-direction rewriting on whether the
   rule's lhs vars are a subset of its rhs vars -- the same
   atp_vars_contained guard ordered rewriting uses in the C engine. *)
cplVarsIn[expr_, varSyms_] := DeleteDuplicates @ Cases[expr,
    v_Symbol /; MemberQ[varSyms, v], {0, Infinity}, Heads -> True]

(* Rewrite each first-order variable occurrence (members of
   varSyms) in `term` as Pattern[v, Blank[]].  The var -> var_ rules
   are built with MapThread so no Pattern lands on a RuleDelayed
   rhs.  The verifier computes a step's expected result from the
   cited rules' patterned lhs's, so every dataset Statement that
   carries variables must be patternized to match. *)
cplPatternize[term_, varSyms_] := term /. MapThread[Rule,
    {varSyms, Map[Pattern[#, Blank[]] &, varSyms]}]

(* A completion rule {lhs, rhs} as a WL rewrite rule, with the lhs
   variables patternized so the rule actually fires. *)
cplAsRule[eq_List, varSyms_] :=
    cplPatternize[eq[[1]], varSyms] -> eq[[2]]

(* Compare an entry's Statement equation to a rule's {lhs, rhs}
   (up to variable renaming): +1 when they agree, -1 when reversed.
   This is the Orientation the verifier needs to read a cited
   entry's Statement as the rule the step actually applied. *)
cplOrient[entryEq_, ruleEq_, varSyms_] := With[{
    ce = cplCanonVars[entryEq, varSyms]
},
    Which[
        ce === cplCanonVars[ruleEq, varSyms], 1,
        ce === cplCanonVars[Reverse[ruleEq], varSyms], -1,
        True, atpDbgFail["cplOrient.no-match"]; Throw[$Failed]
    ]
]

(* Assemble a verifier-shaped ProofObject dataset from the MAIN
   completion trace DAG.  Resolution is keyed on trace index: every
   TRACE_AXIOM becomes an Axiom entry, every TRACE_CP a Critical-
   PairLemma (Construct / MatchingConstruct = the two parent rules'
   entries, Position / Subpattern = the recorded superposition), and
   every TRACE_ORIENT / TRACE_SIMPLIFY whose rule differs from its
   parent equation a chain of SubstitutionLemma entries that replays
   the normalization.  The goal chain (MainSteps) becomes the closing
   SubstitutionLemma / Conclusion entries.  Returns $Failed if any
   lineage cannot be assembled -- callers degrade to $Failed, never
   to an unsound proof. *)
buildCplDataset[enc_, conjPair_, cRes_] := Catch[
    Block[{$RecursionLimit = 100000, $IterationLimit = 1000000},
    Module[{
        trace = cRes["Trace"], mainRules = cRes["MainRules"],
        rTrace = cRes["RTrace"], mainSteps = cRes["MainSteps"],
        axPairs = enc["AxPairs"], varSyms, entries, traceInfo,
        inProgress, aliveRulesAt, slN, cpN, axiomKeyFor, rewriteOnce,
        emitNorm, resolveCp, resolveTrace, resolveRule, axiomEntries,
        cjp, hypKey, chainEntries, runEq, prevChainKey, allEntries,
        stmt
    },
        varSyms = cRes["VarSyms"];
        (* the trace-decoded terms carry bare variable symbols;
           atpEncodeProblem's AxPairs / ConjPair carry Pattern[v, _]
           wrappers -- strip them so every comparison is bare-to-bare
           (cplAsRule re-patternizes when building rewrite rules). *)
        axPairs = axPairs /. Verbatim[Pattern][s_Symbol, _] :> s;
        cjp = conjPair /. Verbatim[Pattern][s_Symbol, _] :> s;
        entries = {};         (* lemma key -> assoc, accumulated *)
        (* traceInfo / inProgress use DownValues on the Module-renamed
           symbols (not Associations): single-arg DownValues are hashed,
           so cache lookup stays O(1) on large traces.  A Module-renamed
           symbol's DownValues vanish when the Module exits, so the next
           buildCplDataset call starts with a clean slate. *)
        Clear[traceInfo, inProgress];
        slN = 0; cpN = 0;
        (* a dataset Statement: the equation in the verifier's
           HoldForm[Equal[...]] shape, variables left bare (only the
           Proof Rule fields are patternized). *)
        stmt[eq_] := toHoldEq[Inactive[Equal] @@ eq];

        (* The rules alive when the rule at trace index `ti` was
           oriented: every TRACE_ORIENT born before ti, minus the
           ones interreduction dropped before ti (a TRACE_SIMPLIFY's
           ParentA names the rule it dropped).  A CP -> rule
           normalization must replay against THIS set, not the final
           rule set -- completion interreduces rules away, so the
           final set may lack a rule the normalization needed.
           Returns {{traceIdx, ruleEq}, ...}. *)
        aliveRulesAt[ti_] := Block[{dropped},
            dropped = Cases[Range[0, ti - 1],
                s_ /; trace[[s + 1]]["Reason"] === $TraceSimplify :>
                    trace[[s + 1]]["ParentA"]];
            Table[
                {t, {trace[[t + 1]]["Lhs"], trace[[t + 1]]["Rhs"]}},
                {t, Select[Range[0, ti - 1],
                    trace[[# + 1]]["Reason"] === $TraceOrient &&
                    ! MemberQ[dropped, #] &]}
            ]
        ];

        (* axiom key whose {lhs,rhs} matches eq up to a side swap *)
        axiomKeyFor[eq_] := Block[{
            i = FirstPosition[axPairs, p_ /; cplEqSetQ[p, eq, varSyms],
                Missing[], {1}, Heads -> False]
        },
            If[ MissingQ[i],
                atpDbgFail["axiomKeyFor.missing"]; Throw[$Failed],
                {$AxiomSym, First[i]}]
        ];

        (* one-step rewrites of equation `eq` by the alive rules
           (each {traceIdx, ruleEq}); returns {newEq, traceIdx,
           side, relPos, dir} tuples where dir is +1 for an lhs->rhs
           application of the rule and -1 for rhs->lhs.  Reverse-
           direction is tried only when tryReverse is True AND the
           rule is variable-safe (vars(lhs) subset-of vars(rhs)) --
           the same atp_vars_contained guard ordered rewriting uses
           in the C engine, so a completion-oriented rule -- whose
           reverse would introduce unbound variables -- never fires
           reversed. *)
        rewriteOnce[eq_, aliveList_, tryReverse_] := Block[{out = {}},
            Do[
                Block[{eqA = ar[[2]],
                       rlF = cplAsRule[ar[[2]], varSyms],
                       rlR, revSafe, sub, new},
                    revSafe = tryReverse && SubsetQ[
                        cplVarsIn[eqA[[2]], varSyms],
                        cplVarsIn[eqA[[1]], varSyms]];
                    rlR = If[ revSafe,
                        cplAsRule[Reverse[eqA], varSyms], Null];
                    Do[
                        sub = eq[[side + 1]];
                        Do[
                            new = Quiet @ ReplaceAt[sub, rlF, pos];
                            If[ new =!= sub && FreeQ[new, ReplaceAt],
                                AppendTo[out, {
                                    ReplacePart[eq, side + 1 -> new],
                                    ar[[1]], side, pos, 1}]
                            ],
                            {pos, Position[sub, rlF[[1]],
                                {0, Infinity}, Heads -> False]}
                        ];
                        If[ revSafe,
                            Do[
                                new = Quiet @ ReplaceAt[sub, rlR, pos];
                                If[ new =!= sub && FreeQ[new, ReplaceAt],
                                    AppendTo[out, {
                                        ReplacePart[eq, side + 1 -> new],
                                        ar[[1]], side, pos, -1}]
                                ],
                                {pos, Position[sub, rlR[[1]],
                                    {0, Infinity}, Heads -> False]}
                            ]
                        ],
                        {side, 0, 1}
                    ]
                ],
                {ar, aliveList}
            ];
            out
        ];

        (* BFS one phase: explore from `start` looking for a rewrite
           path to anything cplEqSetQ-equal to `target`.  Returns the
           hist on success, Missing[] on cap exhaustion or queue
           emptiness. *)
        runBfs[start_, target_, aliveList_, tryReverse_, cap_] :=
            Block[{queue, seenA, found = Missing[], explored = 0, nbrs},
                queue = {{start, {}}};
                seenA = <|start -> True|>;
                While[ queue =!= {} && MissingQ[found] && explored < cap,
                    Block[{node = First[queue], eq, hist},
                        queue = Rest[queue];
                        explored++;
                        {eq, hist} = node;
                        If[ cplEqSetQ[eq, target, varSyms],
                            found = hist,
                            nbrs = rewriteOnce[eq, aliveList, tryReverse];
                            Do[
                                If[ ! KeyExistsQ[seenA, nb[[1]]],
                                    AssociateTo[seenA, nb[[1]] -> True];
                                    AppendTo[queue,
                                        {nb[[1]], Append[hist, nb]}]
                                ],
                                {nb, nbrs}
                            ]
                        ]
                    ]
                ];
                found
            ];

        (* re-derive startEq ->* targetEq, replaying with the rules
           alive when the rule at trace `ti` was oriented; emit a
           SubstitutionLemma per step; return the final entry's
           {key, eq}.  inKey is the dataset key of startEq. *)
        emitNorm[inKey_, startEq_, targetEq_, ti_] :=
            Block[{aliveList, found, curKey, curEq, st, cInfo, rEq},
                aliveList = aliveRulesAt[ti];
                (* Two-phase BFS.  Phase 1 is forward-only with a large
                   cap -- preserves the pre-reverse behavior on every
                   case the original engine handled (no behavior change
                   to byte-identical proofs).  Phase 2 only runs when
                   Phase 1 exhausts: it re-explores with reverse
                   direction enabled (variable-safe rules only) and a
                   tight cap, which unlocks ordered-rewriting paths
                   the forward-only BFS could not reach. *)
                found = runBfs[startEq, targetEq, aliveList, False, 50000];
                If[ MissingQ[found],
                    found = runBfs[startEq, targetEq, aliveList, True, 600]];
                If[ MissingQ[found],
                    atpDbgFail["emitNorm.no-rewrite-path"]; Throw[$Failed]];
                curKey = inKey;
                curEq = startEq;
                Do[
                    cInfo = resolveTrace[step[[2]]];
                    rEq = {trace[[step[[2]] + 1]]["Lhs"],
                           trace[[step[[2]] + 1]]["Rhs"]};
                    slN++;
                    st = stmt[step[[1]]];
                    AppendTo[entries, {$SubstitutionLemmaSym, slN} -> <|
                        "Statement" -> st,
                        "Proof" -> <|
                            "Input" -> curKey,
                            "Construct" -> cInfo["Key"],
                            "Position" -> step[[4]],
                            (* step[[5]] is +1 for an lhs->rhs rewrite,
                               -1 for the reverse direction taken by an
                               unorientable equation; combined with the
                               entry-vs-rule alignment, this is the
                               Orientation the verifier needs to read
                               the cited Statement the same way. *)
                            "Rule" -> cplAsRule[
                                If[ step[[5]] === -1,
                                    Reverse[rEq], rEq], varSyms],
                            "Orientation" -> step[[5]] *
                                cplOrient[cInfo["Eq"], rEq, varSyms],
                            "ConstructSide" -> 1,
                            "InputOrientation" -> 1,
                            "Side" -> step[[3]] + 1,
                            "OutputExpression" -> st,
                            "Source" -> "cpl"
                        |>
                    |>];
                    curKey = {$SubstitutionLemmaSym, slN};
                    curEq = step[[1]];
                    ,
                    {step, found}
                ];
                (* emitNorm's bridging chain inherits the source CP's
                   Swapped convention (the BFS is structural, not
                   convention-aware), so subsequent NORM_STEPs that
                   descend from this chain pick up the right side. *)
                <|"Key" -> curKey, "Eq" -> curEq, "Swapped" -> True|>
            ];

        (* emit a CriticalPairLemma for the TRACE_CP at trace index
           ti; return its <|Key, Eq|>. *)
        resolveCp[ti_] := Block[{
            cte, cpEq, pos, aTe, bTe, ruleAEq, ruleBEq, aInfo, bInfo,
            key, st
        },
            cte = trace[[ti + 1]];
            (* WL's verifier builds the CP as
               (Construct's non-overlap side, overlap side rewritten)
               -- the reverse of thvm's recorded (rewritten-lhs, rhs)
               critical pair -- so the CriticalPairLemma Statement
               takes the sides swapped. *)
            cpEq = {cte["Rhs"], cte["Lhs"]};
            pos = cte["Pos"];
            aTe = trace[[cte["ParentA"] + 1]];
            bTe = trace[[cte["ParentB"] + 1]];
            ruleAEq = {aTe["Lhs"], aTe["Rhs"]};
            ruleBEq = {bTe["Lhs"], bTe["Rhs"]};
            aInfo = resolveTrace[cte["ParentA"]];
            bInfo = resolveTrace[cte["ParentB"]];
            cpN++;
            key = {"CriticalPairLemma", cpN};
            st = stmt[cpEq];
            AppendTo[entries, key -> <|
                "Statement" -> st,
                "Proof" -> <|
                    "Construct" -> aInfo["Key"],
                    "Orientation" ->
                        cplOrient[aInfo["Eq"], ruleAEq, varSyms],
                    "Rule" -> cplAsRule[ruleAEq, varSyms],
                    "Side" -> 1,
                    "Subpattern" -> Extract[
                        cplAsRule[ruleAEq, varSyms][[1]], pos],
                    "MatchingConstruct" -> bInfo["Key"],
                    "MatchingOrientation" ->
                        cplOrient[bInfo["Eq"], ruleBEq, varSyms],
                    "MatchingRule" -> cplAsRule[ruleBEq, varSyms],
                    "MatchingSide" -> 1,
                    "Position" -> pos
                |>
            |>];
            <|"Key" -> key, "Eq" -> cpEq, "Swapped" -> True|>
        ];

        (* resolve trace index ti to <|Key, Eq, Swapped|>, emitting
           the lemma entries its derivation needs; memoized, cycle-
           guarded.  `Swapped` tracks whether the entry's Eq is in
           the verifier's (non-overlap, overlap-rewritten) CP order
           (True) or the C engine's native (lhs, rhs) order (False).
           NORM_STEP propagates this through the chain so its Side /
           equation order match the chain root's convention. *)
        resolveTrace[ti_] := Block[{te, ruleEq, pInfo, pEq, info},
            (* Cache check: an unset DownValue leaves traceInfo[ti]
               unevaluated (Head == the symbol traceInfo itself).
               A set DownValue evaluates to its stored Association,
               whose Head is Association, not traceInfo. *)
            If[ Head[traceInfo[ti]] =!= traceInfo,
                Return[traceInfo[ti]] ];
            If[ TrueQ[inProgress[ti]],
                atpDbgFail["resolveTrace.cycle@" <> ToString[ti]];
                Throw[$Failed]];
            inProgress[ti] = True;
            te = trace[[ti + 1]];
            ruleEq = {te["Lhs"], te["Rhs"]};
            info = Which[
                te["Reason"] === $TraceCp,
                    resolveCp[ti],
                te["Reason"] === $TraceAxiom,
                    <|"Key" -> axiomKeyFor[ruleEq], "Eq" -> ruleEq,
                      "Swapped" -> False|>,
                te["Reason"] === $TraceNormStep && ! TrueQ[$AtpUseChain],
                    (* Chain-off retry path: NORM_STEP entries pass
                       through transparently so ORIENT/SIMPLIFY land
                       on the CP via the chain and emitNorm bridges
                       via BFS, recovering the pre-chain extraction
                       behavior verbatim. *)
                    resolveTrace[te["ParentA"]],
                te["Reason"] === $TraceNormStep,
                    (* The C engine recorded this rewrite step
                       directly: emit one SubstitutionLemma citing the
                       rule the engine used, no search.  ParentA is
                       the previous chain link (NORM_STEP, CP, or
                       AXIOM); ParentB is the rule's TRACE_ORIENT
                       trace index (interreduction-stable).
                       The C engine records te.Lhs / te.Rhs and side
                       in its own (lhs / rhs) convention.  resolveCp
                       swaps a CP so CriticalPairLemma's Statement
                       matches the verifier's (non-overlap, overlap-
                       rewritten) order; an AXIOM is in C-engine
                       order unchanged.  Detect the parent's
                       convention by comparing pInfo's Eq to its
                       trace entry's stored Lhs / Rhs, and mirror the
                       swap (or not) on this NORM_STEP so the chain
                       stays in ONE convention end-to-end. *)
                    Module[{pInfo, rInfo, rTe, rEq, swapped,
                            wlEq, wlSide, sl, st, dir},
                        pInfo = resolveTrace[te["ParentA"]];
                        rInfo = resolveTrace[te["ParentB"]];
                        rTe = trace[[te["ParentB"] + 1]];
                        rEq = {rTe["Lhs"], rTe["Rhs"]};
                        swapped = TrueQ[pInfo["Swapped"]];
                        wlEq = If[ swapped,
                            {te["Rhs"], te["Lhs"]},
                            {te["Lhs"], te["Rhs"]}];
                        wlSide = If[ swapped,
                            If[ te["Side"] === 0, 2, 1],
                            te["Side"] + 1];
                        slN++;
                        sl = {$SubstitutionLemmaSym, slN};
                        st = stmt[wlEq];
                        dir = If[ te["Fwd"] === 1, 1, -1];
                        AppendTo[entries, sl -> <|
                            "Statement" -> st,
                            "Proof" -> <|
                                "Input" -> pInfo["Key"],
                                "Construct" -> rInfo["Key"],
                                "Position" -> te["Pos"],
                                "Rule" -> cplAsRule[
                                    If[ dir === -1, Reverse[rEq], rEq],
                                    varSyms],
                                "Orientation" -> dir *
                                    cplOrient[rInfo["Eq"], rEq, varSyms],
                                "ConstructSide" -> 1,
                                "InputOrientation" -> 1,
                                "Side" -> wlSide,
                                "OutputExpression" -> st,
                                "Source" -> "norm"
                            |>
                        |>];
                        <|"Key" -> sl, "Eq" -> wlEq, "Swapped" -> swapped|>
                    ],
                te["Reason"] === $TraceOrient ||
                  te["Reason"] === $TraceSimplify,
                    pInfo = resolveTrace[te["ParentA"]];
                    pEq = pInfo["Eq"];
                    (* When chain extraction is on AND the parent is a
                       NORM_STEP, the chain already terminates at the
                       equation that gets oriented -- the cplEqSetQ
                       check is redundant and dominates cost on large
                       traces.  Inherit directly.  When chain is off,
                       fall through to cplEqSetQ / emitNorm so the
                       BFS bridges the rule's normalization. *)
                    If[ (TrueQ[$AtpUseChain] &&
                          trace[[te["ParentA"] + 1]]["Reason"] ===
                            $TraceNormStep)
                          || cplEqSetQ[ruleEq, pEq, varSyms],
                        <|"Key" -> pInfo["Key"], "Eq" -> pEq,
                          "Swapped" -> TrueQ[pInfo["Swapped"]]|>,
                        emitNorm[pInfo["Key"], pEq, ruleEq, ti]
                    ],
                True, atpDbgFail["resolveTrace.unknown-reason@" <> ToString[ti]];
                Throw[$Failed]
            ];
            inProgress[ti] = False;
            traceInfo[ti] = info;
            info
        ];

        (* resolve C rule index k via the trace index it was born at *)
        resolveRule[k_] := resolveTrace[rTrace[[k + 1]]];

        (* axiom entries *)
        axiomEntries = Table[
            {$AxiomSym, n} -> <|
                "Statement" -> stmt[axPairs[[n]]],
                "Proof" -> <||>
            |>,
            {n, Length[axPairs]}
        ];
        hypKey = {$HypothesisSym, 1};

        (* the goal chain: each MainStep rewrites one side of the
           running equation, citing its (resolved) rule. *)
        If[ mainSteps === {},
            atpDbgFail["buildCplDataset.empty-mainSteps"]; Throw[$Failed]];
        runEq = cjp;
        prevChainKey = hypKey;
        chainEntries = Table[
            Block[{step = mainSteps[[s]], cInfo, cKey, mr, dir, ori,
                   ruleEq, newEq, st, isLast, key, inKey},
                cInfo = resolveRule[step["RuleC"]];
                cKey = cInfo["Key"];
                mr = mainRules[[step["RuleC"] + 1]];
                (* Ordered rewriting in the C engine fires whichever
                   direction strictly decreases the redex.  Fwd = 1
                   for an lhs->rhs application, 0 for rhs->lhs.  The
                   verifier reads the Construct's Statement direction
                   determined by Orientation = step-direction * entry-
                   vs-rule alignment, so the SubstitutionLemma replays
                   the same rewrite. *)
                dir = If[ step["Fwd"] === 1, 1, -1];
                ori = dir * cplOrient[cInfo["Eq"], mr, varSyms];
                ruleEq = If[ dir === -1, Reverse[mr], mr];
                newEq = ReplacePart[runEq,
                    step["Side"] + 1 -> step["After"]];
                runEq = newEq;
                isLast = s === Length[mainSteps];
                key = If[ isLast, {$ConclusionSym, 1},
                    {$SubstitutionLemmaSym, ++slN}];
                inKey = prevChainKey;
                prevChainKey = key;
                st = stmt[newEq];
                key -> <|
                    "Statement" -> st,
                    "Proof" -> <|
                        "Input" -> inKey,
                        "Construct" -> cKey,
                        "Position" -> step["PosPath"],
                        "Rule" -> cplAsRule[ruleEq, varSyms],
                        "Orientation" -> ori,
                        "ConstructSide" -> 1,
                        "InputOrientation" -> 1,
                        "Side" -> step["Side"] + 1,
                        "OutputExpression" -> st,
                        "Source" -> "cpl"
                    |>
                |>
            ],
            {s, Length[mainSteps]}
        ];

        (* Order is axioms, hypothesis, then `entries` in emission
           order -- resolveTrace is depth-first, so a lemma is
           always emitted after the entries it cites -- then the
           goal chain.  The verifier replays entries in order and
           needs every Construct / Input already defined, so this
           dependency order must NOT be re-sorted. *)
        allEntries = Join[
            axiomEntries,
            {hypKey -> <|
                "Statement" -> stmt[cjp],
                "Proof" -> <||>
            |>},
            entries,
            chainEntries
        ];
        allEntries
    ]]
]

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
        axioms = CanonicalizePatterns /@ (unquantifyFormula /@ axRaw);
        (* A NotableTheorem resolves to a list of equation formulas:
           a one-element list holds a single theorem, a longer list
           is a conjunction (e.g. BooleanAxioms `DeMorgan` ships both
           DeMorgan equations).  Equational provability distributes
           over conjunction -- {eq1, ..., eqn} holds iff each eq_i
           does -- so prove each conjunct separately, returning a
           list of ProofObjects.  $Failed if any conjunct fails. *)
        Which[
            ! ListQ[cjRaw],
                TFindEquationalProof[
                    CanonicalizePatterns @ unquantifyFormula @ cjRaw,
                    axioms, opts],
            Length[cjRaw] === 1,
                TFindEquationalProof[
                    CanonicalizePatterns @ unquantifyFormula @ First[cjRaw],
                    axioms, opts],
            True,
                Module[{proofs},
                    proofs = Table[
                        TFindEquationalProof[
                            CanonicalizePatterns @ unquantifyFormula @ c,
                            axioms, opts],
                        {c, cjRaw}];
                    If[ AllTrue[proofs, Head[#] === ProofObject &],
                        proofs, $Failed]
                ]
        ]
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
        chain, dataset, varNames, axEq, conjStmt, po
    },
        enc = atpEncodeProblem[axioms, conjecture, True];
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
           lemma DAG from the completion trace.  Two-phase extraction:
             1. chain ON (TRACE_NORM_STEP entries become Substitution-
                Lemmas directly).
             2. chain OFF (NORM_STEP transparent, emitNorm BFS bridges
                the chain) -- recovers cases the chain-extracted
                Statement misses the verifier on (the Boolean ⊕
                Orderless interaction on DeMorgan, etc).
           Each attempt is built into a ProofObject and run through
           WL's verifier; only a verifying proof is returned. *)
        varNames = cRes["VarSyms"];
        axEq = holdToInactive /@ enc["AxHCsRaw"];
        conjStmt = holdToInactive[enc["ConjHCRaw"]];
        Module[{tryBuild, poA, poB},
            tryBuild[chainOn_, baseDataset_] := Module[{ds, p, v},
                ds = If[ baseDataset =!= $Failed, baseDataset,
                    Block[{$AtpUseChain = chainOn},
                        Quiet @ Check[
                            buildCplDataset[enc, conjPair, cRes],
                            $Failed]]];
                If[ ds === $Failed, $Failed,
                    p = ProofObject["EquationalLogic", conjStmt, axEq,
                        <|"Variables" -> varNames,
                          "Constants" -> {}, "Proof" -> ds|>];
                    v = Quiet @ Check[
                        p["ProofFunction"][p["Theorems"]], $Failed];
                    If[ Head[p] === ProofObject && Head[v] === Success,
                        p, $Failed]
                ]
            ];
            poA = tryBuild[True, dataset];
            If[ Head[poA] === ProofObject,
                poA,
                poB = tryBuild[False, $Failed];
                If[ Head[poB] === ProofObject, poB, $Failed]
            ]
        ]
    ],
    "TATPError"
]

End[];
EndPackage[];
