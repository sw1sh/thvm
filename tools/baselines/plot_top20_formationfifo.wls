#!/usr/bin/env wolframscript
(* Horizontal grouped BarChart: thvm FormationFifo parity preset vs wmcli banked
   wall time, top-20 safe-PROVED notable theorems by Waldmeister critical-pair count.
   thvm = Method->"Waldmeister" + FormationFifo (the committed discrimination-tree
   engine, CP-selection byte-identical to WM).  Waldmeister = wmcli reference (banked).
   Plot log10(seconds) with second-valued ticks to dodge the
   ScalingFunctions->"Log" + BarOrigin->Left layout bug. *)

tsv = Import["tools/baselines/top20_formationfifo_timing.tsv", "TSV"];
header = First[tsv];
rows = Rest[tsv];
idx = AssociationThread[header -> Range[Length[header]]];
getN[s_] := Quiet @ Module[{v = ToExpression[s]}, If[ NumberQ[v], N[v], Missing[]]];

data = Table[
   Module[{thm, cps, thvm, wm},
    thm = row[[idx["theorem"]]]; cps = getN @ row[[idx["cp_count"]]];
    thvm = getN @ row[[idx["thvm_s"]]]; wm = getN @ row[[idx["wm_s"]]];
    <|"thm" -> thm, "cps" -> cps, "thvm" -> thvm, "wmcli" -> wm|>],
   {row, rows}];
data = Take[ReverseSortBy[data, #cps &], UpTo[20]];

shortLabel[s_] := StringReplace[s,
   {"CombinatorAxioms__" -> "Combinator ", "WolframAxioms__" -> "Wolfram ",
    "BooleanAxioms__" -> "Boolean ", "CommutativeRingWithIdentityAxioms__" -> "CommRing1 ",
    "CommutativeRingAxioms__" -> "CommRing ", "RingWithIdentityAxioms__" -> "Ring1 ",
    "RingAxioms__" -> "Ring ", "McCuneAxioms__" -> "McCune ", "MeredithAxioms__" -> "Meredith ",
    "HillmanAxioms__" -> "Hillman ", "HuntingtonAxioms__" -> "Huntington ",
    "AbelianMcCuneAxioms__" -> "AbelMcCune ", "__" -> " ", "_" -> " "}];

(* log10 of seconds, floored so the sub-ms wmcli bars are visible *)
floor = 0.0008;            (* 0.8 ms display floor *)
lg[x_] := Log10[Max[x, floor]];
labels = (shortLabel[#thm] <> "  (cp=" <> ToString[Round[#cps]] <> ")") & /@ data;
thvmL = lg[#thvm] & /@ data;
wmcliL = lg[#wmcli] & /@ data;

(* reverse so largest-cps is on top *)
labels = Reverse @ labels; thvmL = Reverse @ thvmL; wmcliL = Reverse @ wmcliL;
pairs = Transpose[{thvmL, wmcliL}];

blue = LightDarkSwitched[RGBColor[0.16, 0.50, 0.73], RGBColor[0.36, 0.68, 0.91]];
red = LightDarkSwitched[RGBColor[0.78, 0.24, 0.20], RGBColor[0.93, 0.42, 0.38]];

secTicks = {{lg[0.001], "1 ms"}, {lg[0.003], "3 ms"}, {lg[0.01], "10 ms"},
   {lg[0.03], "30 ms"}, {lg[0.1], "0.1 s"}, {lg[0.3], "0.3 s"}, {lg[1], "1 s"}};

chart = BarChart[pairs,
   BarOrigin -> Left,
   ChartLabels -> {Placed[labels, Axis], None},
   ChartLegends -> Placed[{"thvm  (Method->\"Waldmeister\" + FormationFifo; CP-selection byte-identical to WM)",
       "Waldmeister  (wmcli reference, banked wall)"}, Below],
   ChartStyle -> {blue, red},
   BarSpacing -> {0.25, 1.1},
   Ticks -> {None, secTicks},
   GridLines -> {First /@ secTicks, None},
   GridLinesStyle -> Directive[GrayLevel[0.5, 0.35], Dashed],
   PlotLabel -> Style[
     "thvm (FormationFifo parity preset) vs Waldmeister (wmcli) wall time\n" <>
      "top-20 notable theorems by Waldmeister critical-pair count  (log scale; all PROVED, CP count = WM exactly)",
     14, Bold],
   ImageSize -> {1050, 1150},
   LabelStyle -> 11,
   AspectRatio -> 1.25,
   PlotRangePadding -> {{Scaled[0.02], Scaled[0.08]}, {Scaled[0.01], Scaled[0.03]}}];

Export["tools/baselines/plots/top20_thvm_formationfifo_vs_waldmeister.png", chart,
   ImageResolution -> 130, Background -> GrayLevel[0.12]];
Print["exported tools/baselines/plots/top20_thvm_formationfifo_vs_waldmeister.png  rows=", Length[data]];
