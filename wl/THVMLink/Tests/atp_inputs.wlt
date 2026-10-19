(* atp_inputs.wlt -- the theorem-proving functions refuse bad input with their own message and
   $Failed (or False), report what they did, and keep public names public. *)

VerificationTest[
    (* TATP holds its arguments, so the path goes in evaluated. *)
    With[{p = FileNameJoin[{$TemporaryDirectory, "thvm_no_such_problem.pr"}]}, TATP[File[p]]],
    $Failed,
    {TATP::nofile},
    TestID -> "ATP/inputs/tatp-missing-file"
]

VerificationTest[
    TFindFiniteModels[f[a, b] == a, 2, Method -> "Foo"],
    $Failed,
    {TFindFiniteModels::method},
    TestID -> "ATP/inputs/finite-models-unknown-method"
]

VerificationTest[
    TAtpSetLearnedScorer[<|"Kind" -> "Linear", "W" -> {1, 2}|>],
    False,
    {TAtpSetLearnedScorer::model},
    TestID -> "ATP/inputs/learned-scorer-bad-model"
]

VerificationTest[
    TAtpSetGnnScorer[<|"Kind" -> "GNN"|>],
    False,
    {TAtpSetGnnScorer::model},
    TestID -> "ATP/inputs/gnn-scorer-bad-model"
]

VerificationTest[
    {TAtpDescribeMethod["VampireUEQ"]["GoalDirected"], TAtpDescribeMethod["Twee"]["GoalDirected"],
        TAtpDescribeMethod[{"Twee", "GoalDirected" -> True}]["GoalDirected"]},
    {True, False, True},
    TestID -> "ATP/inputs/describe-method-goal-directed"
]

(* NProofs counts the runs that proved, not the conjectures. *)
VerificationTest[
    TAtpCpDataset[{OverBar[OverBar[x]] == x, x ** y == y ** x}, AxiomaticTheory["AbelianGroupAxioms"], TimeConstraint -> 5]["NProofs"],
    1,
    TestID -> "ATP/inputs/cp-dataset-counts-proofs"
]

VerificationTest[
    TStringPath["A1" -> "AB", {"A" -> "B"}, TimeConstraint -> 5],
    $Failed,
    TestID -> "ATP/inputs/string-path-unreachable"
]

(* A string that is no identifier inside an axiom becomes a symbol instead of looping. *)
VerificationTest[
    TimeConstrained[TFindProof[a == c, {a == "x y"}, "Status", TimeConstraint -> 5], 60],
    _String,
    SameTest -> MatchQ,
    TestID -> "ATP/inputs/non-identifier-string"
]

VerificationTest[
    {Context[PortfolioFrontLoad], ListQ[$AtpMethodPresets]},
    {"WolframInstitute`THVMLink`ATP`", True},
    TestID -> "ATP/inputs/public-names"
]

(* A goal-less completion keeps its TimeConstraint whatever is asked of it: the derivation
   dataset of the completed rules is built only for a ProofObject, in the time left. *)
VerificationTest[
    With[{ax = {CircleTimes[a_, e] == a_, CircleTimes[a_, inv[a_]] == e, CircleTimes[a_, CircleTimes[b_, c_]] == CircleTimes[CircleTimes[a_, b_], c_]}},
        Map[First @ AbsoluteTiming[TFindProof[ax, #, TimeConstraint -> 3]] < 15 &, {"Status", "Statistics", "Lemmas"}]],
    {True, True, True},
    TestID -> "ATP/inputs/completion-keeps-time-limit"
]

VerificationTest[
    With[{ax = {CircleTimes[a_, e] == a_, CircleTimes[a_, inv[a_]] == e, CircleTimes[a_, CircleTimes[b_, c_]] == CircleTimes[CircleTimes[a_, b_], c_]}},
        {#[[1]] < 15, Head[#[[2]]]} & @ AbsoluteTiming[TFindProof[ax, TimeConstraint -> 3]]],
    {True, ProofObject},
    {TFindProof::cplpo},
    TestID -> "ATP/inputs/completion-proof-object-in-time"
]

(* A theory's variables do not take the names of the goal's constants, so the path keeps a and b
   constant. *)
VerificationTest[
    With[{p = TFindEquationalPath[OverBar[CircleTimes[a, b]] == CircleTimes[OverBar[b], OverBar[a]], "AbelianGroupAxioms"]},
        {FreeQ[p, _Pattern], First[p], Last[p]}],
    {True, OverBar[CircleTimes[a, b]], CircleTimes[OverBar[b], OverBar[a]]},
    TestID -> "ATP/inputs/theory-variables-avoid-goal-constants"
]

(* "BBAA" == "AABB" is False before TFindProof sees it: the encoder's Failure comes back, with no
   Lookup messages from the method schedule. *)
VerificationTest[
    {Head[TFindProof["BBAA" == "AABB", {"BA" -> "AB"}]], TFindProof["BBAA" == "AABB", {"BA" -> "AB"}, "Status"]},
    {Failure, "Failed"},
    TestID -> "ATP/inputs/false-goal-is-a-failure"
]

(* The theory's variables avoid the goal's names in any context, as on a documentation page whose
   symbols live in a private context. *)
VerificationTest[
    Block[{$Context = "ThvmPageCtx`", $ContextPath = {"WolframInstitute`THVMLink`ATP`", "WolframInstitute`THVMLink`", "System`"}},
        ToExpression["FreeQ[TFindEquationalPath[OverBar[a \\[CircleTimes] b] == OverBar[b] \\[CircleTimes] OverBar[a], \"AbelianGroupAxioms\"], _Pattern]"]],
    True,
    TestID -> "ATP/inputs/theory-variables-avoid-goal-names-in-any-context"
]

(* TAtpTrainScorer refuses a dataset that is not 14-feature rows with 0/1 labels, and a hidden width
   outside 0 to 64, with its own message instead of training on garbage. *)
VerificationTest[
    TAtpTrainScorer[<|"Features" -> {{1., 2.}}, "Labels" -> {1}|>],
    $Failed,
    {TAtpTrainScorer::data},
    TestID -> "ATP/inputs/train-scorer-bad-data"
]

VerificationTest[
    TAtpTrainScorer[<|"Features" -> Table[N[Mod[i + j, 4]], {i, 6}, {j, 14}], "Labels" -> {1, 0, 0, 0, 1, 0}|>, "Hidden" -> 100],
    $Failed,
    {TAtpTrainScorer::hidden},
    TestID -> "ATP/inputs/train-scorer-bad-hidden"
]

(* A second training in the same kernel works (it used to die in a JIT-compiled CPU kernel). *)
VerificationTest[
    With[{d = <|"Features" -> Table[N[Mod[i + j, 4]], {i, 6}, {j, 14}], "Labels" -> {1, 0, 0, 0, 1, 0}|>},
        {TAtpTrainScorer[d]["TrainAUC"], TAtpTrainScorer[d]["TrainAUC"]}],
    {1., 1.},
    TestID -> "ATP/inputs/train-scorer-twice"
]
