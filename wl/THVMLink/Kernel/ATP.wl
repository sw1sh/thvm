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
         Run the WL-side BFS chain synth and return a real WL
         ProofObject -- the same head FindEquationalProof returns,
         supporting the property interface (p["ProofDataset"],
         p["ProofGraph"], p["ProofFunction"], p["ProofLength"], etc.).
         Returns $Failed if the conjecture isn't reached within
         MaxSteps rewrites.  The C-side ATP saturator is no longer
         consulted (milestone 6 of the IC-native ATP arc).

   Options
     MaxSteps       (TATP, TFindEquationalProof) -> 64
     Witness        (TATP)                       -> {}    list of x_
     AllWitnesses   (TATP)                       -> False
     MaxDepth       (TATP / AllWitnesses)        -> 8
     MaxWitnesses   (TATP / AllWitnesses)        -> 16

   See docs/plans/waldmeister_ic_atp.md for the algorithmic intent. *)

BeginPackage["THVMLink`"];

TATP::usage = "TATP[{lhs == rhs, ...}, conjecture] runs the IC-native ATP saturation on the given equational axioms and conjecture, returning an Association with Status, Steps, Rules, QueueSize.  Variables are written as `x_` (Pattern[name, Blank[]]).  TATP[File[path]] parses a Waldmeister .pr file and runs the saturator directly.";

TFindEquationalProof::usage = "TFindEquationalProof[conjecture, axioms] runs the WL-side BFS chain synth and returns a real WL ProofObject -- the same head FindEquationalProof returns, supporting the full property interface (p[\"ProofDataset\"], p[\"ProofGraph\"], p[\"ProofFunction\"], p[\"ProofLength\"], etc.).  The 4th-arg Association is built to satisfy ProofObjectQ, which causes WL to skip its auto-dispatch back to FindEquationalProof and preserve thvm's data.  ProofDataset entries are keyed by {\"Axiom\" | \"Hypothesis\" | \"SubstitutionLemma\" | \"Conclusion\", k} with Statement and Proof sub-fields.  Returns $Failed when the BFS doesn't close the conjecture within its budget.";

