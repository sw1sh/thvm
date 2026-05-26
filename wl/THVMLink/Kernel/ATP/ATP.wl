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

     TFindProof[conjecture, axioms, opts]
     TFindProof["Theorem", "Theory", opts]
         Run thvm's C ATP completion engine on the conjecture +
         axioms and return a real WL ProofObject -- the same head
         FindEquationalProof returns, supporting the property
         interface (p["ProofDataset"], p["ProofGraph"],
         p["ProofFunction"], p["ProofLength"], ...).  The string
         form resolves theorem + theory names through
         AxiomaticTheory.  Returns $Failed when the conjecture is
         not proved.

         The legacy spelling TFindProof is kept as a
         back-compat alias (deprecated) and forwards every call
         to TFindProof.

   Options
     MaxSteps       (TATP)                  -> 64
     MaxSteps       (TFindProof)            -> 200000
     Witness        (TATP)                  -> {}    list of x_
     AllWitnesses   (TATP)                  -> False
     MaxDepth       (TATP / AllWitnesses)   -> 8
     MaxWitnesses   (TATP / AllWitnesses)   -> 16

   See docs/plans/waldmeister_ic_atp.md for the algorithmic intent. *)

(* THVMLink`ATP` is the single ATP entry context.  All public ATP /
   SMT / TPTPImport symbols live here so user code can do
   `Get["THVMLink`ATP`"]` (or equivalently `<< THVMLink`ATP``) and
   call them by bare name.  THVMLink` is on the context path so bare
   IC primitives (TDef / TRef / TLam / ...) owned by sibling Kernel
   files still resolve transparently. *)
BeginPackage["THVMLink`ATP`", {"THVMLink`"}];

TATP::usage = "TATP[{lhs == rhs, ...}, conjecture] runs the IC-native ATP saturation on the given equational axioms and conjecture, returning an Association with Status, Steps, Rules, QueueSize.  Variables are written as `x_` (Pattern[name, Blank[]]).  TATP[File[path]] parses a Waldmeister .pr file and runs the saturator directly.";

TFindProof::usage = "TFindProof[conjecture, axioms] runs thvm's C ATP completion engine and returns a real WL ProofObject -- the same head FindEquationalProof returns, supporting the full property interface (p[\"ProofDataset\"], p[\"ProofGraph\"], p[\"ProofFunction\"], p[\"ProofLength\"], etc.).  Primary use is equational logic (the engine's unfailing Knuth-Bendix completion + reconstruction path); the name is broadened beyond the original TFindEquationalProof so future first-order / Horn / propositional engines can plug in behind the same surface.  TFindProof[\"Theorem\", \"Theory\"] resolves the theorem and theory names through AxiomaticTheory; a theorem stated as a multi-equation conjunction (an n-element list, e.g. BooleanAxioms `DeMorgan`) returns a List of n ProofObjects, one per conjunct.  TFindProof[conjecture, \"Theory\"] proves a given conjecture (an equation, a list of equations, or an Association whose Values are taken -- e.g. the whole AxiomaticTheory[\"Theory\", \"NotableTheorems\"] table) against the axioms of the named theory.  The C engine saturates the axioms; the resulting equational rewrite chain is decoded into a verifier-shaped ProofObject.  Returns $Failed when the conjecture is not proved.  An optional LAST positional argument selects the return type: a String, a list of Strings, or All, drawn from {\"ProofObject\", \"Lemmas\", \"PreprocessedAxioms\", \"RelevantAxioms\", \"RawTrace\", \"Statistics\", \"Status\"}.  A single String returns that one value bare; a list returns an Association keyed by the requested names; All returns an Association of every spec.  The default (\"ProofObject\") returns the bare ProofObject, so existing calls are unchanged.  \"Lemmas\" gives the completed rule set as Inactive[Equal] equations; \"PreprocessedAxioms\" the normalized axioms fed to the engine; \"RelevantAxioms\" the TRelevantAxioms <|\"Mode\",\"Kept\",\"Dropped\"|> partition; \"RawTrace\" the decoded completion trace; \"Statistics\" a small run-stats Association; \"Status\" a \"Proved\"/\"Saturated\"/\"TimedOut\"/\"Failed\" tag.  SINGLE-ARGUMENT COMPLETION: TFindProof[axioms] (a list of axiom equations) or TFindProof[\"Theory\"] (a named theory) runs a time-constrained completion with NO goal -- it saturates the axioms and returns the derived lemmas (default return \"Lemmas\"; pass a return spec as the 2nd argument, e.g. TFindProof[axioms, \"RawTrace\"]).  Bound completion with TimeConstraint, since a non-terminating axiom set never saturates.  Options: MaxSteps (CP-processing cap, default 200000); TimeConstraint (wall-clock seconds, default Infinity = unbounded -- bounds non-terminating recursive-axiom saturations; TimeConstrained[...] and Abort[] also interrupt the running C engine); Method (Automatic | \"Portfolio\" -- Waldmeister-style strategy schedules that try a list of configs in turn, returning the first that proves+verifies.  \"Portfolio\" is the FIXED 4-entry schedule (Mix2 weight, then LPO+AutoPrecedence, then GT weight, then GoalDirected).  Automatic is PROBLEM-AWARE: it analyzes the axioms + conjecture, detects the algebraic structure (a port of Waldmeister's PhilMarlow/XFiles structure recognition), and FRONT-LOADS a tailored config for that structure (e.g. Group/AbelianGroup -> GT weight + AutoPrecedence; Ring -> KBO + AutoPrecedence; Combinatory -> Add weight + LPO; AC -> GT weight; Sheffer/Nand -> GoalDirected MNF front), then APPENDS the full fixed \"Portfolio\" as a fallback tail -- so Automatic only REORDERS and can never prove less than \"Portfolio\".  Or a single explicit config {\"Completion\" (or \"GoalDirected\"), \"CriticalPairWeight\"->\"Add\"|\"Max\"|\"Ord\"|\"Gt\"|\"Mix\"|\"Mix2\"|\"Unif\"|\"Goal\"(CPinGoal goal-directed), \"Ordering\"->\"KBO\"|\"LPO\", \"AutoPrecedence\"->True|False, \"AxiomRelevance\"->None|\"Safe\"|\"Connected\"|\"SInE\"|{\"SInE\",\"SineTolerance\"->st,\"SineDepth\"->sd,\"SineGenerality\"->sgt} (the latter ports Vampire's SInE -- Hoder-Voronkov D-relation + bounded BFS, defaults 3/2/8), \"MaxWeight\"->n (drop CPs over n symbols; 0=unbounded), \"GoalInterleave\"->n (every n-th selection is goal-directed), \"GroundJoin\"->True (delete ground-joinable CPs -- a sound Martin-Nipkow/Twee redundancy criterion), \"Connectedness\"->True (delete a critical pair whose two sides join through terms strictly below the peak -- the sound Bachmair-Dershowitz connectedness criterion, Twee section 6.2), \"SelectionRatio\"->n (Waldmeister CPdimension fairness: 1 FIFO pick per n selections, default 11), \"AutoMaxWeight\"->b (growing CP-weight bound b + 2*deepest-rule-weight: defers over-weight CPs to a stash and force-drains them when the active queue empties; keeps the CP queue small without losing completeness, default 0 = off), \"RHSInterreduce\"->True (Waldmeister IR_InterreduktionRechts: normalize the RHS of every rule against each new rule, keeping R reduced), \"UnfailingCP\"->True (superpose BOTH faces of an unorientable equation -- unfailing completion's completeness requirement; the default overlaps the stored lhs only), \"CPSetInterreduce\"->True (Waldmeister KPV_KPMengeInterreduzieren: periodically re-normalize the whole CP queue against the rule set, deleting CPs that became joinable and reweighting the rest, so the heap-min selection tracks live, irreducible CPs), \"Precedence\"->{sym1,sym2,...} (an explicit reduction-ordering precedence, symbol names highest-to-lowest, mirroring Waldmeister's `p > q > nand` ORDERING block; resolved against the engine's symbol labels and applied to both LPO and KBO), \"SkolemHighest\"->True (rank the goal's ground/skolemized constants above every operator -- the structural rule Waldmeister's `p > q > nand` precedence encodes; takes effect only when supplied, leaving the default precedence byte-identical otherwise), \"FifoTiebreak\"->True (Waldmeister `-:w1=fifo` secondary CP key: preserve each surviving critical pair's insertion age across the post-orient CP-normalize sweep, so equal-weight ties resolve oldest-first run-wide; off by default, engine byte-identical), \"RecordNorm\"->True|False (per-step normalize-trace recording for the ProofObject builder; default True is the historical path -- WL walks CP -> NORM_STEP* -> ORIENT linearly; False routes search through the fast indexed/flatterm normalize so a long completion saturates at the C-bench rate, and WL then reconstructs the chain through the emitNorm BFS over the CP/ORIENT/SIMPLIFY trace DAG)}.  Method->\"Waldmeister\" is a preset for Waldmeister's faithful DEFAULT strategy on an unrecognized (single-operator Sheffer/Wolfram nand) problem: Mix weight + KBO + AutoPrecedence + SelectionRatio 51 (itl(mi)) + RHSInterreduce + UnfailingCP + CPSetInterreduce.  Method exposes the saturator's CP-selection heuristic, reduction ordering, Waldmeister structure-driven precedence, the axiom-relevance filter (inspect with TRelevantAxioms), critical-pair redundancy, interreduction, and queue fairness.  Under a portfolio, TimeConstraint divides FAIRLY across the schedule (each remaining config gets the remaining-budget/remaining-configs share); default 60s per config when TimeConstraint is Infinity.";

TFindEquationalProof::usage = "TFindEquationalProof is a deprecated alias for TFindProof; every call forwards to TFindProof.  Kept for back-compat with notebooks and downstream code that already use the name.  New code should call TFindProof.";

TRelevantAxioms::usage = "TRelevantAxioms[conjecture, axioms] reports which axioms the relevance filter keeps vs. drops for proving conjecture, without running a proof -- making the filter transparent.  TRelevantAxioms[\"Theorem\", \"Theory\"] resolves names through AxiomaticTheory.  Returns <|\"Mode\"->..., \"Kept\"->{axioms}, \"Dropped\"->{<|\"Axiom\", \"Symbols\", \"Reason\"|>...}|>.  The relevance mode is set by the Method \"AxiomRelevance\" suboption: None (keep all); \"Safe\" (default -- drop only provably dead-weight axioms: a confined symbol occurring on both sides, e.g. the Y combinator when the goal is Y-free; sound and completeness-preserving); \"Connected\" or {\"Connected\", \"FrequencyCutoff\"->f, \"MaxGenerations\"->n} (symbol-reachability pruning -- a coarse heuristic, may drop a needed axiom); \"SInE\" or {\"SInE\", \"SineTolerance\"->st, \"SineDepth\"->sd, \"SineGenerality\"->sgt} (the Hoder-Voronkov SInE premise-selection algorithm as shipped in Vampire -- D-relation + bounded BFS from the conjecture's symbols.  Defaults 3/2/8 mirror Vampire's --sine_tolerance/--sine_depth/--sine_generality_threshold, the winning option block from the parallel Vampire benchmark of thvm's uncrackable theorems).";

(* Forward-declare sibling-file public symbols (SMT.wl owns
   TSatEUF / TSmtDecide / TFindProofSMT; TPTPImport.wl owns
   TPTPImport) so bare references inside this file's Begin[`Private`]
   resolve to the shared THVMLink`ATP`X symbol rather than creating
   a phantom THVMLink`ATP`Private`X.  The alphabetical autoload order
   (ATP -> SMT -> TPTPImport) means those symbols don't exist yet when
   this file is parsed; the bare mention here pre-creates them in the
   public context.  Mirrors the iter-9 idiom in the original ATP.wl. *)
{TSatEUF, TSmtDecide, TFindProofSMT, TPTPImport};

(* (The IC primitives TDef / TRef / TLam / TCollapse / ... are owned by
   the depth-4 sibling Switch.wl, which already loaded before this
   depth-5 file -- bare references resolve via the context path
   THVMLink` pushed by the BeginPackage second arg.) *)

