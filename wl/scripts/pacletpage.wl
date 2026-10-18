(* ::Package:: *)

(* What build_docs.wls, release.wls and submit.wls share: PacletPage, from the checkout that
   PACLETPAGE_CHECKOUT names or else from its published resource; the paclet's directory; and how
   its documentation is built. *)

$pacletDirectory = FileNameJoin[{ParentDirectory[DirectoryName[$InputFileName]], "THVMLink"}];

With[{checkout = Environment["PACLETPAGE_CHECKOUT"]},
    If[ StringQ[checkout] && FileExistsQ[FileNameJoin[{checkout, "PacletInfo.wl"}]],
        PacletDirectoryLoad[checkout],
        PacletInstall[ResourceObject["https://www.wolframcloud.com/obj/wolframinstitute/DeployedResources/Paclet/WolframInstitute/PacletPage"]]
    ]
];
Needs["WolframInstitute`PacletPage`"];

(* Pages built without evaluating their examples: their code and the `<!-- => -->` hints are shown
   as written.
   - AtpMethods.md and TAtpTrainScorer.md: a second TAtpTrainScorer or TNetTrain in one kernel dies
     inside a JIT-compiled CPU tensor kernel (docs/plans/atp_heap_leak.md, "Poison 3").
   - Train.md: its training example aborts the kernel, fresh or not, with "thvm fatal: fired kernel
     kid=96, which the kernel GC stripped (its inputs were freed)".
   Each comes off this list when its crash is fixed. *)
$unevaluatedPages = {"AtpMethods.md", "TAtpTrainScorer.md", "Train.md"};

(* The converter's own resets: a symbol page clears its bindings at every --- and heading, so each
   example group stands alone (run_doc_examples.wls checks the same), and a tutorial threads its
   state from the first cell to the last.  THVMLink`ATP` loads Wolfram`Parser`, so the examples run against
   it too, which PacletPage does not read from PacletInfo.wl's {name, version} form.  Each page
   converts in its own parallel kernel, so an ATP saturation that outlives its time limit, or a
   crash, ends that kernel and not the build; six of them leave the machine to other builds. *)
$documentationOptions = {
    "Dependencies" -> {"Wolfram/Parser"},
    "Parallel" -> 6,
    "TimeConstraint" -> 900
};

(* The libraries the release ships: one per Wolfram $SystemID, all built by `make wl-cross` from
   the same commit.  Debug output the toolchains leave beside them stays out of the archive. *)
$resourceOptions = {"Exclude" -> {"*.dSYM", "*.dSYM/*", "*.pdb", "*.lib", "*.exp", "*.DS_Store", "docs/*", "Tests/*"}};

(* Pages whose examples compute in parallel kernels of their own (TFindFiniteModels'
   "Parallelize"): they convert in this kernel, since parallel computations cannot nest in the
   parallel kernel that would convert them, with $sequentialKernels kernels launched for them,
   since a kernel `wl` starts has none configured. *)
$sequentialPages = {"TFindFiniteModels.md"};
$sequentialKernels = 4;

(* The pages in three groups, into the same Documentation/English tree and the same cache: most on
   parallel kernels, the sequential ones in this kernel, and the unevaluated ones without their
   examples. The definition notebook is built with the first group. *)
buildDocumentation[opts___] := Module[{all, groupOf, groups, build, results},
    all = Sort @ FileNames["*.md", FileNameJoin[{$pacletDirectory, "docs"}], Infinity];
    groupOf[file_] := Which[
        MemberQ[$unevaluatedPages, FileNameTake[file]], "Unevaluated",
        MemberQ[$sequentialPages, FileNameTake[file]], "Sequential",
        True, "Parallel"];
    groups = {
        {"Parallel", {}},
        {"Sequential", {"Parallel" -> False, "Definition" -> None}},
        {"Unevaluated", {"ConverterOptions" -> {"Evaluate" -> False}, "Definition" -> None}}
    };
    build[files_, groupOptions_] := BuildPacletDocumentation[$pacletDirectory, opts, "Files" -> files, Sequence @@ groupOptions, Sequence @@ $documentationOptions];
    results = Map[
        Function[{group}, With[{files = Select[all, groupOf[#] === First[group] &]},
            If[ files === {}, Nothing,
                Replace[
                    If[ First[group] === "Sequential",
                        WithCleanup[LaunchKernels[$sequentialKernels], build[files, Last[group]], CloseKernels[]],
                        build[files, Last[group]]],
                    Except[_Association] :> Return[$Failed, Module]]]]],
        groups];
    Merge[results, Replace[{{l__List} :> Join[l], {a__Association} :> Join[a], {a_, ___} :> a}]]
]
