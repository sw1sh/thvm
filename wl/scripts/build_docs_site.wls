#!/usr/bin/env wolframscript
(* ::Package:: *)

(* Build and deploy a custom single-page documentation browser for THVMLink.

   Adapted from PureMath/scripts/build_docs_site.wls (3-column nav variant).

   Rationale: the stock paclet shingle deploys each documentation page as a
   "delayed embed" that fetches /statichtml/<page>.nb?maxwaitmillis=1000 at
   request time -- one second is not enough to render these heavy notebooks, so
   every page except a pre-warmed one comes back empty.

   So we RE-HOST every documentation page (the guide, the tutorials, and the
   symbol reference) as a clean, self-contained static file under
   <account>/THVMLinkDocs/{guide,tutorial,ref}/<Name>.html (rendered once via the
   cloud's statichtml endpoint, internal links rewritten to our copies, plus a
   page-fix CSS + a click-to-copy JS baked in), then deploy ONE index.html that
   frames them in three columns: the guide + tutorials on the left, the iframe in
   the middle, the symbol reference on the right.  Navigation only swaps the
   iframe src; a same-origin click hook routes in-content links through the same
   loader, so it never leaves our shell.  Static files serve instantly.

   The home / landing view is the re-hosted GUIDE page (a public THVMLinkDocs
   object), so the site is fully self-contained and does not depend on the
   deployed resource being anonymously readable.  The header "Resource page" link
   still points at the deployed resource for those who want the official shingle.

   Run after deploy_paclet.wls (the resource must be deployed so its
   /Documentation/{guide,ref,tutorial}/<Name>.nb statichtml renders).

   Usage:
     wl -f wl/scripts/build_docs_site.wls          (build + deploy)
     DRYRUN=1 wl -f wl/scripts/build_docs_site.wls (print the tree, no cloud) *)

pacletDir    = FileNameJoin[{ParentDirectory[DirectoryName[$InputFileName]], "THVMLink"}];
docsDir      = FileNameJoin[{pacletDir, "docs"}];
guidesDir    = FileNameJoin[{docsDir, "Guides"}];
symbolsDir   = FileNameJoin[{docsDir, "Symbols"}];
tutorialsDir = FileNameJoin[{docsDir, "Tutorials"}];
webDir       = FileNameJoin[{docsDir, "web"}];

(* ---- config ---- *)
$target      = "THVMLinkDocs";   (* cloud dir for our shell + re-hosted pages, under the account root *)
$resourceRel = "DeployedResources/Paclet/WolframInstitute/THVMLink";  (* where deploy_paclet.wls deploys the paclet *)
$rootGuide   = "THVMLink";
$nbWidth     = 900;
$waitMs      = 25000;
$fetchPar    = 16;

If[ ! DirectoryQ[guidesDir], Print["Guides dir not found: ", guidesDir]; Exit[1] ];

(* ---- doc-page Title from the markdown frontmatter (Title: / Name:), else basename ---- *)
mdTitle[file_] := Module[{m},
    m = StringCases[Import[file, "Text"],
            StartOfLine ~~ ("Title:" | "Name:") ~~ Whitespace... ~~ t : Except["\n"].. :> StringTrim[t], 1];
    If[m === {}, FileBaseName[file], First[m]]
];

(* left rail: the single guide, then a delimiter, then the tutorials.
   right rail: the symbol reference, flat + alphabetical. *)