Begin["`Private`"];

(* `load` is the LibraryFunctionLoad helper defined in
   THVMLink`Private` by THVMLink.wl; alias it here so bare `load[...]`
   in $atpRunProofFn / $atpRunExistFn / ... resolves to the same
   helper (THVMLink`ATP`Private is a separate context). *)
load = THVMLink`Private`load;

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
   TFindProof flips this for a fallback retry when chain
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
     Integer, Integer, Integer, Integer, Integer, {Integer, 1}, Integer,
     Integer, Integer, Integer, Integer, Integer, Integer, {Integer, 1}},
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
   the string form of TFindProof to normalize the
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
atpSymName[s_String] := s;
atpSymName[s_Symbol] := SymbolName[Unevaluated[s]];
SetAttributes[atpSymName, HoldFirst];
atpSymName[s_] := ToString[s];

(* The 0-arity (ground constant) symbol names occurring in a held
   conjecture pair -- these are the goal's skolem constants. *)
atpGroundConstNames[enc_] := Block[{names},
    names = Cases[
        Hold @@ {enc["ConjPair"]},
        s_Symbol /; AtomQ[Unevaluated[s]] :>
            SymbolName[Unevaluated[s]],
        {0, Infinity}, Heads -> False];
    DeleteDuplicates[names]
];

(* The result is a plain Int64 List the FFI reads label-indexed
   (element i = precedence rank of label i; element 0 is the unused
   label-0 placeholder).  An empty List leaves the C default in place. *)
atpPrecedenceArray[None, enc_] := {};
atpPrecedenceArray[order_List, enc_] := Block[{
    sym = enc["State"]["sym"], maxLab = enc["MaxLab"], names, ranks, arr
},
    names = atpSymName /@ order;
    (* Highest-to-lowest: first name gets the largest rank.  Unlisted
       symbols stay 0. *)
    ranks = AssociationThread[names -> Range[Length[names], 1, -1]];
    arr = ConstantArray[0, maxLab + 1];
    KeyValueMap[
        Function[{nm, lab},
            If[ KeyExistsQ[ranks, nm] && lab >= 1 && lab <= maxLab,
                arr[[lab + 1]] = ranks[nm]]],
        sym];
    arr
];
atpPrecedenceArray["SkolemHighest", enc_] := Block[{
    sym = enc["State"]["sym"], maxLab = enc["MaxLab"], skNames, arr,
    nNonSk
},
    skNames = atpGroundConstNames[enc];
    arr = ConstantArray[0, maxLab + 1];
    (* Non-skolem symbols get ranks 1..k by label order; skolem
       constants get ranks strictly above all of them. *)
    nNonSk = Count[Keys[sym], nm_ /; ! MemberQ[skNames, nm]];
    Block[{nonSkRank = 0},
        KeyValueMap[
            Function[{nm, lab},
                If[ lab >= 1 && lab <= maxLab,
                    arr[[lab + 1]] = If[ MemberQ[skNames, nm],
                        nNonSk + 1,
                        nonSkRank = nonSkRank + 1; nonSkRank]]],
            sym]];
    arr
];
atpPrecedenceArray[_, enc_] := {};

(* Method "SymbolWeights" -> {sym -> w, ...}: an explicit per-symbol
   KBO weight map.  Returns a label-indexed Int64 list (element i = the
   weight to assign to label i; 0 = leave at default 1, mirroring
   atpPrecedenceArray's "leave at default" sentinel).  An empty list
   short-circuits to the engine default (uniform 1).  Waldmeister
   SymbolGewichte port (CLAS/SymbolGewichte.c::SG_SymbGewichteEintragen,
   -w DEF=2:VAR=5:f=5:g=0). *)
atpSymbolWeightsArray[None, enc_] := {};
atpSymbolWeightsArray[map_Association, enc_] := Block[{
    sym = enc["State"]["sym"], maxLab = enc["MaxLab"], arr,
    nameMap = KeyMap[atpSymName, map]
},
    arr = ConstantArray[0, maxLab + 1];
    KeyValueMap[
        Function[{nm, lab},
            If[ KeyExistsQ[nameMap, nm] && lab >= 1 && lab <= maxLab,
                arr[[lab + 1]] = nameMap[nm]]],
        sym];
    arr
];
atpSymbolWeightsArray[rules_List, enc_] :=
    atpSymbolWeightsArray[Association[rules], enc];
atpSymbolWeightsArray[_, enc_] := {};

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
   via `Sequence @@ atpMethodCfg`, which always supplies the full 21
   values, so no default-bearing Optional patterns are needed.  Dropping
   them keeps the pattern under WL's 13-optional threshold and silences
   the Pattern::patm noise that previously fired on every TFindProof
   call. *)
cEngineProof[enc_, maxSteps_, wallSeconds_,
    cpWeight_, ordering_, autoPrec_, useMnf_,
    maxCpWeight_, goalInterleave_, groundJoin_,
    selRatio_, autoMaxWeight_, rhsInterreduce_, unfailingCP_,
    cpSetInterreduce_, connectedness_, precedenceSpec_,
    fifoTiebreak_, recordNorm_, useLRS_, useSOS_,
    useFwdSub_, useBwdSub_, useBwdDemod_, symbolWeightsSpec_] := Block[{
    raw, status, nRules, nTrace, nSteps, nCps, extNRules, extNSteps,
    mnfNSteps, cur, labelToName, idToName, mainSteps, extSteps,
    mnfSteps, mainRules, rTrace, traceEntries, precArray, symbolWeightsArr
},
    precArray = atpPrecedenceArray[precedenceSpec, enc];
    symbolWeightsArr = atpSymbolWeightsArray[symbolWeightsSpec, enc];
    raw = Normal @ $atpRunProofFn[enc["Packed"], maxSteps, enc["MaxLab"],
        N[wallSeconds], cpWeight, ordering, autoPrec, useMnf, maxCpWeight,
        goalInterleave, groundJoin, selRatio, autoMaxWeight, rhsInterreduce,
        unfailingCP, cpSetInterreduce, connectedness, precArray, fifoTiebreak,
        recordNorm, useLRS, useSOS, useFwdSub, useBwdSub, useBwdDemod,
        symbolWeightsArr];
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

(* True iff `v` is a completion variable: a named rule variable (member
   of varSyms), an "x<id>" symbol decodeAtpTerm minted for a fresh FVR,
   or a "cplU<n>" symbol cplFreshen introduced when re-deriving a CP's
   superposition.  The single variable predicate every cpl-* helper
   shares so canonicalization, unification, and patternization agree on
   what is a variable. *)
cplVarQ[v_, varSyms_] := MatchQ[v, _Symbol] &&
    (MemberQ[varSyms, v] || atpXVarQ[v] ||
        StringMatchQ[SymbolName[v], "cplU" ~~ DigitCharacter ..]);

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
    ca === cplCanonVars[b, varSyms] ||
    ca === cplCanonVars[Reverse[b], varSyms]
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
        If[ Head[sub] === Extract || MissingQ[sub], Return[False]];
        rl = cplAsRule[ruleBEq, varSyms];
        MatchQ[sub, First[rl]] &&
            (res = Quiet @ Replace[sub, rl]; res === sub)
    ]

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

(* Rename every variable of `expr` (members of varSyms / x-FVR vars) to a
   batch of fresh "cplU<n>" symbols, so two rules superposed together do
   not share variable names.  Returns {renamedExpr, freshVars}. *)
cplFreshen[expr_, varSyms_] := Block[{occ, ren},
    occ = cplVarsIn[expr, varSyms];
    ren = AssociationThread[occ, Table[Unique["cplU"], {Length[occ]}]];
    {expr /. ren, Values[ren]}
];

(* Robinson syntactic unification over the head/argument tree.  `vars`
   is the set of unifiable variable symbols; every other symbol is a
   constant.  Returns an Association substitution, or $Failed when the
   two terms do not unify.  Used to reconstruct a CriticalPairLemma's
   superposition the way WL's verifier does: unify the matching rule's
   lhs into the construct rule's lhs at the recorded position. *)
cplUnify[s_, t_, vars_] := cplUnifyLoop[{{s, t}}, <||>, vars];
cplUnifyLoop[$Failed, _, _] := $Failed;
cplUnifyLoop[{}, sub_, _] := sub;
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
];

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
    If[ Head[subt] === Extract || MissingQ[subt], Return[$Failed]];
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
                    Function[c,
                        With[{r = cplReconCp[c[[5]][c[[2]]],
                                c[[6]][c[[4]]], pos, varSyms]},
                            ListQ[r] && cplEqSetQ[r, cpEq, varSyms]]]];
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

