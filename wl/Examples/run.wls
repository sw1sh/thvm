#!/usr/bin/env wolframscript
(* run.wls -- run and render every wl/Examples/<id>/term*.wl
   (or just one example folder).

   Run from the repo root:

       wolframscript -f wl/Examples/run.wls
           run + render every example

       wolframscript -f wl/Examples/run.wls id-app-era
           run + render just one folder (all its term*.wl)

       wolframscript -f wl/Examples/run.wls id-app-era --eval
           run only, do not (re)write any PNGs

   Each example folder holds:
     term.wl           unreduced construction (required)
     term-reduced.wl   optional post-TWnf variant
     term.hvm          optional HVM4 surface-syntax reference

   The runner scans for every `term*.wl` in the folder and writes a
   matching `<basename>.png` next to each.  A `term.wl` produces
   `term.png`; `term-reduced.wl` produces `term-reduced.png`.

   The runner loads the paclet, calls TInit / TReset per file, and
   exports the resulting `THeapGraph[term]` onto a dark GrayLevel[.12]
   canvas so LightDarkSwitched-aware styling picks the dark arm.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];   (* Get not Needs: avoids Global`TInit shadow *)

debugPrint[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]

$root = "wl/Examples";

$args     = Drop[$ScriptCommandLine, 1];
$evalOnly = MemberQ[$args, "--eval"];
$picked   = First[Select[$args, ! StringStartsQ[#, "--"] &], None];

allExampleDirs[] := Sort @ Select[
    FileNames["*", $root],
    DirectoryQ[#] && FileExistsQ[FileNameJoin[{#, "term.wl"}]] &
]

(* Render only the input term.  `term-reduced.wl` (and other
   `term-*.wl` siblings) are reference values used by the test suite
   to compare TTermTree[TWnf[...]] against; they are not rendered. *)
termFiles[dir_String] := Select[
    FileNames["term*.wl", dir],
    FileBaseName[#] === "term" &
]

renderFile[wlFile_String] := Block[{pngFile, base, term, graph},
    base    = FileBaseName[wlFile];
    pngFile = FileNameJoin[{DirectoryName[wlFile], base <> ".png"}];
    TInit[];
    term = Get[wlFile];
    If[ $evalOnly,
        debugPrint["[eval] ", wlFile, " -> ", ToString[term, InputForm]];
        TFree[];
        Return[]
    ];
    graph = THeapGraph[term, Background -> GrayLevel[0.12]];
    Export[
        pngFile,
        Style[graph, "DarkScheme", Background -> GrayLevel[0.12]],
        "PNG",
        ImageResolution -> 144
    ];
    debugPrint["[ok] ", wlFile, " -> ", pngFile];
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
    Do[renderFile[wl], {wl, termFiles[d]}],
    {d, dirs}
];
