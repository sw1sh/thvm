#!/usr/bin/env wolframscript
(* export_all.wls - read .uncrackable.txt, generate TPTP files for
   every Theory::Theorem listed (skipping comment lines).
   Inlines the exporter so we only pay one kernel startup. *)

If[ Length[$ScriptCommandLine] < 3,
    Print["usage: export_all.wls <list-file> <outdir>\n"];
    Exit[2]];

listFile = $ScriptCommandLine[[2]];
outDir   = $ScriptCommandLine[[3]];

If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];

(* ---------- TPTP exporter (inlined) ----------------------------- *)

$opNames = <|
    CircleTimes -> "and", CirclePlus  -> "or",
    OverBar     -> "not",
    CircleMinus -> "nand_op", CircleDot -> "fop",
    Diamond -> "diamond", Star -> "star",
    Wedge -> "wedge", Vee -> "vee", SmallCircle -> "circ",
    Times -> "mul", Plus -> "add",
    Equivalent -> "equiv", Implies -> "implies",
    Not -> "lnot", And -> "land", Or -> "lor",
    Nand -> "nand", Nor -> "nor"
|>;

$constSeen   = <||>;
$nextConstId = 0;

opNameForHead[h_] := Lookup[$opNames, h,
    StringJoin["op_", ToLowerCase[ToString[h, InputForm]]]];

renderConst[v_] := Block[{key = HoldPattern[v], existing, nm},
    existing = Lookup[$constSeen, key, None];
    If[ existing =!= None, Return[existing]];
    nm = ToString[Unevaluated[v], InputForm];
    nm = StringReplace[nm, {
        "\\[Formal" ~~ x : LetterCharacter .. ~~ "]" :>
            "f_" <> ToLowerCase[x],
        "\\[" ~~ x__ ~~ "]" :> "u_" <> ToLowerCase[x]}];
    nm = StringReplace[nm, RegularExpression["[^a-zA-Z0-9_]"] -> "_"];
    (* TPTP CNF: bareword starting with uppercase = variable.  All our
       constants need a lowercase leading char.  Prefix with "c_". *)
    If[ nm =!= "" && StringMatchQ[StringTake[nm, 1], RegularExpression["[A-Z]"]],
        nm = "c_" <> ToLowerCase[StringTake[nm, 1]] <> StringDrop[nm, 1]];
    If[ nm === "" || StringMatchQ[nm, RegularExpression["[0-9].*"]],
        $nextConstId += 1; nm = "k" <> ToString[$nextConstId]];
    $constSeen[key] = nm;
    nm];

makeVarMap[vars_List] := MapIndexed[
    Function[{v, i}, v -> "X" <> ToString[i[[1]]], HoldFirst], vars];

makeSkolemMap[vars_List] := MapIndexed[
    Function[{v, i}, v -> "sk_c" <> ToString[i[[1]]], HoldFirst], vars];

lookupHold[expr_, rules_List] := Block[{found = $Failed},
    Do[ If[ expr === First[rules[[i]]],
            found = Last[rules[[i]]]; Break[]],
        {i, Length[rules]}];
    found];

renderTerm[expr_, vmap_List] := Block[{v},
    v = lookupHold[expr, vmap];
    If[ v =!= $Failed, Return[v]];
    Which[
        AtomQ[expr], renderConst[expr],
        True, Block[{h = Head[expr], parts},
            parts = renderTerm[#, vmap] & /@ List @@ expr;
            opNameForHead[h] <> "(" <>
                StringRiffle[parts, ","] <> ")"]]];

extractVars[expr_] := If[ Head[expr] === ForAll,
    Block[{v = expr[[1]]},
        If[ Head[v] === List, List @@ v, {v}]], {}];

stripForAll[expr_] := If[ Head[expr] === ForAll, expr[[2]], expr];

axToCnf[expr_, name_String] := Block[{
    body = stripForAll[expr], vars = extractVars[expr], vmap},
    vmap = makeVarMap[vars];
    If[ ! MatchQ[body, _Equal], Return[$Failed]];
    StringJoin["cnf(", name, ", axiom, ",
        renderTerm[body[[1]], vmap], " = ",
        renderTerm[body[[2]], vmap], ")."]];

conjToNegatedCnf[expr_, name_String] := Block[{
    body = stripForAll[expr], vars = extractVars[expr], vmap},
    vmap = makeSkolemMap[vars];
    If[ ! MatchQ[body, _Equal], Return[$Failed]];
    StringJoin["cnf(", name, ", negated_conjecture, ",
        renderTerm[body[[1]], vmap], " != ",
        renderTerm[body[[2]], vmap], ")."]];

exportOne[theoryName_, theoremName_, outFile_] := Block[{
    axioms, ntAssoc, cjRaw, conjuncts, axClauses,
    written = {}},
    (* Reset per-problem const table so each file's constants are
       named from scratch. *)
    $constSeen = <||>;
    $nextConstId = 0;
    axioms = AxiomaticTheory[theoryName, "Axioms"];
    If[ ! ListQ[axioms] || Length[axioms] === 0,
        Print["  no axioms for ", theoryName, "\n"]; Return[written]];
    ntAssoc = AxiomaticTheory[theoryName, "NotableTheorems"];
    If[ ! AssociationQ[ntAssoc],
        Print["  no notable theorems for ", theoryName, "\n"]; Return[written]];
    cjRaw = Lookup[ntAssoc, theoremName, None];
    If[ cjRaw === None,
        Print["  ", theoremName, " not in ", theoryName, " NotableTheorems\n"];
        Return[written]];
    conjuncts = If[ MatchQ[cjRaw, _List], cjRaw, {cjRaw}];
    axClauses = MapIndexed[
        axToCnf[#1, "ax" <> ToString[#2[[1]]]] &, axioms];
    axClauses = Select[axClauses, # =!= $Failed &];
    MapIndexed[
        Block[{idx = #2[[1]], conj = #1, fn, gcl, lines},
            fn = If[ Length[conjuncts] === 1, outFile,
                StringReplace[outFile, ".p" -> "__c" <> ToString[idx] <> ".p"]];
            gcl = conjToNegatedCnf[conj, "goal"];
            If[ gcl === $Failed,
                Print["  skip (non-equation): ", fn, "\n"]; Return[]];
            lines = Join[
                {"% TPTP problem: " <> theoryName <> "::" <> theoremName <>
                    If[ Length[conjuncts] > 1,
                        " (conjunct " <> ToString[idx] <> ")", ""],
                 "% axioms = " <> ToString[Length[axClauses]] <>
                    ", conjuncts total = " <> ToString[Length[conjuncts]]},
                axClauses,
                {gcl}];
            Export[fn, StringRiffle[lines, "\n"] <> "\n", "Text"];
            AppendTo[written, fn]] &,
        conjuncts];
    written];

(* ---------- main batch ------------------------------------------ *)

lines = Import[listFile, "Lines"];
entries = Select[lines,
    ! StringStartsQ[#, "#"] && StringContainsQ[#, "::"] &];

Print["entries: ", Length[entries], "\n"];

allFiles = {};
Do[
    Block[{parts = StringSplit[entry, "::"], theory, theorem, out, w},
        theory = parts[[1]];
        theorem = parts[[2]];
        out = FileNameJoin[{outDir, theory <> "__" <> theorem <> ".p"}];
        Print["[", entry, "]\n"];
        w = exportOne[theory, theorem, out];
        AppendTo[allFiles, w];
        Do[ Print["  -> ", f, "\n"], {f, w}];
    ],
    {entry, entries}];

Print["TOTAL FILES: ", Length[Flatten[allFiles]], "\n"];
Exit[0]