(* === TFindProof ========================================= *)

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
    "Goal" -> 7, "CPinGoal" -> 7,
    "Twee" -> 8,   (* Twee KB-completion asymmetric weight (Smallbone),
                     biases toward CPs whose smaller side is small;
                     ported from src/Twee/CP.hs Twee.CP.score. *)
    "Learned" -> 9,
    "ConjSym" -> 10,  (* E ConjectureSymbolWeight (HEURISTICS/
                         che_funweights.c::ConjectureSymbolWeightInit).
                         Walks both sides; CTR nodes whose head symbol
                         appears in the conjecture get weight 1, others
                         get weight 4 (E's fweight=4 vs conj_fweight=1
                         penalty).  Cheap symbol-set biasing toward
                         goal-relevant CPs -- a poor man's "Goal" mode
                         that does not need structural matching. *)
    "Diversity" -> 11, (* E DiversityWeight (HEURISTICS/
                         che_diversityweight.c::DiversityWeightCompute).
                         base + #distinct CTR labels + #distinct FVR
                         ids.  Penalizes CPs whose sides drag in many
                         unrelated symbols / variables -- favors
                         structurally compact CPs.  Linear shape
                         (E's fdiff1=1, fdiff2=0, vdiff1=1, vdiff2=0). *)
    "RelLevel" -> 12,  (* E RelevanceLevelWeight (HEURISTICS/
                         che_funweights.c::RelevanceLevelWeightInit +
                         init_relevance_vector).  N-level scoring:
                         each CTR symbol gets its BFS distance from the
                         conjecture through the "co-occurs-in-an-axiom"
                         relation, capped at ATP_REL_LEVEL_MAX = 8.  A
                         node's weight is 1 + sym_level[label]; remote
                         symbols (unreachable) collapse to level MAX+1.
                         Variable nodes weight 1.  Deeper goal-
                         relevance bias than ConjSym (the 1-level
                         analog). *)
    Automatic -> -1
|>;

TFindProof::badmethod =
    "Unrecognized Method `1`; using Automatic (completion).";
TFindProof::badcpw =
    "Unrecognized \"CriticalPairWeight\" `1`; using engine default.";
TFindProof::dropax =
    "Axiom-relevance filter (mode `2`) dropped axiom(s) keyed on \
symbol set(s) `1` as irrelevant to the conjecture.  Mode \"Safe\" is \
sound and completeness-preserving (the symbols are private to the \
dropped axiom and occur on both sides, so it cannot enter a proof of \
this goal); modes \"Connected\" and \"SInE\" are heuristics and may \
drop a needed axiom.  Inspect with TRelevantAxioms, or set \
\"AxiomRelevance\" -> None in Method to keep every axiom.";
TFindProof::badrel =
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
(* "CPSetInterreduce" -> True: Waldmeister KPV_KPMengeInterreduzieren --
   periodically re-normalize the whole CP queue against the full rule set,
   deleting CPs that became joinable and reweighting the rest, so the
   heap-min selection tracks live, irreducible CPs.  True = on; the engine
   default (and other methods) leave it off. *)
atpCPSetInterreduceOpt[o_Association] := Switch[Lookup[o, "CPSetInterreduce", Automatic],
    True, 1, False | Automatic, 0, _, 0];
(* "Connectedness" -> True: Bachmair-Dershowitz connectedness CP deletion
   (Twee section 6.2) -- drop a critical pair whose two sides join through
   intermediate terms STRICTLY BELOW the peak in the reduction order.  A
   sound generation-cut redundancy criterion (stronger than trivial
   joinability): such a CP is a consequence of smaller overlaps, so it adds
   nothing.  True = on; False/Automatic = off (engine byte-identical). *)
atpConnectednessOpt[o_Association] := Switch[Lookup[o, "Connectedness", Automatic],
    True, 1, False | Automatic, 0, _, 0];
(* "Precedence" -> {sym1, sym2, ...} (highest-to-lowest symbol names) or
   "SkolemHighest" -> True: an explicit reduction-ordering precedence,
   resolved against the engine's symbol labels in cEngineProof
   (atpPrecedenceArray).  Mirrors Waldmeister's `p > q > nand` ORDERING
   block.  None/Automatic/absent = the chosen default (identity or
   AutoPrecedence), keeping the engine byte-identical. *)
atpPrecedenceOpt[o_Association] := Block[{p, sk},
    sk = Lookup[o, "SkolemHighest", Automatic];
    If[ sk === True, Return["SkolemHighest"]];
    p = Lookup[o, "Precedence", Automatic];
    Which[
        ListQ[p], p,
        p === "SkolemHighest", "SkolemHighest",
        True, None]];
(* "SymbolWeights" -> {sym1 -> w1, ...} | <|sym1 -> w1, ...|>: an
   explicit per-symbol KBO weight map.  Waldmeister `SymbolGewichte`
   port (CLAS/SymbolGewichte.c).  Resolved against engine labels in
   cEngineProof (atpSymbolWeightsArray).  Absent / None / Automatic =
   the uniform-1 default (engine byte-identical). *)
atpSymbolWeightsOpt[o_Association] :=
    Replace[Lookup[o, "SymbolWeights", None],
        Automatic -> None];
(* "FifoTiebreak" -> True: Waldmeister `-:w1=fifo` secondary key.  Preserve
   each surviving CP's insertion age across the post-orient CP-normalize
   sweep, so equal-weight ties resolve oldest-first run-wide (the heap
   reheapify otherwise reassigns the age, scrambling the FIFO tie order).
   True = on; False/Automatic = off (engine byte-identical). *)
atpFifoTiebreakOpt[o_Association] := Switch[Lookup[o, "FifoTiebreak", Automatic],
    True, 1, False | Automatic, 0, _, 0];
(* "RecordNorm" -> True/False: per-step normalize-trace recording for the
   ProofObject builder.  Default True (engine byte-identical, the
   historical path: WL walks CP -> NORM_STEP* -> ORIENT linearly).  False
   routes the search through the fast indexed/flatterm normalize so a long
   completion saturates at the C-bench rate; WL then reconstructs the
   chain through the emitNorm BFS over the CP/ORIENT/SIMPLIFY trace DAG. *)
atpRecordNormOpt[o_Association] := Switch[Lookup[o, "RecordNorm", Automatic],
    False, 0, True | Automatic, 1, _, 1];
(* "LRS" -> True: Vampire Limited Resource Strategy (Riazanov & Voronkov,
   JSC 36, 2003).  Under a wall-clock budget, periodically prune the CP
   queue of CPs above the predicted-reachable weight horizon -- the
   saturator concentrates on the budget-tractable subset.  Sound (the
   discarded CPs cannot be reached in budget; same incomplete-in-principle,
   complete-in-budget tradeoff Vampire ships).  True = on; False/Automatic
   = off (default), engine byte-identical. *)
atpLRSOpt[o_Association] := Switch[Lookup[o, "LRS", Automatic],
    True, 1, False | Automatic, 0, _, 0];
(* "SetOfSupport" -> True: bias CP-queue priority toward CPs whose
   terms share symbols with the goal.  Sound -- the heap ordering
   shifts but no CP is dropped, so completeness is preserved.  Mirrors
   Vampire's --sos / E-prover's -S sos in spirit; tailored for the
   equational-completion engine where the "support set" is symbols
   rather than a separate clause set. *)
atpSOSOpt[o_Association] := Switch[Lookup[o, "SetOfSupport", Automatic],
    True, 1, False | Automatic, 0, _, 0];
(* "ForwardSubsume" -> True: when adding a new rule l'=r' to R, drop
   it if some already-stored rule l=r subsumes the new one (\E sigma:
   l*sigma = l' AND r*sigma = r', or the cross-orientation).  Sound +
   completeness-preserving: the new equation is a substitution
   instance of the existing rule, so it adds no deductive power that
   an instance-of-the-existing-rule rewrite step cannot produce.
   Vampire's --forward_subsumption analog, unit-only (every equation
   in UEQ is a unit clause).  Default off (engine byte-identical). *)
atpFwdSubsumeOpt[o_Association] :=
    Switch[Lookup[o, "ForwardSubsume", Automatic],
        True, 1, False | Automatic, 0, _, 0];
(* "BackwardSubsume" -> True: after adding a new rule, soft-delete
   any existing rule it subsumes.  Vampire's bs=unit_only analog.
   Soft-delete uses an out-of-range FVR sentinel (id=255 >=
   REWRITE_MAX_VAR=64) in the slot's lhs/rhs so thvm_match and
   thvm_unify return 0 naturally on the slot; originals are saved
   for proof reconstruction.  Sound + completeness-preserving for
   the same reason as ForwardSubsume (the soft-deleted rule is a
   substitution instance of the new one).  Default off. *)
atpBwdSubsumeOpt[o_Association] :=
    Switch[Lookup[o, "BackwardSubsume", Automatic],
        True, 1, False | Automatic, 0, _, 0];
(* "BackwardDemod" -> True: after each newly-added rule batch, also
   normalize each older rule's LHS against the new rule(s).  If the
   LHS reduces, drop the rule and re-queue (reduced_lhs, old_rhs)
   so orient_and_add can re-admit it under the now-canonical R.
   Vampire's bd=all analog (LHS half; the RHS half is the existing
   "RHSInterreduce" option).  Sound + completeness-preserving (the
   rewritten equation is a logical consequence of the original).
   Default off. *)
