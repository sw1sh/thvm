(* ::Package:: *)
(* Profile.wl -- structured runtime profile snapshot for grad +
   materialization debugging.

   TProfile[]            captures the current heap / tensor / kernel
                          state into an Association.
   TProfile[label]       same, with a "Label" key set to `label`.
   TProfileTable[ps]     renders a Tabular comparing several profiles
                          (e.g. successive TGrad rounds).
   TProfileGrowth[ps]    computes round-over-round growth ratios for
                          headline metrics.
   TProfilePlot[ps, key] line plot of `key` across profile snapshots.

   Profile data shape:

   <| "Label"           -> _String,
      "HeapCells"       -> _Integer,
      "CellsByTag"      -> <|tag -> count, ...|>,
      "Tensors"         -> _Integer,
      "DistinctBufs"    -> _Integer,
      "NonContigAliases"-> _Integer,
      "Kernels"         -> _Integer,
      "MaxKernelInputs" -> _Integer,
      "MaxKernelOps"    -> _Integer,
      "PerKernel"       -> {<|"Kid"->_, "Inputs"->_, "Ops"->_, "Numel"->_|>, ...},
      "ITRS"            -> _Integer
   |>

   Use to spot allocation leaks across chain-rule rounds, kernel
   input/op explosions, alias bloat, and round-over-round bench
   growth without scrolling raw heap dumps. *)

BeginPackage["THVMLink`"];

TProfile::usage = "TProfile[] returns an Association snapshotting the current runtime state (heap cells by tag, tensors, kernels, ITRS).  TProfile[label] sets the \"Label\" key.";

TProfileTable::usage = "TProfileTable[{profile_1, ..., profile_n}] renders a Tabular comparing the headline metrics across snapshots.";

TProfileGrowth::usage = "TProfileGrowth[{profile_1, ..., profile_n}] returns an Association of round-over-round growth ratios for HeapCells, Tensors, Kernels, MaxKernelInputs, MaxKernelOps, and per-tag cell counts.";

TProfilePlot::usage = "TProfilePlot[{profile_1, ..., profile_n}, key] line-plots metric `key` across snapshots.  Common keys: \"HeapCells\", \"Kernels\", \"MaxKernelInputs\".";

TProfileReport::usage = "TProfileReport[profile] returns a short Column rendering of headline metrics + per-tag cell counts + per-kernel inputs/ops.  TProfileReport[{p_1, ..., p_n}] formats a sequence of snapshots side-by-side, showing growth ratios.  Use for stdout / notebook display.";

(* Forward-declare bridge symbols owned by THVMLink.wl. *)
{THeapPos, THeapRead, TItrs, TTagName, TTermTag,
 TKernelTable, TTensTable, decodeKernelInfo};

Begin["`Private`"];

profileCellsByTag[] := Module[{n, byTag = <||>, t, tag},
    n = THeapPos[];
    Do[
        t = THeapRead[i];
        tag = TTagName[TTermTag[t]];
        byTag[tag] = Lookup[byTag, tag, 0] + 1,
        {i, 0, n - 1}];
    byTag
]

profilePerKernel[] := Module[{kt, info},
    kt = TKernelTable[];
    Table[
        info = THVMLink`Private`decodeKernelInfo[k][[1]];
        <| "Kid"    -> k,
           "Inputs" -> info["InputCount"],
           "Ops"    -> info["OpCount"],
           "Numel"  -> info["OutputNumel"]
        |>,
        {k, 1, Length[kt]}]
]

