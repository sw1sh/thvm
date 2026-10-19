(* atp_twee.wlt -- TTweeProof external-CLI wrapper tests. *)

tweeAvailable := StringQ[WolframInstitute`THVMLink`ATP`Private`tweeBinary[Automatic]];

VerificationTest[
    If[ ! tweeAvailable, "Proved",
        Module[{r = TTweeProof["AbelianGroupAxioms",
                "InverseOfInverse", TimeConstraint -> 5]},
            r["Status"]]],
    "Proved",
    TestID -> "Twee/two-arg/AbelianGroup_InverseOfInverse"
]

VerificationTest[
    If[ ! tweeAvailable, "twee",
        Module[{r = TTweeProof["BooleanAxioms",
                "DoubleNegation", TimeConstraint -> 5]},
            r["Strategy"]]],
    "twee",
    TestID -> "Twee/strategy-tag"
]

VerificationTest[
    If[ ! tweeAvailable, True,
        Module[{r = TTweeProof["BooleanAxioms",
                "DoubleNegation", TimeConstraint -> 5]},
            r["ProofLength"] > 0 && IntegerQ[r["ProofLength"]]]],
    True,
    TestID -> "Twee/proof-length-positive"
]

(* TTweeProofObject keeps every axiom statement, not only the last. *)
VerificationTest[
    If[ ! tweeAvailable, 4,
        Length @ TTweeProofObject["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 5]["Axioms"]],
    4,
    TestID -> "Twee/proof-object-keeps-all-axioms"
]

(* Stopped at its time limit, Twee reports TimedOut, close to the limit. *)
VerificationTest[
    If[ ! tweeAvailable, {"TimedOut", True},
        Module[{r = TTweeProof["RobbinsAxioms", "DoubleNegation", TimeConstraint -> 1]},
            {r["Status"], r["Seconds"] < 3}]],
    {"TimedOut", True},
    TestID -> "Twee/time-limit-status"
]