atpBwdDemodOpt[o_Association] :=
    Switch[Lookup[o, "BackwardDemod", Automatic],
        True, 1, False | Automatic, 0, _, 0];
atpParseMethod[Automatic] := {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, None, 0, 1, 0, 0, 0, 0, 0, None};
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
            Message[TFindProof::badcpw, cwRaw]; cw = -1];
        ord = Switch[Lookup[o, "Ordering", Automatic],
            "LPO", 1, "KBO" | Automatic, 0, _, 0];
        ap = Switch[Lookup[o, "AutoPrecedence", Automatic],
            True, 1, False | Automatic, 0, _, 0];
        {cw, ord, ap, mnf, atpMaxWeightOpt[o], atpGoalInterleaveOpt[o],
         atpGroundJoinOpt[o], atpSelectionRatioOpt[o], atpAutoMaxWeightOpt[o],
         atpRHSInterreduceOpt[o], atpUnfailingCPOpt[o],
         atpCPSetInterreduceOpt[o], atpConnectednessOpt[o],
         atpPrecedenceOpt[o], atpFifoTiebreakOpt[o], atpRecordNormOpt[o],
         atpLRSOpt[o], atpSOSOpt[o], atpFwdSubsumeOpt[o], atpBwdSubsumeOpt[o],
         atpBwdDemodOpt[o], atpSymbolWeightsOpt[o]}
    ];
atpParseMethod[{"Completion", subopts___Rule}] :=
    atpParseCompletionOpts[{subopts}, 0];

(* "GoalDirected" / "MNF": enable the front search.  Bare form defaults
   to Mix2 weight (like Automatic) so completion still drives R forward
   while MNF watches for a front collision; the list form takes the same
   Ordering / AutoPrecedence / CriticalPairWeight knobs as "Completion"
   so the front search can run over an LPO-oriented, structure-precedence
   rule set -- the combination the hard Sheffer cross-axiom goals need. *)
atpParseMethod[m : ("GoalDirected" | "MNF")] := {5, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, None, 0, 1, 0, 0, 0, 0, 0, None};
atpParseMethod[{("GoalDirected" | "MNF"), subopts___Rule}] :=
    atpParseCompletionOpts[{subopts}, 1];

(* Method -> "Waldmeister": the faithful Waldmeister DEFAULT strategy for
   an unrecognized (single-operator nand / Sheffer / Wolfram) problem --
   the "Orkus" fallback StdS = kbo(std), itl(mi), zb(mnf) (Sinai.h:109,
   :131).  StdS carries no cph(...) clause, so the classification
   defaults to Heu_MixWeight (NewClassification.c:850); the itl(mi)
   token is the interleave RATIO (CPdimension fairness), NOT a weight.
   Decoded into thvm knobs:
     - CriticalPairWeight -> "Mix"  (default heuristic=mixweight, the
       CH_MixWeight formula, ClasHeuristics.c:130)
     - Ordering -> "KBO", AutoPrecedence -> True  (kbo(std) with the
       Praezedenzgenerator auto-precedence)
     - SelectionRatio -> 51  (itl(mi) = interleave fifo:heuristic 1:50,
       YFiles.c:114-122; CPdimension fairness, KPVerwaltung.c:582)
     - RHSInterreduce -> True  (IR_InterreduktionRechts -- the
       divergence that made the deep theorems unreachable)
     - UnfailingCP -> True  (faithful unfailing completion)
   List form takes the same suboptions, overriding any default.  Pass
   "GoalDirected" -> True to add the MNF bidirectional front on top of
   the completion path for a symmetric goal that never meets at one
   normal form. *)
atpParseMethod["Waldmeister"] := atpParseMethod[{"Waldmeister"}];
atpParseMethod[{"Waldmeister", subopts___Rule}] :=
    Block[{o = Association[{subopts}], merged, mnf},
        (* Waldmeister's Orkus default for an unrecognized (single-
           operator nand) problem is StdS = kbo(std), itl(mi), zb(mnf)
           (Sinai.h:109,131): KBO ordering, the interleaved CPdimension
           (itl(mi) -> SelectionRatio 51), the MixWeight classification
           (no cph(...) -> Heu_MixWeight, NewClassification.c:850), and
           goal normalization (zb(mnf)).  With Mix the engine follows
           WM's exact selection trajectory; Add diverges at rule 10.
           RHSInterreduce + UnfailingCP are part of faithful unfailing
           completion.  StdS has no gj(), so GroundJoin is off.

           WM's zb(mnf) is goal normalization, not a separate exhaustive
           bidirectional front; thvm's MNF front search re-expands its
           whole node table every time a rule is added (O(n_nodes) per
           selection), which dominates a deep completion.  The preset
           runs the completion path -- whose single-normal-form goal
           check closes every goal WM's StdS closes -- and only adds
           the MNF front when "GoalDirected" -> True is requested for a
           symmetric goal the single-NF check cannot reach. *)
        mnf = If[ TrueQ @ Lookup[o, "GoalDirected", False], 1, 0];
        o = KeyDrop[o, "GoalDirected"];
        merged = Join[<|
            "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO",
            "AutoPrecedence" -> True, "SelectionRatio" -> 51,
            "RHSInterreduce" -> True, "UnfailingCP" -> True,
            "CPSetInterreduce" -> True|>, o];
        atpParseCompletionOpts[Normal[merged], mnf]
    ];

(* Method -> "VampireUEQ": a preset modeled on the Vampire 5.0.1 UEQ
   portfolio entry that cracks ShefferAxioms/AndAssociativity --
       dis+10_6_to=lpo:tgt=full:fde=none:sp=arity:nwc=1.2:bs=unit_only:
       bd=all:av=off:gtg=exists_sym
   Decoded to thvm knobs (best-effort mapping; Vampire's
   gtg=exists_sym / bd=all are not yet ported, and bs=unit_only is
   approximated by FORWARD subsumption since backward subsumption
   is not yet ported -- FS catches the same rule shape at add time
   instead of after the fact):
     - GoalDirected -> True            (Vampire's `tgt=full`: prefer
       goal-aimed expansion across the queue).
     - Ordering -> "LPO"               (`to=lpo`).
     - AutoPrecedence -> True          (`sp=arity`: our layered
       AutoPrecedence reduces to the arity ladder for single-operator
       Sheffer-shape problems; for multi-operator problems we add
       inverse/distributor structure on top, which Vampire's sp=arity
       does not -- a strict superset for the cases that need it).
     - SelectionRatio -> 10            (`dis+10` = age:weight 1:10
       in Vampire; our SelectionRatio is the inverse FIFO ratio).
     - UnfailingCP -> True             (necessary completeness for
       unorientable equations under LPO).
     - AutoMaxWeight -> True           (Vampire keeps its CP queue
       small via age-weight balance; the closest analog we have is the
       growing-bound weight stash).
     - BackwardSubsume -> True         (direct port of `bs=unit_only`:
       after adding a new rule, soft-delete any existing rule subsumed
       by it.  Iter 18 c7c42f3d shipped the C-side implementation;
       earlier iters used ForwardSubsume as an approximation -- now
       replaced by the real backward variant).
     - BackwardDemod -> True           (direct port of `bd=all` LHS
       half: after a new-rule batch, normalize each older rule's LHS
       with the new rule(s); if it reduces, drop and re-queue the
       simplified equation.  Iter 20 07205c88).
     - RHSInterreduce -> True          (the bd=all RHS half: the
       Waldmeister IR_InterreduktionRechts equivalent.  Pairs with
       BackwardDemod to give the full bd=all both-sides demodulation). *)
atpParseMethod["VampireUEQ"] := atpParseMethod[{"VampireUEQ"}];
atpParseMethod[{"VampireUEQ", subopts___Rule}] :=
    Block[{o = Association[{subopts}], merged, mnf},
        mnf = If[ TrueQ @ Lookup[o, "GoalDirected", True], 1, 0];
        o = KeyDrop[o, "GoalDirected"];
        merged = Join[<|
            "Ordering" -> "LPO", "AutoPrecedence" -> True,
            "SelectionRatio" -> 10, "UnfailingCP" -> True,
            "AutoMaxWeight" -> True,
            "BackwardSubsume" -> True,
            "BackwardDemod" -> True,
            "RHSInterreduce" -> True|>, o];
        atpParseCompletionOpts[Normal[merged], mnf]
    ];

(* Method -> "VampirePortfolio": a 10-entry rotation modeled on the
   portfolio-cycling shape Vampire 5.0.1 ships for UEQ -- many short
   strategy slices rather than one tuned config.  With TimeConstraint
   -> T, each entry runs at T / 10 wall time.  Designed to exercise
   the full knob surface (CP weight modes, orderings, redundancy
   criteria) on a single Method invocation.

   Each entry below picks a different combination of the CP-weight /
   ordering / redundancy levers shipped through iters 10-25, so a goal
   that walls on one slice has a chance to surface on the next.
   Returns a SCHEDULE (list of configs) rather than a single config,
   so the engine's existing portfolio dispatcher fairly divides
   TimeConstraint across the 10 entries. *)
atpParseMethod["VampirePortfolio"] :=
    (* atpScheduleFor pattern-matches a list directly, but
       atpParseMethod's contract is "single config".  Return a sentinel
       that atpScheduleFor recognizes for the rotation. *)
    {-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, None, 0, 1, 0, 0, 0, 0, 0, None};

