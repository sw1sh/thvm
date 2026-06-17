#!/usr/bin/env wolframscript
(* test_reductions.wls -- for every wl/Examples/<id>/ folder that has
   both `term.wl` and `term-reduced.wl`, run TWnf on the constructed
   term and verify TTermExpr of the result equals TTermExpr of the
   expected reduced term.

   Run from the repo root:
       wolframscript -f wl/Examples/test_reductions.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

debugPrint[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]

$root = "wl/Examples";

allExampleDirs[] := Sort @ Select[
    FileNames["*", $root],
    DirectoryQ[#] && FileExistsQ[FileNameJoin[{#, "term.wl"}]] &
]

runOne[dir_String] := Block[{
    base = FileNameTake[dir],
    inputFile, expectedFile, input, expected, reducedTree, expectedTree
},
    inputFile    = FileNameJoin[{dir, "term.wl"}];
    expectedFile = FileNameJoin[{dir, "term-reduced.wl"}];
    If[ ! FileExistsQ[expectedFile],
        debugPrint["[skip] ", base, " (no term-reduced.wl)"];
        Return[True]
    ];
    TInit[];
    input    = Get[inputFile];
    reducedTree = TTermExpr[TWnf[input]];
    TFree[];
    TInit[];
    expected     = Get[expectedFile];
    expectedTree = TTermExpr[expected];
    TFree[];
    If[ reducedTree === expectedTree,
        debugPrint["[ok]   ", base];
        True,
        debugPrint["[FAIL] ", base];
        debugPrint["       got:      ", ToString[reducedTree, InputForm]];
        debugPrint["       expected: ", ToString[expectedTree, InputForm]];
        False
    ]
]

results = runOne /@ allExampleDirs[];
nPass   = Count[results, True];
nFail   = Count[results, False];
debugPrint["reductions: ", nPass, " passed, ", nFail, " failed"];
Exit[If[ nFail > 0, 1, 0]]
