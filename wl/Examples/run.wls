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

(* Wolfram`DiagrammaticComputation` is assumed to be installed
   (PacletInstall["Wolfram/DiagrammaticComputation"]).  THVMLink's
   Diagram subpackage Needs it on load. *)
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

(* exportPng[file, expr] -- shared PNG exporter used for both heap
   graphs and IC string diagrams.  Background is the dark canvas
   that LightDarkSwitched-aware styling targets. *)
exportPng[file_String, expr_] := Export[
    file,
    Style[expr, Background -> GrayLevel[0.12]],
    "PNG",
    ImageResolution -> 144
]

renderHeap[term_, file_] :=
    exportPng[file, Style[THeapGraph[term, Background -> GrayLevel[0.12]], "DarkScheme"]]

renderDiagram[term_, file_] :=
    exportPng[file, THeapDiagram[term]["Arrange"]["Grid", "PortLabels" -> False]]

(* Per-render time budget (seconds).  Dense tensor graphs (e.g. the
   gradient of MSE through a dot product) blow the IC string-diagram
   layout up by 100x; rather than hang the whole batch we let
   TimeConstrained abort that one render and keep going. *)
$renderBudget = 30;

renderWithBudget[label_String, file_String, body_] := With[{
    result = TimeConstrained[body; True, $renderBudget, $TimedOut]
},
    Switch[result,
        True,      debugPrint["[ok] ",   label, " -> ", file],
        $TimedOut, debugPrint["[skip] ", label, " -> ", file,
                              " (over ", $renderBudget, "s)"],
        _,         debugPrint["[err] ",  label, " -> ", file]
    ]
]

renderFile[wlFile_String] := Block[{
    base, pngFile, diagFile, wnfPng, wnfDiagFile, term, reduced,
    isWnfExample
},
    base         = FileBaseName[wlFile];
    pngFile      = FileNameJoin[{DirectoryName[wlFile], base <> ".png"}];
    diagFile     = FileNameJoin[{DirectoryName[wlFile], "diagram.png"}];
    wnfPng       = FileNameJoin[{DirectoryName[wlFile], base <> "-wnf.png"}];
    wnfDiagFile  = FileNameJoin[{DirectoryName[wlFile], "diagram-wnf.png"}];
    isWnfExample = StringStartsQ[FileNameTake[DirectoryName[wlFile]], "grad-"];

    TInit[];
    term = Get[wlFile];
    If[ $evalOnly,
        debugPrint["[eval] ", wlFile, " -> ", ToString[term, InputForm]];
        TFree[];
        Return[]
    ];

    (* Pre-reduce heap graph (always). *)
    renderWithBudget[wlFile, pngFile, renderHeap[term, pngFile]];

    (* Pre-reduce IC diagram for the canonical term.wl. *)
    If[ base === "term",
        renderWithBudget[wlFile, diagFile, renderDiagram[term, diagFile]]
    ];

    (* Post-reduce graphs for grad- prefixed examples: TWnf returns
       the rewritten outermost term, which we use as the discovery
       seed so the new ADD/MUL/CONST sub-graph is reachable. *)
    If[ isWnfExample && base === "term",
        reduced = TWnf[term];
        renderWithBudget[wlFile, wnfPng,      renderHeap[reduced, wnfPng]];
        renderWithBudget[wlFile, wnfDiagFile, renderDiagram[reduced, wnfDiagFile]]
    ];

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
