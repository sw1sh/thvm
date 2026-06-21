#!/usr/bin/env wolframscript
(* Performance-gap charts for the Waldmeister tutorial.

   Three provers on the same equational goals:
     - thvm  Method -> "Waldmeister"   (in-process C engine, discrimination-tree retrieval)
     - WL    FindEquationalProof        (FEQ; banked tools/baselines/wl_notable_theorems.tsv)
     - wmcli Waldmeister CLI            (banked tools/baselines/wm_cli_sweep.tsv col wm_wall_s)

   The thvm column on the two Sheffer-class rows is MEASURED LIVE (2026-06-22,
   discrimination-tree build); the rest of the thvm column is banked
   tools/baselines/thvm_notable_theorems.tsv.  wmcli is ALWAYS banked: re-running
   wmcli on a Sheffer/Wolfram theorem diverges to ~20GB RSS, so its wall times
   come from the recorded sweep only.

   Two PNGs into tools/baselines/plots/:
     waldmeister_sheffer_headtohead.png  -- the byte-identical Sheffer pair (thvm live vs wmcli banked)
     waldmeister_thvm_vs_feq.png         -- thvm "Waldmeister" vs WL FindEquationalProof (fair WL-to-WL wall)
*)

outDir = "tools/baselines/plots";
If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];

readTsv[path_] := Module[{lines, header, rows},
    lines = Select[StringSplit[Import[path, "Text"], "\n"], # =!= "" &];
    header = StringSplit[First @ lines, "\t"];
    rows = StringSplit[#, "\t"] & /@ Rest[lines];
    AssociationThread[header -> #] & /@ rows];

num[s_] := Quiet @ Module[{v = Check[ToExpression[s], $Failed]}, If[ NumericQ[v], N[v], Missing[]]];

thvm = readTsv["tools/baselines/thvm_notable_theorems.tsv"];
wl   = readTsv["tools/baselines/wl_notable_theorems.tsv"];
wm   = readTsv["tools/baselines/wm_cli_sweep.tsv"];

thvmSec[thy_, thm_] := SelectFirst[thvm,
    #["theory"] === thy && #["thm"] === thm &, <||>] /. a_Association :> num[a["seconds"]];
wlSec[thy_, thm_] := SelectFirst[wl,
    #["theory"] === thy && #["thm"] === thm &, <||>] /. a_Association :> num[a["seconds"]];
wmSec[thy_, thm_] := SelectFirst[wm,
    #["theory_theorem"] === thy <> "__" <> thm &, <||>] /. a_Association :> num[a["wm_wall_s"]];

blue = RGBColor[0.20, 0.45, 0.85];
orange = RGBColor[0.85, 0.45, 0.20];
red = RGBColor[0.78, 0.24, 0.20];

(* ---- Chart 1: Sheffer-class head-to-head (thvm live vs wmcli banked) ---- *)
(* thvm numbers MEASURED LIVE 2026-06-22 (full TFindProof wall, ProofObject lift
   included); proofs are byte-identical to wmcli's CP-selection sequence. *)
sheffer = {
    {"Wolfram /\nCommutativity", 5.2, 2.352},
    {"Wolfram /\nDoubleNegation", 3.59, 2.319}};
shefferChart = BarChart[sheffer[[All, {2, 3}]],
    ChartLabels -> {sheffer[[All, 1]], None},
    ChartLegends -> {"thvm  Method -> \"Waldmeister\"  (live, in-process)",
        "wmcli  Waldmeister CLI  (banked wall)"},
    ChartStyle -> {blue, red},
    BarSpacing -> {0.3, 1.0},
    AspectRatio -> 0.55,
    ImageSize -> 760,
    PlotLabel -> Style[
        "Sheffer-class goals: thvm Method -> \"Waldmeister\" vs wmcli\n"
            <> "byte-identical proofs; thvm live, wmcli banked (re-running wmcli here is ~20GB RSS)",
        Bold, 13],
    FrameLabel -> {None, "wall seconds (lower = faster)"},
    Frame -> {{True, False}, {True, False}},
    LabelStyle -> Directive[FontFamily -> "Helvetica", 11],
    GridLines -> {None, Automatic},
    GridLinesStyle -> Directive[GrayLevel[0.5, 0.3], Dashed]];
Export[FileNameJoin[{outDir, "waldmeister_sheffer_headtohead.png"}], shefferChart,
    ImageResolution -> 144];

(* ---- Chart 2: thvm "Waldmeister" vs WL FindEquationalProof, fair WL-to-WL ----
   Both are full in-process WL calls (engine + ProofObject lift), so the wall
   times are directly comparable.  wmcli is NOT on this chart: its recorded
   sweep is BARE-ENGINE sub-millisecond on these tiny goals (no WL lift), so a
   bar comparison would misread; wmcli appears in the doc's banked table and on
   the Sheffer head-to-head instead.  Both banked. *)
rows = {
    {"AbelianGroup", "InverseOfInverse"},
    {"AbelianGroup", "InverseOfComposite"},
    {"AbelianGroup", "ImpliesMcCuneAxioms"},
    {"AbelianMcCune", "Commutativity"},
    {"Boolean", "DoubleNegation"},
    {"Boolean", "AndIdempotence"}};
thyFull = <|"AbelianGroup" -> "AbelianGroupAxioms",
    "AbelianMcCune" -> "AbelianMcCuneAxioms", "Boolean" -> "BooleanAxioms"|>;
recs = Table[
    Module[{thy = thyFull[r[[1]]], thm = r[[2]]},
        <|"label" -> r[[1]] <> " /\n" <> thm,
          "thvm" -> thvmSec[thy, thm],
          "wl" -> wlSec[thy, thm]|>],
    {r, rows}];
recs = Select[recs, FreeQ[#, _Missing] &];
pairs = ({#["thvm"], #["wl"]} & /@ recs);
gapChart = BarChart[pairs,
    ChartLabels -> {#["label"] & /@ recs, None},
    ChartLegends -> {"thvm  Method -> \"Waldmeister\"", "WL  FindEquationalProof"},
    ChartStyle -> {blue, orange},
    BarSpacing -> {0.25, 0.9},
    AspectRatio -> 0.5,
    ImageSize -> 900,
    PlotLabel -> Style[
        "thvm Method -> \"Waldmeister\" vs WL FindEquationalProof (both banked, full WL wall)\n"
            <> "bounded group / Boolean theorems both engines prove  (lower = faster)",
        Bold, 13],
    FrameLabel -> {None, "wall seconds (lower = faster)"},
    Frame -> {{True, False}, {True, False}},
    GridLines -> {None, Automatic},
    GridLinesStyle -> Directive[GrayLevel[0.5, 0.3], Dashed],
    LabelStyle -> Directive[FontFamily -> "Helvetica", 10]];
Export[FileNameJoin[{outDir, "waldmeister_thvm_vs_feq.png"}], gapChart,
    ImageResolution -> 144];

WriteString["stdout", "wrote ", outDir, "/waldmeister_sheffer_headtohead.png\n"];
WriteString["stdout", "wrote ", outDir, "/waldmeister_thvm_vs_feq.png  (", Length[recs], " rows)\n"];
