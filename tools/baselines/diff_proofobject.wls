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

(* Constant detector: nullary leaves that aren't variables.
   Includes:
     * Integers / Reals (preset uses literal 1 for identity);
     * String tokens NOT matching the x<digits> variable shape
       (TPTPImport's skolem constants like "k1", "skc1" land here);
     * Non-Global` Symbols that aren't operators -- e.g. System`
       symbols introduced as constant names; ruled out by checking
       they don't take args anywhere in the expression. *)
constQ[s_Integer] := True;
constQ[s_Real]    := True;
constQ[s_String] := ! varQ[s];
constQ[_]         := False;

(* Alpha-rename ALL leaves (variables + constants together) into a
   single canonical c1, c2, ... sequence in DFS-first-occurrence
   order.  Merging the two namespaces means the preset's
   universally-quantified `a` matches the CLI's negated-conjecture
   skolem `skC1` -- both are the same theorem-level object after
   alpha-equivalence.  Specifically:
     preset Hyp: ~~a == a   -> ~~c1 == c1
     CLI   Hyp: ~~skc1 == skc1 -> ~~c1 == c1   (match) *)
alphaRename[expr_] := Block[
    {leaves, mapping},
    leaves = DeleteDuplicates @ Cases[
        {expr}, x_ /; (varQ[x] || constQ[x]), Infinity];
    mapping = AssociationThread[leaves,
        Symbol /@ Table["c" <> ToString[i], {i, Length[leaves]}]];
    expr /. mapping
];

(* Bidirectional equation canonicalizer: an equation `a == b` and
   `b == a` are the same axiom under EquationalLogic.  Pick the
   side-orientation that puts the lexicographically-smaller form
   (by InputForm string) on the left so the match key is unique. *)
canonEq[Equal[lhs_, rhs_]] := Block[
    {a = ToString @ InputForm @ lhs, b = ToString @ InputForm @ rhs},
    If[ OrderedQ[{a, b}], Equal[lhs, rhs], Equal[rhs, lhs]]
];
canonEq[e_] := e;

normalizeStmt[s_] := Quiet @ Block[{e = s},
    e = e /. Inactive[Equal] -> Equal;
    (* Preset's Statement is wrapped in HoldForm by the
       ProofObject builder; CLI side isn't.  Unwrap so both sides
       reach Equal[lhs, rhs] before alpha-rename. *)
    e = e /. HoldForm[x_] :> x;
    (* TPTPImport renders TPTP variables AND constants as String
       tokens wrapped in a zero-arg call: "x1"[] for variables,
       "k1"[] / "skc1"[] for constants.  Strip the trailing `[]`
       from any String / Integer / Real leaf -- it's never a real
       function call. *)
    e = e //. h_[] /; (StringQ[h] || IntegerQ[h] || ! varQ[h]
            && AtomQ[h]) :> h;
    e = e /. (Verbatim[Pattern][v_, _] :> v);
    canonEq @ alphaRename[e]
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

$summary = {};

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
    AppendTo[$summary, <|
        "Theory" -> theory, "Theorem" -> thm,
        "PresetN" -> Length[dsP], "CliN" -> Length[dsC],
        "Matched" -> Length[matchedP],
        "PresetOnly" -> Length[unmatchedP],
        "CliOnly" -> Length[unmatchedC]
    |>];
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

cases = {
    {"AbelianGroupAxioms", "InverseOfInverse"},
    {"AbelianGroupAxioms", "InverseOfComposite"},
    {"AbelianGroupAxioms", "ImpliesMcCuneAxioms"},
    {"AbelianGroupAxioms", "ImpliesAbelianMcCuneAxioms"},
    {"BooleanAxioms",      "DoubleNegation"},
    {"GroupAxioms",        "InverseOfInverse"},
    {"HillmanAxioms",      "Commutativity"},
    {"MeredithAxioms",     "Commutativity"}
};

Scan[Apply[diffOne], cases];

print["===================================================="];
print["sweep summary"];
print["===================================================="];
print[
    StringPadRight["theory", 22], " ",
    StringPadRight["theorem", 26], " ",
    StringPadLeft["preset", 7], " ",
    StringPadLeft["cli", 5], " ",
    StringPadLeft["match", 7], " ",
    StringPadLeft["pOnly", 7], " ",
    StringPadLeft["cOnly", 7]];
Scan[
    r |-> print[
        StringPadRight[r["Theory"],   22], " ",
        StringPadRight[r["Theorem"],  26], " ",
        StringPadLeft[ToString[r["PresetN"]],    7], " ",
        StringPadLeft[ToString[r["CliN"]],       5], " ",
        StringPadLeft[ToString[r["Matched"]],    7], " ",
        StringPadLeft[ToString[r["PresetOnly"]], 7], " ",
        StringPadLeft[ToString[r["CliOnly"]],    7]],
    $summary];

Export["tools/baselines/parity_wm_perstep.tsv",
    Prepend[
        Map[
            r |-> {r["Theory"], r["Theorem"],
                r["PresetN"], r["CliN"],
                r["Matched"], r["PresetOnly"], r["CliOnly"]},
            $summary],
        {"theory", "theorem",
            "preset_constructs", "cli_constructs",
            "matched", "preset_only", "cli_only"}
    ],
    "TSV"];
print[];
print["wrote tools/baselines/parity_wm_perstep.tsv"];
