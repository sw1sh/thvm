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

TFindEquationalProof::usage = "TFindEquationalProof[conjecture, axioms] runs thvm's C ATP completion engine and returns a real WL ProofObject -- the same head FindEquationalProof returns, supporting the full property interface (p[\"ProofDataset\"], p[\"ProofGraph\"], p[\"ProofFunction\"], p[\"ProofLength\"], etc.).  TFindEquationalProof[\"Theorem\", \"Theory\"] resolves the theorem and theory names through AxiomaticTheory; a theorem stated as a multi-equation conjunction (an n-element list, e.g. BooleanAxioms `DeMorgan`) returns a List of n ProofObjects, one per conjunct.  TFindEquationalProof[conjecture, \"Theory\"] proves a given conjecture (an equation, a list of equations, or an Association whose Values are taken -- e.g. the whole AxiomaticTheory[\"Theory\", \"NotableTheorems\"] table) against the axioms of the named theory.  The C engine saturates the axioms; the resulting equational rewrite chain is decoded into a verifier-shaped ProofObject.  Returns $Failed when the conjecture is not proved.  An optional LAST positional argument selects the return type: a String, a list of Strings, or All, drawn from {\"ProofObject\", \"Lemmas\", \"PreprocessedAxioms\", \"RelevantAxioms\", \"RawTrace\", \"Statistics\", \"Status\"}.  A single String returns that one value bare; a list returns an Association keyed by the requested names; All returns an Association of every spec.  The default (\"ProofObject\") returns the bare ProofObject, so existing calls are unchanged.  \"Lemmas\" gives the completed rule set as Inactive[Equal] equations; \"PreprocessedAxioms\" the normalized axioms fed to the engine; \"RelevantAxioms\" the TRelevantAxioms <|\"Mode\",\"Kept\",\"Dropped\"|> partition; \"RawTrace\" the decoded completion trace; \"Statistics\" a small run-stats Association; \"Status\" a \"Proved\"/\"Saturated\"/\"TimedOut\"/\"Failed\" tag.  SINGLE-ARGUMENT COMPLETION: TFindEquationalProof[axioms] (a list of axiom equations) or TFindEquationalProof[\"Theory\"] (a named theory) runs a time-constrained completion with NO goal -- it saturates the axioms and returns the derived lemmas (default return \"Lemmas\"; pass a return spec as the 2nd argument, e.g. TFindEquationalProof[axioms, \"RawTrace\"]).  Bound completion with MaxWallSeconds / TimeConstraint, since a non-terminating axiom set never saturates.  Options: MaxSteps (CP-processing cap, default 200000); MaxWallSeconds (wall-clock budget, 0.=unbounded -- bounds non-terminating recursive-axiom saturations); TimeConstraint (wall-clock seconds, default Infinity = fall back to MaxWallSeconds; TimeConstrained[...] and Abort[] also interrupt the running C engine); Method (Automatic | \"Portfolio\" -- Waldmeister-style strategy schedules that try a list of configs in turn, returning the first that proves+verifies.  \"Portfolio\" is the FIXED schedule (Mix2 weight, then LPO+AutoPrecedence, then GT weight, then GoalDirected).  Automatic is PROBLEM-AWARE: it analyzes the axioms + conjecture, detects the algebraic structure (a port of Waldmeister's PhilMarlow/XFiles structure recognition), and FRONT-LOADS a tailored config for that structure (e.g. Group/AbelianGroup -> GT weight + AutoPrecedence; Ring -> KBO + AutoPrecedence; Combinatory -> Add weight + LPO; AC -> GT weight; Sheffer/Nand -> GoalDirected MNF front), then APPENDS the full fixed \"Portfolio\" as a fallback tail -- so Automatic only REORDERS and can never prove less than \"Portfolio\".  Or a single explicit config {\"Completion\" (or \"GoalDirected\"), \"CriticalPairWeight\"->\"Add\"|\"Max\"|\"Ord\"|\"Gt\"|\"Mix\"|\"Mix2\"|\"Unif\"|\"Goal\"(CPinGoal goal-directed), \"Ordering\"->\"KBO\"|\"LPO\", \"AutoPrecedence\"->True|False, \"AxiomRelevance\"->None|\"Safe\"|\"Connected\", \"MaxWeight\"->n (drop CPs over n symbols; 0=unbounded), \"GoalInterleave\"->n (every n-th selection is goal-directed), \"GroundJoin\"->True (delete ground-joinable CPs -- a sound Martin-Nipkow/Twee redundancy criterion), \"SelectionRatio\"->n (Waldmeister CPdimension fairness: 1 FIFO pick per n selections, default 11), \"RHSInterreduce\"->True (Waldmeister IR_InterreduktionRechts: normalize the RHS of every rule against each new rule, keeping R reduced), \"UnfailingCP\"->True (superpose BOTH faces of an unorientable equation -- unfailing completion's completeness requirement; the default overlaps the stored lhs only)}.  Method->\"Waldmeister\" is a preset for Waldmeister's faithful DEFAULT strategy on an unrecognized (single-operator Sheffer/Wolfram nand) problem: Mix weight + KBO + AutoPrecedence + SelectionRatio 51 (itl(mi)) + RHSInterreduce + UnfailingCP + GroundJoin.  Method exposes the saturator's CP-selection heuristic, reduction ordering, Waldmeister structure-driven precedence, the axiom-relevance filter (inspect with TRelevantAxioms), critical-pair redundancy, interreduction, and queue fairness.  Under a portfolio, MaxWallSeconds bounds EACH scheduled config (default 60s per config when unset).";

