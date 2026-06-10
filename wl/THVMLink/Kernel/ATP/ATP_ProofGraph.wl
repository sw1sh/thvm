(* ::Package:: *)
(* ATP_ProofGraph.wl -- proof reconstruction: ProofDataset key conventions, axiom / hypothesis
   preprocessing, the rewrite-rule list, the ProofDataset builder, the
   C-engine proof decoder, and the critical-pair-lemma DAG that TFindProof
   decodes a completion trace into a verifier-shaped ProofObject through.

   Sibling of ATP.wl in the THVMLink`ATP` context; the recursive Kernel
   loader Gets it after ATP.wl (files sorted by {depth, lowercased path}),
   and it shares the THVMLink`ATP`Private` context, so it references
   ATP.wl's loaders, encoder, and helpers by bare name. *)

BeginPackage["THVMLink`ATP`", {"THVMLink`", "Wolfram`Parser`"}];

Begin["`Private`"];

(* === ProofDataset key conventions ================================ *)

(* Use the same keys WL's ProofObject expects so our dataset matches
   the FindEquationalProof shape exactly. *)
$AxiomSym = "Axiom"
$HypothesisSym = "Hypothesis"
$SubstitutionLemmaSym = "SubstitutionLemma"
$ConclusionSym = "Conclusion"

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
   the string form of TFindProof to normalize the
   AxiomaticTheory-resolved formulas. *)

wrap[expr_, head_: List] := Replace[expr, x : Except[_head] :> head[x]]

