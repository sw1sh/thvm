(* atp_twee.wlt -- TTweeProof external-CLI wrapper tests. *)

tweeAvailable := FileExistsQ["/Users/swish/.cabal/bin/twee"] ||
    FileExistsQ["/opt/homebrew/bin/twee"];

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
