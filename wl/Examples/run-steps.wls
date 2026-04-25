#!/usr/bin/env wolframscript
(* run-steps.wls -- step a single example one interaction at a time
   via TStep[] and render the heap graph + IC diagram at each step.

   Run from the repo root:
       wolframscript -f wl/Examples/run-steps.wls church-2-applied
       wolframscript -f wl/Examples/run-steps.wls grad-mul

   For each step, writes to wl/Examples/<id>/steps/:
     step-NN.png         -- THeapGraph snapshot
     step-NN-diagram.png -- THeapDiagram snapshot
   plus an index file listing the ITRS at each step.

   Stops when TStep makes no progress (TItrs delta == 0) or after
   --max-steps (default 30). *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

debugPrint[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]

$args     = Drop[$ScriptCommandLine, 1];
$picked   = First[Select[$args, ! StringStartsQ[#, "--"] &], None];
$maxSteps = With[{a = First[Select[$args, StringStartsQ[#, "--max-steps="] &], None]},
    If[ a === None, 30, ToExpression[StringDrop[a, StringLength["--max-steps="]]]]
];

If[ $picked === None,
    debugPrint["usage: run-steps.wls <example-id> [--max-steps=N]"];
    Exit[1]
]

$exampleDir = FileNameJoin[{"wl/Examples", $picked}];
$termFile   = FileNameJoin[{$exampleDir, "term.wl"}];
$stepsDir   = FileNameJoin[{$exampleDir, "steps"}];

If[ ! FileExistsQ[$termFile],
    debugPrint["no term.wl in ", $exampleDir];
    Exit[1]
]
If[ ! DirectoryQ[$stepsDir], CreateDirectory[$stepsDir]]

(* Clean previous step images so a shorter run doesn't leave stale files. *)
Do[ DeleteFile[f], {f, FileNames["step-*.png", $stepsDir]}]

stepLabel[n_Integer] := StringPadLeft[ToString[n], 2, "0"]

renderHeap[term_, file_] := Export[file,
    Style[THeapGraph[term, Background -> GrayLevel[0.12]], "DarkScheme"],
    "PNG", ImageResolution -> 144,
    Background -> GrayLevel[0.12]
]

renderDiagram[term_, file_] := Export[file,
    Style[THeapDiagram[term]["Arrange"]["Grid", "PortLabels" -> False],
          Background -> GrayLevel[0.12]],
    "PNG", ImageResolution -> 144
]

(* Per-render time budget (seconds) -- some IC diagrams blow up. *)
$renderBudget = 30;

renderBoth[term_, n_Integer] := With[{
    heapFile = FileNameJoin[{$stepsDir, "step-" <> stepLabel[n] <> ".png"}],
    diagFile = FileNameJoin[{$stepsDir, "step-" <> stepLabel[n] <> "-diagram.png"}]
},
    TimeConstrained[renderHeap[term, heapFile], $renderBudget];
    TimeConstrained[renderDiagram[term, diagFile], $renderBudget];
]

TInit[];
term = Get[$termFile];

renderBoth[term, 0];
itrs = TItrs[];
debugPrint["[step 00] itrs=0 tag=", TTagName[TTermTag[term]]];

n = 0;
While[ n < $maxSteps,
    prevItrs = TItrs[];
    next = TStep[term];
    delta = TItrs[] - prevItrs;
    n++;
    If[ delta === 0,
        debugPrint["[step ", stepLabel[n], "] no progress; stopping"];
        n--; (* don't bump if no work *)
        Break[]
    ];
    term = next;
    renderBoth[term, n];
    debugPrint["[step ", stepLabel[n], "] itrs +", delta,
               " tag=", TTagName[TTermTag[term]],
               " stack=", TTagName[TTermTag[#]]& /@ TStack[]];
];

debugPrint["wrote ", n + 1, " step diagrams to ", $stepsDir];
TFree[];
