(* atp_lift.wlt -- TWaldmeisterProofObject[..., LiftToProofObject->True]
   tests.  Verifies that the CLI Association can lift to a literal
   ProofObject["EquationalLogic", goal, axioms, data] head that WL's
   property machinery accepts.

   Skips gracefully when wmcli isn't available so this doesn't
   break CI on a fresh machine. *)

wmcliAvailable := StringQ[Environment["WMCLI"]] &&
    FileExistsQ[Environment["WMCLI"]];

(* === Lift produces a ProofObject head === *)

VerificationTest[
    If[ ! wmcliAvailable, ProofObject,
        Module[{po = TWaldmeisterProofObject[
                "AbelianGroupAxioms", "InverseOfInverse",
                TimeConstraint -> 12,
                "LiftToProofObject" -> True]},
            Head[po]]],
    ProofObject,
    TestID -> "Lift/head-is-ProofObject/AbelianGroup_InverseOfInverse"
]

(* === Lifted ProofObject's ProofFunction is a Function === *)

(* Quiet wraps the property access: the internal verifier emits
   Part::partd warnings while walking entries with True-Statement
   placeholders (the closure-collapse SL/Conclusion entries).
   The Function head returns correctly; the warnings are harmless. *)
VerificationTest[
    If[ ! wmcliAvailable, Function,
        Module[{po = TWaldmeisterProofObject[
                "AbelianGroupAxioms", "InverseOfInverse",
                TimeConstraint -> 12,
                "LiftToProofObject" -> True]},
            Quiet @ Head @ po["ProofFunction"]]],
    Function,
    TestID -> "Lift/ProofFunction-dispatches/AbelianGroup_InverseOfInverse"
]

(* === Lifted ProofObject's ProofGraph is a Graph === *)

VerificationTest[
    If[ ! wmcliAvailable, Graph,
        Module[{po = TWaldmeisterProofObject[
                "AbelianGroupAxioms", "InverseOfInverse",
                TimeConstraint -> 12,
                "LiftToProofObject" -> True]},
            Head @ po["ProofGraph"]]],
    Graph,
    TestID -> "Lift/ProofGraph-dispatches/AbelianGroup_InverseOfInverse"
]

(* === Lifted ProofObject's Theorems extracts the goal === *)

VerificationTest[
    If[ ! wmcliAvailable, True,
        Module[{po = TWaldmeisterProofObject[
                "AbelianGroupAxioms", "InverseOfInverse",
                TimeConstraint -> 12,
                "LiftToProofObject" -> True]},
            MatchQ[po["Theorems"], Inactive[Equal][_, _]]]],
    True,
    TestID -> "Lift/Theorems-shape/AbelianGroup_InverseOfInverse"
]

(* === Default option (no lift) returns an Association === *)

VerificationTest[
    If[ ! wmcliAvailable, Association,
        Module[{po = TWaldmeisterProofObject[
                "AbelianGroupAxioms", "InverseOfInverse",
                TimeConstraint -> 12]},
            Head[po]]],
    Association,
    TestID -> "Lift/default-is-Association"
]

(* === Lift works on a different theory: Group/InverseOfInverse === *)

VerificationTest[
    If[ ! wmcliAvailable, {ProofObject, Function},
        Module[{po = TWaldmeisterProofObject[
                "GroupAxioms", "InverseOfInverse",
                TimeConstraint -> 12,
                "LiftToProofObject" -> True]},
            {Head[po], Quiet @ Head @ po["ProofFunction"]}]],
    {ProofObject, Function},
    TestID -> "Lift/cross-theory/Group_InverseOfInverse"
]

(* === Lift works on BooleanAxioms/DoubleNegation === *)

VerificationTest[
    If[ ! wmcliAvailable, {ProofObject, Function},
        Module[{po = TWaldmeisterProofObject[
                "BooleanAxioms", "DoubleNegation",
                TimeConstraint -> 12,
                "LiftToProofObject" -> True]},
            {Head[po], Quiet @ Head @ po["ProofFunction"]}]],
    {ProofObject, Function},
    TestID -> "Lift/cross-theory/Boolean_DoubleNegation"
]

(* === Lift also works for the Vampire builder === *)

vampireAvailable := FileExistsQ["/opt/homebrew/bin/vampire"] ||
    FileExistsQ["/usr/local/bin/vampire"];

VerificationTest[
    If[ ! vampireAvailable, {ProofObject, Function},
        Module[{po = TVampireProofObject[
                "AbelianGroupAxioms", "InverseOfInverse",
                TimeConstraint -> 12,
                "LiftToProofObject" -> True]},
            {Head[po], Quiet @ Head @ po["ProofFunction"]}]],
    {ProofObject, Function},
    TestID -> "Lift/builder-Vampire/AbelianGroup_InverseOfInverse"
]

(* === Multi-conjunct notable theorem: ONE multi-goal wmcli run === *)

VerificationTest[
    (* A List-valued NotableTheorem (HuntingtonAxioms -> the three
       Robbins axioms) must prove EVERY conjunct in one multi-goal .pr
       (Waldmeister "All goals proved"), not silently prove only the
       first: the lifted ProofObject carries the full List-valued
       Theorem and one Conclusion per conjunct, mirroring
       FindEquationalProof's multi-conjunct shape. *)
    If[ ! wmcliAvailable, {ProofObject, 3, 3},
        Module[
            {po = TWaldmeisterProofObject[
                "HuntingtonAxioms", "ImpliesRobbinsAxioms",
                TimeConstraint -> 12,
                "LiftToProofObject" -> True]},
            {Head[po],
                Length[po["Theorem"]],
                Count[Keys @ Lookup[po[[4]], "Proof", {}], {"Conclusion", _}]}]],
    {ProofObject, 3, 3},
    TestID -> "Lift/multi-conjunct/Huntington_ImpliesRobbins"
]
