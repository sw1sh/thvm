#!/usr/bin/env wolframscript
(* Three-way bar charts: thvm vs WL FindEquationalProof vs Vampire on
   the NotableTheorems baseline.  Reads:
     tools/baselines/thvm_notable_theorems.tsv     (theory, thm, status, seconds, proofLength)
     tools/baselines/wl_notable_theorems.tsv       (same schema)
     tools/baselines/vampire_notable_theorems.tsv  (theory, thm, conjunct, status, seconds, winningStrategy, proofClauses)
   Joins on (theory, thm), filters to rows where ALL THREE prove the
   theorem (multi-conjunct vampire entries get summed across conjuncts).
   Emits PNGs to tools/baselines/plots/.

   Axis labels are rotated 90 degrees (orthogonal) so the long
   `Theory/Thm` names fit.  Reports per-prover total + per-pair
   intersection counts so the reader can see the coverage gaps.
*)

thvmPath = "tools/baselines/thvm_notable_theorems.tsv";
wlPath   = "tools/baselines/wl_notable_theorems.tsv";
vampPath = "tools/baselines/vampire_notable_theorems.tsv";
outDir   = "tools/baselines/plots";
If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];

readTsv[path_] := Module[{lines, header, rows},
    lines = Select[StringSplit[Import[path, "Text"], "\n"], # =!= "" &];
    header = StringSplit[First @ lines, "\t"];
    rows   = StringSplit[#, "\t"] & /@ Rest[lines];
    {header, AssociationThread[header -> #] & /@ rows}];

parseN[s_String] := Module[{v = Quiet @ Check[ToExpression[s], $Failed]},
    If[ NumericQ[v], N[v], 0.0]];
parseN[_] := 0.0;

{_, thvmRows} = readTsv[thvmPath];
{_, wlRows}   = readTsv[wlPath];
{_, vampRowsRaw} = readTsv[vampPath];

(* Vampire: collapse multiple-conjunct rows into one per (theory, thm).
   sum seconds + proofClauses; status = PROVED iff every conjunct PROVED. *)
collapseVampire[rows_] := Module[{grouped},
    grouped = GroupBy[rows, {#["theory"], #["thm"]} &];
    KeyValueMap[Function[{key, group},
        Module[{secs, pls, statuses},
            secs = Total[parseN[#["seconds"]] & /@ group];
            pls = Total[parseN[#["proofClauses"]] & /@ group];
            statuses = #["status"] & /@ group;
            <|"theory" -> key[[1]], "thm" -> key[[2]],
              "status" -> If[ AllTrue[statuses, # === "PROVED" &],
                              "PROVED", First @ statuses],
              "seconds" -> ToString @ Round[secs, 0.01],
              "proofLength" -> ToString @ Round[pls]|>]],
        grouped]];
vampRows = collapseVampire[vampRowsRaw];

keyOf[r_] := {r["theory"], r["thm"]};
thvmByKey = Association[(keyOf[#] -> #) & /@ thvmRows];
wlByKey   = Association[(keyOf[#] -> #) & /@ wlRows];
vampByKey = Association[(keyOf[#] -> #) & /@ vampRows];

provedKeys[m_] := Select[Keys[m], m[#]["status"] === "PROVED" &];

thvmProved = provedKeys[thvmByKey];
wlProved   = provedKeys[wlByKey];
vampProved = provedKeys[vampByKey];

WriteString["stdout", "\n=== Per-prover totals ===\n"];
WriteString["stdout", "thvm   : ", Length[thvmRows], " rows, ", Length[thvmProved], " PROVED\n"];
WriteString["stdout", "WL     : ", Length[wlRows],   " rows, ", Length[wlProved],   " PROVED\n"];
WriteString["stdout", "Vampire: ", Length[vampRows], " rows, ", Length[vampProved], " PROVED\n"];

WriteString["stdout", "\n=== Pairwise PROVED-intersections ===\n"];
sectAB[a_, b_] := Length @ Intersection[a, b];
WriteString["stdout", "thvm  ∩ WL  : ", sectAB[thvmProved, wlProved], "\n"];
WriteString["stdout", "thvm  ∩ Vamp: ", sectAB[thvmProved, vampProved], "\n"];
WriteString["stdout", "WL    ∩ Vamp: ", sectAB[wlProved, vampProved], "\n"];

allThree = Intersection[thvmProved, wlProved, vampProved];
WriteString["stdout", "thvm ∩ WL ∩ Vamp : ", Length[allThree], "\n"];

WriteString["stdout", "\n=== Gaps (one prover misses what others crack) ===\n"];
WriteString["stdout", "WL ∪ Vamp but not thvm : ",
    Length @ Complement[Union[wlProved, vampProved], thvmProved], "\n"];
WriteString["stdout", "Vamp only (neither thvm nor WL): ",
    Length @ Complement[vampProved, Union[thvmProved, wlProved]], "\n"];

(* Build records for the all-three-PROVED intersection. *)
recOf[k_] := <|
    "thvmSec" -> parseN @ thvmByKey[k]["seconds"],
    "wlSec"   -> parseN @ wlByKey[k]["seconds"],
    "vampSec" -> parseN @ vampByKey[k]["seconds"],
    "thvmPL"  -> parseN @ thvmByKey[k]["proofLength"],
    "wlPL"    -> parseN @ wlByKey[k]["proofLength"],
    "vampPL"  -> parseN @ vampByKey[k]["proofLength"]
|>;
recs = Association[(# -> recOf[#]) & /@ allThree];

labelOf[{theory_, thm_}] := theory <> "/" <> thm;

makeBar[keys_, getters_, names_, colors_, title_, ylabel_] := Module[
    {labels, paired},
    labels = labelOf /@ keys;
    paired = Map[Function[k, Map[#[recs[k]] &, getters]], keys];
    BarChart[paired,
        ChartLabels -> {Placed[Rotate[#, Pi / 2] & /@ labels, Axis], None},
        ChartLegends -> names,
        ChartStyle -> colors,
        AspectRatio -> 0.4,
        ImageSize -> {Max[1200, Length[keys] * 24], 600},
        PlotLabel -> Style[title, Bold, 14],
        FrameLabel -> {None, ylabel},
        Frame -> {{True, False}, {True, False}},
        LabelStyle -> Directive[FontFamily -> "Helvetica", 9]]];

sortByWL[ks_, getter_] := SortBy[ks, -getter[recs[#]] &];

styleSet = {RGBColor[0.20, 0.45, 0.85], RGBColor[0.85, 0.45, 0.20], RGBColor[0.30, 0.70, 0.30]};
names    = {"thvm", "WL FindEquationalProof", "Vampire"};

(* Time chart: sort by WL seconds desc. *)
ksByTime = sortByWL[allThree, #["wlSec"] &];
timeTitle = StringJoin["Time (s) -- ",
    ToString[Length @ allThree],
    " NotableTheorems where ALL THREE prove"];
timeChart = makeBar[ksByTime,
    {#["thvmSec"] &, #["wlSec"] &, #["vampSec"] &},
    names, styleSet, timeTitle, "seconds (lower = better)"];

(* Proof-length chart. *)
ksByPL = sortByWL[allThree, #["wlPL"] &];
plTitle = StringJoin["Proof length (steps) -- ",
    ToString[Length @ allThree],
    " NotableTheorems where ALL THREE prove"];
plChart = makeBar[ksByPL,
    {#["thvmPL"] &, #["wlPL"] &, #["vampPL"] &},
    names, styleSet, plTitle, "steps (lower = more concise)"];

Export[FileNameJoin[{outDir, "thvm_vs_wl_vs_vamp_time.png"}], timeChart];
Export[FileNameJoin[{outDir, "thvm_vs_wl_vs_vamp_proof_length.png"}], plChart];

WriteString["stdout", "\nWrote ",
    outDir, "/thvm_vs_wl_vs_vamp_time.png and ",
    outDir, "/thvm_vs_wl_vs_vamp_proof_length.png\n"];

(* Summary stats over the intersection. *)
sumOf[g_] := Total[g[recs[#]] & /@ allThree];
medOf[g_] := Median[g[recs[#]] & /@ allThree];
WriteString["stdout", "\n=== Totals (over the ", Length @ allThree, "-theorem intersection) ===\n"];
WriteString["stdout", "Time -- thvm: ", N @ Round[sumOf[#["thvmSec"] &], 0.1],
    "s   WL: ", N @ Round[sumOf[#["wlSec"] &], 0.1],
    "s   Vamp: ", N @ Round[sumOf[#["vampSec"] &], 0.1], "s\n"];
WriteString["stdout", "PL   -- thvm: ", Round @ sumOf[#["thvmPL"] &],
    "   WL: ", Round @ sumOf[#["wlPL"] &],
    "   Vamp: ", Round @ sumOf[#["vampPL"] &], "\n"];
WriteString["stdout", "\n=== Medians ===\n"];
WriteString["stdout", "Time -- thvm: ", N @ Round[medOf[#["thvmSec"] &], 0.01],
    "s   WL: ", N @ Round[medOf[#["wlSec"] &], 0.01],
    "s   Vamp: ", N @ Round[medOf[#["vampSec"] &], 0.01], "s\n"];
WriteString["stdout", "PL   -- thvm: ", Round @ medOf[#["thvmPL"] &],
    "   WL: ", Round @ medOf[#["wlPL"] &],
    "   Vamp: ", Round @ medOf[#["vampPL"] &], "\n"];
