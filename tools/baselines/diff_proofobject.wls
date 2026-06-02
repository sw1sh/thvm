#!/usr/bin/env wolframscript
SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
$RecursionLimit = 16384;
print[a___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {a}], "\n"];

(* Per-step structural-identity diff of two ProofObjects.

   Given preset & cli ProofObjects on the same theorem, walk each
   "Proof" Association and report per-construct match status:
     * the construct key (e.g. {CriticalPairLemma, 2}) is shared
     * the canonicalized Statement matches

   Useful for the user's directive: "compare ProofObjects, they
   should be identical if everything is ported faithfully". *)

normalizeStmt[s_] := Quiet @ Block[{e},
    e = s /. Inactive[Equal] -> Equal;
    e /. (Verbatim[Pattern][v_, _] :> v)
];

statementOf[entry_] := If[
    AssociationQ[entry],
    normalizeStmt @ Lookup[entry, "Statement", Missing[]],
    Missing[]
];

dataset[po_] := Which[
    Head[po] === ProofObject && Length[po] >= 4,
        Association @ Lookup[po[[4]], "Proof", {}],
    AssociationQ[po] && KeyExistsQ[po, "ProofDataset"],
        Block[{ds = po["ProofDataset"]},
            If[ AssociationQ[ds], ds, Association @ Normal[ds]]],
    True, <||>
];

constructOrder = {"Axiom", "Hypothesis", "CriticalPairLemma",
    "SubstitutionLemma", "Conclusion"};

sortKey[k_] := With[
    {idx = FirstPosition[constructOrder, First[k], {99}][[1]]},
    {idx, Last[k]}
];

diffOne[theory_String, thm_String] := Block[
    {pP, pC, dsP, dsC, keysP, keysC, allKeys,
     sortedKeys, matchCount, mismatchCount, onlyP, onlyC},
    pP = TFindProof[thm, theory, Method -> "Waldmeister",
        TimeConstraint -> 8];
    pC = TFindProof[thm, theory, Method -> "WaldmeisterProcess",
        TimeConstraint -> 12];
    dsP = dataset[pP];
    dsC = dataset[pC];
    keysP = Keys[dsP];
    keysC = Keys[dsC];
    allKeys = SortBy[Union[keysP, keysC], sortKey];
    onlyP = Complement[keysP, keysC];
    onlyC = Complement[keysC, keysP];
    matchCount = 0;
    mismatchCount = 0;
    print["=== ", theory, " / ", thm, " ==="];
    print["preset constructs: ", Length[keysP],
        "    cli constructs: ", Length[keysC]];
    print[];
    print[StringPadRight["key", 28], " ",
        StringPadRight["status", 12], " ",
        "stmt (preset vs CLI)"];
    Scan[
        k |-> Block[{entryP, entryC, sP, sC, status},
            (* Keys are Lists like {"Axiom", 1}.  Lookup with a list
               as key triggers multi-key lookup (returns a List of
               per-element lookups).  Wrap with Key[] to force a
               single-key lookup. *)
            entryP = Lookup[dsP, Key[k], Missing[]];
            entryC = Lookup[dsC, Key[k], Missing[]];
            sP = statementOf[entryP];
            sC = statementOf[entryC];
            status = Which[
                MissingQ[entryP] && MissingQ[entryC], "BOTH-MISS",
                MissingQ[entryP], "ONLY-CLI",
                MissingQ[entryC], "ONLY-PRESET",
                sP === sC, (matchCount += 1; "MATCH"),
                True, (mismatchCount += 1; "MISMATCH")
            ];
            print["  ",
                StringPadRight[ToString[k], 28], " ",
                StringPadRight[status, 12], " ",
                If[ status === "MATCH",
                    ToString[Short[sP, 60]],
                    StringJoin[
                        ToString[Short[sP, 28]],
                        "  /  ",
                        ToString[Short[sC, 28]]
                    ]
                ]]],
        allKeys];
    print[];
    print["matches:    ", matchCount];
    print["mismatches: ", mismatchCount];
    print["onlyPreset: ", Length[onlyP], ": ", onlyP];
    print["onlyCLI:    ", Length[onlyC], ": ", onlyC];
    print[];
];

Quiet @ TFindProof["InverseOfInverse", "GroupAxioms",
    Method -> "Completion", TimeConstraint -> 2.0];

diffOne["AbelianGroupAxioms", "InverseOfInverse"];