guideFile  = FileNameJoin[{guidesDir, $rootGuide <> ".md"}];
tutorials  = SortBy[({FileBaseName[#], mdTitle[#]} &) /@ FileNames["*.md", tutorialsDir], First];
symbolsAll = Sort @ DeleteDuplicates[FileBaseName /@ FileNames["*.md", symbolsDir]];

(* every page to re-host: {type ("guide"|"tutorial"|"ref"), name} *)
entries = Join[
    {{"guide", $rootGuide}},
    ({"tutorial", #[[1]]} &) /@ tutorials,
    ({"ref", #} &) /@ symbolsAll
];
Print["build_docs_site: 1 guide, ", Length[tutorials], " tutorials, ",
    Length[symbolsAll], " symbols -> ", Length[entries], " pages"];

(* ---- DRYRUN: print the structure and stop (no cloud needed) ---- *)
If[ Environment["DRYRUN"] === "1",
    Print["+ ", $rootGuide, " (", mdTitle[guideFile], ")"];
    Print["--- tutorials (left) ---"];
    Print["  = " <> #[[1]] <> " (" <> #[[2]] <> ")"] & /@ tutorials;
    Print["--- symbols (right rail): ", Length[symbolsAll], " ---"];
    Print["  " <> StringRiffle[Take[symbolsAll, UpTo[12]], ", "], " ..."];
    Print["build_docs_site: DRYRUN, not deploying"]; Exit[0]
];

(* ---- connect to the cloud and derive the account-relative base URLs ----
   $CloudRootDirectory is the account root (CloudObject[".../obj/<account>"]); both
   the paclet resource and our docs dir live under it, so deriving from it makes
   this run under any cloud id (local or CI) without hardcoding the account. *)
If[ ! $CloudConnected,
    With[{u = Environment["WOLFRAM_CLOUD_USER"], p = Environment["WOLFRAM_CLOUD_PASSWORD"]},
        If[StringQ[u] && StringQ[p], CloudConnect[u, p], CloudConnect[]]]];
If[ ! $CloudConnected, Print["build_docs_site: cloud connection failed"]; Exit[1] ];
accountBase = First[$CloudRootDirectory];
$base  = accountBase <> "/" <> $resourceRel;
$base2 = accountBase <> "/" <> $target;
Print["build_docs_site: account root ", accountBase];

(* ---- URLs ---- *)
srcStatic[type_, name_] :=
    StringReplace[$base, "/obj/" -> "/statichtml/"] <> "/Documentation/" <> type <>
        "/" <> name <> ".nb?width=" <> ToString[$nbWidth] <>
        "&maxwaitmillis=" <> ToString[$waitMs];

esc[s_] := StringReplace[ToString[s],
    {"&" -> "&amp;", "<" -> "&lt;", ">" -> "&gt;", "\"" -> "&quot;"}];

readAsset[f_] := Module[{p = FileNameJoin[{webDir, f}]},
    If[FileExistsQ[p], Import[p, "Text"], Print["build_docs_site: missing asset ", p]; Exit[1]]];

(* ---- sidebars (links carry data-id; the loader builds the page URL) ---- *)
aTag[id_, label_, cls_] :=
    "<a class=\"" <> cls <> "\" data-id=\"" <> esc[id] <> "\" href=\"#" <> esc[id] <>
        "\">" <> esc[label] <> "</a>";

(* left rail: the guide, a thick delimiter, then the tutorials *)
sidebar = StringJoin[
    "<ul class=\"nav-root\"><li class=\"leaf\">",
    aTag["guide/" <> $rootGuide, mdTitle[guideFile], "gd"],
    "</li></ul>",
    "<div class=\"nav-sep\"></div>",
    "<span class=\"rail-label\">Tutorials</span>",
    "<ul class=\"nav-tutorials\">",
    StringJoin[("<li>" <> aTag["tutorial/" <> #[[1]], #[[2]], "tut"] <> "</li>" &) /@ tutorials],
    "</ul>"
];

(* right rail: the symbols, flat + alphabetical *)
symbolsHTML = StringJoin[
    "<span class=\"rail-label\">Reference</span>",
    "<ul class=\"nav-symbols\">",
    StringJoin[("<li>" <> aTag["ref/" <> #, #, "sym"] <> "</li>" &) /@ symbolsAll],
    "</ul>"
];

(* the landing view is the re-hosted, public GUIDE page (self-contained) *)
homeURL   = $base2 <> "/guide/" <> $rootGuide <> ".html";
homeTitle = $rootGuide;

(* ---- assemble index.html from the separate .css / .js / .html templates ---- *)
jsContent = TemplateApply[
    StringTemplate[readAsset["docs_site.js"]],
    <|"BASE" -> $base, "BASE2" -> $base2, "HOME" -> homeURL|>
];
indexHTML = TemplateApply[
    StringTemplate[readAsset["docs_site.html"]],
    <|
        "title"   -> esc[homeTitle],
        "css"     -> readAsset["docs_site.css"],
        "base"    -> esc[$base],
        "homeURL" -> esc[homeURL],
        "sidebar" -> sidebar,
        "symbols" -> symbolsHTML,
        "js"      -> jsContent
    |>
];

If[ ! StringQ[indexHTML], Print["build_docs_site: index assembly failed"]; Exit[1] ];
Print["build_docs_site: index.html assembled, ", StringLength[indexHTML], " chars"];

(* ---- fetch every page's statichtml in parallel (curl) ---- *)
rawDir = FileNameJoin[{$TemporaryDirectory, "thvmdocs_raw"}];
Quiet @ DeleteDirectory[rawDir, DeleteContents -> True];
CreateDirectory[rawDir];
rawPath[i_] := FileNameJoin[{rawDir, ToString[i] <> ".html"}];
manifest = StringRiffle[
    MapIndexed[(rawPath[First[#2]] <> " " <> srcStatic[#1[[1]], #1[[2]]]) &, entries], "\n"];
manifestFile = FileNameJoin[{rawDir, "_manifest.txt"}];
Export[manifestFile, manifest, "Text"];

Print["build_docs_site: fetching ", Length[entries], " pages (", $fetchPar, "-way parallel)..."];
fetch = RunProcess[{"/bin/bash", "-c",
    "xargs -P " <> ToString[$fetchPar] <> " -L 1 curl -fsS -o < " <> manifestFile}];
Print["build_docs_site: fetch exit ", fetch["ExitCode"]];

(* prepended to every page: page-fix CSS (collapse the blank gap) + click-to-copy JS *)
pageFix = "<style>" <> readAsset["docs_page.css"] <> "</style>\n<script>" <>
    readAsset["docs_page.js"] <> "</script>\n";
(* rewrite the resource's in-content links to our re-hosted, chrome-free pages *)
rewrite[s_] := StringReplace[s, {
    $base <> "/Documentation/guide/"    ~~ n : (WordCharacter ..) ~~ ".html" :> $base2 <> "/guide/" <> n <> ".html",
    $base <> "/Documentation/tutorial/" ~~ n : (WordCharacter ..) ~~ ".html" :> $base2 <> "/tutorial/" <> n <> ".html",
    $base <> "/Documentation/ref/"      ~~ n : (WordCharacter ..) ~~ ".html" :> $base2 <> "/ref/" <> n <> ".html"
}];

pages = {}; bad = {};
MapIndexed[
    Module[{type = #1[[1]], name = #1[[2]], rp = rawPath[First[#2]]},
        If[ FileExistsQ[rp] && FileByteCount[rp] > 400,
            AppendTo[pages, {type <> "/" <> name <> ".html", pageFix <> rewrite[Import[rp, "Text"]]}],
            AppendTo[bad, type <> "/" <> name]
        ]
    ] &, entries];
AppendTo[pages, {"index.html", indexHTML}];
Print["build_docs_site: ", Length[pages], " pages ready",
    If[bad === {}, "", " (missing: " <> StringRiffle[Take[bad, UpTo[10]], ", "] <> If[Length[bad] > 10, " ...", ""] <> ")"]];

(* ---- deploy: one public CloudObject per file (path-based, overwrites cleanly) ---- *)
cleanCloud[co_] := (Module[{ch = Quiet @ CloudObjects[co]}, If[ListQ[ch], cleanCloud /@ ch]]; Quiet @ DeleteObject[co]);
cleanCloud[CloudObject[$target]];

Print["build_docs_site: deploying ", Length[pages], " files..."];
n = 0; failed = {};
Do[
    Module[{r = CloudExport[p[[2]], {"String", "text/html"},
                CloudObject[$target <> "/" <> p[[1]]], Permissions -> "Public"]},
        If[MatchQ[r, _CloudObject], n++, AppendTo[failed, p[[1]]]]],
    {p, pages}
];
Print["build_docs_site: deployed ", n, "/", Length[pages], " files",
    If[failed === {}, "", " (FAILED: " <> StringRiffle[Take[failed, UpTo[8]], ", "] <> ")"]];
Print["published: ", $base2, "/index.html"];
Exit[0]
