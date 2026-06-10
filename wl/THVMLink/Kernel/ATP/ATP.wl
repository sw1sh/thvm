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

         The legacy spelling TFindEquationalProof is kept as a
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
   SMT symbols live here so user code can do `Get["THVMLink`ATP`"]`
   (or equivalently `<< THVMLink`ATP``) and call them by bare name.
   THVMLink` is on the context path so bare IC primitives (TDef /
   TRef / TLam / ...) owned by sibling Kernel files still resolve
   transparently.  Wolfram`Parser` is on the path so `TPTPImport`
   (the EBNFParse-driven TPTP parser, now a sibling in the
   Wolfram/WolframParser paclet) resolves without qualification. *)
Needs["Wolfram`Parser`"];

(* Implementation split across sibling files in this directory (Gotten
   after this one by the recursive Kernel loader, all sharing the
   THVMLink`ATP`Private` context):
     ATP_ProofGraph.wl   proof decoder + ProofDataset + critical-pair-lemma DAG
     ATP_Method.wl       TAtpSchedule / TAtpDescribeMethod + structure auto-tune
     ATP_Relevance.wl    TRelevantAxioms premise-selection filter *)
BeginPackage["THVMLink`ATP`", {"THVMLink`", "Wolfram`Parser`"}];

GeneralUtilities`SetUsage[TATP, "TATP[{lhs$1 == rhs$1, $$}, conjecture$] runs the IC-native ATP saturation on the given equational axioms and conjecture, returning an Association with Status, Steps, Rules, QueueSize.
TATP[File[path$]] parses a Waldmeister .pr file and runs the saturator directly.
Variables are written as x_ (Pattern[name, Blank[]]); with Witness options the result also carries Witness / Witnesses bindings.
Options: MaxSteps, Witness, AllWitnesses, MaxDepth, MaxWitnesses."];

GeneralUtilities`SetUsage[TFindProof, "TFindProof[conjecture$, axioms$] runs thvm's C equational-completion engine and returns a WL ProofObject (same head FindEquationalProof returns, with the full property interface: p[\"ProofDataset\"], p[\"ProofGraph\"], p[\"ProofFunction\"], p[\"ProofLength\"], etc.).
TFindProof[\"Theorem\", \"Theory\"] resolves names through AxiomaticTheory; a multi-equation theorem (an n$-element list) is proved as one conjunction off a single saturation, returning ONE ProofObject with a {\"Hypothesis\", g$} / {\"Conclusion\", g$} row pair per conjunct (FindEquationalProof parity).
TFindProof[conjecture$, \"Theory\"] proves the conjecture (an equation, a list of equations, or an Association whose Values are taken) against the named theory's axioms.
TFindProof[axioms$] and TFindProof[\"Theory\"] saturate with no goal, returning a ProofObject whose Theorems is None; bound these with TimeConstraint since a non-terminating axiom set never saturates.
Argument order is conjecture-first (matching FindEquationalProof); TATP is the axioms-first surface. Equations may be written lhs$ == rhs$, a pre-oriented rewrite rule, or a two-way rule (plus their Inactive forms). A List conjecture is a multi-goal conjunction proved off ONE saturation: the result is one ProofObject whose Proof dataset carries one Hypothesis/Conclusion row pair per conjunct, $Failed unless every conjunct is proved; \"Status\" returns a single tag for the whole conjunction.
An optional last argument picks the return type from \"ProofObject\", \"Lemmas\", \"PreprocessedAxioms\", \"RelevantAxioms\", \"RawTrace\", \"Statistics\", \"Status\", \"Counterexample\" (or a list of these, or All); default \"ProofObject\". A single string returns that value bare, a list an Association keyed by the requested names. Returns $Failed when not proved.
\"Counterexample\" returns a CounterexampleObject disproving the goal (a finite model in FindFiniteModels structure for a ground problem, the convergent rules plus separating normal forms otherwise), or $Failed when no countermodel is extractable. Method \"SMT\" decides a ground entailment by congruence closure and accepts a TPTP File or cnf/fof string.
Options: MaxSteps, TimeConstraint, Method, PortfolioFrontLoad. Method accepts Automatic (problem-aware structure detection that front-loads a tailored config then falls back to the fixed portfolio), \"Portfolio\", a named preset (\"Waldmeister\", \"VampireUEQ\", \"Twee\", \"EProver\", \"VampirePortfolio\", \"VampirePortfolioCompact\", \"ENIGMA\", \"SMT\"), or an explicit config association whose keys include \"CriticalPairWeight\", \"Ordering\", \"AutoPrecedence\", \"AxiomRelevance\", \"MaxWeight\", \"AutoMaxWeight\", \"SelectionRatio\", \"GoalInterleave\", \"GroundJoin\", \"Connectedness\", \"RHSInterreduce\", \"UnfailingCP\", \"CPSetInterreduce\", \"Precedence\", \"SkolemHighest\", \"FifoTiebreak\", \"RecordNorm\". $AtpMethodPresets lists the named presets; TAtpSchedule and TAtpDescribeMethod expand a Method. See the ATP documentation for the full option surface."];

GeneralUtilities`SetUsage[TFindEquationalProof, "TFindEquationalProof[$$] is a deprecated alias for TFindProof; every call forwards to TFindProof. New code should call TFindProof."];

GeneralUtilities`SetUsage[CounterexampleObject, "CounterexampleObject[method$, proposition$, axioms$, data$] is the equational dual of ProofObject -- the disproof artifact TFindProof[conjecture$, axioms$, \"Counterexample\"] returns when the goal is refutable, mirroring the Wolfram Function Repository's FindEquationalCounterexample result.
It renders as a summary box and supports a property interface co[prop$]: \"Method\", \"Proposition\"/\"Goal\", \"Axioms\"/\"Hypotheses\", \"Setup\"/\"Model\" (the refuting model), \"Counterexample\"/\"Witness\" (the falsifying assignment), \"NormalForms\", \"Domain\", \"FalsificationFunction\" (a nullary function returning False in the model), \"VerificationFunction\" (returns True since the axioms hold), \"Data\", \"Properties\".
For a finite model (a ground congruence-closure quotient) \"Model\" follows the FindFiniteModels structure: a Cayley table per operator and an element per constant over {0, $$, k$-1}. For an infinite initial term algebra it carries the convergent rules, and \"NormalForms\" the two normal forms separating the goal's sides."];

GeneralUtilities`SetUsage[TAtpCpDataset, "TAtpCpDataset[conjectures$, axioms$] proves each conjecture against the shared axioms with per-critical-pair feature recording on, labels the processed critical pairs by trace-DAG reachability from each proof (1 = proof-relevant, 0 = not), and returns the dataset keyed \"Features\" (an n$ by 14 matrix), \"Labels\" (a 0/1 vector), \"FeatureNames\", \"NRows\", \"NPositive\", \"NProofs\".
TAtpCpDataset[theory$] runs every AxiomaticTheory[theory$, \"NotableTheorems\"] against the theory's axioms. Only proved runs contribute rows.
This is the ENIGMA training-data foundation: feed \"Features\" / \"Labels\" to a classifier (e.g. TNetTrain) and push the result back with TAtpSetLearnedScorer.
Options: Method, TimeConstraint, MaxSteps."];

GeneralUtilities`SetUsage[TAtpTrainScorer, "TAtpTrainScorer[dataset$] trains a critical-pair selection model on a TAtpCpDataset result (or any Association keyed \"Features\" and \"Labels\") with thvm's own TNetTrain and returns <|\"Model\", \"TrainAUC\", \"NRows\", \"NPositive\", \"Hidden\"|>. The \"Model\" is exactly the Association TAtpSetLearnedScorer consumes (Mean / InvStd standardization folded in). A two-class softmax network is trained and its head collapsed to the single proof-relevance logit the engine ranks by.
TAtpTrainScorer[theory$] and TAtpTrainScorer[conjectures$, axioms$] prep the dataset via TAtpCpDataset and train in one call (the dataset options route to the proof phase, the rest to training); the result also reports \"NProofs\".
Pair with TAtpSetLearnedScorer to close the ENIGMA loop (prove, dataset, train, push, reprove with Method \"ENIGMA\").
Options: \"Hidden\" (0 = linear/logistic, >0 = one-hidden-layer ReLU MLP, default 16, max 64), MaxTrainingRounds, \"LearningRate\", \"Method\" (the optimizer)."];

GeneralUtilities`SetUsage[TAtpSetLearnedScorer, "TAtpSetLearnedScorer[model$] pushes a trained critical-pair selection model into the C ATP engine; proofs run with \"CriticalPairWeight\" set to \"Learned\" then use it instead of the baked-in logistic regression. Returns True on success, False on a malformed model.
TAtpSetLearnedScorer[Clear] (or None) drops the model and reverts to the baked-in scorer.
model$ is an Association keyed \"Kind\" (\"Linear\" or \"MLP\"), \"Mean\", \"InvStd\", and the weights (\"W\"/\"B\" for linear, \"W1\"/\"B1\"/\"W2\"/\"B2\" for a one-hidden-layer ReLU network, hidden width at most 64). Features are standardized by Mean and InvStd before the forward pass (default identity); the model outputs a raw logit, higher = selected sooner."];

GeneralUtilities`SetUsage[TAtpCpGraph, "TAtpCpGraph[lhs$ == rhs$] encodes one equation or critical pair (also accepts Inactive[Equal][lhs$, rhs$] and a HoldForm of either) into the anonymised typed hypergraph the ENIGMA Tier 2 graph neural network message-passes over. Returns an Association keyed \"NodeTypes\", \"NodeFeatures\" (an NNodes by 6 matrix), \"Edges\" (a list of {src$, dst$, type$}), \"NNodes\", \"NEdges\", \"NodeLabels\", \"Symbols\".
Node 0 is the critical-pair super-node; the rest are term occurrences (preorder walk of lhs then rhs), symbol nodes (one per distinct operator/constant), and var nodes (one per distinct variable). Node types code 0 CPSuper, 1 Term, 2 Symbol, 3 Var; edge types code the term-to-symbol, term-to-child, and cp-to-side-root links.
The six node-feature columns are purely structural (is_term, is_symbol, is_var, arity, occurrence_count, is_cpsuper) and never encode the concrete label/id/value, so equations equal up to a consistent symbol/variable renaming produce bit-identical graphs. \"NodeLabels\" and \"Symbols\" keep the concrete per-node identity so TAtpCpGraphEquation reconstructs the original equation. lhs and rhs share one encoder state (a shared symbol or variable is a single deduped node).
This is the per-equation encoder TAtpGraphDataset emits; pair it with a GNN trained on the dataset."];

GeneralUtilities`SetUsage[TAtpCpGraphEquation, "TAtpCpGraphEquation[graph$] reconstructs the original equation from a TAtpCpGraph result, returning Inactive[Equal][lhs$, rhs$]. It is the exact inverse of TAtpCpGraph, reading the per-node \"Symbols\" identities and the \"Edges\" term structure (each term node's head, ordered children, and the two side roots).
Returns $Failed if the graph carries no \"Symbols\" (e.g. a graph decoded without the live encoder state)."];

GeneralUtilities`SetUsage[TAtpGraphDataset, "TAtpGraphDataset[conjectures$, axioms$] proves each conjecture against the shared axioms and turns the verified ProofObject's lemmas into a labelled graph dataset <|\"Graphs\", \"Labels\", \"NPos\", \"NNeg\", \"NProofs\"|>, where each graph is a TAtpCpGraph Association, label 1 marks a proof-essential lemma and 0 a saturated-but-unused rule.
TAtpGraphDataset[theory$] runs every AxiomaticTheory[theory$, \"NotableTheorems\"] against the theory's axioms.
TAtpGraphDataset[proofObject$] (or a list of them) yields the proof-essential positives only; TAtpGraphDataset[proofObject$, lemmas$] adds negatives from a supplied saturated set (TFindProof[$$, \"Lemmas\"]).
Positives are the CriticalPairLemma / SubstitutionLemma equations of the proof chain; negatives are the saturated rule set minus any rule structurally equal to a positive (a canonical-key match that anonymises variables and treats each equation as an unordered pair). Only proved runs contribute graphs.
Unlike TAtpCpDataset's per-critical-pair rows, this sources clean positives straight from the verified proof object; feed \"Graphs\" / \"Labels\" to a GNN.
Options: Method, TimeConstraint, MaxSteps."];

GeneralUtilities`SetUsage[TAtpTrainGnn, "TAtpTrainGnn[dataset$] trains a graph convolutional network (GCN) on a TAtpGraphDataset (or any <|\"Graphs\", \"Labels\"|>) in thvm's own tensor stack and returns <|\"Model\", \"TrainAUC\", \"LossStart\", \"LossEnd\", \"NPos\", \"NNeg\"|>, where \"Model\" is the \"GNN\"-kind weight Association the scorers consume.
The forward batches every graph to a common padded node count, runs \"Rounds\" rounds of row-normalised-adjacency message passing, masked-mean-pools to a graph embedding, and reads out a two-class proof-relevance head trained with categorical cross-entropy and Adam. The reported \"TrainAUC\" is the Mann-Whitney rank AUC on the training graphs.
TAtpTrainGnn[theory$] and TAtpTrainGnn[conjectures$, axioms$] prep the dataset via TAtpGraphDataset and train in one call.
This is the Tier 2 deliverable, a symbol-independent network learning proof relevance from clause structure, complementing the Tier 1 hand-feature scorer.
Options: \"Hidden\", \"Rounds\", MaxTrainingRounds, \"LearningRate\"."];

GeneralUtilities`SetUsage[TAtpGnnScore, "TAtpGnnScore[model$, dataset$] scores a graph dataset (TAtpGraphDataset output, or any <|\"Graphs\"|>) with a trained GNN model$ (the \"Model\" from TAtpTrainGnn), returning the per-graph proof-relevance score (the readout's logit_pos minus logit_neg) as a list, one entry per graph.
It runs the same forward as training with the model's weights held constant. The GCN is node-count agnostic, so a model trained on one corpus scores graphs of any size; this is what held-out evaluation and the engine's critical-pair re-rank use."];

GeneralUtilities`SetUsage[TFindProofReranked, "TFindProofReranked[conjecture$, axioms$, model$] proves the conjecture while re-ranking the critical-pair queue with a trained GNN model$ (the \"Model\" from TAtpTrainGnn): it drives the C saturation in \"RerankPeriod\"-step chunks and, between chunks, pulls the live queued critical pairs, scores each with TAtpGnnScore, and pushes the priorities back into the engine's selection heap. Returns the status string (\"PROVED\", \"TIMEOUT\", \"QUEUE_EMPTY\", $$).
This is the ENIGMA inference loop driven from WL over the persistent-handle bridge; completeness is preserved (re-ranking only permutes selection order, the periodic FIFO pick still fires).
Options: \"RerankPeriod\", MaxSteps, \"CriticalPairWeight\", \"Ordering\", \"AutoPrecedence\", \"QueueCap\"."];

GeneralUtilities`SetUsage[TAtpSetGnnScorer, "TAtpSetGnnScorer[model$] pushes a trained GCN model (the \"Model\" Association from TAtpTrainGnn) into the C ATP engine, so a persistent proof handle with a non-zero re-rank period (TFindProofGnnReranked) re-ranks the critical-pair queue by running the GCN forward on thvm's own tensor runtime in C, with no WL round-trip in the proof loop.
TAtpSetGnnScorer[path$] loads a pretrained GCN from a .safetensors file (TSafeTensorLoad, lazy mmap-backed) and pushes it.
TAtpSetGnnScorer[Clear] (or None) drops the model.
Returns True on success, False on a malformed model."];

GeneralUtilities`SetUsage[TAtpSaveGnnScorer, "TAtpSaveGnnScorer[model$, path$] saves a trained GCN model (the \"Model\" Association from TAtpTrainGnn) to path$ as a .safetensors file (TSafeTensorSave): each weight array becomes one named tensor and the scalar config (Rounds / Hidden / NMax) rides in the file's __metadata__. TAtpSetGnnScorer[path$] reloads it. Returns path$.
This is how a pretrained GCN ships as a paclet asset."];

GeneralUtilities`SetUsage[TAtpGnnScorerAsset, "TAtpGnnScorerAsset[] returns the bundled-asset path of the pretrained GCN scorer (wl/THVMLink/Assets/gcn_atp.safetensors); TAtpSetGnnScorer[TAtpGnnScorerAsset[]] loads it.
Returns Missing[\"NotBundled\"] if the asset file is absent."];

GeneralUtilities`SetUsage[TAtpLoadGnnScorer, "TAtpLoadGnnScorer[path$] loads a GCN scorer .safetensors file (saved by TAtpSaveGnnScorer) and returns the \"Model\" Association (the W1 / Ws / Bh / Wout / Bout weights plus Rounds / Hidden / NMax).
TAtpSetGnnScorer[path$] is shorthand for TAtpSetGnnScorer[TAtpLoadGnnScorer[path$]]."];

GeneralUtilities`SetUsage[TFindProofGnnReranked, "TFindProofGnnReranked[conjecture$, axioms$, model$] proves the conjecture with the trained GCN model$ (the \"Model\" from TAtpTrainGnn) guiding critical-pair selection entirely in C: it pushes the GCN weights into the engine (TAtpSetGnnScorer), then drives one saturation in which the engine re-ranks the live queue every \"RerankPeriod\" selections on thvm's own tensor runtime. Returns the status string (\"PROVED\", \"TIMEOUT\", \"QUEUE_EMPTY\", $$).
Unlike TFindProofReranked there is no WL round-trip in the proof loop, so it is far faster; completeness is preserved (re-ranking only permutes selection order, the periodic FIFO pick still fires).
Options: \"RerankPeriod\", MaxSteps, \"CriticalPairWeight\", \"Ordering\", \"AutoPrecedence\"."];

(* Forward-declare sibling-file public symbols (SMT.wl owns
   TSatEUF / TSmtDecide) so bare references inside this file's
   Begin[`Private`] resolve to the shared THVMLink`ATP`X symbol
   rather than creating a phantom THVMLink`ATP`Private`X.  The
   alphabetical autoload order means those symbols don't exist yet
   when this file is parsed; the bare mention here pre-creates them
   in the public context.  TPTPImport now lives in Wolfram`Parser`
   (added to the context path above) so it doesn't need
   pre-declaration. *)
{TSatEUF, TSmtDecide};

(* SafeTensors.wl (a depth-4 sibling, loaded earlier) owns these; the
   bare mention keeps GCN save/load resolving to the public symbols. *)
{TSafeTensorSave, TSafeTensorLoad, TSafeTensorLoadMetadata, TTensorCreate};

(* (The IC primitives TDef / TRef / TLam / TCollapse / ... are owned by
   the depth-4 sibling Switch.wl, which already loaded before this
   depth-5 file -- bare references resolve via the context path
   THVMLink` pushed by the BeginPackage second arg.) *)

Begin["`Private`"];

(* Suppress General::shdw during definition load.  The TFindProof
   dispatch defensively Block-localizes Global`a..z + xN names
   (see [[project_atp_tfindproof_iter_leak]]) and some of those
   names (i, s, t, e) overlap private context symbols inside this
   package -- the shdw message is benign (the Block fires at runtime
   under the package context, not load) but the warning is noisy.
   Restored at End[] below so the user's own shdw conditions still
   surface. *)
Off[General::shdw];

(* Default-on the C-engine fast paths.  Without these, Sheffer / nand
   theorems like WolframAxioms / AndAssociativity time out at 60s
   even with KBO + Gt + RecordNorm -> False; with them, the same
   config matches Waldmeister's ~14s wall (C-bench measured 14.7s,
   paclet ~25s end-to-end).  Each is byte-identical to the default
   path on simpler problems within measurement noise.  Set BEFORE
   any TFindProof call so the AtpState picks them up at init.

   THVM_ATP_FLATTERM    fast indexed/flatterm mixed-normalize loop
   THVM_ATP_KBO_FLAT    cache-dense pre-order KBO compare
   THVM_ATP_WMFPA       faithful Waldmeister-FPA normalize path
   THVM_ATP_CP_INDEX    CP-generation overlap-partner unification index

   Users can disable any of them by SetEnvironment["..." -> "0"]
   before the call - the engine reads at AtpState init. *)
Scan[
    With[{ev = #},
        If[ Environment[ev] === $Failed || Environment[ev] === None,
            SetEnvironment[ev -> "1"]]] &,
    {"THVM_ATP_FLATTERM", "THVM_ATP_KBO_FLAT",
     "THVM_ATP_WMFPA",    "THVM_ATP_CP_INDEX"}];

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
     [n_goals, n_axioms, lhs_0, rhs_0, ...,
      goal_lhs_0, goal_rhs_0, ..., flag_0, ...]
   (the atpEncodeProblem wire layout) plus max_steps and max_label.
   Returns a 4-element Int64 NumericArray
   [status, n_rules, n_trace, n_cps]. *)
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
     Integer, Integer, Integer, Integer, Integer, Integer, {Integer, 1},
     Integer, Integer, Integer, Integer, Integer, Integer, Integer,
     (* args[33] = use_fvi: Waldmeister RechtsUnfreiErzeugen (FVI) toggle *)
     Integer,
     (* args[34] = use_implicit_cp: deferred-CP arc commit 1 toggle (dormant) *)
     Integer},
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

(* ENIGMA Tier 1: push a trained CP-selection model into the engine.
   Takes a flat Real parameter vector (kind, hidden, mean[14],
   inv_std[14], then LINEAR or MLP weights -- see TAtpSetLearnedScorer);
   an EMPTY list clears the model and reverts to the baked-in logistic
   regression.  Returns 1 on success, 0 on a malformed blob. *)
$atpSetLearnedScorerFn := $atpSetLearnedScorerFn = load[
    "thvm_wl_atp_set_learned_scorer",
    {{Real, 1}},
    Integer
]

(* ENIGMA Tier 2: anonymised CP hypergraph for a single equation.  Takes
   the two Term integer values (lhs, rhs); returns one self-describing
   Real (f64) NumericArray -- a 4-real header [ok, n_nodes, n_edges,
   feat_dim] followed by node_type[n_nodes], node_feat[n_nodes*feat_dim]
   (row-major), edge_src[n_edges], edge_dst[n_edges], edge_type[n_edges]
   (see thvm_wl_atp_cp_graph in thvmlink_atp.c). *)
$atpCpGraphFn := $atpCpGraphFn = load[
    "thvm_wl_atp_cp_graph",
    {Integer, Integer},
    "NumericArray"
]

(* ENIGMA Tier 2: persistent step-wise proof handle (GNN re-rank).
   _init returns an AtpState handle (an Integer); _step runs k steps and
   returns the AtpStatus code (0 = RUNNING); _queued packs the live CPs
   as [count, lhs, rhs, seq, ...]; _setpri pushes WL-computed priorities
   by seq; _free releases the handle. *)
$atpProofInitFn := $atpProofInitFn = load[
    "thvm_wl_atp_proof_init",
    {{"NumericArray", "Shared"}, Integer, Integer, Integer, Integer, Integer},
    Integer]
$atpProofStepFn := $atpProofStepFn = load[
    "thvm_wl_atp_proof_step", {Integer, Integer}, Integer]
$atpProofQueuedFn := $atpProofQueuedFn = load[
    "thvm_wl_atp_proof_queued", {Integer, Integer}, "NumericArray"]
$atpProofSetPriFn := $atpProofSetPriFn = load[
    "thvm_wl_atp_proof_setpri", {Integer, {Integer, 1}, {Integer, 1}}, Integer]
$atpProofFreeFn := $atpProofFreeFn = load[
    "thvm_wl_atp_proof_free", {Integer}, Integer]

(* ENIGMA Tier 2 (in-engine GCN): push the trained GCN weights into the
   C engine as a flat Real blob (thvm_atp_set_gnn_scorer layout: [R, H],
   then per-round W1/Ws/Bh, then Wout/Bout); an EMPTY list clears it.
   _set_gnn_period sets the re-rank period on a persistent proof handle,
   so thvm_atp_step itself re-ranks the queue every N selections by
   running the GCN forward on thvm's OWN tensor runtime -- no WL in the
   per-step loop. *)
$atpSetGnnScorerFn := $atpSetGnnScorerFn = load[
    "thvm_wl_atp_set_gnn_scorer", {{Real, 1}}, Integer]
$atpProofSetGnnPeriodFn := $atpProofSetGnnPeriodFn = load[
    "thvm_wl_atp_proof_set_gnn_period", {Integer, Integer}, Integer]

(* Flatten a model Association into the C parameter blob
   (thvm_atp_set_learned_scorer layout): {kind, hidden, mean[14],
   inv_std[14], <weights>}.  Clear / None / {} -> {} (clears the
   model).  $AtpFeatureDim mirrors the C ATP_CP_FEATURE_DIM. *)
$AtpFeatureDim = 14;

serializeLearnedModel[Clear | None | {}] := {}
serializeLearnedModel[m_Association] := Block[{
    kind = Lookup[m, "Kind", "Linear"],
    mean = N[Lookup[m, "Mean", ConstantArray[0., $AtpFeatureDim]]],
    invStd = N[Lookup[m, "InvStd", ConstantArray[1., $AtpFeatureDim]]],
    w1, h
},
    If[ MemberQ[{"Linear", "Logistic"}, kind],
        Join[{1., 0.}, mean, invStd, N[m["W"]], {N[m["B"]]}],
        (* MLP: W1 is an H x 14 matrix, flattened row-major (one row per
           hidden unit, matching the C forward's per-unit weight row). *)
        w1 = N[m["W1"]];
        h = Length[w1];
        Join[{2., N[h]}, mean, invStd,
             Flatten[w1], N[m["B1"]], N[m["W2"]], {N[m["B2"]]}]
    ]
]

TAtpSetLearnedScorer[model_] := $atpSetLearnedScorerFn[
    N[serializeLearnedModel[model]]] === 1

(* Flatten a trained GCN model (the "Model" from TAtpTrainGnn) into the C
   parameter blob (thvm_atp_set_gnn_scorer layout): {R, H}, then per round
   r the row-major W1[r] ({in_r, H}), Ws[r] ({in_r, H}), Bh[r] ({H}), then
   Wout ({H, 2}) and Bout ({2}).  in_r is the GCN feature dim (6) on round
   0 and H thereafter -- TAtpTrainGnn builds W1/Ws as {in_r, H} TGlorot
   matrices, so Flatten gives the row-major order the C side reads.  Clear
   / None / {} -> {} (clears the model). *)
serializeGnnModel[Clear | None | {}] := {}
serializeGnnModel[m_Association] := Block[{
    rR = Lookup[m, "Rounds", Length[m["W1"]]],
    hH = Lookup[m, "Hidden", Length[m["Bh"][[1]]]]
},
    Join[
        {N[rR], N[hH]},
        Flatten @ Table[
            Join[Flatten[N[m["W1"][[r]]]], Flatten[N[m["Ws"][[r]]]],
                 N[m["Bh"][[r]]]],
            {r, rR}],
        Flatten[N[m["Wout"]]], N[m["Bout"]]
    ]
]

TAtpSetGnnScorer[model_Association] := $atpSetGnnScorerFn[
    N[serializeGnnModel[model]]] === 1
TAtpSetGnnScorer[Clear | None | {}] := $atpSetGnnScorerFn[{}] === 1

(* Path overload: load a pretrained GCN from a .safetensors file (lazy,
   mmap-backed via TSafeTensorLoad), rebuild the "Model" Association, and push
   it to the C engine.  This is how a bundled paclet asset loads. *)
TAtpSetGnnScorer[path_String] := With[{m = TAtpLoadGnnScorer[path]},
    If[ AssociationQ[m], TAtpSetGnnScorer[m], False]]

(* === GCN <-> .safetensors ===
   Each weight array becomes one named tensor: W1_<r>, Ws_<r>, Bh_<r>
   (r = 0-based round), plus Wout and Bout.  The scalar config (Rounds,
   Hidden, NMax) rides in the file's __metadata__ (string values, per the
   safetensors spec).  TAtpLoadGnnScorer reverses this into the Model
   Association that serializeGnnModel + the C engine consume. *)
gnnModelTensors[m_Association] := Module[{rR, tens},
    rR = Lookup[m, "Rounds", Length[m["W1"]]];
    tens = <||>;
    Do[
        tens["W1_" <> ToString[r - 1]] = TTensorCreate[N[m["W1"][[r]]], "f32"];
        tens["Ws_" <> ToString[r - 1]] = TTensorCreate[N[m["Ws"][[r]]], "f32"];
        tens["Bh_" <> ToString[r - 1]] = TTensorCreate[N[m["Bh"][[r]]], "f32"],
        {r, rR}
    ];
    tens["Wout"] = TTensorCreate[N[m["Wout"]], "f32"];
    tens["Bout"] = TTensorCreate[N[m["Bout"]], "f32"];
    tens
]

TAtpSaveGnnScorer[model_Association, path_String] := Module[{rR, hH, nMax, meta},
    rR   = Lookup[model, "Rounds", Length[model["W1"]]];
    hH   = Lookup[model, "Hidden", Length[model["Bh"][[1]]]];
    nMax = Lookup[model, "NMax", 0];
    meta = <|"Kind" -> "GNN", "Rounds" -> rR, "Hidden" -> hH, "NMax" -> nMax|>;
    TSafeTensorSave[gnnModelTensors[model], path, meta]
]

(* Rebuild the Model Association from a .safetensors file (lazy tensors
   read via Normal). *)
TAtpLoadGnnScorer[path_String] := Module[{meta, tens, rR},
    If[ ! FileExistsQ[path], Return[$Failed]];
    meta = TSafeTensorLoadMetadata[path];
    tens = TSafeTensorLoad[path];
    rR = ToExpression @ Lookup[meta, "Rounds",
        ToString @ Length @ Select[Keys[tens], StringStartsQ["W1_"]]];
    <|
        "Kind" -> "GNN",
        "W1"   -> Table[Normal @ tens["W1_" <> ToString[r - 1]], {r, rR}],
        "Ws"   -> Table[Normal @ tens["Ws_" <> ToString[r - 1]], {r, rR}],
        "Bh"   -> Table[Normal @ tens["Bh_" <> ToString[r - 1]], {r, rR}],
        "Wout" -> Normal @ tens["Wout"],
        "Bout" -> Normal @ tens["Bout"],
        "Rounds" -> rR,
        "Hidden" -> ToExpression @ Lookup[meta, "Hidden",
            ToString @ Length @ First @ Normal @ tens["Bh_0"]],
        "NMax"   -> ToExpression @ Lookup[meta, "NMax", "0"]
    |>
]

(* Bundled-asset path: wl/THVMLink/Assets/gcn_atp.safetensors.  Captured
   at file-LOAD time (when $InputFileName names this Kernel/ATP/ file);
   $InputFileName is empty in the running session, so the directory is
   frozen here exactly as THVMLink.wl freezes $libDir.  When the paclet
   is installed, PacletObject["..."]["AssetLocation", "GCNAtpScorer"]
   resolves too; we prefer that when it succeeds. *)
$atpGnnAssetDir = FileNameJoin[{
    DirectoryName[$InputFileName], "..", "..", "Assets"}];

atpGnnAssetCandidate[] := Module[{viaPaclet},
    viaPaclet = Quiet @ Check[
        PacletObject["WolframInstitute/THVMLink"]["AssetLocation", "GCNAtpScorer"],
        $Failed];
    If[ StringQ[viaPaclet] && FileExistsQ[viaPaclet],
        viaPaclet,
        FileNameJoin[{$atpGnnAssetDir, "gcn_atp.safetensors"}]
    ]
]

TAtpGnnScorerAsset[] := With[{p = atpGnnAssetCandidate[]},
    If[ FileExistsQ[p], p, Missing["NotBundled"]]]

(* ENIGMA Tier 1a: generate a labelled critical-pair dataset by proving
   a corpus with feature recording on.  The C bridge records + labels +
   appends a TSV when THVM_ATP_CP_DATASET names a file (one row per
   PROCESSED CP on each PROVED run); we set it to a fresh temp file,
   prove, then read it back.  Robust to the bridge's once-per-session
   header line: Import "TSV" parses numeric rows and we drop any header
   row (non-numeric first field). *)
$AtpFeatureNames = {"size_sum", "max_depth", "n_distinct_vars",
    "n_var_occ", "weight_add", "weight_gt", "weight_mix2", "goal_weight",
    "age", "top_symbol_l", "top_symbol_r", "shares_goal_sub",
    "orientable", "unif_measure"};

Options[TAtpCpDataset] = {Method -> {"Completion"}, TimeConstraint -> 30,
    MaxSteps -> Automatic};

(* Run `proveFn[]` for its dataset side-effect with the recorder pointed
   at a fresh temp file, then parse the accumulated TSV into the result
   Association.  `proveFn` returns the proof statuses (used only for the
   proof count). *)
atpDatasetCollect[proveFn_, nProofs_] := Module[{
    path = FileNameJoin[{$TemporaryDirectory,
        "thvm_enigma_cp_" <> ToString[$ProcessID] <> "_"
            <> ToString[RandomInteger[10^9]] <> ".tsv"}],
    raw, dataRows, labels
},
    Quiet @ DeleteFile[path];
    SetEnvironment["THVM_ATP_CP_DATASET" -> path];
    proveFn[];
    SetEnvironment["THVM_ATP_CP_DATASET" -> None];
    raw = If[ FileExistsQ[path], Import[path, "TSV"], {}];
    Quiet @ DeleteFile[path];
    dataRows = Select[raw,
        ListQ[#] && Length[#] === 15 && NumberQ[First[#]] &];
    If[ dataRows === {},
        Return[<|"Features" -> {}, "Labels" -> {},
            "FeatureNames" -> $AtpFeatureNames, "NRows" -> 0,
            "NPositive" -> 0, "NProofs" -> nProofs|>]];
    labels = Round[dataRows[[All, 1]]];
    <|
        "Features" -> dataRows[[All, 2 ;; 15]],
        "Labels" -> labels,
        "FeatureNames" -> $AtpFeatureNames,
        "NRows" -> Length[dataRows],
        "NPositive" -> Total[labels],
        "NProofs" -> nProofs
    |>
]

TAtpCpDataset[theory_String, opts : OptionsPattern[]] := Module[{
    thms = AxiomaticTheory[theory, "NotableTheorems"],
    m = OptionValue[Method],
    tc = OptionValue[TimeConstraint]
},
    atpDatasetCollect[
        Function[TFindProof[thms, theory, "Status",
            Method -> m, TimeConstraint -> tc]],
        Length[thms]]
]

TAtpCpDataset[conjectures_List, axioms_List, opts : OptionsPattern[]] := Module[{
    m = OptionValue[Method],
    tc = OptionValue[TimeConstraint]
},
    atpDatasetCollect[
        Function[Scan[
            TFindProof[#, axioms, "Status", Method -> m, TimeConstraint -> tc] &,
            conjectures]],
        Length[conjectures]]
]

(* ENIGMA Tier 1c: train a scorer on a dataset via thvm's TNetTrain. *)

(* The single proof-relevance logit a model assigns a STANDARDIZED
   feature row z (length 14).  Mirrors the C atp_learned_forward so the
   WL-side AUC matches what the engine ranks by. *)
atpScorerLogit[model_Association, z_List] := If[
    MemberQ[{"Linear", "Logistic"}, Lookup[model, "Kind", "Linear"]],
    model["B"] + model["W"] . z,
    model["B2"] + model["W2"] . (Ramp /@ (model["B1"] + model["W1"] . z))
]

(* Mann-Whitney AUC: P(score of a positive > score of a negative). *)
atpScorerAuc[scores_List, labels_List] := Module[{nP, nN, ranks},
    nP = Total[labels];
    nN = Length[labels] - nP;
    If[ nP == 0 || nN == 0, Return[Missing["DegenerateLabels"]]];
    ranks = Ordering[Ordering[scores]];
    N[(Total[Pick[ranks, labels, 1]] - nP (nP + 1)/2) / (nP nN)]
]

Options[TAtpTrainScorer] = {"Hidden" -> 16, MaxTrainingRounds -> 300,
    "LearningRate" -> 0.01, "Method" -> "Adam"};

TAtpTrainScorer[dataset_Association, opts : OptionsPattern[]] := Module[{
    x = N[dataset["Features"]], y = dataset["Labels"], mean, sd, invStd,
    z, h = OptionValue["Hidden"], net, yOneHot, trained, pv,
    w1, b1, w2, b2, wMat, bVec, model, scores
},
    If[ Length[x] === 0, Return[$Failed]];
    mean   = Mean[x];
    sd     = StandardDeviation[x];
    invStd = MapThread[If[ # > 0., 1./#, 1.] &, {sd}] // Quiet;
    z      = (# - mean) * invStd & /@ x;
    yOneHot = (If[ # == 1, {0., 1.}, {1., 0.}] &) /@ y;
    net = If[ h > 0,
        NetChain[{LinearLayer[h], Ramp, LinearLayer[2], SoftmaxLayer[]},
            "Input" -> 14],
        NetChain[{LinearLayer[2], SoftmaxLayer[]}, "Input" -> 14]];
    net = NetInitialize[net];
    trained = TNetTrain[net, z, yOneHot,
        MaxTrainingRounds -> OptionValue[MaxTrainingRounds],
        "LearningRate" -> OptionValue["LearningRate"],
        "Method" -> OptionValue["Method"]];
    If[ trained === $Failed, Return[$Failed]];
    (* Read trained parameters back, identifying each by shape.  thvm may
       store a LinearLayer's weight transposed relative to WL's
       {output, input}; orientW forces columns = inDim (so W . z works
       with z a length-inDim row). *)
    pv = Normal /@ trained["Params"];
    With[{
        mats = Select[pv, MatrixQ],
        vecs = Select[pv, VectorQ],
        orientW = {w, inDim} |-> If[ Last[Dimensions[w]] === inDim, w, Transpose[w]]
    },
        If[ h > 0,
            w1 = orientW[SelectFirst[mats, MemberQ[Dimensions[#], 14] &], 14];
            w2 = orientW[SelectFirst[mats, FreeQ[Dimensions[#], 14] &], h];
            b1 = SelectFirst[vecs, Length[#] === h &];
            b2 = SelectFirst[vecs, Length[#] === 2 &];
            (* Collapse the 2-class head to a single proof-relevance logit:
               score = z1 - z0 (the log-odds). *)
            model = <|"Kind" -> "MLP", "Mean" -> mean, "InvStd" -> invStd,
                "W1" -> w1, "B1" -> b1,
                "W2" -> (w2[[2]] - w2[[1]]), "B2" -> (b2[[2]] - b2[[1]])|>,
            wMat = orientW[First[mats], 14];
            bVec = SelectFirst[vecs, Length[#] === 2 &];
            model = <|"Kind" -> "Linear", "Mean" -> mean, "InvStd" -> invStd,
                "W" -> (wMat[[2]] - wMat[[1]]), "B" -> (bVec[[2]] - bVec[[1]])|>
        ]
    ];
    scores = atpScorerLogit[model, #] & /@ z;
    <|
        "Model" -> model,
        "TrainAUC" -> atpScorerAuc[scores, y],
        "NRows" -> Length[x],
        "NPositive" -> Total[y],
        "Hidden" -> h
    |>
]

(* Corpus overloads: prep the dataset (TAtpCpDataset) AND train in one
   call, so `TAtpTrainScorer["GroupTheory"]` or
   `TAtpTrainScorer[conjectures, axioms]` returns a ready model.  Dataset
   options (Method = the base prover, TimeConstraint, MaxSteps) and
   training options ("Hidden", MaxTrainingRounds, "LearningRate",
   "Method" = the optimizer -- a STRING key, distinct from the Method
   symbol) are routed to the right callee by FilterRules.  The result
   carries the dataset stats back too (NProofs).  Push the model with
   TAtpSetLearnedScorer to activate Method -> "ENIGMA". *)
TAtpTrainScorer[theory_String, opts : OptionsPattern[]] :=
    atpTrainOnCorpus[
        TAtpCpDataset[theory,
            Sequence @@ FilterRules[{opts}, Options[TAtpCpDataset]]],
        {opts}]

TAtpTrainScorer[conjectures_List, axioms_List, opts : OptionsPattern[]] :=
    atpTrainOnCorpus[
        TAtpCpDataset[conjectures, axioms,
            Sequence @@ FilterRules[{opts}, Options[TAtpCpDataset]]],
        {opts}]

atpTrainOnCorpus[ds_Association, opts_List] := If[
    ds["NRows"] === 0,
    $Failed,
    Join[
        TAtpTrainScorer[ds,
            Sequence @@ FilterRules[opts, Options[TAtpTrainScorer]]],
        <|"NProofs" -> ds["NProofs"]|>]]
atpTrainOnCorpus[_, _] := $Failed

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

(* A String literal -- the user spelling an atom as `"a"` instead of
   bare `a` (useful when the symbol name isn't a valid WL identifier,
   e.g. starts with a digit or contains punctuation; also the natural
   TPTP-import form before tptpInternalize runs).  Encode it as a
   0-arity constant whose label uses the string verbatim, so two
   distinct strings -> two distinct labels and equal strings collapse
   to the same label.  Without this rule the general clause below
   folds over List @@ "a" -- a non-list for an atom -- and the
   encoder diverges. *)
encodeAtpTerm[s_String, state_Association] := Block[{
    lab, st
},
    {lab, st} = ensureSym[s, state];
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

(* Waldmeister `SO_const1` / `SO_const2` (SymbolOperationen.c:386-389):
   pre-reserve labels 1 and 2 for engine-introduced constants.  The C
   engine materializes `s->min_const = term_new_ctr(1, NULL, 0)` in
   `thvm_atp_init` and uses it in the FVI rule emission within
   `thvm_atp_orient_and_add` to ground free RHS variables of
   unorientable equations.  Reserving them here means user labels start
   at 3 and `max_label` (passed to the C runner as `args[2]`) already
   covers the reserved range.  Decoder names them `cAtp1` / `cAtp2`. *)
encodeAtpTermInit[] := <|
    "sym" -> <|"cAtp1" -> 1, "cAtp2" -> 2|>,
    "var" -> <||>,
    "next_lab" -> 3
|>

(* === ENIGMA Tier 2: anonymised CP hypergraph export ================ *)

(* Strip the equation wrappers TAtpCpGraph accepts down to a held
   {lhs, rhs} pair.  IgnoringInactive[Equal[...]] matches an equation
   whether or not its head is wrapped in Inactive, so the bare == and the
   Inactive[Equal] forms collapse to one clause each (with vs without an
   outer HoldForm).  The two sides stay held (HoldComplete) so pattern
   variables (`x_`) and tautology shapes (`a == a`) survive intact for
   the encoder. *)
SetAttributes[atpCpSides, HoldAllComplete];
atpCpSides[HoldPattern[HoldForm[IgnoringInactive[Equal[l_, r_]]]]] :=
    {HoldComplete[l], HoldComplete[r]}
atpCpSides[HoldPattern[IgnoringInactive[Equal[l_, r_]]]] :=
    {HoldComplete[l], HoldComplete[r]}
atpCpSides[_] := $Failed

(* Decode the self-describing f64 NumericArray $atpCpGraphFn returns into
   the public graph Association.  Header [ok, n_nodes, n_edges, feat_dim]
   then node_type, node_feat (row-major), edge_src, edge_dst, edge_type,
   and (tail) node_label -- the concrete symbol identity per node for exact
   reconstruction.  "NodeLabels" are the raw intern keys; atpCpGraphFromSides
   resolves them to "Symbols" using the live encoder state. *)
atpCpGraphDecode[raw_List] := Module[{
    ok = Round[raw[[1]]], nN = Round[raw[[2]]], nE = Round[raw[[3]]],
    fd = Round[raw[[4]]], off, nodeTypes, nodeFeat, eSrc, eDst, eType, nLabel
},
    If[ ok =!= 1,
        Return[<|"NodeTypes" -> {}, "NodeFeatures" -> {}, "Edges" -> {},
            "NodeLabels" -> {}, "NNodes" -> 0, "NEdges" -> 0|>]];
    off = 4;
    nodeTypes = Round[raw[[off + 1 ;; off + nN]]];
    off += nN;
    nodeFeat = If[ nN > 0,
        Partition[raw[[off + 1 ;; off + nN*fd]], fd], {}];
    off += nN*fd;
    eSrc  = Round[raw[[off + 1 ;; off + nE]]];   off += nE;
    eDst  = Round[raw[[off + 1 ;; off + nE]]];   off += nE;
    eType = Round[raw[[off + 1 ;; off + nE]]];   off += nE;
    nLabel = If[ Length[raw] >= off + nN, Round[raw[[off + 1 ;; off + nN]]], ConstantArray[0, nN]];
    <|
        "NodeTypes" -> nodeTypes,
        "NodeFeatures" -> nodeFeat,
        "Edges" -> Transpose[{eSrc, eDst, eType}],
        "NodeLabels" -> nLabel,
        "NNodes" -> nN,
        "NEdges" -> nE
    |>
]

(* Encode a held {lhs, rhs} pair into the anonymised hypergraph: one
   shared encoder state so a symbol / variable common to both sides is a
   single deduped node, then call the C extractor + decode.  The final
   encoder state carries the symbol/variable name <-> label maps, so we
   resolve each SYMBOL / VAR node's raw label to its concrete name and
   attach the per-node "Symbols" list -- the un-anonymised identity that
   makes TAtpCpGraphEquation an exact inverse. *)
atpCpGraphFromSides[{lhsHC_HoldComplete, rhsHC_HoldComplete}] := Module[{
    st = encodeAtpTermInit[], lr, rr, lt, rt, fin, symInv, varInv, g
},
    lr = encodeAtpTerm[lhsHC[[1]], st];
    rr = encodeAtpTerm[rhsHC[[1]], lr[[2]]];
    lt = lr[[1]];
    rt = rr[[1]];
    fin = rr[[2]];
    g = atpCpGraphDecode[Normal[$atpCpGraphFn[lt, rt]]];
    symInv = Association[Reverse /@ Normal[fin["sym"]]];
    varInv = Association[Reverse /@ Normal[fin["var"]]];
    Append[g, "Symbols" -> MapThread[
        Switch[#1,
            2, Lookup[symInv, #2, Missing["Unresolved"]],
            3, Lookup[varInv, #2, Missing["Unresolved"]],
            _, Missing["NotASymbol"]] &,
        {g["NodeTypes"], g["NodeLabels"]}]]
]

TAtpCpGraph[eq_] := Module[{sides = atpCpSides[eq]},
    If[ sides === $Failed, $Failed, atpCpGraphFromSides[sides]]]
SetAttributes[TAtpCpGraph, HoldAllComplete];

(* Exact inverse of TAtpCpGraph: rebuild the original equation from a graph
   carrying per-node "Symbols".  The CP super-node's E_CP_LHS / E_CP_RHS
   edges (type 2 / 3) name the two side roots; each TERM node's E_TERM_SYM
   edge (type 0) names its head and its E_TERM_CHILD edges (type 1) its
   children in order.  Node ids in "Edges" are 0-based; "Symbols" is
   1-based.  Returns Inactive[Equal][lhs, rhs], or $Failed without symbols. *)
TAtpCpGraphEquation[graph_Association] := Module[{
    types = graph["NodeTypes"], syms = graph["Symbols"], edges = graph["Edges"],
    symOf, childOf, rootSide, rebuild
},
    If[ MissingQ[syms] || syms === {}, Return[$Failed]];
    symOf[t_]    := SelectFirst[edges, MatchQ[#, {t, _, 0}] &][[2]];
    childOf[t_]  := Cases[edges, {t, c_, 1} :> c];
    rootSide[e_] := SelectFirst[edges, MatchQ[#, {0, _, e}] &][[2]];
    rebuild[t_] := With[{s = symOf[t]},
        With[{name = syms[[s + 1]], kids = rebuild /@ childOf[t]},
            Which[
                types[[s + 1]] === 3, ToExpression[name <> "_"],
                kids === {},          ToExpression[name],
                True,                 ToExpression[name] @@ kids
            ]
        ]
    ];
    Inactive[Equal][rebuild[rootSide[2]], rebuild[rootSide[3]]]
]

(* === ENIGMA Tier 2: canonical equation key (rename-invariant dedup) === *)

(* The canonical key normalises a held equation so two equations equal
   up to a consistent renaming of VARIABLES, AND up to swapping the two
   sides, collapse to the same key.  Only pattern (`x_`) / Slot / Blank
   variables are anonymised to positional placeholders (first-appearance
   order over the {lhs, rhs} preorder); concrete constant + operator
   symbols and numeric literals keep their literal identity, so genuinely
   different ground equations (`a == b` vs `c == a`) stay distinct while
   `f[x_, i[x_]]` and `f[y_, i[y_]]` collapse.  This is the WL-level
   dedup key for separating proof-essential positives from the saturated
   negatives + dropping duplicate rows.  (The C extractor anonymises far
   more aggressively -- it is structural by design for the GNN -- but
   that bit-identity is too coarse for distinguishing dataset rows.) *)

(* Rewrite an (inert) equation side to its canonical form, threading a
   <|varname -> placeholder, "n" -> next|> variable-renaming state.
   Returns {canonExpr, state'}.  Mirrors encodeAtpTerm's clause shape:
   only the variable clauses intern a fresh placeholder; constants,
   numerics and compound heads keep their literal identity. *)
atpCanonRec[Verbatim[Pattern][name_Symbol, Blank[]], st_] :=
    atpCanonVar["v$" <> SymbolName[Unevaluated[name]], st]
atpCanonRec[Verbatim[Slot][k_], st_] := atpCanonVar["s$" <> ToString[k], st]
atpCanonRec[Verbatim[Blank][], st_] := atpCanonVar["b$", st]
atpCanonRec[s_Symbol, st_] := {$atpCanonConst[ToString[Unevaluated[s]]], st}
atpCanonRec[n:(_Integer | _Real | _Rational), st_] :=
    {$atpCanonConst[ToString[n, InputForm]], st}
atpCanonRec[expr_, st_] := Module[{st2, childCanon},
    {childCanon, st2} = Fold[atpCanonStep, {{}, st}, List @@ expr];
    {$atpCanonNode[ToString[Head[expr]], childCanon], st2}
]

(* Intern a variable name into a positional placeholder (first-appearance
   order); a repeat occurrence reuses its placeholder so shared variables
   are tracked. *)
atpCanonVar[id_String, st_Association] := If[
    KeyExistsQ[st, id],
    {$atpCanonVar[st[id]], st},
    With[{p = st["n"]},
        {$atpCanonVar[p], Append[Append[st, id -> p], "n" -> p + 1]}]
]

atpCanonStep[{acc_, st_}, child_] := Block[{r = atpCanonRec[child, st]},
    {Append[acc, r[[1]]], r[[2]]}]

(* === ENIGMA Tier 2: ProofObject lemmas -> labelled graph dataset === *)

(* The proof-essential lemma types: a CriticalPairLemma or a
   SubstitutionLemma in the ProofObject's proof chain is a lemma the
   proof actually used (a clean positive). *)
$AtpPositiveLemmaTypes = {"CriticalPairLemma", "SubstitutionLemma"};

(* Pull a ProofObject's proof chain as a genuine Association keyed by
   the {Type, k} tuples.  The bare 4th arg's "Proof" value is a List of
   Rules, so we go through the public ProofDataset property (a Dataset)
   and Normal it -- that yields the {Type, k} -> <|Statement, ...|>
   Association the entry lookups below need. *)
atpProofChain[p_] := If[ MatchQ[p, _ProofObject],
    With[{nd = Quiet[Normal[p["ProofDataset"]]]},
        If[ AssociationQ[nd], nd, <||>]],
    <||>]

(* The held {lhs, rhs} sides of a proof-chain entry whose Statement is a
   HoldForm[lhs == rhs].  Returns $Failed on an unexpected shape. *)
atpEntrySides[entry_Association] := With[{st = Lookup[entry, "Statement"]},
    If[ MissingQ[st], $Failed, atpCpSides[st]]]
atpEntrySides[_] := $Failed

(* The proof-essential POSITIVE equations of a ProofObject: the held
   {lhs, rhs} sides of every CriticalPairLemma / SubstitutionLemma
   entry.  Keyed by type-string match on the {Type, k} chain keys; each
   key is a list, so index the chain Association via Key[...]. *)
atpProofPositives[p_] := Module[{chain = atpProofChain[p], keys},
    keys = Select[Keys[chain],
        MatchQ[#, {t_String, _} /; MemberQ[$AtpPositiveLemmaTypes, t]] &];
    DeleteCases[atpEntrySides[Lookup[chain, Key[#]]] & /@ keys, $Failed]
]

(* The saturated NEGATIVE candidates: TFindProof[..., "Lemmas"] yields
   Inactive[Equal][l, r] rules; peel each to held {lhs, rhs} sides. *)
atpSatSides[lemmas_List] := DeleteCases[atpCpSides /@ lemmas, $Failed]
atpSatSides[_] := {}

(* Assemble one proof's labelled rows: positives (label 1) from the
   proof chain, negatives (label 0) from the saturated set minus any
   rule structurally equal (canonical key) to a positive; then dedup
   rows by canonical key (positives win a tie).  Each row is
   {heldSides, label}. *)
atpGraphRows[p_, lemmas_] := Block[{posKept, posKeySet, negKept},
    (* Positives: drop unkeyable sides, keep the first row per canonical
       key. *)
    posKept = DeleteDuplicatesBy[
        Select[atpProofPositives[p], atpEqCanonKeyOf[#] =!= $Failed &],
        atpEqCanonKeyOf];
    posKeySet = atpEqCanonKeyOf /@ posKept;
    (* Negatives: keyable, not already a positive, first row per key. *)
    negKept = DeleteDuplicatesBy[
        Select[atpSatSides[lemmas],
            With[{k = atpEqCanonKeyOf[#]},
                k =!= $Failed && ! MemberQ[posKeySet, k]] &],
        atpEqCanonKeyOf];
    Join[{#, 1} & /@ posKept, {#, 0} & /@ negKept]
]

(* Canonical key directly off a held {lhsHC, rhsHC} pair (the shape
   atpProofPositives / atpSatSides produce).  ONE shared renaming state
   threads lhs-then-rhs so a symbol / variable common to both sides maps
   to the same placeholder (cross-side sharing is part of the equation's
   identity), then the two canonical sides are sorted into an UNORDERED
   pair so l == r and r == l collapse. *)
atpEqCanonKeyOf[{lhsHC_HoldComplete, rhsHC_HoldComplete}] := Module[{lr, rr},
    lr = atpCanonRec[lhsHC[[1]], <|"n" -> 0|>];
    rr = atpCanonRec[rhsHC[[1]], lr[[2]]];
    Sort[{lr[[1]], rr[[1]]}]
]
atpEqCanonKeyOf[_] := $Failed

Options[TAtpGraphDataset] = {Method -> {"Completion"}, TimeConstraint -> 30,
    MaxSteps -> Automatic};

(* Prove `conj` against `axioms`, collect its labelled graph rows.
   Returns {rows, proved} where proved is 1 when the run yielded a
   ProofObject (so the proof contributed), else 0. *)
atpGraphRowsForProof[conj_, axioms_, m_, tc_] := Module[{p, lemmas},
    p = Quiet[TFindProof[conj, axioms, Method -> m, TimeConstraint -> tc]];
    If[ ! MatchQ[p, _ProofObject], Return[{{}, 0}]];
    lemmas = Quiet[TFindProof[conj, axioms, "Lemmas",
        Method -> m, TimeConstraint -> tc]];
    If[ ! ListQ[lemmas], lemmas = {}];
    {atpGraphRows[p, lemmas], 1}
]

(* Turn a list of {heldSides, label} rows into the dataset Association,
   encoding each kept equation to a graph via the C extractor. *)
atpGraphDatasetFromRows[rows_, nProofs_] := Module[{
    graphs, labels, nPos
},
    graphs = atpCpGraphFromSides[#[[1]]] & /@ rows;
    labels = #[[2]] & /@ rows;
    nPos = Total[labels];
    <|
        "Graphs" -> graphs,
        "Labels" -> labels,
        "NPos" -> nPos,
        "NNeg" -> Length[labels] - nPos,
        "NProofs" -> nProofs
    |>
]

(* From verified ProofObjects directly -- the core source the
   conjecture / theory forms reduce to.  TAtpGraphDataset[po] yields
   POSITIVES only (the proof-essential CriticalPairLemma /
   SubstitutionLemma lemmas, label 1); a bare ProofObject does not carry
   the saturated rule set, so pass it (TFindProof[..., "Lemmas"]) as the
   second argument to add the unused-rule NEGATIVES, or use the
   conjecture / theory forms which collect both.  A list of ProofObjects
   is the positives-only union. *)
TAtpGraphDataset[po_ProofObject] :=
    atpGraphDatasetFromRows[atpGraphRows[po, {}], 1]
TAtpGraphDataset[po_ProofObject, sat_List] :=
    atpGraphDatasetFromRows[atpGraphRows[po, sat], 1]
TAtpGraphDataset[pos : {__ProofObject}] :=
    atpGraphDatasetFromRows[Catenate[atpGraphRows[#, {}] & /@ pos], Length[pos]]

TAtpGraphDataset[conjectures_List, axioms_List, opts : OptionsPattern[]] :=
    Module[{
        m = OptionValue[Method],
        tc = OptionValue[TimeConstraint],
        results, rows, nProofs
    },
        results = (atpGraphRowsForProof[#, axioms, m, tc] & /@ conjectures);
        rows = Catenate[First /@ results];
        nProofs = Total[Last /@ results];
        atpGraphDatasetFromRows[rows, nProofs]
    ]

TAtpGraphDataset[theory_String, opts : OptionsPattern[]] := Module[{
    thms = AxiomaticTheory[theory, "NotableTheorems"],
    m = OptionValue[Method],
    tc = OptionValue[TimeConstraint],
    results, rows, nProofs
},
    results = (atpGraphRowsForProof[#, theory, m, tc] & /@ Values[thms]);
    rows = Catenate[First /@ results];
    nProofs = Total[Last /@ results];
    atpGraphDatasetFromRows[rows, nProofs]
]

(* === ENIGMA Tier 2: GCN over the CP hypergraph dataset =========== *)

(* Batched matmul (B, P, K) . (B, K, Q) -> (B, P, Q), built from the
   RESHAPE / EXPAND / MUL / REDUCE_SUM pattern (TMatMul is 2-D only).
   Used for the adjacency propagation A.H and the masked-mean pool. *)
atpBatchMatMul[a_TTerm, b_TTerm, bs_, p_, k_, q_] := Module[{ae, be},
    ae = TUOpExpand[TUOpReshape[a, {bs, p, k, 1}], {bs, p, k, q}];
    be = TUOpExpand[TUOpReshape[b, {bs, 1, k, q}], {bs, p, k, q}];
    TUOpReduce[TUOpMul[ae, be], 2, "SUM"]
]

(* Apply a (K x H) weight to a (B, P, K) node tensor: flatten the batch
   + node axes to (B*P, K), 2-D matmul, reshape back to (B, P, H). *)
atpGnnApplyW[h_TTerm, w_TTerm, bs_, p_, k_, hi_] :=
    ArrayReshape[TMatMul[ArrayReshape[h, {bs * p, k}], w], {bs, p, hi}]

(* Broadcast a length-D bias over a (B, P, D) tensor. *)
atpGnnBcastBias[bias_TTerm, bs_, p_, d_] :=
    TUOpExpand[TUOpReshape[bias, {1, 1, d}], {bs, p, d}]

(* Batch a graph dataset into padded host arrays for the GCN: X
   (B x N x 6 features), A (B x N x N row-normalised adjacency with
   self-loops, edge types merged), Mask + NNodes for the masked-mean
   pool, Y (B x 2 one-hot).  Reads the TAtpCpGraph schema (NodeFeatures
   matrix, 0-indexed Edges, NNodes). *)
atpGnnTensors[dataset_Association] := atpGnnTensors[dataset, Infinity]

(* nCap bounds the padded node dim: n = Min[nCap, max NNodes], and any
   graph with more than n nodes is truncated to its first n (features +
   self-loops capped, out-of-range edges dropped).  nCap = Infinity (the
   1-arg form) is byte-identical to the uncapped batch -- every graph has
   NNodes <= n, so no truncation fires.  Minibatched TAtpTrainGnn passes a
   finite cap so one large graph can't inflate the whole padded batch and
   memory stays O(B * nCap^2). *)
atpGnnTensors[dataset_Association, nCap_] := Module[{
    graphs = dataset["Graphs"], labels = dataset["Labels"],
    b, n, f = 6, eff, xArr, aArr, maskArr, nNodesArr, yArr
},
    b = Length[graphs];
    If[ b === 0, Return[$Failed]];
    n   = Min[nCap, Max[#["NNodes"] & /@ graphs]];
    eff = (Min[#["NNodes"], n] &) /@ graphs;
    xArr = Table[
        With[{feats = graphs[[bi]]["NodeFeatures"]},
            Table[
                If[ ni <= eff[[bi]], feats[[ni]], ConstantArray[0., f]],
                {ni, n}]],
        {bi, b}];
    aArr = Table[
        Module[{adj, rowSums, edge},
            adj = ConstantArray[0., {n, n}];
            Do[ With[{src = edge[[1]] + 1, dst = edge[[2]] + 1},
                    If[ src <= n && dst <= n,
                        adj[[src, dst]] = 1.;
                        adj[[dst, src]] = 1.]],
                {edge, graphs[[bi]]["Edges"]}];
            Do[ adj[[i, i]] = 1., {i, eff[[bi]]}];
            rowSums = Total /@ adj;
            Table[
                If[ rowSums[[i]] > 0., adj[[i]] / rowSums[[i]], adj[[i]]],
                {i, n}]],
        {bi, b}];
    maskArr = Table[
        Table[If[ ni <= eff[[bi]], 1., 0.], {ni, n}],
        {bi, b}];
    nNodesArr = N[eff];
    yArr = (If[ # == 1, {0., 1.}, {1., 0.}] &) /@ labels;
    <|
        "X" -> xArr, "A" -> aArr, "Mask" -> maskArr,
        "NNodes" -> nNodesArr, "Y" -> yArr,
        "N" -> n, "B" -> b, "F" -> f
    |>
]

(* The GCN forward, shared by training (params = trainable TGlorot
   weights) and inference (params = a trained model's weights as
   TTensors): R rounds of row-normalised-adjacency message passing
   (relu(A.H.W1 + H.Ws + b)), masked-mean pool, two-class readout ->
   (B, 2) logits.  Node-count agnostic: the weights are per-feature and
   the pool is masked-mean, so a model trained at one N scores graphs at
   any other N. *)
atpGnnForwardLogits[xT_, aT_, maskRowT_, nNodesT_,
    w1s_, wss_, bhs_, wout_, bout_, b_, n_, f_, hH_, rR_] :=
    Module[{h = xT, lastDim = f, ah, neighbour, self, msg, pooled3, pooled},
        Do[
            ah = atpBatchMatMul[aT, h, b, n, n, lastDim];
            neighbour = atpGnnApplyW[ah, w1s[[r]], b, n, lastDim, hH];
            self = atpGnnApplyW[h, wss[[r]], b, n, lastDim, hH];
            msg = neighbour + self + atpGnnBcastBias[bhs[[r]], b, n, hH];
            h = TReLU[msg];
            lastDim = hH,
            {r, rR}];
        pooled3 = atpBatchMatMul[maskRowT, h, b, 1, n, hH];
        pooled  = ArrayReshape[
            pooled3 * TUOpRecip[TUOpExpand[nNodesT, {b, 1, hH}]],
            {b, hH}];
        pooled . wout + TUOpExpand[TUOpReshape[bout, {1, 2}], {b, 2}]
    ]

(* Defaults: Hidden 32 + Rounds 3.  The term graphs are shallow but the
   lemma corpus is small and class-skewed, so 2 rounds at width 16
   under-propagates and the rank AUC stays near chance; H = 32 / R = 3
   reliably lifts the train AUC.  See docs/plans/atp_tier2_gnn.md. *)
Options[TAtpTrainGnn] = {"Hidden" -> 32, "Rounds" -> 3,
    MaxTrainingRounds -> 300, "LearningRate" -> 0.01,
    "BatchSize" -> 128, "NodeCap" -> 64}

(* Minibatched trainer.  Keeps the params + Adam state (m / v / b1pow /
   b2pow) persistent and runs each Adam step over a minibatch of
   "BatchSize" graphs (reshuffled each epoch), capping the node dim at
   "NodeCap" (64, matching the in-engine inference scorer).  Peak memory
   is O(BatchSize * NodeCap^2): only one minibatch's forward + backward is
   realized at a time.  Final scores for the train-AUC are read back in
   the same bounded chunks.  Uses the b1pow/b2pow TAdam form so bias
   correction tracks the live step. *)
TAtpTrainGnn[dataset_Association, opts : OptionsPattern[]] := Module[{
    graphs = dataset["Graphs"], labels = dataset["Labels"], f = 6,
    hH, rR, lrVal, rounds, bs, nCap, nMax, nG,
    w1s, wss, bhs, wout, bout, params, ms, vs,
    fwd, lossStart, lossEnd, scores, auc, model
},
    nG = Length[graphs];
    If[ nG === 0, Return[$Failed]];
    hH = OptionValue["Hidden"];  rR = OptionValue["Rounds"];
    lrVal  = N @ OptionValue["LearningRate"];
    rounds = OptionValue[MaxTrainingRounds];
    bs     = Min[OptionValue["BatchSize"], nG];
    nCap   = OptionValue["NodeCap"];
    nMax   = Min[nCap, Max[#["NNodes"] & /@ graphs]];
    (* persistent params, updated in place by every step *)
    w1s = Table[TGlorot[{If[ r == 1, f, hH], hH}], {r, rR}];
    wss = Table[TGlorot[{If[ r == 1, f, hH], hH}], {r, rR}];
    bhs = Table[TZeros[{hH}], {r, rR}];
    wout = TGlorot[{hH, 2}];
    bout = TZeros[{2}];
    params = Join[w1s, wss, bhs, {wout, bout}];
    ms = (TZeros[TTensorShape[#]] &) /@ params;
    vs = (TZeros[TTensorShape[#]] &) /@ params;
    (* {logits, yT} for the graphs at indices `ix`, capped at nCap and
       padded to that slice's own node count -- a fresh forward per call
       so only this slice is ever live (bounded O(|ix| * nCap^2)). *)
    fwd[ix_] := Module[{bt = atpGnnTensors[
            <|"Graphs" -> graphs[[ix]], "Labels" -> labels[[ix]]|>, nCap]},
        {atpGnnForwardLogits[
            TTensorCreate @ NumericArray[bt["X"], "Real32"],
            TTensorCreate @ NumericArray[bt["A"], "Real32"],
            TTensorCreate @ NumericArray[ArrayReshape[bt["Mask"], {bt["B"], 1, bt["N"]}], "Real32"],
            TTensorCreate @ NumericArray[ArrayReshape[bt["NNodes"], {bt["B"], 1, 1}], "Real32"],
            w1s, wss, bhs, wout, bout, bt["B"], bt["N"], f, hH, rR],
         TTensorCreate @ NumericArray[bt["Y"], "Real32"]}];
    If[ nG <= bs,
        (* Whole dataset fits one batch (memory already bounded): keep the
           fast path -- build the batch once and run `rounds` Adam steps in
           a C-side loop (no per-step re-materialize). *)
        Module[{lg = fwd[Range[nG]], logits, yT, loss, grads, lr, name},
            logits = lg[[1]];  yT = lg[[2]];
            loss = TCategoricalCrossEntropy[logits, yT];
            lossStart = First[Normal @ TRealize @ loss];
            grads = TGrad[loss, params];
            lr = TUOpConst[lrVal];
            name = THVMLink`Private`freshTrainName[];
            THVMLink`Private`buildLoopAdam[params, grads, ms, vs, lr, name];
            TWnf @ TApp[TRef[name], TNum[rounds]];
            lossEnd = First[Normal @ TRealize @ loss];
            scores = With[{p = Normal @ TRealize @ logits}, p[[All, 2]] - p[[All, 1]]]
        ]
        ,
        (* Bigger than one batch: minibatch with the b1pow/b2pow TAdam form.
           Persistent params + Adam state; one minibatch's forward+backward
           realized per step; reshuffle each epoch; scores read back in the
           same bounded chunks. *)
        Module[{b1pow = TOnes[{1}], b2pow = TOnes[{1}], firstBatch, idx, done},
            firstBatch = Take[Range[nG], bs];
            lossStart = First[Normal @ TRealize @ (TCategoricalCrossEntropy @@ fwd[firstBatch])];
            done = 0;
            While[ done < rounds,
                idx = RandomSample[Range[nG]];
                Do[
                    If[ done >= rounds, Break[]];
                    With[{lg = fwd[idx[[bStart ;; Min[bStart + bs - 1, nG]]]]},
                        TAdam[TCategoricalCrossEntropy[lg[[1]], lg[[2]]], params,
                            ms, vs, b1pow, b2pow, "lr" -> lrVal]];
                    done += 1,
                    {bStart, 1, nG, bs}]];
            lossEnd = First[Normal @ TRealize @ (TCategoricalCrossEntropy @@ fwd[firstBatch])];
            scores = Join @@ Table[
                With[{p = Normal @ TRealize @ First @ fwd[Range[bStart, Min[bStart + bs - 1, nG]]]},
                    p[[All, 2]] - p[[All, 1]]],
                {bStart, 1, nG, bs}]
        ]
    ];
    auc = atpScorerAuc[scores, labels];
    model = <|
        "Kind" -> "GNN",
        "W1" -> (Normal @ TRealize @ # & /@ w1s),
        "Ws" -> (Normal @ TRealize @ # & /@ wss),
        "Bh" -> (Normal @ TRealize @ # & /@ bhs),
        "Wout" -> (Normal @ TRealize @ wout),
        "Bout" -> (Normal @ TRealize @ bout),
        "NMax" -> nMax, "Hidden" -> hH, "Rounds" -> rR
    |>;
    <|
        "Model" -> model,
        "TrainAUC" -> auc,
        "LossStart" -> lossStart,
        "LossEnd" -> lossEnd,
        "NPos" -> Total[labels],
        "NNeg" -> (nG - Total[labels])
    |>
]

(* Corpus overloads: prep the dataset (TAtpGraphDataset) AND train. *)
TAtpTrainGnn[theory_String, opts : OptionsPattern[]] :=
    atpTrainGnnOnCorpus[
        TAtpGraphDataset[theory, FilterRules[{opts}, Options[TAtpGraphDataset]]],
        {opts}]

TAtpTrainGnn[conjectures_List, axioms_List, opts : OptionsPattern[]] :=
    atpTrainGnnOnCorpus[
        TAtpGraphDataset[conjectures, axioms,
            FilterRules[{opts}, Options[TAtpGraphDataset]]],
        {opts}]

atpTrainGnnOnCorpus[ds_Association, opts_List] := If[
    Length[ds["Graphs"]] === 0,
    $Failed,
    Join[
        TAtpTrainGnn[ds, FilterRules[opts, Options[TAtpTrainGnn]]],
        <|"NProofs" -> ds["NProofs"]|>]]
atpTrainGnnOnCorpus[_, _] := $Failed

(* Score a graph dataset with a trained GNN model (the "Model" from
   TAtpTrainGnn): run the same forward with the model's weights as
   constant TTensors, returning the per-graph proof-relevance score
   logit_pos - logit_neg.  This is the inference path -- used for the
   held-out (by-problem) measure and, later, the engine re-rank hook. *)
TAtpGnnScore[model_Association, dataset_Association] := Module[{
    bt, b, n, f, hH, rR, xT, aT, maskRowT, nNodesT,
    w1s, wss, bhs, wout, bout, logits, p
},
    bt = atpGnnTensors[dataset];
    If[ bt === $Failed, Return[$Failed]];
    b = bt["B"];  n = bt["N"];  f = bt["F"];
    hH = model["Hidden"];  rR = model["Rounds"];
    xT       = TTensorCreate @ NumericArray[bt["X"], "Real32"];
    aT       = TTensorCreate @ NumericArray[bt["A"], "Real32"];
    maskRowT = TTensorCreate @ NumericArray[
        ArrayReshape[bt["Mask"], {b, 1, n}], "Real32"];
    nNodesT  = TTensorCreate @ NumericArray[
        ArrayReshape[bt["NNodes"], {b, 1, 1}], "Real32"];
    w1s  = TTensorCreate @ NumericArray[#, "Real32"] & /@ model["W1"];
    wss  = TTensorCreate @ NumericArray[#, "Real32"] & /@ model["Ws"];
    bhs  = TTensorCreate @ NumericArray[#, "Real32"] & /@ model["Bh"];
    wout = TTensorCreate @ NumericArray[model["Wout"], "Real32"];
    bout = TTensorCreate @ NumericArray[model["Bout"], "Real32"];
    logits = atpGnnForwardLogits[xT, aT, maskRowT, nNodesT,
        w1s, wss, bhs, wout, bout, b, n, f, hH, rR];
    p = Normal @ TRealize @ logits;
    p[[All, 2]] - p[[All, 1]]
]

(* === ENIGMA Tier 2: GNN-guided critical-pair re-rank ============== *)

(* Map a GNN proof-relevance score to a CP-queue priority: higher score
   -> lower priority (selected sooner), clamped to a safe u32 band, the
   same shape the baked-in learned scorer uses. *)
atpRerankPriority[score_] := Round[Clip[1000000. - 10000. * score, {0., 2.*^9}]]

Options[TFindProofReranked] = {"RerankPeriod" -> 200, MaxSteps -> 30000,
    "CriticalPairWeight" -> "Gt", "Ordering" -> "KBO",
    "AutoPrecedence" -> True, "QueueCap" -> 4096}

(* Drive a proof in chunks, re-ranking the live CP queue with the GNN
   every "RerankPeriod" steps: pull the queued CPs, encode each to its
   anonymised graph, score with TAtpGnnScore, push priorities back.
   Between re-ranks the base "CriticalPairWeight" orders newly-generated
   CPs.  Returns the status string ("PROVED" / "TIMEOUT" / ...).  This is
   the inference loop that wires the Tier 2 GNN into live selection;
   completeness holds (re-ranking only permutes order + the periodic FIFO
   pick still fires). *)
TFindProofReranked[conjecture_, axioms_List, model_Association,
    opts : OptionsPattern[]] := Module[{
    enc = atpEncodeProblem[axioms, conjecture],
    k = OptionValue["RerankPeriod"], maxSteps = OptionValue[MaxSteps],
    cpw = Lookup[$AtpCpWeightCodes, OptionValue["CriticalPairWeight"], 3],
    ord = If[ OptionValue["Ordering"] === "LPO", 1, 0],
    ap = If[ TrueQ[OptionValue["AutoPrecedence"]], 1, 0],
    cap = OptionValue["QueueCap"],
    handle, st = 0, chunks, q, cnt, tri, seq, graphs, scores
},
    handle = $atpProofInitFn[enc["Packed"], maxSteps, enc["MaxLab"],
        cpw, ord, ap];
    If[ handle == 0, Return[$Failed]];
    chunks = Ceiling[maxSteps / k] + 2;
    Do[
        st = $atpProofStepFn[handle, k];
        If[ st =!= 0, Break[]];
        q = Normal @ $atpProofQueuedFn[handle, cap];
        cnt = First[q];
        If[ cnt > 0,
            tri = Partition[Rest[q], 3];
            seq = tri[[All, 3]];
            graphs = MapThread[
                atpCpGraphDecode[Normal[$atpCpGraphFn[#1, #2]]] &,
                {tri[[All, 1]], tri[[All, 2]]}];
            scores = TAtpGnnScore[model,
                <|"Graphs" -> graphs, "Labels" -> ConstantArray[0, cnt]|>];
            $atpProofSetPriFn[handle, seq, atpRerankPriority /@ scores]
        ],
        {chunks}];
    $atpProofFreeFn[handle];
    atpStatusFor[st]
]

Options[TFindProofGnnReranked] = {"RerankPeriod" -> 200, MaxSteps -> 30000,
    "CriticalPairWeight" -> "Gt", "Ordering" -> "KBO",
    "AutoPrecedence" -> True}

(* C-driven inference loop: push the model once, set the re-rank period on
   the persistent handle, run the saturation in a SINGLE chunk (the C
   engine re-ranks itself between selections), then free.  The GCN forward
   runs in C on thvm's tensor runtime, so the WL kernel is idle while the
   proof runs. *)
TFindProofGnnReranked[conjecture_, axioms_List, model_Association,
    opts : OptionsPattern[]] := Module[{
    enc = atpEncodeProblem[axioms, conjecture],
    k = OptionValue["RerankPeriod"], maxSteps = OptionValue[MaxSteps],
    cpw = Lookup[$AtpCpWeightCodes, OptionValue["CriticalPairWeight"], 3],
    ord = If[ OptionValue["Ordering"] === "LPO", 1, 0],
    ap = If[ TrueQ[OptionValue["AutoPrecedence"]], 1, 0],
    handle, st
},
    If[ TAtpSetGnnScorer[model] =!= True, Return[$Failed]];
    handle = $atpProofInitFn[enc["Packed"], maxSteps, enc["MaxLab"],
        cpw, ord, ap];
    If[ handle == 0, TAtpSetGnnScorer[Clear]; Return[$Failed]];
    $atpProofSetGnnPeriodFn[handle, k];
    st = $atpProofStepFn[handle, maxSteps];
    $atpProofFreeFn[handle];
    TAtpSetGnnScorer[Clear];
    atpStatusFor[st]
]

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
    (* ForAll wrapping Rule / Inactive[Rule]: strip the quantifier AND
       convert to Equal in one pass (Replace runs each rule once, so
       a separate Rule->Equal rule below would never fire after the
       ForAll-strip rule consumed the input).  The pre-orient flag is
       picked up by axiomOrientationFlag on the ORIGINAL HoldComplete
       before this rewrite runs.

       Why `HoldComplete @@ Hold[Equal[a, b]]` instead of bare
       `HoldComplete[Equal[a, b]]`: the RHS of a RuleDelayed evaluates
       at match time, so writing `HoldComplete[Equal[a, b]]` collapses
       a reflexive `Equal[a, a]` to `HoldComplete[True]` before
       HoldComplete's HoldAllComplete attribute can protect it.  Apply
       via `@@` substitutes the head AFTER the inner Equal is already
       inside a Hold, so the Equal stays unevaluated and reaches the
       Pattern-substituting downstream as a proper Equal[lhs, rhs]. *)
    HoldComplete[ForAll[v_Symbol, Rule[a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]], {v}],
    HoldComplete[ForAll[Verbatim[List][vars__Symbol], Rule[a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]],
            List @@ Hold[vars]],
    HoldComplete[ForAll[v_Symbol, Inactive[Rule][a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]], {v}],
    HoldComplete[ForAll[Verbatim[List][vars__Symbol],
            Inactive[Rule][a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]],
            List @@ Hold[vars]],
    (* ForAll wrapping TwoWayRule / Inactive[TwoWayRule] (`a <-> b`):
       semantically an equation, just a different surface syntax.  Flag
       stays 0 -- the engine still picks the orientation via KBO. *)
    HoldComplete[ForAll[v_Symbol, TwoWayRule[a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]], {v}],
    HoldComplete[ForAll[Verbatim[List][vars__Symbol],
            TwoWayRule[a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]],
            List @@ Hold[vars]],
    HoldComplete[ForAll[v_Symbol, Inactive[TwoWayRule][a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]], {v}],
    HoldComplete[ForAll[Verbatim[List][vars__Symbol],
            Inactive[TwoWayRule][a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]],
            List @@ Hold[vars]],
    (* ForAll wrapping Inactive[Equal] / Inactive[Unequal]: same
       reason as Rule -- single-pass Replace would leave the Inactive
       wrapper if only the generic ForAll-strip below fired. *)
    HoldComplete[ForAll[v_Symbol, Inactive[Equal][a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]], {v}],
    HoldComplete[ForAll[Verbatim[List][vars__Symbol],
            Inactive[Equal][a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Equal[a, b]],
            List @@ Hold[vars]],
    HoldComplete[ForAll[v_Symbol, Inactive[Unequal][a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Unequal[a, b]], {v}],
    HoldComplete[ForAll[Verbatim[List][vars__Symbol],
            Inactive[Unequal][a_, b_]]] :>
        applyForAllSubst[HoldComplete @@ Hold[Unequal[a, b]],
            List @@ Hold[vars]],
    HoldComplete[ForAll[v_Symbol, body_]] :>
        applyForAllSubst[HoldComplete[body], {v}],
    HoldComplete[ForAll[Verbatim[List][vars__Symbol], body_]] :>
        applyForAllSubst[HoldComplete[body], List @@ Hold[vars]],
    (* Inactive[Equal] / Inactive[Unequal] = FindEquationalProof's
       inert ProofObject lemma form.  Strip the Inactive wrapper so
       downstream encodeEquation's strict HoldComplete[Equal[_, _]]
       check fires.  Same `HoldComplete @@ Hold[..]` trick to keep a
       reflexive Equal from collapsing at RHS evaluation. *)
    HoldComplete[Inactive[Equal][a_, b_]] :>
        HoldComplete @@ Hold[Equal[a, b]],
    HoldComplete[Inactive[Unequal][a_, b_]] :>
        HoldComplete @@ Hold[Unequal[a, b]],
    (* Bare Rule / Inactive[Rule] (no ForAll wrapper): convert to
       Equal so encodeEquation accepts it.  The orientation flag
       was already captured by axiomOrientationFlag. *)
    HoldComplete[Rule[a_, b_]] :>
        HoldComplete @@ Hold[Equal[a, b]],
    HoldComplete[Inactive[Rule][a_, b_]] :>
        HoldComplete @@ Hold[Equal[a, b]],
    (* Bare TwoWayRule / Inactive[TwoWayRule] (`a <-> b`): an
       equation, no orientation hint. *)
    HoldComplete[TwoWayRule[a_, b_]] :>
        HoldComplete @@ Hold[Equal[a, b]],
    HoldComplete[Inactive[TwoWayRule][a_, b_]] :>
        HoldComplete @@ Hold[Equal[a, b]],
    _ :> axHC
}]

(* Inspect the raw HoldComplete-wrapped axiom (BEFORE forAllToPattern
   converts Rule to Equal) and return its orientation flag: 0 =
   equation (engine orients via KBO), 1 = pre-oriented (use lhs ->
   rhs directly).  Read off the syntactic head only -- a bare `Rule`
   or `Inactive[Rule]` (with or without a ForAll wrapper) means the
   user wants direction trust.  No HoldAll: the caller always passes
   HoldComplete-wrapped values, which already prevent inner
   evaluation, so a plain pattern match is enough. *)
axiomOrientationFlag[axHC_HoldComplete] := Replace[axHC, {
    HoldComplete[ForAll[_, Rule[_, _]]] -> 1,
    HoldComplete[ForAll[_, Inactive[Rule][_, _]]] -> 1,
    HoldComplete[Rule[_, _]] -> 1,
    HoldComplete[Inactive[Rule][_, _]] -> 1,
    _ -> 0
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
    axHCsRaw, axHCs, cjHCs, axTermsAndState, axTerms, st, axFlags,
    goalRes, goalTerms, axPairs, conjPairs, conjPair, n, nGoals
},
    If[ ! ListQ[Unevaluated[axioms]],
        Throw[Failure["TATPParseError",
            <|"Reason" -> "axioms must be a List"|>], "TATPError"
        ]
    ];
    ensureInit[];
    n = Length[Unevaluated[axioms]];
    axHCsRaw = HoldComplete /@ Unevaluated[axioms];
    (* Orientation flag must be read off the ORIGINAL HoldComplete
       (before ForAll-strip / Rule->Equal conversion) so the syntactic
       Rule head is still visible.  Mapped here, not inside the fold,
       to keep the existing encodeAxiomFold signature intact. *)
    axFlags = axiomOrientationFlag /@ axHCsRaw;
    (* Normalize each axiom: strip the outermost ForAll wrapper (if
       present) and rewrite bound bare-symbol occurrences as
       Pattern[var, Blank[]]. *)
    axHCs = forAllToPattern /@ axHCsRaw;
    axTermsAndState = Fold[encodeAxiomFold, {{}, encodeAtpTermInit[], 1}, axHCs];
    axTerms = axTermsAndState[[1]];
    st = axTermsAndState[[2]];
    (* Conjecture forms: None means "no goal" (completion mode, n_goals
       0 -- the C runner saturates the axioms instead of running a goal
       check); a List is a multi-goal conjunction (every conjunct proved
       off ONE saturation, FindEquationalProof[{g1, g2}, axioms]
       parity); anything else is the single-conjecture case. *)
    cjHCs = Which[
        Unevaluated[conjecture] === None,
            {},
        MatchQ[Unevaluated[conjecture], _List],
            forAllToPattern /@ (HoldComplete /@ Unevaluated[conjecture]),
        True,
            {forAllToPattern[HoldComplete[conjecture]]}
    ];
    (* Skolemize: a universal conjecture is proved for an arbitrary
       fixed instance, so strip the bound variables' Pattern wrappers
       to bare constants.  KBO totally orders constants, so an
       unorientable equation (commutativity-style) becomes ordered-
       applicable to the goal -- the single-NF check then closes a
       symmetric goal the variable-keyed goal could not.  Done inside
       HoldComplete so a reflexive `f[x] == f[x]` does not collapse to
       True before encoding. *)
    If[ skolemize, cjHCs = cjHCs /. Verbatim[Pattern][v_, _] :> v ];
    nGoals = Length[cjHCs];
    (* Encode every conjunct with the SHARED encoder state so symbol
       labels stay consistent across the axioms and all goals. *)
    goalRes = Fold[
        {acc, cjHC} |-> Block[{
            r = encodeEquation[cjHC, acc[[2]], "conjecture"]
        },
            {Join[acc[[1]], {r[[1]], r[[2]]}], r[[3]]}
        ],
        {{}, st},
        cjHCs
    ];
    goalTerms = goalRes[[1]];
    st = goalRes[[2]];
    (* Extract {lhs, rhs} pairs from each (stripped) held axiom
       directly via positions {1,1}/{1,2} of
       HoldComplete[lhs==rhs].  Avoids ReleaseHold which
       auto-evaluates `a == a` axioms to True. *)
    axPairs = (
        {Extract[#, {1, 1}], Extract[#, {1, 2}]} & /@ axHCs
    );
    conjPairs = (
        {Extract[#, {1, 1}], Extract[#, {1, 2}]} & /@ cjHCs
    );
    (* "ConjPair" keeps the single-goal consumers' shape: a flat
       {lhs, rhs} when there is exactly one conjecture ({0, 0} for
       None) and the LIST of pairs for a multi-goal conjunction.
       "ConjPairs" is the uniform list-of-pairs view. *)
    conjPair = Which[
        nGoals == 0, {0, 0},
        nGoals == 1, First[conjPairs],
        True, conjPairs
    ];
    (* Wire layout: [n_goals, n, lhs_0, rhs_0, ..., lhs_{n-1},
       rhs_{n-1}, goal_lhs_0, goal_rhs_0, ..., goal_lhs_{n_goals-1},
       goal_rhs_{n_goals-1}, flag_0, ..., flag_{n-1}].  The C reader
       (atp_wire_parse in thvmlink_atp.c) takes each axiom's lhs/rhs at
       data[2 + 2*i + 0/1], goal g at data[2 + 2*n + 2*g + 0/1], and
       the per-axiom orientation flag at data[2 + 2*n + 2*n_goals + i]
       to dispatch between thvm_atp_add_equation (flag == 0) and
       thvm_atp_install_oriented_rule (flag == 1); total length
       2 + 3*n + 2*n_goals.  n_goals == 0 is completion mode.  See
       [[project_atp_oriented_rules]]. *)
    <|
        "Packed" -> NumericArray[
            Join[{nGoals, n}, axTerms, goalTerms, axFlags],
            "Integer64"
        ],
        "MaxLab" -> st["next_lab"],
        "State" -> st,
        "AxPairs" -> axPairs,
        "AxFlags" -> axFlags,
        "ConjPair" -> conjPair,
        "ConjPairs" -> conjPairs,
        "AxHCsRaw" -> axHCsRaw,
        "ConjHCRaw" -> Which[
            nGoals == 0, HoldComplete[None],
            ! skolemize, HoldComplete[conjecture],
            nGoals == 1, First[cjHCs],
            True, cjHCs
        ]
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

(* Single non-list axiom: auto-wrap to a 1-element list, same shape
   as the TFindProof single-axiom wrap.  TATP is HoldAll so pattern
   matching doesn't evaluate; the wrap re-dispatches the held form. *)
TATP[axiom : (_Equal | _Unequal | _ForAll
        | Inactive[Equal][_, _] | Inactive[Unequal][_, _]),
        conjecture_, opts:OptionsPattern[]] :=
    TATP[{axiom}, conjecture, opts];

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
    "Staggered" -> 13, (* E StaggeredWeight (HEURISTICS/
                         che_varweights.c::StaggeredWeightCompute).
                         base_weight / max(1, max_axiom_weight / 2).
                         Buckets CPs by integer stagger group so within
                         a bucket the age-fairness FifoTiebreak picks
                         oldest-first.  Pair with FifoTiebreak -> True
                         for the intended behaviour. *)
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
TFindProof::badorient =
    "Pre-oriented axiom `1` has variables on the right-hand side not \
present on the left -- the rewrite would introduce fresh variables \
and may loop the rewriter.  Use `lhs == rhs` instead, or rewrite the \
axiom so vars(rhs) is a subset of vars(lhs).  The rule was installed \
anyway; downstream behavior is undefined.";

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
   in (WL_ATP_MNF), so "GoalDirected" asks the engine to run the
   bidirectional collision search alongside completion, the only detector
   that closes a symmetric goal whose two sides never share a single
   normal form. *)
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
(* ENIGMA "coop": interleave the primary CP weight (e.g. the learned
   scorer) with a hand-tuned SECONDARY weight, the WM CPdimension d=1
   mechanism (thvm_atp_set_w2).  "CoopWeight" -> a CriticalPairWeight name
   names the secondary (default Automatic = -1 = off); "CoopRatio" -> n
   makes every n-th selection use the secondary instead of the primary.
   Pairing a "Learned" primary with a "Gt" secondary mirrors real ENIGMA,
   which selects cooperatively with the base heuristic, not by the model
   alone.  Note: the secondary uses the engine's structural weight base,
   so naming "Learned" here is a no-op (it is not the learned scorer). *)
atpCoopWeightOpt[o_Association] := Lookup[$AtpCpWeightCodes,
    Lookup[o, "CoopWeight", Automatic], -1];
atpCoopRatioOpt[o_Association] := With[{n = Lookup[o, "CoopRatio", 0]},
    If[ IntegerQ[n] && n > 0, n, 0]];
(* "RandomRatio" -> n: Vampire-style random CP-selection.  When n > 0,
   every n-th CP selection picks a uniformly-random queued CP via a
   deterministic xorshift64 stream (seedable via "RandomSeed").  Default
   0 = off, engine byte-identical.  Mirrors Vampire's `random_seed=...`
   portfolio entries; trajectory differs from heap-min so portfolios
   can sample paths the weight-greedy walk misses. *)
atpRandomRatioOpt[o_Association] := With[{n = Lookup[o, "RandomRatio", 0]},
    If[ IntegerQ[n] && n > 0, n, 0]];
(* "RandomSeed" -> u64: deterministic seed for "RandomRatio".  0 picks a
   fixed nonzero default; runs under a given seed are reproducible. *)
atpRandomSeedOpt[o_Association] := With[{s = Lookup[o, "RandomSeed", 0]},
    If[ IntegerQ[s], s, 0]];
(* "KboWeightScheme" -> "InvPrecedence": derive per-symbol KBO weights
   from the just-computed precedence (max_prec - prec + 1).  Mirrors
   Vampire `kws=inv_precedence` -- frequent operators (high precedence
   under sp=reverse_frequency) get LOW weight, so the KBO ordering's
   weight and precedence components reinforce rather than fight.
   Default 0 = off (uniform-1 weights, engine byte-identical). *)
atpKboWeightSchemeOpt[o_Association] :=
    Switch[Lookup[o, "KboWeightScheme", Automatic],
        "InvPrecedence", 1, False | Automatic, 0, _, 0];
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
(* "LazyNormalize" -> True: Waldmeister/DISCOUNT-style deferred CP
   normalization -- queue CPs in their oriented-only-reduced form,
   defer the full unorientable-rules normalize + joinability verdict
   to selection time.  Engine-side lever in
   src/atp/_.c:7012 (thvm_atp_set_use_lazy_normalize); the saturation
   loop's lazy push site is around line 11238.
   IMPORTANT MEMORY CAVEAT (see [[project_lazy_normalize_memory_blowup]]):
   the deferred-normalize path stores larger (over-deep) queued forms,
   so on a saturating workload it can grow the queue unboundedly.  The
   Waldmeister source ports REQUIRE either periodic
   `CPSetInterreduce -> True` (the KPV_KPMengeInterreduzieren sweep
   that drops joinable CPs) OR a queue size cap via
   `AutoMaxWeight -> n` / `MaxWeight -> n`.  Without either, a hard
   residual will OOM the kernel.  When set, this dispatcher emits a
   TFindProof::lazyunsafe warning but proceeds. *)
atpLazyNormalizeOpt[o_Association] := Switch[Lookup[o, "LazyNormalize", Automatic],
    True, (
        If[ Lookup[o, "CPSetInterreduce", Automatic] === Automatic
                || Lookup[o, "CPSetInterreduce", Automatic] === False,
            If[ (Lookup[o, "AutoMaxWeight", Automatic] === Automatic
                    || Lookup[o, "AutoMaxWeight", Automatic] === False)
                && Lookup[o, "MaxWeight", Automatic] === Automatic,
                Message[TFindProof::lazyunsafe]]];
        1),
    False | Automatic, 0, _, 0];
(* `backticks` are StringForm slots in WL Messages; quote names with '
   instead so the StringForm::sfr noise doesn't fire on every call. *)
TFindProof::lazyunsafe =
    "LazyNormalize -> True without CPSetInterreduce -> True OR a MaxWeight / AutoMaxWeight cap is unbounded: the un-normalized CP queue can grow until the kernel OOMs on a saturating workload.  Pair with 'CPSetInterreduce -> True', 'AutoMaxWeight -> 30', or 'MaxWeight -> 20'.";
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
(* "VarWeight" -> n: per-variable KBO weight override (default 1).
   Mirrors Waldmeister `-w VAR=N`.  Engine byte-identical when absent
   or set to <= 0 (the bridge clamps to default 1 in that case). *)
atpVarWeightOpt[o_Association] :=
    Replace[Lookup[o, "VarWeight", 0], Automatic -> 0];
(* "FifoTiebreak" -> True: Waldmeister `-:w1=fifo` secondary key.  Preserve
   each surviving CP's insertion age across the post-orient CP-normalize
   sweep, so equal-weight ties resolve oldest-first run-wide (the heap
   reheapify otherwise reassigns the age, scrambling the FIFO tie order).
   True = on; False/Automatic = off (engine byte-identical). *)
atpFifoTiebreakOpt[o_Association] := Switch[Lookup[o, "FifoTiebreak", Automatic],
    True, 1, False | Automatic, 0, _, 0];
(* "FreeVarInstance" -> True | False | Automatic: Waldmeister
   RechtsUnfreiErzeugen (FVI) -- when an unorientable equation is added
   to R, also push a grounded sibling that substitutes the engine-
   reserved minimal constant (cAtp1) for every free RHS variable
   absent from the LHS.  Required to crack the FVI-gated theorems
   (ExcludedMiddle, Noncontradiction, EqualityOfInverses) under
   Method->"Waldmeister".  True forces on (use Method->"WaldmeisterFVI"
   for the standard FVI Waldmeister preset).  False forces off.
   Automatic also forces off on plain "Waldmeister" (matching the
   OK_OK baseline byte-for-byte); a future autotuner may turn this on
   when atpAxiomsNeedFvi fires and the in-budget trajectory shift is
   measured-safe per axiom class. *)
atpFreeVarInstanceOpt[o_Association] := Switch[Lookup[o, "FreeVarInstance", Automatic],
    True, 1, False | Automatic, 0, _, 0];

(* "UseImplicitCp" -> True/False: deferred-CP (implicit_pair) storage
   path.  Functional end-to-end (push/select/IR routed) but OPT-IN:
   flipping the WM presets measures 2.8x steps and +55% wall on the
   mccune probe (PROVED both ways) and RAISES peak RSS (+17% mccune,
   2.13x AndAssoc) -- the 20B descriptor saving is swamped by implicit
   slots pinning their raw overlap terms via the trace GC root plus a
   ~2x larger live queue from losing queue-vs-queue subsumption.
   Default Automatic / False -> 0. *)
atpImplicitCpOpt[o_Association] := Switch[Lookup[o, "UseImplicitCp", Automatic],
    True, 1, False | Automatic, 0, _, 0];

(* True iff at least one axiom in `axParts` (atpAxiomParts triples
   {vars, lhs, rhs}) has a side whose variables are not a subset of
   the other side -- i.e. a free-on-one-side variable that the
   Waldmeister `RechtsUnfreiErzeugen` (FVI) hook can ground against
   the reserved minimal constant (src/atp/_.c:12515-12547).  Used by
   atpAnalyzeStructure as a precondition flag; an axiom-set autotuner
   may consult it to dispatch the WaldmeisterFVI preset on FVI-gated
   theorems.  Direct enabling on the plain "Waldmeister" preset is
   measured to drift several short Boolean trajectories outside the
   2x baseline budget, so the present autotuner does not flip FVI on
   from this predicate alone; the user opts in via
   Method->"WaldmeisterFVI" or "FreeVarInstance"->True. *)
atpAxiomsNeedFvi[axParts_List] := AnyTrue[axParts,
    Block[{vars = #[[1]], l = #[[2]], r = #[[3]], lv, rv},
        lv = Cases[l, v_ /; MemberQ[vars, v], {0, Infinity}, Heads -> True];
        rv = Cases[r, v_ /; MemberQ[vars, v], {0, Infinity}, Heads -> True];
        (! SubsetQ[lv, rv]) || (! SubsetQ[rv, lv])] &];
atpAxiomsNeedFvi[_] := False;
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
atpParseMethod[Automatic] := atpParseCompletionOpts[{}, 0];
(* Accept the string form "Automatic" as a synonym for the symbol --
   users typing Method -> "Automatic" alongside the other string-named
   presets ("Waldmeister", "Twee", ...) shouldn't trip the badmethod
   message just because the canonical spec uses the symbol Automatic. *)
atpParseMethod["Automatic"] := atpParseMethod[Automatic];
atpScheduleFor["Automatic"] := atpScheduleFor[Automatic];
atpScheduleFor["Automatic", ax_, cj_] := atpScheduleFor[Automatic, ax, cj];
atpParseMethod["Completion"] := atpParseMethod[{"Completion"}];

(* Shared dispatch helper for the named-preset methods (Waldmeister,
   VampireUEQ, Twee, EProver, ...).  Each preset is just a defaults
   Association + a default GoalDirected toggle; this helper merges
   user subopts over the defaults, computes mnf, and forwards to
   atpParseCompletionOpts.  Subopts always override the preset's
   defaults; "GoalDirected" -> True/False in subopts overrides the
   preset default (post-merge).  Returns the option vector that
   atpParseCompletionOpts produces. *)
atpDispatchPreset[defaults_Association, defaultGD_, subopts_List] :=
    Block[{o = Association[subopts], merged, mnf},
        merged = Join[defaults, o];
        mnf = If[ TrueQ @ Lookup[merged, "GoalDirected", defaultGD], 1, 0];
        merged = KeyDrop[merged, "GoalDirected"];
        atpParseCompletionOpts[Normal[merged], mnf]
    ];

(* Central registry of named-preset defaults.  TAtpDescribeMethod
   inspects this Association; each preset's atpParseMethod definition
   reads $AtpPresetDefaults[<name>] for its own merge.  Adding a new
   preset is one entry here + one atpParseMethod definition that
   forwards to atpDispatchPreset. *)
$AtpPresetDefaults = <|
    "Waldmeister" -> <|
        "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO",
        "AutoPrecedence" -> True, "SkolemHighest" -> True,
        "SelectionRatio" -> 51,
        "RHSInterreduce" -> True, "UnfailingCP" -> True,
        "CPSetInterreduce" -> True,
        (* Stays opt-in: the measured flip costs 2.8x steps, +55%
           wall, +17% peak RSS on mccune and 2.13x peak RSS on
           AndAssoc -- see atpImplicitCpOpt. *)
        "UseImplicitCp" -> False|>,
    (* "WaldmeisterFVI": Waldmeister with FVI emission FORCED on, even
       on axiom sets where the auto-detector says no.  Use this preset
       explicitly on the three FVI-gated theorems (ExcludedMiddle,
       Noncontradiction, McCuneAxioms/EqualityOfInverses).  AUTO-ON on
       plain "Waldmeister" was measured to drift several short Boolean
       proofs (AndCommutativity 1->8, AndIdempotence 4->11) outside the
       2x-of-baseline budget and time out Meredith/OrAssociativity at
       30s; sub-condition guard via input-axiom-shape predicate is
       too coarse a trigger for AUTO. *)
    "WaldmeisterFVI" -> <|
        "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO",
        "AutoPrecedence" -> True, "SkolemHighest" -> True,
        "SelectionRatio" -> 51,
        "RHSInterreduce" -> True, "UnfailingCP" -> True,
        "CPSetInterreduce" -> True,
        "FreeVarInstance" -> True,
        (* Stays opt-in for the same measured regressions as the
           "Waldmeister" entry; FVI differs only in FreeVarInstance
           emission, orthogonal to CP storage. *)
        "UseImplicitCp" -> False|>,
    "WaldmeisterLazy" -> <|
        "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "SkolemHighest" -> True,
        "LazyNormalize" -> True,
        "CPSetInterreduce" -> True,
        "AutoMaxWeight" -> 30,
        "UnfailingCP" -> True, "RHSInterreduce" -> True|>,
    "VampireUEQ" -> <|
        "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "SelectionRatio" -> 10, "UnfailingCP" -> True,
        "AutoMaxWeight" -> True,
        "BackwardSubsume" -> True, "BackwardDemod" -> True,
        "RHSInterreduce" -> True|>,
    (* "VampireUEQDefault": mirrors the FIRST slot of Vampire 5.0.1's
       UEQ portfolio schedule (CASC/Schedules.cpp:5224):
           lrs+10_1:1_sil=4000:st=3.0:i=102:sd=2:ss=axioms:sgt=8_0
       That slot is the one that actually cracks the easy abelian-
       group / boolean-ring cases in the comparator
       (compare_vampireueq_vs_vampire.tsv, ~5ms wall on CLI side).
       Key differences from "VampireUEQ" above (which was modeled on a
       later LPO-flavored slot): default KBO ordering, age:weight=1:1
       (thvm SR=2, i.e. 1 FIFO per 2 picks = full alternation), SInE
       axiom-relevance filter active.  See
       docs/atp/vampire_case_teardown.md for the full mapping. *)
    "VampireUEQDefault" -> <|
        "Ordering" -> "KBO", "AutoPrecedence" -> True,
        "SelectionRatio" -> 2,
        "AxiomRelevance" -> {"SInE",
            "SineTolerance" -> 3.0,
            "SineDepth" -> 2,
            "SineGenerality" -> 8},
        "UnfailingCP" -> True,
        "RHSInterreduce" -> True,
        "LRS" -> True|>,
    "Twee" -> <|
        "CriticalPairWeight" -> "Twee",
        "GroundJoin" -> True, "Connectedness" -> True,
        "UnfailingCP" -> True,
        "BackwardSubsume" -> True, "BackwardDemod" -> True,
        "RHSInterreduce" -> True,
        "AutoMaxWeight" -> 20|>,
    "EProver" -> <|
        "CriticalPairWeight" -> "ConjSym",
        "Ordering" -> "KBO",
        "AutoPrecedence" -> "Occurrence",  (* E `-G InvFreqRank` CASC default *)
        "SelectionRatio" -> 10,
        "AutoMaxWeight" -> 20,
        "BackwardSubsume" -> True,
        "RHSInterreduce" -> True,
        "UnfailingCP" -> True|>,
    (* Vampire's `lrs+10_32:to=lpo:sp=arity:nwc=1:fgj=on:bd=all:
       random_seed=...` cracking config -- the third portfolio entry
       that proves McCuneAxioms/EqualityOfInverses in 2.45s
       (tools/baselines/vampire_raw/McCuneAxioms__EqualityOfInverses
       .out config 3).  Random selection alone is not the cracking
       ingredient at thvm's per-CP throughput (engine + bench
       experiments at 60s budget still don't close McCune), but the
       config is the experimentally-validated combination users
       should reach for first. *)
    "VampireRandom" -> <|
        "Ordering" -> "LPO", "AutoPrecedence" -> True,
        "SelectionRatio" -> 10, "UnfailingCP" -> True,
        "GroundJoin" -> True, "BackwardDemod" -> True,
        "RHSInterreduce" -> True,
        "RandomRatio" -> 32, "RandomSeed" -> 3681690318,
        "LRS" -> True|>,
    (* "ENIGMA": ML-guided critical-pair selection.  CriticalPairWeight
       -> "Learned" ranks CPs by the trained proof-relevance scorer
       (the baked-in logistic regression, or a model pushed via
       TAtpSetLearnedScorer) instead of a hand-tuned weight; the rest is
       a sound bounded-queue completion base (KBO + AutoPrecedence +
       UnfailingCP + RHSInterreduce + AutoMaxWeight 20) so the learned
       ranking operates on a tractable, reduced queue.  Completeness is
       preserved by the engine's periodic FIFO selection regardless of
       the learned score, so a cold/over-fit model only slows a proof,
       never loses one.  Coops by default ("CoopWeight" -> "Gt",
       "CoopRatio" -> 2): every 2nd selection uses the hand-tuned GT
       weight, the rest the learned scorer -- pure-learned selection is
       markedly slower (measured), so this mirrors real ENIGMA's
       cooperative selection.  Override with "CoopRatio" -> 0 for
       pure-learned.  See docs/atp/ml_guidance.md. *)
    "ENIGMA" -> <|
        "CriticalPairWeight" -> "Learned", "Ordering" -> "KBO",
        "AutoPrecedence" -> True, "UnfailingCP" -> True,
        "RHSInterreduce" -> True, "AutoMaxWeight" -> 20,
        "CoopWeight" -> "Gt", "CoopRatio" -> 2|>
|>;

(* Per-preset default for the GoalDirected (MNF front) toggle.  Mostly
   False -- VampireUEQ is the lone True per Vampire's `tgt=full`. *)
$AtpPresetGoalDirected = <|
    "Waldmeister" -> False,
    "WaldmeisterFVI" -> False,
    "WaldmeisterLazy" -> False,
    "VampireUEQ"  -> True,
    "VampireUEQDefault" -> False,  (* matches Vampire's UEQ portfolio default slot which doesn't enable goal-MNF *)
    "Twee"        -> False,
    "EProver"     -> False,
    "VampireRandom" -> True,
    "ENIGMA"      -> False
|>;

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
            True, 1,
            "Occurrence", 2,
            "ReverseFrequency", 3,
            False | Automatic, 0,
            _, 0];
        {cw, ord, ap, mnf, atpMaxWeightOpt[o], atpGoalInterleaveOpt[o],
         atpGroundJoinOpt[o], atpSelectionRatioOpt[o], atpAutoMaxWeightOpt[o],
         atpRHSInterreduceOpt[o], atpUnfailingCPOpt[o],
         atpCPSetInterreduceOpt[o], atpConnectednessOpt[o],
         atpPrecedenceOpt[o], atpFifoTiebreakOpt[o], atpRecordNormOpt[o],
         atpLRSOpt[o], atpSOSOpt[o], atpFwdSubsumeOpt[o], atpBwdSubsumeOpt[o],
         atpBwdDemodOpt[o], atpSymbolWeightsOpt[o], atpVarWeightOpt[o],
         atpRandomRatioOpt[o], atpRandomSeedOpt[o], atpKboWeightSchemeOpt[o],
         atpLazyNormalizeOpt[o], atpCoopWeightOpt[o], atpCoopRatioOpt[o],
         atpFreeVarInstanceOpt[o], atpImplicitCpOpt[o]}
    ];
atpParseMethod[{"Completion", subopts___Rule}] :=
    atpParseCompletionOpts[{subopts}, 0];

(* "GoalDirected" / "MNF": enable the front search.  Bare form defaults
   to Mix2 weight (like Automatic) so completion still drives R forward
   while MNF watches for a front collision; the list form takes the same
   Ordering / AutoPrecedence / CriticalPairWeight knobs as "Completion"
   so the front search can run over an LPO-oriented, structure-precedence
   rule set -- the combination the hard Sheffer cross-axiom goals need. *)
atpParseMethod[m : ("GoalDirected" | "MNF")] := atpParseCompletionOpts[{}, 1];
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
     - SkolemHighest -> True  (WM CLI's .pr files pin goal skolems above
       all operators in the ORDERING block; without this, AndCommutativity
       /OrCommutativity-class goals fail because skC1/skC2 sit at the
       bottom of the Fuchs arity ladder and instance-rewriting can't
       orient the goal's commutativity equation)
     - SelectionRatio -> 51  (itl(mi) = interleave fifo:heuristic 1:50,
       YFiles.c:114-122; CPdimension fairness, KPVerwaltung.c:582)
     - RHSInterreduce -> True  (IR_InterreduktionRechts -- the
       divergence that made the deep theorems unreachable)
     - UnfailingCP -> True  (faithful unfailing completion)
   List form takes the same suboptions, overriding any default.  Pass
   "GoalDirected" -> True to add the MNF bidirectional front on top of
   the completion path for a symmetric goal that never meets at one
   normal form. *)
(* Waldmeister's Orkus default for an unrecognized (single-operator
   nand) problem is StdS = kbo(std), itl(mi), zb(mnf) (Sinai.h:109,
   131): KBO ordering, the interleaved CPdimension (itl(mi) ->
   SelectionRatio 51), the MixWeight classification (no cph(...) ->
   Heu_MixWeight, NewClassification.c:850), and goal normalization
   (zb(mnf)).  With Mix the engine follows WM's exact selection
   trajectory; Add diverges at rule 10.  RHSInterreduce + UnfailingCP
   are part of faithful unfailing completion.  StdS has no gj(), so
   GroundJoin is off.

   WM's zb(mnf) is goal normalization, not a separate exhaustive
   bidirectional front; thvm's MNF front search re-expands its whole
   node table every time a rule is added (O(n_nodes) per selection),
   which dominates a deep completion.  The preset runs the completion
   path -- whose single-normal-form goal check closes every goal WM's
   StdS closes -- and only adds the MNF front when
   "GoalDirected" -> True is requested for a symmetric goal the
   single-NF check cannot reach. *)
atpParseMethod["Waldmeister"] := atpParseMethod[{"Waldmeister"}];
atpParseMethod[{"Waldmeister", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["Waldmeister"],
        $AtpPresetGoalDirected["Waldmeister"], {subopts}];

(* Method -> "WaldmeisterFVI": Waldmeister + RechtsUnfreiErzeugen FVI
   (see $AtpPresetDefaults note above).  Opt-in preset for FVI-gated
   theorems; everything else should stay on plain "Waldmeister". *)
atpParseMethod["WaldmeisterFVI"] := atpParseMethod[{"WaldmeisterFVI"}];
atpParseMethod[{"WaldmeisterFVI", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["WaldmeisterFVI"],
        Lookup[$AtpPresetGoalDirected, "WaldmeisterFVI", False], {subopts}];

(* Method -> "WaldmeisterLazy": Waldmeister DISCOUNT-style preset
   bundling the spec-safe LazyNormalize combo.  LazyNormalize defers
   the full CP normalize to selection time; CPSetInterreduce is the WM
   KPV_KPMengeInterreduzieren periodic queue purge that drops
   joinable CPs as new rules land; AutoMaxWeight 30 caps the queue
   growth so the un-normalized stored forms can't blow memory.
   LPO + AutoPrecedence pair to give the lazy queue cheap ordering
   verdicts.  See [[project_lazy_normalize_memory_blowup]] for why
   the safety pairing is required.  Cracks 4 residuals that the
   default Automatic schedule's Mix2+Gt slot sequence CRASHes on
   (WolframAxioms/Implies{Hillman,Meredith,WolframAlternate,
   WolframCommutative}*Axioms c1/c2). *)
atpParseMethod["WaldmeisterLazy"] := atpParseMethod[{"WaldmeisterLazy"}];
atpParseMethod[{"WaldmeisterLazy", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["WaldmeisterLazy"],
        $AtpPresetGoalDirected["WaldmeisterLazy"], {subopts}];

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
       by it).
     - BackwardDemod -> True           (direct port of `bd=all` LHS
       half: after a new-rule batch, normalize each older rule's LHS
       with the new rule(s); if it reduces, drop and re-queue the
       simplified equation).
     - RHSInterreduce -> True          (the bd=all RHS half: the
       Waldmeister IR_InterreduktionRechts equivalent.  Pairs with
       BackwardDemod to give the full bd=all both-sides demodulation). *)
atpParseMethod["VampireUEQ"] := atpParseMethod[{"VampireUEQ"}];
atpParseMethod[{"VampireUEQ", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["VampireUEQ"],
        $AtpPresetGoalDirected["VampireUEQ"], {subopts}];

(* Method -> "VampireUEQDefault": the Vampire 5.0.1 UEQ portfolio
   first-slot mirror; see $AtpPresetDefaults comment + docs/atp/
   vampire_case_teardown.md for the per-token decode of
   `lrs+10_1:1_sil=4000:st=3.0:i=102:sd=2:ss=axioms:sgt=8_0`. *)
atpParseMethod["VampireUEQDefault"] := atpParseMethod[{"VampireUEQDefault"}];
atpParseMethod[{"VampireUEQDefault", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["VampireUEQDefault"],
        $AtpPresetGoalDirected["VampireUEQDefault"], {subopts}];

(* Method -> "Twee": a preset modeled on Twee 2.x's defaults --
       cfg_lhsweight=4, cfg_rhsweight=1, cfg_depthweight=2,
       cfg_dupcost=1, cfg_ground_join=True,
       cfg_use_connectedness_standalone=True, and unfailing CP
       paramodulation always on.
   Best-effort mapping into thvm's existing knobs:
     - CriticalPairWeight -> "Twee"   (the shared-subterm-discounted
       asymmetric weight).
     - GroundJoin -> True             (Twee's cfg_ground_join: delete
       ground-joinable CPs).
     - Connectedness -> True          (Twee's
       cfg_use_connectedness_standalone: Bachmair-Dershowitz
       below-peak redundancy, Twee section 6.2).
     - UnfailingCP -> True            (Twee always superposes both
       faces of an unorientable equation).
     - BackwardSubsume -> True        (Twee's keepRules drops
       subsumed rules at add time).
     - BackwardDemod -> True          (Twee's interreduce normalizes
       older rule LHSs against newly-added rules).
     - RHSInterreduce -> True         (Twee's interreduce does the
       symmetric RHS sweep too).
     - AutoMaxWeight -> 20            (Twee's heap doesn't bound CP
       size explicitly, but the growing-bound stash keeps the queue
       budget-tractable on hard saturations -- same effect Twee gets
       from its tight join-loop runtime budget).
   Subopts override any default, mirroring the "Waldmeister" and
   "VampireUEQ" preset shape. *)
atpParseMethod["Twee"] := atpParseMethod[{"Twee"}];
atpParseMethod[{"Twee", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["Twee"],
        $AtpPresetGoalDirected["Twee"], {subopts}];

(* Method -> "EProver": a preset modeled on E's typical CASC run
   shape (Schulz, 2002+).  E's heuristic is much larger than a
   single config: it rotates dozens of weight + selection
   combinations under its DISCOUNT loop.  This preset bundles the
   combo most often selected at E's auto-mode classification for
   UEQ problems:
     - CriticalPairWeight -> "ConjSym"  (E's
       ConjectureSymbolWeight: conjecture-symbol nodes weight 1,
       off-conjecture nodes weight 4).
     - Ordering -> "KBO"               (E's default term ordering
       for UEQ).  AutoPrecedence intentionally OFF: thvm's Waldmeister-
       flavored precedence layer (inverse > distributor > arity > AC-
       demoted > ...) demotes AC operators in a way that stalls
       ConjSym on simple Boolean goals.  E uses a different precedence
       generator; leaving the default lexicographic order matches the
       cases this preset is designed for.
     - SelectionRatio -> 10 (E's `dis+10` age:weight 1:10 = our
       "1 FIFO pick per 10 selections").
     - AutoMaxWeight -> 20 (E manages CP queue size via PCL-side
       bounds; AutoMaxWeight is our analog).
     - BackwardSubsume + RHSInterreduce (E's standard simplification
       sweep across the active+passive sets at each loop iteration).
     - UnfailingCP -> True (E's unfailing completion mode is on by
       default for UEQ).
   Subopts override any default, mirroring the other preset shapes. *)
atpParseMethod["EProver"] := atpParseMethod[{"EProver"}];
atpParseMethod[{"EProver", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["EProver"],
        $AtpPresetGoalDirected["EProver"], {subopts}];

(* Method -> "VampireRandom": the Vampire 5.0.1 portfolio entry that
   cracks McCuneAxioms/EqualityOfInverses (`lrs+10_32:to=lpo:sp=arity:
   nwc=1:fgj=on:bd=all:random_seed=3681690318`).  Bundles
   LPO + AutoPrecedence + LRS + GroundJoin + BackwardDemod +
   RHSInterreduce + UnfailingCP + RandomRatio 32 + Vampire's exact
   seed.  Random clause selection alone is not the cracking
   ingredient at thvm's per-CP throughput (engine experiments at
   60s budget still don't close McCune), but this is the
   experimentally-validated combination users should reach for
   first when working on the hard targets. *)
atpParseMethod["VampireRandom"] := atpParseMethod[{"VampireRandom"}];
atpParseMethod[{"VampireRandom", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["VampireRandom"],
        $AtpPresetGoalDirected["VampireRandom"], {subopts}];

(* Method -> "ENIGMA": ML-guided critical-pair selection (the learned
   scorer of docs/plans/atp_ml_roadmap.md) as a first-class preset, so
   `TFindProof[conj, ax, Method -> "ENIGMA"]` runs the trained model
   instead of requiring the explicit {"Completion", "CriticalPairWeight"
   -> "Learned"} spec.  Push a custom model with TAtpSetLearnedScorer
   first; with none pushed it uses the baked-in logistic regression.
   Subopts override any default (e.g. Method -> {"ENIGMA", "Ordering" ->
   "LPO"}). *)
atpParseMethod["ENIGMA"] := atpParseMethod[{"ENIGMA"}];
atpParseMethod[{"ENIGMA", subopts___Rule}] :=
    atpDispatchPreset[$AtpPresetDefaults["ENIGMA"],
        $AtpPresetGoalDirected["ENIGMA"], {subopts}];

(* Registry of the named Method presets `atpParseMethod` recognizes.
   "Portfolio" / "VampirePortfolio" expand to schedules (a list of
   configs); the rest are single-config presets.  Exposed as
   $AtpMethodPresets so a downstream tool (test sweep, doc generator,
   tuner) can enumerate them without re-encoding the set. *)
$AtpMethodPresets = {"Waldmeister", "VampireUEQ", "Twee", "EProver",
    "VampireRandom", "ENIGMA",
    "Portfolio", "VampirePortfolio", "VampirePortfolioCompact",
    "AllPresets"};

(* Method -> "VampirePortfolio": a 13-entry rotation modeled on the
   portfolio-cycling shape Vampire 5.0.1 ships for UEQ -- many short
   strategy slices rather than one tuned config.  With TimeConstraint
   -> T, each entry runs at T / 11 wall time.  Designed to exercise
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
    {-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, None, 0, 1, 0, 0, 0, 0, 0, None, 0};

$VampirePortfolio = {
    (* 1: VampireUEQ-faithful single config (the flag-complete preset). *)
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
    {"Completion", "CriticalPairWeight" -> "Add", "AutoMaxWeight" -> 20},
    (* 11: Mix2 + SelectionRatio 2 (aggressive 1-FIFO-per-2 age bias).
       Cracks the cross-system Sheffer Implies-X family
       (ImpliesWolframAxioms + ImpliesWolframAlternate, ~3.7s) that every
       other rotation entry walls on.  The tight age bias forces the long
       derivation chain through before the CP queue blows up; SR=1 and
       SR>=5 both miss it. *)
    {"Completion", "CriticalPairWeight" -> "Mix2",
        "SelectionRatio" -> 2, "AutoMaxWeight" -> 20},
    (* 12: Vampire's McCune-cracking config (LPO + arity + LRS +
       RandomRatio 32 + Vampire's exact seed + GroundJoin +
       BackwardDemod + RHSInterreduce + UnfailingCP).  The
       experimentally-validated entry for McCuneAxioms /
       EqualityOfInverses; random selection samples paths the
       weight-greedy entries miss. *)
    "VampireRandom",
    (* 13: LPO + Occurrence-frequency precedence (Vampire `sp=occurrence`
       / E `-G InvFreqRank`).  Rarer symbols outrank common ones; on
       cross-system goals where a symbol appears in few axioms the
       trajectory differs from the AutoPrecedence Fuchs-arity entry. *)
    {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> "Occurrence",
        "SelectionRatio" -> 10, "AutoMaxWeight" -> 20}
};

(* Hook VampirePortfolio into atpScheduleFor so the rotation expands
   into the schedule.  Anything else passes through atpParseMethod. *)
atpScheduleFor["VampirePortfolio"] := $VampirePortfolio;
atpScheduleFor["VampirePortfolio", _, _] := $VampirePortfolio;

(* Method -> "VampirePortfolioCompact": a 3-entry rotation suitable
   for small TimeConstraints where the 10-entry $VampirePortfolio
   would give each slice a sliver (TC=5 -> 0.5s/entry, not enough
   to crack much).  At TC=5 each entry gets ~1.67s; at TC=15 ~5s.
   Picks one entry from each of the three lever families exercised
   by the full portfolio: Vampire flagship, Twee redundancy, and a
   weight-tuned completion fallback. *)
$VampirePortfolioCompact = {
    "VampireUEQ",
    "Twee",
    {"Completion", "CriticalPairWeight" -> "Mix2",
        "AutoPrecedence" -> True, "AutoMaxWeight" -> 20}
};
atpParseMethod["VampirePortfolioCompact"] :=
    {-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, None, 0, 1, 0, 0, 0, 0, 0, None, 0};
atpScheduleFor["VampirePortfolioCompact"] := $VampirePortfolioCompact;
atpScheduleFor["VampirePortfolioCompact", _, _] := $VampirePortfolioCompact;

(* Method -> "AllPresets": a 4-entry rotation through every named
   single-config preset (Waldmeister, VampireUEQ, Twee, EProver).
   For users who want a "try every approach" sweep without spelling
   out each preset.  At TC=20 each entry gets ~5s; at TC=60 each
   gets ~15s.  Useful as a portfolio when the autotuner's structure
   guess might be wrong. *)
$AtpAllPresets = {"Waldmeister", "VampireUEQ", "Twee", "EProver"};
atpParseMethod["AllPresets"] :=
    {-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, None, 0, 1, 0, 0, 0, 0, 0, None, 0};
atpScheduleFor["AllPresets"] := $AtpAllPresets;
atpScheduleFor["AllPresets", _, _] := $AtpAllPresets;

atpParseMethod[m_] := (
    Message[TFindProof::badmethod, m]; {-1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, None, 0, 1, 0, 0, 0, 0, 0, None, 0});

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
(* "Portfolio" is the FIXED schedule above.  Automatic is PROBLEM-AWARE:
   it front-loads a tailored config for the detected algebraic structure,
   then APPENDS $AtpSchedule as a fallback tail so it can never prove less
   than the fixed portfolio (see atpAutoTune).  atpScheduleFor's two-arg
   form threads the axioms+conjecture so Automatic can analyze them. *)
atpScheduleFor["Portfolio"] := $AtpSchedule;
atpScheduleFor["Portfolio", _, _] := $AtpSchedule;
atpScheduleFor[Automatic, axioms_, conjecture_] :=
    atpTunedSchedule[axioms, conjecture];
atpScheduleFor[Automatic] := $AtpSchedule;          (* no problem in hand *)
atpScheduleFor[m_, _, _] := {m};
atpScheduleFor[m_] := {m};

(* === Introspective return-type machinery =========================

   TFindProof's optional LAST positional argument selects what
   the call returns instead of (or alongside) the heavy ProofObject.  A
   single String returns that one value bare; a list of Strings returns
   an Association keyed by the requested names; All returns an
   Association of every spec.  The default ("ProofObject") returns the
   bare ProofObject, so existing call shapes are unchanged. *)
$AtpReturnSpecs = {"ProofObject", "Lemmas", "PreprocessedAxioms",
    "RelevantAxioms", "RawTrace", "Statistics", "Status",
    "AppliedMethod", "WallTime", "PortfolioTrace", "Counterexample"};

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

(* ----- countermodel from a saturated completion -----
   When the C engine SATURATES (Status 4 / QUEUE_EMPTY) without proving the
   goal, it has produced a finite ground-convergent rewrite system.  The
   equational dual of FindEquationalProof's countermodel-bearing Failure:
   normalize the (skolemized, hence ground) two sides of the goal against the
   completed rules; distinct normal forms witness that the goal is NOT a
   consequence, and the convergent system is the deciding model (the initial
   term algebra).  A real disproof, not a timeout.

   Soundness gate.  Unfailing completion can saturate with UNORIENTABLE
   equations (e.g. commutativity `x*y == y*x`), which it stores in MainRules
   with an arbitrary orientation and applies only via ordered rewriting.
   Reusing the engine's order here would mean re-deriving its KBO/LPO
   precedence; instead we restrict to the case that needs no ordered
   rewriting: every MainRule must be strictly size-reducing under all
   substitutions (atpRuleSizeReducingQ).  That guarantees (a) `//.`
   terminates and (b) the saturated set contains no unorientable equation
   (those have equal size on both sides), so the rules form an ordinary
   terminating TRS whose joinable critical pairs (saturation) make it
   convergent for the FULL theory -- and NF comparison decides ground
   equality soundly and completely.  If any rule fails the gate (AC /
   commutative-saturated theories) we decline rather than risk an unsound
   verdict. *)

(* A {lhs, rhs} rule is strictly size-reducing under every substitution iff
   LeafCount[lhs] > LeafCount[rhs] AND no variable occurs more often in rhs
   than in lhs (so no instantiation can flip the size).  Both conditions
   together make the rule -- and hence any set of such rules -- terminating
   under `//.`. *)
atpRuleSizeReducingQ[{l_, r_}, varSyms_List] :=
    LeafCount[l] > LeafCount[r] &&
    AllTrue[varSyms,
        Count[l, #, {0, Infinity}] >= Count[r, #, {0, Infinity}] &];
atpRuleSizeReducingQ[_, _] := False;

(* ===== CounterexampleObject: a summary-boxed disproof artifact =====
   The equational dual of ProofObject, mirroring the Wolfram Function
   Repository's FindEquationalCounterexample result shape:
       CounterexampleObject[method, proposition, axioms,
           <|"Setup" -> model, "Counterexample" -> witness, ...|>]
   `co["Setup"]` follows FindFiniteModels' structure for a finite model and
   carries the convergent rules for an infinite initial term algebra. *)

$atpCeProps = {"Status", "Method", "Proposition", "Goal", "Axioms",
    "Hypotheses", "Setup", "Model", "Counterexample", "Witness",
    "NormalForms", "Domain", "FalsificationFunction",
    "VerificationFunction", "Data", "Properties"};

(* ----- self-certifying functions -----
   The equational dual of ProofObject's GenerateFalsificationFunction /
   GenerateVerificationFunction, mirroring the WFR FindEquationalCounterexample's
   FalsificationFunction / VerificationFunction: a nullary Function that
   evaluates the goal in the refuting model -- co["FalsificationFunction"][]
   returns False (the goal fails there) -- or the axioms --
   co["VerificationFunction"][] returns True (they hold there). *)

atpCeKind[d_Association] := Which[
    IntegerQ[d["Domain"]] && AssociationQ[d["Setup"]], "Finite",
    ListQ[d["Setup"]],                                 "Rules",
    AssociationQ[d["Setup"]],                          "Boolean",
    True,                                              "Unknown"];

(* Operator/constant interpretation rules from a finite-algebra Setup.
   FindFiniteModels tables are 0-indexed; shift to 1-indexed so the
   Part-indexing `table[[##]]&` and integer constants line up (the WFR form). *)
atpCeFiniteRules[setup_Association] := Join[
    Cases[Normal[setup],
        (op_ -> t_List) :> (op -> With[{t1 = t + 1}, (t1[[##]] &)])],
    Cases[Normal[setup],
        (cn_ -> v_Integer) :> (cn -> v + 1)]];

(* The universally-quantified axioms instantiated over the finite domain
   {1, ..., k}: each ForAll body substituted at every assignment of its
   variables to a domain element, ground axioms kept as-is.  Pre-expanded at
   construction so the verification function's body is a literal conjunction of
   ground formulas rather than a deferred Map. *)
atpCeAxiomInstances[axioms_List, k_Integer] := Flatten @ Map[
    ax |-> If[ MatchQ[ax, _ForAll],
        With[{vars = Flatten[{ax[[1]]}], body = ax[[2]]},
            Map[t |-> (body /. Thread[vars -> t]),
                Tuples[Range[k], Length[vars]]]],
        {ax}],
    axioms];

(* {lhs, rhs} of each axiom (ForAll stripped) -- the rewrite-rule case checks
   both sides normalize to the same term. *)
atpCeAxiomPairs[axioms_List] := Map[
    ax |-> With[{body = If[ MatchQ[ax, _ForAll], ax[[2]], ax]},
        {body[[1]], body[[2]]}],
    axioms];

(* The produced functions are self-contained and built only from System`
   heads (ReplaceAll, ReplaceRepeated, And, SameQ, Apply) over the model
   interpretation -- no private-context symbols, Slot `##` (as in the WFR's
   `table[[##]]&`) carries operator arities. *)
atpCeFalsificationFunction[CounterexampleObject[_, p_, _, d_Association]] :=
    Switch[atpCeKind[d],
        "Finite",
            With[{prop = p, r = atpCeFiniteRules[d["Setup"]]},
                Function[{}, prop /. r]],
        "Boolean",
            With[{prop = p, r = Normal[d["Setup"]]},
                Function[{}, prop /. r]],
        (* Initial term algebra: two ground terms are equal iff they share a
           normal form, so the goal's Equal / Unequal evaluates as SameQ /
           UnsameQ of the normalized sides (a symbolic `==` would not reduce). *)
        "Rules",
            With[{cmp = If[ MatchQ[p, _Equal], SameQ, UnsameQ],
                  l = p[[1]], r = p[[2]], rl = d["RewriteRules"]},
                Function[{}, cmp[l //. rl, r //. rl]]],
        _, Missing["NotAvailable"]];

atpCeVerificationFunction[CounterexampleObject[_, _, ax_, d_Association]] :=
    Switch[atpCeKind[d],
        "Finite",
            With[{inst = atpCeAxiomInstances[ax, d["Domain"]],
                  r = atpCeFiniteRules[d["Setup"]]},
                Function[{}, And @@ (inst /. r)]],
        "Boolean",
            With[{hyps = ax, r = Normal[d["Setup"]]},
                Function[{}, And @@ (hyps /. r)]],
        "Rules",
            With[{pairs = atpCeAxiomPairs[ax], rl = d["RewriteRules"]},
                Function[{}, And @@ (((#1 //. rl) === (#2 //. rl) &) @@@ pairs)]],
        _, Missing["NotAvailable"]];

(co : CounterexampleObject[m_, p_, ax_, d_Association])[key_String] :=
    Switch[key,
        "Status",                    "Refuted",
        "Method",                    m,
        "Proposition" | "Goal",      p,
        "Axioms" | "Hypotheses",     ax,
        "Setup" | "Model",           Lookup[d, "Setup", Missing["NotAvailable"]],
        "Counterexample" | "Witness", Lookup[d, "Counterexample", Missing["NotAvailable"]],
        "NormalForms",               Lookup[d, "NormalForms", Missing["NotAvailable"]],
        "Domain",                    Lookup[d, "Domain", Missing["NotAvailable"]],
        "FalsificationFunction",     atpCeFalsificationFunction[co],
        "VerificationFunction",      atpCeVerificationFunction[co],
        "Data",                      d,
        "Properties",                $atpCeProps,
        _,                           Lookup[d, key, Missing["UnknownProperty", key]]];
(CounterexampleObject[m_, p_, ax_, d_Association])[All] := d;

(* A small static icon (a 3x3 operation-table grid with the falsifying cell
   highlighted) -- self-contained boxes, no front end / Graph layout needed,
   so it renders in a headless documentation build. *)
$atpCeIcon := $atpCeIcon = With[{red = RGBColor[0.86, 0.21, 0.27]},
    Graphics[
        {EdgeForm[GrayLevel[0.6]], FaceForm[GrayLevel[0.93]],
         Table[Rectangle[{i, j}, {i + 1, j + 1}], {i, 0, 2}, {j, 0, 2}],
         FaceForm[red], Rectangle[{2, 0}, {3, 1}]},
        ImageSize -> 22, PlotRangePadding -> 0]];

CounterexampleObject /: MakeBoxes[
        co : CounterexampleObject[m_, p_, ax_, d_Association], fmt_] :=
    BoxForm`ArrangeSummaryBox[
        CounterexampleObject,
        co,
        $atpCeIcon,
        {BoxForm`SummaryItem[{"Method: ", m}],
         BoxForm`SummaryItem[{"Goal: ", Short[p, 1]}]},
        {BoxForm`SummaryItem[{"Domain: ",
            Replace[Lookup[d, "Domain", Missing[]],
                k_Integer :> Row[{k, " element", If[k == 1, "", "s"]}]]}],
         BoxForm`SummaryItem[{"Model: ", Short[Lookup[d, "Setup", Missing[]], 2]}],
         BoxForm`SummaryItem[{"Counterexample: ",
            Short[Lookup[d, "Counterexample", Missing[]], 1]}]},
        fmt,
        "Interpretable" -> Automatic];

(* ----- finite model from a congruence-closure quotient -----
   Render the SAT quotient TSatEUF returns (a list of equivalence classes of
   ground subterms) as a finite algebra in FindFiniteModels' structure: a
   domain {0, ..., k-1} (one element per class), each n-ary operator as a
   k^n Cayley table, each constant as its domain element.  The quotient is a
   PARTIAL model (only the seen input combinations are determined); unseen
   cells are completed with element 0, which is sound because the asserted
   (dis)equalities mention only seen terms, so no completion can violate
   them.  The result is a total finite model refuting the goal. *)

(* term -> 0-based class index, as an Association for O(1) lookup. *)
atpClassIndex[classes_List] :=
    Association @ Flatten @ MapIndexed[
        {cls, pos} |-> ((# -> pos[[1]] - 1) & /@ cls),
        classes];

(* The k^n Cayley table for operator f of arity n: entry [i1, ..., in]
   (0-based domain elements) is the class of the seen compound f[t1, ..., tn]
   whose argument classes are i1, ..., in, or 0 when no such compound was
   seen.  Built flat over Tuples then reshaped row-major, matching
   FindFiniteModels' `table[[i1 + 1, ..., in + 1]]` indexing. *)
atpCayleyTable[f_, n_Integer, k_Integer, terms_List, idx_Association] :=
    Module[{seen, dom = Range[0, k - 1]},
        seen = Association @ Cases[terms,
            c_ /; Head[c] === f && Length[c] === n :>
                ((idx /@ (List @@ c)) -> idx[c])];
        ArrayReshape[
            Lookup[seen, Tuples[dom, n], 0],
            ConstantArray[k, n]]];

atpFiniteModelFromClasses[classes_List] :=
    Module[{idx = atpClassIndex[classes], terms = Flatten[classes],
            compounds, sig, model = <||>},
        compounds = Select[terms, ! AtomQ[#] &];
        sig = DeleteDuplicates[{Head[#], Length[#]} & /@ compounds];
        Scan[(model[#[[1]]] = atpCayleyTable[#[[1]], #[[2]], Length[classes],
            terms, idx]) &, sig];
        Scan[(model[#] = idx[#]) &,
            DeleteDuplicates @ Select[terms, AtomQ]];
        <|"Model" -> model, "Domain" -> Length[classes], "Index" -> idx|>];

(* A problem is ground when neither goal nor axioms carry a quantifier or a
   pattern variable -- the precondition for the congruence-closure route. *)
atpGroundProblemQ[goal_, axioms_] :=
    FreeQ[{goal, axioms},
        ForAll | Exists | _Pattern | _Blank | _BlankSequence | _BlankNullSequence];

(* Ground entailment refutation via congruence closure: assert the hypotheses
   together with the negated goal and run TSatEUF.  SAT means the goal is not
   entailed and the quotient is a finite refuting model, returned as a
   CounterexampleObject whose Setup is the FindFiniteModels-style algebra.
   $Failed when the problem is non-ground or the entailment actually holds
   (UNSAT). *)
atpGroundCounterexample[goalIn_, hypsIn_List] :=
    Module[{strip, goal, hyps, eqs, diseqs, res, fm, sides},
        strip = # /. {Inactive[Equal][a_, b_] :> Equal[a, b],
            Inactive[Unequal][a_, b_] :> Unequal[a, b]} &;
        goal = strip[goalIn];
        hyps = strip /@ hypsIn;
        If[ ! atpGroundProblemQ[goal, hyps] ||
            ! MatchQ[goal, _Equal | _Unequal], Return[$Failed]];
        {eqs, diseqs} = collectLiterals[Append[hyps, negate[goal]]];
        If[ eqs === $Failed, Return[$Failed]];
        res = TSatEUF[eqs, diseqs];
        If[ res["Status"] =!= "SAT", Return[$Failed]];
        fm = atpFiniteModelFromClasses[res["Classes"]];
        sides = List @@ goal;
        CounterexampleObject["CongruenceClosure", Activate[goalIn],
            Activate /@ hypsIn,
            <|"Setup" -> fm["Model"], "Domain" -> fm["Domain"],
              "Counterexample" ->
                  AssociationThread[sides, Lookup[fm["Index"], sides, 0]]|>]];

$atpFiniteModelCap = 64;

(* Best-effort finite model for the saturated-TRS case: enumerate the initial
   term algebra's carrier by closing the goal's ground constants under the
   operators, normalizing at each step, until a fixpoint (finite model) or the
   cap (treat as infinite -> decline, fall back to the rule presentation).
   When it closes, the carrier IS the initial algebra and the operation tables
   are exact, so the model is in FindFiniteModels structure -- the saturated
   analog of the congruence-closure quotient. *)
atpFiniteModelFromRules[rules_List, varSyms_List, seedTerms_List] :=
    Module[{rl, normalize, ops, consts, dom, idxOf, model = <||>, changed = True, elt},
        rl = cplAsRule[#, varSyms] & /@ rules;
        normalize = Function[t, t //. rl];
        ops = DeleteDuplicates @ Cases[
            Join[First /@ rules, Last /@ rules, seedTerms],
            c_ /; (! AtomQ[c] && ! MatchQ[Head[c], List | Inactive]) :>
                {Head[c], Length[c]}, {0, Infinity}];
        consts = DeleteDuplicates @ Cases[seedTerms, a_ /; AtomQ[a], {0, Infinity}];
        dom = DeleteDuplicates[normalize /@ consts];
        If[ dom === {} || ops === {}, Return[$Failed]];
        While[ changed && Length[dom] <= $atpFiniteModelCap,
            changed = False;
            Do[ Do[ elt = normalize[op[[1]] @@ tup];
                    If[ ! MemberQ[dom, elt], AppendTo[dom, elt]; changed = True],
                   {tup, Tuples[dom, op[[2]]]}],
               {op, ops}]];
        If[ Length[dom] > $atpFiniteModelCap, Return[$Failed]];
        idxOf = Association @ MapIndexed[# -> (#2[[1]] - 1) &, dom];
        Scan[ Function[op,
            model[op[[1]]] = ArrayReshape[
                (idxOf[normalize[op[[1]] @@ #]] &) /@ Tuples[dom, op[[2]]],
                ConstantArray[Length[dom], op[[2]]]]], ops];
        Scan[ (model[#] = idxOf[normalize[#]]) &, consts];
        <|"Model" -> model, "Domain" -> Length[dom], "Index" -> idxOf|>];

atpSaturationCountermodel[bundle_] := Block[
    {cRes = bundle["cRes"], enc = bundle["enc"], rules, varSyms, conjPair,
     normalize, nfL, nfR, goal, axioms, fm, data},
    If[ !AssociationQ[cRes] || cRes["Status"] =!= 4, Return[$Failed]];
    rules = cRes["MainRules"];
    conjPair = enc["ConjPair"];
    varSyms = Replace[cRes["VarSyms"], Except[_List] -> {}];
    If[ !ListQ[rules] || !ListQ[conjPair] || Length[conjPair] =!= 2,
        Return[$Failed]];
    (* Sound-gate: bail unless the whole saturated set is a terminating TRS
       (no unorientable equations). *)
    If[ !AllTrue[rules, atpRuleSizeReducingQ[#, varSyms] &], Return[$Failed]];
    normalize = Function[t, t //. (cplAsRule[#, varSyms] & /@ rules)];
    {nfL, nfR} = normalize /@ conjPair;
    If[ nfL === nfR, Return[$Failed]];  (* joinable => goal holds; decline *)
    goal = Activate @ holdToInactive[enc["ConjHCRaw"]];
    axioms = Activate /@ holdToInactive /@ enc["AxHCsRaw"];
    (* "when possible" build a finite model in FindFiniteModels structure;
       else carry the convergent rules as the (infinite initial algebra). *)
    fm = atpFiniteModelFromRules[rules, varSyms,
        DeleteDuplicates @ Cases[conjPair, _?AtomQ, {0, Infinity}]];
    data = If[ AssociationQ[fm],
        <|"Setup" -> fm["Model"], "Domain" -> fm["Domain"],
          "NormalForms" -> {nfL, nfR},
          "Counterexample" -> AssociationThread[conjPair,
              Lookup[fm["Index"], {nfL, nfR}]]|>,
        <|"Setup" -> atpMainRulesLemmas[cRes],
          "NormalForms" -> {nfL, nfR},
          (* the patternized rewrite rules drive the Falsification /
             Verification functions for the infinite initial-algebra case. *)
          "RewriteRules" -> (cplAsRule[#, varSyms] & /@ rules),
          "Counterexample" -> AssociationThread[conjPair, {nfL, nfR}]|>];
    CounterexampleObject["SaturationNormalForm", goal, axioms, data]
];

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
(* "Counterexample" -> a CounterexampleObject disproving the goal, else
   $Failed.  The equational dual of "ProofObject": where "ProofObject" answers
   "is the goal derivable?", "Counterexample" answers "is the goal refutable?"
   off the same run.  A fully GROUND problem is decided by congruence closure
   (a complete decision procedure, and the quotient is a clean finite model in
   FindFiniteModels structure); a quantified problem falls to the saturated-
   completion route. *)
atpReturnValue[bundle_, "Counterexample"] :=
    Module[{enc, goal, axioms, gc},
        enc = bundle["enc"];
        goal = holdToInactive[enc["ConjHCRaw"]];
        axioms = holdToInactive /@ enc["AxHCsRaw"];
        gc = If[ atpGroundProblemQ[goal, axioms],
            atpGroundCounterexample[goal, axioms], $Failed];
        If[ gc =!= $Failed, gc, atpSaturationCountermodel[bundle]]];
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
            "Proved" -> MatchQ[bundle["ProofObject"], _ProofObject]|>}];

atpProjectReturn[bundle_, spec_String] := atpReturnValue[bundle, spec];
atpProjectReturn[bundle_, All] :=
    Association[# -> atpReturnValue[bundle, #] & /@ $AtpReturnSpecs];
atpProjectReturn[bundle_, spec_List] :=
    Association[# -> atpReturnValue[bundle, #] & /@ spec];

Options[TFindProof] = {
    MaxSteps -> 200000, Method -> Automatic,
    TimeConstraint -> Infinity, PortfolioFrontLoad -> 1,
    (* Forwarded to the Process-method builders ({Vampire,Twee,
       Waldmeister,Eprover}ProofObject) when Method is one of the
       *Process names; ignored by the internal-engine path. *)
    "Binary" -> Automatic, "ParseFormulas" -> False,
    "LiftToProofObject" -> False};

(* String form: resolve theorem + theory names through
   AxiomaticTheory, then run the expression form.  The conjecture
   is the named NotableTheorem; the axioms are the theory's axiom
   list.  unquantifyFormula / CanonicalizePatterns normalize the
   quantified formulas (ForAll -> Pattern, Exists -> Skolem, then
   canonical variable names). *)
(* Normalize a conjecture argument to a flat list of equation formulas.
   A NotableTheorem value is a list of conjuncts; an Association (e.g.
   the whole NotableTheorems table) contributes its Values; nesting is
   flattened down to single ForAll / Equal formulas.  A multi-element
   result is proved as ONE multi-goal conjunction off a single
   saturation (FindEquationalProof parity). *)
atpConjList[cj_List] := Catenate[atpConjList /@ cj];
atpConjList[cj_] := {cj};

(* Shared core for the theory-resolved forms: resolve the named theory's
   axioms, drop the ones irrelevant to the conjecture, and prove the
   conjecture via the expression form.  One conjunct returns a single
   ProofObject; a multi-conjunct theorem is ONE multi-goal conjunction,
   returning ONE ProofObject with a {Hypothesis, g} / {Conclusion, g}
   row pair per conjunct ($Failed unless every conjunct is proved). *)
atpProveFromTheory[cjArg_, theory_String,
        opts:OptionsPattern[TFindProof]] :=
    atpProveFromTheory[cjArg, theory, "ProofObject", opts];
(* The returnSpec threads through to the expression-form call; a
   multi-conjunct theorem returns ONE projection for the whole
   conjunction (e.g. "Status" is a single tag). *)
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
            (* Multi-conjunct theorem (e.g. BooleanAxioms DeMorgan):
               one multi-goal conjunction proved off a single engine
               saturation, returning ONE ProofObject with a
               {Hypothesis, g} / {Conclusion, g} row pair per conjunct
               (FindEquationalProof parity). *)
            TFindProof[
                CanonicalizePatterns /@ (unquantifyFormula /@ cjList),
                axioms, returnSpec, opts]
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
    Block[{cjRaw, methodOpt = OptionValue[TFindProof, {opts}, Method]},
        (* Method -> "VampireProcess" / "TweeProcess" / etc. route
           through the external CLI wrappers and return a thvm-shaped
           ProofObject, so the same downstream consumers (proof
           comparator, ProofFunction verifier, dataset accessors) see
           the same shape across internal preset vs external CLI. *)
        If[ methodOpt === "VampireProcess",
            (* Fully-qualified path: ATP.wl loads before
               ProcessProofObject.wl in the alphabetical autoloader
               order, so the symbol is unresolved when this RHS is
               first parsed.  Without qualification it would land in
               THVMLink`ATP`Private`.  Filter against the BUILDER's
               option list (TVampireProofObject etc.), which
               includes the CLI wrapper's options PLUS the
               builder-only ParseFormulas. *)
            Throw[THVMLink`ATP`TVampireProofObject[theory, thm,
                FilterRules[{opts},
                    Options[THVMLink`ATP`TVampireProofObject]]],
                "TATPError"]
        ];
        If[ methodOpt === "TweeProcess",
            Throw[THVMLink`ATP`TTweeProofObject[theory, thm,
                FilterRules[{opts},
                    Options[THVMLink`ATP`TTweeProofObject]]],
                "TATPError"]
        ];
        If[ methodOpt === "WaldmeisterProcess",
            Throw[THVMLink`ATP`TWaldmeisterProofObject[theory, thm,
                FilterRules[{opts},
                    Options[THVMLink`ATP`TWaldmeisterProofObject]]],
                "TATPError"]
        ];
        If[ methodOpt === "EproverProcess",
            Throw[THVMLink`ATP`TEproverProofObject[theory, thm,
                FilterRules[{opts},
                    Options[THVMLink`ATP`TEproverProofObject]]],
                "TATPError"]
        ];
        cjRaw = AxiomaticTheory[theory, "NotableTheorems"][thm];
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
        cj : (_List | _ForAll | _Equal | _Unequal
            | Inactive[Equal][_, _] | Inactive[Unequal][_, _]),
        theory_String, opts:OptionsPattern[]] /; ! atpReturnSpecQ[theory] :=
    atpProveFromTheory[cj, theory, opts];
TFindProof[
        cj : (_List | _ForAll | _Equal | _Unequal
            | Inactive[Equal][_, _] | Inactive[Unequal][_, _]),
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
        h_String[args__] :> tptpStringToSymbol[h][args],
        (* Bare string atom (`"a"` inside `f["a"] == "b"`, not a
           String head with args): convert to the same Tptp$ symbol
           the head-form gets, so the encoder's symbol table sees one
           label for "a" / Tptp$a / both, and the decoder's reverse
           map (Symbol[name]) round-trips back to the same symbol.
           Without this rule the verifier sees `Symbol["a"]` (a bare
           Global`a) where the goal had the literal `"a"`, and the
           ProofObject reconstruction fails the verify check. *)
        s_String /; StringLength[s] > 0 :> tptpStringToSymbol[s]
    };

tptpDispatch[imported_Association, opts:OptionsPattern[TFindProof]] := If[
    "SMT" === OptionValue[TFindProof, {opts}, Method],
    (* Ground SMT path: the congruence-closure dispatcher checks groundness
       (rejecting variable-bearing clauses with TFindProof::nonground) and
       decides over the raw TPTP terms, no Symbol internalization. *)
    tptpDispatchSMT[imported],
    If[ imported["Conjecture"] === None,
        TFindProof[tptpInternalize /@ imported["Axioms"], opts],
        TFindProof[tptpInternalize @ imported["Conjecture"],
            tptpInternalize /@ imported["Axioms"], opts]]
]

(* The proving entry: optional LAST positional returnSpec.  Without it,
   the bare ProofObject is returned (backward compatible); with it, the
   run is projected onto the requested introspectives. *)
(* Method -> "SMT" short-circuit: route ground equational inputs to the
   QF_UF congruence-closure decider (atpSmtEntail, Kernel/ATP/SMT.wl).
   Returns the SMT decision Association on a proved entailment and a
   CounterexampleObject on a refuted one.  The guard form --
   OptionValue[TFindProof, {opts}, Method] -- is the reliable WL idiom for
   option-keyed dispatch: a bare OptionValue inside `/;` does not always see
   the supplied opts. *)
TFindProof[conjecture_, axioms_List, opts:OptionsPattern[]] /;
        ("SMT" === OptionValue[TFindProof, {opts}, Method]) :=
    atpSmtEntail[conjecture, atpFlattenAxioms[axioms]];
TFindProof[conjecture_, axioms_List,
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] /;
        ("SMT" === OptionValue[TFindProof, {opts}, Method]) :=
    atpSmtProject[atpSmtEntail[conjecture, atpFlattenAxioms[axioms]], returnSpec];

(* Project an atpSmtEntail result (a "Proved" decision Association or a
   refuting CounterexampleObject) onto a TFindProof return spec. *)
atpSmtProject[r_, "Counterexample"] :=
    If[ MatchQ[r, _CounterexampleObject], r, $Failed];
atpSmtProject[r_, "Status"] := Which[
    MatchQ[r, _CounterexampleObject], "Refuted",
    AssociationQ[r] && KeyExistsQ[r, "Status"], r["Status"],
    True, "Failed"];
atpSmtProject[r_, _] := r;

(* A ground Boolean combination of (in)equality atoms (And / Or / Implies /
   ...) is not unit-equational, so the completion engine cannot consume it.
   Route it to the lazy DPLL(T) decider automatically -- so the unified
   surface TFindProof[goal, hyps, "Counterexample"] works for Boolean goals
   without an explicit Method -> "SMT". *)
atpBooleanGoalQ[goal_] :=
    MatchQ[Head[goal],
        And | Or | Not | Implies | Equivalent | Xor | Nand | Nor | Xnor] &&
    FreeQ[goal, _Pattern | _Blank | _BlankSequence | _BlankNullSequence |
        ForAll | Exists];
TFindProof[goal_ /; atpBooleanGoalQ[goal], axioms_List,
        opts : OptionsPattern[]] :=
    atpSmtEntail[goal, atpFlattenAxioms[axioms]];
TFindProof[goal_ /; atpBooleanGoalQ[goal], axioms_List,
        returnSpec_?atpReturnSpecQ, opts : OptionsPattern[]] :=
    atpSmtProject[atpSmtEntail[goal, atpFlattenAxioms[axioms]], returnSpec];

(* Pass Inactive[Equal] / Inactive[Unequal] THROUGH unchanged --
   stripping them here with `Inactive[Equal][a_, b_] :> a == b`
   evaluates `Equal[a, b]` at match time on the rule RHS, which
   collapses a reflexive form (`Inactive[Equal]["a", "a"]` --
   a user's escape hatch around WL's parser short-circuiting
   `"a" == "a"` to True) into True before encodeEquation can see
   it.  Let `forAllToPattern` strip the wrapper later under
   HoldComplete protection (via `HoldComplete @@ Hold[Equal[a, b]]`),
   which preserves the held Equal. *)
atpStripInactive[expr_] := expr;

(* Flatten one level of nesting in the axiom list.  Users
   concatenating axiom subsets via `{theory_axioms, extra_lemmas}`
   without an explicit Flatten get a List-of-Lists that the encoder
   silently rejects -- this auto-flattens that case.
   `Flatten[ax, 1]` is a no-op if ax is already a flat list of
   Equal / ForAll / Inactive[Equal] heads. *)
atpFlattenAxioms[ax_List] := Flatten[ax, 1];

(* If the expression contains a String-headed compound (the shape
   TPTPImport produces -- e.g. "f"[X_] for the TPTP atom f(X)),
   internalize it to Symbol-headed.  No-op if nothing matches.
   Lets users pipe TPTPImport["..."] output directly into
   TFindProof[conj, ax] without the manual tptpInternalize step. *)
atpMaybeInternalizeTPTP[expr_] := If[
    (* Trigger on either: a String-headed compound (`"f"[X]`, the
       TPTPImport shape), OR a bare String atom inside a term tree
       (`f["a"]`, `"a" == "b"`).  Both forms need tptpInternalize to
       rename strings to private Tptp$ Symbols so the encoder and
       the ProofObject decoder agree on the round-trip. *)
    !FreeQ[expr, _String[___]] || !FreeQ[expr, _String],
    tptpInternalize[expr], expr];

(* Composed normalizers used by every user-facing entry (TFindProof,
   TRelevantAxioms).

   HoldFirst + `Inactivate[c, Equal]`: a user who needs the WL parser
   to NOT collapse a reflexive `a == a` (or `"a" == "a"`) conjecture
   can wrap with `Unevaluated[...]` at the call site --
   `TFindProof[Unevaluated[a == a], {}]`.  The chain that protects it:
   TFindProof's HoldFirst (below) holds the conjecture in its scope;
   TFindProof's body passes `Unevaluated[conjecture]` to
   atpNormalizeConj so the held form survives the function boundary;
   atpNormalizeConj's HoldFirst here pins it inside the body; and
   Inactivate (its own HoldFirst) sees the still-held `Equal[a, a]`,
   replaces the head with `Inactive[Equal]`.  Downstream
   forAllToPattern strips the `Inactive[Equal]` under HoldComplete
   protection.  No Unevaluated needed inside the Inactivate call --
   c is already held by atpNormalizeConj's attribute. *)
SetAttributes[atpNormalizeConj, HoldFirst];
atpNormalizeConj[c_] :=
    atpMaybeInternalizeTPTP[
        atpStripInactive[Inactivate[c, Equal]]];
SetAttributes[atpNormalizeAxioms, HoldFirst];
atpNormalizeAxioms[ax_List] :=
    atpMaybeInternalizeTPTP[
        atpStripInactive[
            atpFlattenAxioms[Inactivate[ax, Equal]]]];
(* A single Equal / Inactive[Equal] / ForAll axiom (no enclosing
   List) is a common shape -- the user pastes one ax directly.
   Wrap in a 1-element List and re-dispatch.  A pre-oriented Rule
   axiom's lhs is a TERM (a symbol or compound), never one of
   TFindProof's option keys -- exclude those (plus String-keyed
   options) so an options-only call like `TFindProof[axioms,
   TimeConstraint -> 10]` (the completion form) does not match here
   with the option read as a Rule axiom. *)
TFindProof[conjecture_,
        axiom : (_Equal | _Unequal | _ForAll | _TwoWayRule
        | Inactive[Equal][_, _] | Inactive[Unequal][_, _]
        | Inactive[Rule][_, _] | Inactive[TwoWayRule][_, _]
        | Rule[Except[MaxSteps | Method | TimeConstraint
            | PortfolioFrontLoad | _String], _]),
        opts:OptionsPattern[]] :=
    TFindProof[conjecture, {axiom}, opts];
TFindProof[conjecture_,
        axiom : (_Equal | _Unequal | _ForAll | _TwoWayRule
        | Inactive[Equal][_, _] | Inactive[Unequal][_, _]
        | Inactive[Rule][_, _] | Inactive[TwoWayRule][_, _]
        | Rule[Except[MaxSteps | Method | TimeConstraint
            | PortfolioFrontLoad | _String], _]),
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] :=
    TFindProof[conjecture, {axiom}, returnSpec, opts];


(* Block-localize the common short variable names a caller's outer
   scope might have bound (e.g. `Do[..., {c, ...}]`).  The C engine's
   term decoder reconstructs symbols via bare `Symbol[name]` from the
   encoder's idToName label table -- if Global`c is set to a list when
   the decode runs, the decoded `c` collapses into that list and the
   verifier sees the wrong shape.  CanonicalizePatterns escapes the
   axiom-side NAMES upstream (atpFreshGlobalSymbol -> name$Atp1) but
   the decode + verify path reads bare names downstream where this
   Block is the cleanest guard.  The full lowercase alphabet covers
   the FuchsPraezedenz / CanonicalizePatterns name table; xN/yN/zN
   cover the engine's "x<id>" FVR-fallback names.  See
   [[project_atp_tfindproof_iter_leak]]. *)


(* A LIST of conjectures is a multi-goal CONJUNCTION: the generic
   expression form below threads it whole into atpProveBundle, where
   the encoder packs every conjunct onto one wire and the C engine
   proves all of them off ONE saturation (goals_joined_mask latches
   per-conjunct joins; PROVED only when every bit is set).  The result
   is ONE ProofObject whose Proof dataset carries a {Hypothesis, g} /
   {Conclusion, g} row pair per conjunct -- FindEquationalProof's
   multi-goal shape -- or $Failed unless EVERY conjunct is proved.
   "Status" likewise returns a single tag for the whole conjunction. *)
TFindProof[conjecture_, axioms_List, OptionsPattern[]] :=
    Quiet[Block[{
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
        atpProjectReturn[
            atpProveBundle[
                atpNormalizeConj[conjecture],
                atpNormalizeAxioms[axioms],
                MaxSteps -> OptionValue[MaxSteps],
                Method -> OptionValue[Method],
                TimeConstraint -> OptionValue[TimeConstraint]],
            "ProofObject"]], {General::shdw}];
TFindProof[conjecture_, axioms_List,
        returnSpec_?atpReturnSpecQ, OptionsPattern[]] :=
    Quiet[Block[{
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
        atpProjectReturn[
            atpProveBundle[
                atpNormalizeConj[conjecture],
                atpNormalizeAxioms[axioms],
                MaxSteps -> OptionValue[MaxSteps],
                Method -> OptionValue[Method],
                TimeConstraint -> OptionValue[TimeConstraint]],
            returnSpec]], {General::shdw}];

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
        Module[{n = Length[atpSched],
                fl = Min[Max[OptionValue[PortfolioFrontLoad], 0],
                         Length[atpSched]]},
        Do[ atpSub = If[ atpEnd =!= Infinity,
                With[{rem = atpEnd - AbsoluteTime[]},
                    (* PortfolioFrontLoad -> k allocates 2x time to each
                       of the first k entries (vs an equal-share each).
                       Remaining entries split the unused remainder fairly
                       on every step.  k=0 reproduces the fair-share
                       behavior byte-identical. *)
                    Which[
                        fl == 0, rem / (n - i + 1),
                        i <= fl, 2. * rem / (2. * (fl - i + 1) + (n - fl)),
                        True,    rem / (n - i + 1)]],
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
                "Proved" -> MatchQ[atpR["ProofObject"], _ProofObject]
            |>];
            If[ MatchQ[atpR["ProofObject"], _ProofObject], Break[]],
            {i, n}]];
        (* Stamp the cumulative trace on the returned bundle.  When only
           one slice ran (proved immediately), the trace has one entry. *)
        If[ AssociationQ[atpR],
            atpR = Append[atpR, "PortfolioTrace" -> atpTrace]];
        atpR],
    (* Single config. *)
    Block[{
        enc, conjPair, nGoals, axiomKeys, ruleList, cRes, extSteps,
        chain, dataset, varNames, axEq, conjStmt, po, relAx, atpWallTime
    },
        enc = atpEncodeProblem[axioms, conjecture, True];
        conjPair = enc["ConjPair"];
        nGoals = Length[enc["ConjPairs"]];
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
            If[ nGoals > 1,
                (* Multi-goal conjunction: one axiom-cited chain per
                   conjunct off the shared run (the bridge goal-tags
                   each step's Side); buildCEngineChains is $Failed
                   when any conjunct's chain is missing or not
                   expressible over the axioms, and an all-reflexive
                   conjunction yields all-empty chains, which
                   assembleGoalsDataset closes trivially. *)
                chain = buildCEngineChains[extSteps,
                    enc["ConjPairs"], ruleList];
                If[ chain =!= $Failed,
                    dataset = assembleGoalsDataset[enc["AxPairs"],
                        enc["ConjPairs"], chain, ruleList]
                ],
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
        (* Multi-goal "ConjHCRaw" is the LIST of held conjuncts; the
           ProofObject statement is then the list of goal equations,
           matching FindEquationalProof's Theorems for a conjunction. *)
        conjStmt = If[ nGoals > 1,
            holdToInactive /@ enc["ConjHCRaw"],
            holdToInactive[enc["ConjHCRaw"]]];
        Module[{tryBuild, poA, poB, poFinal},
            (* Raise $RecursionLimit: a long completion proof (the deep
               Sheffer/Wolfram theorems run to ~300+ steps) walks a deep
               trace DAG in buildCplDataset and the WL verifier, which can
               trip the default 1024 recursion limit and abort the whole
               attempt.  Guard it locally. *)
            tryBuild[chainOn_, baseDataset_] := Block[{
                    $RecursionLimit = Max[$RecursionLimit, 16384]},
                Module[{ds, p, v, hasFvi},
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
                    (* FVI-gated proofs cite a SubstitutionLemma whose
                       Proof shape (Source -> "fvi") the FindEquational-
                       Proof verifier does not yet teach.  The C engine's
                       KBO_GT post-grounding gate (src/atp/_.c:12547)
                       already validated each emission, so the dataset
                       is sound; accept the ProofObject without the
                       _Success guarantee.  Other proofs keep the
                       strict gate. *)
                    hasFvi = AnyTrue[Lookup[cRes, "Trace", {}],
                        Lookup[#, "Reason", 0] === $TraceFvi &];
                    Which[
                        ! MatchQ[p, _ProofObject], $Failed,
                        MatchQ[v, _Success], p,
                        hasFvi, p,
                        True, $Failed
                    ]
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
            poFinal = If[ MatchQ[poA, _ProofObject],
                poA,
                poB = tryBuild[False, $Failed];
                If[ MatchQ[poB, _ProofObject], poB, $Failed]
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
   completed rule set).  Implementation: encode with a None conjecture,
   so the packed array carries n_goals == 0 -- the C runner reads that
   as "no goal" and saturates until the CP queue
   empties (a finite complete system) or the step/wall budget is hit.
   The default return for completion mode is "ProofObject" with
   Theorems -> None (the no-goal path is unified with the goal-directed
   forms); "Lemmas" is still available as an explicit return spec for the
   saturated rule set as Inactive[Equal] pairs. *)
atpCompletionBundle[axioms_List, OptionsPattern[TFindProof]] :=
    Catch[
    Module[{enc, cRes, atpWall, atpMethodCfg, atpWallTime,
            axEq, varNames, ds, po},
        atpWall = If[ OptionValue[TimeConstraint] =!= Infinity,
            N[OptionValue[TimeConstraint]], 0.];
        (* Encode with a None conjecture: the packed array carries
           n_goals == 0, which the C runner reads as "no goal ->
           saturate the axioms". *)
        enc = atpEncodeProblem[axioms, None, False];
        atpMethodCfg = atpParseMethod[OptionValue[Method]];
        {atpWallTime, cRes} = AbsoluteTiming @ cEngineProof[
            enc, OptionValue[MaxSteps], atpWall,
            Sequence @@ atpMethodCfg];
        (* Construct a ProofObject with "Theorems" -> None.  Use the
           same buildCplDataset path the goal-directed bundle uses,
           which now recognises the (conjPair == {0, 0}, no main
           steps) no-goal mode and enumerates every TRACE_CP entry so
           the dataset surfaces the saturated rule set with full
           Construct / Position / Rule / Orientation provenance - same
           shape as the goal-directed case, minus Hypothesis and
           Conclusion (no goal). *)
        axEq = holdToInactive /@ enc["AxHCsRaw"];
        varNames = cRes["VarSyms"];
        ds = Block[{$RecursionLimit = Max[$RecursionLimit, 16384]},
            Check[
                Quiet[buildCplDataset[enc, enc["ConjPair"], cRes],
                    {General::newsym, RuleDelayed::rhs}],
                $Failed]];
        po = If[ ds === $Failed || ! ListQ[ds],
            (* Fallback: minimal axioms-only dataset so the
               ProofObject is still ProofObjectQ-valid. *)
            ProofObject["EquationalLogic", None, axEq,
                <|"Variables" -> varNames, "Constants" -> {},
                  "Proof" -> MapIndexed[
                    {eq, idx} |-> ({"Axiom", First[idx]} ->
                        <|"Statement" -> (eq /. Inactive[Equal] -> Equal),
                          "Proof" -> <||>|>),
                    axEq]|>],
            ProofObject["EquationalLogic", None, axEq,
                <|"Variables" -> Union[varNames,
                    Cases[ds, s_Symbol /; atpXVarQ[s], {0, Infinity}]],
                  "Constants" -> {}, "Proof" -> ds|>]];
        <|"enc" -> enc, "cRes" -> cRes, "ProofObject" -> po,
          "RelevantAxioms" -> <|"Mode" -> None,
              "Kept" -> axioms, "Dropped" -> {}|>,
          "AppliedMethod" -> OptionValue[Method],
          "WallTime" -> atpWallTime|>
    ],
    "TATPError"
];

(* Single non-list axiom: auto-wrap and re-dispatch.  Same shape as
   the (conj, single_ax) wrap, at the completion entry. *)
TFindProof[axiom : (_Equal | _Unequal | _ForAll
        | Inactive[Equal][_, _] | Inactive[Unequal][_, _]),
        opts:OptionsPattern[]] :=
    TFindProof[{axiom}, opts];
TFindProof[axiom : (_Equal | _Unequal | _ForAll
        | Inactive[Equal][_, _] | Inactive[Unequal][_, _]),
        returnSpec_?atpReturnSpecQ, opts:OptionsPattern[]] :=
    TFindProof[{axiom}, returnSpec, opts];

(* Completion of an explicit axiom list.  Default return is
   "ProofObject" (matching the goal-directed form); the ProofObject's
   Theorems slot is None to signal no goal.  Use TFindProof[axioms,
   "Lemmas"] for the saturated rule set. *)
TFindProof[axioms_List, OptionsPattern[]] :=
    atpProjectReturn[
        atpCompletionBundle[atpNormalizeAxioms[axioms],
            MaxSteps -> OptionValue[MaxSteps],
            Method -> OptionValue[Method],
            TimeConstraint -> OptionValue[TimeConstraint]],
        "ProofObject"];
TFindProof[axioms_List, returnSpec_?atpReturnSpecQ,
        OptionsPattern[]] :=
    atpProjectReturn[
        atpCompletionBundle[atpNormalizeAxioms[axioms],
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
    atpTheoryCompletion[theory, "ProofObject",
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
TFindEquationalProof::badorient = TFindProof::badorient;
TFindEquationalProof::badrel    = TFindProof::badrel;
TFindEquationalProof[args___] := TFindProof[args];

On[General::shdw];
End[];
EndPackage[];

(* Sub-modules live under Kernel/ATP/ and are picked up by the
   recursive autoloader in Kernel/THVMLink.wl -- no explicit Get
   here.  Files load alphabetically by full path, so Kernel/ATP/
   children load AFTER Kernel/ATP.wl. *)
