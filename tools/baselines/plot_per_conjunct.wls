#!/usr/bin/env wolframscript
(* Plots per-conjunct visibility on thvm's NotableTheorems baseline.
   Source: tools/baselines/thvm_per_conjunct.tsv (one row per
   conjunct: theory, thm, k/n, status, seconds, proofLength, verifies).

   Emits two PNGs to tools/baselines/plots/:
     thvm_per_conjunct_status.png  -- stacked bar of per-conjunct
                                      PROVED/Failed/TimedOut/CRASH
                                      grouped by theorem family
     thvm_per_theorem_rollup.png   -- rolled-up per-theorem status:
                                      PROVED / Partial(k/n) / Failed
                                      grouped by theory *)
PacletDirectoryLoad["/Users/swish/src/thvm/wl/THVMLink"];

(* Two TSV pairs: the canonical default-Method bench AND the best-of
   over all probed configs.  Plotting both shows the lever progress. *)
srcPath = If[ Length[$ScriptCommandLine] >= 2 && $ScriptCommandLine[[2]] === "best",
    "tools/baselines/thvm_per_conjunct_best.tsv",
    "tools/baselines/thvm_per_conjunct.tsv"];
sumPath = If[ Length[$ScriptCommandLine] >= 2 && $ScriptCommandLine[[2]] === "best",
    "tools/baselines/thvm_per_conjunct_best_summary.tsv",
    "tools/baselines/thvm_per_conjunct_summary.tsv"];
plotSuffix = If[ Length[$ScriptCommandLine] >= 2 && $ScriptCommandLine[[2]] === "best",
    "_best", ""];
outDir  = "tools/baselines/plots";
If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];

readTsv[path_] := Module[{lines, header, rows},
    lines = Select[StringSplit[Import[path, "Text"], "\n"], # =!= "" &];
    header = StringSplit[First @ lines, "\t"];
    rows   = StringSplit[#, "\t"] & /@ Rest[lines];
    {header, AssociationThread[header -> #] & /@ rows}];

{_, perConj} = readTsv[srcPath];
{_, perThm}  = readTsv[sumPath];

Print[];
Print["=== Per-conjunct status counts (", Length[perConj], " conjuncts) ==="];
conjStatus = Counts[#["status"] & /@ perConj];
Scan[(Print["  ", StringPadRight[#[[1]], 15], " ", #[[2]]]) &,
    Normal @ KeySort @ conjStatus];

Print[];
Print["=== Per-theorem rolled-up status (", Length[perThm], " theorems) ==="];
thmStatus = Counts[#["status"] & /@ perThm];
Scan[(Print["  ", StringPadRight[#[[1]], 25], " ", #[[2]]]) &,
    Normal @ KeySort @ thmStatus];

(* === Per-conjunct status overall: pie + stacked bar ============= *)
colors = <|
    "PROVED"   -> RGBColor[0.20, 0.65, 0.30],
    "Failed"   -> RGBColor[0.75, 0.20, 0.20],
    "TimedOut" -> RGBColor[0.85, 0.55, 0.10],
    "CRASH"    -> RGBColor[0.30, 0.30, 0.30],
    "HardKilled" -> RGBColor[0.50, 0.10, 0.50]|>;
statusOrder = {"PROVED", "Failed", "TimedOut", "CRASH", "HardKilled"};
present = Select[statusOrder, KeyExistsQ[conjStatus, #] &];
pieData = Map[#[[2]] &, Normal @ KeyTake[conjStatus, present]];
pieLabels = Map[
    StringJoin[#[[1]], " (", ToString[#[[2]]], ")"] &,
    Normal @ KeyTake[conjStatus, present]];
pieColors = Map[colors[#] &, present];

conjPie = PieChart[pieData,
    ChartLabels -> pieLabels,
    ChartStyle -> pieColors,
    PlotLabel -> Style[StringJoin["Per-conjunct status (",
        ToString[Length[perConj]], " conjuncts across ",
        ToString[Length[perThm]], " NotableTheorems)"], Bold, 14],
    ImageSize -> {900, 700}];

Export[FileNameJoin[{outDir, "thvm_per_conjunct_status" <> plotSuffix <> ".png"}], conjPie];
Print["Wrote ", outDir, "/thvm_per_conjunct_status", plotSuffix, ".png"];

(* === Per-theorem rollup: bar chart grouped by family =========== *)
thmStatusOrder = {"PROVED", "Partial(2/3)", "Partial(1/3)",
    "Partial(1/2)", "Failed", "TimedOut", "CRASH"};
thmColors = <|
    "PROVED"        -> RGBColor[0.20, 0.65, 0.30],
    "Partial(2/3)"  -> RGBColor[0.50, 0.75, 0.30],
    "Partial(1/3)"  -> RGBColor[0.85, 0.75, 0.30],
    "Partial(1/2)"  -> RGBColor[0.65, 0.75, 0.30],
    "Failed"        -> RGBColor[0.75, 0.20, 0.20],
    "TimedOut"      -> RGBColor[0.85, 0.55, 0.10],
    "CRASH"         -> RGBColor[0.30, 0.30, 0.30]|>;

thmPresent = Select[thmStatusOrder, KeyExistsQ[thmStatus, #] &];
thmPieData = Map[thmStatus[#] &, thmPresent];
thmPieLabels = Map[
    StringJoin[#, " (", ToString[thmStatus[#]], ")"] &, thmPresent];
thmPieColors = Map[thmColors[#] &, thmPresent];

thmPie = PieChart[thmPieData,
    ChartLabels -> thmPieLabels,
    ChartStyle -> thmPieColors,
    PlotLabel -> Style[StringJoin["Rolled-up theorem status (",
        ToString[Length[perThm]], " NotableTheorems)"], Bold, 14],
    ImageSize -> {900, 700}];
Export[FileNameJoin[{outDir, "thvm_per_theorem_rollup" <> plotSuffix <> ".png"}], thmPie];
Print["Wrote ", outDir, "/thvm_per_theorem_rollup", plotSuffix, ".png"];

(* === Per-theory-family breakdown =============================== *)
families = GatherBy[perThm, #["theory"] &];
Print[];
Print["=== Per-theory family breakdown ==="];
Print[StringJoin[
    StringPadRight["Theory", 30], StringPadRight["Total", 7],
    StringPadRight["PROVED", 8], StringPadRight["Partial", 9],
    StringPadRight["Failed", 8], StringPadRight["Other", 7]]];
isPartialQ[s_String] := StringStartsQ[s, "Partial"];
isPartialQ[_] := False;

Scan[Function[fam,
    Module[{theory, total, proved, partial, failed, other, statuses},
        theory   = First[fam]["theory"];
        total    = Length[fam];
        statuses = #["status"] & /@ fam;
        proved   = Count[statuses, "PROVED"];
        partial  = Count[statuses, s_String /; isPartialQ[s]];
        failed   = Count[statuses, "Failed"];
        other    = total - proved - partial - failed;
        Print[StringJoin[
            StringPadRight[theory, 30], StringPadRight[ToString[total], 7],
            StringPadRight[ToString[proved], 8],
            StringPadRight[ToString[partial], 9],
            StringPadRight[ToString[failed], 8],
            StringPadRight[ToString[other], 7]]]]],
    families];
