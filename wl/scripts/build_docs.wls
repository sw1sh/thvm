#!/usr/bin/env wolframscript
(* Build the THVMLink paclet documentation from the structured markdown
   in docs/, writing each notebook into the standard paclet layout under
   Documentation/English/.  Also builds the paclet-repository submission
   notebook (docs/ResourceDefinition.md -> ResourceDefinition.nb at the
   paclet root).

   MarkdownToNotebook is fetched from its public cloud deployment, so no
   local clone of the converter is required.  Example cells are evaluated
   once and persisted via the built-in caching framework, so subsequent
   builds skip unchanged cells. *)

pacletDir = FileNameJoin[{DirectoryName[$InputFileName], "..", "THVMLink"}]

mtn = ResourceFunction[ResourceObject["https://www.wolframcloud.com/obj/nikm/DeployedResources/Function/MarkdownToNotebook"]]
Needs["Wolfram`Parser`"]

PacletDirectoryLoad[pacletDir]
Check[
    Needs["WolframInstitute`THVMLink`"],
    Print["[warn] could not load WolframInstitute`THVMLink`; example cells will report load errors."];
    Print["[warn] build the dylib first: cd ", FileNameJoin[{pacletDir, "..", ".."}], " && make wl"]
]

subdirs = <|
    "Symbol"   -> {"Documentation", "English", "ReferencePages", "Symbols"},
    "Guide"    -> {"Documentation", "English", "Guides"},
    "TechNote" -> {"Documentation", "English", "Tutorials"},
    "Overview" -> {"Documentation", "English", "Tutorials"},
    (* Paclet submission notebook lives at the paclet root, alongside PacletInfo.wl. *)
    "Paclet"   -> {}
|>

(* Read just the YAML frontmatter so we don't run example cells just to learn
   what template a file uses. *)
frontmatterTemplate[srcFile_String] := Module[{ text, head, m },
    text = Import[srcFile, "Text"];
    head = If[ StringStartsQ[text, "---\n"],
        StringTake[text, StringPosition[text, "\n---\n", 2][[1, 1]]],
        ""
    ];
    m = StringCases[head, "Template:" ~~ Whitespace ~~ tmpl : (LetterCharacter ..) :> tmpl];
    If[ m === {}, Missing["NoTemplate"], First[m] ]
]

(* All docs evaluate.  Individually-heavy cells in ATP.md / AtpMethods.md
   carry a per-cell `#| eval: false` so the code stays visible but its
   saturation cost stays out of the shared build kernel. *)
noEvaluate = {}

(* Hard per-file time budget for example evaluation.  A cell that takes
   longer than this almost certainly indicates a hung saturator / kernel,
   not a slow-but-finishing run, and the build falls back to a no-eval
   render of that file rather than waiting forever.

   Lives in a private context (not Global) so the per-file ClearGlobalScratch
   below cannot wipe it. *)
Build`perFileBudgetSec = 60

(* Cells inside one file share the kernel with EVERY prior file's cells, so a
   bare scratch name like `a` from one cell leaks into another file's cell
   that also references `a`.  MTN sets `$Context` to the file's frontmatter
   Context for the duration of the eval, so a bare `a` is parsed as
   `WolframInstitute`THVMLink``ATP``a` for an ATP-context file or `WolframInstitute`THVMLink``a` for a base-
   context file - NOT `Global``a`.  We snapshot every relevant scratch
   context once the build's setup is done, then between files ClearAll any
   new symbol in any of them - i.e. only doc-cell scratch, never the build
   harness, MTN, or the paclet's own exports. *)
Build`scratchContexts =
    {"Global`", "WolframInstitute`THVMLink`", "WolframInstitute`THVMLink`ATP`"}

