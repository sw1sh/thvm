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

print[args___] := WriteString["stdout", StringJoin[ToString /@ {args}], "\n"]

pacletDir = DirectoryName[$InputFileName]

(* Prefer the cached local copy when persistence is healthy; otherwise download
   the upstream source directly.  ResourceFunction[ResourceObject[...]] returns
   the unapplied form (the function never runs) when $DefaultLocalBase is broken,
   so we side-step it by Getting the source file. /tmp survives across kernels
   and is not sandboxed inside the wolframscript-launched kernel. *)
mtnSourceCache = "/tmp/MarkdownToNotebook.wl"
mtnSourceUrl   = "https://raw.githubusercontent.com/sw1sh/MarkdownToNotebook/main/MarkdownToNotebook.wl"
If[ ! FileExistsQ[mtnSourceCache] || FileByteCount[mtnSourceCache] < 1000,
    print["fetching MarkdownToNotebook source -> ", mtnSourceCache];
    URLDownload[mtnSourceUrl, mtnSourceCache]
]
Get[mtnSourceCache]
mtn[args___] := MarkdownToNotebook[args]

(* Sibling paclet used in TPTP examples and by the ATP context's load chain;
   not on the public paclet server, so probe a few common sibling-checkout
   locations and PacletDirectoryLoad BEFORE Needs["THVMLink`"], which otherwise
   aborts mid-scan trying to Get Wolfram`Parser` and leaves later siblings
   like Tensor.wl unloaded. *)
With[ { wpRoots = Select[
        { FileNameJoin[{pacletDir, "..", "..", "..", "wolfram", "AISkills", "examples", "WolframParser"}],
          FileNameJoin[{pacletDir, "..", "..", "..", "wolfram", "WolframParser"}],
          FileNameJoin[{pacletDir, "..", "..", "WolframParser"}] },
        FileExistsQ[FileNameJoin[{#, "PacletInfo.wl"}]] &] },
    If[ wpRoots =!= {},
        PacletDirectoryLoad[First[wpRoots]];
        print["loaded sibling paclet Wolfram/WolframParser from ", First[wpRoots]],
        print["[warn] Wolfram/WolframParser not found; TPTP example cells will report Get::noopen."]
    ]
]

PacletDirectoryLoad[pacletDir]
Check[
    Needs["THVMLink`"],
    print["[warn] could not load THVMLink`; example cells will report load errors."];
    print["[warn] build the dylib first: cd ", FileNameJoin[{pacletDir, "..", ".."}], " && make && make wl"]
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

(* Heavy ATP tutorials accumulate too much C-engine memory if every cell
   evaluates in the same kernel; render them without evaluation (the code
   stays visible, just unrun). *)
noEvaluate = {"ATP.md", "AtpMethods.md"}

(* Hard per-file time budget for example evaluation.  A cell that takes
   longer than this almost certainly indicates a hung saturator / kernel,
   not a slow-but-finishing run, and the build falls back to a no-eval
   render of that file rather than waiting forever.

   Lives in a private context (not Global) so the per-file ClearGlobalScratch
   below cannot wipe it. *)
THVMLink`Build`Private`perFileBudgetSec = 60

(* Cells inside one file share the kernel with EVERY prior file's cells, so a
   bare scratch name like `a` from one cell leaks into another file's cell
   that also references `a`.  MTN sets `$Context` to the file's frontmatter
   Context for the duration of the eval, so a bare `a` is parsed as
   `THVMLink``ATP``a` for an ATP-context file or `THVMLink``a` for a base-
   context file - NOT `Global``a`.  We snapshot every relevant scratch
   context once the build's setup is done, then between files ClearAll any
   new symbol in any of them - i.e. only doc-cell scratch, never the build
   harness, MTN, or the paclet's own exports. *)
THVMLink`Build`Private`scratchContexts =
    {"Global`", "THVMLink`", "THVMLink`ATP`"}

ClearGlobalScratch[] := Module[{ scratch },
    scratch = Complement[
        Join @@ (Names[# <> "*"] & /@ THVMLink`Build`Private`scratchContexts),
        THVMLink`Build`Private`globalBoot
    ];
    Quiet[Unprotect @@ scratch; ClearAll @@ scratch]
]

(* The output basename is the source basename; it must match the URI tail
   (DocumentationBuild builds the URI from the file path). *)
buildDoc[srcFile_String] := Block[{ tmpl, baseName, dir, out, evalQ, res, t0, dt, fallbackQ },
    tmpl = frontmatterTemplate[srcFile];
    If[ ! KeyExistsQ[subdirs, tmpl],
        print["  [skip] ", FileNameTake[srcFile], " (no Template: in frontmatter)"];
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
        THVMLink`Build`Private`perFileBudgetSec,
        $TimedOut
    ];
    If[ res === $TimedOut,
        fallbackQ = True;
        print["  [timeout >", THVMLink`Build`Private`perFileBudgetSec, "s] ",
              FileNameTake[srcFile], " - falling back to no-eval render"];
        mtn[srcFile, out, "Evaluate" -> False]
    ];
    dt = Round[AbsoluteTime[] - t0, 0.1];
    print["  ",
        FileNameTake[srcFile], " -> ",
        If[ subdirs[tmpl] === {}, "", "Documentation/English/" <> Last[subdirs[tmpl]] <> "/" ],
        baseName, ".nb",
        If[ evalQ && !fallbackQ, "", " (no-eval)"],
        "  (", dt, "s)"
    ]
]

(* Snapshot every scratch-context's name set right before the loop;
   ClearGlobalScratch[] preserves these and wipes anything new between files. *)
THVMLink`Build`Private`globalBoot =
    Join @@ (Names[# <> "*"] & /@ THVMLink`Build`Private`scratchContexts)

print["building THVMLink documentation"]
Scan[buildDoc, Sort @ FileNames["*.md", FileNameJoin[{pacletDir, "docs"}], Infinity]]

print["done"]
