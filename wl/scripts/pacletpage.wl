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
   as written.  A second TAtpTrainScorer or TNetTrain in one kernel dies inside a JIT-compiled CPU
   tensor kernel (docs/plans/atp_heap_leak.md, "Poison 3"), and the pages TAtpCpDataset and Train
   already train during the build, so these two stay unevaluated until that crash is fixed. *)
$unevaluatedPages = {"AtpMethods.md", "TAtpTrainScorer.md"};

(* A symbol page's or the guide's sections share one context, as the pages were written; tutorials
   and overviews do by default.  THVMLink`ATP` loads Wolfram`Parser`, so the examples run against
   it too, which PacletPage does not read from PacletInfo.wl's {name, version} form.  Each page
   converts in its own parallel kernel, so an ATP saturation that outlives its time limit, or a
   crash, ends that kernel and not the build. *)
$documentationOptions = {
    "EvaluateSeparator" -> <|"Symbol" -> None, "Guide" -> None|>,
    "Dependencies" -> {"Wolfram/Parser"},
    "Parallel" -> True,
    "TimeConstraint" -> 900
};

(* The libraries the release ships: one per Wolfram $SystemID, all built by `make wl-cross` from
   the same commit.  Debug output the toolchains leave beside them stays out of the archive. *)
$resourceOptions = {"Exclude" -> {"*.dSYM", "*.dSYM/*", "*.pdb", "*.lib", "*.exp", "*.DS_Store", "docs/*", "Tests/*"}};

(* Every page but the unevaluated ones, with their examples evaluated, then those, without; both
   into the same Documentation/English tree and the same cache. *)
buildDocumentation[opts___] := Module[{all, pinned, evaluated, unevaluated},
    all = Sort @ FileNames["*.md", FileNameJoin[{$pacletDirectory, "docs"}], Infinity];
    pinned = Select[all, MemberQ[$unevaluatedPages, FileNameTake[#]] &];
    evaluated = BuildPacletDocumentation[$pacletDirectory, opts, "Files" -> Complement[all, pinned], Sequence @@ $documentationOptions];
    If[ ! AssociationQ[evaluated] || pinned === {}, Return[evaluated, Module]];
    unevaluated = BuildPacletDocumentation[$pacletDirectory, opts, "Files" -> pinned, "Definition" -> None,
        "ConverterOptions" -> {"Evaluate" -> False}, Sequence @@ $documentationOptions];
    If[ ! AssociationQ[unevaluated], Return[$Failed, Module]];
    Merge[{evaluated, unevaluated}, Replace[{{a_List, b_List} :> Join[a, b], {a_Association, b_Association} :> Join[a, b], {a_, ___} :> a}]]
]