$VampirePortfolio = {
    (* 1: VampireUEQ-faithful single config (the iter-21 flag-complete
       preset). *)
    "VampireUEQ",
    (* 2: Twee weight + GroundJoin + Connectedness + BS + BD + RHSI. *)
    {"Completion", "CriticalPairWeight" -> "Twee",
        "GroundJoin" -> True, "Connectedness" -> True,
        "BackwardSubsume" -> True, "BackwardDemod" -> True,
        "RHSInterreduce" -> True, "AutoMaxWeight" -> 20},
    (* 3: RelLevel weight + SInE relevance filter for cross-system. *)
    {"Completion", "CriticalPairWeight" -> "RelLevel",
        "AxiomRelevance" -> "SInE", "AutoMaxWeight" -> 20},
    (* 4: ConjSym weight + GoalDirected MNF front. *)
    {"GoalDirected", "CriticalPairWeight" -> "ConjSym",
        "AutoMaxWeight" -> 20},
    (* 5: Diversity weight + UnfailingCP for asymmetric saturation. *)
    {"Completion", "CriticalPairWeight" -> "Diversity",
        "UnfailingCP" -> True, "AutoMaxWeight" -> 20},
    (* 6: Mix2 + LRS + AutoMaxWeight: Vampire age:weight balance. *)
    {"Completion", "CriticalPairWeight" -> "Mix2", "LRS" -> True,
        "AutoMaxWeight" -> 20},
    (* 7: KBO Waldmeister default. *)
    {"Waldmeister"},
    (* 8: LPO + GoalInterleave for combinator-shape goals. *)
    {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "GoalInterleave" -> 50, "AutoMaxWeight" -> 20},
    (* 9: GoalDirected + SInE for cross-system many-axiom goals. *)
    {"GoalDirected", "AxiomRelevance" -> "SInE"},
    (* 10: Add weight, the bare default for combinator / Sheffer-X. *)
    {"Completion", "CriticalPairWeight" -> "Add", "AutoMaxWeight" -> 20}
};

(* Hook VampirePortfolio into atpScheduleFor so the rotation expands
   into the schedule.  Anything else passes through atpParseMethod. *)
atpScheduleFor["VampirePortfolio"] := $VampirePortfolio;
atpScheduleFor["VampirePortfolio", _, _] := $VampirePortfolio;

