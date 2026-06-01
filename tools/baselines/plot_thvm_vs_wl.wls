#!/usr/bin/env wolframscript
(* Side-by-side comparison plots of thvm vs WL on the
   NotableTheorems baseline.  Reads:
     tools/baselines/thvm_notable_theorems.tsv
     tools/baselines/wl_notable_theorems.tsv
   Joins on (theory, thm), filters to rows where BOTH PROVED, and
   emits two bar charts:
     tools/baselines/plots/thvm_vs_wl_time.png
     tools/baselines/plots/thvm_vs_wl_proof_length.png
*)

thvmPath = "tools/baselines/thvm_notable_theorems.tsv";
wlPath   = "tools/baselines/wl_notable_theorems.tsv";
outDir   = "tools/baselines/plots";
If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];

readTsv[path_] := Module[{lines, header, rows},
    lines = Select[StringSplit[Import[path, "Text"], "\n"], # =!= "" &];
    header = StringSplit[First @ lines, "\t"];
    rows   = StringSplit[#, "\t"] & /@ Rest[lines];
    AssociationThread[header -> #] & /@ rows];

thvmRows = readTsv[thvmPath];
wlRows   = readTsv[wlPath];

(* Joinable keys: (theory, thm) -> assoc. *)
keyOf[r_] := {r["theory"], r["thm"]};
thvmByKey = Association[(keyOf[#] -> #) & /@ thvmRows];
wlByKey   = Association[(keyOf[#] -> #) & /@ wlRows];

(* Filter to rows where BOTH are PROVED. *)
provedQ[k_] := AllTrue[
    {KeyExistsQ[wlByKey, k],
     thvmByKey[k]["status"] === "PROVED",
     wlByKey[k]["status"] === "PROVED"},
    TrueQ];
sharedKeys = Select[Keys @ thvmByKey, provedQ];

WriteString["stdout", "thvm rows: ", Length @ thvmRows,
    "  WL rows: ", Length @ wlRows,
    "  shared+both-PROVED: ", Length @ sharedKeys, "\n"];

(* For each shared key, pull (thvm_secs, wl_secs, thvm_pl, wl_pl). *)
parseN[s_String] := Module[{v},
    v = Quiet @ Check[ToExpression[s], $Failed];
    If[ NumericQ[v], N[v], 0.0]];
parseN[_] := 0.0;

records = (#1 -> <|
        "thvmSec" -> parseN @ thvmByKey[#1]["seconds"],
        "wlSec"   -> parseN @ wlByKey[#1]["seconds"],
        "thvmPL"  -> parseN @ thvmByKey[#1]["proofLength"],
        "wlPL"    -> parseN @ wlByKey[#1]["proofLength"]
    |>) & /@ sharedKeys;
recs = Association @ records;

(* Sort by WL time descending so the most-expensive cases are at top. *)
sortedKeys = SortBy[Keys @ recs, -recs[#]["wlSec"] &];

(* Label format: short. *)
labelOf[{theory_, thm_}] := theory <> "/" <> thm;

makePairBarChart[keys_, getThvm_, getWl_, title_, xlabel_] := Module[
    {labels, paired},
    labels = labelOf /@ keys;
    paired = MapThread[{getThvm[recs[#1]], getWl[recs[#1]]} &, {keys}];
    BarChart[paired,
        ChartLabels -> {Placed[labels, Axis], None},
        ChartLegends -> {"thvm", "WL FindEquationalProof"},
        ChartStyle -> {RGBColor[0.20, 0.45, 0.85], RGBColor[0.85, 0.45, 0.20]},
        BarOrigin -> Left,
        AspectRatio -> Length[keys] / 28.0,
        ImageSize -> {800, Length[keys] * 14},
        PlotLabel -> Style[title, Bold, 14],
        FrameLabel -> {xlabel, None},
        Frame -> {{True, False}, {False, False}},
        LabelStyle -> Directive[FontFamily -> "Helvetica", 9]]];

timeTitle = StringJoin["Time (seconds) -- ",
    ToString @ Length[sortedKeys],
    " NotableTheorems where both prove"];
timeChart = makePairBarChart[sortedKeys,
    #["thvmSec"] &, #["wlSec"] &, timeTitle, "seconds (lower = better)"];

(* For proof-length, sort by WL proofLength descending. *)
sortedKeysPL = SortBy[Keys @ recs, -recs[#]["wlPL"] &];
plTitle = StringJoin["Proof length (MainSteps) -- ",
    ToString @ Length[sortedKeysPL],
    " NotableTheorems where both prove"];
plChart = makePairBarChart[sortedKeysPL,
    #["thvmPL"] &, #["wlPL"] &, plTitle, "MainSteps (lower = more concise)"];

Export[FileNameJoin[{outDir, "thvm_vs_wl_time.png"}], timeChart];
Export[FileNameJoin[{outDir, "thvm_vs_wl_proof_length.png"}], plChart];

WriteString["stdout", "Wrote ", outDir, "/thvm_vs_wl_time.png and ",
    outDir, "/thvm_vs_wl_proof_length.png\n"];

(* Summary stats. *)
thvmSecs = #["thvmSec"] & /@ Values[recs];
wlSecs   = #["wlSec"]   & /@ Values[recs];
thvmPLs  = #["thvmPL"]  & /@ Values[recs];
wlPLs    = #["wlPL"]    & /@ Values[recs];

WriteString["stdout", "\nTotal time -- thvm: ",
    Round[Total[thvmSecs], 0.1], "s   WL: ",
    Round[Total[wlSecs], 0.1], "s   ratio: ",
    Round[Total[thvmSecs] / Total[wlSecs], 0.01], "x\n"];
WriteString["stdout", "Total proof length -- thvm: ",
    Round @ Total[thvmPLs], "   WL: ",
    Round @ Total[wlPLs], "   ratio: ",
    Round[Total[thvmPLs] / Total[wlPLs], 0.01], "x\n"];
WriteString["stdout", "Median time     -- thvm: ",
    Round[Median @ thvmSecs, 0.01], "s   WL: ",
    Round[Median @ wlSecs, 0.01], "s\n"];
WriteString["stdout", "Median PL       -- thvm: ",
    Round @ Median @ thvmPLs, "    WL: ",
    Round @ Median @ wlPLs, "\n"];
