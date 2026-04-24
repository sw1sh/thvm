#!/usr/bin/env wolframscript
(* run.wls -- run and render every wl/Examples/<id>/term.wl
   (or just one of them).

   Run from the repo root:

       wolframscript -f wl/Examples/run.wls
           run + render every example

       wolframscript -f wl/Examples/run.wls 02-id-app-era
           run + render just one

       wolframscript -f wl/Examples/run.wls 02-id-app-era --eval
           run only, do not (re)write graph.png

   The runner is the only place that loads the paclet, calls TInit /
   TReset, captures the resulting term, and exports the graph as
   `graph.png` next to the corresponding `term.wl`.  The example
   files themselves carry no boilerplate -- a `term.wl` is a single
   expression (or a CompoundExpression) whose final value is the term
   to render, exactly what THeapGraph[term] expects as a seed.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];   (* Get not Needs: avoids Global`TInit shadow *)

debugPrint[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]

$root = "wl/Examples";
$size = {360, 240};

(* Parse args: optional first positional = example id, optional --eval flag. *)
$args     = Drop[$ScriptCommandLine, 1];
$evalOnly = MemberQ[$args, "--eval"];
$picked   = First[Select[$args, ! StringStartsQ[#, "--"] &], None];

allExampleDirs[] := Sort @ Select[
    FileNames["*", $root],
    DirectoryQ[#] && FileExistsQ[FileNameJoin[{#, "term.wl"}]] &
]

renderOne[dir_String] := Block[{wlFile, pngFile, term, graph},
    wlFile  = FileNameJoin[{dir, "term.wl"}];
    pngFile = FileNameJoin[{dir, "graph.png"}];
    TInit[];
    term = Get[wlFile];
    If[ $evalOnly,
        debugPrint["[eval] ", FileNameTake[dir], " -> ", ToString[term, InputForm]];
        TFree[];
        Return[]
    ];
    graph = THeapGraph[term, Background -> GrayLevel[0.12]];
    (* Force the dark Wolfram theme so LightDarkSwitched picks the
       dark-mode arm (white labels, darker fills, white outlines). *)
    Export[
        pngFile,
        Style[graph, "DarkScheme", Background -> GrayLevel[0.12]],
        "PNG",
        ImageResolution -> 144
    ];
    debugPrint["[ok] ", FileNameTake[dir], " -> ", pngFile];
    TFree[];
]

dirs = If[ $picked === None,
    allExampleDirs[],
    {FileNameJoin[{$root, $picked}]}
];

If[ Length[dirs] === 0,
    debugPrint["no examples to run"];
    Exit[1]
];

Do[
    If[ ! FileExistsQ[FileNameJoin[{d, "term.wl"}]],
        debugPrint["[skip] ", FileNameTake[d], " (no term.wl)"];
        Continue[]
    ];
    renderOne[d],
    {d, dirs}
];