TRelevantAxioms::usage = "TRelevantAxioms[conjecture, axioms] reports which axioms the relevance filter keeps vs. drops for proving conjecture, without running a proof -- making the filter transparent.  TRelevantAxioms[\"Theorem\", \"Theory\"] resolves names through AxiomaticTheory.  Returns <|\"Mode\"->..., \"Kept\"->{axioms}, \"Dropped\"->{<|\"Axiom\", \"Symbols\", \"Reason\"|>...}|>.  The relevance mode is set by the Method \"AxiomRelevance\" suboption: None (keep all); \"Safe\" (default -- drop only provably dead-weight axioms: a confined symbol occurring on both sides, e.g. the Y combinator when the goal is Y-free; sound and completeness-preserving); \"Connected\" or {\"Connected\", \"FrequencyCutoff\"->f, \"MaxGenerations\"->n} (SInE-style symbol-connectivity pruning -- heuristic, may drop a needed axiom).";

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
   Boolean XOR Orderless interaction on DeMorgan). *)
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
    {{"NumericArray", "Shared"}, Integer, Integer, Real,
     Integer, Integer, Integer, Integer, Integer, Integer, Integer, Integer,
     Integer, Integer, Integer},
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
    (* Completion mode: conjecture === None means "no goal" -- the
       packed goal pair is (0, 0), which the C runner reads as "saturate
       the axioms" instead of running a goal check. *)
    If[ Unevaluated[conjecture] === None,
        cjHC = HoldComplete[None];
        goalLhs = 0; goalRhs = 0,
    (* else: a real conjecture. *)
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
        st = goalRes[[3]]
    ];
    (* Extract {lhs, rhs} pairs from each (stripped) held axiom
       directly via positions {1,1}/{1,2} of
       HoldComplete[lhs==rhs].  Avoids ReleaseHold which
       auto-evaluates `a == a` axioms to True. *)
    axPairs = (
        {Extract[#, {1, 1}], Extract[#, {1, 2}]} & /@ axHCs
    );
    conjPair = If[ Unevaluated[conjecture] === None, {0, 0},
        {Extract[cjHC, {1, 1}], Extract[cjHC, {1, 2}]}];
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
cEngineProof[enc_, maxSteps_, wallSeconds_:0.0,
    cpWeight_:-1, ordering_:0, autoPrec_:0, useMnf_:0,
    maxCpWeight_:0, goalInterleave_:0, groundJoin_:0, selRatio_:0,
    autoMaxWeight_:0, rhsInterreduce_:0, unfailingCP_:0] := Block[{
    raw, status, nRules, nTrace, nSteps, extNRules, extNSteps,
    mnfNSteps, cur, labelToName, idToName, mainSteps, extSteps,
    mnfSteps, mainRules, rTrace, traceEntries
},
    raw = Normal @ $atpRunProofFn[enc["Packed"], maxSteps, enc["MaxLab"],
        N[wallSeconds], cpWeight, ordering, autoPrec, useMnf, maxCpWeight,
        goalInterleave, groundJoin, selRatio, autoMaxWeight, rhsInterreduce,
        unfailingCP];
    status = raw[[1]];
    nRules = raw[[2]]; nTrace = raw[[3]]; nSteps = raw[[5]];
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
        "L2N" -> labelToName, "I2N" -> idToName,
        (* every variable symbol the decode produced: the named
           encoder vars plus any "x<id>" fallbacks for FVR ids
           completion introduced past the original signature.  Both
           the dataset builder and the ProofObject "Variables" list
           need the complete set, or the verifier reads a stray
           completion variable as a constant. *)
        "VarSyms" -> Union[
            Symbol /@ Values[idToName],
            Cases[{mainRules, mainSteps, extSteps, mnfSteps},
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
            v_Symbol /; (MemberQ[varSyms, v] || atpXVarQ[v]), {0, Infinity},
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
    v_Symbol /; (MemberQ[varSyms, v] || atpXVarQ[v]), {0, Infinity}, Heads -> True]

(* Rewrite each first-order variable occurrence (members of
   varSyms) in `term` as Pattern[v, Blank[]].  The var -> var_ rules
   are built with MapThread so no Pattern lands on a RuleDelayed
   rhs.  The verifier computes a step's expected result from the
   cited rules' patterned lhs's, so every dataset Statement that
   carries variables must be patternized to match. *)
atpXVarQ[s_Symbol] := StringMatchQ[SymbolName[s], "x" ~~ DigitCharacter ..];
atpXVarQ[_] := False;

(* Patternize the completion variables in `term`: the named vars in
   varSyms plus any "x<id>" symbol decodeAtpTerm minted for a fresh
   FVR completion introduced past the signature.  Recognizing the
   x-pattern here means varSyms need only carry the named vars, so the
   trace need not be eagerly scanned for completion vars. *)
cplPatternize[term_, varSyms_] := term /.
    (v_Symbol /; (MemberQ[varSyms, v] || atpXVarQ[v])) :> Pattern[v, Blank[]]

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
        prepareRules, runBfs, emitNorm, resolveCp, resolveTrace,
        resolveRule, axiomEntries,
        cjp, hypKey, chainEntries, runEq, prevChainKey, allEntries,
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
        (* Precompute the rule data once per emitNorm call: cplAsRule
           is O(term-size) and was previously rebuilt for every BFS
           node, dominating cost on large alive sets.  Returns a list
           of {traceIdx, fwdRule, revSafe, revRule|Null}. *)
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
           O(1) push/pop -- the prior AppendTo/Rest was O(n) per step
           and dominated cost on long BFS runs.  `wallSec` bounds the
           single-BFS wall time (Infinity == no bound). *)
        runBfs[start_, target_, preRules_, tryReverse_, cap_,
               wallSec_:Infinity] :=
            Block[{queue, seenA, found = Missing[], explored = 0,
                   nbrs, t0 = AbsoluteTime[]},
                queue = CreateDataStructure["Queue"];
                queue["Push", {start, {}}];
                seenA = <|start -> True|>;
                While[ queue["Length"] > 0 && MissingQ[found]
                       && explored < cap
                       && (wallSec === Infinity ||
                           AbsoluteTime[] - t0 < wallSec),
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

        (* re-derive startEq ->* targetEq, replaying with the rules
           alive when the rule at trace `ti` was oriented; emit a
           SubstitutionLemma per step; return the final entry's
           {key, eq}.  inKey is the dataset key of startEq. *)
        emitNorm[inKey_, startEq_, targetEq_, ti_] :=
            Block[{aliveList, preRules, found, curKey, curEq, st,
                   cInfo, rEq},
                aliveList = aliveRulesAt[ti];
                (* Precompute rule data once, share across both BFS
                   phases -- cplAsRule was previously the dominant per-
                   node cost. *)
                preRules = prepareRules[aliveList];
                (* Two-phase BFS.  Phase 1 is forward-only with a large
                   cap -- preserves the pre-reverse behavior on every
                   case the original engine handled (no behavior change
                   to byte-identical proofs).  Phase 2 only runs when
                   Phase 1 exhausts: it re-explores with reverse
                   direction enabled (variable-safe rules only) and a
                   tight cap, which unlocks ordered-rewriting paths
                   the forward-only BFS could not reach.  A per-BFS
                   wall budget (2 seconds) keeps a single pathological
                   normalization from monopolizing the wrapper's time
                   budget; we throw $Failed and let the outer two-
                   phase retry / FindEquationalProof fall through. *)
                found = runBfs[startEq, targetEq, preRules, False, 50000, 2];
                If[ MissingQ[found],
                    found = runBfs[startEq, targetEq, preRules, True, 600, 2]];
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
            cpEq = {tR[cte], tL[cte]};
            pos = cte["Pos"];
            aTe = trace[[cte["ParentA"] + 1]];
            bTe = trace[[cte["ParentB"] + 1]];
            ruleAEq = {tL[aTe], tR[aTe]};
            ruleBEq = {tL[bTe], tR[bTe]};
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
                            parentReason === $TraceOrient ||
                              parentReason === $TraceSimplify,
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
                                "Orientation" -> dir *
                                    cplOrient[rInfo["Eq"], rEq, varSyms],
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
                (* MNF cites its rule by TRACE index (resolveTrace),
                   completion cites by live-rule index (resolveRule ->
                   rTrace -> resolveTrace).  Both land on the same
                   Axiom / CriticalPairLemma key; mr is the rule's
                   oriented equation, read off mainRules for a live
                   rule or the trace node for an MNF citation. *)
                cInfo = If[ usingMnf, resolveTrace[step["RuleC"]],
                    resolveRule[step["RuleC"]]];
                cKey = cInfo["Key"];
                mr = If[ usingMnf,
                    With[{te = trace[[step["RuleC"] + 1]]},
                        {tL[te], tR[te]}],
                    mainRules[[step["RuleC"] + 1]]];
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

(* Method-option vocabulary.  The completion suboptions map onto the
   C engine's runtime knobs (thvm_atp_set_cp_weight_mode /
   thvm_atp_set_lpo / atp_auto_precedence / thvm_atp_set_use_mnf),
   threaded through cEngineProof as the
   {cpWeight, ordering, autoPrec, useMnf} tuple.

   "CriticalPairWeight" -- Waldmeister ClasHeuristics CP-selection
     weight (which pending critical pair to process next):
       "Add"  CH_AddWeight  (bare wl+wr symbol-count sum)
       "Max"  CH_MaxWeight
       "Ord"  CH_OrdWeight
       "Gt"   CH_GtWeight    (ordering-directed; engine default)
       "Mix"  CH_MixWeight   "Mix2" CH_MixWeight2
       "Unif" CH_Unifikationsmass
       Automatic -> engine default (Gt).
   "Ordering" -- reduction ordering: "KBO" (default) | "LPO" |
     Automatic (-> KBO).
   "AutoPrecedence" -- True/Automatic = Waldmeister structure-driven
     precedence (PhilMarlow port), False = identity precedence. *)
$AtpCpWeightCodes = <|
    "Add" -> 0, "Max" -> 1, "Ord" -> 2, "Gt" -> 3,
    "Mix" -> 4, "Mix2" -> 5, "Unif" -> 6,
    "Goal" -> 7, "CPinGoal" -> 7, Automatic -> -1
|>;

TFindEquationalProof::badmethod =
    "Unrecognized Method `1`; using Automatic (completion).";
TFindEquationalProof::badcpw =
    "Unrecognized \"CriticalPairWeight\" `1`; using engine default.";
TFindEquationalProof::dropax =
    "Axiom-relevance filter (mode `2`) dropped axiom(s) keyed on \
symbol set(s) `1` as irrelevant to the conjecture.  Mode \"Safe\" is \
sound and completeness-preserving (the symbols are private to the \
dropped axiom and occur on both sides, so it cannot enter a proof of \
this goal); mode \"Connected\" is a heuristic and may drop a needed \
axiom.  Inspect with TRelevantAxioms, or set \"AxiomRelevance\" -> \
None in Method to keep every axiom.";
TFindEquationalProof::badrel =
    "Unrecognized \"AxiomRelevance\" `1`; using \"Safe\".";

(* parse a Method spec into {cpWeight, ordering, autoPrec, useMnf} ints
   for cEngineProof.  Automatic = Mix2 critical-pair weight
   (CH_MixWeight2, g*10 + (wl+wr)), KBO, identity precedence, MNF off.
   Mix2 reaches the proof far sooner than the engine's bare GT default
   on the harder associativity / cross-axiom Boolean theorems (e.g.
   MeredithAxioms AndAssociativity: ~7s under Mix2 vs ~60s under GT)
   while leaving the easy cases and atp.wlt unchanged.  Auto-precedence,
   LPO, and MNF change the search globally, so they stay opt-in via
   explicit Method suboptions / "GoalDirected".

   useMnf = the 4th element flips the runtime MNF goal-directed front
   search (thvm_atp_set_use_mnf).  The paclet dylib always compiles MNF
   in (WL_ATP_MNF), so "GoalDirected" no longer falls back -- it asks
   the engine to run the bidirectional collision search alongside
   completion, the only detector that closes a symmetric goal whose two
   sides never share a single normal form. *)
(* "MaxWeight" -> n (Waldmeister MaxWeight): drop critical pairs whose
   combined term weight exceeds n; 0 / Automatic = unbounded. *)
atpMaxWeightOpt[o_Association] := With[{w = Lookup[o, "MaxWeight", 0]},
    If[ IntegerQ[w] && w > 0, w, 0]];
(* "GoalInterleave" -> n: every n-th CP selection is a goal-directed
   (CPinGoal) pick; the rest use the chosen weight.  0/Automatic = off. *)
atpGoalInterleaveOpt[o_Association] := With[{n = Lookup[o, "GoalInterleave", 0]},
    If[ IntegerQ[n] && n > 0, n, 0]];
(* "GroundJoin" -> True: drop ground-joinable critical pairs (a sound
   Martin-Nipkow / Twee redundancy criterion -- every ground instance of
   the CP joins, so it adds nothing).  False/Automatic = off. *)
atpGroundJoinOpt[o_Association] := Switch[Lookup[o, "GroundJoin", Automatic],
    True, 1, False | Automatic, 0, _, 0];
(* "SelectionRatio" -> n (Waldmeister CPdimension / YFiles Schrittweiten):
   1 FIFO (oldest-CP) pick per n CP selections, the rest by weight.  The
   fairness lever against smallest-weight starvation.  0/Automatic keeps
   the engine default (11); Waldmeister also uses 50/100/200. *)
atpSelectionRatioOpt[o_Association] := With[{n = Lookup[o, "SelectionRatio", 0]},
    If[ IntegerQ[n] && n > 0, n, 0]];
(* "AutoMaxWeight" -> b: a growing CP-weight bound (base b + 2*deepest-
   rule-weight) that defers over-weight critical pairs to a stash and
   force-drains them when the active queue empties.  Keeps the CP queue
   small (measured ~3.5x) WITHOUT losing completeness (nothing is dropped).
   0/Automatic = off. *)
atpAutoMaxWeightOpt[o_Association] := With[{b = Lookup[o, "AutoMaxWeight", 0]},
    If[ IntegerQ[b] && b > 0, b, 0]];
(* "RHSInterreduce" -> True: Waldmeister IR_InterreduktionRechts -- after
   a rule is oriented, normalize the RHS of every other rule against it,
   re-queuing any rule whose RHS shrinks.  Keeps R fully reduced so the
   CP set stays small.  True/Automatic-when-Waldmeister = on; the engine
   default (and other methods) leave it off. *)
atpRHSInterreduceOpt[o_Association] := Switch[Lookup[o, "RHSInterreduce", Automatic],
    True, 1, False | Automatic, 0, _, 0];
(* "UnfailingCP" -> True: superpose BOTH faces of every unorientable
   equation (unfailing completion's completeness requirement).  True =
   on; False/Automatic = off (the default lhs-only overlap). *)
atpUnfailingCPOpt[o_Association] := Switch[Lookup[o, "UnfailingCP", Automatic],
    True, 1, False | Automatic, 0, _, 0];
atpParseMethod[Automatic] := {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
atpParseMethod["Completion"] := atpParseMethod[{"Completion"}];

(* Shared suboption decoder for the completion-family methods.  Returns
   {cpWeight, ordering, autoPrec, useMnf}; `mnf` is fixed by the head
   ("Completion" -> 0, "GoalDirected"/"MNF" -> 1) so every method takes
   the same CriticalPairWeight / Ordering / AutoPrecedence knobs. *)
atpParseCompletionOpts[subopts_List, mnf_] :=
    Block[{o = Association[subopts], cw, ord, ap, cwRaw},
        cwRaw = Lookup[o, "CriticalPairWeight", Automatic];
        cw = Lookup[$AtpCpWeightCodes, cwRaw, $Failed];
        If[ cw === $Failed,
            Message[TFindEquationalProof::badcpw, cwRaw]; cw = -1];
        ord = Switch[Lookup[o, "Ordering", Automatic],
            "LPO", 1, "KBO" | Automatic, 0, _, 0];
        ap = Switch[Lookup[o, "AutoPrecedence", Automatic],
            True, 1, False | Automatic, 0, _, 0];
        {cw, ord, ap, mnf, atpMaxWeightOpt[o], atpGoalInterleaveOpt[o],
         atpGroundJoinOpt[o], atpSelectionRatioOpt[o], atpAutoMaxWeightOpt[o],
         atpRHSInterreduceOpt[o], atpUnfailingCPOpt[o]}
    ];
atpParseMethod[{"Completion", subopts___Rule}] :=
    atpParseCompletionOpts[{subopts}, 0];

(* "GoalDirected" / "MNF": enable the front search.  Bare form defaults
   to Mix2 weight (like Automatic) so completion still drives R forward
   while MNF watches for a front collision; the list form takes the same
   Ordering / AutoPrecedence / CriticalPairWeight knobs as "Completion"
   so the front search can run over an LPO-oriented, structure-precedence
   rule set -- the combination the hard Sheffer cross-axiom goals need. *)
atpParseMethod[m : ("GoalDirected" | "MNF")] := {5, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
atpParseMethod[{("GoalDirected" | "MNF"), subopts___Rule}] :=
    atpParseCompletionOpts[{subopts}, 1];

(* Method -> "Waldmeister": the faithful Waldmeister DEFAULT strategy for
   an unrecognized (single-operator nand / Sheffer / Wolfram) problem --
   the "Orkus" fallback StdS = kbo(std), itl(mi), zb(mnf) (Sinai.h:109,
   :131) plus the default mixweight classification (Parameter.c:165).
   Decoded into thvm knobs:
     - CriticalPairWeight -> "Mix"  (default heuristic=mixweight, the
       CH_MixWeight formula, ClasHeuristics.c:130)
     - Ordering -> "KBO", AutoPrecedence -> True  (kbo(std) with the
       Praezedenzgenerator auto-precedence)
     - SelectionRatio -> 51  (itl(mi) = interleave fifo:heuristic 1:50,
       YFiles.c:114-122; CPdimension fairness, KPVerwaltung.c:582)
     - RHSInterreduce -> True  (IR_InterreduktionRechts -- the
       divergence that made the deep theorems unreachable)
     - GroundJoin -> True  (sound CP redundancy, keeps the queue lean)
   List form takes the same suboptions, overriding any default. *)
atpParseMethod["Waldmeister"] := atpParseMethod[{"Waldmeister"}];
atpParseMethod[{"Waldmeister", subopts___Rule}] :=
    Block[{o = Association[{subopts}], merged},
        merged = Join[<|
            "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO",
            "AutoPrecedence" -> True, "SelectionRatio" -> 51,
            "RHSInterreduce" -> True, "GroundJoin" -> True,
            "UnfailingCP" -> True|>, o];
        atpParseCompletionOpts[Normal[merged], 0]
    ];

atpParseMethod[m_] := (
    Message[TFindEquationalProof::badmethod, m]; {-1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0});

(* Strategy schedule (Waldmeister-style portfolio).  Automatic and
   "Portfolio" expand to an ORDERED list of concrete Method configs
   tried in turn until one proves+verifies; a concrete Method is its
   own one-element schedule (no portfolio).  Ordering matters: cheap,
   broadly-effective configs first so easy goals close immediately and
   only hard goals pay for the later ordering/weight variants.
     1. Mix2 weight   -- the single best general weight (default).
     2. LPO + auto-precedence -- structural / combinator reductions
        KBO cannot orient (variable-duplicating rules).
     3. GT weight     -- the engine's bare default; occasionally
        reaches a proof the others' CP order misses.
     4. GoalDirected  -- the MNF bidirectional front search, the only
        config that closes a symmetric goal whose two sides never meet
        at a single normal form (Boolean Noncontradiction /
        ExcludedMiddle / DoubleNegation, Sheffer Commutativity).  Last
        because it runs the front search alongside completion on every
        step, so a goal the cheaper completion configs already close
        never pays for it. *)
$AtpSchedule = {
    {"Completion", "CriticalPairWeight" -> "Mix2"},
    {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True},
    {"Completion", "CriticalPairWeight" -> "Gt"},
    "GoalDirected"
};
(* "Portfolio" is the FIXED schedule above (prior behavior, kept
   reachable verbatim).  Automatic is now PROBLEM-AWARE: it front-loads
   a tailored config for the detected algebraic structure, then APPENDS
   $AtpSchedule as a fallback tail so it can never prove less than the
   fixed portfolio (see atpAutoTune).  atpScheduleFor's two-arg form
   threads the axioms+conjecture so Automatic can analyze them. *)
atpScheduleFor["Portfolio"] := $AtpSchedule;
atpScheduleFor["Portfolio", _, _] := $AtpSchedule;
atpScheduleFor[Automatic, axioms_, conjecture_] :=
    atpTunedSchedule[axioms, conjecture];
atpScheduleFor[Automatic] := $AtpSchedule;          (* no problem in hand *)
atpScheduleFor[m_, _, _] := {m};
atpScheduleFor[m_] := {m};

(* ====================================================================
   Problem-analysis auto-tuner  (port of Waldmeister's PhilMarlow /
   XFiles "structure recognition -> strategy database").

   PORTED EXACTLY (cited per function):
     - The law detectors (commutativity / associativity / idempotence /
       left+right unit / left+right inverse / distributivity) are a
       faithful WL re-statement of the C predicates in
       src/atp/precedence.c (prec_is_commutativity etc.), which are
       themselves ports of waldmeister TermOperationen.c's
       TO_IstKommutativitaet / TO_IstAssoziativitaet /
       TO_IstDistribution and the Sinai.h Tafel1 law equations
       (Sinai.h:54-101).  The arg-order and orientation handling match
       precedence.c:51-189 line for line.
     - The structure -> strategy assignments reproduce the relevant
       rows of waldmeister Tafel2 (Sinai.h:113-131), decoded through
       YFiles.c into thvm Method knobs:
         StdS  = kbo(std),itl(mi),zb(mnf)        (Sinai.h:109)
                 -> KBO, GoalInterleave 50, GoalDirected(mnf front)
         GtS   = kbo(std),cph(gt),itl(mi)        (Sinai.h:110)
                 -> KBO, CriticalPairWeight Gt, GoalInterleave 50
         KombS = rose(),kbo(std),cph(add),...    (Sinai.h:111)
                 -> KBO, CriticalPairWeight Add (combinator logic)
       The Tafel2 assignments used: Gruppe / BoolescheAlgebra ->
       GtS (Sinai.h:114-116); WajsbergAlgebra / LDAlgebra -> StdS
       (Sinai.h:118,124); Ring -> kbo(Std) (Sinai.h:122);
       Kombinatorlogik* -> KombS (Sinai.h:125-128); itl/cph/zb decoded
       via YFiles.c:78-122 (itl(mi) = -pq interleave=1.50;
       cph(gt) = gtweight; cph(add) = addweight; zb(mnf) = MNF goal
       normal-form front search; kbo = -ord kbo).

   APPROXIMATED-IN-SPIRIT (NOT a literal port):
     - Structure RECOGNITION.  Waldmeister's XFiles.c builds Komprimate
       and runs the Tafel3 premise-matching subsumption engine
       (PhilMarlow.c PraemissenDurchgenudelt / ZuordnungQuasiSpezieller)
       to pick the most-specific structure.  We do NOT port that table
       machinery; instead we classify directly from the per-operator law
       flags into a coarse class (AC / Group / AbelianGroup / Monoid /
       Ring / Sheffer / Combinatory / Lattice / General).  Faithful in
       intent, not in mechanism.
     - The exact YFiles parametrizations (mnfanalysis tuples, DEF/VAR
       weights, the 400/6000/14000 bis() step bounds) are NOT carried
       across; thvm's knobs are coarser (a single GoalInterleave int, a
       weight name, KBO/LPO, GroundJoin).  We pick the closest knob.
   ==================================================================== *)

(* {boundVars, lhs, rhs} of an axiom.  Handles ForAll[vars, l == r]
   (vars a single symbol or a List), a bare l == r, and an Inactive
   Equal.  Returns $Failed for a non-equation. *)
atpAxiomParts[ax_] := Block[{vars, eq, l, r},
    vars = Switch[ax,
        _ForAll, Flatten[{ax[[1]]}],
        _, {}];
    eq = ax /. ForAll[_, e_] :> e;
    eq = eq /. Inactive[Equal] -> Equal;
    If[ Head[eq] =!= Equal || Length[eq] =!= 2, Return[$Failed]];
    {l, r} = List @@ eq;
    {vars, l, r}
];

(* a term is the i-th bound variable. *)
atpIsVar[t_, vars_] := MemberQ[vars, t];

(* a binary application op[_, _] with op a non-variable head.  Returns
   the head, or $Failed.  Mirrors prec_is_binop (precedence.c:43). *)
atpBinHead[t_, vars_] := If[
    MatchQ[t, _[_, _]] && ! atpIsVar[Head[t], vars] && Head[t] =!= List,
    Head[t], $Failed];

(* Commutativity  f[x,y] == f[y,x].  Port of prec_is_commutativity
   (precedence.c:51) / TO_IstKommutativitaet. *)
atpLawCommutative[l_, r_, vars_] := Block[{f},
    f = atpBinHead[l, vars];
    f =!= $Failed && atpBinHead[r, vars] === f &&
        MatchQ[{l, r}, {f[a_, b_], f[b_, a_]}] &&
        atpIsVar[l[[1]], vars] && atpIsVar[l[[2]], vars] &&
        l[[1]] =!= l[[2]]
];

(* one orientation of associativity: a = f[f[x,y],z], b = f[x,f[y,z]].
   Port of prec_assoc_dir (precedence.c:65). *)
atpAssocDir[a_, b_, f_, vars_] :=
    MatchQ[a, f[f[x_, y_], z_] /;
        atpIsVar[x, vars] && atpIsVar[y, vars] && atpIsVar[z, vars] &&
        MatchQ[b, f[x, f[y, z]]]];

(* Associativity (either side flattened).  Port of
   prec_is_associativity (precedence.c:90). *)
atpLawAssociative[l_, r_, vars_] := Block[{f},
    f = atpBinHead[l, vars];
    If[ f === $Failed, f = atpBinHead[r, vars]];
    f =!= $Failed &&
        (atpAssocDir[l, r, f, vars] || atpAssocDir[r, l, f, vars])
];

(* Idempotence  f[x,x] == x.  Port of prec_is_idempotence
   (precedence.c:104) / Sinai Idempotenz. *)
atpLawIdempotent[l_, r_, vars_] := Block[{f},
    f = atpBinHead[l, vars];
    f =!= $Failed && atpIsVar[l[[1]], vars] && l[[1]] === l[[2]] &&
        r === l[[1]]
];

(* a constant (a nullary CTR): an atom that is not a bound variable, or
   a unit-wrapper like OverTilde[1] / OverBar[0] / a CircleTimes-free
   integer.  Waldmeister's prec_is_identity wants the unit slot to be a
   nullary CTR (precedence.c:126); in WL the "unit" is a ground term
   with no bound variable. *)
atpIsConstTerm[t_, vars_] := FreeQ[t, Alternatives @@ vars] &&
    ! atpIsVar[t, vars];

(* Left/right identity  f[e,x]==x  /  f[x,e]==x, e a constant.  Port of
   prec_is_identity (precedence.c:118).  side 0 = left, 1 = right. *)
atpLawIdentity[l_, r_, vars_, side_] := Block[{f, unit, var},
    f = atpBinHead[l, vars];
    If[ f === $Failed || ! atpIsVar[r, vars], Return[False]];
    unit = l[[side + 1]];
    var = l[[2 - side]];
    atpIsVar[var, vars] && var === r && atpIsConstTerm[unit, vars]
];

(* Left/right inverse  f[i[x],x]==e  /  f[x,i[x]]==e, i unary, e const.
   Port of prec_is_inverse (precedence.c:136).  side 0 = left. *)
atpLawInverse[l_, r_, vars_, side_] := Block[{f, inv, var},
    f = atpBinHead[l, vars];
    If[ f === $Failed || ! atpIsConstTerm[r, vars], Return[False]];
    inv = l[[side + 1]];
    var = l[[2 - side]];
    atpIsVar[var, vars] && MatchQ[inv, _[_]] && ! atpIsVar[Head[inv], vars] &&
        atpIsVar[inv[[1]], vars] && inv[[1]] === var
];

(* one orientation of left distributivity: a = f[x,g[y,z]],
   b = g[f[x,y],f[x,z]], f != g.  Port of prec_distrib_dir
   (precedence.c:155). *)
atpDistribDir[a_, b_, vars_] := Block[{f, g, x, inner},
    f = atpBinHead[a, vars];
    If[ f === $Failed || ! atpIsVar[a[[1]], vars], Return[$Failed]];
    x = a[[1]];
    inner = a[[2]];
    g = atpBinHead[inner, vars];
    If[ g === $Failed || g === f, Return[$Failed]];
    If[ ! atpIsVar[inner[[1]], vars] || ! atpIsVar[inner[[2]], vars] ||
        inner[[1]] === inner[[2]], Return[$Failed]];
    If[ MatchQ[b, g[f[x, inner[[1]]], f[x, inner[[2]]]]],
        {f, g}, $Failed]
];

(* Distributivity f over g (either orientation).  Port of
   prec_is_distributivity (precedence.c:186). *)
atpLawDistributes[l_, r_, vars_] := Block[{d},
    d = atpDistribDir[l, r, vars];
    If[ d === $Failed, d = atpDistribDir[r, l, vars]];
    d
];

(* === atpAnalyzeStructure ===========================================
   Walk the axiom list once, tag each operator with its laws, then
   classify into a coarse structure class.  Returns an Association:
     "Operators"      <|head -> <|"Commutative", "Associative",
                                   "Idempotent", "HasUnit", "HasInverse",
                                   "Distributes", "Arity"|> ...|>
     "ACOperators"    {heads that are commutative AND associative}
     "Class"          "AC" | "Group" | "AbelianGroup" | "Monoid" |
                      "Ring" | "Sheffer" | "Combinatory" | "Lattice" |
                      "General"
     "NOperators", "MaxArity", "NAxioms"
   conjecture is accepted for symmetry with atpAutoTune but does not
   change the class (Waldmeister classifies from the equation set;
   PhilMarlow.c:1422-1429). *)
atpAnalyzeStructure[axioms_List, conjecture_ : Null] := Block[{
    parts, vars, ops, allHeads, combinatorConsts, isSheffer, isComb,
    acOps, hasAssoc, hasComm, hasInv, hasUnit, hasIdem, hasDistrib,
    class, maxArity, arities},
    parts = DeleteCases[atpAxiomParts /@ axioms, $Failed];
    (* per-operator law flags *)
    ops = <||>;
    Module[{tag},
        tag[h_, key_] := ops[h] = Append[Lookup[ops, h, <|
            "Commutative" -> False, "Associative" -> False,
            "Idempotent" -> False, "HasUnit" -> False,
            "HasInverse" -> False, "Distributes" -> False,
            "Arity" -> 0|>], key -> True];
        Do[ Block[{vs = p[[1]], l = p[[2]], r = p[[3]], f, d},
            (* record arity of every applied non-variable head *)
            Scan[Function[t,
                If[ MatchQ[t, _[___]] && ! atpIsVar[Head[t], vs] &&
                    Head[t] =!= List && Head[t] =!= Equal,
                    ops[Head[t]] = Append[
                        Lookup[ops, Head[t], <|
                            "Commutative" -> False, "Associative" -> False,
                            "Idempotent" -> False, "HasUnit" -> False,
                            "HasInverse" -> False, "Distributes" -> False,
                            "Arity" -> 0|>],
                        "Arity" -> Length[t]]]],
                {l, r}, {0, Infinity}, Heads -> False];
            If[ atpLawCommutative[l, r, vs], tag[atpBinHead[l, vs], "Commutative"]];
            If[ atpLawAssociative[l, r, vs],
                f = atpBinHead[l, vs]; If[ f === $Failed, f = atpBinHead[r, vs]];
                tag[f, "Associative"]];
            If[ atpLawIdempotent[l, r, vs], tag[atpBinHead[l, vs], "Idempotent"]];
            Do[ Block[{a = If[sw == 1, r, l], b = If[sw == 1, l, r]},
                If[ atpLawIdentity[a, b, vs, 0], tag[atpBinHead[a, vs], "HasUnit"]];
                If[ atpLawIdentity[a, b, vs, 1], tag[atpBinHead[a, vs], "HasUnit"]];
                If[ atpLawInverse[a, b, vs, 0], tag[atpBinHead[a, vs], "HasInverse"]];
                If[ atpLawInverse[a, b, vs, 1], tag[atpBinHead[a, vs], "HasInverse"]]],
                {sw, 0, 1}];
            d = atpLawDistributes[l, r, vs];
            If[ d =!= $Failed, tag[First[d], "Distributes"]]],
            {p, parts}]
    ];
    acOps = Keys @ Select[ops, #["Commutative"] && #["Associative"] &];
    hasAssoc = AnyTrue[Values[ops], #["Associative"] &];
    hasComm = AnyTrue[Values[ops], #["Commutative"] &];
    hasInv = AnyTrue[Values[ops], #["HasInverse"] &];
    hasUnit = AnyTrue[Values[ops], #["HasUnit"] &];
    hasIdem = AnyTrue[Values[ops], #["Idempotent"] &];
    hasDistrib = AnyTrue[Values[ops], #["Distributes"] &];
    arities = #["Arity"] & /@ Values[ops];
    maxArity = If[arities === {}, 0, Max[arities]];
    (* Combinatory logic: a single binary "application" operator and the
       rest of the heads are nullary combinator CONSTANTS (S,K,B,...).
       Sinai Tafel3 Kombinatorlogik* use the "_" application op
       (Sinai.h:95-102, 348-362).  Heuristic: one binary op, >= 1
       nullary head, no commutativity/unit/inverse. *)
    allHeads = Keys[ops];
    isComb = Length[Select[Values[ops], #["Arity"] == 2 &]] == 1 &&
        AnyTrue[Values[ops], #["Arity"] == 0 &] &&
        ! hasComm && ! hasUnit && ! hasInv && Length[parts] <= 4;
    (* Sheffer / Nand: a single binary operator, no other structure
       (no associativity, commutativity is the GOAL not an axiom).
       WolframAxioms is the canonical case (one CenterDot axiom). *)
    isSheffer = Length[allHeads] == 1 && First[Values[ops]]["Arity"] == 2 &&
        ! hasAssoc && ! hasComm && ! hasUnit && ! hasInv && ! hasDistrib;
    class = Which[
        isComb, "Combinatory",
        isSheffer, "Sheffer",
        hasDistrib && hasInv, "Ring",
        hasInv && hasUnit && hasAssoc && hasComm, "AbelianGroup",
        hasInv && hasUnit && hasAssoc, "Group",
        hasIdem && hasComm && hasAssoc && ! hasInv, "Lattice",
        hasUnit && hasAssoc, "Monoid",
        hasComm && hasAssoc, "AC",
        True, "General"];
    <|"Operators" -> ops, "ACOperators" -> acOps, "Class" -> class,
      "NOperators" -> Length[ops], "MaxArity" -> maxArity,
      "NAxioms" -> Length[parts]|>
];
(* the named-theory / single-arg conveniences resolve through
   AxiomaticTheory like the rest of the surface. *)
atpAnalyzeStructure[theory_String] :=
    atpAnalyzeStructure[AxiomaticTheory[theory]];

(* === atpAutoTune ===================================================
   Map the detected structure class to an ORDERED list of tailored
   Method configs, FRONT of the schedule.  The mappings reproduce
   waldmeister Tafel2 (Sinai.h:113-131) decoded into thvm knobs (see
   the citation block above atpAxiomParts).  These are best-GUESS
   front-loads only -- the safety tail in atpTunedSchedule guarantees
   the fixed portfolio still runs if none close. *)
atpAutoTune[axioms_List, conjecture_ : Null] := Block[{prof = atpAnalyzeStructure[axioms, conjecture]},
    atpAutoTuneForClass[prof["Class"]]
];
atpAutoTune[theory_String, conjecture_ : Null] :=
    atpAutoTune[AxiomaticTheory[theory], conjecture];

(* Tafel2 row -> thvm config list.  Decoding key:
   GtS  -> Gt weight, KBO, GoalInterleave 50  (Sinai.h:110)
   StdS -> KBO, GoalInterleave 50, GoalDirected MNF front (Sinai.h:109)
   KombS-> Add weight, KBO                    (Sinai.h:111) *)
atpGtS = {"Completion", "CriticalPairWeight" -> "Gt",
    "GoalInterleave" -> 50};
atpStdS = {"Completion", "GoalInterleave" -> 50};
atpKombS = {"Completion", "CriticalPairWeight" -> "Add",
    "Ordering" -> "LPO", "AutoPrecedence" -> True};

atpAutoTuneForClass["AbelianGroup" | "Group"] := {
    (* Tafel2: Gruppe -> GtS (Sinai.h:115).  AutoPrecedence puts the
       unary inverse on top (Sinai Group precedence "+-0", Sinai.h:117/
       precedence.c:329) so i(i(x)) -> x orients cleanly. *)
    {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50,
        "AutoPrecedence" -> True},
    atpGtS};
atpAutoTuneForClass["Ring"] := {
    (* Tafel2: Ring -> kbo(Std) (Sinai.h:122); structure precedence
       puts the distributor "*" above "+" (Tafel3 Ring "*+",
       Sinai.h:243 / precedence.c:331). *)
    {"Completion", "Ordering" -> "KBO", "AutoPrecedence" -> True},
    atpGtS};
atpAutoTuneForClass["AC"] := {
    (* AC theories: GtS family -- a KBO with ordering-directed (gt) CP
       weight handles commutative+associative operators (Sinai Verband /
       group rows all use GtS / kbo, Sinai.h:114-122). *)
    atpGtS,
    {"Completion", "CriticalPairWeight" -> "Mix2", "GoalInterleave" -> 50}};
atpAutoTuneForClass["Lattice"] := {
    (* Tafel2: Verband -> kbo(Std),cph(gt),gj() ... lpo(std),itl(re)
       (Sinai.h:120) -- GroundJoin on, then an LPO pass. *)
    {"Completion", "CriticalPairWeight" -> "Gt", "GroundJoin" -> True,
        "GoalInterleave" -> 50},
    {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "GoalInterleave" -> 100}};
atpAutoTuneForClass["Monoid"] := {atpGtS, atpStdS};
atpAutoTuneForClass["Combinatory"] := {
    (* Tafel2: Kombinatorlogik* -> KombS = cph(add) (Sinai.h:111,125).
       Variable-duplicating combinator rules (S,W,M) need LPO. *)
    atpKombS,
    {"Completion", "CriticalPairWeight" -> "Add"}};
atpAutoTuneForClass["Sheffer"] := {
    (* No Tafel2 Sheffer row.  Sheffer/Nand goals (WolframAxioms) are
       symmetric and never share a normal form, so the MNF front search
       (StdS's zb(mnf), Sinai.h:109) is the closer; pair it with the
       Mix2 weight that thvm finds best on the hard cross-axiom Sheffer
       theorems. *)
    {"GoalDirected", "CriticalPairWeight" -> "Mix2", "AutoMaxWeight" -> 20},
    {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "AutoMaxWeight" -> 20}};
atpAutoTuneForClass[_] := {};   (* "General": no front-load, just tail *)

(* Front-load the tuned configs, then APPEND the full fixed portfolio
   as a fallback tail and DeleteDuplicates.  This is the safety
   constraint: Automatic only REORDERS -- every config the fixed
   portfolio runs is still present, so the tuned schedule can never
   prove strictly less than "Portfolio". *)
atpTunedSchedule[axioms_, conjecture_] := DeleteDuplicates @ Join[
    Quiet @ Check[atpAutoTune[axioms, conjecture], {}],
    $AtpSchedule];

(* === Axiom-relevance filter ======================================

   Prunes axioms that cannot (or, in the heuristic mode, are unlikely
   to) contribute to a proof of the conjecture, so completion does not
   waste effort -- or diverge -- on them.  Configurable via the Method
   suboption "AxiomRelevance" (back-compat alias: "DropDivergentAxioms"):

     None | All | False    -- keep every axiom.
     Automatic | "Safe"     -- DEFAULT.  Drop only axioms that are
       PROVABLY dead weight for this goal: a "confined" axiom carrying
       a private symbol on BOTH sides (see atpConfinedSymbols).  Sound
       AND completeness-preserving.
     "Connected" | {"Connected", "FrequencyCutoff" -> f,
                    "MaxGenerations" -> n}
       -- SInE-style symbol-connectivity pruning: keep only axioms
       reachable from the goal's symbols (ignoring ubiquitous symbols
       that occur in >= f of the axioms; f defaults to 1, i.e. ignore
       only symbols common to ALL axioms).  HEURISTIC: may drop a
       needed axiom, so it is opt-in, not the default.

   The decision is inspectable without proving via TRelevantAxioms. *)

(* function symbols of an axiom/conjecture (raw ForAll form, a list of
   them, or an unquantified equation): non-variable atoms, minus the
   ForAll-bound variables and the structural heads. *)
atpFnSyms[expr_] := Block[{vars, body},
    (* ForAll is HoldAll, so a `ForAll[v_, _] :> v` rule does NOT bind
       v -- extract the bound-variable spec by Part instead. *)
    vars = Flatten @ Map[#[[1]] &, Cases[expr, _ForAll, {0, Infinity}]];
    body = expr /. ForAll[_, e_] :> e;
    (* Heads -> True is essential here: the discriminating function
       symbol is usually an operator (CenterDot, OverTilde, ...)
       appearing as a HEAD, which Cases skips by default. *)
    DeleteDuplicates @ DeleteCases[
        Cases[body, s_Symbol, {0, Infinity}, Heads -> True],
        Alternatives @@ Join[vars,
            {Equal, Inactive, ForAll, Exists, List, And, Or, Not,
             Implies, Pattern, Blank, HoldPattern, Verbatim, Rule}]]
];

(* {lhs, rhs} symbol form of an axiom (raw or unquantified). *)
atpAxSides[axForm_] := Block[{eq},
    eq = axForm /. ForAll[_, e_] :> e;
    If[ Head[eq] === Equal, List @@ eq, {eq, eq}]
];

(* private symbols of `ax` (relative to the conjecture + other
   axioms): function symbols of ax that occur nowhere else. *)
atpConfinedSymbols[ax_, others_, conjRaw_] :=
    Complement[atpFnSyms[ax],
        If[others === {}, {}, Union @@ (atpFnSyms /@ others)],
        atpFnSyms[conjRaw]];

(* Does ax carry a private symbol on BOTH sides?  Then neither rewrite
   direction can fire without that symbol already present, so ax can
   never enter (or be introduced into) a derivation of a goal free of
   it -- dropping ax is sound (proof over a subset is valid) AND
   completeness-preserving.  The Y combinator Y x == x (Y x) is the
   canonical case: Y is private and on both sides.  Returns the
   witnessing symbols, or {}. *)
atpSafeDropSymbols[ax_, others_, conjRaw_] := Block[{priv, sides},
    priv = atpConfinedSymbols[ax, others, conjRaw];
    If[ priv === {}, Return[{}]];
    sides = atpAxSides[ax];
    Intersection[priv, atpFnSyms[sides[[1]]], atpFnSyms[sides[[2]]]]
];

(* normalize a Method spec / option value into a relevance spec:
   None | "Safe" | {"Connected", <|opts|>}. *)
atpRelevanceSpec[Automatic | "Portfolio"] := "Safe";
atpRelevanceSpec["Completion"] := "Safe";
atpRelevanceSpec[{"Completion", subopts___Rule}] := Block[{o, r, dd},
    o = Association[subopts];
    r = Lookup[o, "AxiomRelevance", Automatic];
    dd = Lookup[o, "DropDivergentAxioms", Automatic];  (* back-compat *)
    Which[
        r =!= Automatic, atpNormRelevance[r],
        dd === False, None,
        True, "Safe"]
];
atpRelevanceSpec[_] := "Safe";

atpNormRelevance[None | All | False] := None;
atpNormRelevance[Automatic | True | "Safe"] := "Safe";
atpNormRelevance["Connected"] := {"Connected", <||>};
atpNormRelevance[{"Connected", a_Association}] := {"Connected", a};
atpNormRelevance[{"Connected", o___Rule}] := {"Connected", Association[o]};
atpNormRelevance[other_] := (
    Message[TFindEquationalProof::badrel, other]; "Safe");

(* Partition `axFormList` into kept / dropped (with reasons) under a
   relevance spec.  Returns <|"Kept" -> {...}, "Dropped" -> {<|"Axiom",
   "Symbols", "Reason"|>...}, "Mode" -> ...|>.  Order-preserving:
   auto-precedence keys off axiom/label order, so re-sorting would
   change the LPO orientation. *)
atpRelevancePartition[axFormList_, conjRaw_, specRaw_] :=
    Block[{spec = atpNormRelevance[specRaw], dropAssoc},
        Switch[spec,
            None,
                <|"Kept" -> axFormList, "Dropped" -> {}, "Mode" -> None|>,
            "Safe",
                dropAssoc = DeleteMissing @ Map[
                    Function[ax, Block[{syms},
                        syms = atpSafeDropSymbols[ax,
                            DeleteCases[axFormList, ax], conjRaw];
                        If[ syms === {}, Missing[],
                            <|"Axiom" -> ax, "Symbols" -> syms,
                              "Reason" -> "ConfinedBothSides"|>]]],
                    axFormList];
                <|"Kept" -> Select[axFormList,
                        FreeQ[#["Axiom"] & /@ dropAssoc, #] &],
                  "Dropped" -> dropAssoc, "Mode" -> "Safe"|>,
            {"Connected", _},
                atpConnectedPartition[axFormList, conjRaw, Last[spec]]
        ]
    ];

(* SInE-style connectivity partition. *)
atpConnectedPartition[axFormList_, conjRaw_, opts_Association] := Block[{
    symLists, nAx, freq, cutoff, ignored, relevant, keep, changed, gen,
    maxGen, dropAssoc},
    nAx = Length[axFormList];
    symLists = atpFnSyms /@ axFormList;
    freq = Counts[Flatten[symLists]];
    cutoff = Lookup[opts, "FrequencyCutoff", 1];
    maxGen = Lookup[opts, "MaxGenerations", nAx];
    (* ignore symbols occurring in >= cutoff fraction of the axioms *)
    ignored = Keys @ Select[freq, # >= Ceiling[cutoff * nAx] &];
    relevant = Complement[atpFnSyms[conjRaw], ignored];
    keep = ConstantArray[False, nAx];
    changed = True; gen = 0;
    While[ changed && gen < maxGen,
        changed = False;
        Do[ If[ ! keep[[i]] &&
                IntersectingQ[Complement[symLists[[i]], ignored], relevant],
                keep[[i]] = True;
                relevant = Union[relevant,
                    Complement[symLists[[i]], ignored]];
                changed = True],
            {i, nAx}];
        gen++];
    dropAssoc = Table[
        If[ ! keep[[i]],
            <|"Axiom" -> axFormList[[i]],
              "Symbols" -> Complement[symLists[[i]], ignored],
              "Reason" -> "Disconnected"|>, Nothing],
        {i, nAx}];
    <|"Kept" -> Pick[axFormList, keep],
      "Dropped" -> dropAssoc, "Mode" -> "Connected"|>
];

(* Apply the relevance filter, Message the dropped axioms, return the
   kept list (order-preserving). *)
atpApplyRelevance[axFormList_, conjRaw_, specRaw_] := Block[{part},
    part = atpRelevancePartition[axFormList, conjRaw, specRaw];
    If[ part["Dropped"] =!= {},
        Message[TFindEquationalProof::dropax,
            #["Symbols"] & /@ part["Dropped"], part["Mode"]]];
    part["Kept"]
];

(* Public: inspect the relevance decision without proving. *)
Options[TRelevantAxioms] = {Method -> Automatic};
TRelevantAxioms[thm_String, theory_String, opts:OptionsPattern[]] :=
    Block[{axRaw, cjRaw},
        axRaw = AxiomaticTheory[theory];
        cjRaw = AxiomaticTheory[theory, "NotableTheorems"][thm];
        If[ ! ListQ[axRaw] || MissingQ[cjRaw], Return[$Failed]];
        TRelevantAxioms[cjRaw, axRaw, opts]
    ];
TRelevantAxioms[conjRaw_, axRaw_List, opts:OptionsPattern[]] :=
    atpRelevancePartition[axRaw, conjRaw,
        atpRelevanceSpec[OptionValue[Method]]];

(* Render a held expression in the form WL's ProofObject expects
   for its top-level Axioms list / ConjectureStatement: keep the
   ForAll wrapper if present, but rewrite every nested `Equal[lhs,
   rhs]` to `Inactive[Equal][lhs, rhs]` so trivial tautology axioms
   `a == a` don't collapse to True under ReleaseHold. *)
holdToInactive[axHC_HoldComplete] :=
    ReleaseHold[axHC /. Equal -> Inactive[Equal]]

(* === Introspective return-type machinery =========================

   TFindEquationalProof's optional LAST positional argument selects what
   the call returns instead of (or alongside) the heavy ProofObject.  A
   single String returns that one value bare; a list of Strings returns
   an Association keyed by the requested names; All returns an
   Association of every spec.  The default ("ProofObject") returns the
   bare ProofObject, so existing call shapes are unchanged. *)
$AtpReturnSpecs = {"ProofObject", "Lemmas", "PreprocessedAxioms",
    "RelevantAxioms", "RawTrace", "Statistics", "Status"};

atpReturnSpecQ[All] := True;
atpReturnSpecQ[x_String] := MemberQ[$AtpReturnSpecs, x];
atpReturnSpecQ[x_List] :=
    x =!= {} && AllTrue[x, StringQ[#] && MemberQ[$AtpReturnSpecs, #] &];
atpReturnSpecQ[_] := False;

(* A terminal-status code -> a human "Proved"/"Saturated"/"TimedOut"/
   "Failed" tag.  PROVED(1) -> Proved; QUEUE_EMPTY(4) -> Saturated (a
   finite complete system; the natural completion-mode terminal state);
   TIMEOUT(3) -> TimedOut; everything else -> Failed. *)
atpReturnStatus[code_] := Switch[code,
    1, "Proved", 4, "Saturated", 3, "TimedOut", _, "Failed"];

(* The completion rule set (cRes["MainRules"], a list of decoded
   {lhs, rhs} expression pairs) rendered as inert equations.  Inactive
   blocks evaluation so an oriented `a -> a`-style rule renders as a
   real Equal rather than collapsing to True. *)
atpMainRulesLemmas[cRes_] := Block[{mr = cRes["MainRules"]},
    If[ ListQ[mr],
        Inactive[Equal][#[[1]], #[[2]]] & /@ mr,
        {}]
];

(* A small run-stats Association derived purely from cRes. *)
atpStatisticsAssoc[cRes_] := <|
    "Status" -> atpReturnStatus[cRes["Status"]],
    "Steps" -> If[ ListQ[cRes["MainSteps"]], Length[cRes["MainSteps"]], 0],
    "Rules" -> If[ ListQ[cRes["MainRules"]], Length[cRes["MainRules"]], 0],
    "Trace" -> If[ ListQ[cRes["Trace"]], Length[cRes["Trace"]], 0]
|>;

(* Project a finished run (the prove/completion bundle) onto a return
   spec.  `bundle` carries "enc", "cRes", "ProofObject", and the lazily
   computed "RelevantAxioms" thunk.  A single String returns its value
   bare; a list returns an Association of just those keys; All returns
   every spec. *)
atpReturnValue[bundle_, "ProofObject"] := bundle["ProofObject"];
atpReturnValue[bundle_, "Lemmas"] := atpMainRulesLemmas[bundle["cRes"]];
atpReturnValue[bundle_, "PreprocessedAxioms"] :=
    holdToInactive /@ bundle["enc"]["AxHCsRaw"];
atpReturnValue[bundle_, "RelevantAxioms"] := bundle["RelevantAxioms"];
atpReturnValue[bundle_, "RawTrace"] := bundle["cRes"]["Trace"];
atpReturnValue[bundle_, "Statistics"] := atpStatisticsAssoc[bundle["cRes"]];
atpReturnValue[bundle_, "Status"] :=
    atpReturnStatus[bundle["cRes"]["Status"]];

atpProjectReturn[bundle_, spec_String] := atpReturnValue[bundle, spec];
atpProjectReturn[bundle_, All] :=
    Association[# -> atpReturnValue[bundle, #] & /@ $AtpReturnSpecs];
atpProjectReturn[bundle_, spec_List] :=
    Association[# -> atpReturnValue[bundle, #] & /@ spec];

Options[TFindEquationalProof] = {
    MaxSteps -> 200000, MaxWallSeconds -> 0., Method -> Automatic,
    TimeConstraint -> Infinity};

(* String form: resolve theorem + theory names through
   AxiomaticTheory, then run the expression form.  The conjecture
   is the named NotableTheorem; the axioms are the theory's axiom
   list.  unquantifyFormula / CanonicalizePatterns normalize the
   quantified formulas (ForAll -> Pattern, Exists -> Skolem, then
   canonical variable names). *)
(* Shared core for the theory-resolved forms: given the axiom theory
   name and an already-resolved conjecture (a single equation or a list
   of conjunct equations), drop irrelevant axioms and prove each
   conjunct via the expression form.  A one-element list returns one
   ProofObject; a longer list returns a list of them ($Failed if any
   conjunct fails). *)
(* Normalize a conjecture argument to a flat list of equation formulas.
   A NotableTheorem value is a list of conjuncts; an Association (e.g.
   the whole NotableTheorems table) contributes its Values; nesting is
   flattened down to single ForAll / Equal formulas.  Equational
   provability distributes over the conjunction, so each is proved
   independently. *)
atpConjList[cj_List] := Catenate[atpConjList /@ cj];
atpConjList[cj_] := {cj};

(* Shared core for the theory-resolved forms: resolve the named theory's
   axioms, drop the ones irrelevant to the conjecture, and prove each
   conjunct via the expression form.  One conjunct returns a single
   ProofObject; several return a list ($Failed if any fails). *)
atpProveFromTheory[cjArg_, theory_String,
        opts:OptionsPattern[TFindEquationalProof]] :=
    atpProveFromTheory[cjArg, theory, "ProofObject", opts];
(* The returnSpec threads through to each conjunct's expression-form
   call: a single-conjunct theorem returns that conjunct's projection;
   a multi-conjunct theorem returns a List of projections (only the
   default "ProofObject" multi-conjunct case keeps the all-or-$Failed
   contract). *)
atpProveFromTheory[cjArg_, theory_String, returnSpec_,
        opts:OptionsPattern[TFindEquationalProof]] := Catch[
    Block[{axRaw, axioms, cjList = atpConjList[cjArg]},
        axRaw = AxiomaticTheory[theory];
        If[ ! ListQ[axRaw],
            Throw[Failure["TATPParseError",
                <|"Reason" -> "AxiomaticTheory[\"" <> theory <>
                    "\"] did not resolve to an axiom list"|>],
                "TATPError"]
        ];
        axRaw = atpApplyRelevance[axRaw, cjList,
            atpRelevanceSpec[
                OptionValue[TFindEquationalProof, {opts}, Method]]];
        axioms = CanonicalizePatterns /@ (unquantifyFormula /@ axRaw);
        If[ Length[cjList] === 1,
            TFindEquationalProof[
                CanonicalizePatterns @ unquantifyFormula @ First[cjList],
                axioms, returnSpec, opts],
            Module[{proofs},
                proofs = Table[
                    TFindEquationalProof[
                        CanonicalizePatterns @ unquantifyFormula @ c,
                        axioms, returnSpec, opts],
                    {c, cjList}];
                If[ returnSpec === "ProofObject"
                        && ! AllTrue[proofs, Head[#] === ProofObject &],
                    $Failed, proofs]]
        ]
    ],
    "TATPError"
]

(* Named NotableTheorem of a named theory.  DISAMBIGUATION: a 2-string
   call whose SECOND string is a return spec (e.g.
   TFindEquationalProof["AbelianGroupAxioms", "Lemmas"]) is the named-
   theory COMPLETION form, NOT a (theorem, theory) prove -- the prove
   form would otherwise read the spec as a theorem name.  The /; guard
   on the prove form (theory =!= a return spec) sends it to the
   completion form below. *)
TFindEquationalProof[theory_String, returnSpec_String,
        opts:OptionsPattern[]] /; atpReturnSpecQ[returnSpec] :=
    atpTheoryCompletion[theory, returnSpec, opts];
TFindEquationalProof[thm_String, theory_String,
        opts:OptionsPattern[]] /; ! atpReturnSpecQ[theory] := Catch[
    Block[{cjRaw = AxiomaticTheory[theory, "NotableTheorems"][thm]},
        If[ MissingQ[cjRaw],
            Throw[Failure["TATPParseError",
                <|"Reason" -> "theorem \"" <> thm <>
                    "\" not in AxiomaticTheory[\"" <> theory <>
                    "\", \"NotableTheorems\"]"|>],
                "TATPError"]
        ];
        atpProveFromTheory[cjRaw, theory, opts]
    ],
    "TATPError"
]
(* (theorem, theory, returnSpec): prove the named theorem, projected. *)
TFindEquationalProof[thm_String, theory_String,
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] := Catch[
    Block[{cjRaw = AxiomaticTheory[theory, "NotableTheorems"][thm]},
        If[ MissingQ[cjRaw],
            Throw[Failure["TATPParseError",
                <|"Reason" -> "theorem \"" <> thm <>
                    "\" not in AxiomaticTheory[\"" <> theory <>
                    "\", \"NotableTheorems\"]"|>],
                "TATPError"]
        ];
        atpProveFromTheory[cjRaw, theory, returnSpec, opts]
    ],
    "TATPError"
]

(* A conjecture against a NAMED theory's axioms.  conjecture may be a
   single equation, a list of equations, or an Association whose Values
   are taken (e.g. the NotableTheorems table) -- so both
     TFindEquationalProof[#, "WolframAxioms"] & /@
       AxiomaticTheory["WolframAxioms", "NotableTheorems"]
   and
     TFindEquationalProof[
       AxiomaticTheory["WolframAxioms", "NotableTheorems"], "WolframAxioms"]
   work.  The /; guard keeps a (axioms, returnSpec) COMPLETION call --
   whose 2nd arg is a return-spec String, not a theory name -- from
   matching here. *)
TFindEquationalProof[
        cj : (_List | _ForAll | _Equal | _Inactive),
        theory_String, opts:OptionsPattern[]] /; ! atpReturnSpecQ[theory] :=
    atpProveFromTheory[cj, theory, opts];
TFindEquationalProof[
        cj : (_List | _ForAll | _Equal | _Inactive),
        theory_String, returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] :=
    atpProveFromTheory[cj, theory, returnSpec, opts];
(* An Association (e.g. the whole NotableTheorems table) "just does
   Values": each value is proved on its own, so a theorem that fails to
   prove is $Failed in its slot rather than failing the whole call. *)
TFindEquationalProof[thms_Association, theory_String, opts:OptionsPattern[]] :=
    atpProveFromTheory[#, theory, opts] & /@ Values[thms];
TFindEquationalProof[thms_Association, theory_String,
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] :=
    atpProveFromTheory[#, theory, returnSpec, opts] & /@ Values[thms];(* Expression form: run thvm's C ATP completion engine on the
   conjecture + axioms, decode the equational rewrite chain, and
   wrap it in a verifier-shaped WL ProofObject.  Returns $Failed
   when the goal is not proved (or the proof is not expressible in
   the axiom-citing dataset -- a completion-derived chain).

   atpEncodeProblem validates axiom/conjecture shape and surfaces
   the encoder state (the Variables list + the Term decoder maps). *)
(* The proving entry: optional LAST positional returnSpec.  Without it,
   the bare ProofObject is returned (backward compatible); with it, the
   run is projected onto the requested introspectives. *)
TFindEquationalProof[conjecture_, axioms_List, OptionsPattern[]] :=
    atpProjectReturn[
        atpProveBundle[conjecture, axioms,
            MaxSteps -> OptionValue[MaxSteps],
            MaxWallSeconds -> OptionValue[MaxWallSeconds],
            Method -> OptionValue[Method],
            TimeConstraint -> OptionValue[TimeConstraint]],
        "ProofObject"];
TFindEquationalProof[conjecture_, axioms_List,
        returnSpec_?atpReturnSpecQ, OptionsPattern[]] :=
    atpProjectReturn[
        atpProveBundle[conjecture, axioms,
            MaxSteps -> OptionValue[MaxSteps],
            MaxWallSeconds -> OptionValue[MaxWallSeconds],
            Method -> OptionValue[Method],
            TimeConstraint -> OptionValue[TimeConstraint]],
        returnSpec];

(* Run a goal-directed proof and return a bundle:
     <|"enc", "cRes", "ProofObject", "RelevantAxioms"|>
   "ProofObject" is $Failed when the goal is not proved (or the proof
   is not expressible over the axioms).  "RelevantAxioms" is computed
   eagerly off the same conjecture+axioms+Method as TRelevantAxioms.
   atpEncodeProblem validates axiom/conjecture shape and surfaces the
   encoder state (the Variables list + the Term decoder maps). *)
atpProveBundle[conjecture_, axioms_List, OptionsPattern[TFindEquationalProof]] :=
    Catch[
    (* Raise $RecursionLimit for the whole bundle: a deep Sheffer/Wolfram
       proof (~300+ steps) walks long trace DAGs in buildCEngineChain /
       buildCplDataset and the WL verifier, any of which can trip the
       default 1024 limit and abort the run (and, in a portfolio sweep,
       terminate the enclosing evaluation). *)
    Block[{$RecursionLimit = Max[$RecursionLimit, 16384]},
    Module[{atpSched = atpScheduleFor[OptionValue[Method], axioms, conjecture],
        atpWall = If[ OptionValue[TimeConstraint] =!= Infinity,
            N[OptionValue[TimeConstraint]], OptionValue[MaxWallSeconds]]},
    If[ Length[atpSched] > 1,
    (* Portfolio: try each scheduled config under a per-config wall
       budget; return the first bundle whose ProofObject verifies.  Each
       config is a concrete Method (one-element schedule), so the
       recursive call takes the single-config path below -- no further
       nesting.  When nothing proves, the last bundle is returned so the
       introspectives ("Lemmas"/"RawTrace"/...) still reflect a real run. *)
    Module[{atpSub, atpR = $Failed, atpEnd},
        (* TimeConstraint is a TOTAL budget across the schedule (like the
           built-in FindEquationalProof).  Divide the REMAINING time
           FAIRLY among the REMAINING configs (recomputed each step) so a
           late-but-winning strategy -- typically GoalDirected/MNF, which
           closes goal-shaped theorems plain completion misses -- is never
           starved by an earlier config that fails slowly.  A config that
           returns early rolls its unused time forward to the rest.
           MaxWallSeconds (no TimeConstraint) stays per-config. *)
        atpEnd = If[ OptionValue[TimeConstraint] =!= Infinity,
            AbsoluteTime[] + N[OptionValue[TimeConstraint]], Infinity];
        Module[{n = Length[atpSched]},
        Do[ atpSub = Which[
                atpEnd =!= Infinity, (atpEnd - AbsoluteTime[]) / (n - i + 1),
                OptionValue[MaxWallSeconds] > 0, OptionValue[MaxWallSeconds],
                True, 60.];
            If[ atpSub <= 0., Break[]];
            atpR = atpProveBundle[conjecture, axioms,
                Method -> atpSched[[i]], MaxWallSeconds -> atpSub,
                MaxSteps -> OptionValue[MaxSteps]];
            If[ Head[atpR["ProofObject"]] === ProofObject, Break[]],
            {i, n}]];
        atpR],
    (* Single config. *)
    Block[{
        enc, conjPair, axiomKeys, ruleList, cRes, extSteps,
        chain, dataset, varNames, axEq, conjStmt, po, relAx
    },
        enc = atpEncodeProblem[axioms, conjecture, True];
        conjPair = enc["ConjPair"];
        relAx = TRelevantAxioms[conjecture, axioms, Method -> OptionValue[Method]];
        axiomKeys = Table[{$AxiomSym, k}, {k, Length[enc["AxPairs"]]}];
        ruleList = buildRuleList[enc["AxPairs"], axiomKeys];
        Block[{atpMethodCfg = atpParseMethod[OptionValue[Method]]},
            cRes = cEngineProof[enc, OptionValue[MaxSteps],
                atpWall, Sequence @@ atpMethodCfg]];
        (* status 1 == PROVED.  A non-PROVED run still returns a bundle
           (the ProofObject is $Failed) so the introspectives reflect it. *)
        If[ cRes["Status"] =!= 1,
            Return[<|"enc" -> enc, "cRes" -> cRes,
                "ProofObject" -> $Failed, "RelevantAxioms" -> relAx|>]
        ];
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
                Statement misses the verifier on (the Boolean XOR
                Orderless interaction on DeMorgan, etc).
           Each attempt is built into a ProofObject and run through
           WL's verifier; only a verifying proof is returned. *)
        varNames = cRes["VarSyms"];
        axEq = holdToInactive /@ enc["AxHCsRaw"];
        conjStmt = holdToInactive[enc["ConjHCRaw"]];
        Module[{tryBuild, poA, poB, poFinal},
            (* Raise $RecursionLimit: a long completion proof (the deep
               Sheffer/Wolfram theorems run to ~300+ steps) walks a deep
               trace DAG in buildCplDataset and the WL verifier, which can
               trip the default 1024 recursion limit and abort the whole
               attempt.  Guard it locally. *)
            tryBuild[chainOn_, baseDataset_] := Block[{
                    $RecursionLimit = Max[$RecursionLimit, 16384]},
                Module[{ds, p, v},
                ds = If[ baseDataset =!= $Failed, baseDataset,
                    Block[{$AtpUseChain = chainOn},
                        Check[
                            Quiet[buildCplDataset[enc, conjPair, cRes],
                                {General::newsym, RuleDelayed::rhs}],
                            $Failed]]];
                If[ ds === $Failed, $Failed,
                    p = ProofObject["EquationalLogic", conjStmt, axEq,
                        <|"Variables" -> Union[varNames,
                            Cases[ds, s_Symbol /; atpXVarQ[s], {0, Infinity}]],
                          "Constants" -> {}, "Proof" -> ds|>];
                    v = Quiet @ Check[
                        p["ProofFunction"][p["Theorems"]], $Failed];
                    If[ Head[p] === ProofObject && Head[v] === Success,
                        p, $Failed]
                ]]
            ];
            poA = tryBuild[True, dataset];
            poFinal = If[ Head[poA] === ProofObject,
                poA,
                poB = tryBuild[False, $Failed];
                If[ Head[poB] === ProofObject, poB, $Failed]
            ];
            <|"enc" -> enc, "cRes" -> cRes,
                "ProofObject" -> poFinal, "RelevantAxioms" -> relAx|>
        ]
    ]]]],
    "TATPError"
]

(* === Single-argument completion mode =============================

   TFindEquationalProof[axioms] (no conjecture) runs a time-constrained
   completion of the axiom equations and returns the derived lemmas (the
   completed rule set).  Implementation: encode with a dummy conjecture,
   then overwrite the goal pair in the packed array to (0, 0) -- the C
   runner reads (0, 0) as "no goal" and saturates until the CP queue
   empties (a finite complete system) or the step/wall budget is hit.
   The default return for completion mode is "Lemmas" (there is no goal,
   so no ProofObject). *)
atpCompletionBundle[axioms_List, OptionsPattern[TFindEquationalProof]] :=
    Catch[
    Module[{enc, cRes, atpWall, atpMethodCfg},
        atpWall = If[ OptionValue[TimeConstraint] =!= Infinity,
            N[OptionValue[TimeConstraint]], OptionValue[MaxWallSeconds]];
        (* Encode with a None conjecture: the packed goal pair is (0, 0),
           which the C runner reads as "no goal -> saturate the axioms". *)
        enc = atpEncodeProblem[axioms, None, False];
        atpMethodCfg = atpParseMethod[OptionValue[Method]];
        cRes = cEngineProof[enc, OptionValue[MaxSteps], atpWall,
            Sequence @@ atpMethodCfg];
        (* No goal, so no ProofObject; Mode None means all axioms kept. *)
        <|"enc" -> enc, "cRes" -> cRes, "ProofObject" -> $Failed,
          "RelevantAxioms" -> <|"Mode" -> None,
              "Kept" -> axioms, "Dropped" -> {}|>|>
    ],
    "TATPError"
];

(* Completion of an explicit axiom list. *)
TFindEquationalProof[axioms_List, OptionsPattern[]] :=
    atpProjectReturn[
        atpCompletionBundle[axioms,
            MaxSteps -> OptionValue[MaxSteps],
            MaxWallSeconds -> OptionValue[MaxWallSeconds],
            Method -> OptionValue[Method],
            TimeConstraint -> OptionValue[TimeConstraint]],
        "Lemmas"];
TFindEquationalProof[axioms_List, returnSpec_?atpReturnSpecQ,
        OptionsPattern[]] :=
    atpProjectReturn[
        atpCompletionBundle[axioms,
            MaxSteps -> OptionValue[MaxSteps],
            MaxWallSeconds -> OptionValue[MaxWallSeconds],
            Method -> OptionValue[Method],
            TimeConstraint -> OptionValue[TimeConstraint]],
        returnSpec];

(* Completion of a named theory: resolve its axioms the same way the
   theory-prove forms do (unquantify + canonicalize), then complete. *)
atpTheoryCompletion[theory_String, returnSpec_,
        opts:OptionsPattern[TFindEquationalProof]] := Catch[
    Block[{axRaw, axioms},
        axRaw = AxiomaticTheory[theory];
        If[ ! ListQ[axRaw],
            Throw[Failure["TATPParseError",
                <|"Reason" -> "AxiomaticTheory[\"" <> theory <>
                    "\"] did not resolve to an axiom list"|>],
                "TATPError"]
        ];
        axioms = CanonicalizePatterns /@ (unquantifyFormula /@ axRaw);
        atpProjectReturn[
            atpCompletionBundle[axioms,
                MaxSteps -> OptionValue[MaxSteps],
                MaxWallSeconds -> OptionValue[MaxWallSeconds],
                Method -> OptionValue[Method],
                TimeConstraint -> OptionValue[TimeConstraint]],
            returnSpec]
    ],
    "TATPError"
];
(* Single-argument named-theory completion.  (The 2-string completion
   form -- theory + return spec -- lives with the other String forms
   above, guarded by atpReturnSpecQ to disambiguate from a
   (theorem, theory) prove.) *)
TFindEquationalProof[theory_String, OptionsPattern[]] :=
    atpTheoryCompletion[theory, "Lemmas",
        MaxSteps -> OptionValue[MaxSteps],
        MaxWallSeconds -> OptionValue[MaxWallSeconds],
        Method -> OptionValue[Method],
        TimeConstraint -> OptionValue[TimeConstraint]];

End[];
EndPackage[];