atpParseMethod[m_] := (
    Message[TFindProof::badmethod, m]; {-1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, None, 0, 1, 0, 0, 0, 0, 0, None});

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
        never pays for it.

   THE ZOO IS REACHED THROUGH STRUCTURE-AWARE FRONT-LOADING by
   atpTunedSchedule (Method -> Automatic), not by extending this
   sequential schedule -- a 4-entry default keeps Method -> Automatic
   fast on the easy 95% of inputs and only the matched structure case
   pays for the specialized preset (Waldmeister / SInE / deep MNF). *)
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
    (* Combinatory logic: a single binary "application" operator
       (Sinai Tafel3 Kombinatorlogik* use the "_" application op,
       Sinai.h:95-102, 348-362).  The combinator constants
       (S,K,B,W,...) appear as atomic SYMBOLS inside axiom subterms
       (e.g. Application[CombinatorS, x] -- CombinatorS is a leaf, not
       an operator HEAD with its own arity slot), so the nullary-op
       check that earlier classifiers used misses them.  Discriminator:
       one binary op + no commutativity / inverse + >=4 axioms (Sheffer
       / Wolfram / Meredith / McCune have at most 3, so this cleanly
       separates).  HasUnit allowed -- some AxiomaticTheory combinator
       presentations set HasUnit on Application (the Y axiom shape). *)
    allHeads = Keys[ops];
    isComb = Length[Select[Values[ops], #["Arity"] == 2 &]] == 1 &&
        ! hasComm && ! hasInv && Length[parts] >= 4;
    (* Sheffer / Nand: a single binary operator, no other structure
       (no associativity, commutativity is the GOAL not an axiom).
       WolframAxioms is the canonical case (one CenterDot axiom). *)
    isSheffer = Length[allHeads] == 1 && First[Values[ops]]["Arity"] == 2 &&
        ! hasAssoc && ! hasComm && ! hasUnit && ! hasInv && ! hasDistrib;
    (* Boolean lattice presentation: two binary ops that are both
       commutative AND distribute, plus a unary "complement" head
       (Huntington-style axioms where associativity is a THEOREM, not
       an axiom, so the assoc detector does NOT fire; that's the gap
       AC-class detection misses).  Symmetric goals dominate here
       (DeMorgan, ExcludedMiddle, Noncontradiction), so GoalDirected
       must come first in the front-load. *)
    isBoolean = Length[Select[Values[ops],
            #["Arity"] == 2 && #["Commutative"] && #["Distributes"] &]] >= 2 &&
        AnyTrue[Values[ops], #["Arity"] == 1 &];
    (* AC theory with a unary complement (Huntington, Robbins): the
       symmetric goals (DoubleNegation, ImpliesRobbins) need MNF/
       GoalDirected, but the bulk completion still wants the AC weight
       schedule -- so this is a "GoalDirected first, then AC" variant. *)
    isACWithComplement = ! isBoolean && hasComm && hasAssoc &&
        AnyTrue[Values[ops], #["Arity"] == 1 &];
    class = Which[
        isComb, "Combinatory",
        isSheffer, "Sheffer",
        isBoolean, "Boolean",
        hasDistrib && hasInv, "Ring",
        hasInv && hasUnit && hasAssoc && hasComm, "AbelianGroup",
        hasInv && hasUnit && hasAssoc, "Group",
        hasIdem && hasComm && hasAssoc && ! hasInv, "Lattice",
        hasUnit && hasAssoc, "Monoid",
        isACWithComplement, "ACWithComplement",
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
       Variable-duplicating combinator rules (S,W,M) need LPO.  Front-load
       GoalDirected too -- the cross-system equivalence direction (e.g.
       SKIToBCKW: S via B,C,W) is symmetric and closes in MNF (~0.03s) but
       walls plain completion (~10s under LPO+AutoPrec). *)
    atpKombS,
    "GoalDirected",
    {"Completion", "CriticalPairWeight" -> "Add"}};
atpAutoTuneForClass["Sheffer"] := {
    (* No Tafel2 Sheffer row.  Measured per-entry behavior:
       - Add weight closes the cross-axiom Implies-X family quickly
         (ImpliesWolframAxioms 0.68s, ImpliesWolframAlternate 0.68s)
         where Gt/Mix2 wall.  Add FIRST.
       - GoalDirected (MNF bidirectional front) is the closer for the
         symmetric goals (nand-Commutativity, etc.) and a known
         engine bug can hang MNF on hard cross-axiom Implies-X if it's
         the first attempt.  Run AFTER Add so any hang is bounded by
         the remaining-budget slice and the shell-level kill-after.
       - SInE relevance pruning helps the cross-system Meredith-class
         Implies-X (Vampire benchmark winner).
       All entries cap CP weight via AutoMaxWeight -> 20 -- no memory
       thrashing.  AndAssociativity-class deep saturations stay out of
       this lean schedule (need Waldmeister preset + minutes of
       saturation; the safety tail's Gt entry still picks them up). *)
    {"Completion", "CriticalPairWeight" -> "Add", "AutoMaxWeight" -> 20},
    {"GoalDirected", "CriticalPairWeight" -> "Mix2", "AutoMaxWeight" -> 20},
    {"Completion", "CriticalPairWeight" -> "Mix2",
        "AxiomRelevance" -> "SInE", "AutoMaxWeight" -> 20},
    {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "AutoMaxWeight" -> 20},
    (* Vampire LRS (Limited Resource Strategy): predicts how many CPs
       the saturator can REACH in the remaining time budget and prunes
       the queue to those.  Sound (discarded CPs are unreachable in
       budget; same incomplete-in-budget tradeoff Vampire ships) and
       cheap (one quickselect + O(queue) prune per recompute period).
       For the deep cross-axiom Sheffer Implies-X family where the
       lean configs above wall, LRS narrows the heap-min selection to
       the budget-tractable subset. *)
    {"Completion", "CriticalPairWeight" -> "Mix2", "LRS" -> True,
        "AutoMaxWeight" -> 20},
    (* Twee-style config: Twee's CP weight (asymmetric, biases small
       reduct) + GroundJoin + Connectedness BOTH on (Twee.Join.Config
       defaults cfg_ground_join=True, cfg_use_connectedness_standalone=
       True).  These redundancy criteria delete CPs whose two sides
       join through a path strictly below the peak; the combination
       prunes the CP queue without losing completeness.  Iters 18 / 20
       also bundle BS / BD / RHSI here (sentinel-LHS soft-delete +
       backward demodulation): every redundancy criterion the engine
       has, on the same entry, so the CP queue stays tightest for the
       deep Implies-X family that walls every cheaper config. *)
    {"Completion", "CriticalPairWeight" -> "Twee",
        "GroundJoin" -> True, "Connectedness" -> True,
        "AutoMaxWeight" -> 20,
        "BackwardSubsume" -> True, "BackwardDemod" -> True,
        "RHSInterreduce" -> True}};
atpAutoTuneForClass["Boolean"] := {
    (* BooleanAxioms has both asymmetric (DeMorgan / Absorption /
       OrAssociativity / Distributivity) and symmetric (ExcludedMiddle /
       Noncontradiction / DoubleNegation) NotableTheorems.  Measured per-
       entry: GoalDirected closes Noncontradiction in 0.48s but Mix2
       walls (the symmetric goals never meet at one normal form); Mix2
       closes DeMorgan in 1.06s and GoalDirected in 3.15s.  GoalDirected
       first wins NET because the symmetric cases save more than DeMorgan
       loses (DeMorgan still proves on the second Mix2 entry). *)
    "GoalDirected",
    {"Completion", "CriticalPairWeight" -> "Mix2"}};
atpAutoTuneForClass["ACWithComplement"] := {
    (* Huntington / Robbins: AC operator + unary complement.  The
       symmetric goals (DoubleNegation, ImpliesRobbins) need MNF
       first; bulk completion (GtS) is the fallback. *)
    "GoalDirected",
    atpGtS};
atpAutoTuneForClass[_] := {};   (* "General": no front-load, just tail *)

(* Front-load the tuned configs, then APPEND the full fixed portfolio
   as a fallback tail and DeleteDuplicates.  This is the safety
   constraint: Automatic only REORDERS -- every config the fixed
   portfolio runs is still present, so the tuned schedule can never
   prove strictly less than "Portfolio".

   Many-axiom tail: when the input is sufficiently large (>=8 axioms),
   ALSO append a SInE-pruned variant of the broadest weight (Mix2) and
   the goal-directed config.  SInE is Vampire's single most successful
   premise-selection technique for the parallel benchmark of thvm's
   uncrackable cross-system Implies* theorems (35/74 wins under the
   strategy lrs+10_1 st=3:sd=2:ss=axioms:sgt=8).  Since SInE may drop
   needed axioms, it runs LAST -- only after the un-pruned portfolio
   has been exhausted -- so a proof reachable without pruning is never
   missed. *)
atpTunedSchedule[axioms_, conjecture_] := Block[{base, sineTail},
    base = DeleteDuplicates @ Join[
        Quiet @ Check[atpAutoTune[axioms, conjecture], {}],
        $AtpSchedule];
    sineTail = If[ Length[axioms] >= 8,
        {{"Completion", "CriticalPairWeight" -> "Mix2",
            "AxiomRelevance" -> "SInE"},
         {"GoalDirected", "AxiomRelevance" -> "SInE"}}, {}];
    DeleteDuplicates @ Join[base, sineTail]];

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
   None | "Safe" | {"Connected", <|opts|>} | {"SInE", <|opts|>}.  The
   "AxiomRelevance" suboption is accepted on every completion-family
   method head, so portfolio entries like {"GoalDirected",
   "AxiomRelevance" -> "SInE"} are honored the same as
   {"Completion", "AxiomRelevance" -> "SInE"}. *)
atpRelevanceSpec[Automatic | "Portfolio"] := "Safe";
atpRelevanceSpec["Completion" | "GoalDirected" | "MNF" | "Waldmeister"] := "Safe";
atpRelevanceSpec[{("Completion" | "GoalDirected" | "MNF" | "Waldmeister"),
        subopts___Rule}] := Block[{o, r, dd},
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
(* SInE: the Hoder-Voronkov premise-selection algorithm (IJCAR 2011) as
   shipped in Vampire (Shell/SineUtils.cpp).  Knob names mirror
   Vampire's option flags --sine_tolerance (st), --sine_depth (sd),
   --sine_generality_threshold (sgt).  Defaults 3 / 2 / 8 reproduce
   the winning portfolio strategy lrs+10_1 st=3:sd=2:ss=axioms:sgt=8
   identified by the Vampire benchmark of thvm's 40 uncrackable
   theorems (Track B). *)
atpNormRelevance["SInE"] := {"SInE", <||>};
atpNormRelevance[{"SInE", a_Association}] := {"SInE", a};
atpNormRelevance[{"SInE", o___Rule}] := {"SInE", Association[o]};
(* Numeric/symbolic shorthand: {"SInE", st, sd, sgt}. *)
atpNormRelevance[{"SInE", st_?NumericQ}] :=
    {"SInE", <|"SineTolerance" -> st|>};
atpNormRelevance[{"SInE", st_?NumericQ, sd_Integer}] :=
    {"SInE", <|"SineTolerance" -> st, "SineDepth" -> sd|>};
atpNormRelevance[{"SInE", st_?NumericQ, sd_Integer, sgt_Integer}] :=
    {"SInE", <|"SineTolerance" -> st, "SineDepth" -> sd,
               "SineGenerality" -> sgt|>};
atpNormRelevance[other_] := (
    Message[TFindProof::badrel, other]; "Safe");

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
                atpConnectedPartition[axFormList, conjRaw, Last[spec]],
            {"SInE", _},
                atpSinePartition[axFormList, conjRaw, Last[spec]]
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

(* Vampire-faithful SInE (Sumo-Inspired premise selection, Hoder &
   Voronkov, IJCAR 2011) port.  Reference impl: vprover/vampire,
   Shell/SineUtils.cpp -- the canonical D-relation + bounded BFS that
   the Vampire option block --sine_selection axioms / --sine_tolerance
   st / --sine_depth sd / --sine_generality_threshold sgt drives.

   Algorithm:
     1. Count occ(s) = number of axioms containing function symbol s.
     2. For each axiom A, minOcc(A) = min_{s in A} occ(s).
     3. D-relation: axiom A is D-related to symbol s iff
          s in A  AND  occ(s) <= st * minOcc(A).
        ("s is among the rarest symbols of A, up to tolerance st.")
     4. BFS from the conjecture's symbols (S_0) to depth sd:
        for each new symbol added at step k-1, pull in every axiom
        D-related to it; those axioms contribute their symbols to S_k.
        Stop at depth sd (Vampire default 2).
     5. Generality threshold sgt: a symbol with occ(s) > sgt is "too
        general" and is NOT used as a trigger (it neither seeds from
        the goal nor propagates through newly-pulled axioms), even if
        it would be the rarest in some axiom.  Drops the very common
        symbols (the operator that appears in every axiom of the
        theory) from D-relation triggering.

   Knobs (Method "AxiomRelevance" -> {"SInE", <|...|>} or "SInE" with
   Vampire defaults):
     "SineTolerance"  st, real, default 3.   (Vampire --sine_tolerance)
     "SineDepth"      sd, int,  default 2.   (Vampire --sine_depth)
     "SineGenerality" sgt, int, default 8.   (Vampire --sine_generality_threshold)

   Tolerance st=1 is strictest (only the strictly-rarest symbol(s) of
   an axiom can trigger it); st=Infinity collapses to plain
   symbol-reachability (any shared symbol triggers).  Depth sd=0 keeps
   no axiom; sd large saturates to the full connected component. *)
atpSinePartition[axFormList_, conjRaw_, opts_Association] := Block[{
    symLists, nAx, occ, minOccA, st, sd, sgt, conjSyms, frontier,
    visited, axTaken, keep, gen, newFrontier, dropAssoc, generalQ},
    nAx = Length[axFormList];
    If[ nAx === 0,
        Return[<|"Kept" -> {}, "Dropped" -> {}, "Mode" -> "SInE"|>]];
    symLists = atpFnSyms /@ axFormList;
    occ = Counts[Flatten[symLists]];
    st  = N @ Lookup[opts, "SineTolerance",  3.];
    sd  =     Lookup[opts, "SineDepth",      2 ];
    sgt =     Lookup[opts, "SineGenerality", 8 ];
    (* Per-axiom minOcc.  An axiom with no function symbols (e.g.
       a == a -- vacuously true) has no minOcc; we never D-relate
       any symbol to it, so it stays dropped unless seeded directly,
       matching Vampire's behavior of skipping empty-signature units. *)
    minOccA = Table[
        If[ symLists[[i]] === {}, Infinity,
            Min[Lookup[occ, #, Infinity] & /@ symLists[[i]]]],
        {i, nAx}];
    (* "Too-general" predicate: a symbol with occ(s) > sgt is not used
       as a trigger.  sgt <= 0 disables the cutoff (mirrors Vampire's
       --sine_generality_threshold 0 = off). *)
    generalQ = If[ IntegerQ[sgt] && sgt > 0,
        Function[s, Lookup[occ, s, 0] > sgt],
        Function[s, False]];
    (* Seed: conjecture's symbols, minus the too-general ones. *)
    conjSyms = atpFnSyms[conjRaw];
    frontier = Select[conjSyms, ! generalQ[#] &];
    visited = frontier;
    axTaken = ConstantArray[False, nAx];
    Do[ newFrontier = {};
        Do[ If[ ! axTaken[[i]] && symLists[[i]] =!= {} &&
                AnyTrue[frontier, Function[s,
                    MemberQ[symLists[[i]], s] &&
                    Lookup[occ, s, 0] <= st * minOccA[[i]]]],
                axTaken[[i]] = True;
                newFrontier = Union[newFrontier,
                    Select[symLists[[i]],
                        ! MemberQ[visited, #] && ! generalQ[#] &]]],
            {i, nAx}];
        If[ newFrontier === {}, Break[]];
        visited = Union[visited, newFrontier];
        frontier = newFrontier,
        {gen, sd}];
    keep = axTaken;
    dropAssoc = Table[
        If[ ! keep[[i]],
            <|"Axiom" -> axFormList[[i]],
              "Symbols" -> symLists[[i]],
              "Reason" -> "SInEUnreachable"|>, Nothing],
        {i, nAx}];
    <|"Kept" -> Pick[axFormList, keep],
      "Dropped" -> dropAssoc, "Mode" -> "SInE"|>
];

(* Apply the relevance filter, Message the dropped axioms, return the
   kept list (order-preserving). *)
atpApplyRelevance[axFormList_, conjRaw_, specRaw_] := Block[{part},
    part = atpRelevancePartition[axFormList, conjRaw, specRaw];
    If[ part["Dropped"] =!= {},
        Message[TFindProof::dropax,
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

   TFindProof's optional LAST positional argument selects what
   the call returns instead of (or alongside) the heavy ProofObject.  A
   single String returns that one value bare; a list of Strings returns
   an Association keyed by the requested names; All returns an
   Association of every spec.  The default ("ProofObject") returns the
   bare ProofObject, so existing call shapes are unchanged. *)
$AtpReturnSpecs = {"ProofObject", "Lemmas", "PreprocessedAxioms",
    "RelevantAxioms", "RawTrace", "Statistics", "Status",
    "AppliedMethod", "WallTime", "PortfolioTrace"};

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
    "Trace" -> If[ ListQ[cRes["Trace"]], Length[cRes["Trace"]], 0],
    "QueueSize" -> Replace[cRes["NCps"], Except[_Integer] -> 0]
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
(* "AppliedMethod" -> the Method config that produced this bundle.
   For a portfolio run, this is the winning schedule entry; for a
   single-config or completion run, the only entry tried.  Useful for
   debugging "what did Automatic actually try?". *)
atpReturnValue[bundle_, "AppliedMethod"] :=
    Replace[Lookup[bundle, "AppliedMethod", Missing["NotAvailable"]],
        Missing[___] :> Automatic];
(* "WallTime" -> seconds (AbsoluteTiming) the C-engine cEngineProof
   call took for the SINGLE config that produced this bundle.  For a
   portfolio run this is the WINNING config's slice only -- earlier
   non-proving slices are not summed in. *)
atpReturnValue[bundle_, "WallTime"] :=
    Replace[Lookup[bundle, "WallTime", Missing["NotAvailable"]],
        Missing[___] :> Missing["NotAvailable"]];
(* "PortfolioTrace" -> the full list of {Method, WallTime, Proved}
   records for every schedule entry the portfolio dispatcher tried,
   in order.  The last entry is the WINNING slice (Proved -> True);
   any earlier entries are non-proving slices.  For a single-config
   call, returns a single-element list with that one config. *)
atpReturnValue[bundle_, "PortfolioTrace"] :=
    Replace[Lookup[bundle, "PortfolioTrace", Missing["NotAvailable"]],
        Missing[___] :> {<|
            "Method" -> atpReturnValue[bundle, "AppliedMethod"],
            "WallTime" -> atpReturnValue[bundle, "WallTime"],
            "Proved" -> (Head[bundle["ProofObject"]] === ProofObject)|>}];

atpProjectReturn[bundle_, spec_String] := atpReturnValue[bundle, spec];
atpProjectReturn[bundle_, All] :=
    Association[# -> atpReturnValue[bundle, #] & /@ $AtpReturnSpecs];
atpProjectReturn[bundle_, spec_List] :=
    Association[# -> atpReturnValue[bundle, #] & /@ spec];

Options[TFindProof] = {
    MaxSteps -> 200000, Method -> Automatic,
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
        opts:OptionsPattern[TFindProof]] :=
    atpProveFromTheory[cjArg, theory, "ProofObject", opts];
(* The returnSpec threads through to each conjunct's expression-form
   call: a single-conjunct theorem returns that conjunct's projection;
   a multi-conjunct theorem returns a List of projections (only the
   default "ProofObject" multi-conjunct case keeps the all-or-$Failed
   contract). *)
atpProveFromTheory[cjArg_, theory_String, returnSpec_,
        opts:OptionsPattern[TFindProof]] := Catch[
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
                OptionValue[TFindProof, {opts}, Method]]];
        axioms = CanonicalizePatterns /@ (unquantifyFormula /@ axRaw);
        If[ Length[cjList] === 1,
            TFindProof[
                CanonicalizePatterns @ unquantifyFormula @ First[cjList],
                axioms, returnSpec, opts],
            Module[{proofs},
                proofs = Table[
                    TFindProof[
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
   TFindProof["AbelianGroupAxioms", "Lemmas"]) is the named-
   theory COMPLETION form, NOT a (theorem, theory) prove -- the prove
   form would otherwise read the spec as a theorem name.  The /; guard
   on the prove form (theory =!= a return spec) sends it to the
   completion form below. *)
TFindProof[theory_String, returnSpec_String,
        opts:OptionsPattern[]] /; atpReturnSpecQ[returnSpec] :=
    atpTheoryCompletion[theory, returnSpec, opts];
TFindProof[thm_String, theory_String,
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
TFindProof[thm_String, theory_String,
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
     TFindProof[#, "WolframAxioms"] & /@
       AxiomaticTheory["WolframAxioms", "NotableTheorems"]
   and
     TFindProof[
       AxiomaticTheory["WolframAxioms", "NotableTheorems"], "WolframAxioms"]
   work.  The /; guard keeps a (axioms, returnSpec) COMPLETION call --
   whose 2nd arg is a return-spec String, not a theory name -- from
   matching here. *)
TFindProof[
        cj : (_List | _ForAll | _Equal | _Inactive),
        theory_String, opts:OptionsPattern[]] /; ! atpReturnSpecQ[theory] :=
    atpProveFromTheory[cj, theory, opts];
TFindProof[
        cj : (_List | _ForAll | _Equal | _Inactive),
        theory_String, returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] :=
    atpProveFromTheory[cj, theory, returnSpec, opts];
(* An Association (e.g. the whole NotableTheorems table) "just does
   Values": each value is proved on its own, so a theorem that fails to
   prove is $Failed in its slot rather than failing the whole call. *)
TFindProof[thms_Association, theory_String, opts:OptionsPattern[]] :=
    atpProveFromTheory[#, theory, opts] & /@ Values[thms];
TFindProof[thms_Association, theory_String,
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] :=
    atpProveFromTheory[#, theory, returnSpec, opts] & /@ Values[thms];(* Expression form: run thvm's C ATP completion engine on the
   conjecture + axioms, decode the equational rewrite chain, and
   wrap it in a verifier-shaped WL ProofObject.  Returns $Failed
   when the goal is not proved (or the proof is not expressible in
   the axiom-citing dataset -- a completion-derived chain).

   atpEncodeProblem validates axiom/conjecture shape and surfaces
   the encoder state (the Variables list + the Term decoder maps). *)
(* TPTP source -> conjecture + axioms, then prove.  Accepts:
     - File["foo.p"]: read + parse, then prove the Conjecture against
       the Axioms.
     - a string containing "cnf(": parse inline, then prove.
   If the file has no conjecture (axioms only), dispatch to the
   single-arg completion form -- saturate the axioms and return the
   completed rule set as the default "Lemmas" projection.
   The parser handles the TPTP UEQ fragment (one equational literal
   per cnf clause).  fof / tff / thf clauses + include directives are
   skipped with a console warning.  See Kernel/ATP/TPTPImport.wl. *)
TFindProof[File[path_String], opts:OptionsPattern[]] :=
    tptpDispatch[TPTPImport[File[path]], opts]
TFindProof[s_String, opts:OptionsPattern[]] /;
        StringContainsQ[s, "cnf("] || StringContainsQ[s, "fof("] :=
    tptpDispatch[TPTPImport[s], opts]

(* Convert TPTP's String-headed terms ("and"[X, Y], "a"[]) into
   Symbol-headed terms in a private context so atpEncodeProblem and
   the WL ProofObject verifier (which expect Symbol heads) work as
   usual.  The conversion is one-way at dispatch time: TPTPImport's
   user-visible output stays String-headed for clean InputForm display
   ("and"[X, Y] instead of THVMLink`...`Tptp`and[X, Y]). *)
(* CamelCase-fold underscored names so Symbol[] accepts them.
   sk_c1 -> skC1, op_overtilde -> opOvertilde, $true -> Dollar$true.
   Symbol[] rejects identifier strings with `_` (parsed as Blank) or
   leading `$` (parsed as $-prefix); this fold side-steps both. *)
tptpStringToSymbol[s_String] :=
    Symbol["THVMLink`ATP`Private`Tptp$" <> Which[
        StringStartsQ[s, "$"], "Dollar" <> StringDrop[s, 1],
        StringContainsQ[s, "_"], With[{parts = StringSplit[s, "_"]},
            First[parts] <> StringJoin[Capitalize /@ Rest[parts]]],
        True, s
    ]];
(* Internalize: convert "h"[args...] -> Tptp$h[args...].  Nullary
   "a"[] (with empty args) collapses to bare Symbol Tptp$a so it
   matches the WL ProofObject decoder's `Symbol[name]` shape for
   0-arity constants (line ~780 -- the decoder returns
   `Symbol[name]` rather than `Symbol[name][]` for arity 0, so the
   verifier sees consistent shapes on round-trip). *)
tptpInternalize[expr_] :=
    expr //. {
        h_String[] :> tptpStringToSymbol[h],
        h_String[args__] :> tptpStringToSymbol[h][args]
    };

tptpDispatch[imported_Association, opts:OptionsPattern[TFindProof]] := If[
    imported["Conjecture"] === None,
    TFindProof[tptpInternalize /@ imported["Axioms"], opts],
    TFindProof[tptpInternalize @ imported["Conjecture"],
        tptpInternalize /@ imported["Axioms"], opts]
]

(* The proving entry: optional LAST positional returnSpec.  Without it,
   the bare ProofObject is returned (backward compatible); with it, the
   run is projected onto the requested introspectives. *)
(* Method -> "SMT" short-circuit: route ground equational inputs to the
   QF_UF congruence-closure decider in Kernel/ATP/SMT.wl.  Returns the
   SMT ProofObject-shaped Association directly (Method,Witness,...).
   The guard form -- OptionValue[TFindProof, {opts}, Method] -- is the
   reliable WL idiom for option-keyed dispatch: a bare OptionValue
   inside `/;` does not always see the supplied opts. *)
TFindProof[conjecture_, axioms_List, opts:OptionsPattern[]] /;
        ("SMT" === OptionValue[TFindProof, {opts}, Method]) :=
    TFindProofSMT[conjecture, axioms];
TFindProof[conjecture_, axioms_List,
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] /;
        ("SMT" === OptionValue[TFindProof, {opts}, Method]) :=
    TFindProofSMT[conjecture, axioms];

TFindProof[conjecture_, axioms_List, OptionsPattern[]] :=
    atpProjectReturn[
        atpProveBundle[conjecture, axioms,
            MaxSteps -> OptionValue[MaxSteps],
            Method -> OptionValue[Method],
            TimeConstraint -> OptionValue[TimeConstraint]],
        "ProofObject"];
TFindProof[conjecture_, axioms_List,
        returnSpec_?atpReturnSpecQ, OptionsPattern[]] :=
    atpProjectReturn[
        atpProveBundle[conjecture, axioms,
            MaxSteps -> OptionValue[MaxSteps],
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
atpProveBundle[conjecture_, axioms_List, OptionsPattern[TFindProof]] :=
    Catch[
    (* Raise $RecursionLimit for the whole bundle: a deep Sheffer/Wolfram
       proof (~300+ steps) walks long trace DAGs in buildCEngineChain /
       buildCplDataset and the WL verifier, any of which can trip the
       default 1024 limit and abort the run (and, in a portfolio sweep,
       terminate the enclosing evaluation). *)
    Block[{$RecursionLimit = Max[$RecursionLimit, 16384]},
    Module[{atpSched = atpScheduleFor[OptionValue[Method], axioms, conjecture],
        atpWall = If[ OptionValue[TimeConstraint] =!= Infinity,
            N[OptionValue[TimeConstraint]], 0.]},
    If[ Length[atpSched] > 1,
    (* Portfolio: try each scheduled config under a per-config wall
       budget; return the first bundle whose ProofObject verifies.  Each
       config is a concrete Method (one-element schedule), so the
       recursive call takes the single-config path below -- no further
       nesting.  When nothing proves, the last bundle is returned so the
       introspectives ("Lemmas"/"RawTrace"/...) still reflect a real run. *)
    Module[{atpSub, atpR = $Failed, atpEnd, atpTrace = {}},
        (* TimeConstraint is a TOTAL budget across the schedule (like the
           built-in FindEquationalProof).  Divide the REMAINING time
           FAIRLY among the REMAINING configs (recomputed each step) so a
           late-but-winning strategy -- typically GoalDirected/MNF, which
           closes goal-shaped theorems plain completion misses -- is never
           starved by an earlier config that fails slowly.  A config that
           returns early rolls its unused time forward to the rest.
           Unset TimeConstraint stays at the per-config default 60s. *)
        atpEnd = If[ OptionValue[TimeConstraint] =!= Infinity,
            AbsoluteTime[] + N[OptionValue[TimeConstraint]], Infinity];
        Module[{n = Length[atpSched]},
        Do[ atpSub = If[ atpEnd =!= Infinity,
                (atpEnd - AbsoluteTime[]) / (n - i + 1),
                60.];
            If[ atpSub <= 0., Break[]];
            atpR = atpProveBundle[conjecture, axioms,
                Method -> atpSched[[i]], TimeConstraint -> atpSub,
                MaxSteps -> OptionValue[MaxSteps]];
            (* Record this slice's outcome for the "PortfolioTrace"
               return spec: which Method, how long the C-engine call
               took, whether it produced a verifying ProofObject. *)
            AppendTo[atpTrace, <|
                "Method" -> atpSched[[i]],
                "WallTime" -> Lookup[atpR, "WallTime", Missing["NotAvailable"]],
                "Proved" -> (Head[atpR["ProofObject"]] === ProofObject)
            |>];
            If[ Head[atpR["ProofObject"]] === ProofObject, Break[]],
            {i, n}]];
        (* Stamp the cumulative trace on the returned bundle.  When only
           one slice ran (proved immediately), the trace has one entry. *)
        If[ AssociationQ[atpR],
            atpR = Append[atpR, "PortfolioTrace" -> atpTrace]];
        atpR],
    (* Single config. *)
    Block[{
        enc, conjPair, axiomKeys, ruleList, cRes, extSteps,
        chain, dataset, varNames, axEq, conjStmt, po, relAx, atpWallTime
    },
        enc = atpEncodeProblem[axioms, conjecture, True];
        conjPair = enc["ConjPair"];
        relAx = TRelevantAxioms[conjecture, axioms, Method -> OptionValue[Method]];
        axiomKeys = Table[{$AxiomSym, k}, {k, Length[enc["AxPairs"]]}];
        ruleList = buildRuleList[enc["AxPairs"], axiomKeys];
        Block[{atpMethodCfg = atpParseMethod[OptionValue[Method]]},
            {atpWallTime, cRes} = AbsoluteTiming @ cEngineProof[
                enc, OptionValue[MaxSteps],
                atpWall, Sequence @@ atpMethodCfg]];
        (* status 1 == PROVED.  A non-PROVED run still returns a bundle
           (the ProofObject is $Failed) so the introspectives reflect it. *)
        If[ cRes["Status"] =!= 1,
            Return[<|"enc" -> enc, "cRes" -> cRes,
                "ProofObject" -> $Failed, "RelevantAxioms" -> relAx,
                "AppliedMethod" -> OptionValue[Method],
                "WallTime" -> atpWallTime|>]
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
            (* When the C engine ran with per-step recording OFF
               (cRes["RecordNorm"] === 0 -- Method "RecordNorm" -> False,
               the fast-search path), no TRACE_NORM_STEP entries exist, so
               the chain-ON extraction has nothing to walk: go straight to
               the chain-OFF emitNorm BFS, which bridges the CP/ORIENT/
               SIMPLIFY trace DAG.  A pre-built axiom-cited EXT dataset
               (when present) still wins regardless. *)
            poA = If[ Lookup[cRes, "RecordNorm", 1] === 0 && dataset === $Failed,
                tryBuild[False, $Failed],
                tryBuild[True, dataset]];
            poFinal = If[ Head[poA] === ProofObject,
                poA,
                poB = tryBuild[False, $Failed];
                If[ Head[poB] === ProofObject, poB, $Failed]
            ];
            <|"enc" -> enc, "cRes" -> cRes,
                "ProofObject" -> poFinal, "RelevantAxioms" -> relAx,
                "AppliedMethod" -> OptionValue[Method],
                "WallTime" -> atpWallTime|>
        ]
    ]]]],
    "TATPError"
]

(* === Single-argument completion mode =============================

   TFindProof[axioms] (no conjecture) runs a time-constrained
   completion of the axiom equations and returns the derived lemmas (the
   completed rule set).  Implementation: encode with a dummy conjecture,
   then overwrite the goal pair in the packed array to (0, 0) -- the C
   runner reads (0, 0) as "no goal" and saturates until the CP queue
   empties (a finite complete system) or the step/wall budget is hit.
   The default return for completion mode is "Lemmas" (there is no goal,
   so no ProofObject). *)
atpCompletionBundle[axioms_List, OptionsPattern[TFindProof]] :=
    Catch[
    Module[{enc, cRes, atpWall, atpMethodCfg, atpWallTime},
        atpWall = If[ OptionValue[TimeConstraint] =!= Infinity,
            N[OptionValue[TimeConstraint]], 0.];
        (* Encode with a None conjecture: the packed goal pair is (0, 0),
           which the C runner reads as "no goal -> saturate the axioms". *)
        enc = atpEncodeProblem[axioms, None, False];
        atpMethodCfg = atpParseMethod[OptionValue[Method]];
        {atpWallTime, cRes} = AbsoluteTiming @ cEngineProof[
            enc, OptionValue[MaxSteps], atpWall,
            Sequence @@ atpMethodCfg];
        (* No goal, so no ProofObject; Mode None means all axioms kept. *)
        <|"enc" -> enc, "cRes" -> cRes, "ProofObject" -> $Failed,
          "RelevantAxioms" -> <|"Mode" -> None,
              "Kept" -> axioms, "Dropped" -> {}|>,
          "AppliedMethod" -> OptionValue[Method],
          "WallTime" -> atpWallTime|>
    ],
    "TATPError"
];

(* Completion of an explicit axiom list. *)
TFindProof[axioms_List, OptionsPattern[]] :=
    atpProjectReturn[
        atpCompletionBundle[axioms,
            MaxSteps -> OptionValue[MaxSteps],
            Method -> OptionValue[Method],
            TimeConstraint -> OptionValue[TimeConstraint]],
        "Lemmas"];
TFindProof[axioms_List, returnSpec_?atpReturnSpecQ,
        OptionsPattern[]] :=
    atpProjectReturn[
        atpCompletionBundle[axioms,
            MaxSteps -> OptionValue[MaxSteps],
            Method -> OptionValue[Method],
            TimeConstraint -> OptionValue[TimeConstraint]],
        returnSpec];

(* Completion of a named theory: resolve its axioms the same way the
   theory-prove forms do (unquantify + canonicalize), then complete. *)
atpTheoryCompletion[theory_String, returnSpec_,
        opts:OptionsPattern[TFindProof]] := Catch[
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
TFindProof[theory_String, OptionsPattern[]] :=
    atpTheoryCompletion[theory, "Lemmas",
        MaxSteps -> OptionValue[MaxSteps],
        Method -> OptionValue[Method],
        TimeConstraint -> OptionValue[TimeConstraint]];

(* === Back-compat alias =============================================
   TFindEquationalProof is the legacy name; every call forwards to
   TFindProof.  Mirror the Options + Messages so OptionsPattern[
   TFindEquationalProof] and Quiet[..., TFindEquationalProof::badmethod]
   in user code keep working byte-identically. *)
Options[TFindEquationalProof] = Options[TFindProof];
TFindEquationalProof::badmethod = TFindProof::badmethod;
TFindEquationalProof::badcpw    = TFindProof::badcpw;
TFindEquationalProof::dropax    = TFindProof::dropax;
TFindEquationalProof::badrel    = TFindProof::badrel;
TFindEquationalProof[args___] := TFindProof[args];

End[];
EndPackage[];

(* Sub-modules live under Kernel/ATP/ and are picked up by the
   recursive autoloader in Kernel/THVMLink.wl -- no explicit Get
   here.  Files load alphabetically by full path, so Kernel/ATP/
   children load AFTER Kernel/ATP.wl. *)
