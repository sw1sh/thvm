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
    (* Use a custom CanonVar[i] head instead of Symbol[name] --
       Symbol["1"] would fail on names starting with a digit, and
       some hard CLI cases (Meredith / Wolfram) push integer
       constants into the leaf set that fed Symbol[ToString[1]]. *)
    mapping = AssociationThread[leaves,
        Table[CanonVar[i], {i, Length[leaves]}]];
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

(* Incremental TSV writer: appends one row per case so a mid-sweep
   crash leaves a partial file behind.  The final batch Export at
   the end overwrites with the full table. *)
$partialTsv = "tools/baselines/parity_perstep_partial.tsv";
Quiet @ DeleteFile[$partialTsv];
Export[$partialTsv,
    {{"preset", "theory", "theorem",
      "preset_constructs", "cli_constructs",
      "matched", "preset_only", "cli_only"}},
    "TSV"];

appendPartialRow[r_Association] := Block[
    {existing = Quiet @ Import[$partialTsv, "TSV"]},
    If[ ! ListQ[existing], existing = {}];
    Quiet @ Export[$partialTsv,
        Append[existing,
            {r["Preset"], r["Theory"], r["Theorem"],
             r["PresetN"], r["CliN"],
             r["Matched"], r["PresetOnly"], r["CliOnly"]}],
        "TSV"]
];

(* presetMethod is a thvm Method spec (a single-String preset like
   "Waldmeister" / "VampireUEQ" / "Portfolio", or Automatic for
   the structure-recognized auto-tune); cliBuilder is the symbol
   of the matching CLI ProofObject builder
   (TWaldmeisterProofObject, TVampireProofObject, etc.). *)
diffOne[theory_String, thm_String,
        presetMethod_ : "Waldmeister",
        cliBuilder_Symbol : THVMLink`ATP`TWaldmeisterProofObject] :=
    Block[
    {pP, pC, dsP, dsC, kvP, kvC, matchedP, matchedC,
     unmatchedP, unmatchedC},
    pP = TFindProof[thm, theory, Method -> presetMethod,
        TimeConstraint -> 8];
    pC = cliBuilder[theory, thm,
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
    Block[{row = <|
            "Theory" -> theory, "Theorem" -> thm,
            "Preset" -> ToString[presetMethod],
            "PresetN" -> Length[dsP], "CliN" -> Length[dsC],
            "Matched" -> Length[matchedP],
            "PresetOnly" -> Length[unmatchedP],
            "CliOnly" -> Length[unmatchedC]
        |>},
        AppendTo[$summary, row];
        appendPartialRow[row]];
    print["=== ", ToString[presetMethod], " :: ", theory, " / ", thm, " ==="];
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

(* Easy tier (8): saturator-friendly AbelianGroup + Group + Hillman
   + Meredith Commutativity.  Hard tier (5): WolframAxioms +
   MeredithAxioms theorems where the preset cracks the case via
   Method->Automatic (the structure-recognized auto-tune) -- some
   harder Wolfram theorems (And/OrAssociativity) timeout at 20s on
   all three presets and aren't covered by this sweep. *)
cases = {
    {"AbelianGroupAxioms", "InverseOfInverse"},
    {"AbelianGroupAxioms", "InverseOfComposite"},
    {"AbelianGroupAxioms", "ImpliesMcCuneAxioms"},
    {"AbelianGroupAxioms", "ImpliesAbelianMcCuneAxioms"},
    {"BooleanAxioms",      "DoubleNegation"},
    {"GroupAxioms",        "InverseOfInverse"},
    {"HillmanAxioms",      "Commutativity"},
    {"MeredithAxioms",     "Commutativity"},
    {"WolframAxioms",      "DoubleNegation"},
    {"WolframAxioms",      "Commutativity"},
    {"MeredithAxioms",     "AndAssociativity"},
    {"MeredithAxioms",     "OrAssociativity"}
};

(* Two-prover sweep: WM (default 2-arg diffOne) + Vampire (extra
   args).  Concatenates into one summary table tagged by Preset.
   Method->Automatic is the problem-aware structure-recognized
   schedule -- on theories where the fixed "Waldmeister" preset
   doesn't fit (Hillman / Meredith Commutativity timeout at 60s
   even though wmcli proves them in <12s), Automatic's auto-tune
   correctly picks a strategy that proves quickly. *)
(* Three-preset sweep on the WM CLI side: "Waldmeister" (the
   actual fixed port -- matches wmcli on most theorems but times
   out on Hillman/Meredith), Automatic (problem-aware auto-tune
   -- cracks Hillman/Meredith in <100ms but diverges from wmcli's
   strategy on AbelianGroup theorems), and one row per case for
   VampireUEQ vs the Vampire CLI. *)
Scan[
    Apply[Function[{theory, thm},
        diffOne[theory, thm,
            "Waldmeister",
            THVMLink`ATP`TWaldmeisterProofObject]]],
    cases];
Scan[
    Apply[Function[{theory, thm},
        diffOne[theory, thm,
            Automatic,
            THVMLink`ATP`TWaldmeisterProofObject]]],
    cases];
Scan[
    Apply[Function[{theory, thm},
        diffOne[theory, thm,
            "VampireUEQ",
            THVMLink`ATP`TVampireProofObject]]],
    cases];

print["===================================================="];
print["per-step parity sweep summary"];
print["===================================================="];
print[
    StringPadRight["preset", 12], " ",
    StringPadRight["theory", 22], " ",
    StringPadRight["theorem", 26], " ",
    StringPadLeft["pN", 5], " ",
    StringPadLeft["cN", 5], " ",
    StringPadLeft["match", 7], " ",
    StringPadLeft["pOnly", 7], " ",
    StringPadLeft["cOnly", 7]];
Scan[
    r |-> print[
        StringPadRight[r["Preset"],   12], " ",
        StringPadRight[r["Theory"],   22], " ",
        StringPadRight[r["Theorem"],  26], " ",
        StringPadLeft[ToString[r["PresetN"]],    5], " ",
        StringPadLeft[ToString[r["CliN"]],       5], " ",
        StringPadLeft[ToString[r["Matched"]],    7], " ",
        StringPadLeft[ToString[r["PresetOnly"]], 7], " ",
        StringPadLeft[ToString[r["CliOnly"]],    7]],
    $summary];

Export["tools/baselines/parity_perstep.tsv",
    Prepend[
        Map[
            r |-> {r["Preset"], r["Theory"], r["Theorem"],
                r["PresetN"], r["CliN"],
                r["Matched"], r["PresetOnly"], r["CliOnly"]},
            $summary],
        {"preset", "theory", "theorem",
            "preset_constructs", "cli_constructs",
            "matched", "preset_only", "cli_only"}
    ],
    "TSV"];
print[];
print["wrote tools/baselines/parity_perstep.tsv"];