(* Rename every Pattern variable in `expr` to a canonical short name
   (a, b, c, ..., then a1, b1, ... if more are needed).
   IMPORTANT: the short names live in the user-visible Global` context,
   so if a user has e.g. `c = "x"` set globally and an axiom binds three
   variables (renamed a/b/c), the third pattern variable Pattern[c, _]
   gets caught by the global binding when downstream code touches its
   bare-c reference -- the encoder later sees "x" where it expected a
   matched value, Folds over the String head, and SIGSEGVs the kernel.
   Guard against this by escaping the canonical names if the global
   binding already exists.  Without the guard, any of c / m / x / a /
   b / d / f / g / h would be a footgun. *)
CanonicalizePatterns[expr_] := Module[{
    chars = CharacterRange["a", "z"],
    patts = DeleteDuplicates[Cases[expr, _Pattern, All, Heads -> True]]
},
    chars = First @ NestWhile[
        Apply[{cs, k} |-> {Join[cs, (StringJoin[#, ToString[k]] &) /@ cs], k + 1}],
        {chars, 1}, Length[First[#]] < Length[patts] &];
    expr /. MapIndexed[
        With[{canonical = Pattern @@ {
                atpFreshGlobalSymbol[Extract[chars, #2]], Last[#1]}},
            Verbatim[#1] :> canonical] &,
        patts]
]

(* Return the Symbol for `name` if its OwnValues + DownValues are empty;
   otherwise suffix with `$Atp<k>` (k bumped per probe) until a fresh
   Symbol is found.  Keeps the canonical-pattern naming free of
   user-globals collisions.  OwnValues/DownValues are HoldFirst, so the
   freshness check threads the candidate name through `With` to force
   substitution of the actual System symbol before introspection. *)
atpFreshGlobalSymbol[name_String] := Module[{n = name, k = 0},
    While[
        Quiet @ Check[
            With[{sym = Symbol[n]},
                OwnValues[sym] =!= {} || DownValues[sym] =!= {}],
            True (* introspection errored -- treat name as taken *)],
        k++; n = name <> "$Atp" <> ToString[k]];
    Symbol[n]
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
    Inactive[Equal][a_, b_]   :> a == b,
    Inactive[Unequal][a_, b_] :> Unequal[a, b],
    universal_ForAll          :> universalPatterns[universal],
    existential_Exists        :> skolemPatterns[existential]}

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
chainEntry[stepRec_, isLast_, lemmaIdx_, prevKey_, ruleEntry_, concIdx_] := Block[{
    stepKey, absPos, side, relPos, statement
},
    stepKey = If[ isLast,
        {$ConclusionSym, concIdx},
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
   Hypothesis with no rule applied.  `concIdx` is the goal index --
   conjunct g of a multi-goal conjunction closes {Hypothesis, g}
   into {Conclusion, g}; a single goal is g == 1. *)
trivialConclusionEntry[hypInactive_, concIdx_] := {$ConclusionSym, concIdx} -> <|
    "Statement" -> toHoldEq[hypInactive],
    "Proof" -> <|
        "Input" -> {$HypothesisSym, concIdx},
        "Construct" -> {$HypothesisSym, concIdx},
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

(* Assemble the sorted ProofDataset from finished rewrite chains, one
   per goal (FindEquationalProof multi-goal parity: one {Hypothesis, g}
   and one {Conclusion, g} row per conjunct, SubstitutionLemma rows
   numbered globally across the goals).  `conjPairs` and `chains` are
   parallel lists; each chain is a list of step records (<|NewExpr,
   Position, RuleIdx, Rule|>); `ruleList` is the forward+backward rule
   table the RuleIdx fields index into.  An empty chain is the
   trivial-tautology case for that conjunct. *)
assembleGoalsDataset[axioms_, conjPairs_, chains_, ruleList_] := Block[{
    axCount = Length[axioms], hypsInactive,
    axiomEntries, hypEntries, slN = 0, chainEntries, allEntries
},
    hypsInactive = Inactive[Equal] @@ # & /@ conjPairs;
    axiomEntries = Table[
        {$AxiomSym, k} -> <|
            "Statement" -> toHoldEq[Inactive[Equal] @@ axioms[[k]]],
            "Proof" -> <||>
        |>,
        {k, axCount}
    ];
    hypEntries = Table[
        {$HypothesisSym, g} -> <|
            "Statement" -> toHoldEq[hypsInactive[[g]]],
            "Proof" -> <||>
        |>,
        {g, Length[conjPairs]}
    ];
    chainEntries = Join @@ Table[
        Block[{chain = chains[[g]], base = slN},
            If[ chain === {},
                {trivialConclusionEntry[hypsInactive[[g]], g]},
                slN += Length[chain] - 1;
                Table[
                    chainEntry[
                        chain[[s]],
                        s === Length[chain],
                        base + s,
                        If[ s === 1,
                            {$HypothesisSym, g},
                            {$SubstitutionLemmaSym, base + s - 1}
                        ],
                        ruleList[[chain[[s, "RuleIdx"]]]],
                        g
                    ],
                    {s, Length[chain]}
                ]
            ]
        ],
        {g, Length[conjPairs]}
    ];
    allEntries = Join[axiomEntries, hypEntries, chainEntries];
    SortBy[allEntries, $ProofKeyOrder[First[#]] &]
]

(* Single-goal surface kept for the existing call sites. *)
assembleDataset[axioms_, conjPair_, chain_, ruleList_] :=
    assembleGoalsDataset[axioms, {conjPair}, {chain}, ruleList]

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
$AtpTagCTR = 20
$AtpTagFVR = 22

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

(* Method "Precedence" / "SkolemHighest": resolve a precedence spec to
   the label-indexed Int64 array thvm_wl_atp_run_proof's args[17]
   expects.  prec[label] is the LPO/KBO precedence rank of that label
   (higher = greater; mirrors Waldmeister's `p > q > nand` ORDERING).
   None -> an empty array (the C side leaves identity / auto-precedence
   in place, so the engine is byte-identical unless precedence is set).

   The spec is one of:
     None
         inactive (empty array).
     {sym1, sym2, ...}  (Strings or Symbols, highest-to-lowest)
         sym1 gets the largest rank, the last the smallest; any symbol
         the goal/axioms use but the list omits keeps rank 0 (lowest).
     "SkolemHighest"
         the structural rule WM's `p > q > nand` precedence encodes:
         rank every ground (0-arity) constant the goal skolemized into
         ABOVE every other symbol (the operators).  Among the skolem
         constants, order is by label (stable, matches encode order). *)
atpSymName[s_String] := s
atpSymName[s_Symbol] := SymbolName[Unevaluated[s]]
SetAttributes[atpSymName, HoldFirst];
atpSymName[s_] := ToString[s]

(* The 0-arity (ground constant) symbol names occurring in a held
   conjecture pair -- these are the goal's skolem constants. *)
atpGroundConstNames[enc_] := Block[{names},
    names = Cases[
        Hold @@ {enc["ConjPair"]},
        s_Symbol /; AtomQ[Unevaluated[s]] :>
            SymbolName[Unevaluated[s]],
        {0, Infinity}, Heads -> False];
    DeleteDuplicates[names]
]

(* The result is a plain Int64 List the FFI reads label-indexed
   (element i = precedence rank of label i; element 0 is the unused
   label-0 placeholder).  An empty List leaves the C default in place. *)
atpPrecedenceArray[None, enc_] := {}
atpPrecedenceArray[order_List, enc_] := Block[{
    sym = enc["State"]["sym"], maxLab = enc["MaxLab"], names, ranks, arr
},
    names = atpSymName /@ order;
    (* Highest-to-lowest: first name gets the largest rank.  Unlisted
       symbols stay 0. *)
    ranks = AssociationThread[names -> Range[Length[names], 1, -1]];
    arr = ConstantArray[0, maxLab + 1];
    KeyValueMap[
        {nm, lab} |-> If[ KeyExistsQ[ranks, nm] && lab >= 1 && lab <= maxLab,
            arr[[lab + 1]] = ranks[nm]],
        sym];
    arr
]
atpPrecedenceArray["SkolemHighest", enc_] := Block[{
    sym = enc["State"]["sym"], maxLab = enc["MaxLab"], skNames, arr,
    nNonSk, skRankOf,
    reservedNames = {"cAtp1", "cAtp2"}
},
    (* Skolem constants get strict order above every operator, matching
       Waldmeister's `p > q > r > nand` ORDERING block: first-occurring
       skolem name (k of them) ranks highest, second next, etc.  Equal-
       ranking the skolems leaves LPO with several incomparable ground
       constants which prevents many Sheffer-style orientation choices;
       the strict order is what real WM's andassoc.pr uses.
       The engine-reserved constants cAtp1 / cAtp2 (used by the FVI rule
       emission in `thvm_atp_orient_and_add`) get rank 0 = LOWEST so a
       grounded equation `lhs > cAtp1` actually orients KBO_GT (mirrors
       Waldmeister's `SO_minimaleKonstante` at the bottom of the
       precedence chain). *)
    skNames = atpGroundConstNames[enc];
    arr = ConstantArray[0, maxLab + 1];
    nNonSk = Count[Keys[sym],
        nm_ /; ! MemberQ[skNames, nm] && ! MemberQ[reservedNames, nm]];
    skRankOf = AssociationThread[skNames,
        Range[nNonSk + Length[skNames], nNonSk + 1, -1]];
    Block[{nonSkRank = 0},
        KeyValueMap[
            {nm, lab} |-> If[ lab >= 1 && lab <= maxLab,
                arr[[lab + 1]] = Which[
                    MemberQ[reservedNames, nm], 0,
                    MemberQ[skNames, nm], skRankOf[nm],
                    True, nonSkRank = nonSkRank + 1; nonSkRank]],
            sym]];
    arr
]
atpPrecedenceArray[_, enc_] := {}

(* Method "SymbolWeights" -> {sym -> w, ...}: an explicit per-symbol
   KBO weight map.  Returns a label-indexed Int64 list (element i = the
   weight to assign to label i; 0 = leave at default 1, mirroring
   atpPrecedenceArray's "leave at default" sentinel).  An empty list
   short-circuits to the engine default (uniform 1).  Waldmeister
   SymbolGewichte port (CLAS/SymbolGewichte.c::SG_SymbGewichteEintragen,
   -w DEF=2:VAR=5:f=5:g=0). *)
atpSymbolWeightsArray[None, enc_] := {}
atpSymbolWeightsArray[map_Association, enc_] := Block[{
    sym = enc["State"]["sym"], maxLab = enc["MaxLab"], arr,
    nameMap = KeyMap[atpSymName, map]
},
    arr = ConstantArray[0, maxLab + 1];
    KeyValueMap[
        {nm, lab} |-> If[ KeyExistsQ[nameMap, nm] && lab >= 1 && lab <= maxLab,
            arr[[lab + 1]] = nameMap[nm]],
        sym];
    arr
]
atpSymbolWeightsArray[rules_List, enc_] :=
    atpSymbolWeightsArray[Association[rules], enc]
atpSymbolWeightsArray[_, enc_] := {}

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
(* All Method-knob arguments are passed positionally by atpProveBundle
   via `Sequence @@ atpMethodCfg`, which always supplies the full set of
   values, so the pattern uses no default-bearing Optional patterns: that
   keeps it under WL's 13-optional threshold and avoids the Pattern::patm
   message. *)
cEngineProof[enc_, maxSteps_, wallSeconds_,
    cpWeight_, ordering_, autoPrec_, useMnf_,
    maxCpWeight_, goalInterleave_, groundJoin_,
    selRatio_, autoMaxWeight_, rhsInterreduce_, unfailingCP_,
    cpSetInterreduce_, connectedness_, precedenceSpec_,
    fifoTiebreak_, recordNorm_, useLRS_, useSOS_,
    useFwdSub_, useBwdSub_, useBwdDemod_, symbolWeightsSpec_,
    varWeight_, randomRatio_, randomSeed_, kwsMode_,
    lazyNormalize_, coopWeight_, coopRatio_, useFvi_,
    useImplicitCp_] := Block[{
    raw, status, nRules, nTrace, nSteps, nCps, extNRules, extNSteps,
    mnfNSteps, cur, labelToName, idToName, mainSteps, extSteps,
    mnfSteps, mainRules, rTrace, traceEntries, precArray, symbolWeightsArr
},
    precArray = atpPrecedenceArray[precedenceSpec, enc];
    symbolWeightsArr = atpSymbolWeightsArray[symbolWeightsSpec, enc];
    raw = $atpRunProofFn[enc["Packed"], maxSteps, enc["MaxLab"],
        N[wallSeconds], cpWeight, ordering, autoPrec, useMnf, maxCpWeight,
        goalInterleave, groundJoin, selRatio, autoMaxWeight, rhsInterreduce,
        unfailingCP, cpSetInterreduce, connectedness, precArray, fifoTiebreak,
        recordNorm, useLRS, useSOS, useFwdSub, useBwdSub, useBwdDemod,
        symbolWeightsArr, varWeight, randomRatio, randomSeed, kwsMode,
        lazyNormalize, coopWeight, coopRatio, useFvi, useImplicitCp];
    (* C engine returns LibraryFunctionError on memory-guard abort
       (THVM_ATP_RSS_ABORT_MB / THVM_ATP_HEAP_ABORT_FRAC) or other
       hard-stop conditions.  Bail BEFORE the structural part extraction
       so an aborted run returns a clean Failure association instead of
       crashing the kernel via Part[][[k]] on a Failure expression. *)
    If[ MatchQ[raw, _LibraryFunctionError],
        Return[<|"Status" -> "Aborted", "Reason" -> raw|>]
    ];
    raw = Normal @ raw;
    status = raw[[1]];
    nRules = raw[[2]]; nTrace = raw[[3]]; nSteps = raw[[5]]; nCps = raw[[4]];
    extNRules = raw[[6]]; extNSteps = raw[[7]]; mnfNSteps = raw[[8]];
    labelToName = Association[Reverse /@ Normal[enc["State"]["sym"]]];
    idToName = Association[Reverse /@ Normal[enc["State"]["var"]]];
    (* The C runner emits the full output (MAIN rules + trace + steps,
       EXT/MNF blocks) for ANY terminal status, so completion mode
       (TIMEOUT / QUEUE_EMPTY -- there is no goal to PROVE) still decodes
       the derived rule set as "MainRules".  Only the goal-chain EXT /
       MNF steps are meaningless without a PROVED goal, so they are left
       $Failed below for the non-PROVED case. *)
    (* MAIN rules block: 2*nRules packed Terms. *)
    cur = 8;
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
                "LhsRaw" -> l, "RhsRaw" -> r,
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
    (* MNF steps block: the GREEN/RED front chains for a goal closed
       by the MNF bidirectional search.  Same per-step layout. *)
    {mnfSteps, cur} =
        decodeStepsBlock[raw, cur, mnfNSteps, labelToName, idToName];
    <|
        "Status" -> status,
        "ExtSteps" -> If[ extNSteps === 0, {}, extSteps],
        "MainSteps" -> If[ nSteps === 0, {}, mainSteps],
        "MnfSteps" -> If[ mnfNSteps === 0, {}, mnfSteps],
        "MainRules" -> mainRules,
        "RTrace" -> rTrace,
        "Trace" -> traceEntries,
        "NCps" -> nCps,
        "RecordNorm" -> recordNorm,
        "L2N" -> labelToName, "I2N" -> idToName,
        (* every variable symbol the decode produced: the named
           encoder vars plus any "x<id>" fallbacks for FVR ids
           completion introduced past the original signature.  Both
           the dataset builder and the ProofObject "Variables" list
           need the complete set, or the verifier reads a stray
           completion variable as a constant.

           Block-localize the common short alphabet names around the
           Symbol[name] construction: Symbol["c"] returns the symbol
           Global`c, and any List that contains it eagerly evaluates --
           so a caller with `Do[..., {c, ...}]` would otherwise see the
           Do iter's value land in "VarSyms" instead of the bare symbol.
           CanonicalizePatterns escapes axiom-side names upstream
           (atpFreshGlobalSymbol -> c$Atp1), but the encoder's idToName
           still carries the original short labels, so a Block here is
           the right boundary. *)
        "VarSyms" -> Quiet[Block[{
                Global`a, Global`b, Global`c, Global`d, Global`e,
                Global`f, Global`g, Global`h, Global`i, Global`j,
                Global`k, Global`l, Global`m, Global`n, Global`o,
                Global`p, Global`q, Global`r, Global`s, Global`t,
                Global`u, Global`v, Global`w, Global`x, Global`y, Global`z,
                Global`x1, Global`x2, Global`x3, Global`x4, Global`x5,
                Global`x6, Global`x7, Global`x8, Global`x9, Global`x10,
                Global`x11, Global`x12,
                Global`y1, Global`y2, Global`y3,
                Global`z1, Global`z2, Global`z3},
            Union[
                Symbol /@ Values[idToName],
                Cases[{mainRules, mainSteps, extSteps, mnfSteps},
                    s_Symbol /; StringMatchQ[SymbolName[s],
                        "x" ~~ DigitCharacter ..],
                    {0, Infinity}]
            ]], {General::shdw}]
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

