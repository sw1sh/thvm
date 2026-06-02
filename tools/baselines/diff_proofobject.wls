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

(* Variable detector: catches BOTH
     * preset-side Symbols in Global` (a, b, c, ...);
     * TPTPImport-side String tokens "x1", "x2", ...  (TPTPImport
       renders TPTP variables as String tokens, e.g. `"x1"[]`). *)
varQ[s_Symbol] := Context[s] === "Global`";
varQ[s_String] := StringMatchQ[s, "x" ~~ DigitCharacter ..];
varQ[_] := False;

(* Alpha-rename all variables to a canonical v1, v2, ... in order
   of first DFS appearance.  Pull every leaf that varQ accepts from
   inside the expression (excluding the expression's HEAD). *)
alphaRename[expr_] := Block[
    {vars, mapping},
    vars = DeleteDuplicates @ Cases[
        {expr},
        x_ /; varQ[x],
        Infinity];
    mapping = AssociationThread[vars,
        Symbol /@ Table["v" <> ToString[i], {i, Length[vars]}]];
    expr /. mapping
];

normalizeStmt[s_] := Quiet @ Block[{e = s},
    e = e /. Inactive[Equal] -> Equal;
    (* Preset's Statement is wrapped in HoldForm by the
       ProofObject builder; CLI side isn't.  Unwrap so both sides
       reach Equal[lhs, rhs] before alpha-rename. *)
    e = e /. HoldForm[x_] :> x;
    (* TPTPImport renders TPTP variables as `"x1"[]` (a String
       token wrapped as a zero-arg call); strip the trailing `[]`. *)
    e = e //. h_[] /; varQ[h] :> h;
    e = e /. (Verbatim[Pattern][v_, _] :> v);
    alphaRename[e]
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
    {pP, pC, dsP, dsC, kvP, kvC, matchedP, matchedC,
     unmatchedP, unmatchedC},
    pP = TFindProof[thm, theory, Method -> "Waldmeister",
        TimeConstraint -> 8];
    pC = THVMLink`ATP`TWaldmeisterProofObject[theory, thm,
        TimeConstraint -> 12, "ParseFormulas" -> True];
    dsP = dataset[pP];
    dsC = dataset[pC];
    (* Build {canonStmt -> key} maps so we can pair across renumbered
       axiom indices.  Group by construct type so we don't
       accidentally pair an Axiom statement with a CPL of the same
       canonical form. *)
    kvP = KeyValueMap[#1 -> {First[#1], statementOf[#2]} &, dsP];
    kvC = KeyValueMap[#1 -> {First[#1], statementOf[#2]} &, dsC];
    matchedP = <||>;
    matchedC = <||>;
    Do[
        Block[{kP = kvP[[i, 1]], stmtP = kvP[[i, 2]], j, kC},
            (* Find the first kvC entry with same {type, canonStmt}. *)
            j = FirstPosition[kvC, _ -> stmtP, {Missing[]}][[1]];
            If[ ! MissingQ[j],
                kC = kvC[[j, 1]];
                AssociateTo[matchedP, kP -> kC];
                AssociateTo[matchedC, kC -> kP];
                kvC = ReplacePart[kvC, j -> (Null -> Null)]
            ]],
        {i, Length[kvP]}];
    unmatchedP = Complement[Keys[dsP], Keys[matchedP]];
    unmatchedC = Complement[Keys[dsC], Keys[matchedC]];
    print["=== ", theory, " / ", thm, " ==="];
    print["preset constructs: ", Length[dsP],
        "    cli constructs: ", Length[dsC]];
    print["content-matched:   ", Length[matchedP], " pairs"];
    print["preset-only:       ", Length[unmatchedP],
        " ", unmatchedP];
    print["cli-only:          ", Length[unmatchedC],
        " ", unmatchedC];
    print[];
    print[StringPadRight["preset key", 28], " <->  ",
        StringPadRight["cli key", 28], " ",
        "canonStmt"];
    Scan[
        kP |-> Block[{kC, stmt},
            kC = matchedP[kP];
            stmt = statementOf[dsP[kP]];
            print["  ",
                StringPadRight[ToString[kP], 28], " <->  ",
                StringPadRight[ToString[kC], 28], " ",
                ToString[Short[stmt, 60]]]],
        SortBy[Keys[matchedP], sortKey]];
    Scan[
        kP |-> Block[{stmtP, stmtC},
            stmtP = statementOf[dsP[kP]];
            print["  ",
                StringPadRight[ToString[kP], 28], " <->  ",
                StringPadRight["(no match)", 28], " ",
                "preset: ", ToString[Short[stmtP, 30]]]],
        SortBy[unmatchedP, sortKey]];
    Scan[
        kC |-> Block[{stmtC},
            stmtC = statementOf[dsC[kC]];
            print["  ",
                StringPadRight["(no match)", 28], " <->  ",
                StringPadRight[ToString[kC], 28], " ",
                "cli: ", ToString[Short[stmtC, 30]]]],
        SortBy[unmatchedC, sortKey]];
];

Quiet @ TFindProof["InverseOfInverse", "GroupAxioms",
    Method -> "Completion", TimeConstraint -> 2.0];

diffOne["AbelianGroupAxioms", "InverseOfInverse"];