(* Forward-declare symbols owned by sibling files (Switch.wl owns
   the IC term constructors) so bare references inside
   Begin[`Private`] resolve to THVMLink`X instead of a phantom
   THVMLink`Private`X.  ATP.wl is parsed first in alphabetical
   order, so these public names don't yet exist when this file's
   body runs.  Mirrors the same guard in Lazy.wl. *)
{TDef, TRef, TIfZero, TOp2, TNum, TSup, TApp, TLam,
 TCollapse, FromTTerm, TTerm};

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

(* Strip the outermost ForAll wrapper from a held equation, replacing
   each bound bare-symbol occurrence inside the body with
   Pattern[var, Blank[]].  Pass-through when there's no ForAll.  WL's
   FindEquationalProof accepts both `a == b` and
   `ForAll[x, lhs[x] == rhs[x]]`; we want the same surface.  Built
   as a Replace over the held form so the bound symbols never leak
   into the surrounding evaluation. *)
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
       Pattern[var, Blank[]].  Downstream pairing + BFS work
       uniformly on the stripped form. *)
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

(* === BFS chain synthesizer ======================================= *)

(* Term size used to break ties in BFS priority.  LeafCount counts
   atoms; smaller is "simpler" by KBO's first lexicographic key. *)
termSize[expr_] := LeafCount[expr]

(* Match an Inactive[Equal][x, x] tautology in HoldForm-free
   internal form.  Used both by the chain loop's stop condition
   and by the Conclusion-detection at dataset-build time. *)
tautologyQ[expr_] := MatchQ[expr, Inactive[Equal][x_, x_]]

(* All single-step rewrites that produce a NEW state at any
   matching position.  Build via Table+Flatten; one inner Block
   per rule index isolates rule + position list. *)
ruleRewrites[currentExpr_, ruleList_, i_] := Block[{
    rule = ruleList[[i]]["Rule"],
    allPos
},
    allPos = Position[currentExpr, rule[[1]],
        {1, Infinity}, Heads -> False];
    Table[
        <|
            "NewExpr" -> ReplaceAt[currentExpr, rule, p],
            "Position" -> p,
            "RuleIdx" -> i,
            "Rule" -> rule
        |>,
        {p, allPos}
    ]
]

allRewriteSteps[currentExpr_, ruleList_, seen_Association] := Block[{
    cands
},
    cands = Flatten[
        Table[
            ruleRewrites[currentExpr, ruleList, i],
            {i, Length[ruleList]}
        ],
        1
    ];
    Select[cands,
        #["NewExpr"] =!= currentExpr &&
        ! KeyExistsQ[seen, #["NewExpr"]] &
    ]
]

(* One BFS step.  state = <|Expr, Hist, Seen, Done|>.  Returns the
   next state (Done -> True when a tautology is reached or no
   progress is possible). *)
chainStep[ruleList_][state_Association] := Block[{
    expr = state["Expr"],
    candidates, tautRec, picked
},
    Which[
        tautologyQ[expr],
            Append[state, "Done" -> True],
        True,
            candidates = allRewriteSteps[expr, ruleList, state["Seen"]];
            If[ Length[candidates] === 0,
                Append[state, "Done" -> True],
                tautRec = SelectFirst[candidates,
                    tautologyQ[#["NewExpr"]] &, None];
                picked = If[ tautRec =!= None,
                    tautRec,
                    First @ SortBy[candidates,
                        termSize[#["NewExpr"]] &]
                ];
                <|
                    "Expr" -> picked["NewExpr"],
                    "Hist" -> Append[state["Hist"], picked],
                    "Seen" -> Append[state["Seen"],
                        picked["NewExpr"] -> True],
                    "Done" -> False
                |>
            ]
    ]
]

(* BFS chain synthesizer.  At each step expand all single-rewrite
   successors of the current expression and pick the one that
   either (a) reaches a tautology immediately, or (b) has the
   smallest LeafCount.  Stops when a tautology is reached or no
   progress is possible.  Returns <|Expr, Hist, Closed|> where
   Closed is True iff Expr is a tautology (i.e. the conjecture
   was actually proven). *)
synthesizeChain[hypothesis_, ruleList_, maxSteps_: 30] := Block[{
    init, final
},
    init = <|
        "Expr" -> hypothesis,
        "Hist" -> {},
        "Seen" -> <|hypothesis -> True|>,
        "Done" -> False
    |>;
    final = NestWhile[chainStep[ruleList],
        init, ! #["Done"] &, 1, maxSteps];
    <|
        "Expr" -> final["Expr"],
        "Hist" -> final["Hist"],
        "Closed" -> tautologyQ[final["Expr"]]
    |>
]

(* Strip Pattern[s, Blank[]] wrappers down to the bare symbol s.
   Used to convert an axiom rhs (which has Pattern[s, _] in the same
   shape as the lhs) into a Rule rhs that substitutes the bound
   value back instead of leaking the pattern variable.  Matches both
   `_x` and `Pattern[x, _Blank]` shapes; pass-through otherwise. *)
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

(* === IC-search provability oracle ================================ *)

(* Milestone 4: decide provability by IC reduction instead of the
   WL-side BFS.  buildSearchTerm builds a depth-D Term whose normal
   form is a SUP-tree of NUM(0)/NUM(1) leaves -- the conjecture is
   provable iff any leaf is NUM(1).  Each step fans over (rewrite
   fn, side) via SUPs, so the collapse enumerates every depth-<=D
   rewrite combination at once.

   Atomic-equational only (axioms + conjecture must be bare
   symbols).  Structured / pattern problems fall back to the BFS.
   Decoding the winning leaf's SUP-path into a verifier chain is
   the next milestone-4 step; today this is a cross-check oracle
   that must agree with the BFS on every atomic case.

   Mirrors wl/Examples/atp_ic/transitivity.wls; that toy will be
   rewired onto this shared builder once the decoder lands. *)

(* Per-call counter so repeated runs get fresh TDef names instead
   of clobbering each other's rewrite-fn defs. *)
$icCallCtr = 0;

(* True iff every axiom side + both conjecture sides are bare
   symbols -- the shape the atomic IC search handles. *)
icAtomicProblemQ[axPairs_, conjPair_] :=
    AllTrue[Flatten[{axPairs, conjPair}], MatchQ[#, _Symbol] &]

(* atom -> positive Int id over axioms + conjecture. *)
icCollectAtoms[axPairs_, conjPair_] := Block[{atoms},
    atoms = DeleteDuplicates @ Flatten[{axPairs, conjPair}];
    AssociationThread[atoms -> Range[Length[atoms]]]
]

(* Register one (axiom, direction) rewrite fn as a TDef: a unary
   lambda mapping NUM(old) -> NUM(new), identity elsewhere. *)
icDefineRewriteFn[name_String, old_Integer, new_Integer] := With[{
    v = Module[{vs}, vs]
},
    TDef[name, TLam[v, TIfZero[TOp2["==", v, TNum[old]], v, TNum[new]]]]
]

(* For each axiom, register fwd + bwd rewrite fns and return the
   flat list of TRef.  `pfx` namespaces this call's defs. *)
icAllRewriteRefs[pfx_String, axPairs_, atomMap_] := Flatten @ Table[
    Block[{ax = axPairs[[i]], oldId, newId, fwdName, bwdName},
        oldId = atomMap[ax[[1]]];
        newId = atomMap[ax[[2]]];
        fwdName = pfx <> "ax" <> ToString[i] <> "_fwd";
        bwdName = pfx <> "ax" <> ToString[i] <> "_bwd";
        icDefineRewriteFn[fwdName, oldId, newId];
        icDefineRewriteFn[bwdName, newId, oldId];
        {TRef[fwdName], TRef[bwdName]}
    ],
    {i, Length[axPairs]}
]

(* Right-associated nested SUP over a flat value list; each nested
   SUP gets a distinct label so DUP-SUP labels don't collide. *)
icNestedSup[baseLab_Integer, ts_List] := Which[
    Length[ts] === 1, ts[[1]],
    Length[ts] === 2, TSup[baseLab, ts[[1]], ts[[2]]],
    True, TSup[baseLab, ts[[1]], icNestedSup[baseLab + 1, Rest[ts]]]
]

(* Apply rChoice to v iff sideChoice picks mySideNum, else pass
   v through unchanged. *)
icSideUpdate[v_, mySideNum_, sideChoice_, rChoice_] :=
    TIfZero[TOp2["==", sideChoice, mySideNum], v, TApp[rChoice, v]]

$icSupBase = 100;

(* One depth step: pick (rewrite fn, side) via shared SUPs, apply
   the chosen rewrite to the chosen side.  Fresh labels per step so
   DUPs across steps commute (independent choices) rather than
   annihilate. *)
icStepFold[rFns_, n_][acc_, stepIdx_] := Block[{
    lhs = acc[[1]], rhs = acc[[2]], labCur = acc[[3]], rSup, sideSup
},
    rSup = icNestedSup[labCur, rFns];
    sideSup = TSup[labCur + n, TNum[0], TNum[1]];
    {
        icSideUpdate[lhs, TNum[0], sideSup, rSup],
        icSideUpdate[rhs, TNum[1], sideSup, rSup],
        labCur + n + 1
    }
]

(* Build the depth-D search Term: TOp2["==", lhsD, rhsD] -- yields
   NUM(1) on a leaf iff that combination of rewrites closed the
   conjecture. *)
icBuildSearchTerm[conjPair_, axPairs_, depth_Integer] := Block[{
    pfx, atomMap, lhs0, rhs0, rFns, n, final
},
    $icCallCtr += 1;
    pfx = "atp_ic_" <> ToString[$icCallCtr] <> "_";
    atomMap = icCollectAtoms[axPairs, conjPair];
    lhs0 = TNum[atomMap[conjPair[[1]]]];
    rhs0 = TNum[atomMap[conjPair[[2]]]];
    rFns = icAllRewriteRefs[pfx, axPairs, atomMap];
    n = Length[rFns];
    final = Fold[icStepFold[rFns, n],
        {lhs0, rhs0, $icSupBase}, Range[depth]];
    TOp2["==", final[[1]], final[[2]]]
]

(* True iff the conjecture is provable from the axioms in <= depth
   rewrite steps, decided by IC reduction + collapse.  Returns
   $Failed when the problem isn't atomic (caller falls back to the
   BFS). *)
icSearchProvable[conjPair_, axPairs_, depth_Integer] := If[
    ! icAtomicProblemQ[axPairs, conjPair],
    $Failed,
    Block[{term, leaves},
        term = icBuildSearchTerm[conjPair, axPairs, depth];
        leaves = FromTTerm /@ TCollapse[term];
        AnyTrue[leaves, # === 1 &]
    ]
]

(* === ProofDataset builder ======================================== *)

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
   rewrite the BFS applied.  This keeps the axiom Statement in its
   original orientation even when the chain uses it both ways. *)
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

(* Strategy:
     1. Emit Axiom entries (always in their original orientation).
     2. Emit Hypothesis from the conjecture.
     3. Build candidate rule list (forward + backward of each axiom).
     4. Synthesize a rewrite chain from Hypothesis to a tautology.
     5. Emit each chain step as a SubstitutionLemma; the last step
        becomes the Conclusion.  Backward-axiom steps carry
        Orientation -> -1 so the verifier reads the axiom Statement
        in reverse for that step.
   All Statements are HoldForm[Equal[lhs, rhs]]; toHoldEq is the
   helper that gets us there from Inactive[Equal][...]. *)
(* Degenerate "Conclusion" entry for the trivial-tautology case
   (the conjecture was already x == x).  Points back to the
   Hypothesis with no rule applied.  WL's verifier accepts this
   as a zero-step proof. *)
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

buildProofDataset[axioms_, conjecture_] := Block[{
    axCount = Length[axioms],
    axiomKeys, hypInactive, ruleList, chainRes, chain,
    axiomEntries, chainEntries, allEntries
},
    hypInactive = Inactive[Equal] @@ conjecture;
    ruleList = buildRuleList[axioms, axiomKeys = Table[{$AxiomSym, k},
        {k, axCount}]];
    chainRes = synthesizeChain[hypInactive, ruleList];
    If[ ! chainRes["Closed"], Return[$Failed] ];
    chain = chainRes["Hist"];
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

(* === TFindEquationalProof ======================================== *)

(* Build a verifier-ready ProofDataset via the WL-side BFS chain
   synth and wrap it in a real WL ProofObject.  The chain synth
   does the heavy lifting: when it reaches an Inactive[Equal][x,
   x] tautology, the chain itself is the proof.  Returns $Failed
   when the BFS doesn't close the conjecture within its budget.

   Historically this also called the C-side ATP saturator and
   accepted proven-by-C as a secondary signal.  The C path is
   gone (milestone 6 of the IC-native ATP arc); the BFS is the
   sole provability check.  IC-search-based reduction (milestone
   4 of the same arc) would replace the BFS once the SUP-path
   decoder lands -- the IC search produces "is provable" much
   like the BFS does, but driven by IC reduction instead of
   WL-side rewrite enumeration.

   atpEncodeProblem still validates axiom/conjecture shape
   (HoldAll + MatchQ HoldComplete[Equal[_,_]]) and surfaces the
   encoder state for the Variables list; its Packed / MaxLab
   fields go unused here but feed TATP. *)
(* Render a held expression in the form WL's ProofObject expects
   for its top-level Axioms list / ConjectureStatement: keep the
   ForAll wrapper if present, but rewrite every nested `Equal[lhs,
   rhs]` to `Inactive[Equal][lhs, rhs]` so trivial tautology axioms
   `a == a` don't collapse to True under ReleaseHold. *)
holdToInactive[axHC_HoldComplete] :=
    ReleaseHold[axHC /. Equal -> Inactive[Equal]]

SetAttributes[TFindEquationalProof, HoldAll];
Options[TFindEquationalProof] = {MaxSteps -> 64};
TFindEquationalProof[conjecture_, axioms_, OptionsPattern[]] := Catch[
    Block[{
        enc = atpEncodeProblem[axioms, conjecture],
        dataset, varNames, axEq, conjStmt, conjPair
    },
        conjPair = enc["ConjPair"];
        axEq = holdToInactive /@ enc["AxHCsRaw"];
        conjStmt = holdToInactive[enc["ConjHCRaw"]];
        dataset = buildProofDataset[enc["AxPairs"], conjPair];
        If[ dataset === $Failed, Return[$Failed] ];
        varNames = Symbol /@ Keys[enc["State"]["var"]];
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
