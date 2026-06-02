#!/usr/bin/env wolframscript
SetDirectory["/Users/swish/src/thvm"];
print[a___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {a}], "\n"];

(* Read the 3 parity TSVs and build a grouped construct-count
   bar chart per theorem with one bar per (preset, cli) pair for
   each of Waldmeister / Vampire / Twee. *)

loadTsv[path_String] := Block[{rows},
    rows = Import[path, "TSV"];
    AssociationThread[First[rows] -> #] & /@ Rest[rows]
];

wm     = loadTsv["tools/baselines/parity_wm_wmcli.tsv"];
vamp   = loadTsv["tools/baselines/parity_vampire.tsv"];
twee   = loadTsv["tools/baselines/parity_twee.tsv"];

cases = (StringJoin[#["theory"], "/", #["theorem"]]) & /@ wm;

asNum[s_] := Quiet @ If[ StringQ[s], ToExpression[s], s] /. _Missing | _ToExpression -> Missing[];

pickPair[rows_, label_, idx_] := Block[
    {row, p, c},
    row = rows[[idx]];
    p = asNum[row["preset_constructs"]];
    c = asNum[row[label]];
    {p, c}
];

(* Two horizontally-stacked panels: (1) preset vs CLI absolute
   bar chart per theorem across all 3 provers; (2) signed diff
   (CLI - preset). *)
cliKeys = {"wmcli_constructs", "vampire_constructs", "twee_constructs"};
allRows = {wm, vamp, twee};
proverLabels = {"Waldmeister", "Vampire", "Twee"};

constructPairs = Table[
    pickPair[allRows[[k]], cliKeys[[k]], i],
    {i, Length[cases]}, {k, 3}
];

absoluteData = Table[
    Block[{pair = constructPairs[[i, k]]},
        {pair[[1]] /. Missing[] -> 0,
         pair[[2]] /. Missing[] -> 0}],
    {i, Length[cases]}, {k, 3}
];

diffData = Table[
    Block[{p, c, pair = constructPairs[[i, k]]},
        p = pair[[1]];
        c = pair[[2]];
        If[ MissingQ[p] || MissingQ[c],
            0,
            c - p
        ]],
    {i, Length[cases]}, {k, 3}
];

(* Stacked bar layout: each prover gets a (preset, cli) pair
   side-by-side; row labels are the theorem cases. *)
shortLabel = StringSplit[#, "Axioms/"][[-1]] & /@ cases;
print["cases: ", shortLabel];

(* One panel: per-theorem grouped bars of (preset, cli) construct
   counts, three provers side by side.  Use a flat 2-level dataset
   shape: outer = one row per case, inner = 6 numbers
   {wm_p, wm_c, vamp_p, vamp_c, twee_p, twee_c}. *)
flatRow[i_] := Flatten @ Table[
    Block[{pair = constructPairs[[i, k]]},
        {pair[[1]] /. Missing[] -> 0,
         pair[[2]] /. Missing[] -> 0}],
    {k, 3}
];

absRows = Table[flatRow[i], {i, Length[cases]}];

style[c_] := LightDarkSwitched[{c, Darker[c, 0.25]}];

bcAbs = BarChart[
    absRows,
    ChartLabels -> {shortLabel, None},
    PlotLabel -> "ATP parity: preset vs CLI (3 prover pairs/case)",
    ChartLegends -> Placed[
        {"WM preset", "WM cli",
         "Vamp preset", "Vamp cli",
         "Twee preset", "Twee cli"}, Top],
    AxesLabel -> {None, "# constructs"},
    ChartStyle -> {
        Opacity[0.45, style[StandardBlue]],   style[StandardBlue],
        Opacity[0.45, style[StandardRed]],    style[StandardRed],
        Opacity[0.45, style[StandardOrange]], style[StandardOrange]
    },
    ImageSize -> 1000,
    BarSpacing -> {Small, Medium},
    PlotTheme -> "Detailed"
];

bcDiff = BarChart[
    diffData,
    ChartLabels -> {shortLabel, None},
    PlotLabel -> "delta = CLI - preset (positive: CLI longer; 0 = byte parity)",
    AxesLabel -> {None, "delta"},
    ChartStyle -> {
        style[StandardBlue],
        style[StandardRed],
        style[StandardOrange]
    },
    ChartLegends -> Placed[
        {"Waldmeister", "Vampire", "Twee"}, Top],
    ImageSize -> 1000,
    BarSpacing -> {Small, Medium},
    PlotTheme -> "Detailed"
];

out = Column[{bcAbs, bcDiff}, Spacings -> 1];
Export["tools/baselines/plots/atp_parity.png", out,
    ImageResolution -> 160];
print["wrote tools/baselines/plots/atp_parity.png"];
