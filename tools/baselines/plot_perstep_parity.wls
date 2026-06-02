#!/usr/bin/env wolframscript
SetDirectory["/Users/swish/src/thvm"];
print[a___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {a}], "\n"];

rows = Import["tools/baselines/parity_perstep.tsv", "TSV"];
header = First[rows];
data = AssociationThread[header -> #] & /@ Rest[rows];

style[c_] := LightDarkSwitched[{c, Darker[c, 0.25]}];

(* Match-rate: matched / preset_constructs, capped to [0,1].  When
   preset failed (preset_constructs == 0) report 0. *)
matchRate[r_] := If[ r["preset_constructs"] === 0, 0,
    N[r["matched"] / r["preset_constructs"]]];

caseLabel[r_] := StringSplit[r["theory"], "Axioms"][[1]] <>
    "/" <> r["theorem"];

caseLabels = DeleteDuplicates[caseLabel /@ data];

(* Stacked bars: per-theorem, side-by-side WM bar (blue) + Vampire
   bar (red).  Bar height = match rate (0..1). *)
matchData = Table[
    Block[{wm, vp},
        wm = SelectFirst[data,
            #["preset"] === "Waldmeister" &&
            caseLabel[#] === lbl &];
        vp = SelectFirst[data,
            #["preset"] === "VampireUEQ" &&
            caseLabel[#] === lbl &];
        {If[ AssociationQ[wm], matchRate[wm], 0],
         If[ AssociationQ[vp], matchRate[vp], 0]}],
    {lbl, caseLabels}];

bc = BarChart[
    matchData,
    ChartLabels -> {caseLabels, None},
    PlotLabel ->
        "per-step statement match rate (matched / preset_constructs)",
    AxesLabel -> {None, "fraction matched"},
    ChartStyle -> {style[StandardBlue], style[StandardRed]},
    ChartLegends -> Placed[
        {"Waldmeister == WaldmeisterProcess",
         "VampireUEQ == VampireProcess"}, Top],
    PlotRange -> {0, 1.05},
    ImageSize -> 1000,
    BarSpacing -> {Small, Medium},
    PlotTheme -> "Detailed",
    GridLines -> {None,
        {{1.0, Directive[GrayLevel[0.5], Dashed]}}}
];

Export["tools/baselines/plots/perstep_match_rate.png", bc,
    ImageResolution -> 160];
print["wrote tools/baselines/plots/perstep_match_rate.png"];