ClearGlobalScratch[] := Module[{ scratch },
    scratch = Complement[
        Join @@ (Names[{# <> "*", # <> "**`*"}] & /@ Build`scratchContexts),
        Build`globalBoot
    ];
    Quiet[Unprotect @@ scratch; ClearAll @@ scratch]
]

(* The output basename is the source basename; it must match the URI tail
   (DocumentationBuild builds the URI from the file path). *)
buildDoc[srcFile_String] := Block[{ tmpl, baseName, dir, out, evalQ, res, t0, dt, fallbackQ },
    tmpl = frontmatterTemplate[srcFile];
    If[ ! KeyExistsQ[subdirs, tmpl],
        Print["  [skip] ", FileNameTake[srcFile], " (no Template: in frontmatter)"];
        Return[Null]
    ];
    baseName = FileBaseName[srcFile];
    dir      = FileNameJoin @ Prepend[subdirs[tmpl], pacletDir];
    Quiet @ CreateDirectory[dir, CreateIntermediateDirectories -> True];
    out      = FileNameJoin[{dir, baseName <> ".nb"}];
    evalQ    = ! MemberQ[noEvaluate, FileNameTake[srcFile]];
    t0       = AbsoluteTime[];
    fallbackQ = False;
    ClearGlobalScratch[];
    res = TimeConstrained[
        mtn[srcFile, out, "Evaluate" -> evalQ],
        Build`perFileBudgetSec,
        $TimedOut
    ];
    If[ res === $TimedOut,
        fallbackQ = True;
        Print["  [timeout >", Build`perFileBudgetSec, "s] ",
              FileNameTake[srcFile], " - falling back to no-eval render"];
        mtn[srcFile, out, "Evaluate" -> False]
    ];
    dt = Round[AbsoluteTime[] - t0, 0.1];
    Print["  ",
        FileNameTake[srcFile], " -> ",
        If[ subdirs[tmpl] === {}, "", "Documentation/English/" <> Last[subdirs[tmpl]] <> "/" ],
        baseName, ".nb",
        If[ evalQ && !fallbackQ, "", " (no-eval)"],
        "  (", dt, "s)"
    ];
    If[ Build`showQ, showBuiltDoc[out] ]
]

(* `--show` reads the freshly built .nb back and echoes its Input /
   Output / Message / Print cells to the console, so a single-doc build
   doubles as an example-output test harness: you see each evaluated
   result next to the `<!-- => ... -->` hint in the source without
   opening the notebook in a front end.  DisplayForm renders the cell's
   boxes to plain 2-D text; long renderings are clipped. *)
showBuiltDoc[out_String] := Module[{ nb, cells, render },
    If[ ! FileExistsQ[out], Return[Null]];
    nb = Quiet @ Get[out];
    If[ Head[nb] =!= Notebook, Return[Null]];
    cells = Cases[nb,
        Cell[BoxData[b_], sty : ("Input" | "Output" | "Message" | "Print"), ___] :>
            {sty, b},
        Infinity];
    If[ cells === {},
        Print["    (no example cells)"];
        Return[Null]
    ];
    render[b_] := StringReplace[
        StringTake[Quiet @ ToString[DisplayForm[b]], UpTo[1200]],
        "\n" -> "\n          "];
    Print["    --- evaluated cells ---"];
    Scan[Function[c, Print["    [", c[[1]], "] ", render[c[[2]]]]], cells]
]

(* Snapshot every scratch-context's name set right before the loop;
   ClearGlobalScratch[] preserves these and wipes anything new between files. *)
Build`globalBoot =
    Join @@ (Names[{# <> "*", # <> "**`*"}] & /@ Build`scratchContexts)

(* `wolframscript -file build_docs.wls --clean` wipes MTN's persistent example-
   output cache for this paclet before building, forcing every example cell
   to re-evaluate.  The cache is keyed by the doc's frontmatter `Name:`
   (here `WolframInstitute/WolframInstitute`THVMLink`), NOT by the source filename, so each
   .md file's cache lives under that single dir.  Pre-existing built .nb
   files also wiped so a stale render can't survive the rebuild. *)
If[ MemberQ[Rest @ $ScriptCommandLine, "--clean"],
    Print["[--clean] wiping MTN cache + previously built notebooks"];
    Quiet @ DeleteDirectory[
        FileNameJoin[{$HomeDirectory, "Library", "Wolfram", "Objects",
            "Persistence", "MarkdownToNotebook", "ExampleOutput",
            "WolframInstitute"}],
        DeleteContents -> True
    ];
    Quiet @ DeleteDirectory[FileNameJoin[{pacletDir, "Documentation"}], DeleteContents -> True];
    Quiet @ DeleteFile[FileNameJoin[{pacletDir, "ResourceDefinition.nb"}]]
]

(* Positional (non-flag) args restrict the build to the named docs, matched
   by basename or filename (with or without the ".md"); with no selector
   every doc is built.  Building one file in isolation also sidesteps a
   sibling page poisoning the shared build kernel -- e.g. an ATP-context
   tutorial whose Wolfram`Parser` dependency is unavailable aborts mid-load
   and the later base-context tutorials then fail to render.  Pair a single-doc
   build with --show to echo the evaluated example cells for output checking.
     wolframscript -file build_docs.wls Tensors                 (* just Tensors.md *)
     wolframscript -file build_docs.wls --clean Tensors          (* wipe cache, then it *)
     wolframscript -file build_docs.wls --show FindReplacePath   (* build it + print cell outputs *) *)
Build`showQ = MemberQ[Rest @ $ScriptCommandLine, "--show"]
allDocs = Sort @ FileNames["*.md", FileNameJoin[{pacletDir, "docs"}], Infinity]
fileSel = Select[Rest @ $ScriptCommandLine, sel |-> ! StringStartsQ[sel, "--"]]
toBuild = If[ fileSel === {},
    allDocs,
    Select[allDocs, doc |-> AnyTrue[fileSel,
        sel |-> MemberQ[{FileBaseName[doc], FileNameTake[doc], FileBaseName[doc] <> ".md"}, sel]]]
]
If[ fileSel =!= {} && toBuild === {},
    Print["[error] no docs matched ", fileSel, "; available basenames: ",
        StringRiffle[FileBaseName /@ allDocs, ", "]];
    Exit[1]
]

Print["building THVMLink documentation",
    If[ fileSel === {}, "", " (selected: " <> StringRiffle[FileBaseName /@ toBuild, ", "] <> ")"]]
Scan[buildDoc, toBuild]

Print["done"]
