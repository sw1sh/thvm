#!/usr/bin/env wolframscript
SetDirectory["/Users/swish/src/thvm"];
print[a___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {a}], "\n"];

(* Pull both the easy tier (8 cases, already-committed) and the
   hard tier (4 WolframAxioms + MeredithAxioms NotableTheorems
   that the preset Method->Automatic can crack but where the
   per-step match collapses because the proof space is wide). *)
loadTsv[path_String] := Block[{rows = Import[path, "TSV"]},
    AssociationThread[First[rows] -> #] & /@ Rest[rows]];

data = Join[
    loadTsv["tools/baselines/parity_perstep.tsv"],
    loadTsv["tools/baselines/parity_perstep_hard.tsv"]];

(* LightDarkSwitched isn't a valid ChartStyle directive (BarChart
   silently renders bars white); plain RGB works under any theme. *)
style[c_] := c;

(* Match-rate: matched / preset_constructs, capped to [0,1].  When
   preset failed (preset_constructs == 0) report 0. *)
matchRate[r_] := If[ r["preset_constructs"] === 0, 0,
    N[r["matched"] / r["preset_constructs"]]];

caseLabel[r_] := StringSplit[r["theory"], "Axioms"][[1]] <>
    "/" <> r["theorem"];

caseLabels = DeleteDuplicates[caseLabel /@ data];

(* Per-theorem grouped bars: 3 presets side-by-side.
   Waldmeister (fixed), Automatic (problem-aware), VampireUEQ. *)
presetsTested = {"Waldmeister", "Automatic", "VampireUEQ"};

matchData = Table[
    Map[
        preset |-> Block[
            {r = SelectFirst[data,
                #["preset"] === preset &&
                caseLabel[#] === lbl &]},
            If[ AssociationQ[r], matchRate[r], 0]],
        presetsTested],
    {lbl, caseLabels}];

(* Rotate the x labels 45 deg so 12 cases fit without collision. *)
rotated[s_String] := Rotate[Style[s, FontSize -> 11],
    -45 Degree];

bc = BarChart[
    matchData,
    ChartLabels -> {rotated /@ caseLabels, None},
    PlotLabel ->
        "per-step statement match rate (matched / preset_constructs)",
    AxesLabel -> {None, "fraction matched"},
    ChartStyle -> {style[StandardBlue],
        style[StandardGreen],
        style[StandardRed]},
    ChartLegends -> Placed[
        {"Method->\"Waldmeister\"",
         "Method->Automatic",
         "Method->\"VampireUEQ\""}, Top],
    PlotRange -> {0, 1.05},
    ImageSize -> {1100, 700},
    BarSpacing -> {Small, Medium},
    PlotTheme -> "Detailed",
    GridLines -> {None,
        {{1.0, Directive[GrayLevel[0.5], Dashed]}}}
];

Export["tools/baselines/plots/perstep_match_rate.png", bc,
    ImageResolution -> 200];
print["wrote tools/baselines/plots/perstep_match_rate.png"];
