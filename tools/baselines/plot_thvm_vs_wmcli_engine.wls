#!/usr/bin/env wolframscript
(* Horizontal grouped BarChart: thvm C-engine (discrimination-indexed) vs wmcli wall time.
   Plot log10(seconds) directly with custom second-valued ticks to dodge the
   ScalingFunctions->"Log" + BarOrigin->Left layout bug. *)

tsv = Import["tools/baselines/top20_engine_timing.tsv", "TSV"];
header = First[tsv];
rows = Rest[tsv];
idx = AssociationThread[header -> Range[Length[header]]];
getN[s_] := Quiet @ Module[{v = ToExpression[s]}, If[ NumberQ[v], N[v], Missing[]]];

data = Table[
   Module[{thm, cps, d, w, cli, best},
    thm = row[[idx["theorem"]]]; cps = getN @ row[[idx["wm_cps"]]];
    d = getN @ row[[idx["thvm_engine_default_s"]]]; w = getN @ row[[idx["thvm_engine_wm_s"]]];
    cli = getN @ row[[idx["wmcli_s"]]];
    best = Min @ DeleteMissing[{d, w}];
    <|"thm" -> thm, "cps" -> cps, "thvm" -> best, "wmcli" -> cli, "noproof" -> (best >= 60.0)|>],
   {row, rows}];
data = Take[ReverseSortBy[data, #cps &], UpTo[20]];

shortLabel[s_] := StringReplace[s,
   {"CombinatorAxioms__" -> "Combinator ", "WolframAxioms__" -> "Wolfram ",
    "BooleanAxioms__" -> "Boolean ", "CommutativeRingWithIdentityAxioms__" -> "CommRing1 ",
    "CommutativeRingAxioms__" -> "CommRing ", "RingWithIdentityAxioms__" -> "Ring1 ",
    "RingAxioms__" -> "Ring ", "McCuneAxioms__" -> "McCune ", "MeredithAxioms__" -> "Meredith ",
    "HillmanAxioms__" -> "Hillman ", "__" -> " ", "_" -> " "}];

(* log10 of seconds, floored so the sub-ms wmcli bars are visible *)
floor = 0.0008;            (* 0.8 ms display floor *)
lg[x_] := Log10[Max[x, floor]];
labels = (shortLabel[#thm] <> If[#noproof, " (thvm>60s*)", ""]) & /@ data;
thvmL = lg[#thvm] & /@ data;
wmcliL = lg[#wmcli] & /@ data;

(* reverse so largest-cps is on top *)
labels = Reverse @ labels; thvmL = Reverse @ thvmL; wmcliL = Reverse @ wmcliL;
pairs = Transpose[{thvmL, wmcliL}];

blue = LightDarkSwitched[RGBColor[0.16, 0.50, 0.73], RGBColor[0.36, 0.68, 0.91]];
red = LightDarkSwitched[RGBColor[0.78, 0.24, 0.20], RGBColor[0.93, 0.42, 0.38]];

(* second-valued ticks on the log axis *)
secTicks = {{lg[0.001], "1 ms"}, {lg[0.01], "10 ms"}, {lg[0.1], "0.1 s"},
   {lg[1], "1 s"}, {lg[10], "10 s"}, {lg[60], "60 s (cap)"}};

chart = BarChart[pairs,
   BarOrigin -> Left,
   ChartLabels -> {Placed[labels, Axis], None},
   ChartLegends -> Placed[{"thvm C-engine (discrimination-indexed, best of default/WM)",
       "wmcli (Waldmeister, banked wall)"}, Below],
   ChartStyle -> {blue, red},
   BarSpacing -> {0.25, 1.1},
   Ticks -> {None, secTicks},
   GridLines -> {First /@ secTicks, None},
   GridLinesStyle -> Directive[GrayLevel[0.5, 0.35], Dashed],
   PlotLabel -> Style[
     "thvm C-engine (discrimination-indexed) vs wmcli wall time\n" <>
      "top-20 notable theorems by Waldmeister critical-pair count  (log scale; * = thvm did not prove in 60s)",
     14, Bold],
   ImageSize -> {1000, 1150},
   LabelStyle -> 12,
   AspectRatio -> 1.3,
   PlotRangePadding -> {{Scaled[0.02], Scaled[0.08]}, {Scaled[0.01], Scaled[0.03]}}];

Export["tools/baselines/plots/top20_thvm_vs_wmcli_engine.png", chart, ImageResolution -> 130, Background -> GrayLevel[0.12]];
Print["exported tools/baselines/plots/top20_thvm_vs_wmcli_engine.png  rows=", Length[data]];
