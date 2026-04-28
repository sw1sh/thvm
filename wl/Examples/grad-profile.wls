#!/usr/bin/env wolframscript
PacletDirectoryLoad["wl/THVMLink"]
Get["THVMLink`"]

profile[label_String] := Module[{
    kt, tt, byTag, totalCells, distinctBufs, aliasCount,
    inputCounts, opCounts, outNumels, maxIn, maxOps
},
    WriteString["stdout", "\n=== profile: " <> label <> " ===\n"];
    totalCells = THeapPos[];
    byTag = <||>;
    Do[
        Module[{tag = TTagName[TTermTag[THeapRead[i]]]},
            byTag[tag] = Lookup[byTag, tag, 0] + 1],
        {i, 0, totalCells - 1}];
    WriteString["stdout", "heap_cells=" <> ToString[totalCells]];
    Do[ WriteString["stdout", " " <> kv[[1]] <> "=" <> ToString[kv[[2]]]],
        {kv, SortBy[Normal[byTag], -#[[2]] &]}];
    WriteString["stdout", "\n"];
    tt = TTensTable[];
    distinctBufs = Length @ Union[#[[2]] & /@ tt];
    aliasCount = Count[#[[5]] & /@ tt, 0];
    WriteString["stdout", "tensors=" <> ToString[Length[tt]] <>
        " distinct_bufs=" <> ToString[distinctBufs] <>
        " noncontig_aliases=" <> ToString[aliasCount] <> "\n"];
    kt = TKernelTable[];
    inputCounts = #[[1]] & /@ kt;
    opCounts = Table[ Module[{i = decodeKernelInfo[k][[1]]}, i["OpCount"]], {k, 1, Length[kt]}];
    outNumels = #[[6]] & /@ kt;
    maxIn = If[ Length[inputCounts] > 0, Max[inputCounts], 0];
    maxOps = If[ Length[opCounts] > 0, Max[opCounts], 0];
    WriteString["stdout", "kernels=" <> ToString[Length[kt]] <>
        " max_inputs=" <> ToString[maxIn] <>
        " max_ops=" <> ToString[maxOps] <> "\n"];
    Do[ WriteString["stdout", "  k" <> ToString[k] <>
        " inputs=" <> ToString[inputCounts[[k]]] <>
        " ops=" <> ToString[opCounts[[k]]] <>
        " out_numel=" <> ToString[outNumels[[k]]] <> "\n"], {k, 1, Length[kt]}];
    WriteString["stdout", "ITRS=" <> ToString[TItrs[]] <> "\n"]
];

Do[
    Module[{a, y, seed, label, gnest},
        TInit[];
        a = TTensorCreate[NumericArray[{2.0, 3.0}, "Real32"]];
        y = TUOpMul[TUOpMul[TUOpMul[a, a], a], a];
        seed = TUOpExpand[TUOpConst[1.0, "f32"], {2}];
        gnest = Nest[ TGrad[#, a, seed] &, y, k];
        TRealize[gnest];
        label = "a^4 round=" <> ToString[k];
        profile[label]],
    {k, 1, 4}];