(* Split a flat multi-goal steps block into one chain per conjunct.
   The C bridge tags each step's Side as 2*g + side (goal-major
   order, atp_extract_goal_chains in thvmlink_atp.c), so goal g's
   steps are the Quotient[Side, 2] == g slice with Side reduced back
   to the 0/1 lhs/rhs encoding.  An empty slice is legitimate only
   for a reflexive conjunct -- a non-reflexive conjunct with no steps
   means its chain was not single-NF extractable (an MNF-only join),
   so the whole extraction fails and the caller degrades to the
   completion-lemma dataset.  Returns the list of per-goal chains, or
   $Failed. *)
buildCEngineChains[steps_, conjPairs_, ruleList_] := Catch[
    Table[
        Block[{gSteps, chain},
            gSteps = Select[steps, Quotient[#["Side"], 2] === g - 1 &];
            gSteps = Append[#, "Side" -> Mod[#["Side"], 2]] & /@ gSteps;
            If[ gSteps === {} && conjPairs[[g, 1]] =!= conjPairs[[g, 2]],
                atpDbgFail["buildCEngineChains.unjoined-goal"];
                Throw[$Failed]];
            chain = buildCEngineChain[gSteps, conjPairs[[g]], ruleList];
            If[ chain === $Failed, Throw[$Failed]];
            chain
        ],
        {g, Length[conjPairs]}
    ]
]

(* === critical-pair lemma DAG ====================================== *)

(* Trace-entry reasons (src/thvm.h): an input / re-queued equation,
   a CP oriented into a rule, a critical pair, an interreduce
   re-queue carrying the dropped rule's lineage. *)
$TraceAxiom = 1
$TraceOrient = 2
$TraceCp = 3
$TraceSimplify = 4
$TraceNormStep = 5
(* TRACE_FVI: a Waldmeister `RechtsUnfreiErzeugen` (FVI) sibling rule
   pushed alongside an unorientable KBO_UN equation; the rule grounds
   every free variable on one side that does not appear on the other
   against `s->min_const` (the precedence-minimum reserved label).  See
   `atp_emit_fvi_pair` in src/atp/_.c.  Same `(lhs, rhs)` cell layout
   as TRACE_ORIENT; the lifter walks the parent KBO_UN equation's
   chain to recover provenance. *)
$TraceFvi = 6
$AtpTraceNone = 4294967295

(* Assemble a verifier-shaped ProofObject dataset for a
   completion-derived proof, walking the MAIN-state trace DAG the C
   glue ships (cEngineProof's MainSteps / MainRules / RTrace /
   Trace fields).

   The trace DAG is fully connected: TRACE_SIMPLIFY keeps every
   re-queued rule parented on the rule it descended from, so a
   closing rule traces back through CP / ORIENT / SIMPLIFY nodes to
   the single input axiom.

   Each trace reason maps to one dataset entry:
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
   need their FVRs rendered as Pattern[v, Blank[]] on rule lhs's. *)

(* True iff `v` is a completion variable: a named rule variable (member
   of varSyms), an "x<id>" symbol decodeAtpTerm minted for a fresh FVR,
   or a "cplU<n>" symbol cplFreshen introduced when re-deriving a CP's
   superposition.  The single variable predicate every cpl-* helper
   shares so canonicalization, unification, and patternization agree on
   what is a variable. *)
cplVarQ[v_, varSyms_] := MatchQ[v, _Symbol] && (MemberQ[varSyms, v] || atpXVarQ[v] || StringMatchQ[SymbolName[v], "cplU" ~~ DigitCharacter ..])

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
            v_Symbol /; cplVarQ[v, varSyms], {0, Infinity},
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
    ca === cplCanonVars[b, varSyms] || ca === cplCanonVars[Reverse[b], varSyms]
]

(* The set of first-order variables (members of varSyms) occurring in
   `expr`.  Used to gate reverse-direction rewriting on whether the
   rule's lhs vars are a subset of its rhs vars -- the same
   atp_vars_contained guard ordered rewriting uses in the C engine. *)
cplVarsIn[expr_, varSyms_] := DeleteDuplicates @ Cases[expr,
    v_Symbol /; cplVarQ[v, varSyms], {0, Infinity}, Heads -> True]

(* True iff overlapping ParentB's rule `ruleBEq` onto ParentA's rule
   `ruleAEq` at superposition position `pos` is DEGENERATE for the WL
   verifier: ParentB's rule actually MATCHES the subterm of ParentA's
   lhs at `pos` (so the verifier's unification fires there) yet
   rewriting it is a NO-OP -- the subterm is invariant under ParentB's
   permutation (e.g. commutativity x.y -> y.x applied to a square b.b
   yields b.b unchanged).  A matching no-op is exactly the case where
   the verifier's unification of ParentB into ParentA forces two
   distinct ParentB variables together, collapsing ParentB's
   instantiated Statement to a trivial t == t, which the verifier
   auto-evaluates to True and then crashes folding the next step over
   (MapAt[..., True]).  Such an overlap is really a single ParentB
   rewrite of ParentA's equation, so it is bridged with emitNorm
   instead.  When ParentB does NOT match the subterm the recorded
   superposition unifies elsewhere -- an ordinary CP the verifier
   reconstructs fine -- so it keeps its CriticalPairLemma entry and
   proofs that never hit a permutative self-overlap are untouched. *)
cplDegenerateOverlapQ[ruleAEq_List, ruleBEq_List, pos_, varSyms_] :=
    Block[{sub, rl, res},
        sub = Quiet @ Extract[ruleAEq[[1]], pos];
        If[ MatchQ[sub, _Extract] || MissingQ[sub], Return[False]];
        rl = cplAsRule[ruleBEq, varSyms];
        MatchQ[sub, First[rl]] && (res = Quiet @ Replace[sub, rl]; res === sub)
    ]

(* Rewrite each first-order variable occurrence (members of
   varSyms) in `term` as Pattern[v, Blank[]].  The var -> var_ rules
   are built with MapThread so no Pattern lands on a RuleDelayed
   rhs.  The verifier computes a step's expected result from the
   cited rules' patterned lhs's, so every dataset Statement that
   carries variables must be patternized to match. *)
atpXVarQ[s_Symbol] := StringMatchQ[SymbolName[s], "x" ~~ DigitCharacter ..]
atpXVarQ[_] := False

(* Patternize the completion variables in `term`: the named vars in
   varSyms plus any "x<id>" symbol decodeAtpTerm minted for a fresh
   FVR completion introduced past the signature.  Recognizing the
   x-pattern here means varSyms need only carry the named vars, so the
   trace need not be eagerly scanned for completion vars.

   The replacement is a list of concrete `sym -> sym_` Rules (one per
   occurring variable), NOT a single `(v_Symbol /; ...) :> Pattern[v, _]`
   rule.  The conditional form re-binds `v` as a Pattern on a RuleDelayed
   rhs, which the kernel flags with RuleDelayed::rhs; building the rules per
   concrete symbol keeps every Pattern off a rule rhs and is silent. *)
cplPatternize[term_, varSyms_] := Module[{vars},
    vars = DeleteDuplicates @ Cases[term,
        v_Symbol /; (MemberQ[varSyms, v] || atpXVarQ[v]), {0, Infinity}];
    term /. (Rule[#, Pattern[#, Blank[]]] & /@ vars)]

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

(* Rename every variable of `expr` (members of varSyms / x-FVR vars) to a
   batch of fresh "cplU<n>" symbols, so two rules superposed together do
   not share variable names.  Returns {renamedExpr, freshVars}. *)
cplFreshen[expr_, varSyms_] := Block[{occ, ren},
    occ = cplVarsIn[expr, varSyms];
    ren = AssociationThread[occ, Table[Unique["cplU"], {Length[occ]}]];
    {expr /. ren, Values[ren]}
]

(* Robinson syntactic unification over the head/argument tree.  `vars`
   is the set of unifiable variable symbols; every other symbol is a
   constant.  Returns an Association substitution, or $Failed when the
   two terms do not unify.  Used to reconstruct a CriticalPairLemma's
   superposition the way WL's verifier does: unify the matching rule's
   lhs into the construct rule's lhs at the recorded position. *)
cplUnify[s_, t_, vars_] := cplUnifyLoop[{{s, t}}, <||>, vars]
cplUnifyLoop[$Failed, _, _] := $Failed
cplUnifyLoop[{}, sub_, _] := sub
cplUnifyLoop[lst_List, sub_, vars_] := Block[{s, t, ss, tt, rest},
    {s, t} = First[lst];
    rest = Rest[lst];
    ss = s //. Normal[sub];
    tt = t //. Normal[sub];
    Which[
        ss === tt, cplUnifyLoop[rest, sub, vars],
        cplVarQ[ss, vars], If[ ! FreeQ[tt, ss], $Failed,
            cplUnifyLoop[rest, Append[sub, ss -> tt], vars]],
        cplVarQ[tt, vars], If[ ! FreeQ[ss, tt], $Failed,
            cplUnifyLoop[rest, Append[sub, tt -> ss], vars]],
        AtomQ[ss] || AtomQ[tt], $Failed,
        Head[ss] =!= Head[tt] || Length[ss] =!= Length[tt], $Failed,
        True, cplUnifyLoop[
            Join[Thread[{List @@ ss, List @@ tt}], rest], sub, vars]
    ]
]

(* Reconstruct the equation WL's verifier computes for a CriticalPairLemma
   whose Construct rule is `cEq` ({lhs, rhs}), MatchingConstruct rule is
   `mEq` ({lhs, rhs}), and superposition position is `pos`.  Convention
   (reverse-engineered from FindEquationalProof output): the PEAK is the
   Construct rule's lhs; the subterm at `pos` is unified with the Matching
   rule's lhs; the CP's LHS is the Construct rule's rhs and its RHS is the
   peak with the subterm at `pos` rewritten to the Matching rule's rhs,
   both under the unifier.  Returns {lhs, rhs}, or $Failed when the
   geometry does not unify (a wrong face / truncated position). *)
cplReconCp[cEq_, mEq_, pos_, varSyms_] := Block[{
    cf, cv, mf, mv, peak, subt, sub
},
    {cf, cv} = cplFreshen[cEq, varSyms];
    {mf, mv} = cplFreshen[mEq, varSyms];
    peak = cf[[1]];
    subt = If[ pos === {}, peak, Quiet @ Extract[peak, pos]];
    If[ MatchQ[subt, _Extract] || MissingQ[subt], Return[$Failed]];
    sub = cplUnify[subt, mf[[1]], Join[cv, mv]];
    If[ sub === $Failed, Return[$Failed]];
    {
        cf[[2]] //. Normal[sub],
        (If[ pos === {}, mf[[2]], ReplacePart[peak, pos -> mf[[2]]]]) //.
            Normal[sub]
    }
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
        rTrace = cRes["RTrace"],
        (* Goal chain: the MNF front chain when the goal was closed by
           the bidirectional MNF search (GREEN side-0 + RED side-1
           rewrites driving the conjecture to meet == meet), else the
           completion single-NF MainSteps.  Both are decodeStepsBlock
           output (same {Side, RuleC, Fwd, PosPath, Before, After}
           shape).  RuleC carries a live-rule index for a completion
           step (resolved via resolveRule -> rTrace) and a TRACE index
           for an MNF step (resolved via resolveTrace directly, see the
           chain build below); either way the cited rule -- axiom or
           completion-derived -- resolves through the trace DAG into an
           Axiom / CriticalPairLemma entry, so an MNF front-collision
           proof verifies the same way a completion proof does. *)
        usingMnf = (ListQ[cRes["MnfSteps"]] && cRes["MnfSteps"] =!= {}),
        mainSteps = If[ ListQ[cRes["MnfSteps"]] && cRes["MnfSteps"] =!= {},
            cRes["MnfSteps"], cRes["MainSteps"]],
        axPairs = enc["AxPairs"], varSyms, entries, traceInfo,
        inProgress, aliveRulesAt, slN, cpN, axiomKeyFor, rewriteOnce,
        prepareRules, runBfs, reverseBfsPath, emitNorm, resolveCp,
        resolveTrace, resolveRule, axiomEntries,
        cjp, nGoals, cjps, splitTrivial, chainEntries, allEntries,
        stmt, l2n, i2n, dterm, tL, tR
    },
        varSyms = cRes["VarSyms"];
        (* Lazy, memoized trace-term decode: trace entries ship raw Term
           ints (LhsRaw/RhsRaw); decodeAtpTerm walks the C heap per cell,
           so eagerly decoding all ~15k saturation entries cost seconds.
           Decode on demand -- only the few hundred entries the proof
           actually reaches (resolveTrace DAG + aliveRulesAt ORIENTs)
           get decoded, each once. *)
        l2n = cRes["L2N"]; i2n = cRes["I2N"];
        Clear[dterm];
        dterm[ri_] := dterm[ri] = decodeAtpTerm[ri, l2n, i2n];
        tL[e_] := dterm[e["LhsRaw"]];
        tR[e_] := dterm[e["RhsRaw"]];
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
                {t, {tL[trace[[t + 1]]], tR[trace[[t + 1]]]}},
                {t, Select[Range[0, ti - 1],
                    (trace[[# + 1]]["Reason"] === $TraceOrient || trace[[# + 1]]["Reason"] === $TraceFvi) && ! MemberQ[dropped, #] &]}
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
        (* Precompute the rule data once per emitNorm call: cplAsRule
           is O(term-size), so building it here instead of per BFS node
           keeps it off the inner loop on large alive sets.  Returns a
           list of {traceIdx, fwdRule, revSafe, revRule|Null}. *)
        prepareRules[aliveList_] := Block[{},
            Table[
                Block[{eqA = ar[[2]], rlF, revSafe, rlR},
                    rlF = cplAsRule[eqA, varSyms];
                    revSafe = SubsetQ[
                        cplVarsIn[eqA[[2]], varSyms],
                        cplVarsIn[eqA[[1]], varSyms]];
                    rlR = If[ revSafe,
                        cplAsRule[Reverse[eqA], varSyms], Null];
                    {ar[[1]], rlF, revSafe, rlR}],
                {ar, aliveList}
            ]
        ];

        rewriteOnce[eq_, preRules_, tryReverse_] := Block[{out = {}},
            Do[
                Block[{traceIdx = pr[[1]], rlF = pr[[2]],
                       revSafe = pr[[3]], rlR = pr[[4]], sub, new},
                    Do[
                        sub = eq[[side + 1]];
                        Do[
                            new = Quiet @ ReplaceAt[sub, rlF, pos];
                            If[ new =!= sub && FreeQ[new, ReplaceAt],
                                AppendTo[out, {
                                    ReplacePart[eq, side + 1 -> new],
                                    traceIdx, side, pos, 1}]
                            ],
                            {pos, Position[sub, rlF[[1]],
                                {0, Infinity}, Heads -> False]}
                        ];
                        If[ tryReverse && revSafe,
                            Do[
                                new = Quiet @ ReplaceAt[sub, rlR, pos];
                                If[ new =!= sub && FreeQ[new, ReplaceAt],
                                    AppendTo[out, {
                                        ReplacePart[eq, side + 1 -> new],
                                        traceIdx, side, pos, -1}]
                                ],
                                {pos, Position[sub, rlR[[1]],
                                    {0, Infinity}, Heads -> False]}
                            ]
                        ],
                        {side, 0, 1}
                    ]
                ],
                {pr, preRules}
            ];
            out
        ];

        (* BFS one phase: explore from `start` looking for a rewrite
           path to anything cplEqSetQ-equal to `target`.  Returns the
           hist on success, Missing[] on cap / wall exhaustion or
           queue emptiness.  Uses a CreateDataStructure["Queue"] for
           O(1) push/pop on long BFS runs.  `wallSec` bounds the
           single-BFS wall time (Infinity == no bound). *)
        runBfs[start_, target_, preRules_, tryReverse_, cap_,
               wallSec_:Infinity] :=
            Block[{queue, seenA, found = Missing[], explored = 0,
                   nbrs, t0 = AbsoluteTime[]},
                queue = CreateDataStructure["Queue"];
                queue["Push", {start, {}}];
                seenA = <|start -> True|>;
                While[ queue["Length"] > 0 && MissingQ[found] && explored < cap && (wallSec === Infinity || AbsoluteTime[] - t0 < wallSec),
                    Block[{node = queue["Pop"], eq, hist},
                        explored++;
                        {eq, hist} = node;
                        If[ cplEqSetQ[eq, target, varSyms],
                            found = hist,
                            nbrs = rewriteOnce[eq, preRules, tryReverse];
                            Do[
                                If[ ! KeyExistsQ[seenA, nb[[1]]],
                                    AssociateTo[seenA, nb[[1]] -> True];
                                    queue["Push",
                                        {nb[[1]], Append[hist, nb]}]
                                ],
                                {nb, nbrs}
                            ]
                        ]
                    ]
                ];
                found
            ];

        (* Turn a target ->* start BFS path `rev` (each step
           {resultEq, traceIdx, side, relPos, dir}, the eq sequence
           being target, rev[1].eq, ..., start) into the equivalent
           start ->* target forward path: the forward eq sequence is
           that list reversed, and each forward step rewrites one eq
           into the next using the SAME rule / side / position as the
           reverse step that produced the boundary, with its direction
           FLIPPED (a rewrite and its inverse share rule, side and
           position; only the orientation differs).  Each emitted
           forward step carries the forward RESULT eq in slot 1 so the
           emitNorm loop emits the right intermediate Statements. *)
        reverseBfsPath[rev_List, targetEq_] :=
            Block[{eqs, k = Length[rev]},
                (* The reverse search visits target, rev[1].eq, ...,
                   rev[k].eq == start.  Prepending target and reversing
                   gives the forward sequence eqs with eqs[[1]] == start
                   and eqs[[k+1]] == target.  Forward step j rewrites
                   eqs[[j]] into eqs[[j+1]] using the reverse step that
                   crossed that same boundary (rev[[k-j+1]]) with its
                   direction flipped; relPos / side are shared by a
                   rewrite and its inverse (same redex location). *)
                eqs = Reverse @ Prepend[rev[[All, 1]], targetEq];
                Table[
                    With[{rstep = rev[[k - j + 1]]},
                        {eqs[[j + 1]], rstep[[2]], rstep[[3]],
                         rstep[[4]], -rstep[[5]]}],
                    {j, k}
                ]
            ];

        (* re-derive startEq ->* targetEq, replaying with the rules
           alive when the rule at trace `ti` was oriented; emit a
           SubstitutionLemma per step; return the final entry's
           {key, eq}.  inKey is the dataset key of startEq. *)
        emitNorm[inKey_, startEq_, targetEq_, ti_] :=
            Block[{aliveList, preRules, found, curKey, curEq, st,
                   cInfo, rEq},
                aliveList = aliveRulesAt[ti];
                (* Precompute rule data once, shared across both BFS
                   phases; cplAsRule is the dominant per-node cost. *)
                preRules = prepareRules[aliveList];
                (* Two-phase BFS.  Phase 1 is forward-only with a large
                   cap.  Phase 2 only runs when Phase 1 exhausts: it
                   re-explores with reverse direction enabled (variable-
                   safe rules only) and a tight cap, which unlocks
                   ordered-rewriting paths the forward-only BFS could not
                   reach.  A per-BFS wall budget (2 seconds) keeps a
                   single pathological normalization from monopolizing the
                   wrapper's time budget; we throw $Failed and let the
                   outer two-phase retry / FindEquationalProof fall
                   through. *)
                found = runBfs[startEq, targetEq, preRules, False, 50000, 2];
                If[ MissingQ[found],
                    found = runBfs[startEq, targetEq, preRules, True, 600, 2]];
                (* Phase 3: bidirectional fallback.  A start ->* target
                   bridge whose net direction INCREASES term size (a CP
                   that expands the equation, or a derivation that must
                   apply a contracting rule in its variable-introducing
                   reverse) is unreachable by the forward / variable-safe
                   reverse BFS above.  Search the REVERSE problem
                   target ->* start instead -- which runs in the
                   size-DECREASING (normalizing) direction the forward
                   BFS handles -- then flip the found path into a
                   start -> target emission: walk it forwards, emitting
                   each rewrite with its direction reversed.  Sound: a
                   rewrite t1 -> t2 by a rule at a position is exactly an
                   inverse rewrite t2 -> t1 by the same rule at the same
                   position with the opposite orientation, which the
                   verifier replays via the step's emitted dir / the
                   cited rule's Statement orientation. *)
                If[ MissingQ[found],
                    Block[{rev = runBfs[targetEq, startEq, preRules, True,
                            50000, 3]},
                        If[ ! MissingQ[rev],
                            found = reverseBfsPath[rev, targetEq]]]];
                If[ MissingQ[found],
                    atpDbgFail["emitNorm.no-rewrite-path"]; Throw[$Failed]];
                curKey = inKey;
                curEq = startEq;
                Do[
                    cInfo = resolveTrace[step[[2]]];
                    rEq = {tL[trace[[step[[2]] + 1]]],
                           tR[trace[[step[[2]] + 1]]]};
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
                            "Orientation" -> step[[5]] * cplOrient[cInfo["Eq"], rEq, varSyms],
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

        (* Pick the (construct rule face, matching rule face, role
           assignment) whose superposition reproduces the stored CP under
           the verifier's convention (cplReconCp).  The C saturator may
           have overlapped EITHER face of an unorientable parent (unfailing
           completion superposes both faces of an incomparable equation),
           so the peak the verifier reconstructs is not always ParentA's
           stored lhs.  The recorded position `pos` is exact and the stored
           CP `cpEq` is exact, so trying the (up to) two-by-two-by-two face
           and role combinations and keeping the one whose reconstruction
           equals `cpEq` recovers the real overlap geometry for ANY CP.
           Returns <|Construct, ConstructEq, Matching, MatchingEq|> (each
           *Eq the chosen oriented {lhs, rhs} face), or Missing[] when no
           combination reproduces `cpEq` (e.g. a position truncated past
           CP_MAX_DEPTH), in which case the caller bridges with emitNorm. *)
        chooseCpGeometry[aInfo_, ruleAEq_, bInfo_, ruleBEq_, pos_, cpEq_] :=
            Block[{cands, hit},
                cands = Flatten[
                    Table[
                        {role[[1]], role[[2]], role[[3]], role[[4]],
                         cFace, mFace},
                        {role, {
                            {aInfo, ruleAEq, bInfo, ruleBEq},
                            {bInfo, ruleBEq, aInfo, ruleAEq}}},
                        {cFace, {Identity, Reverse}},
                        {mFace, {Identity, Reverse}}],
                    2];
                hit = SelectFirst[cands,
                    cand |-> With[{r = cplReconCp[cand[[5]][cand[[2]]],
                            cand[[6]][cand[[4]]], pos, varSyms]},
                        ListQ[r] && cplEqSetQ[r, cpEq, varSyms]]];
                If[ MissingQ[hit], Return[Missing[]]];
                <|"Construct" -> hit[[1]]["Key"],
                  "ConstructInfo" -> hit[[1]],
                  "ConstructEq" -> hit[[5]][hit[[2]]],
                  "Matching" -> hit[[3]]["Key"],
                  "MatchingInfo" -> hit[[3]],
                  "MatchingEq" -> hit[[6]][hit[[4]]]|>
            ];

        (* emit a CriticalPairLemma for the TRACE_CP at trace index
           ti; return its <|Key, Eq|>. *)
        resolveCp[ti_] := Block[{
            cte, cpEq, pos, aTe, bTe, ruleAEq, ruleBEq, aInfo, bInfo,
            geom, key, st, cEq, mEq
        },
            cte = trace[[ti + 1]];
            (* WL's verifier builds the CP as
               (Construct's non-overlap side, overlap side rewritten)
               -- the reverse of thvm's recorded (rewritten-lhs, rhs)
               critical pair -- so the CriticalPairLemma Statement
               takes the sides swapped. *)
            cpEq = {tR[cte], tL[cte]};
            pos = cte["Pos"];
            aTe = trace[[cte["ParentA"] + 1]];
            bTe = trace[[cte["ParentB"] + 1]];
            ruleAEq = {tL[aTe], tR[aTe]};
            ruleBEq = {tL[bTe], tR[bTe]};
            aInfo = resolveTrace[cte["ParentA"]];
            bInfo = resolveTrace[cte["ParentB"]];
            (* The WL verifier reconstructs a CriticalPairLemma by
               unifying the MatchingRule (Matching parent) lhs into the
               Construct rule (Construct parent) lhs at the recorded
               superposition position and instantiating the matching
               Statement under that unifier.  A permutative self-overlap --
               a commutativity-style (x.y -> y.x) face superposed onto a
               square subterm b.b -- forces two of the matching face's
               variables together, so the instantiated Statement collapses
               to a trivial t == t, which the verifier auto-evaluates to
               True and then crashes folding the next step over
               (MapAt[..., True]).  Such an overlap is really a single
               rewrite of the construct parent's equation, so bridge it
               with emitNorm (a real rewrite path over the rules alive at
               ti) instead of emitting a CriticalPairLemma the verifier
               crashes on.  Sound: emitNorm only emits real oriented /
               ordered rewrites that the verifier replays. *)
            If[ cplDegenerateOverlapQ[ruleAEq, ruleBEq, pos, varSyms],
                Return[emitNorm[aInfo["Key"], aInfo["Eq"], cpEq, ti]]];
            (* Select the face/role geometry that reproduces the stored CP
               under the verifier's convention.  When no combination does
               (a position truncated past CP_MAX_DEPTH, say), fall back to
               the sound emitNorm bridge instead of emitting a Critical-
               PairLemma whose geometry the verifier would replay into a
               different equation. *)
            geom = chooseCpGeometry[aInfo, ruleAEq, bInfo, ruleBEq,
                pos, cpEq];
            If[ MissingQ[geom],
                atpDbgFail["resolveCp.no-geometry@" <> ToString[ti]];
                Return[emitNorm[aInfo["Key"], aInfo["Eq"], cpEq, ti]]];
            cEq = geom["ConstructEq"];
            mEq = geom["MatchingEq"];
            cpN++;
            key = {"CriticalPairLemma", cpN};
            st = stmt[cpEq];
            AppendTo[entries, key -> <|
                "Statement" -> st,
                "Proof" -> <|
                    "Construct" -> geom["Construct"],
                    "Orientation" ->
                        cplOrient[geom["ConstructInfo"]["Eq"], cEq, varSyms],
                    "Rule" -> cplAsRule[cEq, varSyms],
                    "Side" -> 1,
                    "Subpattern" -> Extract[
                        cplAsRule[cEq, varSyms][[1]], pos],
                    "MatchingConstruct" -> geom["Matching"],
                    "MatchingOrientation" ->
                        cplOrient[geom["MatchingInfo"]["Eq"], mEq, varSyms],
                    "MatchingRule" -> cplAsRule[mEq, varSyms],
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
            ruleEq = {tL[te], tR[te]};
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
                            parentTe, parentReason, cSide0WlPos,
                            wlEq, wlSide, newCSide0WlPos,
                            sl, st, dir, newSide},
                        pInfo = resolveTrace[te["ParentA"]];
                        rInfo = resolveTrace[te["ParentB"]];
                        rTe = trace[[te["ParentB"] + 1]];
                        rEq = {tL[rTe], tR[rTe]};
                        swapped = TrueQ[pInfo["Swapped"]];
                        (* cSide0WlPos: WL position (1 or 2) that
                           corresponds to C engine's side=0.  Tracks
                           the swap convention through the chain.
                           CP-normalize NORM_STEPs (parent is CP or
                           inherited from CP): C side 0 maps to WL
                           position 2 if swapped (CP storage swap),
                           else WL position 1.  Interreduce NORM_STEPs
                           (parent is ORIENT/SIMPLIFY = the dropped
                           rule's trace entry): the rule's Lhs may be
                           at WL pos 1 or 2 depending on whether
                           atp_orient_and_add swapped the CP's sides
                           (KBO_LT case).  Subsequent NORM_STEPs
                           descending from a NORM_STEP/SIMPLIFY
                           inherit the parent's stored mapping
                           verbatim -- the rewrite preserves which
                           side is "side 0". *)
                        parentTe = trace[[te["ParentA"] + 1]];
                        parentReason = parentTe["Reason"];
                        cSide0WlPos = Which[
                            (* fresh interreduce NORM_STEP: parent is the
                               dropped rule's ORIENT/SIMPLIFY entry.  The
                               rule's Lhs (C side 0) sits at the WL
                               position where it appears in pInfo.Eq --
                               compute it fresh.  This MUST take
                               precedence over the inherited-mapping
                               branch below: the parent ORIENT carries a
                               CSide0WlPos inherited from its source CP,
                               but that mapping is for the CP's sides,
                               not the rule's post-orient sides, so
                               inheriting it flips the mapping when
                               atp_orient_and_add swapped the CP. *)
                            parentReason === $TraceOrient || parentReason === $TraceFvi || parentReason === $TraceSimplify,
                                If[ tL[parentTe] === pInfo["Eq"][[1]],
                                    1, 2],
                            (* chained NORM_STEP: parent is itself a
                               NORM_STEP that already computed its
                               mapping -- the rewrite preserves which
                               side is 'side 0', so inherit verbatim. *)
                            KeyExistsQ[pInfo, "CSide0WlPos"],
                                pInfo["CSide0WlPos"],
                            (* fresh CP child: WL stores cpEq =
                               {cte.Rhs, cte.Lhs}, so cte.Lhs (C side
                               0) sits at WL pos 2. *)
                            True,
                                If[ swapped, 2, 1]
                        ];
                        wlSide = If[ te["Side"] === 0,
                            cSide0WlPos, 3 - cSide0WlPos];
                        (* te records BOTH post-step sides (te.Lhs =
                           C-lhs, te.Rhs = C-rhs), so build the full
                           Statement directly from them mapped through
                           the swap convention -- don't splice
                           pInfo.Eq's unchanged side, which can diverge
                           from C's actual other side once the chain
                           switches which C side it rewrites. *)
                        wlEq = If[ cSide0WlPos === 1,
                            {tL[te], tR[te]},
                            {tR[te], tL[te]}];
                        newCSide0WlPos = cSide0WlPos;
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
                                "Orientation" -> dir * cplOrient[rInfo["Eq"], rEq, varSyms],
                                "ConstructSide" -> 1,
                                "InputOrientation" -> 1,
                                "Side" -> wlSide,
                                "OutputExpression" -> st,
                                "Source" -> "norm"
                            |>
                        |>];
                        <|"Key" -> sl, "Eq" -> wlEq,
                          "Swapped" -> swapped,
                          "CSide0WlPos" -> newCSide0WlPos|>
                    ],
                te["Reason"] === $TraceFvi,
                    (* Waldmeister RechtsUnfreiErzeugen (FVI) sibling.
                       The parent KBO_UN equation is unorientable; the
                       engine emitted a grounded substitution instance
                       (every free variable on one side that does not
                       appear on the other goes to cAtp1, the engine's
                       reserved minimal constant).  There is no rewrite
                       path from pEq to ruleEq, so the merged
                       orient/simplify arm's emitNorm BFS would
                       exhaust and abort the lift.

                       Verifier teaching for FVI is out of scope here;
                       emit a flat axiom-shape entry whose Statement is
                       the grounded equation.  Sound: the C engine's
                       KBO_GT(lhs, g_rhs) post-grounding gate
                       (src/atp/_.c:12547) already validated the
                       instance.  Downstream NORM_STEP / CP entries
                       that cite this rule treat it as a leaf, the
                       same way they treat an Axiom entry; the
                       Substitution / Source fields are introspection
                       only and not consumed by the verifier. *)
                    Module[{pInfo, pEq, lhsP, rhsP, lvP, rvP, freeVars,
                            minConstSym, key},
                        pInfo = resolveTrace[te["ParentA"]];
                        pEq = pInfo["Eq"];
                        {lhsP, rhsP} = pEq;
                        lvP = DeleteDuplicates @ Cases[lhsP,
                            v_Symbol /; cplVarQ[v, varSyms],
                            {0, Infinity}, Heads -> True];
                        rvP = DeleteDuplicates @ Cases[rhsP,
                            v_Symbol /; cplVarQ[v, varSyms],
                            {0, Infinity}, Heads -> True];
                        freeVars = Union[
                            Complement[rvP, lvP],
                            Complement[lvP, rvP]];
                        minConstSym = Symbol["cAtp1"];
                        slN++;
                        key = {$SubstitutionLemmaSym, slN};
                        AppendTo[entries, key -> <|
                            "Statement" -> stmt[ruleEq],
                            "Proof" -> <||>,
                            "Substitution" -> Thread[
                                freeVars -> minConstSym],
                            "Source" -> "fvi",
                            "Input" -> pInfo["Key"]
                        |>];
                        <|"Key" -> key, "Eq" -> ruleEq,
                          "Swapped" -> False|>
                    ],
                te["Reason"] === $TraceOrient || te["Reason"] === $TraceSimplify,
                    pInfo = resolveTrace[te["ParentA"]];
                    pEq = pInfo["Eq"];
                    (* When chain extraction is on AND the parent is a
                       NORM_STEP, the chain already terminates at the
                       equation that gets oriented -- the cplEqSetQ
                       check is redundant and dominates cost on large
                       traces.  Inherit directly.  When chain is off,
                       fall through to cplEqSetQ / emitNorm so the
                       BFS bridges the rule's normalization. *)
                    If[ (TrueQ[$AtpUseChain] && trace[[te["ParentA"] + 1]]["Reason"] === $TraceNormStep) || cplEqSetQ[ruleEq, pEq, varSyms],
                        Join[
                            <|"Key" -> pInfo["Key"], "Eq" -> pEq,
                              "Swapped" -> TrueQ[pInfo["Swapped"]]|>,
                            (* propagate CSide0WlPos when inherited
                               from a NORM_STEP (chained interreduce
                               etc.) so downstream NORM_STEPs find the
                               right side-to-position mapping. *)
                            If[ KeyExistsQ[pInfo, "CSide0WlPos"],
                                <|"CSide0WlPos" -> pInfo["CSide0WlPos"]|>,
                                <||>]
                        ],
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
        (* No-goal saturation: the conjPair is (0, 0) and mainSteps is
           empty.  Drive resolveTrace over each surviving main-rule
           (resolveRule maps live-rule index -> trace index), so the
           dataset surfaces ONLY the saturated rule set (the
           "compatible lemmas") as CriticalPairLemma entries with full
           Construct / Position / Rule / Orientation provenance the
           goal-directed buildCplDataset would emit. *)
        If[ conjPair === {0, 0} && mainSteps === {},
            Do[
                Quiet @ Check[resolveRule[k - 1], Null,
                    {General::stop}],
                {k, Length[mainRules]}];
            Throw[Join[axiomEntries, entries]]];

        (* The per-conjunct goal pairs: cjp is the single {lhs, rhs}
           when there is one goal, and already the list of pairs for a
           multi-goal conjunction (atpEncodeProblem's "ConjPair"
           shape). *)
        nGoals = Length[enc["ConjPairs"]];
        cjps = If[ nGoals > 1, cjp, {cjp}];

        (* WM-CLI parity: if a goal chain's Conclusion Statement is a
           trivial `lhs == lhs` (i.e. the last rewrite landed on an
           identity), WM emits TWO entries -- the normalize step as a
           SubstitutionLemma, then a trivial Conclusion -- where thvm
           would emit ONE Conclusion with Source -> "cpl".  Split to
           match the CLI sequence (parity_wm_wmcli +1 trajectory gap on
           every AbelianGroup/Group/Boolean case). *)
        splitTrivial[goalEntries_, g_] := If[
            Length[goalEntries] > 0 &&
            goalEntries[[-1, 1, 1]] === $ConclusionSym &&
            Extract[goalEntries[[-1, 2, "Statement"]], {1, 1}, HoldForm] === Extract[goalEntries[[-1, 2, "Statement"]], {1, 2}, HoldForm],
            Module[{lastEntry = goalEntries[[-1]], lastProof, lastStmt,
                    lhsHF, newSlKey, newSubLem, newConclusion},
                lastStmt = lastEntry[[2, "Statement"]];
                lastProof = lastEntry[[2, "Proof"]];
                lhsHF = Extract[lastStmt, {1, 1}, HoldForm];
                slN += 1;
                newSlKey = {$SubstitutionLemmaSym, slN};
                newSubLem = newSlKey -> <|
                    "Statement" -> lastStmt,
                    "Proof" -> Append[lastProof, "Source" -> "norm"]
                |>;
                newConclusion = {$ConclusionSym, g} -> <|
                    "Statement" -> lastStmt,
                    "Proof" -> <|
                        "Input" -> newSlKey,
                        "Construct" -> newSlKey,
                        "Position" -> {},
                        "Rule" -> ReleaseHold[Hold[Rule][lhsHF, lhsHF]],
                        "Orientation" -> 1,
                        "ConstructSide" -> 1,
                        "InputOrientation" -> 1,
                        "Side" -> 1,
                        "OutputExpression" -> lastStmt,
                        "Source" -> "trivial"
                    |>
                |>;
                Join[Most[goalEntries], {newSubLem, newConclusion}]
            ],
            goalEntries
        ];

        (* the goal chains: conjunct g's MainSteps slice rewrites one
           side of that conjunct's running equation, citing its
           (resolved) rule, and closes into {Conclusion, g}.  A
           multi-goal step's Side is tagged 2*g + side by the C bridge
           (atp_extract_goal_chains); nGoals == 1 keeps the historical
           0/1 values, where the Quotient/Mod split is the identity.
           An empty slice is legitimate only for a reflexive conjunct
           -- a non-reflexive conjunct with no steps (an MNF-only join
           inside a conjunction) is not single-NF expressible, so the
           lift fails. *)
        chainEntries = Join @@ Table[
            Block[{gSteps, runEq, prevChainKey},
                gSteps = Select[mainSteps,
                    Quotient[#["Side"], 2] === g - 1 &];
                gSteps = Append[#, "Side" -> Mod[#["Side"], 2]] & /@ gSteps;
                runEq = cjps[[g]];
                prevChainKey = {$HypothesisSym, g};
                If[ gSteps === {},
                    If[ runEq[[1]] === runEq[[2]],
                        {trivialConclusionEntry[
                            Inactive[Equal] @@ runEq, g]},
                        atpDbgFail["buildCplDataset.empty-goal-chain"];
                        Throw[$Failed]],
                    splitTrivial[
                        Table[
                            Block[{step = gSteps[[s]], cInfo, cKey, mr,
                                   dir, ori, ruleEq, newEq, st, isLast,
                                   key, inKey},
                                (* MNF cites its rule by TRACE index
                                   (resolveTrace), completion cites by
                                   live-rule index (resolveRule ->
                                   rTrace -> resolveTrace).  Both land
                                   on the same Axiom /
                                   CriticalPairLemma key; mr is the
                                   rule's oriented equation, read off
                                   mainRules for a live rule or the
                                   trace node for an MNF citation. *)
                                cInfo = If[ usingMnf,
                                    resolveTrace[step["RuleC"]],
                                    resolveRule[step["RuleC"]]];
                                cKey = cInfo["Key"];
                                mr = If[ usingMnf,
                                    With[{te = trace[[step["RuleC"] + 1]]},
                                        {tL[te], tR[te]}],
                                    mainRules[[step["RuleC"] + 1]]];
                                (* Ordered rewriting in the C engine
                                   fires whichever direction strictly
                                   decreases the redex.  Fwd = 1 for an
                                   lhs->rhs application, 0 for
                                   rhs->lhs.  The verifier reads the
                                   Construct's Statement direction
                                   determined by Orientation =
                                   step-direction * entry-vs-rule
                                   alignment, so the SubstitutionLemma
                                   replays the same rewrite. *)
                                dir = If[ step["Fwd"] === 1, 1, -1];
                                ori = dir * cplOrient[cInfo["Eq"], mr, varSyms];
                                ruleEq = If[ dir === -1, Reverse[mr], mr];
                                newEq = ReplacePart[runEq,
                                    step["Side"] + 1 -> step["After"]];
                                runEq = newEq;
                                isLast = s === Length[gSteps];
                                key = If[ isLast, {$ConclusionSym, g},
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
                            {s, Length[gSteps]}
                        ],
                        g]
                ]
            ],
            {g, Length[cjps]}
        ];

        (* Order is axioms, hypotheses, then `entries` in emission
           order -- resolveTrace is depth-first, so a lemma is
           always emitted after the entries it cites -- then the
           goal chains.  The verifier replays entries in order and
           needs every Construct / Input already defined, so this
           dependency order must NOT be re-sorted. *)
        allEntries = Join[
            axiomEntries,
            Table[
                {$HypothesisSym, g} -> <|
                    "Statement" -> stmt[cjps[[g]]],
                    "Proof" -> <||>
                |>,
                {g, Length[cjps]}],
            entries,
            chainEntries
        ];
        allEntries
    ]]
]

End[];
EndPackage[];
