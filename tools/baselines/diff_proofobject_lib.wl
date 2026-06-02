(* Shared canon-stack + diff library for the per-step ATP parity
   harness.  Loaded by both diff_proofobject.wls (sweep-in-one-
   kernel) and diff_one_case.wls (iso-kernel-per-case).  All
   public symbols are in the THVMBaselines` context. *)

BeginPackage["THVMBaselines`"]

THVMBaselines`runDiff::usage =
    "runDiff[presetPO, cliPO] returns an Association summarizing the " <>
    "per-step structural diff between two ProofObjects (or one ProofObject " <>
    "+ one CLI Association with a ProofDataset).  Keys: PresetN, CliN, " <>
    "Matched, PresetOnly, CliOnly."

Begin["`Private`"]

varQ[s_Symbol] := Context[s] === "Global`";
varQ[s_String] := StringMatchQ[s, "x" ~~ DigitCharacter ..];
varQ[_] := False;

constQ[s_Integer] := True;
constQ[s_Real]    := True;
constQ[s_String]  := ! varQ[s];
constQ[_]         := False;

alphaRename[expr_] := Block[{leaves, mapping},
    leaves = DeleteDuplicates @ Cases[
        {expr}, x_ /; (varQ[x] || constQ[x]), Infinity];
    mapping = AssociationThread[leaves,
        Table[CanonVar[i], {i, Length[leaves]}]];
    expr /. mapping
];

canonEq[Equal[lhs_, rhs_]] := Block[
    {a = ToString @ InputForm @ lhs, b = ToString @ InputForm @ rhs},
    If[ OrderedQ[{a, b}], Equal[lhs, rhs], Equal[rhs, lhs]]
];
canonEq[e_] := e;

normalizeStmt[s_] := Quiet @ Block[{e = s},
    e = e /. Inactive[Equal] -> Equal;
    e = e /. HoldForm[x_] :> x;
    e = e //. h_[] /; (StringQ[h] || IntegerQ[h] || ! varQ[h]
            && AtomQ[h]) :> h;
    e = e /. (Verbatim[Pattern][v_, _] :> v);
    canonEq @ alphaRename[e]
];

statementOf[entry_] := If[ AssociationQ[entry],
    normalizeStmt @ Lookup[entry, "Statement", Missing[]],
    Missing[]];

dataset[po_] := Which[
    Head[po] === ProofObject && Length[po] >= 4,
        Association @ Lookup[po[[4]], "Proof", {}],
    AssociationQ[po] && KeyExistsQ[po, "ProofDataset"],
        Block[{ds = po["ProofDataset"]},
            If[ AssociationQ[ds], ds, Association @ Normal[ds]]],
    True, <||>
];

THVMBaselines`runDiff[poP_, poC_] := Block[
    {dsP, dsC, kvP, kvC, matchedP, matchedC, unmatchedP, unmatchedC},
    dsP = dataset[poP];
    dsC = dataset[poC];
    kvP = KeyValueMap[#1 -> {First[#1], statementOf[#2]} &, dsP];
    kvC = KeyValueMap[#1 -> {First[#1], statementOf[#2]} &, dsC];
    matchedP = <||>;
    matchedC = <||>;
    Do[
        Block[{kP = kvP[[i, 1]], stmtP = kvP[[i, 2]], j, kC},
            j = FirstPosition[kvC, _ -> stmtP, {Missing[]}][[1]];
            If[ ! MissingQ[j],
                kC = kvC[[j, 1]];
                AssociateTo[matchedP, kP -> kC];
                AssociateTo[matchedC, kC -> kP];
                kvC = ReplacePart[kvC, j -> (Null -> Null)]]],
        {i, Length[kvP]}];
    unmatchedP = Complement[Keys[dsP], Keys[matchedP]];
    unmatchedC = Complement[Keys[dsC], Keys[matchedC]];
    <|
        "PresetN"    -> Length[dsP],
        "CliN"       -> Length[dsC],
        "Matched"    -> Length[matchedP],
        "PresetOnly" -> Length[unmatchedP],
        "CliOnly"    -> Length[unmatchedC]
    |>
];

End[]
EndPackage[]