profileTensorStats[] := Module[{tt},
    tt = TTensTable[];
    {Length[tt],
     Length @ Union[#[[2]] & /@ tt],            (* distinct buf_ids *)
     Count[#[[5]] & /@ tt, 0]}                  (* non-contig aliases *)
]

TProfile[] := TProfile[""]
TProfile[label_String] := Module[{
    perK, tStats, maxIn, maxOps
},
    perK = profilePerKernel[];
    tStats = profileTensorStats[];
    maxIn  = If[ Length[perK] > 0, Max[#["Inputs"] & /@ perK], 0];
    maxOps = If[ Length[perK] > 0, Max[#["Ops"]    & /@ perK], 0];
    <| "Label"            -> label,
       "HeapCells"        -> THeapPos[],
       "CellsByTag"       -> profileCellsByTag[],
       "Tensors"          -> tStats[[1]],
       "DistinctBufs"     -> tStats[[2]],
       "NonContigAliases" -> tStats[[3]],
       "Kernels"          -> Length[perK],
       "MaxKernelInputs"  -> maxIn,
       "MaxKernelOps"     -> maxOps,
       "PerKernel"        -> perK,
       "ITRS"             -> TItrs[]
    |>
]

(* Headline metrics shown in TProfileTable, in display order. *)
$tabularKeys = {"Label", "HeapCells", "Tensors", "DistinctBufs",
                "NonContigAliases", "Kernels", "MaxKernelInputs",
                "MaxKernelOps", "ITRS"};

TProfileTable[ps_List] := Module[{rows, allTags, tagRows},
    rows = Table[
        AssociationMap[ p[#] &, $tabularKeys ],
        {p, ps}];
    (* Per-tag breakdown columns: union of all tags seen. *)
    allTags = Union @ Flatten[Keys[#["CellsByTag"]] & /@ ps];
    tagRows = Table[
        AssociationThread[ allTags, Lookup[p["CellsByTag"], #, 0] & /@ allTags ],
        {p, ps}];
    Tabular[ MapThread[ Join, {rows, tagRows} ] ]
]

TProfileGrowth[ps_List] := Module[{
    keys = {"HeapCells", "Tensors", "DistinctBufs", "Kernels",
            "MaxKernelInputs", "MaxKernelOps"},
    pairs, ratios
},
    If[ Length[ps] < 2, Return[<||>] ];
    pairs = Partition[ps, 2, 1];
    ratios = AssociationMap[
        Function[k,
            Map[ Function[pp, ratio[ pp[[1]][k], pp[[2]][k] ]], pairs ]],
        keys];
    ratios
]

ratio[a_, b_] := If[ a == 0, "n/a", Round[N[b / a], 0.01]]

formatProfileLine[label_, p_] := label <> ": cells=" <> ToString[p["HeapCells"]] <>
    " (" <> StringRiffle[
        Table[ k <> "=" <> ToString[p["CellsByTag"][k]],
            {k, SortBy[Keys[p["CellsByTag"]], -p["CellsByTag"][#] &]}],
        " "] <> ")\n" <>
    "  tensors=" <> ToString[p["Tensors"]] <>
    " distinct_bufs=" <> ToString[p["DistinctBufs"]] <>
    " aliases=" <> ToString[p["NonContigAliases"]] <>
    " kernels=" <> ToString[p["Kernels"]] <>
    " max_inputs=" <> ToString[p["MaxKernelInputs"]] <>
    " max_ops=" <> ToString[p["MaxKernelOps"]] <>
    " ITRS=" <> ToString[p["ITRS"]]

TProfileReport[p_Association] := formatProfileLine[p["Label"], p]
TProfileReport[ps_List] := StringJoin[
    Riffle[
        Table[
            If[ i == 1,
                formatProfileLine[ps[[i]]["Label"], ps[[i]]],
                Module[{prev = ps[[i-1]], cur = ps[[i]]},
                    formatProfileLine[ps[[i]]["Label"], cur] <>
                    "  growth: cells x" <> ToString[ratio[prev["HeapCells"], cur["HeapCells"]]] <>
                    " kernels x" <> ToString[ratio[prev["Kernels"], cur["Kernels"]]] <>
                    " max_inputs x" <> ToString[ratio[prev["MaxKernelInputs"], cur["MaxKernelInputs"]]] <>
                    " max_ops x" <> ToString[ratio[prev["MaxKernelOps"], cur["MaxKernelOps"]]]]],
            {i, 1, Length[ps]}],
        "\n"]]

TProfilePlot[ps_List, key_] := Module[{xs, ys},
    xs = Range[Length[ps]];
    ys = (#[key]) & /@ ps;
    ListLinePlot[ Transpose[{xs, ys}],
        PlotMarkers -> Automatic,
        PlotLabel -> key,
        AxesLabel -> {"snapshot index", key} ]
]

End[];

EndPackage[];
